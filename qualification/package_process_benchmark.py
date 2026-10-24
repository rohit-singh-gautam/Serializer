# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Measure fresh-process ZIP/OPC producers from a complete independently checked SDK inventory."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import time

from package_reproducibility import LANGUAGES, ROOT, oracle
from portable_xml_benchmark import process_counters

SAMPLE_COUNT = 5
CODEC_FILENAMES = {
    'cpp': {'package_relocation.hpp'}, 'python': {'schema.py'}, 'c': {'schema.h'},
    'rust': {'schema.rs'}, 'go': {'schema.go'}, 'java': {'Generated.java'},
    'javascript': {'schema.mjs'}, 'typescript': {'schema.mjs', 'schema.d.mts'},
    'csharp': {'Schema.cs'}, 'kotlin': {'Schema.kt'}, 'swift': {'Schema.swift'},
}


def digest(path):
    """Identify exact baseline reports, sources, consumers and package bytes."""
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def require(condition, message):
    """Keep complete-inventory and correctness gates active under optimized Python."""
    if not condition:
        raise ValueError(message)


def main():
    """Measure all eleven real SDKs serially and independently inspect every timed archive afterward."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True,
                        help='Passed complete package_reproducibility/verification.json')
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This profile requires native Windows process CPU and residency counters')
    directory = args.build.resolve()
    if directory == ROOT or ROOT in directory.parents and 'out' not in directory.relative_to(ROOT).parts:
        parser.error('Repository output must remain beneath ignored out/')
    if directory.exists() and any(directory.iterdir()):
        parser.error('Use an empty output directory to preserve earlier measurements')
    baseline = args.baseline.resolve(strict=True)
    baseline_hash = digest(baseline)
    previous = json.loads(baseline.read_text(encoding='utf-8'))
    compiler = args.compiler.resolve(strict=True)
    compiler_hash = digest(compiler)
    manifest = oracle.load_manifest()
    manifest_path = ROOT/'qualification/package_reproducibility/manifest.json'
    require(previous['status'] == 'passed' and previous['full_inventory'], 'A complete passing baseline is required')
    require(set(previous['languages']) == set(LANGUAGES), 'Missing actual SDK baseline')
    require(previous['compiler_sha256'] == compiler_hash, 'Compiler differs from the checked baseline')
    require(previous['manifest_sha256'] == digest(manifest_path), 'Fixture/policy manifest changed')
    directory.mkdir(parents=True, exist_ok=True)
    report = {'status': 'failed', 'host': platform.platform(), 'baseline': str(baseline),
              'baseline_sha256': baseline_hash, 'compiler_sha256': compiler_hash,
              'runner_sha256': digest(__file__), 'manifest_sha256': digest(manifest_path),
              'samples_per_profile': SAMPLE_COUNT, 'maximum_workers': 1, 'languages': {}, 'commands': [],
              'scope': 'Fresh-process producer wall/CPU/peak working set, including SDK startup, fixture parsing, '
                       'owning package Save, producer validation and file I/O. Independent archive inspection is '
                       'outside each timing. This is a startup-inclusive baseline, not isolated Save throughput, '
                       'an allocation count, a provider hard memory bound or an isolated regression threshold.'}
    started = time.monotonic()
    try:
        for language in LANGUAGES:
            prior = previous['languages'][language]
            require(prior['status'] == 'passed', 'Missing passing producer: '+language)
            artifacts = dict(prior['generated_sources_sha256'])
            artifacts.update(prior['consumer_images_sha256'])
            artifacts.update(prior['provider'].get('artifacts', {}))
            require(all(digest(path) == value for path, value in artifacts.items()), 'Baseline producer changed: '+language)
            # Native adapter inventories also contain consumer templates and build manifests.
            # Count only actual compiler output here, including TypeScript's shared JS runtime.
            codec_sources = {path: value for path, value in prior['generated_sources_sha256'].items()
                             if Path(path).name in CODEC_FILENAMES[language]}
            require({Path(path).name for path in codec_sources} == CODEC_FILENAMES[language],
                    'Incomplete generated codec source inventory: '+language)
            generated_bytes = sum(Path(path).stat().st_size for path in codec_sources)
            output = directory/language
            output.mkdir()
            rows = []
            for profile in manifest['profiles']:
                # The full reproducibility corpus separately qualifies forced ZIP64 and all permutations.
                observation = next(row for row in prior['observations'] if row['profile'] == profile
                                   and not row['force_zip64'] and row['permutation'] == row['repetition'] == 0)
                old_path = observation['path']
                command = next(row for row in prior['commands'] if old_path in row['argv'])
                samples = []
                for index in range(SAMPLE_COUNT):
                    path = output/f'{profile}-{index}{Path(old_path).suffix}'
                    argv = [str(path) if value == old_path else value for value in command['argv']]
                    beginning = time.perf_counter_ns()
                    with subprocess.Popen(argv, cwd=command['cwd'], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                          creationflags=subprocess.CREATE_NO_WINDOW) as process:
                        try:
                            stdout, stderr = process.communicate(timeout=120)
                        except subprocess.TimeoutExpired:
                            process.kill()
                            process.communicate()
                            raise
                        wall_ns = time.perf_counter_ns()-beginning
                        counters = process_counters(process)
                        returncode = process.returncode
                    log = output/f'{profile}-{index}.log'
                    log.write_bytes(stdout+stderr)
                    report['commands'].append({'argv': argv, 'cwd': command['cwd'], 'returncode': returncode,
                                               'log': str(log), 'log_sha256': digest(log)})
                    require(returncode == 0, 'Timed producer failed: '+str(log))
                    protocol = [json.loads(line) for line in stdout.decode('utf-8').splitlines() if line.startswith('{')][-1]
                    require(protocol == observation['protocol'], 'Producer values/provider policy changed')
                    inspection = oracle.inspect_archive(path, language, profile, False, manifest)
                    require(inspection['archive_sha256'] == observation['inspection']['archive_sha256'],
                            'Timed archive differs from independently checked canonical bytes')
                    require(wall_ns > 0 and counters['process_peak_working_set_bytes'] > 0, 'Unresolved process counters')
                    samples.append(dict(wall_ns=wall_ns, **counters))
                median_ns = statistics.median(sample['wall_ns'] for sample in samples)
                encoded_bytes = path.stat().st_size
                rows.append({'profile': profile, 'samples': samples, 'median_wall_ns': median_ns,
                             'packages_per_second': 1e9/median_ns,
                             'encoded_mib_per_second': encoded_bytes/(1024*1024)/(median_ns/1e9),
                             'encoded_bytes': encoded_bytes, 'archive_sha256': digest(path),
                             'peak_working_set_bytes': max(sample['process_peak_working_set_bytes'] for sample in samples)})
            require(all(digest(path) == value for path, value in artifacts.items()), 'Producer drift during measurement')
            report['languages'][language] = {'tool': prior['tool'], 'provider': prior['provider'],
                                             'generated_source_bytes': generated_bytes,
                                             'generated_sources_sha256': codec_sources, 'profiles': rows}
            print('PASS', language, len(rows)*SAMPLE_COUNT, 'independently checked timed archives', flush=True)
        require(digest(baseline) == baseline_hash and digest(compiler) == compiler_hash, 'Baseline/compiler drift')
        require(set(report['languages']) == set(LANGUAGES), 'Incomplete SDK measurement inventory')
        report['status'] = 'passed'
        report['timed_archives'] = len(LANGUAGES)*len(manifest['profiles'])*SAMPLE_COUNT
    except BaseException as failure:
        report['failure'] = repr(failure)
        raise
    finally:
        report['elapsed_seconds'] = round(time.monotonic()-started, 3)
        (directory/'verification.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    main()
