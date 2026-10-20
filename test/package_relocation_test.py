# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify canonical fixed-role rebinding with independent OPC producers and consumers."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import posixpath
import re
import shutil
import subprocess
import sys
import time
import xml.etree.ElementTree as etree
import zipfile

ROOT = Path(__file__).resolve().parents[1]
CT = 'http://schemas.openxmlformats.org/package/2006/content-types'
RELS = 'http://schemas.openxmlformats.org/package/2006/relationships'
REL_MIME = 'application/vnd.openxmlformats-package.relationships+xml'
MODEL_MIME = 'application/vnd.serializer.relocation+xml'
MODEL_NAMESPACE = 'urn:serializer:relocation-test'
ROLE_PREFIX = 'urn:serializer:relocation:'
FIRST = '/left/A.xml'
SECOND = '/right/B.xml'
CHILD = '/children/child.xml'
OPAQUE = {'/assets/blob.bin': b'untouched opaque payload\x00\xff',
          '/assets/unmoved.bin': b'unchanged target bytes'}


def ascii_fold(value):
    """Apply OPC's ASCII identity fold without Unicode normalization or URI rewriting."""
    return value.translate(str.maketrans('ABCDEFGHIJKLMNOPQRSTUVWXYZ', 'abcdefghijklmnopqrstuvwxyz'))


def selector_offset(target):
    """Locate the first literal selector delimiter without decoding escaped delimiter bytes."""
    return min((index for index in (target.find('?'), target.find('#')) if index >= 0), default=len(target))


def resolve(source, target):
    """Resolve an independently authored internal part reference in its original source scope."""
    path = target[:selector_offset(target)]
    if not path:
        return source
    return posixpath.normpath(path if path.startswith('/') else posixpath.join(posixpath.dirname(source), path))


def relationship_part(source):
    """Map one OPC relationship source to its physical infrastructure ZIP entry."""
    if source == '/':
        return '_rels/.rels'
    return posixpath.join(posixpath.dirname(source), '_rels', posixpath.basename(source) + '.rels').lstrip('/')


def relative_target(source, target):
    """Author source-relative references independently of generated package URI helpers."""
    return posixpath.relpath(target, posixpath.dirname(source))


def preserved_links(first, second, include_second=True):
    """Exercise both moving endpoints, selectors, inert external URIs and unchanged relative spelling."""
    def link(source, identifier, target, mode='internal'):
        """Keep each authored relationship's ID and exact text available to the output oracle."""
        return {'source': source, 'id': identifier, 'type': ROLE_PREFIX + 'keep:' + identifier,
                'target': target, 'mode': mode}
    links = [
        link('/', 'in_first', first.swapcase() + '?view=%2f#first'),
        link(first, 'outbound', relative_target(first, '/assets/blob.bin') + '?keep=%2f#asset'),
        link(first, 'self', '?view=%2F#self'),
        link(first, 'fragment', '#only'),
        link(first, 'external_relative', '../right/B.xml?opaque=%2f#external', 'external'),
        link(first, 'external_absolute', 'https://example.invalid/A.xml?q=%2f#external', 'external'),
        link('/assets/blob.bin', 'back', relative_target('/assets/blob.bin', first) + '#owner'),
        link('/assets/blob.bin', 'no_move', 'unmoved.bin?raw=%2f#stay'),
    ]
    if include_second:
        links += [link('/', 'in_second', second.lstrip('/') + '#second'),
                  link(first, 'peer', relative_target(first, second) + '?swap=%41#peer'),
                  link(second, 'peer_back', relative_target(second, first) + '?back=%2f#peer'),
                  link(second, 'second_self', '#two')]
    return links


def archive(first, second, extra_parts=None, links=None, duplicate_origin=False):
    """Write original model, content-type and relationship bytes with Python's independent ZIP implementation."""
    payloads = {first: b'<payload xmlns="' + MODEL_NAMESPACE.encode() + b'" id="11" label="first"/>',
                second: b'<payload xmlns="' + MODEL_NAMESPACE.encode() + b'" id="22" label="second"/>',
                CHILD: b'<payload xmlns="' + MODEL_NAMESPACE.encode() + b'" id="33" label="child"/>', **OPAQUE}
    payloads.update(extra_parts or {})
    known = [
        {'source': '/', 'id': 'authored_first', 'type': ROLE_PREFIX + 'first', 'target': first, 'mode': 'internal'},
        {'source': '/', 'id': 'authored_second', 'type': ROLE_PREFIX + 'second',
         'target': first if duplicate_origin else second, 'mode': 'internal'},
        {'source': first, 'id': 'authored_child', 'type': ROLE_PREFIX + 'child',
         'target': relative_target(first, CHILD), 'mode': 'internal'},
    ]
    links = list(known) + list(links if links is not None else preserved_links(first, second))
    manifest = etree.Element('{' + CT + '}Types')
    entries = {name.lstrip('/'): data for name, data in payloads.items()}
    for name in payloads:
        etree.SubElement(manifest, '{' + CT + '}Override', {
            'PartName': name, 'ContentType': MODEL_MIME if name in (first, second, CHILD) else 'application/octet-stream'})
    groups = {}
    for link in links:
        groups.setdefault(link['source'], []).append(link)
    for source, values in groups.items():
        root = etree.Element('{' + RELS + '}Relationships')
        for item in values:
            attributes = {'Id': item['id'], 'Type': item['type'], 'Target': item['target']}
            if item['mode'] == 'external':
                attributes['TargetMode'] = 'External'
            etree.SubElement(root, '{' + RELS + '}Relationship', attributes)
        name = relationship_part(source)
        entries[name] = etree.tostring(root, encoding='utf-8', xml_declaration=True)
        etree.SubElement(manifest, '{' + CT + '}Override', {'PartName': '/' + name, 'ContentType': REL_MIME})
    entries['[Content_Types].xml'] = etree.tostring(manifest, encoding='utf-8', xml_declaration=True)
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, 'w', compression=zipfile.ZIP_STORED) as output:
        for name, payload in sorted(entries.items()):
            output.writestr(name, payload)
    return stream.getvalue()


def build_fixtures(directory):
    """Publish a deterministic independent corpus, including an absent-role swap tombstone regression."""
    directory.mkdir(parents=True, exist_ok=True)
    rows = []
    def add(name, first, second, action='none', expected='OK', **options):
        """Retain exact authored paths, links and action separately from the generated SDK decision."""
        links = options.pop('links', preserved_links(first, second))
        data = archive(first, second, links=links, **options)
        (directory / (name + '.opc')).write_bytes(data)
        rows.append({'name': name, 'action': action, 'expected': expected, 'first': first,
                     'second': second, 'links': links, 'input_sha256': hashlib.sha256(data).hexdigest()})
    add('swap', SECOND, FIRST)
    add('alternate', '/alternate/first.xml', '/alternate/second.xml')
    add('no-move', FIRST, SECOND)
    add('removed-unreferenced', '/alternate/first.xml', '/alternate/second.xml', action='remove_second',
        links=preserved_links('/alternate/first.xml', '/alternate/second.xml', include_second=False))
    add('occupied', '/alternate/first.xml', '/alternate/second.xml', expected='REJECT',
        extra_parts={FIRST: b'preserved opaque canonical collision'})
    collision = preserved_links('/alternate/first.xml', '/alternate/second.xml')
    collision.append({'source': '/alternate/first.xml', 'id': 'collision_probe_placeholder', 'type': ROLE_PREFIX + 'keep:collision',
                      'target': '/assets/blob.bin', 'mode': 'internal'})
    add('scoped-id', '/alternate/first.xml', '/alternate/second.xml', expected='REJECT', links=collision)
    add('removed-second', '/alternate/first.xml', '/alternate/second.xml', action='remove_second', expected='REJECT')
    add('swap-removed-second', SECOND, FIRST, action='remove_second', expected='REJECT')
    add('duplicate-origin', '/alternate/first.xml', '/alternate/second.xml', expected='LOAD_ERROR', duplicate_origin=True)
    (directory / 'cases.tsv').write_text(''.join(row['name'] + '\t' + row['action'] + '\t' + row['expected'] + '\n' for row in rows), encoding='utf-8')
    (directory / 'manifest.json').write_text(json.dumps(rows, indent=2) + '\n', encoding='utf-8')
    return rows


def read_opc(path):
    """Independently inspect CRCs, declared payload identities and source-local relationship closure."""
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
        content = etree.fromstring(archive.read('[Content_Types].xml'))
        assert content.tag == '{' + CT + '}Types'
        types = {item.attrib['PartName']: item.attrib['ContentType'] for item in content if item.tag == '{' + CT + '}Override'}
        defaults = {item.attrib['Extension']: item.attrib['ContentType'] for item in content if item.tag == '{' + CT + '}Default'}
        payloads, links, seen = {}, [], set()
        for info in archive.infolist():
            name = '/' + info.filename
            if info.filename == '[Content_Types].xml':
                continue
            if info.filename == '_rels/.rels':
                source = '/'
            elif '/_rels/' in name and name.endswith('.rels'):
                parent, leaf = name.rsplit('/_rels/', 1)
                source = parent + '/' + leaf[:-len('.rels')]
            else:
                payloads[name] = archive.read(info)
                assert types.get(name, defaults.get(name.rsplit('.', 1)[-1]))
                continue
            root = etree.fromstring(archive.read(info))
            assert root.tag == '{' + RELS + '}Relationships'
            for item in root:
                assert item.tag == '{' + RELS + '}Relationship'
                assert re.fullmatch(r'[A-Za-z_][A-Za-z0-9._-]*', item.attrib['Id'])
                key = (ascii_fold(source), item.attrib['Id'])
                assert key not in seen
                seen.add(key)
                links.append({'source': source, 'id': item.attrib['Id'], 'type': item.attrib['Type'],
                              'target': item.attrib['Target'], 'mode': item.attrib.get('TargetMode', 'Internal').lower()})
        identities = {ascii_fold(name) for name in payloads}
        assert len(identities) == len(payloads)
        for link in links:
            assert link['source'] == '/' or ascii_fold(link['source']) in identities
            if link['mode'] == 'internal':
                assert ascii_fold(resolve(link['source'], link['target'])) in identities
        return payloads, links


def prepare_scoped_collision(directory, rows):
    """Derive a genuine source-local collision from a black-box generated child ID, preserving old algorithms."""
    _, produced = read_opc(directory / 'no-move-encoded.opc')
    children = [link for link in produced if link['type'] == ROLE_PREFIX + 'child']
    assert len(children) == 1 and children[0]['source'] == FIRST
    observed_id = children[0]['id']
    row = next(item for item in rows if item['name'] == 'scoped-id')
    path = directory / 'scoped-id.opc'
    metadata = relationship_part(row['first'])
    stream = io.BytesIO()
    changed = 0
    with zipfile.ZipFile(path) as source, zipfile.ZipFile(stream, 'w') as output:
        for info in source.infolist():
            payload = source.read(info)
            if info.filename == metadata:
                root = etree.fromstring(payload)
                for item in root:
                    if item.attrib['Type'] == ROLE_PREFIX + 'keep:collision':
                        item.set('Id', observed_id)
                        changed += 1
                    else:
                        assert item.attrib['Id'] != observed_id, 'Collision must arise only after canonical composition'
                payload = etree.tostring(root, encoding='utf-8', xml_declaration=True)
            output.writestr(info, payload)
    assert changed == 1
    path.write_bytes(stream.getvalue())
    read_opc(path)
    for item in row['links']:
        if item['type'] == ROLE_PREFIX + 'keep:collision':
            item['id'] = observed_id
    row['input_sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
    (directory / 'manifest.json').write_text(json.dumps(rows, indent=2) + '\n', encoding='utf-8')
    return {'observed_child_id': observed_id, 'source': children[0]['source'],
            'seed_output_sha256': hashlib.sha256((directory / 'no-move-encoded.opc').read_bytes()).hexdigest(),
            'patched_input_sha256': row['input_sha256'], 'changed': 'Only the preserved unknown relationship Id attribute.'}


def inspect_output(path, row, legacy=False):
    """Check exact preserved spelling, simultaneous endpoint moves and untouched typed/opaque payload values."""
    parts, links = read_opc(path)
    mapping = {} if legacy else {ascii_fold(original): canonical
        for original, canonical in ((row['first'], FIRST), (row['second'], SECOND)) if original != canonical}
    first = row['first'] if legacy else FIRST
    second = row['second'] if legacy else SECOND
    expected_payloads = {first: ('11', 'first'), CHILD: ('33', 'child')}
    if row['action'] != 'remove_second':
        expected_payloads[second] = ('22', 'second')
    for name, (identity, label) in expected_payloads.items():
        model = etree.fromstring(parts[name])
        assert model.tag == '{' + MODEL_NAMESPACE + '}payload'
        assert model.attrib == {'id': identity, 'label': label}
    assert set(parts) == set(expected_payloads) | set(OPAQUE)
    for name, payload in OPAQUE.items():
        assert parts[name] == payload
    retained = {link['type']: link for link in links if link['type'].startswith(ROLE_PREFIX + 'keep:')}
    assert len(retained) == len(row['links'])
    for original in row['links']:
        expected = dict(original)
        expected['source'] = mapping.get(ascii_fold(original['source']), original['source'])
        if original['mode'] == 'internal':
            endpoint = resolve(original['source'], original['target'])
            moved = mapping.get(ascii_fold(endpoint), endpoint)
            if expected['source'] != original['source'] or moved != endpoint:
                expected['target'] = moved + original['target'][selector_offset(original['target']):]
        assert retained[original['type']] == expected, (path, original, retained[original['type']], expected)
    children = [link for link in links if link['type'] == ROLE_PREFIX + 'child']
    assert len(children) == 1 and children[0]['source'] == first and resolve(first, children[0]['target']) == CHILD
    return {'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
            'payloads': len(parts), 'preserved_relationships': len(retained)}


def main():
    """Generate real language libraries, execute public consumers and inspect their produced OPC independently."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', choices=('cpp', 'python', 'c', 'rust', 'go', 'java', 'javascript', 'typescript', 'csharp', 'kotlin', 'swift'), required=True)
    parser.add_argument('--consumer', type=Path)
    parser.add_argument('--generated', type=Path)
    parser.add_argument('--cc', default=shutil.which('cl') if os.name == 'nt' else shutil.which('cc'))
    parser.add_argument('--provider-prefix', type=Path)
    parser.add_argument('--cargo', default='cargo')
    parser.add_argument('--go', default='go')
    parser.add_argument('--javac', default='javac')
    parser.add_argument('--java', default='java')
    parser.add_argument('--kotlinc', type=Path)
    parser.add_argument('--node', default='node')
    parser.add_argument('--node-dependencies', type=Path)
    parser.add_argument('--typescript-cli', type=Path)
    parser.add_argument('--dotnet', default='dotnet')
    parser.add_argument('--swiftc', default='swiftc')
    args = parser.parse_args()
    compiler, build = args.compiler.resolve(strict=True), args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository scratch must stay below ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    (build / 'verification.json').unlink(missing_ok=True)
    rows = build_fixtures(build / 'fixtures')
    started, commands = time.monotonic(), []
    environment = os.environ.copy()
    if args.provider_prefix and os.name == 'nt':
        environment['PATH'] = str(args.provider_prefix.resolve() / 'bin') + os.pathsep + environment.get('PATH', '')
    if os.name == 'nt':
        import ctypes
        ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)

    def run(command, label):
        """Retain exact real compiler/SDK commands and refuse partial verification reports."""
        argv = [str(value) for value in command]
        result = subprocess.run(argv, cwd=build, env=environment, capture_output=True,
                                text=True, encoding='utf-8', timeout=900)
        (build / (label + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
        commands.append({'argv': argv, 'returncode': result.returncode})
        (build / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n', encoding='utf-8')
        if result.returncode:
            print(result.stdout + result.stderr)
            result.check_returncode()
        return result.stdout + result.stderr

    def copy_consumer(extension, target, replacements=None):
        """Compile the maintained external consumer against actual freshly generated library bytes."""
        text = (ROOT / ('test/resources/package_relocation_main.' + extension)).read_text(encoding='utf-8')
        for marker, value in (replacements or {}).items():
            text = text.replace(marker, value)
        target.write_text(text, encoding='utf-8')

    schema = ROOT / 'test/resources/package_relocation.serializer'
    schema_hash = hashlib.sha256(schema.read_bytes()).hexdigest()
    base = [compiler, '--input', schema]
    generated = []
    executable = build / ('consumer.exe' if os.name == 'nt' else 'consumer')
    tool = None
    if args.language == 'cpp':
        if args.consumer is None:
            parser.error('C++ requires the CMake-built SDK consumer in --consumer')
        executable = args.consumer.resolve(strict=True)
        launch = [executable]
        if args.generated:
            generated.append(args.generated.resolve(strict=True))
    elif args.language == 'python':
        source = build / 'schema.py'
        run(base + ['--language', 'python', '--python.output', source], 'generation')
        copy_consumer('py', build / 'main.py')
        launch, tool = [sys.executable, '-B', build / 'main.py'], sys.version
        generated.append(source)
    elif args.language == 'java':
        source = build / 'Generated.java'
        run(base + ['--language', 'java', '--output', source, '--java.package', 'generated'], 'generation')
        copy_consumer('java', build / 'RelocationMain.java')
        run([args.javac, '--release', '17', '-encoding', 'UTF-8', '-Xlint:all', '-Werror',
             '-d', build / 'classes', source, build / 'RelocationMain.java'], 'compile')
        tool = run([args.java, '-version'], 'version').strip()
        launch = [args.java, '-cp', build / 'classes', 'consumer.RelocationMain']
        generated.append(source)
    elif args.language == 'kotlin':
        if not args.kotlinc:
            parser.error('Provide the actual Kotlin compiler in --kotlinc')
        source = build / 'Schema.kt'
        run(base + ['--language', 'kotlin', '--output', source, '--kotlin.package', 'generated'], 'generation')
        copy_consumer('kt', build / 'Main.kt')
        home = args.kotlinc.resolve().parent.parent
        command = [args.java, '-Xmx2g', '-cp', home / 'lib/kotlin-compiler.jar',
                   'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler', '-kotlin-home', home, '-jvm-target', '17', '-Werror']
        tool = run(command[:7] + ['-version'], 'version').strip()
        run(command + ['-module-name', 'serializer_library', '-d', build / 'generated.jar', source], 'library-compile')
        run(command + ['-module-name', 'independent_consumer', '-classpath', build / 'generated.jar',
                       '-include-runtime', '-d', build / 'consumer.jar', build / 'Main.kt'], 'consumer-compile')
        launch = [args.java, '-cp', str(build / 'consumer.jar') + os.pathsep + str(build / 'generated.jar'), 'consumer.MainKt']
        generated.append(source)
    elif args.language in ('javascript', 'typescript'):
        from package_node_dependencies import prepare_node_dependencies
        if not args.node_dependencies:
            parser.error('Provide actual repository-locked XML dependencies in --node-dependencies')
        prepare_node_dependencies(build, args.node_dependencies, run)
        source = build / 'schema.mjs'
        typed = args.language == 'typescript'
        generation = base + ['--language', 'js,typescript' if typed else 'js', '--js.output', source]
        if typed:
            generation += ['--typescript.output', build / 'schema.d.mts']
        run(generation, 'generation')
        injection = ''
        if typed:
            if not args.typescript_cli:
                parser.error('Provide the actual TypeScript CLI in --typescript-cli')
            (build / 'api.mts').write_text((ROOT / 'test/resources/package_relocation_api.ts').read_text(encoding='utf-8'), encoding='utf-8')
            run([args.node, args.typescript_cli.resolve(), '--target', 'ES2022', '--module', 'NodeNext',
                 '--moduleResolution', 'NodeNext', '--strict', '--noEmitOnError', '--outDir', build,
                 build / 'api.mts'], 'strict-consumer-compile')
            injection = "const typed=await import('./api.mjs');await typed.verify(input,{compressionProvider:provider},directory+'/'+name+'-typed.opc',host);++checks;"
            generated.append(build / 'schema.d.mts')
        copy_consumer('mjs', build / 'main.mjs', {'@typed_api@': injection})
        launch = [args.node, build / 'main.mjs']
        tool = run([args.node, '--version'], 'version').strip()
        generated.append(source)
    elif args.language == 'csharp':
        import csharp_compression_test as csharp
        versions = re.findall(r'^(8\.0\.\d+) ', run([args.dotnet, '--list-sdks'], 'versions'), re.M)
        if not versions:
            parser.error('Provide the actual .NET8 SDK/runtime')
        tool = max(versions, key=lambda value: tuple(map(int, value.split('.'))))
        csharp.project(build, tool)
        environment.update(DOTNET_CLI_HOME=str(build / 'dotnet-home'), DOTNET_CLI_TELEMETRY_OPTOUT='1',
                           NUGET_PACKAGES=str(build / 'nuget'), DOTNET_CLI_USE_MSBUILD_SERVER='0', MSBUILDDISABLENODEREUSE='1')
        source = build / 'Schema.cs'
        run(base + ['--language', 'csharp', '--output', source, '--csharp.namespace', 'SerializerGenerated'], 'generation')
        copy_consumer('cs', build / 'Program.cs')
        run([args.dotnet, 'build', build / 'Compression.csproj', '--configuration', 'Release', '--nologo',
             '--disable-build-servers', '-nodeReuse:false'], 'compile')
        launch = [args.dotnet, build / 'bin/Release/net8.0/Compression.dll']
        generated.append(source)
    elif args.language == 'swift':
        source = build / 'Schema.swift'
        run(base + ['--language', 'swift', '--output', source], 'generation')
        copy_consumer('swift', build / 'main.swift')
        tool = run([args.swiftc, '--version'], 'version').strip()
        run([args.swiftc, '-O', '-warnings-as-errors', source, build / 'main.swift', '-o', executable], 'compile')
        launch = [executable]
        generated.append(source)
    elif args.language == 'c':
        if not args.cc:
            parser.error('Provide an actual C11 compiler in --cc')
        source = build / 'schema.h'
        run(base + ['--language', 'c', '--output', source], 'generation')
        text = source.read_text(encoding='utf-8')
        helper = re.search(r'/\* Generated C package prefix: (\w+) \*/', text).group(1)
        selected = re.search(r'/\* Generated C selected-codec prefix: (\w+) \*/', text).group(1)
        copy_consumer('c', build / 'main.c', {'@p@': helper, '@selected@': selected})
        msvc = Path(args.cc).name.lower() in ('cl', 'cl.exe')
        if msvc:
            command = [args.cc, '/nologo', '/std:c11', '/W4', '/WX', '/O2', '/utf-8', '/D_CRT_SECURE_NO_WARNINGS',
                       '/DSERIALIZER_COMPRESSION_ZLIB=1', build / 'main.c', '/Fe' + str(executable), '/Fo' + str(build / 'main.obj')]
            if args.provider_prefix:
                prefix = args.provider_prefix.resolve()
                command += ['/I' + str(prefix / 'include'), '/I' + str(prefix / 'include/libxml2'),
                            '/link', '/LIBPATH:' + str(prefix / 'lib')]
            command += ['libxml2.lib', 'z.lib']
        else:
            command = [args.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
                       '-DSERIALIZER_COMPRESSION_ZLIB=1', build / 'main.c', '-o', executable]
            command += subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libxml-2.0'], text=True).split()
            command += ['-lz', '-lm']
        run(command, 'compile')
        launch = [executable]
        generated.append(source)
    elif args.language == 'rust':
        source = build / 'schema.rs'
        run(base + ['--language', 'rust', '--rust.output', source], 'generation')
        helper = re.search(r'// Generated Rust package helper module: (\w+)', source.read_text(encoding='utf-8')).group(1)
        copy_consumer('rs', build / 'main.rs', {'@p@': helper})
        for name in ('Cargo.toml', 'Cargo.lock'):
            shutil.copyfile(ROOT / 'example/xml/rust' / name, build / name)
        run([args.cargo, 'build', '--locked', '--offline', '--release', '--manifest-path', build / 'Cargo.toml'], 'compile')
        launch = [build / 'target/release' / ('serializer-xml-example.exe' if os.name == 'nt' else 'serializer-xml-example')]
        tool = run([args.cargo, '--version'], 'version').strip()
        generated.append(source)
    else:
        source = build / 'model/schema.go'
        source.parent.mkdir(exist_ok=True)
        run(base + ['--language', 'go', '--go.output', source, '--go.package', 'model'], 'generation')
        text = source.read_text(encoding='utf-8')
        selected = re.search(r'// Generated Go selected-codec prefix: (\w+)', text).group(1)
        helper = re.search(r'// Generated package helper prefix: (\w+)', text).group(1)
        copy_consumer('go', build / 'main.go', {'@selected@': selected, '@p@': helper})
        (build / 'go.mod').write_text('module serializer-relocation\n\ngo 1.23\n', encoding='utf-8')
        environment.update(GOWORK='off', GOFLAGS='-mod=readonly', GOCACHE=str(build / 'gocache'))
        run([args.go, 'build', '-o', executable, '.'], 'compile')
        tool = run([args.go, 'version'], 'version').strip()
        launch = [executable]
        generated.append(source)
    if args.language == 'swift':
        for direction in ('encoded', 'saved'):
            (build / 'fixtures' / ('swift-unicode-selectors-' + direction + '.opc')).unlink(missing_ok=True)
    decisions = {}
    outputs = []
    collision_evidence = None
    for row in sorted(rows, key=lambda item: item['name'] != 'no-move'):
        for direction in ('encoded', 'repeat', 'saved', 'legacy', 'typed'):
            (build / 'fixtures' / (row['name'] + '-' + direction + '.opc')).unlink(missing_ok=True)
        output = run([*launch, build / 'fixtures', row['name'], row['action'], row['expected']], 'sdk-' + row['name'])
        decision = [json.loads(line) for line in output.splitlines() if line.startswith('{')][-1]
        assert decision['case'] == row['name'] and decision['decision'] == row['expected']
        decisions[row['name']] = decision
        if row['expected'] == 'OK':
            for direction in ('encoded', 'repeat', 'saved'):
                outputs.append(inspect_output(build / 'fixtures' / (row['name'] + '-' + direction + '.opc'), row))
            for direction in ('legacy', 'typed'):
                path = build / 'fixtures' / (row['name'] + '-' + direction + '.opc')
                if path.is_file():
                    outputs.append(inspect_output(path, row, legacy=direction == 'legacy'))
            if row['name'] == 'no-move':
                collision_evidence = prepare_scoped_collision(build / 'fixtures', rows)
        elif row['expected'] == 'REJECT':
            assert (build / 'fixtures' / (row['name'] + '-saved.opc')).read_bytes() == b'previous destination'
    unicode_outputs = []
    if args.language == 'swift':
        # Python indexes Unicode code points, independently of Swift's extended grapheme clusters.
        unicode_row = dict(next(item for item in rows if item['name'] == 'alternate'))
        suffixes = {'self': '?\u0301#query', 'fragment': '#\u0301', 'outbound': '?\u0301#path'}
        unicode_row['links'] = [dict(link) for link in unicode_row['links']]
        for link in unicode_row['links']:
            if link['id'] in suffixes:
                link['target'] = ('../assets/blob.bin' if link['id'] == 'outbound' else '') + suffixes[link['id']]
        for direction in ('encoded', 'saved'):
            path = build / 'fixtures' / ('swift-unicode-selectors-' + direction + '.opc')
            unicode_outputs.append(inspect_output(path, unicode_row))
        outputs.extend(unicode_outputs)
    assert hashlib.sha256(schema.read_bytes()).hexdigest() == schema_hash, 'Schema changed during qualification'
    report = {'language': args.language, 'compiler_sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(),
              'tool': tool, 'sdk': decisions, 'independent_inputs': len(rows), 'independent_outputs': outputs,
              'sources_sha256': {str(path.relative_to(build)) if path.is_relative_to(build) else str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in generated},
              'schema_sha256': schema_hash, 'metamorphic_scoped_id_collision': collision_evidence,
              'input_manifest_sha256': hashlib.sha256((build / 'fixtures/manifest.json').read_bytes()).hexdigest(), 'commands': commands,
              'elapsed_seconds': round(time.monotonic() - started, 3)}
    if unicode_outputs:
        report['unicode_selector_outputs'] = unicode_outputs
    if Path(launch[0]).is_file():
        report['consumer_sha256'] = hashlib.sha256(Path(launch[0]).read_bytes()).hexdigest()
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f"PASS: {args.language} {len(rows)} independent inputs, {len(outputs)} independently inspected outputs, {sum(value['checks'] for value in decisions.values())} SDK assertions")


if __name__ == '__main__':
    main()
