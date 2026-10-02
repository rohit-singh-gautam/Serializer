"""Qualify managed wire records across native codecs; this does not implement managed engines."""
import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
FEATURES = ('history', 'journal', 'collaboration', 'authentication')


def load_module(name, path):
    """Import the existing SDK runner or a freshly generated Python oracle."""
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


examples = load_module('serializer_examples', ROOT / 'example/run.py')


class Runner(examples.Runner):
    """Translate the build root once per WSL target instead of spawning per matrix edge."""

    def __init__(self, args):
        """Keep SDK selection in the existing runner and cache only verified path roots."""
        super().__init__(args)
        self.mapped_roots = {}

    def path(self, language, path):
        """Derive child paths from the WSL translation of their actual build root."""
        absolute = Path(path).resolve()
        if language in self.wsl and absolute.is_relative_to(self.args.build):
            if language not in self.mapped_roots:
                self.mapped_roots[language] = super().path(language, self.args.build)
            return self.mapped_roots[language].rstrip('/') + '/' + absolute.relative_to(
                self.args.build).as_posix()
        return super().path(language, path)


def qualify(args, feature, languages):
    """Check each producer/consumer pair against independent expected record fields."""
    runner = Runner(args)
    directory = args.build / feature
    directory.mkdir(parents=True, exist_ok=True)
    selected = list(languages)
    # Python supplies an exact, structured oracle even when it is not a participant.
    targets = {('js' if language == 'javascript' else language):
               directory / language / examples.OUTPUTS[language] for language in selected}
    targets.setdefault('python', directory / 'oracle/schema.py')
    if 'typescript' in selected and 'js' not in targets:
        targets['js'] = directory / 'typescript/schema.mjs'
    for target in targets.values():
        target.parent.mkdir(parents=True, exist_ok=True)
    command = [args.compiler, '--input', HERE / feature / 'model.serializer',
               '--language', ','.join(targets), '--cpp.format', 'false',
               '--go.package', 'main', '--kotlin.package=']
    for language, target in targets.items():
        command += [f'--{language}.output', target]
    examples.run(command)
    if 'typescript' in selected and 'javascript' in selected:
        shutil.copyfile(targets['js'], directory / 'typescript/schema.mjs')
    commands = {language: runner.build(language, feature, directory / language,
                                       HERE / 'clients' / language) for language in selected}
    schema = load_module('managed_' + feature, targets['python'])
    fixture = json.loads((HERE / feature / 'fixture.json').read_text(encoding='utf-8'))
    model = schema.ExampleModel.decode(json.dumps(fixture).encode(), schema.Protocol.JSON)
    initial = directory / 'initial.bin'
    initial.write_bytes(model.encode(schema.Protocol.BINARY_INTEGER))

    def verify(path, revision):
        """Compare all runtime metadata, byte payloads and full-width integer values."""
        actual = schema.ExampleModel.decode(path.read_bytes(), schema.Protocol.BINARY_INTEGER)
        expected = dict(fixture, revision=revision)
        if json.loads(actual.encode(schema.Protocol.JSON)) != expected:
            raise AssertionError(f'{feature}: unexpected fields in {path}')

    for language, command in commands.items():
        produced = directory / (language + '.bin')
        examples.run([*command, runner.path(language, initial), runner.path(language, produced)])
        verify(produced, 1)
    for producer in selected:
        for consumer, command in commands.items():
            output = directory / (producer + '_to_' + consumer + '.bin')
            examples.run([*command, runner.path(consumer, directory / (producer + '.bin')),
                          runner.path(consumer, output)])
            verify(output, 2)
    # Exact-message rejection is independent of the positive fixture oracle.
    malformed = directory / 'malformed.bin'
    rejected = directory / 'rejected.bin'
    for payload in (initial.read_bytes()[:-1], initial.read_bytes() + b'\0'):
        malformed.write_bytes(payload)
        for language, command in commands.items():
            if rejected.exists():
                rejected.unlink()
            result = subprocess.run([*command, runner.path(language, malformed),
                                     runner.path(language, rejected)],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60)
            if result.returncode == 0 or rejected.exists():
                raise AssertionError(f'{feature}/{language}: invalid record published an output')
    print(f'PASS {feature}: {len(selected)} individual codec checks, '
          f'{len(selected) ** 2} producer/consumer exchanges, '
          f'{2 * len(selected)} malformed-input checks', flush=True)


def main():
    """Expose explicit SDK selection without silently skipping unavailable languages."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--cpp-library', type=Path)
    parser.add_argument('--build', type=Path, default=ROOT / 'out/managed_multilanguage')
    parser.add_argument('--language', default='all')
    parser.add_argument('--feature', choices=(*FEATURES, 'all'), default='all')
    parser.add_argument('--wsl-languages', default='')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    languages = examples.LANGUAGES if args.language == 'all' else tuple(args.language.split(','))
    if (not languages or len(set(languages)) != len(languages) or
            any(language not in examples.LANGUAGES for language in languages)):
        parser.error('Choose distinct supported language folder names')
    for feature in FEATURES if args.feature == 'all' else (args.feature,):
        qualify(args, feature, languages)


if __name__ == '__main__':
    main()
