# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Measure generated owning C++ XML/native codecs with isolated process residency counters."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import statistics
import subprocess
import sysconfig
import tempfile
import xml.etree.ElementTree as etree

from python_xml_benchmark import ROOT, SOURCE, WORKLOAD_SEED, XML_NAMESPACE, check_xml


def digest(path):
    """Identify an exact retained source, tool, library or encoded artifact."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    """Build both generated profiles, check owning values, then retain every raw measurement."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--runtime-library', type=Path, required=True)
    parser.add_argument('--provider-prefix', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--headers', type=Path, default=ROOT / 'include',
                        help='Public headers matching the supplied immutable runtime library')
    parser.add_argument('--cxx', help='MSVC cl.exe on Windows or a native C++20 compiler on Linux')
    parser.add_argument('--xml-library', type=Path,
                        help='Explicit LibXml2 library; otherwise resolved beneath the provider prefix')
    args = parser.parse_args()
    windows = os.name == 'nt'
    if not windows and platform.system() != 'Linux':
        parser.error('This orchestrator supports native Windows MSVC and Linux C++20 hosts')
    compiler, runtime, provider, build = (path.resolve() for path in
        (args.compiler, args.runtime_library, args.provider_prefix, args.build))
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('In-repository output must remain beneath ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    headers = args.headers.resolve()
    cxx = args.cxx or ('cl.exe' if windows else 'g++')
    if args.xml_library:
        xml_library = args.xml_library.resolve(strict=True)
    elif windows:
        xml_library = (provider / 'lib/libxml2.lib').resolve(strict=True)
    else:
        multiarch = sysconfig.get_config_var('MULTIARCH')
        candidates = [provider / 'lib/libxml2.so', provider / 'lib64/libxml2.so']
        if multiarch:
            candidates.append(provider / 'lib' / multiarch / 'libxml2.so')
        xml_library = next((path.resolve() for path in candidates if path.is_file()), None)
        if xml_library is None:
            parser.error('No LibXml2 shared library under --provider-prefix; supply --xml-library')
    commands = []
    environment = dict(os.environ)
    environment['PATH'] = str(compiler.parent) + os.pathsep + environment.get('PATH', '')

    def run(command, name, timeout=180):
        """Keep exact argv, complete compiler/process output and failures without shell interpolation."""
        command = [str(part) for part in command]
        completed = subprocess.run(command, env=environment, capture_output=True, text=True,
                                   encoding='utf-8', errors='strict', timeout=timeout)
        commands.append({'argv': command, 'returncode': completed.returncode})
        (build / (name + '.log')).write_text(completed.stdout + completed.stderr, encoding='utf-8')
        (build / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n', encoding='utf-8')
        completed.check_returncode()
        return completed.stdout if completed.stdout else completed.stderr

    randomizer = random.Random(WORKLOAD_SEED)
    alphabet = 'abcXYZ012 &<>\t\r\n\U0001f600\u03a9'
    workloads = [('small', ['A', 'B']),
                 ('repeated', ['repeated & <escaped> \U0001f600'] * 20000),
                 ('fixed_seed', [''.join(randomizer.choices(alphabet, k=32)) for _ in range(20000)])]
    driver = ROOT / 'qualification/cpp_xml_benchmark.cpp'
    protocols = ('JSON', 'BINARY_NONE', 'BINARY_INTEGER', 'BINARY_STRING')
    with tempfile.TemporaryDirectory(prefix='cpp-xml-benchmark-', dir=build) as temporary:
        directory = Path(temporary)
        profiles = {}
        for enabled in (True, False):
            name = 'xml_enabled' if enabled else 'native_only'
            output = directory / name
            output.mkdir()
            header = output / 'model.hpp'
            command = [compiler, '--input', SOURCE / 'model.serializer', '--language', 'cpp',
                       '--cpp.output', header, '--cpp.format', 'false',
                       '--xml.enabled', str(enabled).lower()]
            if enabled:
                command += ['--config', SOURCE / 'codegen.ini']
            run(command, name + '-generation')
            executable = output / ('benchmark.exe' if windows else 'benchmark')
            if windows:
                command = [cxx, '/nologo', '/std:c++20', '/EHsc', '/MD', '/O2', '/W4', '/WX',
                           '/utf-8', '/I' + str(headers), '/I' + str(output),
                           '/Fo' + str(output / 'driver.obj'), '/Fe' + str(executable)]
                if enabled:
                    command += ['/I' + str(provider / 'include'),
                                '/I' + str(provider / 'include/libxml2')]
                else:
                    command += ['/DSERIALIZER_XML_BENCHMARK_NATIVE_ONLY']
                command += [driver, '/link', runtime, 'Psapi.lib']
            else:
                command = [cxx, '-std=c++20', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                           '-I' + str(headers), '-I' + str(output)]
                if enabled:
                    command += ['-I' + str(provider / 'include/libxml2')]
                else:
                    command += ['-DSERIALIZER_XML_BENCHMARK_NATIVE_ONLY']
                command += [driver, runtime, '-o', executable]
            if enabled:
                command += [xml_library]
                if not windows:
                    command += ['-Wl,-rpath,' + str(xml_library.parent)]
            run(command, name + '-compile')
            dependency_command = ['dumpbin.exe', '/dependents', executable] if windows else ['ldd', executable]
            dependencies = run(dependency_command, name + '-dependencies')
            if not enabled and any(name in dependencies.lower() for name in ('libxml', 'libxslt')):
                raise AssertionError('Native-only benchmark acquired an XML shared-library dependency')
            profiles[name] = {'executable': executable, 'source_bytes': header.stat().st_size,
                              'source_sha256': digest(header), 'executable_bytes': executable.stat().st_size,
                              'executable_sha256': digest(executable),
                              ('dll_dependencies' if windows else 'shared_library_dependencies'): dependencies}

        def operation(profile, workload, mode, source):
            """Run one codec operation in a fresh process and derive descriptive sample statistics."""
            name = profile + '-' + workload + '-' + mode
            output = build / (name + '.wire')
            result = json.loads(run([profiles[profile]['executable'], mode, source, output], name))
            result['median_wall_ns'] = statistics.median(result['wall_ns'])
            result['median_cpu_ns'] = statistics.median(result['cpu_ns'])
            result['encoded_mib_per_second'] = result['encoded_bytes'] / (1024 * 1024) / (
                result['median_wall_ns'] / 1e9)
            result['sha256'] = digest(output)
            result['output'] = output.name
            return result, output

        results = []
        for name, tags in workloads:
            root = etree.Element('{' + XML_NAMESPACE + '}order', {'id': '7'})
            etree.SubElement(root, '{' + XML_NAMESPACE + '}customer').text = 'R&D\t\r\n\U0001f600'
            etree.SubElement(root, '{' + XML_NAMESPACE + '}count').text = str(len(tags))
            for tag in tags:
                etree.SubElement(root, '{' + XML_NAMESPACE + '}tag').text = tag
            document = etree.tostring(root, encoding='utf-8').replace(b'\r', b'&#13;')
            check_xml(document, tags)
            fixture = build / (name + '-independent.xml')
            fixture.write_bytes(document)
            workload = {'name': name, 'tags': len(tags), 'input_bytes': len(document),
                        'input_sha256': digest(fixture), 'native_protocols': []}
            for mode in ('xml-encode', 'xml-decode'):
                measurement, output = operation('xml_enabled', name, mode, fixture)
                check_xml(output.read_bytes(), tags)
                workload[mode.replace('-', '_')] = measurement
            native_input = None
            for protocol in protocols:
                item = {'protocol': protocol}
                for direction in ('encode', 'decode'):
                    measurement, output = operation('xml_enabled', name, protocol + '-' + direction, fixture)
                    item['xml_enabled_' + direction] = measurement
                    if direction == 'encode':
                        native_wire = output.read_bytes()
                        if protocol == 'BINARY_NONE':
                            native_input = output
                    elif output.read_bytes() != native_wire:
                        raise AssertionError('Native encode/decode fixture differs')
                workload['native_protocols'].append(item)
            if native_input is None:
                raise AssertionError('Missing ordinary binary fixture for native-only model')
            for item in workload['native_protocols']:
                for direction in ('encode', 'decode'):
                    measurement, _ = operation('native_only', name, item['protocol'] + '-' + direction,
                                               native_input)
                    if measurement['sha256'] != item['xml_enabled_' + direction]['sha256']:
                        raise AssertionError('XML-enabled/native-only wire bytes differ')
                    item['native_only_' + direction] = measurement
            results.append(workload)
        if windows:
            run([cxx, '/?'], 'msvc-version')
            toolchain_version = next(line.strip() for line in
                (build / 'msvc-version.log').read_text(encoding='utf-8').splitlines()
                if 'Compiler Version' in line)
        else:
            toolchain_version = run([cxx, '--version'], 'cxx-version').splitlines()[0]
        report = {'status': 'passed', 'scope': 'Selected ' + ('Windows MSVC' if windows else 'native Linux') +
                  ' owning C++ XML/native measurements',
                  'compiler_sha256': digest(compiler), 'runtime_library_sha256': digest(runtime),
                  'compiler_version': run([compiler, '--version'], 'compiler-version').strip(),
                  ('msvc_version' if windows else 'cxx_version'): toolchain_version,
                  'platform': platform.platform(), 'driver_sha256': digest(driver),
                  'runner_sha256': digest(Path(__file__)), 'workload_seed': WORKLOAD_SEED,
                  'public_headers': {'include/' + path.relative_to(headers).as_posix(): digest(path)
                                     for path in sorted((headers / 'rohit').rglob('*.hpp'))},
                  'provider_library': str(xml_library), 'provider_library_sha256': digest(xml_library),
                  'warmup_iterations': 1, 'measured_iterations': 5,
                  'profiles': {name: {key: value for key, value in profile.items() if key != 'executable'}
                               for name, profile in profiles.items()},
                  'measurement_limits': [
                      'Timings exclude source generation, compilation, setup, disk I/O and result destruction.',
                      ('CPU counters use GetProcessTimes, whose small-operation samples may be zero.' if windows else
                       'CPU counters use the native C process clock, whose small-operation samples may be zero.'),
                      ('PeakWorkingSetSize is absolute process residency, including startup, setup and native libraries.'
                       if windows else 'Linux /proc/self/status VmHWM is approximate process residency for this executable image.'),
                      'Prepared and final high-water counters are not exact per-operation allocation or heap peaks.',
                      'Each operation runs in a fresh process, with one warmup and five measured calls.',
                      'Both profiles use the same model values and exact native bytes; XML builds link libxml2.',
                      'Owning complete-message workloads establish no streaming or larger-than-RAM behavior.',
                      'Concurrent machine work affects timings; no performance pass threshold is imposed.'],
                  'workloads': results}
        (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('C++ XML measurements passed: 3 workloads, 54 isolated operations, 270 measured calls')


if __name__ == '__main__':
    main()
