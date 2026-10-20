# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify real generated Rust ZIP/OPC, profile selection and application references."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import time
import zipfile

import python_package_test as independent
import package_application_contract_test as application_contract

ROOT = Path(__file__).resolve().parents[1]


def application_fixtures(build):
    """Reuse the independent application corpus and relocate its fixed singleton without changing IDs."""
    directory = build / 'application'
    rows = application_contract.build_fixtures(directory)
    output = io.BytesIO()
    with zipfile.ZipFile(directory / 'valid.opc') as source, zipfile.ZipFile(output, 'w') as target:
        for item in source.infolist():
            payload = source.read(item.filename)
            if item.filename in ('[Content_Types].xml', '_rels/.rels'):
                payload = payload.replace(b'fixed.xml', b'custom/fixed.xml')
            target.writestr('custom/fixed.xml' if item.filename == 'fixed.xml' else item.filename,
                            payload, compress_type=item.compress_type)
    (directory / 'relocated-singleton.opc').write_bytes(output.getvalue())
    rows.append(('relocated-singleton', 'OK'))
    with (directory / 'cases.tsv').open('a', encoding='utf-8', newline='\n') as manifest:
        manifest.write('relocated-singleton\tOK\n')
    return rows


def application_schema():
    """Include the maintained shared contract declarations in the single generated native unit."""
    return (ROOT / 'test/resources/package_application_contract.serializer').read_text(encoding='utf-8').split('\n', 1)[1]

CONTRACT = '''
namespace selected {
  xml(namespace("urn:serializer:selected"), scalars(attribute));
  class node { public string key; public uint64 id; }
  class link { public string target; public uint64 target_id; }
  class other { public string key; }
  package graph {
    part links:link { name(dynamic); content_type("application/vnd.selected-link+xml");
      relationship("urn:selected:link"); occurs(1,unbounded);
      reference("nodes",target,nodes); reference("ids",target_id,nodes); }
    part nodes:node { name(dynamic); content_type("application/vnd.selected-node+xml");
      relationship("urn:selected:node"); occurs(0,unbounded); key("nodes",key); key("ids",id); }
    part others:other { name(dynamic); content_type("application/vnd.selected-other+xml");
      relationship("urn:selected:other"); occurs(0,unbounded); key("nodes",key); }
    part alias_nodes:node { name(dynamic); content_type("application/vnd.selected-node+xml");
      relationship("urn:selected:alias-node"); occurs(0,unbounded); key("nodes",key); key("ids",id); }
  }
  package fixed { part main:node { name("/node.xml");
    content_type("application/vnd.selected-node+xml"); relationship("urn:selected:node");
    compression(deflate); policy(fixed); } }
  package defaults { part main:node { name("/node.xml");
    content_type("application/vnd.selected-node+xml"); relationship("urn:selected:node");
    compression(deflate); policy(defaults); } }
  package parameters { part main:node { name("/node.xml");
    content_type("application/vnd.selected-node+xml;profile=\\\"a  b\\\";mode=Read");
    relationship("urn:selected:node"); } }
}
'''

COMPANION_GRAPH = '''serializer version 1.21.0;
namespace companion {
  xml(namespace("urn:serializer:companion"));
  class inner { public string token xml(element,qname); }
  class outer { public inner nested; }
  package graph { part main:outer { name("/model.xml");
    content_type("application/vnd.companion+xml"); relationship("urn:companion:main"); } }
}
'''


def fixtures(build):
    """Author independent provider and raw-wire inputs, including malformed ZIP and OPC candidates."""
    directory = build / 'fixtures'
    directory.mkdir(exist_ok=True)
    rows = []

    def add(name, kind, data, valid):
        """Retain exact independent bytes and the acceptance oracle for the native SDK consumer."""
        (directory / name).write_bytes(data)
        rows.append((name, kind, str(int(valid))))

    for wide in (False, True):
        for signed in (False, True):
            for payload_name, payload in [('empty', b''), ('model', b'independent raw DEFLATE'),
                                          ('signature', bytes.fromhex('ac0a7ad5'))]:
                add(f'descriptor-{wide}-{signed}-{payload_name}.zip', 'zip',
                    independent.wire_descriptor_zip(payload, wide=wide, signed=signed), True)
            add(f'descriptor-trailing-{wide}-{signed}.zip', 'zip',
                independent.wire_descriptor_zip(b'payload', wide=wide, signed=signed, trailing=b'X'), False)
    for line in (ROOT / 'test/resources/package_malformed_fixtures/cases.txt').read_text().splitlines():
        _, name = line.split()
        add('malformed-' + name, 'zip', (ROOT / 'test/resources/package_malformed_fixtures' / name).read_bytes(), False)
    add('ordinary.zip', 'zip', independent.independent_zip({'a.bin': b'first', 'b.bin': b'second'}), True)
    add('zip64.zip', 'zip', independent.independent_zip({'a.bin': b'ZIP64'}, wide=True), True)
    ordinary = independent.independent_zip({'a.bin': b'a'})
    for name, at, word in [('count', -12, 2), ('offset', -6, 0xfffffff0)]:
        candidate = bytearray(ordinary)
        struct.pack_into('<H' if name == 'count' else '<I', candidate, len(candidate) + at, word)
        add('bad-' + name + '.zip', 'zip', candidate, False)
    add('trailing.zip', 'zip', ordinary + b'junk', False)
    count_archives = {}
    for count in (65534, 65535, 65536):
        output = io.BytesIO()
        with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_STORED, allowZip64=True) as archive:
            for index in range(count):
                archive.writestr(f'{index:08x}.bin', b'')
        count_archives[count] = output.getvalue()
        add(f'count-{count}.zip', 'zip', count_archives[count], True)
    wide = count_archives[65536]
    locator = wide.rindex(b'PK\x06\x07')
    record = struct.unpack_from('<Q', wide, locator + 8)[0]
    for label, offset, value in [('locator-offset', locator + 8, (1 << 32) + 17),
                                 ('directory-offset', record + 48, (1 << 32) + 31),
                                 ('directory-count', record + 32, (1 << 32) + 1)]:
        candidate = bytearray(wide)
        struct.pack_into('<Q', candidate, offset, value)
        if label == 'directory-count':
            struct.pack_into('<Q', candidate, record + 24, value)
        add('high-' + label + '.zip', 'zip', candidate, False)
    add('relocated.opc', 'opc', independent.independent_zip(independent.independent_parts()), True)
    add('extensions.opc', 'preserve', independent.independent_zip(independent.independent_parts(extension=True)), True)
    scoped = independent.independent_parts(extension=True)
    scoped['Alternate/_rels/root.xml.rels'] = scoped['Alternate/_rels/root.xml.rels'].replace(
        b'</Relationships>', b'<Relationship Id="scoped-vendor" Type="urn:vendor:scoped" Target="../Extensions/vendor.xml?choice=one#node"/><Relationship Id="external-vendor" Type="urn:vendor:external" Target="https://example.invalid/a?x=1#f" TargetMode="External"/></Relationships>')
    add('scoped-extensions.opc', 'preserve', independent.independent_zip(scoped), True)
    add('collection.opc', 'collection', independent.independent_zip(independent.independent_collection_parts()), True)
    add('middle-failure.opc', 'collection', independent.independent_zip(independent.independent_collection_parts(1)), False)
    for label, transform in [
        ('query-selector', lambda parts: parts.__setitem__('_rels/.rels', parts['_rels/.rels'].replace(b'Alternate/root.xml', b'Alternate/root.xml?choice=one#node'))),
        ('wrong-root', lambda parts: parts.__setitem__('Alternate/root.xml', parts['Alternate/root.xml'].replace(b'<model ', b'<other ').replace(b'</model>', b'</other>'))),
        ('wrong-type', lambda parts: parts.__setitem__('Alternate/root.xml', parts['Alternate/root.xml'].replace(b'<model ', b'<model xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:n="urn:wrong" xsi:type="n:model" '))),
        ('whitespace-type', lambda parts: parts.__setitem__('Alternate/root.xml', parts['Alternate/root.xml'].replace(b'<model ', b'<model xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:n="urn:serializer:native:profile" xsi:type=" &#x9;&#xD;&#xA;n:model &#x9;" '))),
        ('internal-whitespace-type', lambda parts: parts.__setitem__('Alternate/root.xml', parts['Alternate/root.xml'].replace(b'<model ', b'<model xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:n="urn:serializer:native:profile" xsi:type="n: model" '))),
        ('nonxml-whitespace-type', lambda parts: parts.__setitem__('Alternate/root.xml', parts['Alternate/root.xml'].replace(b'<model ', b'<model xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:n="urn:serializer:native:profile" xsi:type="&#xA0;n:model&#xA0;" '))),
        ('missing-target', lambda parts: parts.pop('Textures/raw.bin')),
        ('escape', lambda parts: parts.__setitem__('_rels/.rels', parts['_rels/.rels'].replace(b'Alternate/root.xml', b'../../escape.xml'))),
        ('unknown-core', lambda parts: parts.__setitem__('_rels/.rels', parts['_rels/.rels'].replace(b'</Relationships>', b'<Alien/></Relationships>'))),
        ('duplicate-id', lambda parts: parts.__setitem__('_rels/.rels', parts['_rels/.rels'].replace(b'</Relationships>', b'<Relationship Id="main" Type="urn:duplicate" Target="Textures/raw.bin"/></Relationships>'))),
    ]:
        parts = independent.independent_parts()
        transform(parts)
        add(label + '.opc', 'opc', independent.independent_zip(parts), label in ('query-selector', 'whitespace-type'))
    for label, media, accepted in [
        ('equivalent', 'Application/Vnd.Selected-Node+XML;MODE="Read";PROFILE="a  b"', True),
        ('case-sensitive-value', 'application/vnd.selected-node+xml;profile="a  b";mode=read', False),
        ('quoted-whitespace', 'application/vnd.selected-node+xml;profile="a b";mode=Read', False),
        ('duplicate-parameter', 'application/vnd.selected-node+xml;profile="a  b";mode=Read;MODE=Read', False),
        ('bad-media', 'application/vnd.selected-node+xml/extra;profile="a  b";mode=Read', False),
    ]:
        escaped = media.replace('&', '&amp;').replace('"', '&quot;')
        parts = {'node.xml': b'<node xmlns="urn:serializer:selected" key="node" id="9"/>',
                 '_rels/.rels': ('<Relationships xmlns="' + independent.RELS + '"><Relationship Id="n" Type="urn:selected:node" Target="node.xml"/></Relationships>').encode(),
                 '[Content_Types].xml': ('<Types xmlns="' + independent.CT + '"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Override PartName="/node.xml" ContentType="' + escaped + '"/></Types>').encode()}
        add('mime-' + label + '.opc', 'parameters', independent.independent_zip(parts), accepted)
    parts = independent.independent_parts(extension=True)
    parts['Extensions/_rels/nested/plain.rels'] = b'ordinary opaque part'
    parts['[Content_Types].xml'] = parts['[Content_Types].xml'].replace(b'</Types>', b'<Override PartName="/Extensions/_rels/nested/plain.rels" ContentType="application/octet-stream"/></Types>')
    add('nested-ordinary-rels.opc', 'preserve', independent.independent_zip(parts), True)
    parts = independent.independent_parts()
    parts['Textures'] = b'conflicting file ancestry'
    parts['[Content_Types].xml'] = parts['[Content_Types].xml'].replace(b'</Types>', b'<Override PartName="/Textures" ContentType="application/octet-stream"/></Types>')
    add('conflicting-ancestry.opc', 'preserve', independent.independent_zip(parts), False)
    for label in ('valid', 'missing', 'wrong-type', 'duplicate', 'wrong-integer'):
        nodes = [] if label in ('missing', 'wrong-type') else [('n.xml', 'name', (1 << 64) - 1)]
        if label == 'duplicate':
            nodes.append(('n2.xml', 'name', (1 << 64) - 2))
        parts = {'l.xml': ('<link xmlns="urn:serializer:selected" target="name" target_id="' +
                           str((1 << 64) - (2 if label == 'wrong-integer' else 1)) + '"/>').encode()}
        declarations = [('<Override PartName="/l.xml" ContentType="application/vnd.selected-link+xml"/>')]
        links = ['<Relationship Id="l" Type="urn:selected:link" Target="l.xml"/>']
        for index, (name, key, identity) in enumerate(nodes):
            parts[name] = f'<node xmlns="urn:serializer:selected" key="{key}" id="{identity}"/>'.encode()
            declarations.append(f'<Override PartName="/{name}" ContentType="application/vnd.selected-node+xml"/>')
            links.append(f'<Relationship Id="n{index}" Type="urn:selected:node" Target="{name}"/>')
        if label == 'wrong-type':
            parts['other.xml'] = b'<other xmlns="urn:serializer:selected" key="name"/>'
            declarations.append('<Override PartName="/other.xml" ContentType="application/vnd.selected-other+xml"/>')
            links.append('<Relationship Id="o" Type="urn:selected:other" Target="other.xml"/>')
        parts['_rels/.rels'] = ('<Relationships xmlns="' + independent.RELS + '">' + ''.join(links) + '</Relationships>').encode()
        parts['[Content_Types].xml'] = ('<Types xmlns="' + independent.CT + '"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>' + ''.join(declarations) + '</Types>').encode()
        add('graph-' + label + '.opc', 'graph', independent.independent_zip(parts), label == 'valid')
    (directory / 'cases.tsv').write_text(''.join('\t'.join(row) + '\n' for row in rows), encoding='utf-8')
    return rows


def main():
    """Generate with the built compiler, execute a strict Rust SDK, then inspect its wire output independently."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--cargo', default=shutil.which('cargo'))
    parser.add_argument('--cargo-home', type=Path)
    args = parser.parse_args()
    compiler, build = args.compiler.resolve(strict=True), args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('In-repository output must remain beneath ignored out/')
    if not args.cargo:
        parser.error('An actual Rust cargo SDK is required')
    build.mkdir(parents=True, exist_ok=True)
    (build / 'verification.json').unlink(missing_ok=True)
    environment = os.environ.copy()
    environment['RUSTFLAGS'] = '-D warnings'
    spool_temp = build / 'spool-temp'
    spool_temp.mkdir(exist_ok=True)
    environment['TMP'] = environment['TEMP'] = str(spool_temp)
    if args.cargo_home:
        environment['CARGO_HOME'] = str(args.cargo_home.resolve())
    started, commands = time.monotonic(), []

    def run(command, name, failure=False):
        """Capture exact execution and diagnostics; failed SDK commands never count as verification."""
        argv = [str(value) for value in command]
        result = subprocess.run(argv, capture_output=True, text=True, encoding='utf-8', env=environment, timeout=900)
        (build / (name + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
        commands.append({'argv': argv, 'returncode': result.returncode})
        (build / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n', encoding='utf-8')
        if failure:
            assert result.returncode, 'Invalid source unexpectedly generated'
        elif result.returncode:
            print(result.stdout + result.stderr)
            result.check_returncode()
        return result.stdout + result.stderr

    schema = build / 'profile.serializer'
    schema.write_text((ROOT / 'test/resources/opc_profile.serializer').read_text(encoding='utf-8').replace(
        'serializer version 1.20.0;', 'serializer version 1.21.0;') + CONTRACT + application_schema(), encoding='utf-8')
    output = build / 'schema.rs'
    run([compiler, '--input', schema, '--language', 'rust', '--rust.output', output], 'generation')
    generated = output.read_text(encoding='utf-8')
    helper = re.search(r'// Generated Rust package helper module: (\w+)', generated).group(1)
    (build / 'main.rs').write_text((ROOT / 'test/resources/package_rust_main.rs').read_text(encoding='utf-8').replace(
        '@package_name@', helper), encoding='utf-8')
    for name in ('Cargo.toml', 'Cargo.lock'):
        shutil.copyfile(ROOT / 'example/xml/rust' / name, build / name)
    rows = fixtures(build)
    application_rows = application_fixtures(build)
    sdk = run([args.cargo, '--version'], 'sdk').strip()
    run([args.cargo, 'run', '--locked', '--offline', '--release', '--manifest-path', build / 'Cargo.toml', '--', build], 'acceptance')
    for filename in ('generated.opc', 'generated64.opc', 'preserved.opc', 'canonical-a.opc', 'canonical-b.opc'):
        with zipfile.ZipFile(build / filename) as archive:
            assert archive.testzip() is None
            names = archive.namelist()
            assert names == sorted(names), 'Output is not canonically ordered'
            assert all(item.date_time == (1980, 1, 1, 0, 0, 0) for item in archive.infolist())
            if filename != 'preserved.opc':
                assert b'checked &amp; generated' in archive.read('3D/model.xml')
    assert (build / 'canonical-a.opc').read_bytes() == (build / 'canonical-b.opc').read_bytes()
    with zipfile.ZipFile(build / 'preserved.opc') as archive:
        assert archive.read('Extensions/vendor.xml') == independent.independent_parts(extension=True)['Extensions/vendor.xml']
        assert archive.read('Alternate/root.xml').find(b'independent') >= 0
    with zipfile.ZipFile(build / 'scoped-preserved.opc') as archive:
        assert b'../Extensions/vendor.xml?choice=one#node' in archive.read('Alternate/_rels/root.xml.rels')
        assert b'https://example.invalid/a?x=1#f' in archive.read('Alternate/_rels/root.xml.rels')
        assert archive.read('Extensions/vendor.xml') == independent.independent_parts(extension=True)['Extensions/vendor.xml']
    raw = (build / 'generated64.opc').read_bytes()
    assert b'PK\x06\x06' in raw and b'PK\x06\x07' in raw
    assert not list(build.glob('.serializer-package-*.tmp'))
    assert not list(spool_temp.iterdir()), 'Memory-only spooling created temporary files'
    for count in (65534, 65535, 65536):
        with zipfile.ZipFile(build / f'count-output-{count}.zip') as archive:
            assert len(archive.infolist()) == count and archive.testzip() is None
    for filename in ('stored.zip', 'stored64.zip'):
        with zipfile.ZipFile(build / filename) as archive:
            assert archive.read('stored.bin') == b'independent stored bytes'
            assert archive.getinfo('stored.bin').compress_type == zipfile.ZIP_STORED
    # Helper families move around colliding schema declarations without changing the model names.
    collision = build / 'collision.serializer'
    collision.write_text(schema.read_text() + '\nclass serializer_package {} class serializer_package_2 {}\n', encoding='utf-8')
    run([compiler, '--input', collision, '--language', 'rust', '--rust.output', build / 'collision.rs'], 'collision')
    assert 'pub mod serializer_package_3 {' in (build / 'collision.rs').read_text()
    collision_build = build / 'collision-gate'
    collision_build.mkdir(exist_ok=True)
    shutil.copyfile(build / 'collision.rs', collision_build / 'schema.rs')
    for name in ('Cargo.toml', 'Cargo.lock'):
        shutil.copyfile(ROOT / 'example/xml/rust' / name, collision_build / name)
    (collision_build / 'main.rs').write_text('mod schema;\nfn main(){let mut budget=schema::serializer_package_3::Budget::default();assert!(schema::ProfileArchive::encode(&schema::ProfileArchiveInput::default(),&mut budget).is_err());}\n', encoding='utf-8')
    run([args.cargo, 'run', '--locked', '--offline', '--release', '--manifest-path', collision_build / 'Cargo.toml'], 'collision-sdk')
    invalid = build / 'invalid.serializer'
    invalid.write_text(schema.read_text().replace('class node {', 'class graph_value {} class node {'), encoding='utf-8')
    run([compiler, '--input', invalid, '--language', 'rust', '--rust.output', build / 'invalid.rs'], 'companion-rejection', failure=True)
    assert not (build / 'invalid.rs').exists()
    companion = build / 'companion.serializer'
    companion.write_text(COMPANION_GRAPH, encoding='utf-8')
    (build / 'companion.rs').unlink(missing_ok=True)
    diagnostic = run([compiler, '--input', companion, '--language', 'rust', '--rust.output', build / 'companion.rs'], 'companion-graph-rejection', failure=True)
    assert 'document-companion' in diagnostic and not (build / 'companion.rs').exists()
    diagnostic = run([compiler, '--input', ROOT / 'test/resources/opc_identity.serializer',
                      '--config', ROOT / 'test/resources/opc_identity.ini', '--language', 'rust',
                      '--rust.output', build / 'identity.rs'], 'identity-package-rejection', failure=True)
    assert 'identity graphs require shared native codec operation hooks' in diagnostic and not (build / 'identity.rs').exists()
    run([compiler, '--input', ROOT / 'test/resources/xml_identity_generated.serializer',
         '--config', ROOT / 'test/resources/opc_identity.ini', '--language', 'rust',
         '--rust.output', build / 'identity_native.rs'], 'ordinary-identity-generation')
    report = {'status': 'passed', 'scope': 'Actual owning Rust ZIP/OPC profile on ' + os.sys.platform,
              'sdk': sdk, 'independent_cases': len(rows), 'application_cases': len(application_rows), 'source_bytes': output.stat().st_size,
              'source_sha256': hashlib.sha256(output.read_bytes()).hexdigest(),
              'compiler_sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(),
              'elapsed_seconds': round(time.monotonic() - started, 3), 'commands': commands}
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
