# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify explicitly injected Go Zstandard/LZ4 providers with real SDKs and native interoperability."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import re
import shutil
import struct
import subprocess
import sys

import go_compression_test as core
import java_compression_optional_test as shared


ROOT = Path(__file__).resolve().parent.parent
ADAPTER = ROOT / 'qualification/go_compression_providers'


def run(command, directory, environment, log):
    """Retain exact offline SDK/provider diagnostics and fail on every missing required prerequisite."""
    command = [str(part) for part in command]
    result = subprocess.run(command, cwd=directory, env=environment, capture_output=True,
                            text=True, encoding='utf-8', errors='replace', timeout=300)
    log.write_text(json.dumps(command) + '\n' + result.stdout + result.stderr, encoding='utf-8')
    if result.returncode:
        raise RuntimeError(f'{log.name}: exit {result.returncode}\n{result.stdout}{result.stderr}')
    return result.stdout.strip()


def append_case(directory, name, frame, plain, window, counts):
    """Add a mandatory native-producer decision with independently verified expected plaintext."""
    destination = directory / (name + '.bin')
    destination.write_bytes(frame)
    (directory / (name + '.plain')).write_bytes(plain)
    with (directory / 'LZ4.tsv').open('a', encoding='utf-8', newline='\n') as output:
        output.write(f'{name}.bin\t{name}.plain\tok\t{len(frame)}\t{len(plain)}\t{window}\n')
    counts['LZ4'] += 1


def native_fixtures(args, directory, environment, counts):
    """Require real native C++ frames and prove linked inputs contain references to prior block history."""
    import lz4.block
    import lz4.frame
    checks = 0
    for name in ('ZSTD', 'LZ4'):
        for mode in ('encode', 'stream_encode'):
            destination = directory / f'cpp-{name}-{mode}.bin'
            run([args.cpp_helper, mode, name.lower(), directory / 'plain.bin', destination],
                directory, environment, directory / f'cpp-{name}-{mode}.log')
            with (directory / (name + '.tsv')).open('a', encoding='utf-8', newline='\n') as output:
                output.write(f'{destination.name}\tplain.bin\tok\t{destination.stat().st_size}\t'
                             f'{(directory / "plain.bin").stat().st_size}\t{8*1024*1024}\n')
            counts[name] += 1
            checks += 1
    linked_plain = random.Random(147).randbytes(65000) * 10
    source = directory / 'cpp-linked-input.bin'
    source.write_bytes(linked_plain)
    for checksum in ('off', 'on'):
        destination = directory / f'cpp-linked-{checksum}.bin'
        proof = json.loads(run([args.cpp_linked_helper, source, destination, checksum], directory, environment,
                               directory / f'cpp-linked-{checksum}.log'))
        if not proof['linked_history_required']:
            raise AssertionError('Native block decoder found no required linked-block history')
        frame = destination.read_bytes()
        decoder = lz4.frame.LZ4FrameDecompressor()
        if decoder.decompress(frame) != linked_plain or not decoder.eof or decoder.unused_data:
            raise AssertionError('Independent Python decoder rejected the actual C++ linked frame')
        flags = frame[4]
        if flags & 32 or bool(flags & 4) != (checksum == 'on'):
            raise AssertionError('C++ linked/checksum frame descriptor differs')
        offset = 6 + (8 if flags & 8 else 0) + 1
        history_required = False
        while True:
            block = struct.unpack_from('<I', frame, offset)[0]
            offset += 4
            if not block:
                break
            count = block & 0x7fffffff
            if not block & 0x80000000:
                try:
                    lz4.block.decompress(frame[offset:offset+count], uncompressed_size=65536)
                except lz4.block.LZ4BlockError:
                    history_required = True
            offset += count + (4 if flags & 16 else 0)
        if not history_required:
            raise AssertionError('Native fixture contains no independently proven prior-block reference')
        append_case(directory, 'cpp-linked-' + checksum, frame, linked_plain, 65536, counts)
        checks += 1
    return checks


def prepare_module(compiler, directory, schema=core.SCHEMA, extra=()):
    """Emit with the actual compiler, then copy the explicit external adapter/module lockfiles."""
    directory.mkdir(parents=True, exist_ok=True)
    prefix, size = core.generate(compiler, directory, schema, extra)
    for name in ('go.mod', 'go.sum', 'providers.go', 'providers_test.go'):
        text = (ADAPTER / name).read_text(encoding='utf-8')
        if name.endswith('.go'):
            text = text.replace('SerializerCompression', prefix)
        (directory / name).write_text(text, encoding='utf-8', newline='\n')
    (directory / 'reentry_test.go').write_text(
        (ROOT / 'test/resources/compression_reentry_go_test.go').read_text(encoding='utf-8').replace(
            'SerializerCompression', prefix), encoding='utf-8')
    return prefix, size


def verify_output(args, directory, environment):
    """Require independent Python and actual C++ consumers for checksum-on/off native Go payloads."""
    from compression import zstd
    import lz4.frame
    independent = 0
    native = 0
    for name in ('ZSTD', 'LZ4'):
        for protocol in range(4):
            for checked in ('false', 'true'):
                stem = f'go-{name}-{protocol}-{checked}'
                frame = (directory / (stem + '.bin')).read_bytes()
                decoder = zstd.ZstdDecompressor() if name == 'ZSTD' else lz4.frame.LZ4FrameDecompressor()
                plain = decoder.decompress(frame)
                if not decoder.eof or decoder.unused_data or plain != (directory / (stem + '.plain')).read_bytes():
                    raise AssertionError('Independent consumer rejected ' + stem)
                independent += 1
                for mode in ('decode', 'stream_decode'):
                    destination = directory / (stem + '-' + mode + '.plain')
                    run([args.cpp_helper, mode, name.lower(), directory / (stem + '.bin'), destination],
                        directory, environment, directory / (stem + '-' + mode + '.log'))
                    if destination.read_bytes() != plain:
                        raise AssertionError('Native consumer changed ' + stem)
                    native += 1
    return independent, native


def module_metadata(output):
    """Parse the SDK's consecutive JSON module inventory without guessing installed provider versions."""
    decoder = json.JSONDecoder()
    remaining = output.strip()
    values = []
    while remaining:
        item, extent = decoder.raw_decode(remaining)
        values.append({key: item[key] for key in ('Path', 'Version', 'GoVersion', 'Sum', 'GoModSum') if key in item})
        remaining = remaining[extent:].lstrip()
    return values


def main():
    """Run a strict selected-provider qualification offline; absent dependencies are failures, never skips."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--module-cache', type=Path, required=True)
    parser.add_argument('--cpp-helper', type=Path, required=True)
    parser.add_argument('--cpp-linked-helper', type=Path, required=True)
    parser.add_argument('--provider-path', type=Path)
    parser.add_argument('--go', default=shutil.which('go'))
    args = parser.parse_args()
    if not args.go or not all(path.is_file() for path in (args.compiler, args.cpp_helper, args.cpp_linked_helper)):
        parser.error('The actual Go SDK, selected compiler and both native helpers are required')
    directory = args.build.resolve()
    if directory == ROOT or ROOT in directory.parents and directory.relative_to(ROOT).parts[0] != 'out':
        parser.error('In-repository qualification output must remain beneath ignored out')
    if not args.module_cache.is_dir():
        parser.error('The explicit offline provider module cache is required')
    directory.mkdir(parents=True, exist_ok=True)
    if args.provider_path:
        sys.path.insert(0, str(args.provider_path.resolve()))
    args.compiler, args.cpp_helper, args.cpp_linked_helper = (
        path.resolve() for path in (args.compiler, args.cpp_helper, args.cpp_linked_helper))
    environment = os.environ.copy()
    environment.update(GOTOOLCHAIN='local', GOWORK='off', GOPROXY='off', GOSUMDB='off', GOENV='off',
                       GOFLAGS='', CGO_ENABLED='0', GOMAXPROCS='4', GOMODCACHE=str(args.module_cache.resolve()),
                       GOCACHE=str(directory / 'go-cache'), GOTMPDIR=str(directory / 'go-temp'))
    Path(environment['GOTMPDIR']).mkdir(exist_ok=True)
    counts = shared.fixtures(directory, ['ZSTD', 'LZ4'])
    cpp_producers = native_fixtures(args, directory, environment, counts)
    prefix, size = prepare_module(args.compiler, directory)
    environment['SRL_COMPRESSION_FIXTURES'] = str(directory)
    version = run([args.go, 'version'], directory, environment, directory / 'go-version.log')
    test_output = run([args.go, 'test', '-count=1', '-v', '.'], directory, environment, directory / 'go-test.log')
    independent, cpp_consumers = verify_output(args, directory, environment)
    collision = directory / 'collision'
    family, collision_size = prepare_module(args.compiler, collision,
        'serializer version 1; class SerializerCompressionProvider {} '
        'class SerializerCompression_2AvailableWithProvider {} '
        'class AccountRecord stable_ids {public uint32 AccountId(1); public string Label(2);}',
        ('--go.naming', 'preserve'))
    if family != 'SerializerCompression_3':
        raise AssertionError('New provider-family names changed existing schema declarations')
    run([args.go, 'test', '-count=1', '-run', 'TestOptionalAdmission', '.'],
        collision, environment, directory / 'go-collision.log')
    from compression import zstd
    import lz4
    modules = module_metadata(run([args.go, 'list', '-m', '-json', 'all'],
                                  directory, environment, directory / 'modules.log'))
    report = {'sdk': version, 'generated_core_baseline': 'go 1.22', 'selected_adapter_baseline': 'go 1.25',
              'independent_input_decisions': counts, 'independent_output_decisions': independent,
              'cpp_producer_checks': cpp_producers, 'cpp_linked_history_proofs': 2,
              'cpp_consumer_checks': cpp_consumers, 'model_roundtrips': 16,
              'generated_bytes': size, 'collision_generated_bytes': collision_size, 'collision_prefix': family,
              'modules': modules, 'oracle_versions': {'stdlib_zstd': list(zstd.zstd_version_info),
                  'python_lz4': lz4.__version__, 'liblz4': lz4.library_version_string()},
              'compiler_sha256': hashlib.sha256(args.compiler.read_bytes()).hexdigest(),
              'provider_source_sha256': hashlib.sha256((ADAPTER / 'providers.go').read_bytes()).hexdigest(),
              'provider_test_sha256': hashlib.sha256((ADAPTER / 'providers_test.go').read_bytes()).hexdigest(),
              'cpp_helper_sha256': hashlib.sha256(args.cpp_helper.read_bytes()).hexdigest(),
              'cpp_linked_helper_sha256': hashlib.sha256(args.cpp_linked_helper.read_bytes()).hexdigest()}
    report['native_lz4_version'] = json.loads(
        (directory / 'cpp-linked-on.log').read_text(encoding='utf-8').splitlines()[-1])['lz4_version']
    report['go_test_groups'] = sum(line.startswith('--- PASS: ') for line in test_output.splitlines())
    report['callback_reentry_decisions'] = {
        'default': len(re.findall(r'(?m)^\s+--- PASS: TestWrapperReentry/', test_output)),
        'optional': len(re.findall(r'(?m)^\s+--- PASS: TestOptionalWrapperReentry/(?:ZSTD|LZ4)/', test_output))}
    report['driver_sha256'] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    report['shared_reentry_source_sha256'] = hashlib.sha256(
        (ROOT / 'test/resources/compression_reentry_go_test.go').read_bytes()).hexdigest()
    (directory / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
