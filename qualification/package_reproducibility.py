# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Require an independently checked canonical package matrix from all eleven actual SDKs."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
ADAPTERS = Path(__file__).resolve().with_suffix('')
sys.path.insert(0, str(ADAPTERS))
import oracle

LANGUAGES = ('cpp', 'python', 'c', 'rust', 'go', 'java', 'javascript', 'typescript', 'csharp', 'kotlin', 'swift')


def sha(path):
    """Identify exact source, provider and completed artifact bytes."""
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def source_snapshot():
    """Freeze the complete maintained matrix implementation and inventory before SDK execution."""
    paths = [Path(__file__), ROOT / 'test/resources/package_relocation.serializer', ROOT / 'test/resources/opc_profile.ini',
             ROOT / 'test/package_node_dependencies.py', ROOT / 'test/resources/xml_shapes/typescript/package.json',
             ROOT / 'test/resources/xml_shapes/typescript/package-lock.json',
             ROOT / 'example/xml/rust/Cargo.toml', ROOT / 'example/xml/rust/Cargo.lock',
             *sorted(path for path in ADAPTERS.iterdir() if path.is_file() and path.suffix != '.md')]
    return {path.relative_to(ROOT).as_posix(): sha(path) for path in paths}


def build_local(language, compiler, directory, options, run):
    """Prepare the native C++ consumer or the public Python OPC facade and physical ZIP probe."""
    if language == 'cpp':
        executable = Path(options['cpp_producer']).resolve(strict=True)
        generated = Path(options['cpp_generated']).resolve(strict=True)
        provider = json.loads(run([executable, '--provider'], 'provider'))
        artifacts = {str(executable): sha(executable)}
        if options.get('cpp_provider_library'):
            library = Path(options['cpp_provider_library']).resolve(strict=True)
            artifacts[str(library)] = sha(library)
        provider['artifacts'] = artifacts
        return {'launch': [str(executable)], 'generated': [str(generated)], 'tool': 'CMake-built strict C++23 consumer', 'provider': provider}
    generated = directory / 'schema.py'
    run([compiler, '--input', ADAPTERS / 'python_profile.serializer', '--language', 'python', '--python.output', generated], 'generation')
    shutil.copyfile(ADAPTERS / 'python_producer.py', directory / 'main.py')
    launch = [sys.executable, '-B', str(directory / 'main.py')]
    provider = json.loads(run([*launch, '--provider'], 'provider'))
    import zlib
    provider['artifacts'] = {str(Path(sys.executable).resolve()): sha(sys.executable)}
    module = getattr(zlib, '__file__', None)
    if module:
        library = Path(module).resolve(strict=True)
        provider['artifacts'][str(library)] = sha(library)
        if os.name != 'nt':
            dependencies = run(['ldd', library], 'python-provider-dependencies')
            for value in re.findall(r'\blibz[^\s]*\s+=>\s+(/[^\s]+)', dependencies):
                linked = Path(value).resolve(strict=True)
                provider['artifacts'][str(linked)] = sha(linked)
    elif os.name == 'nt':
        library = Path(sys.base_prefix) / f'python{sys.version_info.major}{sys.version_info.minor}.dll'
        library = library.resolve(strict=True)
        provider['artifacts'][str(library)] = sha(library)
    else:
        # A built-in Unix module lives in the already pinned Python image; retain linked provider libraries too.
        dependencies = run(['ldd', sys.executable], 'python-builtin-provider-dependencies')
        for value in re.findall(r'\b(?:libz|libpython)[^\s]*\s+=>\s+(/[^\s]+)', dependencies):
            linked = Path(value).resolve(strict=True)
            provider['artifacts'][str(linked)] = sha(linked)
    return {'launch': launch, 'generated': [str(generated)], 'tool': sys.version, 'provider': provider,
            'scope': 'Public schema-bound OPC Save; plain ZIP is an explicitly diagnostic generated private helper.'}


def execute(language, compiler, build, fixtures, manifest, options):
    """Build one actual consumer and require every fresh-process permutation and wire-policy check."""
    directory = build / language
    directory.mkdir(parents=True, exist_ok=True)
    commands = []
    started = time.monotonic()

    def run(argv, label, *, cwd=None, env=None):
        """Retain commands and exact diagnostics while failing missing tools and nonzero SDK exits."""
        command = [str(value) for value in argv]
        result = subprocess.run(command, cwd=cwd or directory, env=env, capture_output=True,
                                text=True, encoding='utf-8', errors='replace', timeout=1200)
        log = directory / (label + '.log')
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text(result.stdout + result.stderr, encoding='utf-8')
        commands.append({'argv': command, 'cwd': str(cwd or directory), 'returncode': result.returncode,
                         'log': str(log.relative_to(build)), 'log_sha256': sha(log)})
        if result.returncode:
            raise RuntimeError(f'{language} {label} failed ({result.returncode}): {(result.stdout + result.stderr)[-3000:]}')
        return (result.stdout + result.stderr).strip()

    try:
        if language in ('cpp', 'python'):
            consumer = build_local(language, compiler, directory, options, run)
        elif language in ('c', 'rust', 'go'):
            from native_build import build as native_build
            consumer = native_build(language, ROOT, compiler, directory, options, run)
        elif language in ('java', 'javascript', 'typescript', 'kotlin'):
            from jvm_js_build import build as jvm_js_build
            consumer = jvm_js_build(language, ROOT, compiler, directory, options, run)
        else:
            from managed_build import build as managed_build
            consumer = managed_build(language, ROOT, compiler, directory, options, run)
        generated = {str(Path(path).resolve()): sha(path) for path in consumer['generated']}
        launch_artifacts = {}
        for argument in consumer['launch']:
            try:
                candidate = Path(argument)
                if candidate.is_file():
                    candidate = candidate.resolve()
                    launch_artifacts[str(candidate)] = sha(candidate)
            except (OSError, ValueError):
                # Classpaths and command options are not individual executable artifacts.
                pass
        provider = consumer['provider']
        for path, digest in provider.get('artifacts', {}).items():
            assert sha(path) == digest, ('Provider artifact changed', path)
        fingerprint = hashlib.sha256(json.dumps(provider, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
        observations = []
        baseline = {}
        environment = None
        if consumer.get('environment'):
            environment = dict(os.environ, **consumer['environment'])
        output_directory = directory / 'outputs'
        output_directory.mkdir(exist_ok=True)
        for profile in manifest['profiles']:
            for forced in manifest['force_zip64']:
                key = profile + ('-zip64' if forced else '-zip32')
                for permutation in range(len(manifest['permutations'])):
                    for repetition in range(manifest['process_repetitions']):
                        name = f'{key}-p{permutation}-r{repetition}'
                        path = output_directory / (name + ('.opc' if profile.startswith('opc-') else '.zip'))
                        stdout = run([*consumer['launch'], fixtures, profile, permutation, path, int(forced)], name, env=environment)
                        protocol = [json.loads(line) for line in stdout.splitlines() if line.startswith('{')][-1]
                        assert protocol['profile'] == profile and protocol['permutation'] == permutation and protocol['force_zip64'] == forced
                        fixture = manifest['fixtures']['opc' if profile.startswith('opc-') else 'zip']
                        assert protocol['parts'] == len(fixture['parts']) and protocol['relationships'] == len(fixture['links'])
                        for field, value in provider.items():
                            if field not in ('artifacts', 'cross_host_identity'):
                                assert protocol['provider'][field] == value, (language, field)
                        if 'metadata_method' not in provider:
                            expected_method = manifest['backends'][language]['metadata_method'] if profile.startswith('opc-') else 0
                            assert protocol['provider']['metadata_method'] == expected_method, (language, 'profile metadata method')
                        inspection = oracle.inspect_archive(path, language, profile, forced, manifest)
                        digest = inspection['archive_sha256']
                        assert baseline.setdefault(key, digest) == digest, (language, key, permutation, repetition, 'Permutation/process output differs')
                        observations.append({'profile': profile, 'force_zip64': forced, 'permutation': permutation,
                            'repetition': repetition, 'path': str(path), 'protocol': protocol, 'inspection': inspection})
        expected = len(manifest['profiles']) * len(manifest['force_zip64']) * len(manifest['permutations']) * manifest['process_repetitions']
        assert len(observations) == expected
        assert all(sha(path) == digest for path, digest in generated.items()), 'Generated source changed during SDK qualification'
        assert all(sha(path) == digest for path, digest in provider.get('artifacts', {}).items()), 'Provider artifact changed during qualification'
        assert all(sha(path) == digest for path, digest in launch_artifacts.items()), 'Consumer image changed during qualification'
        report = {'status': 'passed', 'language': language, 'compiler_sha256': sha(compiler), 'tool': consumer['tool'],
            'provider': provider, 'provider_fingerprint_sha256': fingerprint, 'scope': consumer.get('scope'),
            'generated_sources_sha256': generated, 'consumer_images_sha256': launch_artifacts, 'provider_cross_host_identity': provider.get('cross_host_identity'),
            'observations': observations, 'canonical_archive_sha256': baseline, 'commands': commands,
            'elapsed_seconds': round(time.monotonic() - started, 3)}
    except Exception as failure:
        report = {'status': 'failed', 'language': language, 'error': str(failure), 'commands': commands,
                  'elapsed_seconds': round(time.monotonic() - started, 3)}
    (directory / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return report


def compare_hosts(current, previous):
    """Require stored ZIP equality and explicit eligibility before qualifying compressed byte equality."""
    comparisons = {}
    assert current['status'] == previous['status'] == 'passed' and previous['manifest_sha256'] == current['manifest_sha256']
    manifest = oracle.load_manifest()
    expected = {(profile, forced, permutation, repetition) for profile in manifest['profiles']
                for forced in manifest['force_zip64'] for permutation in range(len(manifest['permutations']))
                for repetition in range(manifest['process_repetitions'])}
    for report in (current, previous):
        assert report['full_inventory'] and set(report['languages']) == set(LANGUAGES), 'Cross-host comparison requires both complete inventories'
        assert not report['source_verification']['changed'] and report['source_verification']['compiler_unchanged'], 'Qualification source changed'
        for row in report['languages'].values():
            keys = [(item['profile'], item['force_zip64'], item['permutation'], item['repetition']) for item in row['observations']]
            assert len(keys) == len(expected) and set(keys) == expected, 'Incomplete or duplicate process/permutation matrix'
    snapshots = []
    for report in (current, previous):
        original = report['qualification_sources_sha256']
        normalized = {name.replace('\\', '/'): digest for name, digest in original.items()}
        assert len(original) == len(normalized), 'Duplicate normalized source identity'
        snapshots.append(normalized)
    assert snapshots[0] == snapshots[1], 'Cross-host qualification source snapshots differ'
    for language, row in current['languages'].items():
        old = previous['languages'][language]
        assert row['status'] == old['status'] == 'passed'
        eligible = bool(row.get('provider_cross_host_identity')) and row['provider_cross_host_identity'] == old.get('provider_cross_host_identity')
        eligible = eligible or row['provider_fingerprint_sha256'] == old['provider_fingerprint_sha256']
        values = []
        for first, second in zip(row['observations'], old['observations'], strict=True):
            for field in ('profile', 'force_zip64', 'permutation', 'repetition'):
                assert first[field] == second[field]
            assert first['inspection']['projection_sha256'] == second['inspection']['projection_sha256'], (language, first['path'])
            equal = first['inspection']['archive_sha256'] == second['inspection']['archive_sha256']
            contains_deflate = first['profile'].endswith('deflate') or 8 in first['inspection']['metadata_methods']
            if not contains_deflate or eligible:
                assert equal, (language, first['profile'], 'Eligible cross-host bytes differ')
            values.append({'profile': first['profile'], 'force_zip64': first['force_zip64'], 'permutation': first['permutation'],
                           'repetition': first['repetition'], 'byte_identical': equal, 'compressed_equality_eligible': not contains_deflate or eligible})
        comparisons[language] = {'provider_identity_equal': eligible, 'comparisons': values}
    return comparisons


def main():
    """Run the declared inventory; diagnostic subsets cannot produce a full qualification success."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--languages', default=','.join(LANGUAGES))
    parser.add_argument('--jobs', type=int, default=3)
    parser.add_argument('--compare-to', type=Path)
    for name in ('cpp-producer', 'cpp-generated', 'cpp-provider-library', 'cc', 'provider-prefix', 'cargo', 'go',
                 'javac', 'java', 'kotlinc', 'node', 'node-dependencies', 'typescript-cli', 'dotnet', 'swiftc'):
        parser.add_argument('--' + name)
    args = parser.parse_args()
    languages = args.languages.split(',')
    if not languages or len(set(languages)) != len(languages) or any(value not in LANGUAGES for value in languages) or args.jobs not in range(1, 5):
        parser.error('Use unique declared languages and one to four workers')
    compiler = args.compiler.resolve(strict=True)
    directory = args.build.resolve()
    directory.mkdir(parents=True, exist_ok=True)
    manifest = oracle.load_manifest()
    oracle.self_test()
    source_before = source_snapshot()
    compiler_before = sha(compiler)
    fixtures = directory / 'fixtures'
    oracle.materialize(fixtures, manifest)
    results = {}
    options = vars(args).copy()
    started = time.monotonic()
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        jobs = [pool.submit(execute, language, compiler, directory, fixtures, manifest, options) for language in languages]
        for future in as_completed(jobs):
            report = future.result()
            results[report['language']] = report
            print(report['language'], report['status'], report.get('error', ''), flush=True)
    passed = all(report['status'] == 'passed' for report in results.values())
    source_after = source_snapshot()
    changed = [name for name in source_before.keys() | source_after.keys() if source_before.get(name) != source_after.get(name)]
    passed = passed and not changed and compiler_before == sha(compiler)
    full = set(languages) == set(LANGUAGES)
    record = {'status': ('passed' if full else 'diagnostic_passed') if passed else 'failed',
        'full_inventory': full, 'manifest_sha256': sha(ADAPTERS / 'manifest.json'), 'compiler_sha256': sha(compiler),
        'host': platform.platform(), 'invocation': [sys.executable, *sys.argv], 'languages': results, 'elapsed_seconds': round(time.monotonic() - started, 3),
        'qualification_sources_sha256': source_before, 'source_verification': {'changed': changed, 'compiler_unchanged': compiler_before == sha(compiler)}}
    if args.compare_to:
        try:
            record['cross_host'] = compare_hosts(record, json.loads(args.compare_to.read_text(encoding='utf-8')))
        except Exception as failure:
            record['status'] = 'failed'
            record['cross_host_error'] = str(failure)
            passed = False
    (directory / 'verification.json').write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'status': record['status'], 'sdk_count': len(results), 'sha256': sha(directory / 'verification.json')}, sort_keys=True))
    raise SystemExit(0 if passed else 1)


if __name__ == '__main__':
    main()
