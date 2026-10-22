# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify native package XML companions with independent ZIP and expanded-name observations."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET
import zipfile

from lxml import etree

ROOT = Path(__file__).resolve().parents[1]
MODEL = '''serializer version 1.21.0;
namespace package_graph {
  xml(namespace("urn:package-graph"));
  class child { public string value xml(qname); }
  class payload { public array<child> children; }
  package archive {
    part main : payload { name("/main.xml"); content_type("application/vnd.serializer.graph"); relationship("urn:serializer:graph:main"); policy(defaults); }
    part copies : payload { name(dynamic); content_type("application/vnd.serializer.graph"); source(main); relationship("urn:serializer:graph:copy"); policy(defaults); occurs(2,2); }
  }
}
'''
CONFIG = '''[xml.class.package_graph::payload]
extension.foreign.kind=attribute
extension.foreign.namespaces=urn:foreign
extension.foreign.policy=preserve
extension.foreign.minimum=1
extension.foreign.maximum=1
extension.tail.kind=element
extension.tail.namespaces=urn:foreign
extension.tail.policy=preserve
extension.tail.minimum=1
extension.tail.maximum=1
[package.package_graph::archive]
main.requires_type=true
main.vocabularies=urn:package-graph,urn:understood
copies.requires_type=true
copies.vocabularies=urn:package-graph,urn:understood
'''
CT = 'http://schemas.openxmlformats.org/package/2006/content-types'
RELS = 'http://schemas.openxmlformats.org/package/2006/relationships'


def payload(label):
    """Give two same-name child occurrences distinct lexical QName identities and opaque root state."""
    return (f'<g:payload xmlns:g="urn:package-graph" xmlns:p="urn:{label}:first" xmlns:f="urn:foreign" '
            'xmlns:t="urn:serializer:native:package_graph" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" '
            'xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006" xmlns:k="urn:understood" '
            f'xsi:type="t:payload" mc:MustUnderstand="k" f:label="{label}">'
            '<g:children><g:value>p:first</g:value></g:children>'
            f'<g:children xmlns:p="urn:{label}:second"><g:value>p:second</g:value></g:children>'
            '<f:tail opaque="p:unchanged">opaque text</f:tail></g:payload>').encode()


def author(path, replacement=None):
    """Create ordinary OPC parts and source-scoped relationships without generated package code."""
    values = {'/input/main.xml': payload('main'), '/copies/a.xml': payload('a'), '/copies/b.xml': payload('b')}
    if replacement is not None:
        values['/input/main.xml'] = replacement
    types = ET.Element('{' + CT + '}Types')
    for name in values:
        ET.SubElement(types, '{' + CT + '}Override', PartName=name, ContentType='application/vnd.serializer.graph')
    for name in ('/_rels/.rels', '/input/_rels/main.xml.rels'):
        ET.SubElement(types, '{' + CT + '}Override', PartName=name, ContentType='application/vnd.openxmlformats-package.relationships+xml')
    root = ET.Element('{' + RELS + '}Relationships')
    ET.SubElement(root, '{' + RELS + '}Relationship', Id='main', Type='urn:serializer:graph:main', Target='input/main.xml')
    copies = ET.Element('{' + RELS + '}Relationships')
    for label in ('a', 'b'):
        ET.SubElement(copies, '{' + RELS + '}Relationship', Id=label, Type='urn:serializer:graph:copy', Target='../copies/' + label + '.xml')
    with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        archive.writestr('[Content_Types].xml', ET.tostring(types))
        archive.writestr('_rels/.rels', ET.tostring(root))
        archive.writestr('input/_rels/main.xml.rels', ET.tostring(copies))
        for name, value in values.items():
            archive.writestr(name[1:], value)


def observe(path, codec, canonical, renamed=False):
    """Resolve QName identities in original scopes and associate every preserved payload with its physical part."""
    main = 'main.xml' if canonical else 'input/main.xml'
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
        for name, label in ((main, 'main'), ('moved/a.xml' if renamed else 'copies/a.xml', 'a'), ('copies/b.xml', 'b')):
            data = archive.read(name)
            if codec == 'json':
                assert json.loads(data) == {'children': [{'value': 'p:first'}, {'value': 'p:second'}]}
                continue
            root = etree.fromstring(data)
            assert root.tag == '{urn:package-graph}payload'
            assert root.attrib['{urn:foreign}label'] == label
            children = root.findall('{urn:package-graph}children')
            assert len(children) == 2
            for child, suffix in zip(children, ('first', 'second')):
                value = child.find('{urn:package-graph}value')
                prefix, local = value.text.split(':')
                assert (value.nsmap[prefix], local) == ('urn:' + label + ':' + suffix, suffix)
            tail = root.find('{urn:foreign}tail')
            assert tail is not None and tail.attrib['opaque'] == 'p:unchanged' and tail.text == 'opaque text'
            assert tail.nsmap['p'] == 'urn:' + label + ':first'
        relname = '_rels/main.xml.rels' if canonical else 'input/_rels/main.xml.rels'
        links = etree.fromstring(archive.read(relname))
        assert len(links) == 2 and {link.attrib['Type'] for link in links} == {'urn:serializer:graph:copy'}
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    """Build a real native consumer and inspect every successful published generation independently."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--cc', default='cl' if os.name == 'nt' else 'cc')
    parser.add_argument('--provider-prefix', type=Path)
    parser.add_argument('--language', choices=('c', 'rust'), default='c')
    parser.add_argument('--cargo', default='cargo')
    args = parser.parse_args()
    build = args.build.resolve(); build.mkdir(parents=True, exist_ok=True)
    commands = []; environment = dict(os.environ)
    def run(command, stage):
        """Retain full tool evidence and surface a failure only after its exact invocation is saved."""
        result = subprocess.run(list(map(str, command)), cwd=build, capture_output=True, text=True, encoding='utf-8', env=environment)
        commands.append({'stage': stage, 'argv': list(map(str, command)), 'exit_code': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr})
        (build / 'commands.json').write_text(json.dumps(commands, indent=2))
        if result.returncode:
            raise RuntimeError(stage + ': ' + result.stdout + result.stderr)
        return result.stdout
    (build / 'model.serializer').write_text(MODEL)
    (build / 'profile.ini').write_text(CONFIG)
    source = build / ('schema.h' if args.language == 'c' else 'schema.rs')
    run([args.compiler.resolve(), '--input', build / 'model.serializer', '--config', build / 'profile.ini', '--language', args.language, '--output', source], 'generation')
    text = source.read_text()
    if args.language == 'rust':
        prefix = re.search(r'// Generated Rust package helper module: (\w+)', text).group(1)
        (build / 'main.rs').write_text((ROOT / 'test/resources/native_package_xml_graph_main.rs').read_text().replace('@p@', prefix))
        for filename in ('Cargo.toml', 'Cargo.lock'):
            shutil.copyfile(ROOT / 'example/xml/rust' / filename, build / filename)
        command = [args.cargo, 'build', '--locked', '--offline', '--release', '--manifest-path', build / 'Cargo.toml']
        executable = build / 'target/release' / ('serializer-xml-example.exe' if os.name == 'nt' else 'serializer-xml-example')
    else:
        prefix = re.search(r'/\* Generated C package prefix: (\w+) \*/', text).group(1)
        selected = re.search(r'/\* Generated C selected-codec prefix: (\w+) \*/', text).group(1)
        (build / 'main.c').write_text((ROOT / 'test/resources/native_package_xml_graph_main.c').read_text().replace('@p@', prefix).replace('@selected@', selected))
        executable = build / ('consumer.exe' if os.name == 'nt' else 'consumer')
        if Path(args.cc).name.lower() in ('cl', 'cl.exe'):
            command = [args.cc, '/nologo', '/std:c11', '/W4', '/WX', '/O2', '/D_CRT_SECURE_NO_WARNINGS', '/DSERIALIZER_COMPRESSION_ZLIB=1', build / 'main.c', '/Fe' + str(executable), '/Fo' + str(build / 'main.obj')]
            if args.provider_prefix:
                provider = args.provider_prefix.resolve(); environment['PATH'] = str(provider / 'bin') + os.pathsep + environment.get('PATH', '')
                command += ['/I' + str(provider / 'include'), '/I' + str(provider / 'include/libxml2'), '/link', '/LIBPATH:' + str(provider / 'lib')]
            command += ['libxml2.lib', 'z.lib']
        else:
            command = [args.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2', '-DSERIALIZER_COMPRESSION_ZLIB=1', build / 'main.c', '-o', executable]
            command += subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libxml-2.0'], text=True).split() + ['-lz', '-lm']
    run(command, 'compile')
    original = payload('main')
    probes = [('valid', original, True), ('missing-type', original.replace(b' xsi:type="t:payload"', b''), False),
              ('capability', original.replace(b'xmlns:k="urn:understood"', b'xmlns:k="urn:unknown"'), False),
              ('unbound', original.replace(b'p:first', b'missing:first'), False),
              ('missing-attribute', original.replace(b' f:label="main"', b''), False),
              ('wrong-tail', original.replace(b'<f:tail', b'<other:tail xmlns:other="urn:unknown"').replace(b'</f:tail>', b'</other:tail>'), False),
              ('trailing', original + b'!', False)]
    author(build / 'valid.opc')
    for name, data, valid in probes:
        author(build / (name + '.opc'), data)
        result = json.loads(run([executable, build, name, 'OK' if valid else 'LOAD_ERROR'], name).splitlines()[-1])
        assert result['decision'] == ('OK' if valid else 'LOAD_ERROR')
    outputs = {}
    for suffix, codec, canonical in (('encoded', 'xml', False), ('saved', 'xml', False), ('canonical', 'xml', True), ('native', 'json', False)):
        path = build / ('valid-' + suffix + '.opc'); outputs[path.name] = observe(path, codec, canonical)
    for suffix, codec, canonical, renamed in (('selected', 'xml', False, False), ('moved', 'xml', True, True),
                                               ('moved-native', 'json', True, True), ('moved-saved', 'xml', True, True)):
        path = build / ('valid-' + suffix + '.opc'); outputs[path.name] = observe(path, codec, canonical, renamed)
    (build / 'verification.json').write_text(json.dumps({'inputs': len(probes), 'outputs': outputs, 'compiler_sha256': hashlib.sha256(args.compiler.read_bytes()).hexdigest()}, indent=2) + '\n')
    print('PASS: ' + args.language + ' package namespace/foreign companions, repeated parts, canonical sources, native pipelines and bounded publication')


if __name__ == '__main__':
    main()
