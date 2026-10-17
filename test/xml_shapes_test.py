"""Generate and qualify typed XML mappings with every selected output language's SDK."""
import argparse
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / 'test/resources/xml_shapes'
spec = importlib.util.spec_from_file_location('serializer_xml_shape_runner', ROOT / 'example/xml/run.py')
xml_runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(xml_runner)
sdk = xml_runner.sdk


def qualify_xml_names(compiler, build, languages):
    """Reject XML API collisions while preserving the same schemas without XML generation."""
    directory = build / 'name-diagnostics'
    directory.mkdir(parents=True, exist_ok=True)
    count = 0
    def verify(command, source):
        """Require XML-specific rejection and a successful XML-disabled compatibility control."""
        disabled = subprocess.run([*command, '--xml.enabled', 'false'], capture_output=True, text=True)
        if disabled.returncode != 0:
            raise AssertionError(f'XML-disabled name compatibility failed: {source}\n{disabled.stdout}{disabled.stderr}')
        enabled = subprocess.run([*command, '--xml.enabled', 'true'], capture_output=True, text=True)
        if enabled.returncode == 0 or 'XML' not in enabled.stdout + enabled.stderr:
            raise AssertionError(f'Expected an XML naming diagnostic: {source}\n{enabled.stdout}{enabled.stderr}')

    for language in ('java', 'python', 'csharp', 'javascript', 'typescript', 'rust'):
        if language not in languages:
            continue
        profiles = ('profile', 'preserve') if language in ('java', 'csharp') else ('profile',)
        for profile in profiles:
            method = 'encodeXml' if language in ('javascript', 'typescript') else (
                'encode_xml' if profile == 'profile' else ('encodeXml' if language == 'java' else 'EncodeXml'))
            declarations = (
                f'public function double {method}() readonly expression(1.0);',
                f'public factory {method}() {{}}',
                f'public conversion target as {method} {{}};',
            )
            if language in ('java', 'csharp'):
                helper = 'srl_xml_float' if profile == 'profile' else (
                    'srlXmlFloat' if language == 'java' else 'SrlXmlFloat')
                declarations += (f'public function double {helper}(string text, bool single) readonly expression(2.0);',)
            for index, declaration in enumerate(declarations):
                source = directory / f'{language}-{profile}-{index}.serializer'
                source.write_text('serializer version 1.17.0; class target {} '
                                  f'class sample {{ public double value; {declaration} }}', encoding='utf-8')
                cli_language = 'js' if language == 'javascript' else language
                command = [str(compiler), '--input', str(source), '--language', cli_language,
                           f'--{cli_language}.output', str(directory / sdk.OUTPUTS[language]), '--cpp.format', 'false']
                if language in ('java', 'csharp'):
                    command += [f'--{language}.naming', profile]
                verify(command, source)
                count += 1
        if language == 'java':
            for package in ('javax', 'org'):
                source = directory / f'java-package-{package}.serializer'
                source.write_text(f'serializer version 1.17.0; class {package} {{}}', encoding='utf-8')
                command = [str(compiler), '--input', str(source), '--language', 'java',
                           '--java.naming', 'preserve', '--java.output', str(directory / sdk.OUTPUTS['java'])]
                verify(command, source)
                count += 1
    if count:
        print(f'PASS: {count} XML name diagnostics and XML-disabled compatibility controls', flush=True)


def main():
    """Generate real source, compile with strict warnings, and run maintained regressions."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default=','.join(sdk.LANGUAGES))
    parser.add_argument('--protocols', choices=('all', 'binary_none'), default='all')
    parser.add_argument('--wsl-languages', default='')
    parser.add_argument('--python-dependencies', type=Path)
    parser.add_argument('--cpp-executable', type=Path)
    parser.add_argument('--libxml2-prefix', type=Path)
    parser.set_defaults(executable=None, cpp_library=None, sanitize=False)
    args = parser.parse_args()
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    languages = args.language.split(',')
    if not languages or len(languages) != len(set(languages)) or any(item not in sdk.LANGUAGES for item in languages):
        parser.error('Select distinct language names from ' + ','.join(sdk.LANGUAGES))
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    if 'cpp' in languages:
        if args.cpp_executable is None:
            parser.error('C++ shapes require --cpp-executable; select other languages explicitly while native XML unit tests provide C++ coverage')
        args.cpp_executable = args.cpp_executable.resolve()
        if not args.cpp_executable.is_file():
            parser.error('The selected C++ shapes executable does not exist')
    else:
        print('C++ omitted explicitly; run the native XML unit tests or pass a C++ shapes executable separately.', flush=True)
    args.build.mkdir(parents=True, exist_ok=True)
    os.environ.setdefault('npm_config_cache', str(args.build / 'npm-cache'))
    os.environ.setdefault('CARGO_HOME', str(args.build / 'cargo-home'))
    if args.python_dependencies:
        os.environ['PYTHONPATH'] = str(args.python_dependencies.resolve()) + os.pathsep + os.environ.get('PYTHONPATH', '')
    qualify_xml_names(args.compiler, args.build, languages)
    directories = {}
    targets = {}
    for language in languages:
        directory = args.build / language
        directory.mkdir(parents=True, exist_ok=True)
        directories[language] = directory
        targets['js' if language == 'javascript' else language] = directory / sdk.OUTPUTS[language]
    if 'typescript' in languages and 'javascript' not in languages:
        targets['js'] = args.build / 'typescript/schema.mjs'
    command = [args.compiler, '--input', SOURCE / 'model.serializer', '--config', SOURCE / 'options.ini',
               '--language', ','.join(targets), '--protocols', args.protocols,
               '--cpp.format', 'false', '--go.package', 'main', '--kotlin.package=']
    for language, output in targets.items():
        command += [f'--{language}.output', output]
    sdk.run(command)
    if 'typescript' in languages and 'javascript' in languages:
        shutil.copyfile(targets['js'], args.build / 'typescript/schema.mjs')
    runner = sdk.Runner(args)
    for language in languages:
        launch = xml_runner.build_language(args, runner, language, directories[language], source_root=SOURCE)
        sdk.run(launch)
    print(f'PASS: generated typed XML and configured validator boundaries ({len(languages)} backends)', flush=True)


if __name__ == '__main__':
    main()
