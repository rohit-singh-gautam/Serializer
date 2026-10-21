# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify actual generated .NET 8 ZIP/OPC packages against independent wire fixtures."""
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
import tempfile
import time
import xml.etree.ElementTree as etree
import zipfile

import csharp_compression_test as sdk
import java_package_test as corpus
import python_package_test as independent

ROOT = Path(__file__).resolve().parent.parent


def run(command, log, environment, cwd):
    """Retain real compiler/SDK diagnostics without lending disposable cwd to worker processes."""
    with log.open('w', encoding='utf-8') as output:
        result = subprocess.run([str(value) for value in command], cwd=cwd, env=environment,
                                stdout=output, stderr=subprocess.STDOUT)
    if result.returncode:
        print(log.read_text(encoding='utf-8', errors='replace'), end='')
        raise subprocess.CalledProcessError(result.returncode, command)


def inspect_outputs(build):
    """Decode emitted data with independent Python ZIP/XML providers and inspect ZIP64 framing."""
    count = 0
    for path in sorted(build.glob('produced-*.*')):
        with zipfile.ZipFile(path) as archive:
            assert archive.testzip() is None
            if path.suffix == '.opc':
                assert archive.read('Textures/raw.bin') == b'\0\1\xfe\xff'
                assert etree.fromstring(archive.read('3D/model.xml')).attrib['name'] == 'generated & checked'
            else:
                assert archive.read('stored.bin') == b'\0\1\2'
                assert archive.read('empty.bin') == b''
            _, wide, descriptor = path.stem.split('-')
            assert all(bool(item.flag_bits & 8) == (descriptor == 'True') for item in archive.infolist())
            assert (b'PK\x06\x06' in path.read_bytes()) == (wide == 'True')
            assert archive.namelist() == sorted(archive.namelist()) or path.suffix == '.opc'
            inspect_directory(path.read_bytes(), archive.infolist(), wide == 'True', descriptor == 'True')
        count += 1
    assert count == 8, count
    with zipfile.ZipFile(build / 'external-roundtrip.opc') as archive:
        relationships = etree.fromstring(archive.read('_rels/.rels'))
        targets = [item.attrib['Target'] for item in relationships if item.attrib.get('TargetMode') == 'External']
        assert targets == (build / 'fixtures/external-targets.txt').read_text(encoding='utf-8').splitlines()
    with zipfile.ZipFile(build / 'scoped-roundtrip.opc') as archive:
        scopes, pending, identities = [], {}, []
        for event, value in etree.iterparse(io.BytesIO(archive.read('model.xml')), events=('start-ns', 'start', 'end')):
            if event == 'start-ns':
                pending[value[0]] = value[1]
            elif event == 'start':
                scope = dict(scopes[-1]) if scopes else {}
                scope.update(pending)
                pending.clear()
                scopes.append(scope)
            else:
                if value.tag in ('{urn:scoped}first', '{urn:scoped}second'):
                    prefix, local = value.text.split(':')
                    identities.append((scopes[-1][prefix], local))
                scopes.pop()
        assert identities == [('urn:first', 'item'), ('urn:second', 'item')]
    return count + 2


def inspect_directory(data, items, wide, descriptor, *, allow_known_descriptor_sizes=False):
    """Inspect APPNOTE framing; C# additionally chooses zero local ZIP64 sizes with descriptors."""
    end = len(data) - 22
    signature, disk, central_disk, disk_count, count, size, offset, comment = struct.unpack_from('<I4H2IH', data, end)
    assert (signature, disk, central_disk, comment) == (0x06054b50, 0, 0, 0)
    if wide:
        locator = end - 20
        signature, disk, record, disks = struct.unpack_from('<IIQI', data, locator)
        assert (signature, disk, disks) == (0x07064b50, 0, 1)
        signature, record_size, made, needed, disk, central_disk, local_count, total_count, size, offset = struct.unpack_from('<IQ2H2I4Q', data, record)
        assert (signature, record_size, needed, disk, central_disk) == (0x06064b50, 44, 45, 0, 0)
        assert local_count == total_count == len(items)
        assert record + record_size + 12 == locator
        assert (disk_count, count) == (65535, 65535)
        assert offset + size == record
    else:
        assert disk_count == count == len(items)
        assert offset + size == end
    position = offset
    for item in items:
        row = struct.unpack_from('<I6H3I5H2I', data, position)
        assert row[0] == 0x02014b50
        assert row[3] == item.flag_bits and row[4] == item.compress_type
        assert row[7] == item.CRC
        name_size, extra_size, comment_size = row[10:13]
        extra = data[position + 46 + name_size:position + 46 + name_size + extra_size]
        if wide:
            assert (row[8], row[9], row[16]) == (0xffffffff, 0xffffffff, 0xffffffff)
            assert struct.unpack_from('<HH3Q', extra) == (1, 24, item.file_size, item.compress_size, item.header_offset)
        else:
            assert (row[8], row[9], row[16]) == (item.compress_size, item.file_size, item.header_offset)
        local = struct.unpack_from('<I5H3I2H', data, item.header_offset)
        assert local[0] == 0x04034b50 and local[2] == item.flag_bits and local[3] == item.compress_type
        local_name, local_extra = local[9:11]
        payload = item.header_offset + 30 + local_name + local_extra
        if wide:
            assert (local[7], local[8]) == (0xffffffff, 0xffffffff)
            local_sizes = struct.unpack_from('<HH2Q', data, item.header_offset + 30 + local_name)
            assert local_sizes[:2] == (1, 16)
            if descriptor:
                # Known matching sizes are a valid prepared-writer choice; unrelated nonzero values reject.
                assert local_sizes[2] in ((0, item.file_size) if allow_known_descriptor_sizes else (0,))
                assert local_sizes[3] in ((0, item.compress_size) if allow_known_descriptor_sizes else (0,))
            else:
                assert local_sizes[2:] == (item.file_size, item.compress_size)
        if descriptor:
            descriptor_values = struct.unpack_from('<II2Q' if wide else '<4I', data, payload + item.compress_size)
            assert descriptor_values == (0x08074b50, item.CRC, item.compress_size, item.file_size)
        else:
            assert local[6] == item.CRC
        position += 46 + name_size + extra_size + comment_size
    assert position == offset + size


def main():
    """Compile actual emitted C#, exercise public APIs, and retain reproducible qualification evidence."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--dotnet', default=shutil.which('dotnet'))
    parser.add_argument('--cpp-example', type=Path, help='Actual native OPC producer/consumer executable')
    args = parser.parse_args()
    build, compiler = args.build.resolve(), args.compiler.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('In-repository scratch must remain below ignored out/')
    if not args.dotnet:
        parser.error('The actual .NET 8 SDK/runtime is required')
    versions = re.findall(r'^(8\.0\.\d+) ', subprocess.check_output([args.dotnet, '--list-sdks'], text=True), re.M)
    if not versions:
        parser.error('Install a .NET 8 SDK to qualify this baseline')
    build.mkdir(parents=True, exist_ok=True)
    started = time.monotonic()
    rows = corpus.build_fixtures(build / 'fixtures')
    relocated = independent.independent_parts(True)
    relocated['Alternate/_rels/root.xml.rels'] = relocated['Alternate/_rels/root.xml.rels'].replace(
        b'</Relationships>', b'<Relationship Id="opaque-back" Type="urn:vendor:back" Target="root.xml?mode=raw#kept"/></Relationships>')
    (build / 'fixtures/relocation.opc').write_bytes(independent.independent_zip(relocated))
    for label, target in [('escaping-root', '../../Alternate/root.xml'), ('escaping-source', '../../../Textures/raw.bin')]:
        parts = independent.independent_parts()
        key = '_rels/.rels' if label == 'escaping-root' else 'Alternate/_rels/root.xml.rels'
        old = b'Alternate/root.xml' if label == 'escaping-root' else b'../Textures/raw.bin'
        parts[key] = parts[key].replace(old, target.encode())
        path = label + '.zip'
        (build / 'fixtures' / path).write_bytes(independent.independent_zip(parts))
        row = (label, 'package', path, 'OK', '', 'independent')
        rows.append(row)
        with (build / 'fixtures/cases.tsv').open('a', encoding='utf-8') as output:
            output.write('\t'.join(row) + '\n')
    (build / 'fixtures/identity.opc').write_bytes(independent.independent_zip(independent.independent_identity_parts()))
    environment = os.environ.copy()
    environment.update(DOTNET_CLI_HOME=str(build / 'dotnet-home'), DOTNET_SKIP_FIRST_TIME_EXPERIENCE='1',
                       DOTNET_CLI_TELEMETRY_OPTOUT='1', DOTNET_GENERATE_ASPNET_CERTIFICATE='false',
                       DOTNET_ADD_GLOBAL_TOOLS_TO_PATH='false', NUGET_PACKAGES=str(build / 'nuget'),
                       DOTNET_CLI_USE_MSBUILD_SERVER='0', MSBUILDDISABLENODEREUSE='1')
    temporary_root = Path(os.environ.get('LOCALAPPDATA', tempfile.gettempdir()))
    if os.name == 'nt':
        temporary_root /= 'Temp'
    with tempfile.TemporaryDirectory(prefix='serializer-csharp-package-', dir=temporary_root) as temporary:
        project = Path(temporary)
        sdk.project(project, max(versions, key=lambda item: tuple(map(int, item.split('.')))))
        working = sdk.sdk_working_directory(project, environment)
        selected_sdk = subprocess.check_output([args.dotnet, '--version'], cwd=working, env=environment, text=True).strip()
        if not selected_sdk.startswith('8.0.'):
            raise AssertionError('Actual .NET 8 SDK selection failed: ' + selected_sdk)
        sources = [('Schema', ROOT / 'test/resources/opc_profile.serializer'),
                   ('Identity', ROOT / 'test/resources/opc_identity.serializer'),
                   ('Example', ROOT / 'example/opc/model.serializer')]
        parameterized = build / 'parameterized.serializer'
        parameterized.write_text((ROOT / 'test/resources/opc_profile.serializer').read_text(encoding='utf-8').replace(
            independent.MODEL_MIME, independent.MODEL_MIME + ';profile=\\"a  b\\";mode=read'), encoding='utf-8')
        sources.append(('Parameterized', parameterized))
        collision = build / 'collision.serializer'
        collision.write_text('serializer version 1.20.0; class SerializerPackage {} class SerializerPackage_2 {} '
            'class SerializerCompression {} package Bundle {part attachments:bytes {name(dynamic);'
            'content_type(dynamic);relationship("urn:serializer:test-part");occurs(0,unbounded);}}', encoding='utf-8')
        sources.append(('Collision', collision))
        scoped = build / 'scoped.serializer'
        scoped.write_text('serializer version 1.20.0; namespace scoped {xml(namespace("urn:scoped"));'
            'class model {public string first xml(element,qname);public string second xml(element,qname);}'
            'package archive {part main:model {name("/model.xml");content_type("application/vnd.scoped+xml");'
            'relationship("urn:scoped:main");}}}', encoding='utf-8')
        sources.append(('Scoped', scoped))
        scoped_parts = {
            'model.xml': b'<model xmlns="urn:scoped"><first xmlns:p="urn:first">p:item</first><second xmlns:p="urn:second">p:item</second></model>',
            '_rels/.rels': ('<Relationships xmlns="' + independent.RELS + '"><Relationship Id="scope" Type="urn:scoped:main" Target="model.xml"/></Relationships>').encode(),
            '[Content_Types].xml': ('<Types xmlns="' + independent.CT + '"><Override PartName="/model.xml" ContentType="application/vnd.scoped+xml"/><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/></Types>').encode()}
        (build / 'fixtures/scoped.opc').write_bytes(independent.independent_zip(scoped_parts))
        for outer, source in sources:
            command = [compiler, '--input', source, '--language', 'csharp', '--output', build / (outer + '.cs'),
                       '--csharp.namespace', outer + 'Generated']
            if outer == 'Collision':
                command += ['--csharp.naming', 'preserve']
            if outer == 'Identity':
                command += ['--config', ROOT / 'test/resources/opc_identity.ini']
            run(command, build / ('generate-' + outer + '.log'), environment, ROOT)
            shutil.copyfile(build / (outer + '.cs'), project / (outer + '.cs'))
        text = (project / 'Schema.cs').read_text(encoding='utf-8')
        helper = re.search(r'Generated C# package helper name: (\w+)', text)[1]
        driver = (ROOT / 'test/resources/package_csharp_main.cs').read_text(encoding='utf-8').replace('SerializerPackage', helper)
        (project / 'Program.cs').write_text(driver, encoding='utf-8')
        for source in project.glob('*.cs'):
            shutil.copyfile(source, build / source.name)
        run([args.dotnet, 'build', project / 'Compression.csproj', '--configuration', 'Release', '--nologo',
             '--disable-build-servers', '-nodeReuse:false'], build / 'dotnet-build.log', environment, working)
        run([args.dotnet, project / 'bin/Release/net8.0/Compression.dll', build], build / 'dotnet-run.log', environment, working)
        exchanges = 0
        if args.cpp_example:
            native, managed = build / 'cpp.opc', build / 'csharp.opc'
            run([args.cpp_example.resolve(), native], build / 'cpp-producer.log', environment, ROOT)
            run([args.dotnet, project / 'bin/Release/net8.0/Compression.dll', '--exchange', native, managed],
                build / 'dotnet-exchange.log', environment, working)
            run([args.cpp_example.resolve(), '--read', managed], build / 'cpp-consumer.log', environment, ROOT)
            for path in (native, managed):
                with zipfile.ZipFile(path) as archive:
                    assert archive.testzip() is None
                    assert archive.read('Attachments/sample.bin') == b'\0\1\xfe\xff'
                    model = etree.fromstring(archive.read('Catalog/catalog.xml'))
                    assert model.attrib == {'title': 'Parts & tools'}
                    assert [child.attrib for child in model] == [
                        {'id': '7', 'label': 'Bolt <M6>'}, {'id': '8', 'label': 'Nut'}]
            exchanges = 2
    counts = json.loads((build / 'dotnet-run.log').read_text(encoding='utf-8').splitlines()[-1])
    outputs = inspect_outputs(build)
    report = {'compiler_sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(), 'sdk': selected_sdk,
              'native_exchanges': exchanges, 'fixture_decisions': len(rows), 'csharp': counts, 'independent_outputs': outputs,
              'generated_sha256': {source.name: hashlib.sha256(source.read_bytes()).hexdigest() for source in build.glob('*.cs')},
              'elapsed_seconds': round(time.monotonic() - started, 3)}
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
