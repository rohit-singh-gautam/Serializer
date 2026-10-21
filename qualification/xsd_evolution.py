# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify staged XSD authoring and stable native models through eleven actual SDKs."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
ADAPTERS = Path(__file__).resolve().with_suffix('')
sys.path.insert(0, str(ADAPTERS))
import oracle

LANGUAGES = ('cpp', 'python', 'c', 'rust', 'go', 'java', 'javascript', 'typescript', 'csharp', 'kotlin', 'swift')
VARIANTS = ('v1', 'v2')


def sha(path):
    """Pin complete source, SDK, provider and executable bytes."""
    checksum = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            checksum.update(block)
    return checksum.hexdigest()


def source_snapshot(schema_directory=None):
    """Watch the maintained producers, fixed schemas and imported build inputs during execution."""
    paths = [Path(__file__), ROOT / 'test/xsd_import_cli_test.py', ROOT / 'test/xsd_evolution_guard_test.py',
             ROOT / 'test/package_node_dependencies.py',
             ROOT / 'test/resources/xml_shapes/typescript/package.json',
             ROOT / 'test/resources/xml_shapes/typescript/package-lock.json',
             ROOT / 'example/xml/rust/Cargo.toml', ROOT / 'example/xml/rust/Cargo.lock']
    paths += sorted(path for path in ADAPTERS.rglob('*') if path.is_file()
                    and path.suffix != '.md' and '__pycache__' not in path.parts)
    result = {path.relative_to(ROOT).as_posix(): sha(path) for path in paths}
    if schema_directory:
        selected = Path(schema_directory).resolve(strict=True)
        result.update({'schema_override/' + path.relative_to(selected).as_posix(): sha(path)
                       for path in sorted(selected.rglob('*')) if path.is_file()
                       and path.suffix in ('.serializer', '.xsd', '.ini')})
    return result


def linked_artifacts(path, run, label):
    """Pin a selected library and its resolved Unix dependencies without interpreting version strings."""
    path = Path(path).resolve(strict=True)
    artifacts = {str(path): sha(path)}
    if os.name != 'nt':
        dependencies = run(['ldd', path], label)
        assert 'not found' not in dependencies, 'Unresolved provider dependency'
        for value in re.findall(r'=>\s+(/\S+)', dependencies):
            library = Path(value).resolve(strict=True)
            artifacts[str(library)] = sha(library)
    return artifacts


def merge_artifacts(captures):
    """Retain the earliest observation and reject conflicting pins across separate builds."""
    artifacts, conflicts = {}, set()
    for capture in captures:
        for path, digest in capture.items():
            key = os.path.normcase(str(Path(path).resolve()))
            if key in artifacts and artifacts[key] != digest:
                conflicts.add(key)
            else:
                artifacts[key] = digest
    return artifacts, conflicts


def build_local(language, compiler, directory, options, run):
    """Select a strict CMake-built C++ consumer or generate the public Python model directly."""
    variant = options['variant']
    if language == 'cpp':
        executable = Path(options['cpp_' + variant + '_producer']).resolve(strict=True)
        generated = Path(options['cpp_' + variant + '_generated']).resolve(strict=True)
        artifacts = {str(executable): sha(executable)}
        if os.name == 'nt':
            libraries = executable.parent.glob('*.dll')
        else:
            dependencies = run(['ldd', executable], 'cpp-linked-libraries')
            assert 'not found' not in dependencies
            libraries = [Path(value).resolve(strict=True)
                         for value in re.findall(r'=>\s+(/\S+)', dependencies)]
        artifacts.update({str(path): sha(path) for path in libraries})
        return {'launch': [str(executable)], 'generated': [str(generated)],
                'tool': 'CMake strict C++20 ' + platform.platform(), 'artifacts': artifacts}
    generated = directory / 'schema.py'
    schema_directory = Path(options.get('schema_directory') or ADAPTERS)
    run([compiler, '--input', schema_directory / variant / 'order.serializer', '--language', 'python',
         '--output', generated], 'python-generation')
    shutil.copyfile(ADAPTERS / 'python_consumer.py', directory / 'main.py')
    artifacts = {str(Path(sys.executable).resolve()): sha(sys.executable)}
    copied_consumer = directory / 'main.py'
    artifacts[str(copied_consumer)] = sha(copied_consumer)
    if os.name == 'nt':
        library = Path(sys.base_prefix) / f'python{sys.version_info.major}{sys.version_info.minor}.dll'
        artifacts[str(library)] = sha(library)
    else:
        artifacts.update(linked_artifacts(sys.executable, run, 'python-runtime-dependencies'))
    import pyexpat
    import xml.parsers.expat
    wrapper = Path(xml.parsers.expat.__file__).resolve(strict=True)
    artifacts[str(wrapper)] = sha(wrapper)
    artifacts.update(linked_artifacts(getattr(pyexpat, '__file__', None) or sys.executable,
                                     run, 'python-expat-dependencies'))
    return {'launch': [sys.executable, '-B', str(directory / 'main.py')],
            'generated': [str(generated)], 'tool': sys.version, 'artifacts': artifacts}


def prepare(language, variant, compiler, directory, options, run):
    """Build one separate generated library/consumer pair and retain its exact public output."""
    directory.mkdir(parents=True, exist_ok=True)
    selected = dict(options, variant=variant)
    if language in ('cpp', 'python'):
        result = build_local(language, compiler, directory, selected, run)
    else:
        module_name = ('native_build' if language in ('c', 'rust', 'go') else
                       'managed_build' if language in ('csharp', 'swift') else 'jvm_js_build')
        module = __import__(module_name)
        result = module.build(language, ROOT, compiler, directory, selected, run)
    result['generated_sources_sha256'] = {str(path): sha(path) for path in result['generated']}
    result['status'] = 'passed'
    return result


def literal_emission(variant):
    """Derive the mandatory evidence inventory from independent native/XML fixture literals."""
    expected = oracle.manifest()
    prefix = '{' + expected['namespace'] + '}'
    return {'native_sha256': {name: hashlib.sha256(bytes.fromhex(value)).hexdigest()
                              for name, value in expected['native_hex'].items()},
            'xml_projection': {'root': prefix + expected['root'], 'attributes': {'id': '7'},
                               'children': [[prefix + name, value] for name, value in expected['xml_children'][variant]]}}


def compare_hosts(current, previous):
    """Admit complete clean inventories and compare independently checked wire/XML contracts."""
    for report in (current, previous):
        assert report['status'] == 'passed' and report['full_inventory']
        assert set(report['languages']) == set(LANGUAGES)
        assert report['source_verification'] == {'changed': [], 'compiler_unchanged': True, 'changed_artifacts': []}
        for language in LANGUAGES:
            row = report['languages'][language]
            assert set(row) == set(VARIANTS)
            for variant in VARIANTS:
                assert row[variant]['status'] == 'passed'
                literal = literal_emission(variant)
                assert row[variant]['emission']['native_sha256'] == literal['native_sha256']
                assert row[variant]['emission']['xml']['projection'] == literal['xml_projection']
        assert report['decisions'] == {'emissions': 22, 'native_exchanges': 484,
                                        'xml_exchanges': 242, 'rejected_xml': 44}
    assert current['qualification_sources_sha256'] == previous['qualification_sources_sha256']
    compared = []
    for language in LANGUAGES:
        for variant in VARIANTS:
            left, right = current['languages'][language][variant], previous['languages'][language][variant]
            assert left['emission']['native_sha256'] == right['emission']['native_sha256']
            assert left['emission']['xml']['projection'] == right['emission']['xml']['projection']
            compared.append({'language': language, 'variant': variant, 'native_bytes_equal': True,
                             'xml_projection_equal': True,
                             'xml_bytes_equal': left['emission']['xml']['sha256'] == right['emission']['xml']['sha256']})
    return compared


def main():
    """Run every SDK without silently skipping missing tools, then publish a guarded evidence record."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True)
    parser.add_argument('--build', required=True)
    parser.add_argument('--languages', nargs='+', choices=LANGUAGES, default=list(LANGUAGES))
    parser.add_argument('--jobs', type=int, default=3)
    parser.add_argument('--interop-jobs', type=int, default=6)
    parser.add_argument('--compare-to', type=Path)
    parser.add_argument('--schema-directory', type=Path,
                        help='Rebind the same native v1/v2 fixtures to a separately watched XSD directory.')
    for name in ('cpp-v1-producer', 'cpp-v2-producer', 'cpp-v1-generated', 'cpp-v2-generated',
                 'cc', 'provider-prefix', 'cargo', 'go', 'java', 'javac', 'kotlinc', 'node',
                 'node-dependencies', 'typescript-cli', 'dotnet', 'swiftc'):
        parser.add_argument('--' + name)
    args = parser.parse_args()
    assert args.jobs > 0 and args.interop_jobs > 0
    directory = Path(args.build).resolve()
    assert ROOT in directory.parents and 'out' in directory.relative_to(ROOT).parts
    assert not directory.exists() or not any(directory.iterdir()), 'Use a fresh evidence directory'
    directory.mkdir(parents=True, exist_ok=True)
    compiler = Path(args.compiler).resolve(strict=True)
    compiler_before, sources_before = sha(compiler), source_snapshot(args.schema_directory)
    commands, failures = [], []

    def run(argv, label, *, cwd=None, env=None, success=True):
        """Retain exact argv, actual diagnostics and exit decisions for every SDK process."""
        argv = [str(value) for value in argv]
        start = time.monotonic()
        environment = dict(os.environ)
        environment.update(env or {})
        result = subprocess.run(argv, cwd=cwd or ROOT, env=environment, capture_output=True, text=True, errors='replace')
        commands.append({'label': label, 'argv': argv, 'cwd': str(cwd or ROOT), 'returncode': result.returncode,
                         'duration_seconds': time.monotonic() - start, 'stdout': result.stdout, 'stderr': result.stderr})
        assert (result.returncode == 0) == success, label + '\n' + result.stdout + result.stderr
        return result.stdout + result.stderr

    result = {'status': 'failed', 'full_inventory': set(args.languages) == set(LANGUAGES),
              'platform': platform.platform(), 'compiler_sha256': compiler_before,
              'qualification_sources_sha256': sources_before, 'languages': {}, 'failures': failures,
              'commands': commands}
    try:
        result['oracle_controls'] = oracle.self_test()
        validators = {variant: oracle.schema_validator(variant) for variant in VARIANTS}
        from lxml import etree
        result['independent_xml_provider'] = {'lxml': etree.LXML_VERSION, 'libxml2': etree.LIBXML_VERSION,
                                             'artifacts': linked_artifacts(etree.__file__, run, 'oracle-provider-dependencies')}
        run([sys.executable, '-B', ROOT / 'test/xsd_import_cli_test.py', compiler, directory], 'staged-xsd-cli')
    except Exception as error:
        failures.append({'setup': repr(error)})
    jobs = [(language, variant) for language in dict.fromkeys(args.languages) for variant in VARIANTS] if not failures else []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = {pool.submit(prepare, language, variant, compiler, directory / language / variant,
                               vars(args), run): (language, variant) for language, variant in jobs}
        for future in as_completed(futures):
            language, variant = futures[future]
            try:
                row = future.result()
                output = directory / language / variant / 'emitted'
                output.mkdir()
                run([*row['launch'], variant, 'emit', output], language + '-' + variant + '-emit',
                    env=row.get('environment'))
                row['emission'] = {'directory': str(output),
                                   'xml': oracle.inspect_xml(output / 'authored.xml', variant, validators[variant]),
                                   'native_sha256': {name: oracle.inspect_native(output / name, name)
                                                     for name in oracle.manifest()['native_hex']}}
                result['languages'].setdefault(language, {})[variant] = row
                print(language, variant, 'built/emitted/independently checked', flush=True)
            except Exception as error:
                failures.append({'language': language, 'variant': variant, 'error': repr(error)})
    decisions = {'emissions': sum(len(row) for row in result['languages'].values()),
                 'native_exchanges': 0, 'xml_exchanges': 0, 'rejected_xml': 0}

    def exchange(reader_language, reader_variant, producer_language, producer_variant, mode):
        """Exercise actual cross-language readers, then independently validate their public output."""
        reader = result['languages'][reader_language][reader_variant]
        producer = Path(result['languages'][producer_language][producer_variant]['emission']['directory'])
        name = '-'.join((reader_language, reader_variant, producer_language, producer_variant, mode))
        output = directory / 'exchanges' / (name + ('.xml' if mode == 'check-native' else '.bin'))
        source = producer / ('authored.bin' if mode == 'check-native' else 'authored.xml')
        run([*reader['launch'], reader_variant, mode, source, output], name, env=reader.get('environment'))
        if mode == 'check-native':
            oracle.inspect_xml(output, reader_variant, validators[reader_variant])
            return 'native_exchanges'
        oracle.inspect_native(output, 'xml-projection.bin')
        return 'xml_exchanges'

    if not failures:
        (directory / 'exchanges').mkdir()
        exchanges = [(rl, rv, pl, pv, mode) for rl, rv in jobs for pl, pv in jobs
                     for mode in ('check-native', 'check-xml') if mode == 'check-native' or rv == pv]
        with ThreadPoolExecutor(max_workers=args.interop_jobs) as pool:
            futures = {pool.submit(exchange, *decision): decision for decision in exchanges}
            for future in as_completed(futures):
                try:
                    decisions[future.result()] += 1
                except Exception as error:
                    failures.append({'exchange': futures[future], 'error': repr(error)})
        for language, variant in jobs:
            row = result['languages'][language][variant]
            other = 'v2' if variant == 'v1' else 'v1'
            other_xml = Path(result['languages'][language][other]['emission']['directory']) / 'authored.xml'
            own_xml = Path(row['emission']['directory']) / 'authored.xml'
            wrong_namespace = directory / 'exchanges' / (language + '-' + variant + '-wrong-namespace.xml')
            wrong_namespace.write_bytes(own_xml.read_bytes().replace(b'urn:serializer:xsd-evolution', b'urn:wrong'))
            for label, source in (('wrong-variant', other_xml), ('wrong-namespace', wrong_namespace)):
                output = directory / 'exchanges' / (language + '-' + variant + '-' + label + '.bin')
                try:
                    run([*row['launch'], variant, 'check-xml', source, output], language + '-' + variant + '-' + label,
                        env=row.get('environment'), success=False)
                    assert not output.exists(), 'Failed decode published an output'
                    decisions['rejected_xml'] += 1
                except Exception as error:
                    failures.append({'rejected_xml': str(source), 'error': repr(error)})
    captures = [value[group] for row in result['languages'].values() for value in row.values()
                for group in ('artifacts', 'generated_sources_sha256')]
    captures.append(result.get('independent_xml_provider', {}).get('artifacts', {}))
    artifacts, conflicts = merge_artifacts(captures)
    changed_artifacts = sorted(conflicts | {path for path, digest in artifacts.items()
                                          if not Path(path).is_file() or sha(path) != digest})
    after = source_snapshot(args.schema_directory)
    result['source_verification'] = {'changed': sorted(name for name in after.keys() | sources_before.keys()
                                                      if after.get(name) != sources_before.get(name)),
                                     'compiler_unchanged': sha(compiler) == compiler_before,
                                     'changed_artifacts': changed_artifacts}
    result['decisions'] = decisions
    clean = result['source_verification'] == {'changed': [], 'compiler_unchanged': True, 'changed_artifacts': []}
    if not failures and clean:
        result['status'] = 'passed' if result['full_inventory'] else 'diagnostic_passed'
        if args.compare_to:
            try:
                result['cross_host'] = compare_hosts(result, json.loads(args.compare_to.read_text(encoding='utf-8')))
            except Exception as error:
                failures.append({'cross_host': repr(error)})
                result['status'] = 'failed'
    output = directory / 'verification.json'
    output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'status': result['status'], 'decisions': decisions, 'failures': failures,
                      'report_sha256': sha(output)}, indent=2))
    return 0 if result['status'] in ('passed', 'diagnostic_passed') else 1


if __name__ == '__main__':
    sys.exit(main())
