"""Build and run maintained examples; schema compilation always uses the C++ executable."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
LANGUAGES = ('cpp', 'java', 'javascript', 'typescript', 'go', 'csharp', 'rust', 'python', 'swift', 'kotlin', 'c')
EXAMPLES = ('basic', 'collections', 'complex', 'interoperability')
OUTPUTS = {'cpp': 'message.hpp', 'java': 'Schema.java', 'javascript': 'schema.mjs',
           'typescript': 'schema.d.mts', 'go': 'schema.go', 'csharp': 'Schema.cs',
           'rust': 'schema.rs', 'python': 'schema.py', 'swift': 'Schema.swift',
           'kotlin': 'Schema.kt', 'c': 'schema.h'}


def run(command, *, cwd=None, capture=False):
    """Execute an argument vector without a shell and fail on any SDK or example error."""
    return subprocess.run([str(x) for x in command], cwd=cwd, check=True,
                          stdout=subprocess.PIPE if capture else None,
                          text=capture, encoding='utf-8' if capture else None, timeout=600)


def verify_positional_bytes(directory, producers):
    """Pin every producer to independently specified, shared little-endian wire bytes."""
    fixture = json.loads((ROOT / 'test/positional_binary_fixture.json').read_text(encoding='utf-8'))
    for variant, suffix in enumerate(fixture['variant_hex']):
        expected = bytes.fromhex(fixture['prefix_hex'] + suffix)
        for producer in producers:
            actual = (directory / f'{producer}_BINARY_NONE_{variant}.bin').read_bytes()
            if actual != expected:
                raise AssertionError(f'{producer}: positional variant {variant} differs from frozen bytes')


class Runner:
    """Keep native/WSL SDK selection explicit and translate only actual filesystem paths."""
    def __init__(self, args):
        self.args = args
        self.wsl = set(filter(None, args.wsl_languages.split(',')))
        if getattr(args, 'big_endian', False) and os.name == 'nt':
            self.wsl.update(('cpp_s390x', 'c_s390x'))

    def path(self, language, path):
        """Translate a resolved Windows file path for an explicitly selected WSL SDK."""
        absolute = Path(path).resolve()
        if language in self.wsl:
            return run(['wsl', '--exec', 'wslpath', '-a', '-u', str(absolute)], capture=True).stdout.strip()
        return str(absolute)

    def command(self, language, executable, *arguments):
        """Resolve an SDK override or use PATH; missing required tools are hard failures."""
        executable = os.environ.get('SERIALIZER_' + executable.upper(), executable)
        if language in self.wsl:
            return ['wsl', '--exec', executable, *arguments]
        found = shutil.which(executable)
        if not found:
            raise RuntimeError(f'{executable} is required for {language}; install it or set SERIALIZER_{executable.upper()}')
        return [found, *arguments]

    def build(self, language, example, directory, source=None):
        """Compile handwritten consumers against freshly generated sources using the real SDK."""
        source = source or ROOT / 'example' / language / example
        for file in source.iterdir():
            if file.suffix in ('.cpp', '.c', '.java', '.mjs', '.mts', '.go', '.cs', '.rs', '.py', '.swift', '.kt', '.mod', '.csproj'):
                shutil.copyfile(file, directory / file.name)
        path = lambda value: self.path(language, directory / value)
        executable = path('main.exe' if os.name == 'nt' and language not in self.wsl else 'main')
        print(f'Build {language}/{example}', flush=True)
        if language == 'python':
            return [sys.executable, path('main.py')]
        if language in ('javascript', 'typescript'):
            if language == 'typescript':
                # Minimal Node declarations belong to this example, not the generated SDK.
                shutil.copyfile(ROOT / 'example/typescript/node.d.mts', directory / 'node.d.mts')
                run(self.command(language, 'tsc', '--strict', '--target', 'ES2020', '--module', 'NodeNext',
                                 '--moduleResolution', 'NodeNext', path('main.mts'), path('node.d.mts')))
            return self.command(language, 'node', path('main.mjs'))
        if language == 'java':
            run(self.command(language, 'javac', '--release', '17', '-encoding', 'UTF-8', '-Xlint:all', '-Werror',
                             '-d', path('classes'), path('Schema.java'), path('Main.java')))
            return self.command(language, 'java', '-cp', path('classes'), 'Main')
        if language == 'go':
            (directory / 'go.mod').write_text('module serializer.example\n\ngo 1.22\n')
            run(self.command(language, 'go', 'build', '-trimpath', '-o', executable, path('schema.go'), path('main.go')))
            return self.launch(language, executable)
        if language == 'csharp':
            (directory / 'Example.csproj').write_text('<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><OutputType>Exe</OutputType><TargetFramework>net8.0</TargetFramework><Nullable>enable</Nullable><TreatWarningsAsErrors>true</TreatWarningsAsErrors></PropertyGroup></Project>')
            run(self.command(language, 'dotnet', 'build', path('Example.csproj'), '--configuration', 'Release', '-p:Platform=AnyCPU', '--nologo', '-v:q'))
            return self.command(language, 'dotnet', path('bin/Release/net8.0/Example.dll'))
        if language == 'kotlin':
            run(self.command(language, 'kotlinc', path('Schema.kt'), path('Main.kt'), '-Werror', '-include-runtime', '-d', path('main.jar')))
            return self.command(language, 'java', '-jar', path('main.jar'))
        if language == 'swift':
            run(self.command(language, 'swiftc', '-O', '-warnings-as-errors', path('Schema.swift'), path('main.swift'), '-o', executable))
        elif language == 'rust':
            run(self.command(language, 'rustc', '--edition=2021', '-O', '-D', 'warnings', path('main.rs'), '-o', executable))
        elif language in ('cpp', 'c') and os.name == 'nt' and language not in self.wsl:
            library = self.args.cpp_library or self.args.compiler.parent / 'serializer_lib.lib'
            command = self.command(language, 'cl', '/nologo', '/MD', '/O2', '/W4', '/WX', '/utf-8', '/I' + path('.'), '/Fe:' + executable)
            if language == 'cpp':
                command += ['/std:c++20', '/EHsc', '/I' + self.path(language, ROOT / 'include'), path('main.cpp'), str(library)]
            else:
                if self.args.sanitize:
                    raise RuntimeError('Use a GCC/Clang C SDK (or --wsl-languages c) for --sanitize')
                command += ['/std:c11', '/TC', '/D_CRT_SECURE_NO_WARNINGS', path('main.c')]
            run(command, cwd=directory)
        else:
            compiler = 'cc' if language == 'c' else 'c++'
            command = self.command(language, compiler, '-std=c11' if language == 'c' else '-std=c++20',
                                   '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror', path('main.c' if language == 'c' else 'main.cpp'), '-o', executable)
            if language == 'cpp':
                library = self.args.cpp_library or self.args.compiler.parent / 'libserializer_lib.a'
                command += ['-I' + self.path(language, ROOT / 'include'), '-I' + path('.'), self.path(language, library)]
            else:
                command += ['-lm']
                if self.args.sanitize:
                    command += ['-fsanitize=address,undefined', '-g']
            run(command)
        return self.launch(language, executable)

    def launch(self, language, executable):
        """Run generated native binaries under the same host used to build them."""
        return ['wsl', '--exec', executable] if language in self.wsl else [executable]

    def build_big_endian(self, language, directory):
        """Compile the same C/C++ consumers for a big-endian CPU and run them under QEMU."""
        participant = language + '_s390x'
        path = lambda value: self.path(participant, value)
        executable = directory / ('main_' + participant)
        compiler = 's390x-linux-gnu-g++' if language == 'cpp' else 's390x-linux-gnu-gcc'
        command = self.command(participant, compiler, '-std=c++20' if language == 'cpp' else '-std=c11',
                               '-static', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                               '-include', path(ROOT / 'test/big_endian_host.h'),
                               '-I' + path(directory), '-o', path(executable),
                               path(directory / ('main.cpp' if language == 'cpp' else 'main.c')))
        if language == 'cpp':
            command += ['-DSERIALIZER_ENABLE_SIMD=0', '-I' + path(ROOT / 'include'),
                        path(ROOT / 'src/runtime_simd.cpp'), path(ROOT / 'src/simd_dispatch.cpp')]
        else:
            command += ['-lm']
        print(f'Build {participant}/interoperability (big-endian host, little-endian wire)', flush=True)
        run(command)
        return self.command(participant, 'qemu-s390x', path(executable))

    def example(self, example, languages):
        """Generate every selected output from one entry schema, then run all consumers."""
        build = self.args.build / example
        selected = list(languages)
        targets = {('js' if language == 'javascript' else language): build / language / OUTPUTS[language] for language in selected}
        if 'typescript' in selected and 'javascript' not in selected:
            targets['js'] = build / 'typescript/schema.mjs'
        for destination in targets.values():
            destination.parent.mkdir(parents=True, exist_ok=True)
        schema = ROOT / 'example/interoperability/message.serializer' if example == 'interoperability' else ROOT / 'example/schemas' / example / 'model.serializer'
        command = [self.args.compiler, '--input', schema, '--language', ','.join(targets), '--cpp.format', 'false', '--go.package', 'main', '--kotlin.package=']
        for language, destination in targets.items():
            command += [f'--{language}.output', destination]
        run(command)
        if 'typescript' in selected and 'javascript' in selected:
            shutil.copyfile(targets['js'], build / 'typescript/schema.mjs')
        commands = {language: self.build(language, example, build / language) for language in selected}
        if example == 'interoperability' and getattr(self.args, 'big_endian', False):
            for language in ('cpp', 'c'):
                if language not in selected:
                    raise ValueError('--big-endian requires cpp and c in --language')
                commands[language + '_s390x'] = self.build_big_endian(language, build / language)
        fixtures = build / 'fixtures'
        fixtures.mkdir(parents=True, exist_ok=True)
        if example == 'interoperability':
            producers = ['js' if language == 'javascript' else language for language in commands]
            (fixtures / 'producers.txt').write_text('\n'.join(producers) + '\n')
            for mode in ('emit', 'verify'):
                for language, command in commands.items():
                    directory = fixtures / language if mode == 'emit' and language.endswith('_s390x') else fixtures
                    directory.mkdir(parents=True, exist_ok=True)
                    run([*command, self.path(language, directory), mode])
                    if directory != fixtures:
                        source = language.removesuffix('_s390x')
                        for message in directory.glob(source + '_*.bin'):
                            shutil.copyfile(message, fixtures / (language + message.name[len(source):]))
                if mode == 'emit':
                    verify_positional_bytes(fixtures, producers)
            print(f'PASS: {len(commands)} producers x {len(commands)} consumers x 4 protocols x 3 variants = {len(commands)**2*12} exchanges; frozen positional bytes verified', flush=True)
        else:
            input_file = schema.parent / 'fixture.json'
            expected = json.loads(input_file.read_text(encoding='utf-8'))
            expected['revision'] += 1
            for language, command in commands.items():
                output = fixtures / f'{language}.json'
                run([*command, self.path(language, input_file), self.path(language, output)])
                actual = json.loads(output.read_text(encoding='utf-8'))
                if actual != expected:
                    raise AssertionError(f'{language}/{example}: decoded fields differ from the independent fixture')
            print(f'PASS: {example}, {len(selected)} languages, all 4 protocols', flush=True)


def main():
    """Expose one reproducible entry point for examples and CTest qualification."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, default=ROOT / 'out/examples')
    parser.add_argument('--language', default='all', help='all or comma-separated language folder names')
    parser.add_argument('--example', choices=(*EXAMPLES, 'all'), default='all')
    parser.add_argument('--cpp-library', type=Path)
    parser.add_argument('--wsl-languages', default='', help='Explicit Windows-to-WSL SDK selection, e.g. c,rust,swift')
    parser.add_argument('--sanitize', action='store_true', help='Enable address/undefined sanitizers for C')
    parser.add_argument('--big-endian', action='store_true',
                        help='Add s390x C/C++ producers and consumers through QEMU (WSL on Windows)')
    args = parser.parse_args()
    args.compiler = args.compiler.resolve(); args.build = args.build.resolve()
    languages = LANGUAGES if args.language == 'all' else tuple(args.language.split(','))
    if not languages or len(set(languages)) != len(languages) or any(x not in LANGUAGES for x in languages):
        parser.error('Choose distinct supported language folder names')
    if args.big_endian and (not {'cpp', 'c'}.issubset(languages) or args.example not in ('all', 'interoperability')):
        parser.error('--big-endian requires cpp and c and the interoperability example')
    runner = Runner(args)
    for example in EXAMPLES if args.example == 'all' else (args.example,):
        runner.example(example, languages)


if __name__ == '__main__':
    main()
