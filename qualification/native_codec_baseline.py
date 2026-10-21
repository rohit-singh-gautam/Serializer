# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Measure eleven owning native codecs against generated XML-disabled controls on Windows."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
LANGUAGES = ('cpp', 'python', 'java', 'csharp', 'go', 'javascript', 'typescript',
             'c', 'rust', 'kotlin', 'swift')


def digest(path):
    """Identify exact compiler, input report and runner bytes without normalizing text."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def audit(reports, compiler_hash):
    """Require complete current provenance, five samples and exact cross-language native bytes."""
    languages = {}
    wires = {}
    for group, report in reports.items():
        if report['status'] != 'passed' or report['compiler_sha256'] != compiler_hash:
            raise AssertionError('Missing pass or current compiler identity: ' + group)
        if {item['name'] for item in report['workloads']} != {'small', 'repeated', 'fixed_seed'}:
            raise AssertionError('Incomplete workload inventory: ' + group)
        for workload in report['workloads']:
            if group in ('cpp', 'python'):
                items = [item for item in workload['native_protocols'] if item['protocol'] == 'BINARY_NONE']
                if len(items) != 1:
                    raise AssertionError('Missing native positional baseline: ' + group)
                item = items[0]
                selected = {group: (item['sha256'] if group == 'python' else item['xml_enabled_encode']['sha256'],
                                   item['encoded_bytes'] if group == 'python' else item['xml_enabled_encode']['encoded_bytes'],
                                   {key: item[key] for key in ('xml_enabled_encode', 'xml_enabled_decode',
                                                             'native_only_encode', 'native_only_decode')})}
            else:
                selected = {}
                for language in (('java', 'csharp', 'go', 'javascript', 'typescript') if group == 'portable'
                                 else ('c', 'rust', 'kotlin', 'swift')):
                    operations = workload['operations']
                    baseline = operations[language + '-xml-native-encode']
                    selected[language] = (baseline['output_sha256'], baseline['output_bytes'], {
                        profile + '_' + direction: operations[language + '-' + suffix + '-native-' + direction]
                        for profile, suffix in (('xml_enabled', 'xml'), ('native_only', 'native'))
                        for direction in ('encode', 'decode')})
            for language, (wire_hash, size, operations) in selected.items():
                identity = (wire_hash, size)
                if wires.setdefault(workload['name'], identity) != identity:
                    raise AssertionError('Cross-language native values/bytes differ: ' + language)
                summarized = {}
                for mode, measurement in operations.items():
                    if len(measurement['wall_ns']) != 5 or measurement['median_wall_ns'] <= 0:
                        raise AssertionError('Incomplete or unresolved timing: ' + language + '/' + mode)
                    peak_key = ('tracemalloc_peak_bytes' if language == 'python' else
                                'process_peak_resident_bytes' if language == 'cpp' else
                                'process_peak_working_set_bytes')
                    if measurement[peak_key] <= 0:
                        raise AssertionError('Missing measured memory peak: ' + language + '/' + mode)
                    summarized[mode] = {
                        'median_wall_ns': measurement['median_wall_ns'],
                        'encoded_mib_per_second': size / (1024 * 1024) / (measurement['median_wall_ns'] / 1e9),
                        'peak_bytes': measurement[peak_key], 'peak_kind': peak_key}
                languages.setdefault(language, {})[workload['name']] = {
                    'wire_sha256': wire_hash, 'encoded_bytes': size, 'operations': summarized}
    if set(languages) != set(LANGUAGES):
        raise AssertionError('Missing actual SDKs: ' + str(set(LANGUAGES) - set(languages)))
    return languages


def main():
    """Run maintained measurement drivers sequentially and publish an audited aggregate only on success."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--runtime-library', type=Path, required=True)
    parser.add_argument('--provider-prefix', type=Path, required=True)
    parser.add_argument('--node-dependencies', type=Path, required=True)
    parser.add_argument('--kotlin-home', type=Path, required=True)
    parser.add_argument('--cargo-home', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--audit-only', action='store_true', help='Recheck existing reports without rerunning measurements')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This profile requires Windows process counters and real Windows SDKs')
    build = args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository output must stay beneath ignored out/')
    compiler = args.compiler.resolve(strict=True)
    compiler_hash = digest(compiler)
    build.mkdir(parents=True, exist_ok=True)
    (build / 'verification.json').unlink(missing_ok=True)
    commands = {
        'cpp': ['cpp_xml_benchmark.py', '--runtime-library', args.runtime_library,
                '--provider-prefix', args.provider_prefix],
        'python': ['python_xml_benchmark.py'],
        'portable': ['portable_xml_benchmark.py', '--node-dependencies', args.node_dependencies],
        'mobile': ['native_mobile_xml_benchmark.py', '--libxml2-prefix', args.provider_prefix,
                   '--kotlin-home', args.kotlin_home, '--cargo-home', args.cargo_home]}
    reports = {}
    for name, arguments in commands.items():
        directory = build / name
        if not args.audit_only:
            if directory.exists() and any(directory.iterdir()):
                parser.error('Use an empty measurement directory: ' + str(directory))
            command = [sys.executable, '-B', ROOT / 'qualification' / arguments[0],
                       '--compiler', compiler, '--build', directory, *arguments[1:]]
            print('Measure ' + name + ' native codecs and controls', flush=True)
            with (build / (name + '.log')).open('w', encoding='utf-8') as output:
                result = subprocess.run([str(value) for value in command], stdout=output,
                                        stderr=subprocess.STDOUT, timeout=3600,
                                        creationflags=subprocess.CREATE_NO_WINDOW)
            if result.returncode:
                print((build / (name + '.log')).read_text(encoding='utf-8'), flush=True)
                result.check_returncode()
        reports[name] = json.loads((directory / 'verification.json').read_text(encoding='utf-8'))
        if digest(compiler) != compiler_hash:
            raise AssertionError('Compiler changed during measurement')
    languages = audit(reports, compiler_hash)
    report = {'status': 'passed', 'scope': 'Windows owning native BINARY_NONE controls across eleven actual SDKs',
              'platform': platform.platform(), 'compiler_sha256': compiler_hash,
              'compiler_version': reports['cpp']['compiler_version'], 'runner_sha256': digest(Path(__file__)),
              'report_hashes': {name: digest(build / name / 'verification.json') for name in reports},
              'native_operation_profiles': len(LANGUAGES) * 3 * 4,
              'wall_samples': len(LANGUAGES) * 3 * 4 * 5, 'languages': languages,
              'measurement_limits': [
                  'Controls compare XML-enabled and XML-disabled owning native models using identical values and bytes.',
                  'Process high-water residency includes process startup, runtime, setup and providers.',
                  'Python tracemalloc measures traced Python allocation; native parser/process residency is excluded.',
                  'Peak categories are recorded separately and cannot be compared as interchangeable heap measures.',
                  'No earlier-release speed claim, hard memory cap, streaming or larger-than-RAM guarantee is implied.',
                  'Other protocols, graph shapes, platforms and provider configurations retain their separate qualification gates.']}
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('PASS: 132 native operation profiles, 660 wall samples and eleven-language exact-byte controls', flush=True)


if __name__ == '__main__':
    main()
