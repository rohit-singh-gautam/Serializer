# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify compiler XSD tools and prepare exported schemas for a separate eleven-SDK replay."""

import argparse
import ast
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import time
import types
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
TOOLING = Path(__file__).resolve().with_suffix('')
EVOLUTION = ROOT / 'qualification/xsd_evolution'
MODULES = ('bundle_limits', 'comparison_cases', 'export_cases')
VARIANTS = ('v1', 'v2')
COMMAND_TIMEOUT_SECONDS = 120
BUNDLE_IDS = (
    'limits.instances.64', 'limits.instances.65', 'limits.depth.32',
    'limits.depth.cycle-backedge-after-32', 'limits.depth.33',
    'limits.instances.dedup-cycle-at-64', 'limits.chameleon.instances.64',
    'limits.chameleon.instances.65', 'limits.bytes.exact-16mib',
    'limits.bytes.one-over-16mib', 'limits.chameleon.bytes.exact-16mib',
    'limits.chameleon.bytes.one-over-16mib', 'paths.reject.https', 'paths.reject.file-uri',
    'paths.reject.drive', 'paths.reject.absolute', 'paths.reject.percent', 'paths.reject.query',
    'paths.reject.fragment', 'paths.reject.backslash', 'paths.reject.missing-location',
    'paths.reject.parent-escape', 'paths.accept.spaces', 'paths.bind.spaces',
    'paths.accept.unicode-child', 'paths.bind.unicode-child', 'paths.accept.unicode-cli-root',
    'paths.bind.unicode-cli-root', 'paths.accept.canonical-in-bundle-alias',
    'paths.accept.canonical-link-alias', 'paths.reject.canonical-link-escape',
    'paths.reject.include-target', 'paths.reject.import-target', 'paths.reject.same-namespace-import',
    'paths.reject.missing-source-document-import', 'paths.accept.explicit-source-document-import',
    'identity.original', 'identity.idempotent', 'identity.bind-pinned', 'identity.reject-mismatched-pin',
    'identity.moved-checkout', 'identity.source-byte-change', 'identity.relative-path-change',
    'identity.effective-namespace-left', 'identity.effective-namespace-right',
)
COMPARISON_IDS = tuple('comparison.' + name for name in (
    'export-rebind-forward', 'export-rebind-reverse', 'occurrence-max-widen', 'occurrence-max-tighten',
    'occurrence-unbounded-widen', 'occurrence-unbounded-tighten', 'occurrence-min-tighten',
    'occurrence-min-widen', 'sequence-to-all', 'all-to-sequence', 'all-reorder-forward',
    'all-reorder-reverse', 'sequence-reorder-forward', 'sequence-reorder-reverse',
    'root-namespace-forward', 'root-namespace-reverse', 'member-namespace-forward',
    'member-namespace-reverse', 'placement-attribute-to-element', 'placement-element-to-attribute',
    'default-forward', 'default-reverse', 'fixed-forward', 'fixed-reverse',
    'default-to-fixed-forward', 'default-to-fixed-reverse', 'facet-forward', 'facet-reverse',
    'required-attribute-tighten', 'required-attribute-widen', 'renamed-member-unavailable',
    'renamed-root-unavailable', 'missing-member-unavailable', 'new-required-member-unavailable',
    'required-optional-member-unavailable', 'scalar-type-unavailable', 'sequence-to-reordered-all',
    'all-repeated-storage-cpp-unavailable', 'all-repeated-storage-java-unavailable',
    'all-repeated-storage-csharp-unavailable', 'all-repeated-storage-go-unavailable',
    'all-repeated-storage-kotlin-unavailable', 'all-repeated-storage-swift-unavailable',
    'all-repeated-storage-rust-unavailable', 'all-repeated-storage-c-unavailable',
))
COMPATIBLE_COMPARISONS = frozenset('comparison.' + name for name in (
    'export-rebind-forward', 'export-rebind-reverse', 'occurrence-max-widen',
    'occurrence-unbounded-widen', 'occurrence-min-widen', 'sequence-to-all',
    'all-reorder-forward', 'all-reorder-reverse', 'required-attribute-widen',
    'sequence-to-reordered-all'))
ENUM_BINDING_PROFILES = tuple('native_enum_binding_' + placement + '_' + policy
                              for placement in ('attribute', 'text')
                              for policy in ('default', 'fixed', 'intersection'))


def admit_enum_binding_profiles(result, module):
    """Require independent valid-XSD controls and atomic C++/Python rejection for all six enum binding failures."""
    assert set(module.BINDING_FAILURES) == set(ENUM_BINDING_PROFILES)
    profiles = result['enum_binding_profiles']
    artifacts = result['artifacts']
    assert artifacts, 'Enum binding controls require pinned actual schema/XML/generated artifacts'
    assert len(profiles) == len(ENUM_BINDING_PROFILES)
    assert {row['profile'] for row in profiles} == set(ENUM_BINDING_PROFILES)
    labels = []
    for row in profiles:
        name = row['profile']
        diagnostic = module.BINDING_FAILURES[name]
        assert diagnostic and row['expected_diagnostic'] == diagnostic
        independent = row['independent_xsd']
        assert independent['label'] == name + '-xsd-valid' and independent['valid'] is True
        assert independent['entry'] and independent['fixture']
        assert all(re.fullmatch(r'[0-9a-f]{64}', independent[key]) for key in ('entry_sha256', 'sha256'))
        assert artifacts.get(independent['entry']) == independent['entry_sha256']
        assert artifacts.get(independent['fixture']) == independent['sha256']
        labels.append(independent['label'])
        generations = row['generations']
        expected = {(language, publication) for language in ('cpp', 'python')
                    for publication in ('fresh', 'protected')}
        assert len(generations) == len(expected)
        assert {(item['language'], item['publication']) for item in generations} == expected
        for item in generations:
            assert item['label'] == name + '-' + item['language'] + '-' + item['publication']
            assert item['returncode'] != 0 and diagnostic in item['diagnostic']
            assert item['protected_snapshot'] and item['staging_residue'] == []
            assert item['after_snapshot'] == item['protected_snapshot']
            assert diagnostic in item['stderr'] and item['diagnostic'] == (item['stdout'] + item['stderr']).strip()
            retained = item['publication'] == 'protected'
            assert item['output_exists'] is retained and item['depfile_exists'] is retained
            labels.append(item['label'])
    controls = result['enum_binding_controls']
    assert len(controls) == 2 and {row['language'] for row in controls} == {'cpp', 'python'}
    for row in controls:
        assert row['label'] == 'enum-binding-control-' + row['language'] and row['returncode'] == 0
        assert row['generated'] and row['depfile']
        assert all(re.fullmatch(r'[0-9a-f]{64}', row[key]) for key in ('sha256', 'depfile_sha256'))
        assert artifacts.get(row['generated']) == row['sha256'] and artifacts.get(row['depfile']) == row['depfile_sha256']
        labels.append(row['label'])
    assert len(labels) == len(set(labels)) and set(labels) <= set(result['decision_ids'])


def bundle_rejects(identity):
    """Retain the expected rejection inventory independently of a component's nominal passed markers."""
    return identity.startswith('paths.reject.') or identity in {
        'limits.instances.65', 'limits.depth.33', 'limits.chameleon.instances.65',
        'limits.bytes.one-over-16mib', 'limits.chameleon.bytes.one-over-16mib',
        'identity.reject-mismatched-pin'}


def comparison_classification(identity):
    """Distinguish actual compatible/incompatible checks from unavailable association/backend profiles."""
    if identity in COMPATIBLE_COMPARISONS:
        return 'compatible'
    if identity.startswith('comparison.all-repeated-storage-'):
        return 'backend_unavailable'
    return 'association_unavailable' if identity.endswith('-unavailable') else 'incompatible'


def sha(path):
    """Fingerprint exact source/provider/output bytes without decoding or normalizing them."""
    checksum = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            checksum.update(block)
    return checksum.hexdigest()


def source_snapshot():
    """Watch all new tooling inputs and the exact ordinary SDK consumers reused by the later replay."""
    paths = {Path(__file__), ROOT / 'test/xsd_tooling_guard_test.py',
             ROOT / 'qualification/xsd_evolution.py', ROOT / 'test/xsd_import_cli_test.py',
             ROOT / 'test/xsd_evolution_guard_test.py', ROOT / 'test/package_node_dependencies.py',
             ROOT / 'test/xsd_enum_export_test.cpp', ROOT / 'test/xml_schema_test.cpp',
             ROOT / 'test/xml_xsd_contract_test.cpp', ROOT / 'test/CMakeLists.txt',
             ROOT / 'test/resources/xml_shapes/typescript/package.json',
             ROOT / 'test/resources/xml_shapes/typescript/package-lock.json',
             ROOT / 'example/xml/rust/Cargo.toml', ROOT / 'example/xml/rust/Cargo.lock'}
    for directory in (TOOLING, EVOLUTION):
        paths.update(path for path in directory.rglob('*') if path.is_file()
                     and path.suffix.lower() != '.md' and '__pycache__' not in path.parts)
    return {path.relative_to(ROOT).as_posix(): sha(path) for path in sorted(paths)}


def load_component(name):
    """Load an explicitly selected maintained module without importing similarly named build output."""
    assert name in MODULES
    spec = importlib.util.spec_from_file_location('_serializer_xsd_tools_' + name, TOOLING / (name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def required_ids(name):
    """Require complete semantic inventories; command counts cannot stand in for completed checks."""
    if name == 'bundle_limits':
        result = BUNDLE_IDS
    elif name == 'comparison_cases':
        result = COMPARISON_IDS
    else:
        assert name == 'export_cases'
        result = tuple(load_component(name).required_ids())
    assert result and len(result) == len(set(result))
    return tuple(result)


def observed_ids(name, result):
    """Reject blocked/failed rows and normalize the modules' two explicit decision representations."""
    assert result['status'] == 'passed'
    if name == 'export_cases':
        module = load_component(name)
        families = result['families']
        assert len(families) == len(module.FAMILIES)
        assert {row['name'] for row in families} == set(module.FAMILIES)
        for row in families:
            assert row['native_contract_unchanged_after_rebind'] is True and row['staging_residue'] == []
            assert row['authored_models'] and row['fresh_imports'] and row['documents']
            assert {item['root'] for item in row['fresh_imports']} == set(module.FAMILIES[row['name']]['roots'])
            assert all(item['native_ids_are_independently_imported'] is True and item['models']
                       for item in row['fresh_imports'])
            assert any(item['valid'] for item in row['documents']) and any(not item['valid'] for item in row['documents'])
        unsupported = result['unsupported_profiles']
        assert len(unsupported) == 2 * len(module.UNSUPPORTED)
        assert {row['profile'] for row in unsupported} == set(module.UNSUPPORTED)
        assert all(row['returncode'] != 0 and row['diagnostic'] and row['protected_snapshot']
                   and row['staging_residue'] == [] for row in unsupported)
        admit_enum_binding_profiles(result, module)
        assert result['fixture_inputs_unchanged'] is True and result['fixture_sha256']
        values = result['decision_ids']
    else:
        rows = result['decisions']
        assert rows and all(row.get('status', 'passed') == 'passed' for row in rows)
        if name == 'bundle_limits':
            assert all(row['before'] and row['after'] for row in rows), 'Missing protected-input/output snapshots'
            for row in rows:
                assert (row['expected'] == 'reject') == bundle_rejects(row['id'])
                assert (row['returncode'] == 0) == (row['expected'] == 'accept')
                if row['expected'] == 'reject':
                    assert row['before'] == row['after'] and row['expected_diagnostic'] in row['stderr']
        else:
            assert all(row['native_contract'] for row in rows), 'Missing native default/identity/behavior proof'
            assert all(row['classification'] == comparison_classification(row['id'])
                       and (row['returncode'] == 0) == (row['classification'] == 'compatible') for row in rows)
        values = [row['id'] for row in rows]
        if 'decision_count' in result:
            assert result['decision_count'] == len(values)
    assert values and len(values) == len(set(values)), 'Missing or duplicate semantic decisions'
    assert set(values) == set(required_ids(name)), 'Incomplete semantic inventory for ' + name
    return tuple(values)


def merge_artifacts(captures):
    """Keep the earliest observation and expose conflicting repeated captures instead of overwriting them."""
    result, conflicts = {}, set()
    for capture in captures:
        for path, checksum in capture.items():
            key = os.path.normcase(str(Path(path).resolve()))
            if key in result and result[key] != checksum:
                conflicts.add(key)
            else:
                result[key] = checksum
    return result, sorted(conflicts)


def linked_artifacts(path, run, label):
    """Pin one actual binary and its resolved Unix dependencies, rejecting unresolved provider linkage."""
    path = Path(path).resolve(strict=True)
    artifacts = {str(path): sha(path)}
    if os.name != 'nt':
        result = run(['ldd', path], label)
        assert 'not found' not in result.stdout + result.stderr, 'Unresolved dynamic provider dependency'
        for value in re.findall(r'(/[^\s()]+)\s+\(', result.stdout):
            dependency = Path(value).resolve(strict=True)
            artifacts[str(dependency)] = sha(dependency)
    return artifacts


def provider_capture(compiler, run, label):
    """Observe the actual compiler, Python XML parsers and mandatory independent lxml implementation."""
    import pyexpat
    import xml.parsers.expat
    import _elementtree
    from lxml import etree
    import lxml

    captures = [linked_artifacts(compiler, run, label + '-compiler'),
                linked_artifacts(sys.executable, run, label + '-python'),
                linked_artifacts(getattr(pyexpat, '__file__', None) or sys.executable, run, label + '-expat'),
                linked_artifacts(getattr(_elementtree, '__file__', None) or sys.executable, run, label + '-elementtree'),
                linked_artifacts(etree.__file__, run, label + '-lxml')]
    if os.name == 'nt':
        paths = list(Path(compiler).parent.glob('*.dll'))
        paths.append(Path(sys.base_prefix) / f'python{sys.version_info.major}{sys.version_info.minor}.dll')
        captures.append({str(path.resolve(strict=True)): sha(path) for path in paths})
    captures.append({str(Path(path).resolve(strict=True)): sha(path)
                     for path in (xml.parsers.expat.__file__, ET.__file__, lxml.__file__)})
    artifacts, conflicts = merge_artifacts(captures)
    assert not conflicts, 'Conflicting provider observations during capture'
    return {'python': sys.version, 'expat': pyexpat.EXPAT_VERSION,
            'lxml': list(etree.LXML_VERSION), 'libxml2': list(etree.LIBXML_VERSION),
            'independent_xml_artifact': str(Path(etree.__file__).resolve()), 'artifacts': artifacts}


def native_contract(path):
    """Compare native field/codec/default/behavior syntax independently of generated XML methods."""
    selected = {}
    for node in ast.parse(Path(path).read_text(encoding='utf-8')).body:
        if not isinstance(node, ast.ClassDef) or node.name != 'EvolutionOrder':
            continue
        methods = {item.name: item for item in node.body if isinstance(item, ast.FunctionDef)}
        names = ('__init__', '_read', '_write', 'has_id', 'encode', 'decode')
        assert all(name in methods for name in names), 'A public native method disappeared'
        selected[node.name] = {name: ast.dump(methods[name], include_attributes=False) for name in names}
    assert set(selected) == {'EvolutionOrder'}
    return selected


def native_observation(path, variant):
    """Execute real generated Python defaults/native APIs and independently check the variant's XML values."""
    expected = json.loads((EVOLUTION / 'manifest.json').read_text(encoding='utf-8'))
    name = '_serializer_tooling_sdk_' + hashlib.sha256(str(path).encode('utf-8')).hexdigest()
    module = types.ModuleType(name)
    module.__file__ = str(path)
    previous = sys.modules.get(name)
    sys.modules[name] = module
    try:
        exec(compile(Path(path).read_text(encoding='utf-8'), str(path), 'exec'), module.__dict__)
        value = module.EvolutionOrder()
        assert value.id == 7 and value.customer == 'native default' and value.note == ()
        assert value.has_id(7) and not value.has_id(8)
        payloads = {'default.bin': value.encode(module.Protocol.BINARY_NONE)}
        value.customer, value.note = 'R&D', ('memo',)
        assert value.has_id(7) and not value.has_id(8)
        for filename, protocol in (('authored.bin', module.Protocol.BINARY_NONE),
                                   ('authored-integer.bin', module.Protocol.BINARY_INTEGER),
                                   ('authored-string.bin', module.Protocol.BINARY_STRING)):
            payloads[filename] = value.encode(protocol)
            copy = module.EvolutionOrder.decode(payloads[filename], protocol)
            assert copy.id == 7 and copy.customer == 'R&D' and copy.note == ('memo',) and copy.has_id(7)
        document = value.encode_xml()
        projection = module.EvolutionOrder.decode_xml(document)
        assert projection.id == 7 and projection.customer == 'R&D' and projection.note == ('memo',)
        payloads['xml-projection.bin'] = projection.encode(module.Protocol.BINARY_NONE)
        for filename, payload in payloads.items():
            assert payload.hex() == expected['native_hex'][filename], 'Native literal differs: ' + filename
        root = ET.fromstring(document)
        prefix = '{' + expected['namespace'] + '}'
        assert root.tag == prefix + expected['root'] and root.attrib == {'id': '7'}
        assert [[child.tag, child.text] for child in root] == [
            [prefix + local, text] for local, text in expected['xml_children'][variant]]
        return {'native_sha256': {filename: hashlib.sha256(payload).hexdigest()
                                  for filename, payload in payloads.items()},
                'xml_sha256': hashlib.sha256(document).hexdigest(), 'xml': document}
    finally:
        if previous is None:
            sys.modules.pop(name, None)
        else:
            sys.modules[name] = previous


def prepare_sdk_schemas(directory, invoke):
    """Export both ordinary models and retain byte-identical authored sources for a later real SDK matrix."""
    from lxml import etree
    directory.mkdir()
    result = {}
    for variant in VARIANTS:
        original = EVOLUTION / variant
        selected = directory / variant
        selected.mkdir()
        exported = selected / 'order.xsd'
        invoke(['--input', original / 'order.serializer', '--xml.export-xsd', exported], variant + '-export')
        copied = selected / 'order.serializer'
        copied.write_bytes((original / 'order.serializer').read_bytes())
        assert copied.read_bytes() == (original / 'order.serializer').read_bytes()
        generated_original, generated_rebound = selected / 'original.py', selected / 'rebound.py'
        invoke(['--input', original / 'order.serializer', '--language', 'python', '--output', generated_original],
               variant + '-original-python')
        invoke(['--input', copied, '--language', 'python', '--output', generated_rebound,
                '--xml.xsd', exported, '--xml.compare-xsd', original / 'order.xsd'], variant + '-rebound-python')
        invoke(['--input', copied, '--xml.xsd', original / 'order.xsd', '--xml.compare-xsd', exported],
               variant + '-reverse-compare')
        contract = native_contract(generated_original)
        assert contract == native_contract(generated_rebound), 'Export/rebind changed native AST/API/defaults'
        before, after = native_observation(generated_original, variant), native_observation(generated_rebound, variant)
        assert before['native_sha256'] == after['native_sha256']
        parser = etree.XMLParser(resolve_entities=False, no_network=True, load_dtd=False)
        validators = [etree.XMLSchema(etree.parse(str(path), parser)) for path in (original / 'order.xsd', exported)]
        for observed in (before, after):
            for validator in validators:
                validator.assertValid(etree.fromstring(observed['xml'], parser))
            observed.pop('xml')
        result[variant] = {'status': 'passed', 'source_byte_identical': True,
                           'source': str(copied), 'xsd': str(exported),
                           'source_sha256': sha(copied), 'xsd_sha256': sha(exported),
                           'native_ast_sha256': hashlib.sha256(json.dumps(contract, sort_keys=True).encode()).hexdigest(),
                           'original': before, 'rebound': after,
                           'generated': {str(path): sha(path) for path in (generated_original, generated_rebound)}}
    return result


def admit_tooling(report):
    """Require full compiler-tool evidence; explicitly refuse any implied eleven-backend completion claim."""
    assert report['status'] == 'passed' and report['full_inventory']
    assert report['scope'] == 'compiler_tooling_only' and report['runtime_followup_required'] is True
    assert report['runtime_backend_completion'] is False and not report['failures']
    assert set(report['modules']) == set(MODULES)
    identities = []
    for name in MODULES:
        identities.extend(observed_ids(name, report['modules'][name]))
    assert len(identities) == len(set(identities)), 'Decision IDs overlap across modules'
    assert report['decision_count'] == len(identities)
    assert report['qualification_sources_sha256'] == source_snapshot(), 'Source inventory is empty, partial or changed'
    assert report['source_verification'] == {'changed': [], 'compiler_unchanged': True,
                                             'changed_artifacts': [], 'conflicting_artifacts': []}
    assert report['providers_before']['artifacts'] and report['providers_before']['independent_xml_artifact']
    assert report['providers_before']['artifacts'] == report['providers_after']['artifacts']
    assert set(report['sdk_models']) == set(VARIANTS) and report['sdk_schema_directory']
    literals = json.loads((EVOLUTION / 'manifest.json').read_text(encoding='utf-8'))['native_hex']
    expected = {name: hashlib.sha256(bytes.fromhex(value)).hexdigest() for name, value in literals.items()}
    for variant in VARIANTS:
        model = report['sdk_models'][variant]
        assert model['status'] == 'passed' and model['source_byte_identical'] is True
        assert model['source_sha256'] == sha(EVOLUTION / variant / 'order.serializer')
        assert model['native_ast_sha256'] and model['xsd_sha256'] and model['generated']
        assert model['original']['native_sha256'] == model['rebound']['native_sha256'] == expected
    return {'modules': len(MODULES), 'decisions': len(identities), 'sdk_schema_variants': len(VARIANTS)}


def main():
    """Capture every independent component failure and always retain a guarded report in a fresh build directory."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--modules', nargs='+', choices=MODULES, default=list(MODULES))
    args = parser.parse_args()
    selected = tuple(args.modules)
    assert len(selected) == len(set(selected)), 'Choose distinct modules'
    directory = args.build.resolve()
    assert ROOT in directory.parents and 'out' in directory.relative_to(ROOT).parts
    assert not directory.exists() or not any(directory.iterdir()), 'Use a fresh evidence directory'
    directory.mkdir(parents=True, exist_ok=True)
    compiler = args.compiler.resolve()
    commands, failures, captures = [], [], []
    report = {'status': 'failed', 'scope': 'compiler_tooling_only', 'runtime_followup_required': True,
              'runtime_backend_completion': False, 'full_inventory': set(selected) == set(MODULES),
              'platform': platform.platform(), 'compiler': str(compiler), 'modules': {},
              'commands': commands, 'failures': failures, 'sdk_models': {},
              'sdk_schema_directory': str(directory / 'sdk_schemas')}

    def run(arguments, label, success=True):
        """Record exact argv, actual UTF-8 diagnostics, cwd, duration and failures without a shell."""
        argv = [str(value) for value in arguments]
        start = time.monotonic()
        try:
            result = subprocess.run(argv, cwd=ROOT, capture_output=True, text=True, encoding='utf-8',
                                    errors='replace', timeout=COMMAND_TIMEOUT_SECONDS)
        except Exception as error:
            commands.append({'label': label, 'argv': argv, 'cwd': str(ROOT), 'returncode': None,
                             'duration_seconds': time.monotonic() - start, 'error': repr(error)})
            raise
        commands.append({'label': label, 'argv': argv, 'cwd': str(ROOT), 'returncode': result.returncode,
                         'duration_seconds': time.monotonic() - start, 'stdout': result.stdout,
                         'stderr': result.stderr, 'expected_success': success})
        assert (result.returncode == 0) == success, label + '\n' + result.stdout + result.stderr
        return result

    def invoke(arguments, label, success=True):
        """Supply the exact compiler-first CompletedProcess interface borrowed by all component modules."""
        return run([compiler, *arguments], label, success)

    try:
        report['compiler_sha256'] = sha(compiler)
        report['qualification_sources_sha256'] = source_snapshot()
        report['compiler_version'] = invoke(['--version'], 'compiler-version').stdout.strip()
    except Exception as error:
        failures.append({'setup': 'source/compiler capture', 'error': repr(error)})
    try:
        report['providers_before'] = provider_capture(compiler, run, 'before')
        captures.append(report['providers_before']['artifacts'])
    except Exception as error:
        failures.append({'setup': 'required XML providers', 'error': repr(error)})

    for name in selected:
        try:
            result = load_component(name).qualify(compiler, directory / name, invoke)
            report['modules'][name] = result
            observed_ids(name, result)
            captures.append(result.get('artifacts', {}))
            print(name + ' passed', flush=True)
        except Exception as error:
            report['modules'].setdefault(name, {'status': 'failed', 'error': repr(error)})
            failures.append({'module': name, 'error': repr(error)})
    try:
        report['sdk_models'] = prepare_sdk_schemas(directory / 'sdk_schemas', invoke)
        for model in report['sdk_models'].values():
            captures.append(model['generated'])
            captures.append({model['source']: model['source_sha256'], model['xsd']: model['xsd_sha256']})
    except Exception as error:
        failures.append({'sdk_schema_preparation': repr(error)})
    try:
        report['providers_after'] = provider_capture(compiler, run, 'after')
        captures.append(report['providers_after']['artifacts'])
        artifacts, conflicts = merge_artifacts(captures)
        after = source_snapshot()
        before = report.get('qualification_sources_sha256', {})
        report['source_verification'] = {
            'changed': sorted(name for name in before.keys() | after.keys() if before.get(name) != after.get(name)),
            'compiler_unchanged': sha(compiler) == report.get('compiler_sha256'),
            'changed_artifacts': sorted(path for path, checksum in artifacts.items()
                                        if not Path(path).is_file() or sha(path) != checksum),
            'conflicting_artifacts': conflicts}
        report['artifact_observations'] = artifacts
    except Exception as error:
        failures.append({'final_capture': repr(error)})

    report['decision_count'] = sum(len(value.get('decision_ids', value.get('decisions', [])))
                                    for value in report['modules'].values())
    clean = report.get('source_verification') == {'changed': [], 'compiler_unchanged': True,
                                                  'changed_artifacts': [], 'conflicting_artifacts': []}
    if not failures and clean:
        report['status'] = 'passed' if report['full_inventory'] else 'diagnostic_passed'
        if report['full_inventory']:
            try:
                report['admission'] = admit_tooling(report)
            except Exception as error:
                failures.append({'admission': repr(error)})
                report['status'] = 'failed'
    output = directory / 'verification.json'
    output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'status': report['status'], 'decisions': report['decision_count'],
                      'failures': failures, 'report_sha256': sha(output)}, indent=2))
    return 0 if report['status'] in ('passed', 'diagnostic_passed') else 1


if __name__ == '__main__':
    sys.exit(main())
