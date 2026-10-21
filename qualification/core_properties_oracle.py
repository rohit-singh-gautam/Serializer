# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify the native adapter against official fifth-edition schemas and an independent OPC producer."""
import argparse
import ctypes
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parent.parent
CORE = 'http://schemas.openxmlformats.org/package/2006/metadata/core-properties'
DC = 'http://purl.org/dc/elements/1.1/'
TERMS = 'http://purl.org/dc/terms/'
INSTANCE = 'http://www.w3.org/2001/XMLSchema-instance'
RELATIONSHIP = 'http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties'
CONTENT_TYPE = 'application/vnd.openxmlformats-package.core-properties+xml'


def invoke(command, data=None):
    """Capture a bounded child invocation; Windows error-mode suppression is inherited by native children."""
    return subprocess.run([str(item) for item in command], input=data, capture_output=True, timeout=60)


def document(body):
    """Build independent prefixes and namespace declarations without using native XML emission."""
    return (f'<p:coreProperties xmlns:p="{CORE}" xmlns:d="{DC}" xmlns:t="{TERMS}" '
            f'xmlns:i="{INSTANCE}">{body}</p:coreProperties>').encode()


def schema_valid(oracle, schemas, output, payload):
    """Validate against Microsoft's independent XML Schema implementation and the caller's official files."""
    output.write_bytes(payload)
    result = invoke([oracle, schemas, output])
    if result.returncode not in (0, 2):
        raise AssertionError(f'Oracle execution failed: {result.stderr.decode(errors="replace")}')
    return result.returncode == 0


class LibxmlOracle:
    """Validate date grammar independently of native handwritten checks, using the actual imported XSD."""

    def __init__(self, library, schemas, output):
        """Localize import URLs in disposable schema copies, preserving original bodies and source hashes."""
        self.library = ctypes.CDLL(str(library.resolve()))
        self.output = output
        local = output / 'local-schemas'
        local.mkdir(exist_ok=True)
        for name in ('opc-coreProperties.xsd', 'dc.xsd', 'dcterms.xsd', 'dcmitype.xsd', 'xml.xsd'):
            text = (schemas / name).read_text(encoding='utf-8')
            for uri, replacement in (
                ('http://dublincore.org/schemas/xmls/qdc/2003/04/02/dc.xsd', 'dc.xsd'),
                ('http://dublincore.org/schemas/xmls/qdc/2003/04/02/dcterms.xsd', 'dcterms.xsd'),
                ('http://www.w3.org/2001/03/xml.xsd', 'xml.xsd')):
                text = text.replace('schemaLocation="' + uri + '"', 'schemaLocation="' + replacement + '"')
            (local / name).write_text(text, encoding='utf-8')
        signatures = {
            'xmlSchemaNewParserCtxt': ([ctypes.c_char_p], ctypes.c_void_p),
            'xmlSchemaParse': ([ctypes.c_void_p], ctypes.c_void_p),
            'xmlSchemaNewValidCtxt': ([ctypes.c_void_p], ctypes.c_void_p),
            'xmlReadMemory': ([ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int], ctypes.c_void_p),
            'xmlSchemaValidateDoc': ([ctypes.c_void_p, ctypes.c_void_p], ctypes.c_int),
            'xmlFreeDoc': ([ctypes.c_void_p], None),
            'xmlSchemaFreeValidCtxt': ([ctypes.c_void_p], None),
            'xmlSchemaFreeParserCtxt': ([ctypes.c_void_p], None),
            'xmlSchemaFree': ([ctypes.c_void_p], None),
        }
        for name, (arguments, result) in signatures.items():
            function = getattr(self.library, name)
            function.argtypes = arguments
            function.restype = result
        self.error_callback_type = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_void_p)
        self.error_callback = self.error_callback_type(lambda _context, _error: None)
        self.library.xmlSchemaSetValidStructuredErrors.argtypes = [
            ctypes.c_void_p, self.error_callback_type, ctypes.c_void_p]
        self.library.xmlSchemaSetValidStructuredErrors.restype = None
        parser = self.library.xmlSchemaNewParserCtxt(os.fsencode(local / 'opc-coreProperties.xsd'))
        if not parser:
            raise AssertionError('Libxml oracle parser allocation failed')
        try:
            self.schema = self.library.xmlSchemaParse(parser)
        finally:
            self.library.xmlSchemaFreeParserCtxt(parser)
        if not self.schema:
            raise AssertionError('Libxml oracle failed to compile the official schema graph')

    def valid(self, payload):
        """Do not permit network resolution, DTD loading or entity expansion for candidate documents."""
        document = self.library.xmlReadMemory(payload, len(payload), None, None, 2048)
        if not document:
            return False
        context = self.library.xmlSchemaNewValidCtxt(self.schema)
        if not context:
            self.library.xmlFreeDoc(document)
            raise AssertionError('Libxml oracle validation allocation failed')
        self.library.xmlSchemaSetValidStructuredErrors(context, self.error_callback, None)
        try:
            return self.library.xmlSchemaValidateDoc(context, document) == 0
        finally:
            self.library.xmlSchemaFreeValidCtxt(context)
            self.library.xmlFreeDoc(document)

    def close(self):
        """Release the prepared provider schema after all independent decisions have completed."""
        self.library.xmlSchemaFree(self.schema)


def package_bytes(payload):
    """Produce stored OPC bytes independently with Python's ZIP library and arbitrary discovered naming."""
    output = io.BytesIO()
    with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_STORED) as archive:
        archive.writestr('[Content_Types].xml',
            '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
            '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
            f'<Override PartName="/independent/metadata.xml" ContentType="{CONTENT_TYPE}"/></Types>')
        archive.writestr('_rels/.rels',
            '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            f'<Relationship Id="independent" Type="{RELATIONSHIP}" Target="independent/metadata.xml"/>'
            '</Relationships>')
        archive.writestr('independent/metadata.xml', payload)
    return output.getvalue()


def execute(args):
    """Check independently declared accept/reject decisions, output schema validity and ZIP metadata."""
    build = args.build.resolve()
    if build == ROOT or (ROOT in build.parents and build.relative_to(ROOT).parts[0] not in ('out', 'build')):
        raise ValueError('Repository output must remain within an ignored build directory')
    build.mkdir(parents=True, exist_ok=True)
    oracle = build / 'core_properties_oracle.exe'
    if args.oracle_profile == 'dotnet-libxml':
        compile_result = invoke([args.csc, '/nologo', '/warn:4', '/warnaserror+', f'/out:{oracle}',
                                 ROOT / 'qualification/core_properties_oracle.cs'])
        if compile_result.returncode:
            raise AssertionError(compile_result.stdout.decode(errors='replace') + compile_result.stderr.decode(errors='replace'))
    schemas = args.schema_directory.resolve()
    hashes = {name: hashlib.sha256((schemas / name).read_bytes()).hexdigest() for name in (
        'opc-coreProperties.xsd', 'dc.xsd', 'dcterms.xsd', 'dcmitype.xsd', 'xml.xsd')}
    libxml = LibxmlOracle(args.libxml2, schemas, build)
    def independent_valid(payload):
        """Select the explicitly requested provider profile; the summary records each executed oracle."""
        if args.oracle_profile == 'dotnet-libxml':
            return schema_valid(oracle, schemas, xml_file, payload)
        return libxml.valid(payload)
    xml_file = build / 'candidate.xml'
    sample = invoke([args.probe, 'model'])
    if sample.returncode or not independent_valid(sample.stdout) or not libxml.valid(sample.stdout):
        raise AssertionError('Native all-property output failed official-schema validation')
    if len(ET.fromstring(sample.stdout)) != 15:
        raise AssertionError('Expected all fifteen fifth-edition core properties')
    accepted = ['1997', '1997Z', '1997-07', '1997-07+01:00', '2000-02-29',
                '1997-07-16T19:20:30Z', '1997-07-16T19:20:30.123456789-05:30',
                ' 2000-02-29 ', '1997-07-16T19:20:30', '1997-07-16T24:00:00Z',
                '-0001-01-01', '12000-02-29']
    rejected = ['', '0000', '1997-00', '1997-13', '1900-02-29', '2026-04-31',
                '1997-07-16T19:20+01:00', '2026-10-21T12:00', '2026-10-21T24:00Z',
                '2026-10-21T12:60:00Z', '2026-10-21T12:00:60Z', '2026-10-21T12:00:00.Z',
                '2026-10-21T12:00:00+14:01', '02026-10-21T00:00:00Z']
    decisions = []
    provider_differences = []
    for expected, values in ((True, accepted), (False, rejected)):
        for lexical in values:
            payload = document(f'<t:created i:type="t:W3CDTF">{lexical}</t:created>')
            external = independent_valid(payload)
            libxml_valid = libxml.valid(payload)
            native = invoke([args.probe, 'roundtrip'], payload)
            if libxml_valid != expected or (native.returncode == 0) != expected:
                raise AssertionError(f'Date decision mismatch for {lexical!r}: libxml={libxml_valid}, native={native.returncode}')
            # .NET Framework's DateTime-backed validator has narrower midnight/year limits than XSD1.0.
            if external != expected:
                provider_differences.append({'date': lexical, 'dotnet': external, 'libxml': libxml_valid})
            if expected and not libxml.valid(native.stdout):
                raise AssertionError(f'Rewritten date is schema-invalid: {lexical!r}')
            decisions.append({'date': lexical, 'accepted': expected})
    # Prose adds restrictions beyond XSD: xsi:type is required for dates and xml:lang is prohibited on titles.
    for body in ('<t:created>2005-06-12</t:created>', '<d:title xml:lang="en">title</d:title>'):
        payload = document(body)
        if not independent_valid(payload):
            raise AssertionError('Expected schema-only oracle to admit this prose-restricted control')
        if invoke([args.probe, 'roundtrip'], payload).returncode == 0:
            raise AssertionError('Native adapter ignored an additional normative prose requirement')
    for body in ('<p:contentType>legacy</p:contentType>',
                 '<d:title>first</d:title><d:title>second</d:title>',
                 '<p:keywords><p:other/></p:keywords>', '<p:keywords xml:lang="bad_underscore"/>'):
        payload = document(body)
        if independent_valid(payload) or invoke([args.probe, 'roundtrip'], payload).returncode == 0:
            raise AssertionError('Expected both implementations to reject invalid fifth-edition metadata')
    packaged = invoke([args.probe, 'pack'], sample.stdout)
    if packaged.returncode:
        raise AssertionError('Native OPC composition failed')
    with zipfile.ZipFile(io.BytesIO(packaged.stdout)) as archive:
        if archive.testzip() is not None:
            raise AssertionError('Native OPC archive contains a corrupt ZIP entry')
        manifest = ET.fromstring(archive.read('[Content_Types].xml'))
        if not any(item.attrib.get('ContentType') == CONTENT_TYPE for item in manifest):
            raise AssertionError('Missing standard core-properties content type')
        links = ET.fromstring(archive.read('_rels/.rels'))
        selected = [item for item in links if item.attrib['Type'] == RELATIONSHIP]
        if len(selected) != 1 or selected[0].attrib.get('TargetMode', 'Internal') != 'Internal':
            raise AssertionError('Invalid core-properties relationship identity/cardinality/mode')
        payload = archive.read(selected[0].attrib['Target'].lstrip('/'))
        if not independent_valid(payload):
            raise AssertionError('Native OPC payload failed official-schema validation')
    consumed = invoke([args.probe, 'unpack'], package_bytes(sample.stdout))
    if consumed.returncode or ET.tostring(ET.fromstring(consumed.stdout)) != ET.tostring(ET.fromstring(sample.stdout)):
        raise AssertionError('Native consumer changed the independent producer metadata')
    (build / 'native.xml').write_bytes(sample.stdout)
    (build / 'native.opc').write_bytes(packaged.stdout)
    libxml.close()
    summary = {'edition': 'ECMA-376 Part2 fifth edition December2021', 'schema_sha256': hashes,
               'date_decisions': decisions, 'additional_prose_decisions': 2,
               'other_schema_rejections': 4, 'independent_package_exchanges': 2,
               'provider_differences': provider_differences,
               'python': sys.version, 'oracles':
               (['.NET Framework System.Xml.XmlSchemaSet'] if args.oracle_profile == 'dotnet-libxml' else []) +
               [str(args.libxml2)]}
    (build / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print(f'PASS: {len(decisions)} date oracle decisions, 2 prose controls, 4 invalid schema controls, 2 OPC exchanges')


def main():
    """Keep downloaded schema inputs explicit and all compiled/generated oracle artifacts in ignored output."""
    if os.name == 'nt':
        ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--schema-directory', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--libxml2', type=Path, required=True)
    parser.add_argument('--oracle-profile', choices=('dotnet-libxml', 'libxml'), default='dotnet-libxml')
    parser.add_argument('--csc', type=Path, default=Path(os.environ.get('WINDIR', 'C:/Windows')) /
                        'Microsoft.NET/Framework64/v4.0.30319/csc.exe')
    execute(parser.parse_args())


if __name__ == '__main__':
    main()
