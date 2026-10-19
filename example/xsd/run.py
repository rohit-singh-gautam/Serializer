"""Import or author an XSD-bound model, build every selected SDK and exchange typed XML."""
import argparse
from contextlib import contextmanager
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import xml.etree.ElementTree as et

SOURCE = Path(__file__).resolve().parent
ROOT = SOURCE.parent.parent
spec = importlib.util.spec_from_file_location('serializer_xml_sdk_runner', SOURCE.parent / 'xml/run.py')
xml_sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(xml_sdk)
sdk = xml_sdk.sdk


@contextmanager
def compiler_environment(language):
    """Keep manually injected MSVC search lists out of Swift's own Windows toolchain discovery."""
    removed = {}
    if os.name == 'nt' and language == 'swift':
        for key in ('INCLUDE', 'LIB', 'LIBPATH'):
            if key in os.environ:
                removed[key] = os.environ.pop(key)
    try:
        yield
    finally:
        os.environ.update(removed)


def check_xml(path):
    """Check the shared expanded-name contract with an independent XML implementation."""
    data = path.read_bytes()
    if len(data) > 16 * 1024 * 1024 or b'<!DOCTYPE' in data:
        raise AssertionError(f'Invalid XML extent or DTD in {path}')
    root = et.fromstring(data)
    namespace = '{urn:serializer:xsd-example}'
    if root.tag != namespace + 'order' or root.attrib != {'id': '7'}:
        raise AssertionError(f'Root or attribute mapping differs: {path}')
    expected = [(namespace + 'customer', 'R&D'), (namespace + 'count', '3'),
                (namespace + 'tag', 'A'), (namespace + 'tag', 'B')]
    if [(node.tag, node.text or '') for node in root] != expected:
        raise AssertionError(f'Ordered XML child mapping differs: {path}')


def provider_prefix(args):
    """Use an explicit provider prefix or a neighboring compiler-build vcpkg installation."""
    if args.libxml2_prefix is not None:
        return args.libxml2_prefix.resolve()
    if os.name == 'nt':
        for root in (args.compiler.parent, args.compiler.parent.parent):
            candidate = root / 'vcpkg_installed/x64-windows'
            if candidate.is_dir():
                return candidate
    return None


def build_cpp(args, runner, directory, source):
    """Compile the checked C++ client against the requested runtime and XML provider."""
    build = directory / 'native'
    library = args.cpp_library or args.compiler.parent / (
        'serializer_runtime.lib' if os.name == 'nt' else 'libserializer_runtime.a')
    if not library.is_file():
        raise RuntimeError('Build the XML-enabled runtime or pass --cpp-library')
    command = ['cmake', '-S', SOURCE / 'cpp', '-B', build, '-DCMAKE_BUILD_TYPE=Release',
               f'-DXSD_GENERATED_DIRECTORY={directory}', f'-DXSD_EXAMPLE_SOURCE={source / "cpp/main.cpp"}',
               f'-DSERIALIZER_INCLUDE_DIRECTORY={ROOT / "include"}',
               f'-DSERIALIZER_RUNTIME_LIBRARY={library.resolve()}']
    if args.cmake_generator:
        command += ['-G', args.cmake_generator]
    prefix = provider_prefix(args)
    if prefix:
        command.append(f'-DCMAKE_PREFIX_PATH={prefix}')
    sdk.run(command)
    sdk.run(['cmake', '--build', build, '--config', 'Release', '--parallel', '2'])
    name = 'serializer_xsd_cpp' + ('.exe' if os.name == 'nt' else '')
    executable = next((path for path in (build / 'Release' / name, build / name) if path.is_file()), None)
    if executable is None:
        raise RuntimeError('CMake did not produce the C++ example executable')
    if prefix and os.name == 'nt':
        for library in (prefix / 'bin').glob('*.dll'):
            shutil.copyfile(library, executable.parent / library.name)
    return runner.launch('cpp', runner.path('cpp', executable))


def generate_workflow(args, runner, workflow, languages):
    """Run the compiler's real import/binding pipeline and build the explicit clients."""
    build = args.build / workflow
    build.mkdir(parents=True, exist_ok=True)
    source = SOURCE / workflow
    if workflow == 'imported':
        schema = build / 'model.serializer'
        sdk.run([args.compiler, '--import-xsd', SOURCE / 'order.xsd', '--namespace', 'xsd_example',
                 '--root', 'order', '--output', schema, '--depfile', build / 'model.serializer.d'])
    else:
        schema = source / 'model.serializer'
    destinations = {}
    for language in languages:
        directory = build / language
        directory.mkdir(parents=True, exist_ok=True)
        output = 'model.hpp' if language == 'cpp' else (
            'internal/model/model_gen.go' if language == 'go' else sdk.OUTPUTS[language])
        destination = directory / output
        destination.parent.mkdir(parents=True, exist_ok=True)
        destinations['js' if language == 'javascript' else language] = destination
    if 'typescript' in languages and 'javascript' not in languages:
        destinations['js'] = build / 'typescript/schema.mjs'
    command = [args.compiler, '--input', schema, '--config', SOURCE / 'codegen.ini',
               '--language', ','.join(destinations), '--go.package', 'model', '--kotlin.package=',
               '--depfile', build / 'generated.d']
    for language, destination in destinations.items():
        command += [f'--{language}.output', destination]
    sdk.run(command)
    if 'typescript' in languages and 'javascript' in languages:
        shutil.copyfile(destinations['js'], build / 'typescript/schema.mjs')
    commands = {}
    for language in languages:
        directory = build / language
        if language == 'cpp':
            commands[language] = build_cpp(args, runner, directory, source)
        else:
            with compiler_environment(language):
                commands[language] = xml_sdk.build_language(args, runner, language, directory, source)
    return commands


def run_workflow(args, runner, workflow, commands):
    """Exercise each SDK's own checks and every selected producer/consumer pair."""
    build = args.build / workflow
    outputs = {}
    for language, command in commands.items():
        output = build / f'{language}.xml'
        sdk.run([*command, runner.path(language, SOURCE / 'order.xml'), runner.path(language, output)])
        check_xml(output)
        outputs[language] = output
    pairs = 0
    for producer, output in outputs.items():
        for consumer, command in commands.items():
            received = build / f'{producer}-to-{consumer}.xml'
            sdk.run([*command, runner.path(consumer, output), runner.path(consumer, received)])
            check_xml(received)
            pairs += 1
    return outputs, pairs


def main():
    """Require every selected SDK and record only completed import/manual qualification."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, default=ROOT / 'out/build/xsd-examples')
    parser.add_argument('--workflow', choices=('both', 'imported', 'manual'), default='both')
    parser.add_argument('--language', default=','.join(sdk.LANGUAGES))
    parser.add_argument('--wsl-languages', default='')
    parser.add_argument('--cpp-library', type=Path)
    parser.add_argument('--libxml2-prefix', type=Path)
    parser.add_argument('--cmake-generator')
    args = parser.parse_args()
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    args.cpp_library = args.cpp_library.resolve() if args.cpp_library else None
    args.libxml2_prefix = provider_prefix(args)
    args.sanitize = False
    args.cpp_executable = None
    languages = list(dict.fromkeys(filter(None, args.language.split(','))))
    if not languages or any(language not in sdk.LANGUAGES for language in languages):
        parser.error('Choose language names from ' + ','.join(sdk.LANGUAGES))
    if 'cpp' in args.wsl_languages.split(','):
        parser.error('Run C++ from the host platform or invoke this runner inside WSL')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be under ignored out/')
    args.build.mkdir(parents=True, exist_ok=True)
    runner = sdk.Runner(args)
    workflows = ('imported', 'manual') if args.workflow == 'both' else (args.workflow,)
    record = {'compiler': sdk.run([args.compiler, '--version'], capture=True).stdout.strip(),
              'languages': languages, 'requested_workflows': list(workflows), 'completed': {},
              'cross_workflow_pairs': 0, 'status': 'running'}
    report = args.build / 'qualification.json'
    report.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
    commands, exports = {}, {}
    try:
        for workflow in workflows:
            commands[workflow] = generate_workflow(args, runner, workflow, languages)
            exports[workflow], pairs = run_workflow(args, runner, workflow, commands[workflow])
            record['completed'][workflow] = {'sdk_builds': len(languages), 'producer_consumer_pairs': pairs}
            report.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
        if len(workflows) == 2:
            for producer, consumer in (('imported', 'manual'), ('manual', 'imported')):
                for language in languages:
                    output = args.build / f'{producer}-to-{consumer}-{language}.xml'
                    sdk.run([*commands[consumer][language], runner.path(language, exports[producer][language]),
                             runner.path(language, output)])
                    check_xml(output)
                    record['cross_workflow_pairs'] += 1
        record['status'] = 'passed'
    except Exception:
        record['status'] = 'failed'
        raise
    finally:
        report.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(record, indent=2), flush=True)


if __name__ == '__main__':
    main()
