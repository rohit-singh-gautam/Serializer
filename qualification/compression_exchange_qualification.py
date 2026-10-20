# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Run a mandatory, separate C++/Python/Java core-compression exchange inventory."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import re
import shutil
import struct
import subprocess
import sys
import zlib


ROOT = Path(__file__).resolve().parent.parent
SOURCES = ROOT / 'qualification' / 'compression_exchange'
FORMATS = ('NONE', 'GZIP', 'ZLIB', 'DEFLATE')
PROTOCOLS = ('JSON', 'BINARY_NONE', 'BINARY_INTEGER', 'BINARY_STRING')
LANGUAGES = ('cpp', 'python', 'java')
WINDOW_BYTES = 32768
LIMIT_BYTES = 8 * 1024 * 1024
RANDOM_SEED = 0x43504D5052455353


def digest(path):
    """Identify an exact tracked source or generated artifact without embedding its contents."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def tool(spelling, label):
    """Resolve every requested SDK/tool explicitly; absence is a hard qualification failure."""
    candidate = shutil.which(str(spelling)) if spelling else None
    if not candidate:
        raise RuntimeError(label + ' is required and was not found: ' + str(spelling))
    return str(Path(candidate).resolve())


def bounded_output(path):
    """Keep repository-generated output beneath ignored out without deleting existing directories."""
    resolved = path.resolve()
    if resolved == ROOT or ROOT in resolved.parents and resolved.relative_to(ROOT).parts[0] != 'out':
        raise ValueError('In-repository qualification output must remain beneath ignored out')
    return resolved


def run(command, log, environment, timeout=240):
    """Retain exact subprocess diagnostics and fail on any missing tool, provider, or job."""
    command = [str(part) for part in command]
    result = subprocess.run(command, cwd=ROOT, env=environment, capture_output=True,
                            text=True, encoding='utf-8', errors='replace', timeout=timeout)
    log.parent.mkdir(parents=True, exist_ok=True)
    log.write_text(json.dumps(command) + '\n' + result.stdout + result.stderr, encoding='utf-8')
    if result.returncode:
        raise RuntimeError(f'{log.name}: command exited {result.returncode}\n{result.stdout}{result.stderr}')
    return (result.stdout + result.stderr).strip()


def report(command, log, environment):
    """Require one adapter's structured final result rather than counting mere process success."""
    output = run(command, log, environment)
    return json.loads(output.splitlines()[-1])


def pack(plain, name):
    """Produce independent standard identity/RFC1952/RFC1950/RFC1951 fixtures."""
    if name == 'NONE':
        return plain
    encoder = zlib.compressobj(6, zlib.DEFLATED, {'GZIP': 31, 'ZLIB': 15, 'DEFLATE': -15}[name])
    return encoder.compress(plain) + encoder.flush()


def unpack(frame, name):
    """Use an independent provider with explicit EOF/trailing checks for emitted frames."""
    if name == 'NONE':
        return frame
    decoder = zlib.decompressobj({'GZIP': 31, 'ZLIB': 15, 'DEFLATE': -15}[name])
    plain = decoder.decompress(frame, LIMIT_BYTES + 1)
    if len(plain) > LIMIT_BYTES or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
        raise AssertionError('Independent frame validation rejected ' + name)
    return plain


def fixtures(directory, exchanges):
    """Write one shared corpus with independently constructed framing and exact byte boundaries."""
    directory.mkdir(parents=True, exist_ok=True)
    rows = []
    payloads = {'empty': b'', 'byte': b'A',
                'repeated': (b'compression shared corpus\x00\xff\n' * 2600)[:65537],
                'random': random.Random(RANDOM_SEED).randbytes(65537)}

    def write(name, data):
        """Keep one immutable oracle/fixture under the isolated corpus directory."""
        path = directory / (name + '.bin')
        path.write_bytes(data)
        return path.as_posix()

    def add(case, name, data, plain=b'', wanted='ok', operation='decode',
            max_input=LIMIT_BYTES, max_output=LIMIT_BYTES, window=WINDOW_BYTES, protocol=None):
        """Describe a language-neutral decision using bounded, tab-free absolute fixture paths."""
        source = write(case + '-input', data)
        oracle = protocol if protocol else write(case + '-oracle', plain)
        rows.append((case, operation, name, source, oracle, wanted, max_input, max_output, window))

    for label, plain in payloads.items():
        for name in FORMATS:
            frame = pack(plain, name)
            if unpack(frame, name) != plain:
                raise AssertionError('Independent fixture construction failed')
            add(f'{name.lower()}-{label}', name, frame, plain,
                max_input=len(frame), max_output=len(plain))
    plain = payloads['repeated']
    for name in FORMATS:
        frame = pack(plain, name)
        add(name.lower() + '-input-tight', name, frame, wanted='resource_limit', max_input=len(frame)-1)
        add(name.lower() + '-output-tight', name, frame, wanted='resource_limit', max_output=len(plain)-1)
        add(name.lower() + '-encode-input-tight', name, plain, wanted='resource_limit',
            operation='encode', max_input=len(plain)-1)
        add(name.lower() + '-encode-output-zero', name, plain, wanted='resource_limit',
            operation='encode', max_output=0)
        add(name.lower() + '-encode-exact-output', name, plain, operation='encode_bounds')
        if name == 'NONE':
            continue
        add(name.lower() + '-window-tight', name, frame, wanted='resource_limit', window=WINDOW_BYTES-1)
        for position in sorted({0, 1, len(frame)//2, len(frame)-1}):
            add(f'{name.lower()}-truncated-{position}', name, frame[:position], wanted='invalid_data')
        add(name.lower() + '-trailing', name, frame + b'\x00', wanted='invalid_data')
        add(name.lower() + '-concatenated', name, frame + pack(b'next', name), wanted='invalid_data')
        if name in ('GZIP', 'ZLIB'):
            corrupt = bytearray(frame)
            corrupt[-1] ^= 1
            add(name.lower() + '-checksum', name, bytes(corrupt), wanted='invalid_data')

    # RFC1952 optional extra/name/comment/header-CRC fields are assembled independently.
    header = b'\x1f\x8b\x08\x1e\x00\x00\x00\x00\x00\xff'
    header += struct.pack('<H', 3) + b'ext' + b'name\x00comment\x00'
    header_crc = struct.pack('<H', zlib.crc32(header) & 0xffff)
    trailer = struct.pack('<II', zlib.crc32(plain), len(plain))
    optional = header + header_crc + pack(plain, 'DEFLATE') + trailer
    if unpack(optional, 'GZIP') != plain:
        raise AssertionError('Independent optional GZIP header failed')
    add('gzip-optional-header', 'GZIP', optional, plain)
    corrupt = bytearray(optional)
    corrupt[len(header)] ^= 1
    add('gzip-header-crc', 'GZIP', bytes(corrupt), wanted='invalid_data')
    for case, offset, replacement in (('magic', 0, 0), ('method', 2, 0), ('reserved', 3, 0xe0)):
        corrupt = bytearray(pack(plain, 'GZIP'))
        corrupt[offset] = replacement
        add('gzip-invalid-' + case, 'GZIP', bytes(corrupt), wanted='invalid_data')
    add('gzip-unterminated-name', 'GZIP', b'\x1f\x8b\x08\x08' + b'\x00'*6 + b'name', wanted='invalid_data')
    add('gzip-truncated-extra', 'GZIP', b'\x1f\x8b\x08\x04' + b'\x00'*6 + b'\xff\xffextra', wanted='invalid_data')
    dictionary = b'common dictionary'
    encoder = zlib.compressobj(6, zlib.DEFLATED, 15, zdict=dictionary)
    add('zlib-dictionary-required', 'ZLIB', encoder.compress(plain) + encoder.flush(), wanted='invalid_data')

    # A valid outer frame must never hide suffix bytes after one exact generated object.
    for protocol in PROTOCOLS:
        ordinary = (exchanges / f'cpp-{protocol}-NONE.bin').read_bytes()
        for name in FORMATS:
            add(f'inner-{protocol.lower()}-{name.lower()}', name, pack(ordinary + b'\x00', name),
                operation='inner', wanted='native_error', protocol=protocol)
    path = directory / 'corpus.tsv'
    path.write_text(''.join('\t'.join(map(str, row)) + '\n' for row in rows), encoding='utf-8')
    return path, rows


def main():
    """Build real selected adapters, exchange every model, and record complete common-corpus results."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--prefix', type=Path, required=True, help='Installed full core-compression Serializer runtime')
    parser.add_argument('--dependency-prefix', type=Path, action='append', default=[])
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--generator', default='Ninja')
    parser.add_argument('--make-program', type=Path)
    parser.add_argument('--javac', default='javac')
    parser.add_argument('--java', default='java')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build, output = bounded_output(args.build), bounded_output(args.output)
    build.mkdir(parents=True, exist_ok=True)
    output.parent.mkdir(parents=True, exist_ok=True)
    compiler = tool(args.compiler.resolve(), 'Serializer compiler')
    cmake, javac, java = tool(args.cmake, 'CMake'), tool(args.javac, 'javac 17+'), tool(args.java, 'Java 17+')
    environment = os.environ.copy()
    prefixes = [args.prefix.resolve()] + [path.resolve() for path in args.dependency_prefix]
    if not all(path.is_dir() for path in prefixes):
        raise ValueError('Every explicitly selected runtime/dependency prefix must exist')
    environment['PATH'] = os.pathsep.join(str(path / 'bin') for path in prefixes) + os.pathsep + environment.get('PATH', '')
    manifest_path = SOURCES / 'manifest.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    if (manifest.get('version') != 1 or manifest.get('id') != 'compression_core_exchange'
            or tuple(manifest.get('languages', ())) != LANGUAGES or tuple(manifest.get('formats', ())) != FORMATS
            or tuple(manifest.get('protocols', ())) != PROTOCOLS or manifest.get('expected_model_exchanges') != 144
            or manifest.get('exchange') != 'all_producers_to_all_consumers'
            or manifest.get('corpus') != 'independent_core_framing_and_byte_limits_v1'):
        raise ValueError('Unknown or incomplete mandatory compression exchange inventory')
    schema, expected = ((ROOT / manifest[key]).resolve() for key in ('schema', 'input'))
    if schema.parent != SOURCES or expected.parent != SOURCES:
        raise ValueError('Manifest fixture paths must stay in the tracked compression inventory')
    logs, generated = build / 'logs', build / 'generated'
    generated.mkdir(exist_ok=True)
    versions = {
        'compiler': run([compiler, '--version'], logs / 'compiler-version.log', environment),
        'cmake': run([cmake, '--version'], logs / 'cmake-version.log', environment).splitlines()[0],
        'javac': run([javac, '-version'], logs / 'javac-version.log', environment),
        'java': run([java, '-version'], logs / 'java-version.log', environment),
        'python': sys.version,
        'zlib_compiled': zlib.ZLIB_VERSION,
        'zlib_runtime': zlib.ZLIB_RUNTIME_VERSION,
    }
    cpp_build, classes = build / 'cpp', build / 'classes'
    classes.mkdir(exist_ok=True)
    command = [cmake, '-S', SOURCES, '-B', cpp_build, '-G', args.generator,
               '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_PREFIX_PATH=' + ';'.join(map(str, prefixes)),
               '-DSERIALIZER_GENERATOR_EXECUTABLE=' + compiler]
    if args.make_program:
        command.append('-DCMAKE_MAKE_PROGRAM=' + tool(args.make_program.resolve(), 'Build program'))
    run(command, logs / 'cpp-configure.log', environment)
    run([cmake, '--build', cpp_build, '--config', 'Release', '--target', 'compression_exchange_cpp', '--parallel', '4'],
        logs / 'cpp-build.log', environment)
    compiler_metadata = next((cpp_build / 'CMakeFiles').glob('*/CMakeCXXCompiler.cmake')).read_text(encoding='utf-8')
    versions['cpp_compiler'] = {
        key: re.search(r'set\(CMAKE_CXX_COMPILER_' + key + r' "([^"]+)"\)', compiler_metadata).group(1)
        for key in ('ID', 'VERSION')
    }
    python_output, java_output = generated / 'model.py', generated / 'Schema.java'
    run([compiler, '--input', schema, '--language', 'python', '--python.output', python_output],
        logs / 'python-generate.log', environment)
    run([compiler, '--input', schema, '--language', 'java', '--output', java_output],
        logs / 'java-generate.log', environment)
    run([javac, '--release', '17', '-encoding', 'UTF-8', '-Xlint:all', '-Werror', '-d', classes,
         java_output, SOURCES / 'CompressionExchange.java'], logs / 'java-build.log', environment)
    executable = cpp_build / ('compression_exchange_cpp.exe' if os.name == 'nt' else 'compression_exchange_cpp')
    if not executable.is_file():
        executable = cpp_build / 'Release' / executable.name
    runners = {'cpp': [executable],
               'python': [sys.executable, '-B', SOURCES / 'compression_exchange.py', python_output],
               'java': [java, '-cp', classes, 'CompressionExchange']}
    exchange = build / 'exchange'
    exchange.mkdir(exist_ok=True)
    for language in LANGUAGES:
        result = report(runners[language] + ['produce', expected, exchange], logs / (language + '-produce.log'), environment)
        if result != {'produced': len(FORMATS) * len(PROTOCOLS)}:
            raise AssertionError('Incomplete required producer: ' + language)
    jobs = []
    independent_frames = 0
    for language in LANGUAGES:
        for protocol in PROTOCOLS:
            ordinary = (exchange / f'{language}-{protocol}-NONE.bin').read_bytes()
            for name in FORMATS:
                frame_path = exchange / f'{language}-{protocol}-{name}.bin'
                if unpack(frame_path.read_bytes(), name) != ordinary:
                    raise AssertionError('Producer changed the ordinary codec representation')
                independent_frames += 1
                jobs.append((protocol, name, frame_path.as_posix()))
    jobs_path = build / 'exchange.tsv'
    jobs_path.write_text(''.join('\t'.join(row) + '\n' for row in jobs), encoding='utf-8')
    corpus_path, corpus = fixtures(build / 'corpus', exchange)
    results = {}
    independent_boundary_frames = 0
    for language in LANGUAGES:
        decoded = build / ('decoded-' + language)
        result = report(runners[language] + ['check', expected, jobs_path, corpus_path, decoded],
                        logs / (language + '-check.log'), environment)
        if result != {'model_exchanges': len(jobs), 'corpus_decisions': len(corpus)}:
            raise AssertionError('Incomplete required consumer: ' + language)
        for row in corpus:
            if row[1] == 'encode_bounds':
                if unpack((decoded / (row[0] + '.bin')).read_bytes(), row[2]) != Path(row[3]).read_bytes():
                    raise AssertionError('Bounded provider output failed the independent oracle')
                independent_boundary_frames += 1
        results[language] = result
    tracked_sources = sorted(path for path in SOURCES.iterdir() if path.is_file()) + [Path(__file__).resolve()]
    runtime_artifacts = sorted(path for path in (prefixes[0] / 'lib').rglob('*serializer_runtime*') if path.is_file())
    if not runtime_artifacts:
        raise AssertionError('Installed C++ runtime artifact was not identified')
    generated_cpp = list((cpp_build / 'generated').rglob('model.hpp'))
    if len(generated_cpp) != 1:
        raise AssertionError('Actual generated C++ header was not identified')
    result = {
        'qualification': manifest['id'], 'utc': datetime.now(timezone.utc).isoformat(),
        'source_baseline': run(['git', 'rev-parse', 'HEAD'], logs / 'source-baseline.log', environment),
        'source_state': 'baseline plus uncommitted Serializer 2.14.0 changes',
        'platform': platform.platform(), 'tools': versions,
        'compiler_path': compiler, 'runtime_prefix': str(prefixes[0]),
        'dependency_prefixes': [str(path) for path in prefixes[1:]],
        'source_sha256': {str(path.relative_to(ROOT)).replace('\\', '/'): digest(path) for path in tracked_sources},
        'generated_sha256': {'cpp': digest(generated_cpp[0]), 'python': digest(python_output), 'java': digest(java_output)},
        'compiler_sha256': digest(Path(compiler)),
        'cpp_executable_sha256': digest(executable),
        'installed_runtime_sha256': {str(path.relative_to(prefixes[0])).replace('\\', '/'): digest(path)
                                     for path in runtime_artifacts},
        'manifest': manifest, 'results': results,
        'model_exchanges': sum(item['model_exchanges'] for item in results.values()),
        'shared_corpus_cases': len(corpus), 'shared_corpus_decisions': len(corpus) * len(LANGUAGES),
        'independent_producer_frame_checks': independent_frames,
        'independent_bounded_output_checks': independent_boundary_frames,
        'corpus_sha256': digest(corpus_path), 'random_seed': RANDOM_SEED,
        'corpus_case_ids': [row[0] for row in corpus],
        'scope': ['C++/Python/Java only; four mandatory formats and four existing native protocols',
                  'One owning Unicode generated model; selected independent framing/byte-limit corpus',
                  'No ZSTD/LZ4, other languages, XML payload, cumulative shared-operation context or SDK matrix qualification',
                  'Z11 remains open for its broader optional-provider and complete-operation profiles'],
    }
    if result['model_exchanges'] != manifest['expected_model_exchanges']:
        raise AssertionError('Mandatory generated model exchange count differs')
    output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({key: result[key] for key in ('model_exchanges', 'shared_corpus_cases',
                                                 'shared_corpus_decisions', 'independent_producer_frame_checks',
                                                 'independent_bounded_output_checks')}))


if __name__ == '__main__':
    main()
