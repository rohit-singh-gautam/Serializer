# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated Go ZIP foundations with independent archives, native controls and real SDK execution."""
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

import java_package_test as shared
import package_zip64_test as wide_zip
import python_package_test as independent

ROOT = Path(__file__).resolve().parent.parent
SCHEMA = 'serializer version 1; class account_record {public uint32 account_id (1);public string label (2);}'
TEMPLATES = ('go_package_runtime.inc', 'go_package_zip_reader_runtime.inc',
             'go_package_zip_writer_runtime.inc', 'go_package_file_runtime.inc')
IMPORTS = '''import (
  srlPackageTime "time"
  srlPackageOS "os"
  srlPackageIO "io"
  srlPackageErrors "errors"
  srlPackageBytes "bytes"
  srlPackageStrings "strings"
  srlPackageUTF8 "unicode/utf8"
  srlPackageBinary "encoding/binary"
  srlPackageSort "sort"
  srlPackageCRC32 "hash/crc32"
  srlPackageFilepath "path/filepath"
  srlPackageRuntime "runtime"
)
'''


def run(command, directory, environment, log):
    """Retain exact compiler/SDK/native diagnostics and require the actual process to succeed."""
    with log.open('w', encoding='utf-8') as output:
        result = subprocess.run([str(value) for value in command], cwd=directory, env=environment,
                                stdout=output, stderr=subprocess.STDOUT)
    if result.returncode:
        print(log.read_text(encoding='utf-8', errors='replace')[-16000:], end='')
        raise subprocess.CalledProcessError(result.returncode, command)


def fixtures(directory):
    """Reuse independent cross-backend wire authors, selecting only ZIP decisions for this foundation."""
    authored = shared.build_fixtures(directory)
    rows = [row for row in authored if row[1] in ('zip', 'count', 'segment')]
    for row in authored:
        if row[0].startswith('profile-descriptor-'):
            rows.append((row[0], 'zip', row[2], 'OK', 'Textures/raw.bin', '0001feff'))
    for descriptor in (False, True):
        for wide in (False, True):
            label = f'independent-{int(descriptor)}-{int(wide)}'
            (directory / (label + '.zip')).write_bytes(independent.independent_zip(
                {'data.bin': b'independent data', 'empty.bin': b''}, descriptor, wide))
            rows.append((label, 'zip', label + '.zip', 'OK', 'data.bin', b'independent data'.hex()))
    # CP437 decoding is independent of Unicode normalization and the host locale.
    data = bytearray(independent.independent_zip({'data.bin': b'legacy name'}))
    with zipfile.ZipFile(io.BytesIO(data)) as reader:
        item = reader.getinfo('data.bin')
        data[item.header_offset + 30 + len(item.filename) - 1] = 0x82
    central = data.index(b'PK\x01\x02')
    data[central + 46 + len('data.bin') - 1] = 0x82
    (directory / 'cp437.zip').write_bytes(data)
    rows.append(('legacy-cp437', 'zip', 'cp437.zip', 'OK', 'data.bi\u00e9', b'legacy name'.hex()))
    for label, field, value in (('encryption', 8, 1), ('method', 10, 99)):
        data = bytearray(independent.independent_zip({'data.bin': b'unsupported'}))
        central = data.index(b'PK\x01\x02')
        struct.pack_into('<H', data, central + field, value)
        struct.pack_into('<H', data, 6 if field == 8 else 8, value)
        (directory / (label + '.zip')).write_bytes(data)
        rows.append((label, 'zip', label + '.zip', 'UNSUPPORTED_FEATURE', '', ''))
    for label, local, expected in (('central-version', False, 'UNSUPPORTED_FEATURE'),
                                   ('local-version', True, 'INVALID_ARCHIVE')):
        data = bytearray(independent.independent_zip({'data.bin': b'future extraction feature'}))
        field = 4 if local else data.index(b'PK\x01\x02') + 6
        struct.pack_into('<H', data, field, 46)
        (directory / (label + '.zip')).write_bytes(data)
        rows.append((label, 'zip', label + '.zip', expected, '', ''))
    (directory / 'cases.tsv').write_text(''.join('\t'.join(row) + '\n' for row in rows), encoding='utf-8')
    return rows


def generate(args, directory, schema=SCHEMA, extra=()):
    """Generate the real model before optional explicitly labeled development-template assembly."""
    (directory / 'model.serializer').write_text(schema, encoding='utf-8')
    run([args.compiler.resolve(), '--input', directory / 'model.serializer', '--language', 'go',
         '--go.output', directory / 'schema.go', '--go.package', 'main', *extra],
        directory, args.environment, directory / 'generate.log')
    generated = (directory / 'schema.go').read_text(encoding='utf-8')
    if args.staged_runtime:
        generated = generated.replace('package main', 'package main\n' + IMPORTS, 1)
        for name in TEMPLATES:
            generated += ''.join(re.findall(r'R"packagego\((.*?)\)packagego"',
                                           (ROOT / 'src' / name).read_text(encoding='utf-8'), re.S))
        generated += '\n// Generated package helper prefix: SerializerPackage\n'
        (directory / 'schema.go').write_text(generated, encoding='utf-8')
    match = re.search(r'Generated package helper prefix: ([A-Za-z_][A-Za-z_0-9]*)', generated)
    if not match:
        raise AssertionError('Actual compiler does not emit Go ZIP support; staged assembly must be explicit')
    return match[1]


def inspect_directory(data, count, wide):
    """Independently inspect exact ZIP32/ZIP64 directory, locator and record boundaries."""
    end = len(data) - 22
    signature, disk, central_disk, disk_count, entries, size, offset, comment = struct.unpack_from('<IHHHHIIH', data, end)
    assert signature == 0x06054B50 and disk == central_disk == comment == 0
    assert disk_count == entries
    boundary = end
    if wide:
        assert entries == 65535 and size == offset == 0xFFFFFFFF
        signature, disk, record, disks = struct.unpack_from('<IIQI', data, end - 20)
        assert signature == 0x07064B50 and disk == 0 and disks == 1
        (signature, record_bytes, made, needed, disk, central_disk, disk_count,
         entries, size, offset) = struct.unpack_from('<IQHHIIQQQQ', data, record)
        assert signature == 0x06064B50 and record_bytes == 44 and made == needed == 45
        assert disk == central_disk == 0 and disk_count == entries == count
        assert record + record_bytes + 12 == end - 20
        boundary = record
    else:
        assert entries == count
    assert offset + size == boundary
    position = offset
    for _ in range(count):
        assert struct.unpack_from('<I', data, position)[0] == 0x02014B50
        name, extra, comment = struct.unpack_from('<HHH', data, position + 28)
        position += 46 + name + extra + comment
        assert position <= boundary
    assert position == boundary
    return offset


def inspect_outputs(directory):
    """Require independent ZIP CRC, contents, canonical ordering and exact directory envelopes."""
    outputs = sorted(directory.glob('output-*.zip'))
    assert len(outputs) == 6
    for path in outputs:
        data = path.read_bytes()
        descriptor = int(path.stem.rsplit('-', 1)[1])
        with zipfile.ZipFile(path) as archive:
            assert archive.testzip() is None
            assert archive.namelist() == sorted(archive.namelist())
            assert archive.read('data.bin') == b'independent deflate payload'
            assert archive.read('empty.bin') == b''
            assert archive.read('stored.bin') == b'\0\1\xfe\xff'
            assert (b'PK\x06\x06' in data) == ('wide' in path.stem)
            central = inspect_directory(data, len(archive.infolist()), 'wide' in path.stem)
            next_local = 0
            for entry in archive.infolist():
                header = entry.header_offset
                assert header == next_local
                name_bytes, extra_bytes = struct.unpack_from('<HH', data, header + 26)
                after_payload = header + 30 + name_bytes + extra_bytes + entry.compress_size
                assert data[header + 30:header + 30 + name_bytes].decode('utf-8') == entry.filename
                assert struct.unpack_from('<HH', data, header + 10) == (0, 33)
                assert bool(entry.flag_bits & 8) == bool(descriptor)
                if descriptor:
                    if descriptor == 1:
                        assert data[after_payload:after_payload + 4] == b'PK\x07\x08'
                        after_payload += 4
                    width = 'Q' if 'wide' in path.stem else 'I'
                    crc, packed, plain = struct.unpack_from('<I' + width + width, data, after_payload)
                    assert (crc, packed, plain) == (entry.CRC, entry.compress_size, entry.file_size)
                    after_payload += struct.calcsize('<I' + width + width)
                else:
                    assert struct.unpack_from('<I', data, header + 14)[0] == entry.CRC
                next_local = after_payload
            assert next_local == central
    with zipfile.ZipFile(directory / 'writer-65536.zip') as archive:
        assert len(archive.infolist()) == 65536
        assert archive.namelist() == sorted(archive.namelist())
        assert archive.read('part65535') == b''
        inspect_directory((directory / 'writer-65536.zip').read_bytes(), 65536, True)
    return len(outputs) + 1


def profile_smokes(args, build):
    """Check independent package/compression collision families and unchanged binary/XML profiles."""
    if args.staged_runtime:
        return []
    results = []
    collision = ('serializer version 1; class SerializerPackage {} '
                 'class SerializerPackage_2Code {} class SerializerCompression {} '
                 'class SerializerCompression_2Format {}')
    smoke = '''package main
import "bytes"
// main roundtrips a deflated ZIP through the selected helper family without model/protocol effects.
func main() {
  writer,err:=SerializerPackageNewWriter();if err!=nil { panic(err) };defer writer.Close()
  expected:=bytes.Repeat([]byte("independent profile"),4097)
  if err=writer.Add("data.bin",expected,"deflate");err!=nil { panic(err) }
  encoded,err:=writer.Finish();if err!=nil { panic(err) }
  reader,err:=SerializerPackageOpen(encoded);if err!=nil { panic(err) };defer reader.Close()
  actual,err:=reader.Read("data.bin");if err!=nil||!bytes.Equal(actual,expected) { panic("Profile payload mismatch") }
}
'''
    for name, schema, options in (
            ('collision', collision, ('--go.naming', 'preserve')),
            ('binary', SCHEMA, ('--protocols', 'binary_none')),
            ('xml-independent', SCHEMA, ('--xml.enabled', 'true'))):
        directory = build / name
        directory.mkdir(exist_ok=True)
        prefix = generate(args, directory, schema, options)
        generated = (directory / 'schema.go').read_text(encoding='utf-8')
        if name == 'collision':
            assert prefix == 'SerializerPackage_3'
            assert 'Generated compression helper prefix: SerializerCompression_3\n' in generated
            for public_type in ('SerializerPackage', 'SerializerPackage_2Code',
                                'SerializerCompression', 'SerializerCompression_2Format'):
                assert 'type ' + public_type + ' struct' in generated
        (directory / 'go.mod').write_text('module serializer-go-package-profile\n\ngo 1.22\n', encoding='utf-8')
        (directory / 'main.go').write_text(smoke.replace('SerializerPackage', prefix), encoding='utf-8')
        run([args.go, 'run', '.'], directory, args.environment, directory / 'sdk.log')
        results.append({'profile': name, 'prefix': prefix,
                        'generated_sha256': hashlib.sha256((directory / 'schema.go').read_bytes()).hexdigest()})
    return results


def reject_disabled_opc(args, build):
    """Keep an explicit XML-disable/package conflict rejected before replacing any existing output."""
    directory = build / 'opc-prepublication'
    directory.mkdir(exist_ok=True)
    destination = directory / 'schema.go'
    previous = b'Caller output must survive explicitly disabled XML package generation.\n'
    destination.write_bytes(previous)
    result = subprocess.run([str(args.compiler.resolve()), '--input', str(ROOT / 'test/resources/opc_profile.serializer'),
                             '--language', 'go', '--go.output', str(destination), '--xml.enabled', 'false'],
                            cwd=directory, env=args.environment, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (directory / 'generate.log').write_bytes(result.stdout)
    assert result.returncode != 0 and destination.read_bytes() == previous
    assert b'Schema package declarations require XML; explicit XML disable conflicts' in result.stdout
    return True


def main():
    """Run offline Go1.22-language SDK tests and distinguish staged development from real emission."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--go', default=shutil.which('go'))
    parser.add_argument('--staged-runtime', action='store_true', help='Development only: append unreferenced current templates')
    parser.add_argument('--native-producer', type=Path)
    parser.add_argument('--cpp-zip64', type=Path, help='Actual C++ package_zip64_probe for producer/consumer controls')
    args = parser.parse_args()
    if not args.go:
        parser.error('An actual Go1.22+ SDK is required')
    build = args.build.resolve()
    if build == ROOT or build.is_relative_to(ROOT) and not build.is_relative_to(ROOT / 'out'):
        parser.error('Repository scratch must be under ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    args.environment = os.environ.copy()
    args.environment.update(GOTOOLCHAIN='local', GOWORK='off', GOPROXY='off', GOSUMDB='off', GOENV='off',
                            GOFLAGS='', CGO_ENABLED='0', GOMAXPROCS='4', GOCACHE=str(build / 'go-cache'),
                            GOTMPDIR=str(build / 'go-temp'), SRL_PACKAGE_FIXTURES=str(build / 'fixtures'))
    (build / 'go-temp').mkdir(exist_ok=True)
    beginning = time.monotonic()
    binaries = {name: {'path': str(path.resolve()), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
                for name, path in (('compiler', args.compiler), ('native_producer', args.native_producer),
                                   ('cpp_zip64', args.cpp_zip64)) if path}
    rows = fixtures(build / 'fixtures')
    native_cases = 0
    if args.native_producer:
        native = build / 'fixtures/native'
        native.mkdir(exist_ok=True)
        run([args.native_producer.resolve(), native], build, args.environment, build / 'native-producer.log')
        for filename in ('canonical-first.zip', 'canonical-second.zip', 'forced-zip64.zip'):
            rows.append((filename, 'zip', 'native/' + filename, 'OK', 'a.json', b'{"count":7}'.hex()))
            native_cases += 1
    if args.cpp_zip64:
        path = build / 'fixtures/cpp-65536.zip'
        run([args.cpp_zip64.resolve(), 'produce', '65536', path], build, args.environment, build / 'cpp-count-producer.log')
        rows.append(('cpp-count-65536', 'count-sorted', path.name, 'OK', '65536', ''))
        native_cases += 1
    (build / 'fixtures/cases.tsv').write_text(''.join('\t'.join(row) + '\n' for row in rows), encoding='utf-8')
    prefix = generate(args, build)
    (build / 'go.mod').write_text('module serializer-go-package\n\ngo 1.22\n', encoding='utf-8')
    driver = (ROOT / 'test/resources/package_go_main_test.go').read_text(encoding='utf-8')
    (build / 'package_test.go').write_text(driver.replace('SerializerPackage', prefix), encoding='utf-8')
    run([args.go, 'test', '-count=1', '-v', '.'], build, args.environment, build / 'sdk.log')
    outputs = inspect_outputs(build)
    if args.cpp_zip64:
        for path in sorted(build.glob('output-*.zip')) + [build / 'writer-65536.zip']:
            with zipfile.ZipFile(path) as archive:
                entry = archive.infolist()[-1]
                fixture = wide_zip.Fixture(path.stat().st_size, [(0, path.read_bytes())],
                                           name=entry.filename, count=len(archive.infolist()), payload=archive.read(entry))
            description = build / (path.stem + '.segments')
            fixture.write(description)
            result = wide_zip.native_read(args.cpp_zip64.resolve(), description, fixture,
                                          {'metadata_bytes': 64 * wide_zip.MIB, 'buffer_bytes': 16 * wide_zip.MIB,
                                           'io_bytes': 64 * wide_zip.MIB, 'entries': fixture.count})
            assert result['status'] == 'ok' and result['entries'] == fixture.count
            assert result['payload'] == fixture.payload.hex()
            native_cases += 1
    profiles = profile_smokes(args, build)
    opc_rejection = reject_disabled_opc(args, build)
    for entry in binaries.values():
        assert hashlib.sha256(Path(entry['path']).read_bytes()).hexdigest() == entry['sha256'], 'Qualification binary changed during execution'
    report = {'compiler_sha256': hashlib.sha256(args.compiler.read_bytes()).hexdigest(),
              'binaries': binaries,
              'runner_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'driver_sha256': hashlib.sha256((ROOT / 'test/resources/package_go_main_test.go').read_bytes()).hexdigest(),
              'fixture_manifest_sha256': hashlib.sha256((build / 'fixtures/cases.tsv').read_bytes()).hexdigest(),
              'staged_runtime': args.staged_runtime, 'independent_input_decisions': len(rows),
              'independent_outputs': outputs, 'native_interchange_decisions': native_cases,
              'go': subprocess.check_output([args.go, 'version'], env=args.environment, text=True).strip(),
              'language_baseline': 'go 1.22', 'prefix': prefix,
              'additional_profiles': profiles,
              'disabled_xml_opc_rejects_before_output': opc_rejection,
              'generated_sha256': hashlib.sha256((build / 'schema.go').read_bytes()).hexdigest(),
              'output_sha256': {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                                for path in sorted(build.glob('output-*.zip')) + [build / 'writer-65536.zip']},
              'elapsed_seconds': round(time.monotonic() - beginning, 3)}
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
