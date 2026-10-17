"""Compile and execute a separate Kotlin model whose XML fields remain private."""
import argparse
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / 'test/resources/xml_kotlin_private'
spec = importlib.util.spec_from_file_location('serializer_private_xml_examples', ROOT / 'example/run.py')
examples = importlib.util.module_from_spec(spec)
spec.loader.exec_module(examples)


def main():
    """Generate fresh Kotlin, compile with warnings as errors, and check private-field values."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--wsl-languages', choices=('', 'kotlin'), default='')
    args = parser.parse_args()
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    examples.run([args.compiler, '--input', SOURCE / 'model.serializer', '--config', SOURCE / 'options.ini',
                  '--language', 'kotlin', '--kotlin.package=', '--kotlin.output', args.build / 'Schema.kt'])
    runner = examples.Runner(args)
    consumer = runner.build('kotlin', 'xml-private', args.build, SOURCE)
    examples.run(consumer)


if __name__ == '__main__':
    main()
