"""Compile the generic schema with real SDKs and exercise dedicated four-protocol consumers."""
import argparse
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('serializer_examples', ROOT / 'example/run.py')
examples = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(examples)


def main():
    """Generate the same closed applications for each target and check independent fixture data."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, default=ROOT / 'out/generic-examples')
    parser.add_argument('--language', default='python')
    parser.add_argument('--wsl-languages', default='')
    parser.add_argument('--cpp-library', type=Path)
    args = parser.parse_args()
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    args.sanitize = False
    languages = examples.LANGUAGES if args.language == 'all' else args.language.split(',')
    if any(language not in examples.LANGUAGES for language in languages):
        parser.error('Choose supported language names')
    runner = examples.Runner(args)
    pair = {'key': 4294967295, 'value': 'generic \u00e9\u0000'}
    box = {'value': pair, 'items': [pair, {'key': 0, 'value': ''}]}
    value = {'revision': 9, 'payload': box, 'lookup': [{'key': 7, 'value': box}]}
    args.build.mkdir(parents=True, exist_ok=True)
    fixture = args.build / 'fixture.json'
    fixture.write_text(json.dumps(value), encoding='utf-8')
    value['revision'] += 1
    # Each consumer edits only the direct payload, not its array or map copies.
    value = json.loads(json.dumps(value))
    value['payload']['value']['key'] = 7
    for language in languages:
        directory = args.build / language
        directory.mkdir(parents=True, exist_ok=True)
        target = 'js' if language == 'javascript' else language
        command = [args.compiler, '--input', ROOT / 'example' / language / 'generics/model.serializer',
                   '--language', target, '--output', directory / examples.OUTPUTS[language],
                   '--cpp.format', 'false', '--go.package', 'main', '--kotlin.package=']
        examples.run(command)
        if language == 'typescript':
            examples.run([args.compiler, '--input', ROOT / 'example' / language / 'generics/model.serializer',
                          '--language', 'js', '--output', directory / 'schema.mjs'])
        consumer = runner.build(language, 'generics', directory)
        output = directory / 'result.json'
        examples.run([*consumer, runner.path(language, fixture), runner.path(language, output)])
        if json.loads(output.read_text(encoding='utf-8')) != value:
            raise AssertionError(f'{language}: generic data differs from the independent fixture')
    print(f'PASS: {len(languages)} generic consumers, all four protocols', flush=True)


if __name__ == '__main__':
    main()
