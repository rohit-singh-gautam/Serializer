"""Qualify generated Python OPC facades against independent ZIP/XML producers and consumers."""
import argparse
import dataclasses
import importlib.util
import io
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import time
import unittest
import xml.etree.ElementTree as etree
import zipfile
import zlib

ROOT = Path(__file__).resolve().parent.parent
MODEL_MIME = 'application/vnd.serializer.test-model+xml'
CT = 'http://schemas.openxmlformats.org/package/2006/content-types'
RELS = 'http://schemas.openxmlformats.org/package/2006/relationships'
MC = 'http://schemas.openxmlformats.org/markup-compatibility/2006'


def independent_parts(extension=False):
    """Author standard OPC metadata independently, discovering the model at an alternate path."""
    parts = {
        'Alternate/root.xml': b'<model xmlns="urn:serializer:package-test" name="independent"><point x="1" y="2" z="3"/></model>',
        'Textures/raw.bin': b'\x00\x01\xfe\xff',
        '_rels/.rels': f'<Relationships xmlns="{RELS}"><Relationship Id="main" Type="urn:serializer:main-model" Target="Alternate/root.xml"/></Relationships>'.encode(),
        'Alternate/_rels/root.xml.rels': f'<Relationships xmlns="{RELS}"><Relationship Id="texture" Type="urn:serializer:attachment" Target="../Textures/raw.bin"/></Relationships>'.encode(),
    }
    types = {'/Alternate/root.xml': MODEL_MIME, '/Textures/raw.bin': 'application/octet-stream',
             '/_rels/.rels': 'application/vnd.openxmlformats-package.relationships+xml',
             '/Alternate/_rels/root.xml.rels': 'application/vnd.openxmlformats-package.relationships+xml'}
    if extension:
        parts['Extensions/vendor.xml'] = b'<vendor xmlns="urn:vendor" flag="kept"><opaque/></vendor>'
        types['/Extensions/vendor.xml'] = 'application/vnd.vendor+xml'
        parts['_rels/.rels'] = parts['_rels/.rels'].replace(b'</Relationships>',
            b'<Relationship Id="vendor" Type="urn:vendor:metadata" Target="Extensions/vendor.xml"/></Relationships>')
    parts['[Content_Types].xml'] = ('<Types xmlns="' + CT + '">' + ''.join(
        '<Override PartName="' + name + '" ContentType="' + mime + '"/>' for name, mime in types.items()) + '</Types>').encode()
    return parts


def independent_zip(parts, descriptor=False, wide=False, attributes=None):
    """Use Python's independent ZIP provider, including nonseekable ZIP64 descriptor output."""
    class sink:
        """Provide an intentionally nonseekable sink to exercise streaming descriptors."""
        def __init__(self):
            """Own the independent producer's output."""
            self.data = bytearray()
        def write(self, data):
            """Append a complete provider write."""
            self.data.extend(data)
            return len(data)
        def flush(self):
            """Satisfy the ZIP provider's nonowning sink contract."""
    output = sink() if descriptor else io.BytesIO()
    with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for name, payload in parts.items():
            item = name
            if attributes is not None and name in attributes:
                item = zipfile.ZipInfo(name)
                item.create_system, item.external_attr = attributes[name]
                item.compress_type = zipfile.ZIP_DEFLATED
            with archive.open(item, 'w', force_zip64=wide) as entry:
                entry.write(payload)
    return bytes(output.data) if descriptor else output.getvalue()


def wire_descriptor_zip(payload, wide=False, signed=True, trailing=b''):
    """Author descriptor forms directly from ZIP wire fields, independently of either implementation."""
    name, flags, method = b'data.bin', 0x808, 8
    encoder = zlib.compressobj(wbits=-15)
    compressed = encoder.compress(payload) + encoder.flush() + trailing
    crc, sentinel = zlib.crc32(payload), 0xffffffff
    extra = struct.pack('<HHQQ', 1, 16, 0, 0) if wide else b''
    output = struct.pack('<4s5H3I2H', b'PK\x03\x04', 45 if wide else 20, flags, method, 0, 33,
                         0, sentinel if wide else 0, sentinel if wide else 0, len(name), len(extra)) + name + extra + compressed
    output += (b'PK\x07\x08' if signed else b'') + struct.pack('<IQQ' if wide else '<III', crc, len(compressed), len(payload))
    offset = len(output)
    extra = struct.pack('<HHQQ', 1, 16, len(payload), len(compressed)) if wide else b''
    central = struct.pack('<4s6H3I5H2I', b'PK\x01\x02', 45, 45 if wide else 20, flags, method, 0, 33,
                          crc, sentinel if wide else len(compressed), sentinel if wide else len(payload),
                          len(name), len(extra), 0, 0, 0, 0, 0) + name + extra
    return output + central + struct.pack('<4s4H2IH', b'PK\x05\x06', 0, 0, 1, 1, len(central), offset, 0)


class python_package_test(unittest.TestCase):
    """Exercise actual emitted models; no handwritten codec substitutes participate."""
    def fixture(self):
        """Build one generated typed model and caller-supplied binary occurrence."""
        model = self.module.ProfileModel()
        model.name = 'generated & checked'
        point = self.module.ProfilePoint()
        point.x, point.y, point.z = 1.0, 2.0, 3.0
        model.points = [point]
        return self.module.ProfileArchive.Input(main=model, attachments=(
            self.module.BinaryPart(b'\x00\x01\xfe\xff', '/Textures/raw.bin', 'application/octet-stream'),))

    def test_independent_producers_and_generated_consumer(self):
        """Read ZIP32 and forced ZIP64 local headers, with and without descriptors and seekability."""
        for descriptor in (False, True):
            for wide in (False, True):
                archive = independent_zip(independent_parts(), descriptor, wide)
                for source in (archive, io.BytesIO(archive)):
                    result = self.module.ProfileArchive.load(source)
                    self.assertEqual(result.value.main.name, 'independent')
                    self.assertEqual(result.value.main.points[0].z, 3.0)
                    self.assertEqual(result.value.attachments[0].data, b'\x00\x01\xfe\xff')

    def test_generated_output_independent_reader_and_zip64_directory(self):
        """Inspect generated ZIP64 directory/locator, descriptors and XML through independent providers."""
        for wide in (False, True):
            data = self.module.ProfileArchive.encode(self.fixture(), self.module.PackageLimits(force_zip64=wide))
            with zipfile.ZipFile(io.BytesIO(data)) as archive:
                self.assertIsNone(archive.testzip())
                self.assertEqual(archive.read('Textures/raw.bin'), b'\x00\x01\xfe\xff')
                self.assertEqual(etree.fromstring(archive.read('3D/model.xml')).attrib['name'], 'generated & checked')
                self.assertEqual(etree.fromstring(archive.read('[Content_Types].xml')).tag, '{' + CT + '}Types')
                self.assertTrue(all(item.flag_bits & 8 for item in archive.infolist()))
            self.assertEqual(b'PK\x06\x06' in data, wide)
            self.assertEqual(self.module.ProfileArchive.load(data).value.main.name, 'generated & checked')

    def test_typed_media_parameters_keep_case_sensitive_value_identity(self):
        """Match names/order/quoted pairs while rejecting changed case-sensitive profile values."""
        for mime in (MODEL_MIME + ';profile=Case;mode=read',
                     MODEL_MIME.upper() + ';MODE="read";PROFILE="C\\ase"'):
            parts = independent_parts()
            escaped = mime.replace('&', '&amp;').replace('"', '&quot;')
            parts['[Content_Types].xml'] = parts['[Content_Types].xml'].replace(MODEL_MIME.encode(), escaped.encode())
            result = self.parameterized.ProfileArchive.load(independent_zip(parts))
            self.assertEqual(result.value.main.name, 'independent')
        for mime in (MODEL_MIME + ';profile=case;mode=read', MODEL_MIME + ';profile=Case;mode=READ'):
            parts = independent_parts()
            parts['[Content_Types].xml'] = parts['[Content_Types].xml'].replace(MODEL_MIME.encode(), mime.encode())
            with self.assertRaises(self.parameterized.PackageError): self.parameterized.ProfileArchive.load(independent_zip(parts))
        budget = self.module.PackageBudget()
        self.assertTrue(self.module._pkg_mime_equal('application/test;a=x;a=y', 'application/test;a=x;a=y', budget))
        self.assertFalse(self.module._pkg_mime_equal('application/test;a=x;a=y', 'APPLICATION/TEST;A=y;a=x', budget))

    def test_metadata_encodings_and_relationship_part_positions(self):
        """Accept BOM-marked UTF16 infrastructure without misclassifying ordinary _rels descendants."""
        metadata = ('[Content_Types].xml', '_rels/.rels', 'Alternate/_rels/root.xml.rels')
        for encoding, bom in (('utf-16-le', b'\xff\xfe'), ('utf-16-be', b'\xfe\xff')):
            for declaration in ('', '<?xml version="1.0" encoding="UTF-16"?>'):
                parts = independent_parts()
                for name in metadata:
                    parts[name] = bom + (declaration + parts[name].decode()).encode(encoding)
                self.assertEqual(self.module.ProfileArchive.load(independent_zip(parts)).value.main.name, 'independent')
        for payload in (independent_parts()['[Content_Types].xml'].decode().encode('utf-16-le'),
                        b'\xff\xfe' + ('<?xml version="1.0" encoding="UTF-8"?>' + independent_parts()['[Content_Types].xml'].decode()).encode('utf-16-le'),
                        b'\xff\xfe' + ('<?xml version="1.0" encoding="UTF-16LE"?>' + independent_parts()['[Content_Types].xml'].decode()).encode('utf-16-le'),
                        independent_parts()['[Content_Types].xml'].decode().encode('utf-32'),
                        ('<?xml version="1.0" encoding="ISO-8859-1"?>' + independent_parts()['[Content_Types].xml'].decode()).encode()):
            parts = independent_parts()
            parts['[Content_Types].xml'] = payload
            with self.assertRaises(self.module.PackageError): self.module.ProfileArchive.load(independent_zip(parts))
        parts = independent_parts()
        parts['_rels/nested/plain.rels'] = b'opaque payload'
        parts['[Content_Types].xml'] = parts['[Content_Types].xml'].replace(b'</Types>',
            b'<Override PartName="/_rels/nested/plain.rels" ContentType="application/octet-stream"/></Types>')
        result = self.module.ProfileArchive.load(independent_zip(parts), policy=self.module.ExtensionPolicy('preserve', 'preserve'))
        self.assertEqual(result.extensions.parts[0].name, '/_rels/nested/plain.rels')
        output = self.module.ProfileArchive.encode(self.module.ProfileArchive.Input(result.value.main, result.value.attachments, result.extensions))
        with zipfile.ZipFile(io.BytesIO(output)) as archive:
            self.assertEqual(archive.read('_rels/nested/plain.rels'), b'opaque payload')

    def test_physical_nonparts_are_excluded_but_integrity_checked(self):
        """Ignore directories and discarded logical items without skipping CRC, budgets or target checks."""
        parts = independent_parts()
        excluded = {'Textures/': b'', 'unused/': b'', '[trash]/0000.dat': b'discarded data',
                    '[trash]/0000.dat/child.bin': b'ignored descendant', '[trash]/CASE': b'first',
                    '[trash]/case': b'second', 'has space.bin': b'spaced name',
                    'invalid%ZZ.bin': b'bad escape', 'trailing./item.bin': b'bad segment',
                    '[trash]/caf\u00e9.bin': b'not a logical part', 'dos-dir.bin': b'directory payload',
                    'dos-volume.bin': b'volume label', 'ntfs-dir.bin': b'NTFS directory',
                    'vfat-dir.bin': b'VFAT directory', 'unix-link.bin': b'symlink target',
                    'unix-dir.bin': b'Unix directory', 'darwin-socket.bin': b'socket data'}
        attributes = {'dos-dir.bin': (0, 0x10), 'dos-volume.bin': (0, 0x08),
                      'ntfs-dir.bin': (10, 0x10), 'vfat-dir.bin': (14, 0x10),
                      'unix-link.bin': (3, 0o120777 << 16), 'unix-dir.bin': (3, 0o040755 << 16),
                      'darwin-socket.bin': (19, 0o140755 << 16), 'Textures/raw.bin': (3, 0o644 << 16)}
        parts.update(excluded)
        data = independent_zip(parts, attributes=attributes)
        budget = self.module.PackageBudget()
        result = self.module.ProfileArchive.load(data, budget=budget)
        self.assertEqual(result.value.main.name, 'independent')
        self.assertFalse(result.extensions.parts)
        self.assertEqual(budget.used['preserved_bytes'], 0)
        self.assertGreaterEqual(budget.used['expanded_bytes'], sum(map(len, parts.values())))
        output = self.module.ProfileArchive.encode(self.module.ProfileArchive.Input(result.value.main, result.value.attachments))
        with zipfile.ZipFile(io.BytesIO(output)) as archive:
            self.assertTrue(set(excluded).isdisjoint(archive.namelist()))
        corrupted = bytearray(data)
        source = self.module._PackageSource(data, self.module.PackageBudget())
        try:
            archive = self.module._PackageZip(source, source.budget)
            entry = next(entry for entry in archive.physical_entries if entry[0] == '/[trash]/case')
            corrupted[entry[5]] ^= 1
        finally:
            source.close()
        with self.assertRaises(self.module.PackageError): self.module.ProfileArchive.load(corrupted)
        with self.assertRaises(self.module.PackageError):
            self.module.ProfileArchive.load(data, self.module.PackageLimits(max_total_expanded_bytes=budget.used['expanded_bytes'] - 1))
        linked = dict(parts)
        linked['_rels/.rels'] = linked['_rels/.rels'].replace(b'Alternate/root.xml', b'%5Btrash%5D/0000.dat')
        with self.assertRaises(self.module.PackageError): self.module.ProfileArchive.load(independent_zip(linked, attributes=attributes))
        for name in ('caf\u00e9.bin', 'caf%C3%A9.bin'):
            unsupported = dict(parts, **{name: b'possible non-ASCII logical part'})
            with self.assertRaises(self.module.PackageError) as caught:
                self.module.ProfileArchive.load(independent_zip(unsupported, attributes=attributes))
            self.assertEqual(caught.exception.code, 'unsupported_feature')
        orphan = independent_parts()
        orphan['Missing/_rels/file.bin.rels'] = ('<Relationships xmlns="' + RELS + '"/>').encode()
        orphan['[Content_Types].xml'] = orphan['[Content_Types].xml'].replace(b'</Types>',
            b'<Override PartName="/Missing/_rels/file.bin.rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/></Types>')
        with self.assertRaises(self.module.PackageError): self.module.ProfileArchive.load(independent_zip(orphan))
        for name in ('[Content_Types].xml', 'Alternate/root.xml'):
            with self.assertRaises(self.module.PackageError):
                self.module.ProfileArchive.load(independent_zip(independent_parts(), attributes={name: (0, 0x10)}))
        orphan['Missing/file.bin'] = b'nonfile source'
        with self.assertRaises(self.module.PackageError):
            self.module.ProfileArchive.load(independent_zip(orphan, attributes={'Missing/file.bin': (0, 0x10)}))

    def test_relationships_markup_compatibility_scopes_and_choices(self):
        """Apply inherited MCE identities and ordered fallback selection before OPC graph validation."""
        root = f'<Relationships xmlns="{RELS}" xmlns:r="{RELS}" xmlns:mc="{MC}" xmlns:e="urn:extension" '
        relationship = '<Relationship Id="{id}" Type="{kind}" Target="Alternate/root.xml"/>'
        body = (root + 'mc:Ignorable="e" mc:ProcessContent="e:box" mc:MustUnderstand="r" e:flag="ignored">'
            '<e:discard mc:MustUnderstand="e">unknown<bad/>content</e:discard>'
            '<e:box>' + relationship.format(id='main', kind='urn:serializer:main-model').replace('/>', '>before<e:discard/>after</Relationship>') + '</e:box>'
            '<mc:AlternateContent e:flag="ignored"><mc:Choice Requires="e" mc:MustUnderstand="e"><bad/></mc:Choice>'
            '<mc:Choice Requires="r">' + relationship.format(id='choice', kind='urn:vendor:link') + '</mc:Choice>'
            '<mc:Fallback mc:MustUnderstand="e"><bad/></mc:Fallback><e:future/></mc:AlternateContent>'
            '<mc:AlternateContent><mc:Choice Requires="e"><bad/></mc:Choice><mc:Fallback>' +
            relationship.format(id='fallback', kind='urn:vendor:link') + '</mc:Fallback></mc:AlternateContent>'
            '<mc:AlternateContent><mc:Choice Requires="e"><bad/></mc:Choice></mc:AlternateContent>'
            '<e:box xmlns:e="urn:rebound" mc:Ignorable="e" mc:ProcessContent="e:*">' +
            relationship.format(id='rebound', kind='urn:vendor:link') + '</e:box></Relationships>')
        body = body.replace('Type="urn:vendor:link" Target="Alternate/root.xml"', 'Type="urn:vendor:link" Target="Textures/raw.bin"')
        parts = independent_parts()
        parts['_rels/.rels'] = body.encode()
        policy = self.module.ExtensionPolicy('preserve', 'preserve')
        shared = self.module.PackageBudget()
        result = self.module.ProfileArchive.load(independent_zip(parts), policy=policy, budget=shared)
        self.assertEqual(result.value.main.name, 'independent')
        self.assertEqual([link.id for link in result.extensions.relationships], ['choice', 'fallback', 'rebound'])
        output = self.module.ProfileArchive.encode(self.module.ProfileArchive.Input(
            result.value.main, result.value.attachments, result.extensions))
        with zipfile.ZipFile(io.BytesIO(output)) as archive:
            self.assertNotIn(MC.encode(), archive.read('_rels/.rels'))
            self.assertEqual(len(etree.fromstring(archive.read('_rels/.rels'))), 4)
        for resource in ('work_units', 'decoder_allocation_bytes'):
            limits = self.module.PackageLimits(**{'max_' + resource: shared.used[resource] - 1})
            with self.assertRaises(self.module.PackageError):
                self.module.ProfileArchive.load(independent_zip(parts), limits, policy=policy)
            self.module.ProfileArchive.load(independent_zip(independent_parts()), limits)
        parts = independent_parts()
        parts['_rels/.rels'] = (f'<mc:AlternateContent xmlns:mc="{MC}" xmlns:r="{RELS}"><mc:Choice Requires="xml"><bad/></mc:Choice><mc:Fallback>'
            '<r:Relationships>' + relationship.format(id='main', kind='urn:serializer:main-model').replace('<Relationship', '<r:Relationship') +
            '</r:Relationships></mc:Fallback></mc:AlternateContent>').encode()
        self.assertEqual(self.module.ProfileArchive.load(independent_zip(parts)).value.main.name, 'independent')

    def test_relationships_markup_compatibility_rejects_invalid_live_contracts(self):
        """Reject malformed scope, wrapper, capability and ordering contracts without weakening OPC."""
        beginning = f'<Relationships xmlns="{RELS}" xmlns:r="{RELS}" xmlns:mc="{MC}" xmlns:e="urn:extension" '
        for body in (
            'mc:Ignorable="missing">', 'mc:Ignorable="mc">', 'mc:ProcessContent="e:box">',
            'mc:Ignorable="e" mc:ProcessContent="e">', 'mc:Ignorable="e" mc:ProcessContent="e:bad:name">',
            'mc:MustUnderstand="e">', 'mc:MustUnderstand="xml">', 'mc:Unknown="">', '><e:unknown/>', '><mc:Choice Requires="r"/>',
            '><mc:Fallback/>', '><mc:Unknown/>', '><mc:AlternateContent/>',
            '><mc:AlternateContent><mc:Fallback/></mc:AlternateContent>',
            '><mc:AlternateContent><mc:Choice/></mc:AlternateContent>',
            '><mc:AlternateContent><mc:Choice Requires=""/></mc:AlternateContent>',
            '><mc:AlternateContent><mc:Choice Requires="missing"/></mc:AlternateContent>',
            '><mc:AlternateContent><mc:Choice Requires="mc"/></mc:AlternateContent>',
            '><mc:AlternateContent><mc:Choice Requires="r"/><mc:Fallback/><mc:Fallback/></mc:AlternateContent>',
            '><mc:AlternateContent><mc:Choice Requires="r"/><mc:Fallback/><mc:Choice Requires="r"/></mc:AlternateContent>',
            '><mc:AlternateContent bad="x"><mc:Choice Requires="r"/></mc:AlternateContent>',
            '><mc:AlternateContent xml:lang="en"><mc:Choice Requires="r"/></mc:AlternateContent>',
            '><mc:AlternateContent><mc:Choice Requires="r" e:flag="x"/></mc:AlternateContent>',
            '><mc:AlternateContent><mc:Choice Requires="r" mc:MustUnderstand="e"/></mc:AlternateContent>',
            'mc:Ignorable="e" mc:ProcessContent="e:*"><e:box xml:base="x"/>',
            'mc:Ignorable="e" mc:ProcessContent="e:*"><e:box xml:lang="en"/>',
            'mc:Ignorable="e" mc:ProcessContent="e:*"><e:box xml:space="preserve"/>',
            'mc:Ignorable="e" mc:ProcessContent="e:*"><mc:AlternateContent><e:box/><mc:Choice Requires="r"/></mc:AlternateContent>',
        ):
            with self.subTest(body=body):
                parts = independent_parts()
                main = '<Relationship Id="main" Type="urn:serializer:main-model" Target="Alternate/root.xml"/>'
                parts['_rels/.rels'] = (beginning + body + main + '</Relationships>').encode()
                with self.assertRaises(ValueError): self.module.ProfileArchive.load(independent_zip(parts))

    def test_descriptor_forms_and_exact_inflater(self):
        """Reject bad CRC and trailing compressed bytes while accepting all four descriptor forms."""
        for wide in (False, True):
            for signed in (False, True):
                for payload in (b'descriptor' * 100, bytes.fromhex('ac0a7ad5')):
                    data = wire_descriptor_zip(payload, wide, signed)
                    shared = self.module.PackageBudget()
                    source = self.module._PackageSource(data, shared)
                    try:
                        self.assertEqual(self.module._PackageZip(source, shared).read('/data.bin'), payload)
                    finally:
                        source.close()
                data = wire_descriptor_zip(payload, wide, signed, b'trailing')
                shared = self.module.PackageBudget()
                source = self.module._PackageSource(data, shared)
                try:
                    with self.assertRaises(self.module.PackageError):
                        self.module._PackageZip(source, shared).read('/data.bin')
                finally:
                    source.close()

    def test_integrity_structure_and_names(self):
        """Reject malformed lengths, aliases, escaping relationships and CRC errors before publication."""
        original = independent_zip(independent_parts())
        for data in (original[:-1], original + b'trailing', original[:50]):
            with self.assertRaises((self.module.PackageError, ValueError)):
                self.module.ProfileArchive.load(data)
        parts = independent_parts()
        parts['alternate/ROOT.xml'] = parts['Alternate/root.xml']
        with self.assertRaises(self.module.PackageError):
            self.module.ProfileArchive.load(independent_zip(parts))
        parts = independent_parts()
        parts['_rels/.rels'] = parts['_rels/.rels'].replace(b'Alternate/root.xml', b'../../outside.xml')
        with self.assertRaises(self.module.PackageError):
            self.module.ProfileArchive.load(independent_zip(parts))
        data = bytearray(self.module.ProfileArchive.encode(self.fixture()))
        source = self.module._PackageSource(data, self.module.PackageBudget())
        try:
            entry = self.module._PackageZip(source, source.budget).entries['/textures/raw.bin']
            data[entry[5]] ^= 1
        finally:
            source.close()
        with self.assertRaises((self.module.PackageError, zlib.error)):
            self.module.ProfileArchive.load(data)
        commented = bytearray(original)
        comment = b'legal comment containing PK\x05\x06 inside'
        struct.pack_into('<H', commented, len(commented) - 2, len(comment))
        commented.extend(comment)
        self.assertEqual(self.module.ProfileArchive.load(commented).value.main.name, 'independent')

    def test_extensions_and_domain_validation(self):
        """Retain whole unknown parts and links, reject required capabilities and revalidate edited output."""
        data = independent_zip(independent_parts(True))
        with self.assertRaises(self.module.PackageError):
            self.module.ProfileArchive.load(data)
        policy = self.module.ExtensionPolicy('preserve', 'preserve')
        result = self.module.ProfileArchive.load(data, policy=policy)
        self.assertEqual(result.extensions.parts[0].data, independent_parts(True)['Extensions/vendor.xml'])
        rewritten = self.module.ProfileArchive.encode(self.module.ProfileArchive.Input(
            result.value.main, result.value.attachments, result.extensions))
        with zipfile.ZipFile(io.BytesIO(rewritten)) as archive:
            self.assertEqual(archive.read('Extensions/vendor.xml'), independent_parts(True)['Extensions/vendor.xml'])
        with self.assertRaises(self.module.PackageError):
            self.module.ProfileArchive.load(data, policy=self.module.ExtensionPolicy('preserve', 'preserve', lambda part: True))
        selected = []
        def validate(candidate, shared):
            """Charge domain work through the same context and fail the unpublished candidate."""
            selected.append(candidate.main.name)
            shared.consume('references', 1)
            raise ValueError('domain validation rejected')
        with self.assertRaisesRegex(ValueError, 'domain validation rejected'):
            self.module.ProfileArchive.load(data, policy=policy, validate=validate)
        self.assertEqual(selected, ['independent'])

    def test_shared_limits_cancellation_and_atomic_save(self):
        """A middle-stage failure preserves old final bytes and one budget spans ZIP, OPC and generated XML."""
        for limits in (self.module.PackageLimits(max_entries=1), self.module.PackageLimits(max_decoder_allocation_bytes=1),
                       self.module.PackageLimits(is_cancelled=lambda: True), self.module.PackageLimits(deadline=time.monotonic())):
            with self.assertRaises(self.module.PackageError):
                self.module.ProfileArchive.load(independent_zip(independent_parts()), limits)
        budget = self.module.PackageBudget()
        self.module.ProfileArchive.load(independent_zip(independent_parts()), budget=budget)
        self.assertGreater(budget.used['work_units'], 100)
        self.assertGreater(budget.used['decoder_allocation_bytes'], 100)
        destination = self.directory / 'atomic.opc'
        destination.write_bytes(b'old generation')
        def fail(stage):
            """Inject a failure at the selected publication boundary."""
            if stage == 'before_publish': raise RuntimeError('before publish')
        with self.assertRaisesRegex(RuntimeError, 'before publish'):
            self.module.ProfileArchive.save(destination, self.fixture(), fault=fail)
        self.assertEqual(destination.read_bytes(), b'old generation')
        self.assertFalse(list(self.directory.glob('.serializer-opc-*')))
        self.module.ProfileArchive.save(destination, self.fixture())
        self.assertEqual(self.module.ProfileArchive.load(destination).value.main.name, 'generated & checked')


def main():
    """Generate the actual facade with the selected compiler, import it, and run independent qualification."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    build = args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository scratch must remain under ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='python-opc-', dir=build) as temporary:
        directory = Path(temporary)
        generated = directory / 'generated.py'
        subprocess.run([str(args.compiler.resolve()), '--input', str(ROOT / 'test/resources/opc_profile.serializer'),
                        '--language', 'python', '--python.output', str(generated)], check=True)
        spec = importlib.util.spec_from_file_location('serializer_python_opc_fixture', generated)
        module = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = module
        spec.loader.exec_module(module)
        python_package_test.module = module
        python_package_test.directory = directory
        parameterized_source = directory / 'parameterized.serializer'
        parameterized_source.write_text((ROOT / 'test/resources/opc_profile.serializer').read_text(encoding='utf-8').replace(
            MODEL_MIME, MODEL_MIME + ';profile=Case;mode=read'), encoding='utf-8')
        parameterized_output = directory / 'parameterized.py'
        subprocess.run([str(args.compiler.resolve()), '--input', str(parameterized_source), '--language', 'python',
                        '--python.output', str(parameterized_output)], check=True)
        spec = importlib.util.spec_from_file_location('serializer_python_opc_parameter_fixture', parameterized_output)
        parameterized = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = parameterized
        spec.loader.exec_module(parameterized)
        python_package_test.parameterized = parameterized
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(python_package_test))
        if not result.wasSuccessful(): raise SystemExit(1)


if __name__ == '__main__':
    main()
