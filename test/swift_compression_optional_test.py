# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify real generated Swift optional providers with explicit caller-linked native C adapters."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

import swift_compression_test as core
from java_compression_optional_test import fixtures
from go_compression_optional_test import native_fixtures

ROOT = Path(__file__).resolve().parents[1]
ADAPTER = ROOT/'qualification/swift_compression_providers'


def run(command, directory, environment):
    """Retain exact actual SDK commands and require their bounded execution to succeed."""
    command = list(map(str, command))
    print('RUN', subprocess.list2cmdline(command), flush=True)
    result = subprocess.run(command, cwd=directory, env=environment, capture_output=True,
                            text=True, encoding='utf-8', errors='replace', timeout=300)
    print(result.stdout+result.stderr, end='', flush=True)
    result.check_returncode()
    return result.stdout


def compile_adapter(args, build, environment):
    """Build the explicit C11 provider adapter with strict warnings and the application's native headers."""
    destination = build/'provider';destination.mkdir(exist_ok=True)
    for name in ('provider.h', 'provider.c', 'module.modulemap'):
        shutil.copyfile(ADAPTER/name, destination/name)
    compiler = str(args.cc)
    msvc = Path(compiler).name.lower() in ('cl', 'cl.exe')
    obj = destination/('provider.obj' if msvc else 'provider.o')
    if msvc:
        command = [compiler, '/nologo', '/MD', '/std:c11', '/W4', '/WX', '/c',
                   '/I'+str(args.provider_include), destination/'provider.c', '/Fo'+str(obj)]
    else:
        command = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-pedantic',
                   '-I'+str(args.provider_include), '-c', destination/'provider.c', '-o', obj]
    run(command, build, environment)
    return destination, obj


def compile_consumer(args, case, provider, obj, environment, *, linked=True):
    """Compile generated Swift and caller-owned bindings; the absent profile links no optional libraries."""
    program = case/('consumer.exe' if os.name == 'nt' else 'consumer')
    command = [args.swiftc, '-O', '-warnings-as-errors', '-swift-version', '6',
               '-module-cache-path', case/'module-cache', case/'Schema.swift']
    if linked:
        command += ['-I', provider, case/'adapter.swift', obj, args.zstd_library, args.lz4_library]
    command += [case/'main.swift', '-o', program]
    run(command, case, environment)
    return program


def verify_outputs(build, helper, environment):
    """Require independent Python exact-frame checks and both native C++ decoder interfaces."""
    from compression import zstd
    import lz4.frame
    count = 0
    for name in ('ZSTD', 'LZ4'):
        outputs = [(build/f'output-{name}-{protocol}.bin', (build/f'native-{protocol}.bin').read_bytes())
                   for protocol in ('JSON', 'BINARY_NONE', 'BINARY_INTEGER', 'BINARY_STRING')]
        outputs += [(build/f'level-{name}-{level}-{sample}.bin', b'A'*65537 if sample else b'')
                    for level in (0, 1, 6, 9) for sample in (0, 1)]
        for path, expected in outputs:
            decoder = zstd.ZstdDecompressor() if name == 'ZSTD' else lz4.frame.LZ4FrameDecompressor()
            frame = path.read_bytes()
            if decoder.decompress(frame) != expected or not decoder.eof or decoder.unused_data or not frame[4] & 4:
                raise AssertionError('Independent exact checksummed frame differs: '+path.name)
            for mode in ('decode', 'stream_decode'):
                destination = build/(path.stem+'-'+mode+'.plain')
                run([helper, mode, name.lower(), path, destination], build, environment)
                if destination.read_bytes() != expected:
                    raise AssertionError('Native consumer differs: '+path.name)
            count += 1
    if json.loads((build/'native-JSON.bin').read_bytes())['account_id'] != 42:
        raise AssertionError('Ordinary native JSON field identity changed')
    return count


def main():
    """Generate with the actual compiler and retain explicit SDK/provider/protocol provenance."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--swiftc', default=shutil.which('swiftc'))
    parser.add_argument('--cc', default=shutil.which('cl') or shutil.which('cc'))
    parser.add_argument('--provider-include', type=Path, required=True)
    parser.add_argument('--zstd-library', type=Path, required=True)
    parser.add_argument('--lz4-library', type=Path, required=True)
    parser.add_argument('--provider-runtime-directory', type=Path)
    parser.add_argument('--provider-path', type=Path)
    parser.add_argument('--cpp-helper', type=Path, required=True)
    parser.add_argument('--cpp-linked-helper', type=Path, required=True)
    args = parser.parse_args()
    if not args.swiftc or not args.cc:
        parser.error('Actual Swift and C11 SDKs are required')
    build = args.build.resolve()
    if build == ROOT or build.is_relative_to(ROOT) and build.relative_to(ROOT).parts[0] != 'out':
        parser.error('In-repository output must remain beneath ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    for field in ('compiler', 'provider_include', 'zstd_library', 'lz4_library', 'cpp_helper', 'cpp_linked_helper'):
        setattr(args, field, getattr(args, field).resolve())
    if args.provider_path:
        sys.path.insert(0, str(args.provider_path.resolve()))
    environment = os.environ.copy()
    temporary = build/'temporary';temporary.mkdir(exist_ok=True)
    environment['TMP'] = environment['TEMP'] = environment['TMPDIR'] = str(temporary)
    if args.provider_runtime_directory:
        environment['PATH'] = str(args.provider_runtime_directory.resolve())+os.pathsep+environment.get('PATH', '')
    compiler_hash = hashlib.sha256(args.compiler.read_bytes()).hexdigest()
    counts = fixtures(build, ('ZSTD', 'LZ4'))
    producers = native_fixtures(args, build, environment, counts)
    provider, obj = compile_adapter(args, build, environment)
    helper, _ = core.generate(args.compiler, build)
    consumer = (ROOT/'test/resources/compression_swift_optional_main.swift').read_text(encoding='utf-8')
    adapter = (ADAPTER/'adapter.swift').read_text(encoding='utf-8')
    (build/'main.swift').write_text(consumer.replace('__COMPRESSION_NAME__', helper), encoding='utf-8')
    (build/'adapter.swift').write_text(adapter.replace('__COMPRESSION_NAME__', helper), encoding='utf-8')
    program = compile_consumer(args, build, provider, obj, environment)
    output = run([program, build, build], build, environment)
    match = re.search(r'PASS Swift optional (\d+) independent decisions, 24 outputs, (\d+) cancellation boundaries, (\d+) close boundaries', output)
    if not match or int(match[1]) != sum(counts.values()):
        raise AssertionError('Incomplete actual SDK optional decision inventory')
    outputs = verify_outputs(build, args.cpp_helper, environment)
    profiles = {'normal': {'generated_bytes': (build/'Schema.swift').stat().st_size,
                           'generated_sha256': hashlib.sha256((build/'Schema.swift').read_bytes()).hexdigest()}}
    for name, schema, flags in (
            ('collision', core.SCHEMA+' class SerializerCompression {} class SerializerCompression_2 {}', ()),
            ('provider-names', core.SCHEMA+' class Provider {} class Context {} class Progress {} class OptionalFrame {} class NativeProviderFailure {}', ()),
            ('binary', core.SCHEMA, ('--protocols', 'binary_none')),
            ('absent', core.SCHEMA, ())):
        case = build/name;case.mkdir(exist_ok=True)
        prefix, _ = core.generate(args.compiler, case, schema, flags)
        text = core.smoke(prefix, collision=name == 'collision', binary=name == 'binary')
        if name != 'absent':
            text = text.replace('format: .gzip', 'format: .zstd, provider: C.nativeProvider(.zstd)')
            text += 'let lz4 = C.nativeProvider(C.Format.lz4)\nlet packedLz4 = try C.compress(native,options:.init(format:.lz4,provider:lz4))\nlet plainLz4 = try C.decompress(packedLz4,options:.init(format:.lz4,provider:lz4))\nprecondition(plainLz4 == native)\n'
            (case/'adapter.swift').write_text(adapter.replace('__COMPRESSION_NAME__', prefix), encoding='utf-8')
        else:
            text += 'precondition(!C.available(.zstd) && !C.available(.lz4))\n'
        text += 'let legacyEncode:(C.Format,Int,Int,Int,(() throws -> Void)?) -> C.EncodeOptions = C.EncodeOptions.init\n_ = legacyEncode(.none,1,1,6,nil)\n'
        text += 'let legacyDecode:(C.Format,Int,Int,Int,(() throws -> Void)?) -> C.DecodeOptions = C.DecodeOptions.init\n_ = legacyDecode(.none,1,1,32768,nil)\n'
        if name == 'provider-names':
            for model in ('Provider', 'Context', 'Progress', 'OptionalFrame', 'NativeProviderFailure'):
                text += 'let wire'+model+' = try '+model+'().encode(.BINARY_NONE)\n_ = try '+model+'.decode(wire'+model+',.BINARY_NONE)\n'
        (case/'main.swift').write_text(text, encoding='utf-8')
        program = compile_consumer(args, case, provider, obj, environment, linked=name != 'absent')
        run([program], case, environment)
        profiles[name] = {'generated_bytes': (case/'Schema.swift').stat().st_size,
                          'generated_sha256': hashlib.sha256((case/'Schema.swift').read_bytes()).hexdigest()}
    assert hashlib.sha256(args.compiler.read_bytes()).hexdigest() == compiler_hash
    report = {'swift': run([args.swiftc, '--version'], build, environment).strip(),
              'input_decisions': counts, 'independent_outputs': outputs, 'native_cpp_consumers': 2*outputs,
              'native_cpp_producers': producers, 'cancellation_boundaries': int(match[2]),
              'callback_close_boundaries': int(match[3]), 'profiles': profiles,
              'native_provider_versions': re.search(r'PROVIDERS ([^\r\n]+)', output)[1],
              'compiler_sha256': compiler_hash, 'test_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'adapter_sha256': {str(path.relative_to(ADAPTER)): hashlib.sha256(path.read_bytes()).hexdigest()
                                 for path in sorted(ADAPTER.rglob('*')) if path.is_file()}}
    (build/'verification.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
