# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile real native dependency bundles and the maintained portable construction consumers."""
import argparse
import importlib.util
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]


def load(name, path):
    """Load maintained SDK helpers without copying their toolchain policy."""
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    """Generate, compile and run each explicitly requested SDK sequentially."""
    examples = load('layout_examples', ROOT / 'example/run.py')
    consumers = load('layout_consumers', ROOT / 'test/construction_languages.py')
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--languages', default='c,rust,python,swift,kotlin')
    parser.add_argument('--wsl-languages', default='')
    args = parser.parse_args()
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    args.cpp_library = None
    args.sanitize = False
    args.build.mkdir(parents=True, exist_ok=True)
    dependency = ROOT / 'test/resources/construction.serializer'
    schema = args.build / 'entry.serializer'
    relative = Path(os.path.relpath(dependency, schema.parent)).as_posix()
    schema.write_text('serializer version 1.24.0;\ninclude ' + relative + ';\n'
                      'class layout_root { public string label {"fileprivate value"}; }\n', encoding='utf-8')
    runner = examples.Runner(args)
    for language in args.languages.split(','):
        directory = args.build / language
        directory.mkdir(parents=True, exist_ok=True)
        output = directory / examples.OUTPUTS[language]
        depfile = directory / 'schema.d'
        examples.run([args.compiler, '--input', schema, '--language', language, '--output', output,
                      '--include-mode', 'separate', '--depfile', depfile])
        linked = [output.parent / line.split(' Serializer output: ', 1)[1]
                  for line in output.read_text(encoding='utf-8').splitlines()
                  if line.startswith(('// Serializer output: ', '# Serializer output: '))]
        generated = [output, *(path for path in linked if path.suffix == output.suffix)]
        if len(generated) < 2:
            raise AssertionError('Separate mode did not generate dependency source files: ' + language)
        for path in generated:
            if path.name != '__init__.py' and path != output and str(path).replace('\\', '/') not in depfile.read_text().replace('\\:', ':'):
                raise AssertionError('Depfile omitted generated source: ' + str(path))
        source = args.build / 'consumers' / language
        source.mkdir(parents=True, exist_ok=True)
        main_source = source / consumers.FILES[language]
        main_source.write_text(consumers.consumer(language), encoding='utf-8')
        if language in ('swift', 'kotlin'):
            copied = directory / main_source.name
            copied.write_bytes(main_source.read_bytes())
            paths = [runner.path(language, path) for path in [*generated, copied]]
            if language == 'swift':
                executable = runner.path(language, directory / ('main.exe' if os.name == 'nt' else 'main'))
                examples.run(runner.command(language, 'swiftc', '-O', '-j', '1', '-warnings-as-errors', *paths, '-o', executable))
                examples.run(runner.launch(language, executable))
            else:
                jar = runner.path(language, directory / 'main.jar')
                examples.run(runner.command(language, 'kotlinc', *paths, '-Werror', '-include-runtime', '-d', jar))
                examples.run(runner.command(language, 'java', '-jar', jar))
        else:
            examples.run(runner.build(language, 'native_file_layout', directory, source))
        if language == 'python':
            examples.run([sys.executable, '-c',
                "import importlib, sys; sys.path.insert(0, sys.argv[1]); "
                "model = importlib.import_module('python.schema'); "
                "assert model.LayoutRoot().label == 'fileprivate value'", args.build])
        if '"fileprivate value"' not in output.read_text(encoding='utf-8'):
            raise AssertionError('Splitting changed an authored string default')
        print('PASS: ' + language + ' dependency layout, direct construction, defaults and wire round trips', flush=True)


if __name__ == '__main__':
    main()
