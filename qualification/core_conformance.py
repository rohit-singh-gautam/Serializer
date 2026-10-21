# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Replay the pinned official Core-only 3MF corpus through an independent unchanged-XSD oracle."""
import argparse
import ctypes
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import unquote
import xml.etree.ElementTree as etree
import zipfile

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / 'test/resources/3mf_core_1_4'
REVISION = 'f483d3beee069e7adbac125d1f7baa36b5b3e5bf'
CORE = 'http://schemas.microsoft.com/3dmanufacturing/core/2015/02'
MAX_INPUT_BYTES = 16 * 1024 * 1024
OUTPUTS = {'cpp': 'core.hpp', 'python': 'core.py', 'c': 'core.h', 'csharp': 'Schema.cs',
           'java': 'Schema.java', 'kotlin': 'Schema.kt', 'go': 'schema.go', 'rust': 'schema.rs',
           'swift': 'Schema.swift', 'javascript': 'schema.mjs', 'typescript': 'schema.d.mts'}


def digest(path):
    """Hash input bytes incrementally without making corpus size a process-memory requirement."""
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def model_entries(package):
    """Discover arbitrary MIME-declared model names; retain identifiable payloads from package-invalid negatives."""
    overrides, defaults = {}, {}
    try:
        types = etree.fromstring(package.read('[Content_Types].xml'))
        for item in types:
            if item.tag == '{http://schemas.openxmlformats.org/package/2006/content-types}Override':
                overrides[unquote(item.get('PartName', '')).lstrip('/').lower()] = item.get('ContentType')
            elif item.tag == '{http://schemas.openxmlformats.org/package/2006/content-types}Default':
                defaults[item.get('Extension', '').lower()] = item.get('ContentType')
    except (KeyError, etree.ParseError):
        pass
    return [item for item in package.infolist() if item.filename.lower().endswith('.model')
            or overrides.get(item.filename.lower(), defaults.get(item.filename.rsplit('.', 1)[-1].lower()))
            == 'application/vnd.ms-package.3dmanufacturing-3dmodel+xml']


def run(arguments, **options):
    """Require actual successful compiler/provider execution and retain subprocess diagnostics."""
    if os.name == 'nt':
        ctypes.windll.kernel32.SetErrorMode(0x8003)
    result = subprocess.run([str(item) for item in arguments], capture_output=True, timeout=300, **options)
    if result.returncode:
        raise RuntimeError(result.stderr.decode('utf-8', errors='replace') + result.stdout.decode('utf-8', errors='replace'))
    return result


def check_oracle_controls(build, executable):
    """Reject a foreign root and external DTD while admitting the known valid local Core fixture."""
    foreign = build / 'oracle-foreign-root.xml'
    foreign.write_text('<foreign xmlns="urn:foreign"/>', encoding='utf-8')
    doctype = build / 'oracle-doctype.xml'
    doctype.write_text('<!DOCTYPE model SYSTEM "https://example.invalid/probe">'
        '<model xmlns="' + CORE + '"><resources/><build/></model>', encoding='utf-8')
    listing = build / 'oracle-controls.txt'
    listing.write_text('\n'.join(map(str, (foreign, doctype, FIXTURE / 'minimal.model'))), encoding='utf-8')
    output = run(['dotnet', executable, FIXTURE / 'core.xsd', FIXTURE / 'xml.xsd', listing, '--xml'])
    records = list(map(json.loads, output.stdout.decode('utf-8-sig').splitlines()))
    (build / 'oracle-controls.json').write_text(json.dumps(records, indent=2), encoding='utf-8')
    if [row['valid'] for row in records] != [False, False, True]:
        raise ValueError('Independent oracle failed root/DTD/positive controls')


def inventory(official):
    """Identify the exact upstream corpus without treating its semantic negative labels as XSD verdicts."""
    revision = run(['git', '-C', official, 'rev-parse', 'HEAD']).stdout.decode().strip()
    if revision != REVISION:
        raise ValueError('Official corpus revision differs from the pinned qualification contract')
    archives = sorted((official / 'suite3_core').rglob('*.3mf'))
    if len(archives) != 175:
        raise ValueError('The complete Suite3 Core corpus requires exactly 175 archives')
    rows = [{'path': str(path.relative_to(official)).replace('\\', '/'),
             'sha256': digest(path), 'bytes': path.stat().st_size,
             'upstream': 'positive' if path.name.startswith('P_') else 'negative'} for path in archives]
    pinned = json.loads((FIXTURE / 'official_suite3_manifest.json').read_text(encoding='utf-8'))
    if [(row['path'], row['sha256']) for row in rows] != [
            (row['path'], row['sha256']) for row in pinned['archives']]:
        raise ValueError('Official archive bytes differ from the checked-in complete corpus manifest')
    if digest(FIXTURE / 'core.xsd') != pinned['schema_sha256'] or digest(FIXTURE / 'xml.xsd') != pinned['xml_schema_sha256']:
        raise ValueError('Pinned original Core/XML schema bytes changed')
    return rows


def oracle(official, rows, build, executable):
    """Use the independent .NET XSD1.0 validator on original ZIP entry streams, including giant models."""
    import xmlschema
    if xmlschema.__version__ != '4.2.0':
        raise ValueError('Secondary independent XSD provider is pinned to xmlschema 4.2.0')
    secondary = xmlschema.XMLSchema(FIXTURE / 'core.xsd')
    check_oracle_controls(build, executable)
    listing = build / 'archives.txt'
    listing.write_text('\n'.join(str(official / row['path']) for row in rows), encoding='utf-8')
    result = run(['dotnet', executable, FIXTURE / 'core.xsd', FIXTURE / 'xml.xsd', listing])
    (build / 'oracle-native.jsonl').write_bytes(result.stdout)
    records = {(str(Path(row['archive']).resolve()), row['part']): row
               for row in map(json.loads, result.stdout.decode('utf-8-sig').splitlines())}
    results = []
    for index, archive in enumerate(rows):
        row = dict(archive, models=[])
        with zipfile.ZipFile(official / archive['path']) as package:
            for entry in model_entries(package):
                item = {'part': entry.filename, 'bytes': entry.file_size}
                with package.open(entry) as source:
                    item['sha256'] = hashlib.file_digest(source, 'sha256').hexdigest()
                record = records.pop((str((official / archive['path']).resolve()), entry.filename))
                if record['bytes'] != entry.file_size:
                    raise ValueError('Independent oracle observed a different model size')
                item['valid'], item['diagnostic'] = record['valid'], record['diagnostic']
                if entry.file_size <= 8192:
                    try:
                        resource = xmlschema.XMLResource(io.BytesIO(package.read(entry)), defuse='always', allow='none')
                        item['secondary_valid'] = secondary.is_valid(resource)
                    except (xmlschema.XMLResourceError, etree.ParseError):
                        item['secondary_valid'] = False
                    if item['secondary_valid'] != item['valid']:
                        raise ValueError('Independent unchanged-XSD providers disagree')
                row['models'].append(item)
        results.append(row)
        (build / 'oracle.json').write_text(json.dumps({'revision': REVISION, 'provider': '.NET XmlSchemaSet',
            'schema_sha256': digest(FIXTURE / 'core.xsd'), 'archives': results}, indent=2), encoding='utf-8')
        print(f'oracle {index + 1}/{len(rows)} {Path(archive["path"]).name}', flush=True)
    if records:
        raise ValueError('Independent oracle returned unexpected models')
    return results


def generate(compiler, build):
    """Import the actual pinned Core/XML bundle, then attempt all eleven backend bindings explicitly."""
    source = build / 'core.serializer'
    generation = {'compiler_sha256': digest(compiler), 'compiler_version':
                  run([compiler, '--version']).stdout.decode('utf-8').strip()}
    imported = build / 'core-import.serializer'
    run([compiler, '--import-xsd', FIXTURE / 'core.xsd', '--namespace', 'core_model', '--root', 'model', '--output', imported])
    authored = imported.read_text(encoding='utf-8')
    match = re.search(r'class (\w+) stable_ids\s*\{\s*xml\(root\("model"\)', authored)
    if not match:
        raise RuntimeError('Official Core model declaration missing from actual schema import')
    authored = authored.replace('namespace core_model {', 'namespace core_model {\n  xml(unknown(preserve));', 1)
    source.write_text(authored, encoding='utf-8')
    generation['schema_sha256'] = digest(source)
    results = {}
    for language, filename in OUTPUTS.items():
        target = build / ('generated-' + language)
        target.mkdir(exist_ok=True)
        output = target / filename
        selector = 'js' if language == 'javascript' else language
        selection = 'js,typescript' if language == 'typescript' else selector
        arguments = [str(compiler), '--input', str(source), '--language', selection,
                     '--' + selector + '.output', str(output), '--cpp.format', 'false']
        if language == 'typescript':
            arguments.extend(['--js.output', str(target / 'schema.mjs')])
        result = subprocess.run(arguments, capture_output=True, timeout=300)
        results[language] = {'accepted': result.returncode == 0,
            'diagnostic': (result.stderr + result.stdout).decode('utf-8', errors='replace')}
    (build / 'bindings.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    generation['outputs'] = {language: digest(build / ('generated-' + language) / filename)
                             for language, filename in OUTPUTS.items() if results[language]['accepted']}
    (build / 'generation.json').write_text(json.dumps(generation, indent=2), encoding='utf-8')
    if {language for language, row in results.items() if row['accepted']} != {'cpp', 'python'}:
        raise ValueError('Actual Core binding matrix changed; qualify newly admitted SDKs explicitly')
    if any('XML choices, wildcards and extension policies are not implemented' not in row['diagnostic']
           for language, row in results.items() if not row['accepted']):
        raise ValueError('Unsupported Core backend failed for an unrelated reason')
    return results


def python_consumer(official, rows, build):
    """Decode, edit, encode and independently validate actual schema-admitted bounded Core graphs."""
    spec = importlib.util.spec_from_file_location('serializer_official_core', build / 'generated-python/core.py')
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    model_type = module._XML_TYPES['{' + CORE + '}model']
    outputs = build / 'python-outputs'
    outputs.mkdir(exist_ok=True)
    for previous in outputs.glob('*.model'):
        previous.unlink()
    results = []
    for archive in rows:
        with zipfile.ZipFile(official / archive['path']) as package:
            for item in archive['models']:
                row = {'archive': archive['path'], 'part': item['part'], 'oracle_valid': item['valid']}
                if item['bytes'] > MAX_INPUT_BYTES:
                    row['result'] = 'outside-common-input-budget'
                    results.append(row)
                    continue
                data = package.read(item['part'])
                try:
                    limits = module.Limits(max_bytes=MAX_INPUT_BYTES, max_string_bytes=MAX_INPUT_BYTES,
                                           max_elements=1000000, max_depth=64)
                    value = model_type.decode_xml(data, limits)
                    if value.xsd_metadata:
                        value.xsd_metadata[0].xsd_value += ' [Serializer qualification]'
                    output = value.encode_xml(limits)
                    model_type.decode_xml(output, limits)
                    filename = Path(archive['path']).stem + '-' + hashlib.sha256(item['part'].encode()).hexdigest()[:12] + '.model'
                    (outputs / filename).write_bytes(output)
                    row['result'] = 'accepted-roundtrip'
                except Exception as error:
                    row['result'], row['diagnostic'] = 'rejected', str(error)[:1600]
                results.append(row)
                (build / 'python.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    (build / 'python.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    check_consumer(results)
    return results


def validate_outputs(build, executable, language):
    """Independently validate actual emitted native model bytes and fail on any invalid output."""
    listing = build / (language + '-outputs.txt')
    listing.write_text('\n'.join(str(path) for path in sorted((build / (language + '-outputs')).glob('*.model'))), encoding='utf-8')
    result = run(['dotnet', executable, FIXTURE / 'core.xsd', FIXTURE / 'xml.xsd', listing, '--xml'])
    records = list(map(json.loads, result.stdout.decode('utf-8-sig').splitlines()))
    (build / (language + '-output-oracle.json')).write_text(json.dumps(records, indent=2), encoding='utf-8')
    if not records or any(not row['valid'] for row in records):
        raise ValueError('Independent Core schema oracle rejected generated ' + language + ' output')
    return records


def check_consumer(results):
    """Require semantic agreement; declared resource rejection is distinct from weaker XML validation."""
    for row in results:
        if row['result'].startswith('outside-'):
            continue
        accepted = row['result'].startswith('accepted')
        if not row['oracle_valid'] and accepted:
            raise ValueError('Generated consumer accepted schema-invalid original: ' + row['archive'])
        if row['oracle_valid'] and not accepted and 'limit' not in row.get('diagnostic', '').lower():
            raise ValueError('Generated consumer rejected admitted original: ' + row['archive'])


def cpp_consumer(official, rows, build, executable):
    """Replay the same bounded original graphs through a real freshly generated native C++ consumer."""
    inputs = build / 'inputs'
    inputs.mkdir(exist_ok=True)
    outputs = build / 'cpp-outputs'
    outputs.mkdir(exist_ok=True)
    for previous in outputs.glob('*.model'):
        previous.unlink()
    admitted = {}
    results = []
    for archive in rows:
        with zipfile.ZipFile(official / archive['path']) as package:
            for item in archive['models']:
                row = {'archive': archive['path'], 'part': item['part'], 'oracle_valid': item['valid']}
                if item['bytes'] > MAX_INPUT_BYTES:
                    row['result'] = 'outside-common-input-budget'
                    results.append(row)
                    continue
                filename = Path(archive['path']).stem + '-' + hashlib.sha256(item['part'].encode()).hexdigest()[:12] + '.model'
                (inputs / filename).write_bytes(package.read(item['part']))
                admitted[filename] = row
    listing = build / 'cpp-inputs.txt'
    listing.write_text('\n'.join(str(inputs / name) for name in admitted), encoding='utf-8')
    output = run([executable, listing, build / 'cpp-outputs'])
    (build / 'cpp-native.tsv').write_bytes(output.stdout)
    for line in output.stdout.decode('utf-8-sig').splitlines():
        filename, status, *diagnostic = line.split('\t')
        row = admitted.pop(filename)
        row['result'] = status
        if diagnostic:
            row['diagnostic'] = diagnostic[0]
        results.append(row)
    if admitted:
        raise ValueError('Native probe failed to report every admitted input')
    (build / 'cpp.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    check_consumer(results)
    return results


def main():
    """Keep corpus fetch explicit and qualification output under ignored build storage."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--official', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--python-dependencies', type=Path, required=True)
    parser.add_argument('--compiler', type=Path)
    parser.add_argument('--oracle-dll', type=Path)
    parser.add_argument('--cpp-probe', type=Path)
    parser.add_argument('--phase', choices=('oracle', 'generate', 'python', 'cpp', 'validate', 'all'), default='all')
    parser.add_argument('--output-language', choices=('cpp', 'python'), default='python')
    args = parser.parse_args()
    if os.name == 'nt':
        ctypes.windll.kernel32.SetErrorMode(0x8003)
    build = args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository scratch must remain under ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    sys.path.insert(0, str(args.python_dependencies.resolve()))
    rows = inventory(args.official.resolve())
    if args.phase in ('oracle', 'all'):
        if args.oracle_dll is None:
            parser.error('--oracle-dll required for the independent .NET oracle')
        rows = oracle(args.official.resolve(), rows, build, args.oracle_dll.resolve())
    elif args.phase in ('python', 'cpp'):
        recorded = json.loads((build / 'oracle.json').read_text(encoding='utf-8'))['archives']
        if [(row['path'], row['sha256']) for row in rows] != [(row['path'], row['sha256']) for row in recorded]:
            raise ValueError('Oracle record does not describe the current pinned archive bytes')
        rows = recorded
    if args.phase in ('generate', 'all'):
        if args.compiler is None:
            parser.error('--compiler required for generation')
        generate(args.compiler.resolve(), build)
    if args.phase in ('python', 'all'):
        python_consumer(args.official.resolve(), rows, build)
    if args.phase == 'cpp':
        if args.cpp_probe is None:
            parser.error('--cpp-probe required for native C++ execution')
        cpp_consumer(args.official.resolve(), rows, build, args.cpp_probe.resolve())
    if args.phase in ('validate', 'all'):
        if args.oracle_dll is None:
            parser.error('--oracle-dll required for independent output validation')
        validate_outputs(build, args.oracle_dll.resolve(), args.output_language)


if __name__ == '__main__':
    main()
