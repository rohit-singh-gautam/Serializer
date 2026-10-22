"""Generate, build and exchange XML through every selected language's generated codec."""
import argparse
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys

SOURCE = Path(__file__).resolve().parent
ROOT = SOURCE.parent.parent
spec = importlib.util.spec_from_file_location('serializer_example_runner', SOURCE.parent / 'run.py')
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)
manifest_spec = importlib.util.spec_from_file_location('serializer_xml_fixture_manifest', SOURCE / 'fixture_manifest.py')
fixtures = importlib.util.module_from_spec(manifest_spec)
manifest_spec.loader.exec_module(fixtures)


def copy_sources(language, destination, source_root=SOURCE):
    """Stage maintained inputs only; generated files and SDK output stay in the build tree."""
    destination.mkdir(parents=True, exist_ok=True)
    source = source_root / language
    if language == 'cpp' and not source.is_dir():
        return
    for file in source.iterdir():
        if file.is_file() and file.name != 'README.md':
            shutil.copyfile(file, destination / file.name)


def check_xml(path, expected):
    """Compare expanded names and values with an independent standard-library parser."""
    fixtures.check_document(path, expected)


def build_c(args, runner, directory, source_root=SOURCE):
    """Use CMake's native compiler discovery and explicitly link the optional XML provider."""
    build = directory / 'native'
    command = ['cmake', '-S', SOURCE / 'c', '-B', build,
               f'-DXML_GENERATED_DIRECTORY={directory}',
               f'-DXML_EXAMPLE_SOURCE={source_root / "c/main.c"}', '-DCMAKE_BUILD_TYPE=Release']
    prefix = args.libxml2_prefix
    if prefix is None and os.name == 'nt':
        candidate = args.compiler.parent.parent / 'vcpkg_installed/x64-windows'
        if candidate.exists():
            prefix = candidate
    if prefix:
        command.append(f'-DCMAKE_PREFIX_PATH={prefix}')
    sdk.run(command)
    sdk.run(['cmake', '--build', build, '--config', 'Release', '--parallel', '2'])
    executable = build / ('Release/serializer_xml_c.exe' if os.name == 'nt' else 'serializer_xml_c')
    if os.name == 'nt' and not executable.is_file():
        executable = build / 'serializer_xml_c.exe'
    if prefix and os.name == 'nt':
        for library in (prefix / 'bin').glob('*.dll'):
            shutil.copyfile(library, executable.parent / library.name)
    return runner.launch('c', str(executable))


def build_language(args, runner, language, directory, source_root=SOURCE):
    """Build the actual generated source with the selected native SDK and locked dependencies."""
    copy_sources(language, directory, source_root)
    path = lambda name: runner.path(language, directory / name)
    if language == 'cpp':
        executable = args.cpp_executable
        if executable is None:
            base = args.compiler.parent.parent / 'example/xml/cpp'
            executable = base / ('Release/serializer_xml_cpp.exe' if os.name == 'nt' else 'serializer_xml_cpp')
        if not executable.is_file():
            raise RuntimeError('Build serializer_xml_cpp, or pass --cpp-executable')
        return runner.launch(language, runner.path(language, executable))
    if language in ('javascript', 'typescript'):
        sdk.run(runner.command(language, 'npm', 'ci', '--ignore-scripts', '--no-audit', '--no-fund'), cwd=directory)
        if language == 'typescript':
            shutil.copyfile(ROOT / 'example/typescript/node.d.mts', directory / 'node.d.mts')
            sdk.run(runner.command(language, 'node', path('node_modules/typescript/bin/tsc'),
                    '--strict', '--target', 'ES2020', '--module', 'NodeNext', '--moduleResolution',
                    'NodeNext', path('main.mts'), path('node.d.mts')))
        return runner.command(language, 'node', path('main.mjs'))
    if language == 'rust':
        sdk.run(runner.command(language, 'cargo', 'build', '--locked', '--release',
                              '--manifest-path', path('Cargo.toml')))
        executable = path('target/release/serializer-xml-example' +
                          ('.exe' if os.name == 'nt' and language not in runner.wsl else ''))
        return runner.launch(language, executable)
    if language == 'c' and language not in runner.wsl:
        return build_c(args, runner, directory, source_root)
    if language == 'c':
        flags = sdk.run(runner.command(language, 'pkg-config', '--cflags', '--libs', 'libxml-2.0'), capture=True).stdout.split()
        executable = path('main')
        sdk.run(runner.command(language, 'cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Wpedantic',
                              '-Werror', path('main.c'), '-I' + path('.'), '-o', executable, *flags, '-lm'))
        return runner.launch(language, executable)
    return runner.build(language, source_root.name, directory, source_root / language)


def main():
    """Require every requested backend and validate all producer/consumer combinations."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, default=ROOT / 'out/build/xml-examples')
    parser.add_argument('--config', type=Path)
    parser.add_argument('--manifest', type=Path, default=ROOT / 'test/resources/xml_exchange_manifest.json')
    parser.add_argument('--language')
    parser.add_argument('--wsl-languages', default='')
    parser.add_argument('--cpp-executable', type=Path)
    parser.add_argument('--libxml2-prefix', type=Path)
    args = parser.parse_args()
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    if args.libxml2_prefix:
        args.libxml2_prefix = args.libxml2_prefix.resolve()
    args.cpp_library = None
    args.sanitize = False
    manifest = fixtures.load_manifest(args.manifest, ROOT)
    args.config = args.config.resolve() if args.config else fixtures.source_path(ROOT, manifest['config'])
    if fixtures.source_path(ROOT, manifest['schema']) != SOURCE / 'model.serializer':
        parser.error('This runner requires the maintained XML order schema')
    languages = args.language.split(',') if args.language else [entry['id'] for entry in manifest['languages']]
    if not languages or len(languages) != len(set(languages)) or any(language not in sdk.LANGUAGES for language in languages):
        parser.error('Choose supported language names from ' + ','.join(sdk.LANGUAGES))
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    os.environ.setdefault('npm_config_cache', str(args.build / 'npm-cache'))
    os.environ.setdefault('CARGO_HOME', str(args.build / 'cargo-home'))
    runner = sdk.Runner(args)
    fixtures.require_sdks(manifest, languages, runner, sdk.run)
    targets = {}
    for language in languages:
        directory = args.build / language
        directory.mkdir(parents=True, exist_ok=True)
        output = 'internal/model/model_gen.go' if language == 'go' else sdk.OUTPUTS[language]
        destination = directory / output
        destination.parent.mkdir(parents=True, exist_ok=True)
        targets['js' if language == 'javascript' else language] = destination
    if 'typescript' in languages and 'javascript' not in languages:
        targets['js'] = args.build / 'typescript/schema.mjs'
    command = [args.compiler, '--input', fixtures.source_path(ROOT, manifest['schema']), '--config', args.config,
               '--language', ','.join(targets), '--go.package', 'model', '--kotlin.package=']
    for language, destination in targets.items():
        command += [f'--{language}.output', destination]
    sdk.run(command)
    if 'typescript' in languages and 'javascript' in languages:
        shutil.copyfile(targets['js'], args.build / 'typescript/schema.mjs')
    commands = {language: build_language(args, runner, language, args.build / language) for language in languages}
    outputs = {}
    for language, command in commands.items():
        output = args.build / f'{language}.xml'
        sdk.run([*command, runner.path(language, fixtures.source_path(ROOT, manifest['input'])), runner.path(language, output)])
        check_xml(output, manifest['expected'])
        outputs[language] = output
    for producer, output in outputs.items():
        for consumer, command in commands.items():
            received = args.build / f'{producer}-to-{consumer}.xml'
            sdk.run([*command, runner.path(consumer, output), runner.path(consumer, received)])
            check_xml(received, manifest['expected'])
    print(f'PASS: generated XML, {len(languages)} producers x {len(languages)} consumers', flush=True)


if __name__ == '__main__':
    main()
