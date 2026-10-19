"""Qualify real prepared XSD providers through generated Java, C# and Kotlin APIs."""
import argparse
import importlib.util
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / 'test/resources/xml_provider'
spec = importlib.util.spec_from_file_location('serializer_provider_sdk', ROOT / 'example/run.py')
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)


def main():
    """Require each selected SDK, compile generated code strictly, and run provider probes."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default='java,csharp,kotlin')
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args()
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    languages = args.language.split(',')
    if not languages or len(languages) != len(set(languages)) or any(
            language not in ('java', 'csharp', 'kotlin') for language in languages):
        parser.error('Select distinct language names from java,csharp,kotlin')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    runner = sdk.Runner(args)
    targets = {}
    for language in languages:
        directory = args.build / language
        directory.mkdir(parents=True, exist_ok=True)
        for source in (SOURCE / language).iterdir():
            if source.is_file():
                shutil.copyfile(source, directory / source.name)
        targets[language] = directory / sdk.OUTPUTS[language]
    command = [args.compiler, '--input', ROOT / 'test/resources/xml_provider.serializer',
               '--config', ROOT / 'test/resources/xml_provider.ini', '--xml.validation_scope', 'both',
               '--language', ','.join(languages), '--protocols', 'binary_none', '--kotlin.package=']
    for language, target in targets.items():
        command += [f'--{language}.output', target]
    sdk.run(command)
    for language in languages:
        launch = runner.build(language, 'xml_provider', args.build / language, SOURCE / language)
        sdk.run([*launch, runner.path(language, ROOT / 'test/resources/xml_xsd_fixture.xsd')])
    print(f'PASS: prepared XSD provider bounds, concurrency and no-fetch ({len(languages)} SDKs)', flush=True)


if __name__ == '__main__':
    main()
