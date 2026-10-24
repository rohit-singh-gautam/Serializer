# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Compile and execute all sixteen XML/compression provider combinations with real native SDKs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
PROVIDERS = ('ZSTD', 'LZ4', 'ZLIB', 'XML')
PROFILE_COUNT = 1 << len(PROVIDERS)


def digest(path):
    """Identify exact source and linked consumer images without normalizing bytes."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def snapshot(source):
    """Watch native build/runtime inputs and the independently maintained probe before execution."""
    paths = [source/'CMakeLists.txt', *(source/'cmake').rglob('*.cmake'),
             *(source/'cmake').glob('*.in'), *(source/'src').rglob('*'),
             *(source/'include').rglob('*'), *(ROOT/'qualification/provider_matrix').iterdir(), Path(__file__)]
    return {str(path): digest(path) for path in sorted(set(paths)) if path.is_file()}


def main():
    """Run bounded serial profiles, retaining real commands/failures and rejecting incomplete inventories."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--dependency-prefix', type=Path, action='append', default=[])
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--cxx')
    parser.add_argument('--jobs', type=int, choices=(1, 2), default=1)
    parser.add_argument('--profiles', type=int, nargs='+', choices=range(PROFILE_COUNT),
                        help='Diagnostic subset; a full pass requires all sixteen distinct profiles')
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    build = args.build.resolve()
    if build == ROOT or ROOT.is_relative_to(build) or build.is_relative_to(ROOT) and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Keep repository outputs beneath ignored out/')
    profiles = args.profiles if args.profiles is not None else list(range(PROFILE_COUNT))
    if len(profiles) != len(set(profiles)):
        parser.error('Duplicate profiles are not a complete inventory')
    build.mkdir(parents=True, exist_ok=True)
    attempts = build/'attempts'
    attempts.mkdir(exist_ok=True)
    attempt = attempts/str(len(list(attempts.iterdir())) + 1)
    attempt.mkdir()
    environment = os.environ.copy()
    prefixes = [prefix.resolve(strict=True) for prefix in args.dependency_prefix]
    if prefixes:
        for variable, child in (('PATH', 'bin'), ('LD_LIBRARY_PATH', 'lib/x86_64-linux-gnu'),
                                ('LIBRARY_PATH', 'lib/x86_64-linux-gnu')):
            environment[variable] = os.pathsep.join(str(prefix/child) for prefix in prefixes) + os.pathsep + environment.get(variable, '')
    before = snapshot(source)
    report = {'status': 'failed', 'host': platform.platform(), 'profiles': [], 'commands': [],
              'source': str(source), 'sources_sha256': before, 'maximum_workers': args.jobs}
    started = time.monotonic()

    def run(command, label):
        """Record exact argv, exit code and full native diagnostics for every configure/build/execution."""
        beginning = time.monotonic()
        result = subprocess.run([str(value) for value in command], env=environment,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=1800)
        log = attempt/(label+'.log')
        log.write_bytes(result.stdout)
        report['commands'].append({'argv': list(map(str, command)), 'returncode': result.returncode,
                                  'elapsed_seconds': round(time.monotonic()-beginning, 3),
                                  'log': str(log), 'log_sha256': digest(log)})
        if result.returncode:
            raise RuntimeError(label+' failed; see '+str(log))
        return result.stdout.decode('utf-8', errors='strict')

    try:
        report['cmake_version'] = run([args.cmake, '--version'], 'cmake-version').splitlines()[0]
        baselines = {}
        for mask in profiles:
            directory = build/f'profile-{mask:02d}'
            selected = {name: bool(mask & (1 << index)) for index, name in enumerate(PROVIDERS)}
            command = [args.cmake, '-S', ROOT/'qualification/provider_matrix', '-B', directory,
                       '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DSERIALIZER_SOURCE='+str(source)]
            command += ['-DSERIALIZER_WITH_'+name+'='+('ON' if value else 'OFF') for name, value in selected.items()]
            if prefixes:
                command += ['-DCMAKE_PREFIX_PATH='+';'.join(map(str, prefixes))]
            if args.cxx:
                command += ['-DCMAKE_CXX_COMPILER='+args.cxx]
            run(command, f'{mask:02d}-configure')
            run([args.cmake, '--build', directory, '--target', 'provider_matrix_test', '--parallel', args.jobs], f'{mask:02d}-build')
            executable = directory/('provider_matrix_test.exe' if os.name == 'nt' else 'provider_matrix_test')
            observed = json.loads(run([executable], f'{mask:02d}-probe'))
            expected = dict(none=True, zstd=selected['ZSTD'], lz4=selected['LZ4'],
                            gzip=selected['ZLIB'], zlib=selected['ZLIB'], deflate=selected['ZLIB'])
            assert {row['name']: row['available'] for row in observed['formats']} == expected
            # Available compression formats make six decisions; unavailable formats make five.
            # XML makes its availability decision plus either two value checks or one rejection.
            expected_checks = sum(6 if available else 5 for available in expected.values()) + (3 if selected['XML'] else 2)
            assert observed['xml'] == selected['XML'] and observed['checks'] == expected_checks
            for row in observed['formats']:
                if row['available']:
                    assert baselines.setdefault(row['name'], row['encoded_hex']) == row['encoded_hex'], row['name']
            report['profiles'].append({'mask': mask, 'providers': selected, 'observed': observed,
                                       'executable_sha256': digest(executable),
                                       'cmake_cache_sha256': digest(directory/'CMakeCache.txt')})
            print('PASS profile', mask, selected, 'checks', observed['checks'], flush=True)
        assert snapshot(source) == before, 'Native source/probe drifted during qualification'
        full = set(profiles) == set(range(PROFILE_COUNT))
        if full:
            assert len(report['profiles']) == PROFILE_COUNT and len(baselines) == 6
        report['status'] = 'passed' if full else 'diagnostic_passed'
        report['checks'] = sum(row['observed']['checks'] for row in report['profiles'])
    except BaseException as failure:
        report['failure'] = repr(failure)
        raise
    finally:
        report['elapsed_seconds'] = round(time.monotonic()-started, 3)
        text = json.dumps(report, indent=2)+'\n'
        (attempt/'verification.json').write_text(text, encoding='utf-8')
        (build/'verification.json').write_text(text, encoding='utf-8')


if __name__ == '__main__':
    main()
