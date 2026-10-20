# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Measure real Java/C#/Go/JS/TS owning XML codecs and XML-disabled native controls."""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import shutil
import statistics
import subprocess
import sys
import xml.etree.ElementTree as etree

from python_xml_benchmark import ROOT, SOURCE, WORKLOAD_SEED, XML_NAMESPACE, check_xml

LANGUAGES = ('java', 'csharp', 'go', 'javascript', 'typescript')
DRIVERS = ROOT / 'qualification/xml_performance'
OUTPUTS = {'java': 'Schema.java', 'csharp': 'Schema.cs', 'go': 'schema.go',
           'javascript': 'schema.mjs', 'typescript': 'schema.d.mts'}


def digest(path):
    """Identify exact generated sources, fixtures and tooling in the retained report."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def process_counters(process):
    """Read the exited Windows child's absolute residency high-water and total CPU time."""
    class memory_counters(ctypes.Structure):
        """Match the Windows PROCESS_MEMORY_COUNTERS wire layout on the current host."""
        _fields_ = [('cb', wintypes.DWORD), ('PageFaultCount', wintypes.DWORD),
                    ('PeakWorkingSetSize', ctypes.c_size_t), ('WorkingSetSize', ctypes.c_size_t),
                    ('QuotaPeakPagedPoolUsage', ctypes.c_size_t), ('QuotaPagedPoolUsage', ctypes.c_size_t),
                    ('QuotaPeakNonPagedPoolUsage', ctypes.c_size_t), ('QuotaNonPagedPoolUsage', ctypes.c_size_t),
                    ('PagefileUsage', ctypes.c_size_t), ('PeakPagefileUsage', ctypes.c_size_t)]

    psapi = ctypes.WinDLL('psapi', use_last_error=True)
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    psapi.GetProcessMemoryInfo.argtypes = (wintypes.HANDLE, ctypes.POINTER(memory_counters), wintypes.DWORD)
    psapi.GetProcessMemoryInfo.restype = wintypes.BOOL
    kernel.GetProcessTimes.argtypes = (wintypes.HANDLE, *[ctypes.POINTER(wintypes.FILETIME)] * 4)
    kernel.GetProcessTimes.restype = wintypes.BOOL
    counters = memory_counters()
    counters.cb = ctypes.sizeof(counters)
    if not psapi.GetProcessMemoryInfo(int(process._handle), ctypes.byref(counters), counters.cb):
        raise ctypes.WinError(ctypes.get_last_error())
    times = [wintypes.FILETIME() for _ in range(4)]
    if not kernel.GetProcessTimes(int(process._handle), *[ctypes.byref(value) for value in times]):
        raise ctypes.WinError(ctypes.get_last_error())
    ticks = sum((value.dwHighDateTime << 32) | value.dwLowDateTime for value in times[2:])
    return {'process_peak_working_set_bytes': counters.PeakWorkingSetSize,
            'process_cpu_ns': ticks * 100}


def driver(language, enabled):
    """Specialize the same maintained driver for XML-enabled and native-only public APIs."""
    filename = {'java': 'Main.java', 'csharp': 'Program.cs', 'go': 'main.go',
                'javascript': 'main.mjs', 'typescript': 'main.mjs'}[language]
    source = (DRIVERS / filename).read_text(encoding='utf-8')
    native = {
        'java': ('var value = Schema.XmlExample.Order.decode(input, Schema.Protocol.BINARY_NONE);',
                 'result = Schema.XmlExample.Order.decode(encoded, Schema.Protocol.BINARY_NONE);'),
        'csharp': ('var value = XmlExampleOrder.Decode(input, Protocol.BINARY_NONE);',
                   'result = XmlExampleOrder.Decode(encoded, Protocol.BINARY_NONE);'),
        'go': ('value := checked(DecodeXmlExampleOrder(input, BINARY_NONE))',
               'result = checked(DecodeXmlExampleOrder(encoded, BINARY_NONE))'),
        'javascript': ('const value = XmlExampleOrder.decode(input, Protocol.BINARY_NONE);',
                       'if (!(retained instanceof Uint8Array)) throw new Error("Unexpected result");\n'
                       'result = XmlExampleOrder.decode(retained, Protocol.BINARY_NONE);'),
    }
    xml = {
        'java': ('var value = Schema.XmlExample.Order.decodeXml(input);',
                 'case "xml-encode" -> () -> value.encodeXml();\n'
                 'case "xml-decode" -> () -> Schema.XmlExample.Order.decodeXml(input);',
                 'result = mode.startsWith("xml") ? Schema.XmlExample.Order.decodeXml(encoded) : '
                 'Schema.XmlExample.Order.decode(encoded, Schema.Protocol.BINARY_NONE);',
                 'Files.write(Path.of(args[2] + ".xml"), result.encodeXml());'),
        'csharp': ('var value = XmlExampleOrder.DecodeXml(input);',
                   '"xml-encode" => () => value.EncodeXml(),\n'
                   '"xml-decode" => () => XmlExampleOrder.DecodeXml(input),',
                   'result = mode.StartsWith("xml", StringComparison.Ordinal) ? XmlExampleOrder.DecodeXml(encoded) : '
                   'XmlExampleOrder.Decode(encoded, Protocol.BINARY_NONE);',
                   'File.WriteAllBytes(args[2] + ".xml", result.EncodeXml());'),
        'go': ('value := checked(DecodeXmlExampleOrderXML(input))',
               'case "xml-encode": operation = func() any { return checked(value.EncodeXML()) }\n'
               'case "xml-decode": operation = func() any { return checked(DecodeXmlExampleOrderXML(input)) }',
               'if mode == "xml-encode" { result = checked(DecodeXmlExampleOrderXML(encoded)) } '
               'else { result = checked(DecodeXmlExampleOrder(encoded, BINARY_NONE)) }',
               'if err := os.WriteFile(os.Args[3]+".xml", checked(result.EncodeXML()), 0600); err != nil { panic(err) }'),
        'javascript': ('const text = new TextDecoder().decode(input);\nconst value = XmlExampleOrder.decodeXml(text);',
                       "case 'xml-encode': retained = value.encodeXml(); break;\n"
                       "case 'xml-decode': retained = XmlExampleOrder.decodeXml(text); break;",
                       'result = typeof retained === "string" ? XmlExampleOrder.decodeXml(retained) : '
                       'XmlExampleOrder.decode(retained, Protocol.BINARY_NONE);',
                       'writeFileSync(process.argv[4] + ".xml", result.encodeXml());'),
    }
    key = 'javascript' if language == 'typescript' else language
    setup, result = native[key]
    operations, output = '', ''
    if enabled:
        setup, operations, result, output = xml[key]
    for marker, replacement in (('SETUP', setup), ('RESULT', result),
                                ('XML_OPERATIONS', operations), ('XML_OUTPUT', output)):
        source = source.replace('// BENCHMARK_' + marker, replacement)
    return source


def main():
    """Build selected real SDKs, require independent correctness, and retain raw samples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default=','.join(LANGUAGES))
    parser.add_argument('--node-dependencies', type=Path, required=True,
                        help='Installed dependencies from the maintained XML TypeScript lockfile')
    parser.add_argument('--tags', type=int, default=20000)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This measurement profile requires native Windows process counters; other platforms remain pending')
    languages = args.language.split(',')
    if len(set(languages)) != len(languages) or any(value not in LANGUAGES for value in languages):
        parser.error('Choose distinct supported language names')
    if not 1 <= args.tags <= 20000:
        parser.error('--tags must be between 1 and 20000 for this owning workload')
    build, compiler = args.build.resolve(), args.compiler.resolve(strict=True)
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('In-repository output must remain beneath ignored out/')
    if build.exists() and any(build.iterdir()):
        parser.error('Use an empty --build directory to preserve earlier artifacts and exclude stale sources')
    build.mkdir(parents=True, exist_ok=True)
    report_path = build / 'verification.json'
    compiler_hash = digest(compiler)
    commands = []

    def run(command, name, measured=False):
        """Capture exact argv/output and keep each measured child isolated without a shell."""
        argv = [str(value) for value in command]
        with subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              creationflags=subprocess.CREATE_NO_WINDOW) as process:
            try:
                stdout, stderr = process.communicate(timeout=600)
            except subprocess.TimeoutExpired:
                process.kill()
                process.communicate()
                raise
            counters = process_counters(process) if measured else {}
            result = subprocess.CompletedProcess(argv, process.returncode, stdout, stderr)
        (build / (name + '.log')).write_bytes(stdout + stderr)
        commands.append({'argv': argv, 'returncode': result.returncode})
        (build / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n', encoding='utf-8')
        result.check_returncode()
        return stdout.decode('utf-8') or stderr.decode('utf-8'), counters

    dependencies = args.node_dependencies.resolve(strict=True)
    versions = {}
    for package, version in (('saxes', '6.0.0'), ('xmlchars', '2.2.0'),
                             ('@xmldom/xmldom', '0.9.12'), ('typescript', '5.9.3')):
        if json.loads((dependencies / package / 'package.json').read_text(encoding='utf-8'))['version'] != version:
            raise AssertionError('Dependency version differs from maintained exact lock: ' + package)
        versions[package] = version
    # Node resolves imports from these sibling build directories without copying caches.
    if {'javascript', 'typescript'} & set(languages):
        target = build / 'node_modules'
        if not target.exists():
            shutil.copytree(dependencies, target)
    tools = {}
    java_executable, javac_executable = None, None
    if 'java' in languages:
        settings = run(['java', '-XshowSettings:properties', '-version'], 'java-home')[0]
        java_home = next((line.split('=', 1)[1].strip() for line in settings.splitlines()
                          if line.strip().startswith('java.home =')), None)
        if java_home is None:
            raise AssertionError('The selected Java runtime did not report java.home')
        # Oracle's Windows javapath executable launches another process. Counters
        # must belong to the JVM itself, so bypass that shim using its own home.
        java_executable = (Path(java_home) / 'bin/java.exe').resolve(strict=True)
        javac_executable = (Path(java_home) / 'bin/javac.exe').resolve(strict=True)
    for language, command in [('java', ['java', '-version']), ('csharp', ['dotnet', '--info']),
                               ('go', ['go', 'version']), ('javascript', ['node', '--version'])]:
        if language in languages or language == 'javascript' and 'typescript' in languages:
            tools[language] = run(command, language + '-version')[0].strip()
    if javac_executable:
        tools['javac'] = run([javac_executable, '--version'], 'javac-version')[0].strip()
    compiler_version = run([compiler, '--version'], 'compiler-version')[0].strip()
    profiles = {}
    for language in languages:
        for enabled in (True, False):
            name = language + ('-xml' if enabled else '-native')
            directory = build / name
            directory.mkdir(exist_ok=True)
            output = directory / OUTPUTS[language]
            generator = 'js' if language == 'javascript' else language
            command = [compiler, '--input', SOURCE / 'model.serializer', '--language',
                       'js,typescript' if language == 'typescript' else generator,
                       '--' + generator + '.output', output, '--xml.enabled', str(enabled).lower(),
                       '--protocols', 'binary_none', '--go.package', 'main']
            if language == 'typescript':
                command += ['--js.output', directory / 'schema.mjs']
            if enabled:
                command += ['--config', SOURCE / 'codegen.ini']
            run(command, name + '-generation')
            generated = [path for path in directory.iterdir() if path.suffix in ('.java', '.cs', '.go', '.mjs', '.mts')]
            source_sizes = [{'name': path.name, 'bytes': path.stat().st_size, 'sha256': digest(path)} for path in sorted(generated)]
            filename = {'java': 'Main.java', 'csharp': 'Program.cs', 'go': 'main.go',
                        'javascript': 'main.mjs', 'typescript': 'main.mts'}[language]
            (directory / filename).write_text(driver(language, enabled), encoding='utf-8')
            if language == 'java':
                run([javac_executable, '--release', '17', '-encoding', 'UTF-8', '-Xlint:all', '-Werror',
                     '-d', directory / 'classes', output, directory / filename], name + '-compile')
                # Newer JAXP defaults cap aggregate character-reference expansion at
                # 100,000 characters. Keep the larger fixture admitted under an
                # explicit finite provider bound rather than disabling admission.
                launch = [java_executable, '-Djdk.xml.maxGeneralEntitySizeLimit=4194304',
                          '-Djdk.xml.totalEntitySizeLimit=8388608',
                          '-cp', directory / 'classes', 'Main']
            elif language == 'csharp':
                project = directory / 'Benchmark.csproj'
                project.write_text('<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><OutputType>Exe</OutputType>'
                                   '<TargetFramework>net8.0</TargetFramework><Nullable>enable</Nullable>'
                                   '<TreatWarningsAsErrors>true</TreatWarningsAsErrors></PropertyGroup></Project>', encoding='utf-8')
                run(['dotnet', 'build', project, '-c', 'Release', '--nologo', '-v:q'], name + '-compile')
                launch = ['dotnet', directory / 'bin/Release/net8.0/Benchmark.dll']
            elif language == 'go':
                (directory / 'go.mod').write_text('module serializer.benchmark\n\ngo 1.22\n', encoding='utf-8')
                run(['go', '-C', directory, 'build', '-trimpath', '-o', directory / 'benchmark.exe', '.'], name + '-compile')
                launch = [directory / 'benchmark.exe']
            else:
                if language == 'typescript':
                    shutil.copyfile(DRIVERS / 'node.d.mts', directory / 'node.d.mts')
                    run(['node', dependencies / 'typescript/bin/tsc', '--strict', '--target', 'ES2020',
                         '--module', 'NodeNext', '--moduleResolution', 'NodeNext', directory / filename,
                         directory / 'node.d.mts'], name + '-compile')
                launch = ['node', directory / 'main.mjs']
            profiles[name] = {'generated_sources': source_sizes, 'launch': launch}

    randomizer = random.Random(WORKLOAD_SEED)
    alphabet = 'abcXYZ012 &<>\t\r\n\U0001f600\u03a9'
    workloads = [('small', ['A', 'B']), ('repeated', ['repeated & <escaped> \U0001f600'] * args.tags),
                 ('fixed_seed', [''.join(randomizer.choices(alphabet, k=32)) for _ in range(args.tags)])]
    results = []
    for name, tags in workloads:
        root = etree.Element('{' + XML_NAMESPACE + '}order', {'id': '7'})
        etree.SubElement(root, '{' + XML_NAMESPACE + '}customer').text = 'R&D\t\r\n\U0001f600'
        etree.SubElement(root, '{' + XML_NAMESPACE + '}count').text = str(len(tags))
        for tag in tags:
            etree.SubElement(root, '{' + XML_NAMESPACE + '}tag').text = tag
        document = etree.tostring(root, encoding='utf-8').replace(b'\r', b'&#13;')
        check_xml(document, tags)
        fixture = build / (name + '.xml')
        fixture.write_bytes(document)
        workload = {'name': name, 'tags': len(tags), 'input_bytes': len(document),
                    'input_sha256': digest(fixture), 'operations': {}}
        canonical = None
        for language in languages:
            wire = None
            for enabled in (True, False):
                profile = language + ('-xml' if enabled else '-native')
                modes = ['xml-encode', 'xml-decode', 'native-encode', 'native-decode'] if enabled else ['native-encode', 'native-decode']
                for mode in modes:
                    label = name + '-' + profile + '-' + mode
                    output = build / (label + '.wire')
                    repetitions = 100 if name == 'small' else 1
                    for calibration in range(10):
                        text, counters = run([*profiles[profile]['launch'], fixture if enabled else wire,
                                              mode, output, repetitions], label + '-' + str(calibration), measured=True)
                        measurement = json.loads(text)
                        batch_ns = statistics.median(measurement['wall_ns'])
                        if batch_ns >= 10_000_000:
                            break
                        repetitions *= max(2, min(100, int(10_000_000 / max(batch_ns, 1)) + 1))
                    else:
                        raise AssertionError('Could not obtain a resolved timing batch: ' + label)
                    measurement.update(counters)
                    measurement['median_wall_ns'] = statistics.median(measurement['wall_ns']) / measurement['repetitions']
                    encoded_bytes = (Path(str(output) + '.xml').stat().st_size if mode == 'xml-encode'
                                     else len(document) if mode == 'xml-decode' else output.stat().st_size)
                    measurement['encoded_bytes'] = encoded_bytes
                    measurement['encoded_mib_per_second'] = encoded_bytes / (1024 * 1024) / (measurement['median_wall_ns'] / 1e9)
                    measurement['output_bytes'] = output.stat().st_size
                    measurement['output_sha256'] = digest(output)
                    if canonical is None:
                        canonical = output.read_bytes()
                    elif output.read_bytes() != canonical:
                        raise AssertionError('Measured native values differ across operations/profiles/SDKs: ' + label)
                    if enabled:
                        check_xml(Path(str(output) + '.xml').read_bytes(), tags)
                        measurement['xml_bytes'] = Path(str(output) + '.xml').stat().st_size
                        measurement['xml_sha256'] = digest(Path(str(output) + '.xml'))
                        wire = output
                    workload['operations'][profile + '-' + mode] = measurement
        results.append(workload)
        print('PASS: portable XML measurements ' + name + ' across ' + str(len(languages)) + ' SDKs', flush=True)
    if digest(compiler) != compiler_hash:
        raise AssertionError('Compiler changed during benchmark; use an immutable compiler snapshot')
    report = {'status': 'passed', 'scope': 'Selected native Windows owning XML/native profiles',
              'compiler_sha256': compiler_hash, 'compiler_version': compiler_version,
              'platform': platform.platform(), 'python': sys.version, 'tools': tools,
              'java_process_image': ({'path': str(java_executable), 'sha256': digest(java_executable),
                                      'compiler_path': str(javac_executable), 'compiler_sha256': digest(javac_executable)}
                                     if java_executable else None),
              'locked_node_dependencies': versions, 'workload_seed': WORKLOAD_SEED,
              'driver_hashes': {path.name: digest(path) for path in sorted(DRIVERS.iterdir()) if path.is_file()},
              'runner_sha256': digest(Path(__file__)), 'warmup_batches': 3, 'measured_batches': 5,
              'profiles': {name: {key: value for key, value in profile.items() if key != 'launch'} for name, profile in profiles.items()},
              'measurement_limits': [
                  'Each operation uses a fresh process; five wall samples exclude setup, result validation and disk I/O.',
                  'Batches start at 100 calls for small messages or one for larger ones; fresh-process calibration increases them until the median batch reaches 10 ms.',
                  'Process CPU and peak working set include runtime startup, warmups, setup, validation and output.',
                  'Absolute process high-water residency is not exact codec allocation; no before/after subtraction is made.',
                  'Java/C# allocation deltas cover one current-thread call; Go TotalAlloc is process-wide; native provider allocations may be excluded.',
                  'JS/TS have no exact allocation counter in this profile; both use the same generated JavaScript runtime.',
                  'XML-disabled native BINARY_NONE controls use identical values and bytes; native bytes are a different format, not equivalent XML provider work.',
                  'Default generated resource limits are active; owning complete-message measurements do not qualify streaming or hard memory bounds.',
                  'Java sets jdk.xml.maxGeneralEntitySizeLimit=4194304 and jdk.xml.totalEntitySizeLimit=8388608 explicitly for escaped-text fixtures; the JDK 26 defaults reject the larger fixture before typed decoding.',
                  'Concurrent machine work affects timings; there is no speed threshold or earlier-compiler performance regression baseline.',
                  'Other platforms/providers, additional vocabularies and large-input behavior remain pending.'],
              'workloads': results}
    report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('PASS: retained ' + str(len(languages) * 18) + ' isolated operation measurements in ' + str(report_path))


if __name__ == '__main__':
    main()
