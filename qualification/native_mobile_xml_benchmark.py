# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Measure actual C/Rust/Kotlin/Swift owning XML codecs and XML-disabled native controls on Windows."""
import argparse
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

from portable_xml_benchmark import digest, process_counters
from python_xml_benchmark import ROOT, SOURCE, WORKLOAD_SEED, XML_NAMESPACE, check_xml
from python_xml_benchmark import MAX_WORKLOAD_TAGS, suppress_windows_error_dialogs

LANGUAGES = ('c', 'rust', 'kotlin', 'swift')
DRIVERS = ROOT / 'qualification/xml_performance'
OUTPUTS = {'c': 'schema.h', 'rust': 'schema.rs', 'kotlin': 'Schema.kt', 'swift': 'Schema.swift'}
DRIVER_NAMES = {'c': 'main.c', 'rust': 'main.rs', 'kotlin': 'Main.kt', 'swift': 'main.swift'}


def driver(language, enabled, maximum_xml_entries=100000, maximum_xml_text_bytes=8 * 1024 * 1024):
    """Select public XML or native-only operations without changing the maintained measurement loop."""
    native_decode = 'require(xml_example_order_decode(&checked, retained.bytes.data, retained.bytes.size, srl_binary_none, NULL) == srl_ok, "Result native decode failed");'
    setup = {
        'c': 'require(xml_example_order_decode(&value, input.data, input.size, srl_binary_none, NULL) == srl_ok, "Native setup failed");',
        'rust': 'let value = XmlExampleOrder::decode(&input, Protocol::BinaryNone)?;',
        'kotlin': 'val value = XmlExampleOrder.decode(input, Protocol.BINARY_NONE)',
        'swift': 'let value = try XmlExampleOrder.decode(input, .BINARY_NONE)',
    }
    result = {'c': native_decode, 'rust': 'XmlExampleOrder::decode(&encoded, Protocol::BinaryNone)?',
              'kotlin': 'XmlExampleOrder.decode(selected, Protocol.BINARY_NONE)',
              'swift': 'result = try XmlExampleOrder.decode(encoded, .BINARY_NONE)'}
    operations, output = '', ''
    if enabled:
        setup = {
            'c': 'require(xml_example_order_decode_xml(&value, input.data, input.size, NULL) == srl_ok, "XML setup failed");',
            'rust': 'let value = XmlExampleOrder::decode_xml(std::str::from_utf8(&input)?)?;',
            'kotlin': 'val value = XmlExampleOrder.decodeXml(input)',
            'swift': 'let value = try XmlExampleOrder.decodeXml(input)',
        }
        operations = {
            'c': 'else if (strcmp(mode, "xml-encode") == 0) {\n'
                 '    require(xml_example_order_encode_xml(value, &result->bytes, NULL) == srl_ok, "XML encode failed");\n'
                 '  } else if (strcmp(mode, "xml-decode") == 0) {\n'
                 '    require(xml_example_order_decode_xml(&result->model, input->data, input->size, NULL) == srl_ok, "XML decode failed"); result->owning = true;\n  }',
            'rust': '"xml-encode" => Retained::Bytes(value.encode_xml().expect("XML encode").into_bytes()),\n'
                    '        "xml-decode" => Retained::Model(XmlExampleOrder::decode_xml(std::str::from_utf8(_input).expect("UTF8 input")).expect("XML decode")),',
            'kotlin': '"xml-encode" -> { { value.encodeXml() } }\n'
                      '        "xml-decode" -> { { XmlExampleOrder.decodeXml(input) } }',
            'swift': 'case "xml-encode": operation = { .bytes(try value.encodeXml()) }\n'
                     'case "xml-decode": operation = { .model(try XmlExampleOrder.decodeXml(input)) }',
        }[language]
        result = {
            'c': 'if (strcmp(mode, "xml-encode") == 0) { require(xml_example_order_decode_xml(&checked, retained.bytes.data, retained.bytes.size, NULL) == srl_ok, "Result XML decode failed"); } else { ' + native_decode + ' }',
            'rust': 'if mode == "xml-encode" { XmlExampleOrder::decode_xml(std::str::from_utf8(&encoded)?)? } else { XmlExampleOrder::decode(&encoded, Protocol::BinaryNone)? }',
            'kotlin': 'if (mode == "xml-encode") XmlExampleOrder.decodeXml(selected) else XmlExampleOrder.decode(selected, Protocol.BINARY_NONE)',
            'swift': 'if mode == "xml-encode" { result = try XmlExampleOrder.decodeXml(encoded) } else { result = try XmlExampleOrder.decode(encoded, .BINARY_NONE) }',
        }
        output = {
            'c': 'srl_buffer xml = {0}; require(xml_example_order_encode_xml(result, &xml, NULL) == srl_ok, "Final XML encode failed");\n'
                 '  const size_t path_size = strlen(argv[3]) + sizeof(".xml"); char* xml_path = malloc(path_size); require(xml_path != NULL, "Output path allocation");\n'
                 '  require(snprintf(xml_path, path_size, "%s.xml", argv[3]) > 0, "Output path formatting"); write_output(xml_path, &xml); free(xml_path); srl_buffer_free(&xml);',
            'rust': 'fs::write(format!("{}.xml", args[3]), result.encode_xml()?)?;',
            'kotlin': 'File(arguments[2] + ".xml").writeBytes(result.encodeXml())',
            'swift': 'try Data(result.encodeXml()).write(to: URL(fileURLWithPath: arguments[3] + ".xml"))',
        }[language]
    source = (DRIVERS / DRIVER_NAMES[language]).read_text(encoding='utf-8')
    for marker, replacement in (('SETUP', setup[language]), ('RESULT', result[language]),
                                ('XML_OPERATIONS', operations), ('XML_OUTPUT', output)):
        if source.count('// BENCHMARK_' + marker) != 1:
            raise AssertionError('Expected exactly one maintained driver marker: ' + marker)
        source = source.replace('// BENCHMARK_' + marker, replacement)
    if enabled and language == 'kotlin':
        source = source.replace('val value = XmlExampleOrder.decodeXml(input)',
            f'val xmlLimits = XmlLimits(maxBytes=16777216,maxNodes={maximum_xml_entries},maxTextBytes={maximum_xml_text_bytes})\n'
            '    val value = XmlExampleOrder.decodeXml(input,xmlLimits)')
        source = source.replace('.encodeXml()', '.encodeXml(xmlLimits)')
        for argument in ('input', 'selected'):
            source = source.replace('XmlExampleOrder.decodeXml(' + argument + ')',
                                    'XmlExampleOrder.decodeXml(' + argument + ',xmlLimits)')
    if enabled and language == 'swift':
        source = source.replace('let value = try XmlExampleOrder.decodeXml(input)',
            f'let xmlLimits: XmlLimits = {{ var limits=XmlLimits();limits.maxNodes={maximum_xml_entries};'
            f'limits.maxTextBytes={maximum_xml_text_bytes};return limits }}()\n'
            'let value = try XmlExampleOrder.decodeXml(input,xmlLimits)')
        source = source.replace('.encodeXml()', '.encodeXml(xmlLimits)')
        for argument in ('input', 'encoded'):
            source = source.replace('XmlExampleOrder.decodeXml(' + argument + ')',
                                    'XmlExampleOrder.decodeXml(' + argument + ',xmlLimits)')
    return source


def main():
    """Require each selected real SDK, independent values and calibrated fresh-process measurements."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default=','.join(LANGUAGES))
    parser.add_argument('--libxml2-prefix', type=Path)
    parser.add_argument('--kotlin-home', type=Path)
    parser.add_argument('--cargo-home', type=Path)
    parser.add_argument('--tags', type=int, default=20000)
    parser.add_argument('--xml-entries', type=int, default=100000,
                        help='Kotlin/Swift XML cumulative element/namespace-entry budget')
    parser.add_argument('--xml-text-bytes', type=int, default=8 * 1024 * 1024,
                        help='Kotlin/Swift cumulative XML text budget; input remains bounded to 16 MiB')
    args = parser.parse_args()
    suppress_windows_error_dialogs()
    if os.name != 'nt':
        parser.error('This profile requires native Windows CPU/peak-residency counters; other platforms remain pending')
    languages = args.language.split(',')
    if not languages or len(set(languages)) != len(languages) or any(item not in LANGUAGES for item in languages):
        parser.error('Choose distinct supported language names')
    if not 1 <= args.tags <= MAX_WORKLOAD_TAGS:
        parser.error(f'--tags must be between 1 and {MAX_WORKLOAD_TAGS} for this selected owning profile')
    if not 1 <= args.xml_entries <= 200000 or not 1 <= args.xml_text_bytes <= 16 * 1024 * 1024:
        parser.error('Selected Kotlin/Swift XML bounds allow at most 200000 entries and 16 MiB text')
    build, compiler = args.build.resolve(), args.compiler.resolve(strict=True)
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('In-repository output must remain beneath ignored out/')
    if build.exists() and any(build.iterdir()):
        parser.error('Use an empty --build directory to exclude stale sources and preserve earlier evidence')
    if 'c' in languages and args.libxml2_prefix is None:
        parser.error('C requires an explicit --libxml2-prefix and an initialized native compiler environment')
    if 'kotlin' in languages and args.kotlin_home is None:
        parser.error('Kotlin requires --kotlin-home pointing to the actual compiler distribution')
    build.mkdir(parents=True, exist_ok=True)
    compiler_hash = digest(compiler)
    environment = os.environ.copy()
    if args.cargo_home:
        environment['CARGO_HOME'] = str(args.cargo_home.resolve(strict=True))
    environment['CARGO_NET_OFFLINE'] = 'true'
    environment['RUSTFLAGS'] = '-D warnings'
    commands, tools, identities = [], {}, {}

    def run(command, name, measured=False, cwd=None):
        """Retain exact argv and process-image counters after the native measured child exits."""
        argv = [str(value) for value in command]
        with subprocess.Popen(argv, cwd=cwd, env=environment, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, creationflags=subprocess.CREATE_NO_WINDOW) as process:
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
        if result.returncode:
            print((stdout + stderr).decode('utf-8', errors='replace'), file=sys.stderr)
        result.check_returncode()
        return (stdout + stderr).decode('utf-8'), counters

    java = None
    if 'kotlin' in languages:
        settings = run(['java', '-XshowSettings:properties', '-version'], 'java-home')[0]
        home = next((line.split('=', 1)[1].strip() for line in settings.splitlines()
                     if line.strip().startswith('java.home =')), None)
        if home is None:
            raise AssertionError('The actual JVM did not identify java.home')
        java = (Path(home) / 'bin/java.exe').resolve(strict=True)
        identities['java'] = {'path': str(java), 'sha256': digest(java)}
        tools['java'] = run([java, '-version'], 'java-version')[0].strip()
    for language, command in (('rust', ['rustc', '--version', '-v']), ('swift', ['swiftc', '--version']),
                               ('c', ['cmake', '--version'])):
        if language in languages:
            tools[language] = run(command, language + '-version')[0].strip()
    if 'rust' in languages:
        tools['cargo'] = run(['cargo', '--version'], 'cargo-version')[0].strip()
    tools['compiler'] = run([compiler, '--version'], 'compiler-version')[0].strip()
    if 'swift' in languages:
        identities['swift_target'] = json.loads(run(['swiftc', '-print-target-info'], 'swift-target-info')[0])
    profiles = {}
    for language in languages:
        for enabled in (True, False):
            profile = language + ('-xml' if enabled else '-native')
            directory = build / profile
            directory.mkdir()
            generated = directory / OUTPUTS[language]
            command = [compiler, '--input', SOURCE / 'model.serializer', '--language', language,
                       '--' + language + '.output', generated, '--protocols', 'binary_none',
                       '--xml.enabled', str(enabled).lower(), '--kotlin.package=']
            if enabled:
                command += ['--config', SOURCE / 'codegen.ini']
            run(command, profile + '-generate')
            sources = [{'name': generated.name, 'bytes': generated.stat().st_size, 'sha256': digest(generated)}]
            source = directory / DRIVER_NAMES[language]
            source.write_text(driver(language, enabled, args.xml_entries, args.xml_text_bytes), encoding='utf-8')
            dependencies = []
            if language in ('c', 'swift'):
                for name in ('benchmark_sink.c', 'benchmark_sink.h', 'module.modulemap'):
                    shutil.copyfile(DRIVERS / name, directory / name)
                    dependencies.append({'name': name, 'sha256': digest(directory / name)})
            if language == 'c':
                prefix = args.libxml2_prefix.resolve(strict=True)
                (directory / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 4.0)\n'
                    'project(serializer_xml_measurement LANGUAGES C)\nadd_executable(benchmark main.c benchmark_sink.c)\n'
                    'set_property(TARGET benchmark PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)\n'
                    'target_include_directories(benchmark PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")\n'
                    + ('find_package(LibXml2 REQUIRED)\ntarget_link_libraries(benchmark PRIVATE LibXml2::LibXml2)\n' if enabled else '')
                    + 'if(MSVC)\n target_compile_options(benchmark PRIVATE /std:c11 /W4 /WX /O2 /GL- /utf-8)\n'
                      ' target_compile_definitions(benchmark PRIVATE _CRT_SECURE_NO_WARNINGS)\n'
                      'else()\n target_compile_options(benchmark PRIVATE -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror)\nendif()\n', encoding='utf-8')
                native = directory / 'native'
                run(['cmake', '-G', 'Ninja', '-S', directory, '-B', native,
                     '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_PREFIX_PATH=' + str(prefix)], profile + '-configure')
                run(['cmake', '--build', native, '--config', 'Release', '--parallel', '2'], profile + '-compile')
                compiler_metadata = list((native / 'CMakeFiles').glob('*/CMakeCCompiler.cmake'))
                if len(compiler_metadata) != 1:
                    raise AssertionError('Missing exact native C compiler metadata')
                identities[profile + '-compiler-info'] = compiler_metadata[0].read_text(encoding='utf-8')
                launch = [native / 'benchmark.exe']
                if enabled:
                    for library in sorted((prefix / 'bin').glob('*.dll')):
                        shutil.copyfile(library, native / library.name)
                        dependencies.append({'path': str(library), 'sha256': digest(library)})
                identities[profile + '-compiler'] = (native / 'CMakeCache.txt').read_text(encoding='utf-8')
            elif language == 'rust':
                for name in ('Cargo.toml', 'Cargo.lock'):
                    shutil.copyfile(SOURCE / 'rust' / name, directory / name)
                    dependencies.append({'name': name, 'sha256': digest(directory / name)})
                run(['cargo', 'build', '--offline', '--locked', '--release', '--manifest-path', directory / 'Cargo.toml'], profile + '-compile')
                launch = [directory / 'target/release/serializer-xml-example.exe']
            elif language == 'kotlin':
                home = args.kotlin_home.resolve(strict=True)
                compiler_jar = home / 'lib/kotlin-compiler.jar'
                compiler_jar.resolve(strict=True)
                jar = directory / 'benchmark.jar'
                run([java, '-cp', str(home / 'lib/*'), 'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler',
                     '-kotlin-home', home, '-jvm-target', '17', '-Werror', generated, source,
                     '-include-runtime', '-d', jar], profile + '-compile')
                dependencies = [{'name': path.name, 'sha256': digest(path)} for path in sorted((home / 'lib').glob('*.jar'))]
                launch = [java, '-Djdk.xml.maxGeneralEntitySizeLimit=4194304',
                          '-Djdk.xml.totalEntitySizeLimit=8388608', '-jar', jar]
                tools['kotlin'] = (home / 'build.txt').read_text(encoding='utf-8').strip()
            else:
                executable = directory / 'benchmark.exe'
                sink = directory / 'benchmark_sink.obj'
                run(['cl', '/nologo', '/c', '/MD', '/O2', '/GL-', '/W4', '/WX',
                     '/I' + str(directory), '/Fo:' + str(sink), directory / 'benchmark_sink.c'], profile + '-sink-compile')
                run(['swiftc', '-O', '-warnings-as-errors', '-I', directory,
                     generated, source, sink, '-o', executable], profile + '-compile')
                launch = [executable]
            profiles[profile] = {'generated_sources': sources, 'launch': launch, 'dependencies': dependencies,
                                 'driver_sha256': digest(source), 'executable_path': str(launch[-1]),
                                 'executable_sha256': digest(launch[-1])}

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
                        raise AssertionError('Could not resolve timing batch: ' + label)
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
                        raise AssertionError('Native values differ across operations/profiles/SDKs: ' + label)
                    if enabled:
                        check_xml(Path(str(output) + '.xml').read_bytes(), tags)
                        measurement['xml_bytes'] = Path(str(output) + '.xml').stat().st_size
                        measurement['xml_sha256'] = digest(Path(str(output) + '.xml'))
                        wire = output
                    workload['operations'][profile + '-' + mode] = measurement
        results.append(workload)
        print('PASS: native/mobile measurements ' + name + ' across ' + str(len(languages)) + ' SDKs', flush=True)
    if digest(compiler) != compiler_hash:
        raise AssertionError('Compiler changed during measurement; use an immutable snapshot')
    report = {'status': 'passed', 'scope': 'Selected native Windows ' + '/'.join(languages) + ' owning XML/native profiles',
              'languages': languages,
              'compiler_sha256': compiler_hash, 'platform': platform.platform(), 'python': sys.version,
              'tools': tools, 'sdk_identities': identities, 'workload_seed': WORKLOAD_SEED,
              'schema_sha256': digest(SOURCE / 'model.serializer'),
              'kotlin_swift_xml_policy': {'max_bytes': 16 * 1024 * 1024, 'max_nodes_and_namespace_entries': args.xml_entries,
                                          'max_text_bytes': args.xml_text_bytes},
              'mapping_sha256': digest(SOURCE / 'codegen.ini'),
              'driver_hashes': {DRIVER_NAMES[language]: digest(DRIVERS / DRIVER_NAMES[language]) for language in languages},
              'runner_sha256': digest(Path(__file__)), 'warmup_batches': 3, 'measured_batches': 5,
              'profiles': {name: {key: value for key, value in profile.items() if key != 'launch'} for name, profile in profiles.items()},
              'measurement_limits': [
                  'Each measured operation starts a fresh actual native process/JVM; five wall samples exclude setup, independent result validation and output I/O.',
                  'Calibration increases batch size until its median reaches 10 ms; process CPU/peak residency include startup, warmups, calibration batch setup and validation.',
                  'Absolute process high-water is not exact codec allocation; no before/after subtraction is made.',
                  'C/Swift have no exact allocation instrumentation; native provider/allocator overhead is included only in process residency.',
                  'Rust counts successful global allocation/reallocation requested bytes for one separate call, including full realloc requested extents; allocator overhead is excluded. Its disabled counter check remains in timed allocation calls.',
                  'Kotlin uses JVM current-thread allocated bytes for one separate call; JAXP native allocation may be excluded.',
                  'Rust byte-input adapter validates UTF8 for XML decoding inside the measured call; generated decode_xml accepts str.',
                  'C/Rust/Swift owning replacement drops the preceding result during timing; JVM collection behavior belongs to the selected warmed process.',
                  'C/Swift pass every result through a separately compiled opaque C sink without LTO; its call overhead is included. Rust uses black_box; Kotlin uses a volatile result sink.',
                  'XML-disabled BINARY_NONE controls preserve identical native values/wire bytes; this different format is not equivalent XML provider work.',
                  'C/Rust default generated bounds remain active; Kotlin/Swift use the explicitly recorded XML policy. Owning measurements do not qualify streaming or hard process-memory caps.',
                  'Kotlin uses finite JAXP general/total entity bounds 4194304/8388608 for escaped-text workloads.',
                  'Concurrent machine work affects timing; no speed threshold or earlier-compiler performance regression baseline is claimed.',
                  'Other platforms/providers, broader vocabularies and inputs larger than the selected profile remain unqualified.'],
              'workloads': results}
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('PASS: retained ' + str(len(languages) * 18) + ' isolated operation measurements', flush=True)


if __name__ == '__main__':
    main()
