# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify backend default locations, included/generic provenance and atomic publication."""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
EXTENSIONS = {'js': 'mjs', 'typescript': 'd.ts', 'go': 'go', 'csharp': 'cs',
              'rust': 'rs', 'python': 'py', 'swift': 'swift', 'kotlin': 'kt',
              'c': 'h', 'java': 'java'}
SENTINEL = b'previous generated artifact\n'


def invoke(compiler, source, directory, language=None):
    """Generate C++ and the selected portable output in one atomic compiler invocation."""
    selected = 'cpp' + (',' + language if language else '')
    if language == 'typescript':
        selected += ',js'
    command = [compiler, '--input', source, '--language', selected,
               '--cpp.output', directory / 'model.hpp', '--cpp.format', 'false', '--cpp.naming', 'preserve',
               '--depfile', directory / 'model.d']
    if language:
        command += ['--' + language + '.output', directory / ('Schema.' + EXTENSIONS[language])]
    if language == 'typescript':
        command += ['--js.output', directory / 'Schema.mjs']
    return subprocess.run([str(item) for item in command], capture_output=True, text=True,
                          encoding='utf-8', timeout=60)


def fixture(directory, case):
    """Retain the original declaring file even when a newer wrapper instantiates an older generic."""
    root = directory / 'root.serializer'
    declaration = directory / 'legacy.serializer'
    text = 'serializer version 1.13.0;\nclass sample {\n  public uint8 value {999};\n}\n'
    line, field = 3, 'value'
    if case == 'floating':
        text = text.replace('uint8 value {999}', 'double value {1e400}')
    elif case == 'enum':
        text = 'serializer version 1.13.0;\nenum kind { first }\nclass sample {\n  public kind value {other::first};\n}\n'
        line = 4
    elif case == 'escape':
        text = 'serializer version 1.14.0;\nclass sample {\n  public string value {"bad\\q"};\n}\n'
    elif case == 'collection':
        text = text.replace('uint8 value {999}', 'array[2] uint32 values {1,2}')
        field = 'values'
    elif case == 'generic':
        text = 'serializer version 1.13.0;\nclass holder<T> {\n  public T value {999};\n}\ninstantiate tiny = holder<uint8>;\n'
    elif case == 'transitive_generic':
        text = 'serializer version 1.13.0;\nclass holder<T> {\n  public T value {999};\n}\n'
    if case in ('include', 'transitive_generic'):
        declaration.write_text(text, encoding='utf-8')
        if case == 'include':
            root.write_text('serializer version 1.23.0;\ninclude legacy;\n', encoding='utf-8')
        else:
            (directory / 'middle.serializer').write_text('serializer version 1.23.0;\ninclude legacy;\n', encoding='utf-8')
            root.write_text('serializer version 1.23.0;\ninclude middle;\ninstantiate tiny = holder<uint8>;\n', encoding='utf-8')
    else:
        declaration = root
        root.write_text(text, encoding='utf-8')
    return root, f'{declaration.resolve().as_posix()}:{line}:3: member {field}: '


def execute(compiler, directory, baseline=None):
    """Keep rejection reasons, output contents and timestamps while adding precise authored locations."""
    checks = 0
    for case in ('scalar', 'floating', 'enum', 'escape', 'collection', 'generic', 'include', 'transitive_generic'):
        for language, extension in EXTENSIONS.items():
            work = directory / case / language
            work.mkdir(parents=True)
            source, prefix = fixture(work, case)
            outputs = [work / 'model.hpp', work / 'model.d', work / ('Schema.' + extension)]
            if language == 'typescript':
                outputs.append(work / 'Schema.mjs')
            for output in outputs:
                output.write_bytes(SENTINEL)
            before = {path: (path.read_bytes(), path.stat().st_mtime_ns) for path in outputs}
            result = invoke(compiler, source, work, language)
            diagnostic = result.stdout + result.stderr
            if result.returncode == 0 or diagnostic.count(prefix) != 1:
                raise AssertionError(f'Missing/duplicate {case}/{language} declaration location: {diagnostic}')
            if baseline:
                previous = invoke(baseline, source, work, language)
                reason = (previous.stdout + previous.stderr).strip().removeprefix('Serializer: ')
                if previous.returncode == 0 or not diagnostic.strip().endswith(reason):
                    raise AssertionError(f'Existing rejection reason changed: {case}/{language}: {diagnostic} versus {reason}')
            for output, state in before.items():
                if (output.read_bytes(), output.stat().st_mtime_ns) != state:
                    raise AssertionError(f'Rejected generation modified {output}')
            # These older/opaque C++ initializers retain their previous acceptance.
            control = invoke(compiler, source, work)
            if control.returncode:
                raise AssertionError(f'C++ native acceptance changed: {case}: {control.stdout}{control.stderr}')
            checks += 1
    if baseline:
        source = directory / 'valid.serializer'
        source.write_text('serializer version 1.14.0; enum kind { first, second } '
                          'class sample { public uint8 value {255}; public double number {1.25}; '
                          'public string text {"valid"}; public kind choice {kind::second}; }', encoding='utf-8')
        for language in EXTENSIONS:
            old, new = directory / ('old-' + language), directory / ('new-' + language)
            old.mkdir(); new.mkdir()
            for compiler_path, work in ((baseline, old), (compiler, new)):
                result = invoke(compiler_path, source, work, language)
                if result.returncode:
                    raise AssertionError(result.stdout + result.stderr)
            for path in old.iterdir():
                if path.name.endswith('.d'):
                    continue  # Dependency paths intentionally name each output directory.
                if path.read_bytes() != (new / path.name).read_bytes():
                    raise AssertionError('Accepted generated bytes changed: ' + path.name)
    print(f'PASS: {checks} located backend rejections, atomic artifacts and native C++ controls; '
          f'baseline reason/output comparisons {"enabled" if baseline else "not requested"}', flush=True)


def main():
    """Place every transient schema and generated file below the explicitly selected ignored build root."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--baseline-compiler', type=Path)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    compiler, build = args.compiler.resolve(), args.build.resolve()
    if not build.is_relative_to(ROOT / 'out'):
        parser.error('Qualification output must remain beneath the repository out/ directory')
    build.mkdir(parents=True, exist_ok=True)
    baseline = args.baseline_compiler.resolve() if args.baseline_compiler else None
    with tempfile.TemporaryDirectory(prefix='backend defaults-', dir=build) as temporary:
        execute(compiler, Path(temporary), baseline)


if __name__ == '__main__':
    main()
