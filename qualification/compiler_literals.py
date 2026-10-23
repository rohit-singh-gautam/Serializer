# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Compile the existing basic consumers against large defaults and verify independent JSON values."""
import argparse
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TEXT_BYTES = 1024
DEFAULT_FRACTION_DIGITS = 128
MAX_LITERAL_BYTES = 1024 * 1024
SPEC = importlib.util.spec_from_file_location('serializer_examples', ROOT / 'example/run.py')
examples = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(examples)


def main():
    """Keep generated source and real SDK execution under the ignored qualification root."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--cpp-library', type=Path)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default='all')
    parser.add_argument('--wsl-languages', default='')
    parser.add_argument('--generate-only', action='store_true')
    parser.add_argument('--text-bytes', type=int, default=DEFAULT_TEXT_BYTES)
    parser.add_argument('--fraction-digits', type=int, default=DEFAULT_FRACTION_DIGITS)
    args = parser.parse_args()
    args.compiler = args.compiler.resolve()
    if args.cpp_library:
        args.cpp_library = args.cpp_library.resolve()
    args.build = args.build.resolve()
    args.sanitize = False
    if not args.build.is_relative_to(ROOT / 'out'):
        parser.error('Qualification output must remain beneath the repository out/ directory')
    if not (0 < args.text_bytes <= MAX_LITERAL_BYTES and 0 < args.fraction_digits <= MAX_LITERAL_BYTES):
        parser.error(f'Each qualification literal size must be between 1 and {MAX_LITERAL_BYTES} bytes')
    args.build.mkdir(parents=True, exist_ok=True)
    languages = examples.LANGUAGES if args.language == 'all' else tuple(args.language.split(','))
    if not languages or len(set(languages)) != len(languages) or any(item not in examples.LANGUAGES for item in languages):
        parser.error('Choose distinct supported output languages')
    source = args.build / 'model.serializer'
    text_size = args.text_bytes
    source.write_text('serializer version 1.14.0; class example_model stable_ids {\n'
                      ' public string text (1) { "' + 'a' * text_size + '" };\n'
                      ' public double number (2) { 1.' + '0' * args.fraction_digits + ' };\n'
                      ' public uint64 marker (3) { 18446744073709551615 };\n}\n', encoding='utf-8')
    targets = {('js' if language == 'javascript' else language): args.build / language / examples.OUTPUTS[language]
               for language in languages}
    if 'typescript' in languages and 'javascript' not in languages:
        targets['js'] = args.build / 'typescript/schema.mjs'
    command = [args.compiler, '--input', source, '--language', ','.join(targets),
               '--cpp.format=false', '--go.package=main', '--kotlin.package=']
    for language, destination in targets.items():
        destination.parent.mkdir(parents=True, exist_ok=True)
        command += [f'--{language}.output', destination]
    examples.run(command)
    if 'typescript' in languages and 'javascript' in languages:
        (args.build / 'typescript/schema.mjs').write_bytes(targets['js'].read_bytes())
    if args.generate_only:
        print('PASS: all requested outputs generated', flush=True)
        return
    runner = examples.Runner(args)
    empty = args.build / 'empty.json'
    empty.write_text('{}', encoding='utf-8')
    expected = {'text': 'a' * text_size, 'number': 1.0, 'marker': 18446744073709551615}
    for language in languages:
        consumer = args.build / 'consumers' / language
        consumer.mkdir(parents=True, exist_ok=True)
        for file in (ROOT / 'example' / language / 'basic').iterdir():
            if file.name not in ('main.cpp', 'main.c', 'Main.java', 'main.mjs', 'main.mts', 'main.go', 'Program.cs', 'main.rs', 'main.py', 'main.swift', 'Main.kt'):
                continue
            text = file.read_text(encoding='utf-8')
            if language == 'go':
                text = text.replace('"serializer.example/go/basic/internal/model"', '').replace('model.', '')
            (consumer / file.name).write_text(text, encoding='utf-8')
        executable = runner.build(language, 'large_defaults', args.build / language, consumer)
        output = args.build / f'{language}.json'
        examples.run([*executable, runner.path(language, empty), runner.path(language, output)])
        actual = json.loads(output.read_text(encoding='utf-8'))
        assert actual == expected, (language, list(actual))
        print(f'PASS: {language}, independent defaults and all four protocols', flush=True)


if __name__ == '__main__':
    main()
