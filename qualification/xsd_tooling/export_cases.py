# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify actual fixed-XSD exports without deriving expected XML from generated output."""
import ast
import hashlib
import json
from pathlib import Path
import re
import runpy
import shutil

FIXTURES = Path(__file__).resolve().parent / 'export_fixtures'
XSD = '{http://www.w3.org/2001/XMLSchema}'
FAMILIES = {
    'text_enum': {'mode': 'single', 'roots': ['{urn:tooling:text}status'],
                  'models': {'ToolingStatus': [('value', 37), ('id', 11)]}},
    'wrapper': {'mode': 'single', 'roots': ['{urn:tooling:wrapper}collection'],
                'models': {'ToolingCollection': [('id', 37), ('values', 11)]}},
    'cross_namespace': {'mode': 'bundle', 'roots': ['{urn:tooling:order}order'],
                        'models': {'ToolingPiece': [('id', 57), ('label', 4)],
                                   'ToolingOrder': [('revision', 37), ('nested', 11),
                                                    ('foreign', 83), ('stamp', 91)]}},
    'singleton_all': {'mode': 'single', 'roots': ['{urn:tooling:all}entry'],
                     'models': {'ToolingEntry': [('id', 57), ('label', 4), ('extra', 83)]}},
    'chameleon': {'mode': 'bundle', 'roots': ['{urn:tooling:alpha}alpha', '{urn:tooling:beta}beta'],
                  'models': {'ToolingAlpha': [('id', 37)], 'ToolingBeta': [('id', 83)]}},
    'cyclic_include': {'mode': 'single', 'roots': ['{urn:tooling:cycle}order'],
                       'models': {'ToolingOrder': [('id', 37), ('label', 11)]}},
    'native_enum': {'mode': 'single', 'roots': ['{urn:tooling:enum}entry'],
                    'models': {'ToolingEntry': [('value', 37), ('selection', 11), ('trail', 83)]}},
    'native_enum_constraints': {'mode': 'single', 'source': 'constraints.serializer',
        'roots': ['{urn:tooling:enum-constraints}' + name for name in ('default_entry', 'fixed_entry', 'text_entry', 'fixed_text_entry')],
        'models': {'EnumConstraintsDefaultEntry': [('value', 37)],
                   'EnumConstraintsFixedEntry': [('value', 11)], 'EnumConstraintsTextEntry': [('value', 83)],
                   'EnumConstraintsFixedTextEntry': [('value', 91)]}},
}
UNSUPPORTED = {
    'native_enum_list': 'Unsupported XML list scalar type',
    'union': 'XSD export requires explicit union/inherited-pattern/QName value identities',
    'choice': 'XSD choice/wildcard export requires ordered content-model storage',
    'prefix_list': 'XSD export requires explicit namespace contexts for QName defaults/fixed values and prefix lists',
    'ambiguous': 'Ambiguous XSD record; configure its namespace or unique root',
    'native_enum_token': 'XSD native enum bindings require xs:string with preserve whitespace',
    'native_enum_normalized': 'XSD native enum bindings require xs:string with preserve whitespace',
    'native_enum_collapse': 'XSD native enum bindings require xs:string with preserve whitespace',
}
BINDING_FAILURES = {
    'native_enum_binding_attribute_default': 'XSD native enum default/fixed value is not a permitted wire name',
    'native_enum_binding_attribute_fixed': 'XSD native enum default/fixed value is not a permitted wire name',
    'native_enum_binding_attribute_intersection': 'XSD native enum binding has no permitted wire names',
    'native_enum_binding_text_default': 'XSD native enum default/fixed value is not a permitted wire name',
    'native_enum_binding_text_fixed': 'XSD native enum default/fixed value is not a permitted wire name',
    'native_enum_binding_text_intersection': 'XSD native enum binding has no permitted wire names',
}


def required_ids():
    """Declare the complete maintained acceptance inventory independently of any run's returned labels."""
    decisions = []
    for name, expected in FAMILIES.items():
        decisions += [name + '-' + suffix for suffix in (
            'bind', expected['mode'] + '-export', 'idempotent-export', 'rebind', 'reviewed-bundle',
            'edited-directory', 'directory-destination', 'invalid-parent')]
        for index in range(len(expected['roots'])):
            decisions += [name + '-' + suffix + '-' + str(index) for suffix in (
                'fresh-import', 'idempotent-import', 'generate-import')]
        for path in sorted((FIXTURES / name).glob('*.xml')):
            decisions += [name + '-' + mode + '-' + path.stem for mode in ('original', 'exported')]
        if name.startswith('native_enum'):
            decisions += [name + '-native-codec-' + mode for mode in ('authored', 'rebound')]
    decisions += [name + '-' + mode for name in UNSUPPORTED for mode in ('export-xsd', 'export-xsd-bundle')]
    for name in BINDING_FAILURES:
        decisions.append(name + '-xsd-valid')
        decisions += [name + '-' + language + '-' + publication
                      for language in ('cpp', 'python') for publication in ('fresh', 'protected')]
    decisions += ['enum-binding-control-' + language for language in ('cpp', 'python')]
    assert len(decisions) == len(set(decisions))
    return decisions


def digest(path):
    """Fingerprint immutable fixtures and complete generated artifacts as exact bytes."""
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def snapshot(directory):
    """Retain contents and timestamps, including directories, to detect partial failed publication."""
    return {path.relative_to(directory).as_posix():
            {'kind': 'directory' if path.is_dir() else 'file', 'mtime_ns': path.stat().st_mtime_ns,
             'sha256': None if path.is_dir() else digest(path)}
            for path in sorted(Path(directory).rglob('*'))}


def residue(directory):
    """Find reserved publication staging artifacts without mistaking ordinary retained outputs for residue."""
    return sorted(path.relative_to(directory).as_posix() for path in Path(directory).rglob('.serializer-*'))


def native_models(path):
    """Read durable IDs/order/names, defaults and behavior directly from generated Python syntax."""
    models = {}
    for node in ast.parse(Path(path).read_text(encoding='utf-8')).body:
        if not isinstance(node, ast.ClassDef) or node.name.startswith('_'):
            continue
        methods = {item.name: item for item in node.body if isinstance(item, ast.FunctionDef)}
        if '_write' not in methods or '_read' not in methods:
            continue
        ids = [item.args[0].value for item in ast.walk(methods['_write'])
               if isinstance(item, ast.Call) and isinstance(item.func, ast.Attribute)
               and item.func.attr == 'field' and isinstance(item.args[0], ast.Constant)]
        fields = []
        for item in ast.walk(methods['_read']):
            if not isinstance(item, ast.Match):
                continue
            for case in item.cases:
                if isinstance(case.pattern, ast.MatchOr):
                    values = [pattern.value.value for pattern in case.pattern.patterns
                              if isinstance(pattern, ast.MatchValue) and isinstance(pattern.value, ast.Constant)]
                    if len(values) == 2 and type(values[0]) is int and type(values[1]) is str:
                        fields.append((values[1], values[0]))
        assert ids == [identity for _, identity in fields], 'Native read/write declaration order differs'
        selected = {name: ast.dump(method, include_attributes=False) for name, method in methods.items()
                    if name in ('__init__', '_read', '_write') or
                    not name.startswith('_') and name not in ('encode', 'decode', 'encode_xml', 'decode_xml',
                                                            'decode_xml_chunks')}
        models[node.name] = {'fields': fields, 'native_methods': selected}
    assert models, 'No ordinary generated model was found'
    return models


def bundle_inventory(directory):
    """Independently reconstruct the flat exported bundle inventory and documented SHA-256 framing."""
    directory = Path(directory)
    manifest = json.loads((directory / 'manifest.json').read_text(encoding='utf-8'))
    names = manifest['schemas']
    assert manifest['version'] == 1 and len(names) == len(set(names)) and manifest['entry'] in names
    assert all(Path(name).name == name and name.endswith('.xsd') for name in names)
    assert {path.name for path in directory.iterdir()} == set(names) | {'manifest.json'}
    checksum = hashlib.sha256(b'Serializer XSD bundle 1\n')
    for name in sorted(names):
        for value in (name.encode('utf-8'), (directory / name).read_bytes()):
            checksum.update(str(len(value)).encode('ascii') + b':' + value)
    assert manifest['sha256'] == checksum.hexdigest(), 'Exported bundle identity differs'
    return manifest


def independent_documents(entry, family):
    """Validate fixed positive/negative XML with a network-disabled, independent lxml XSD provider."""
    from lxml import etree
    parser = etree.XMLParser(no_network=True, resolve_entities=False, load_dtd=False)
    validator = etree.XMLSchema(etree.parse(str(entry), parser))
    decisions = []
    for path in sorted(Path(family).glob('*.xml')):
        expected = path.name.startswith('valid')
        document = etree.parse(str(path), parser)
        actual = validator.validate(document)
        assert actual == expected, (str(entry), path.name, str(validator.error_log))
        decisions.append({'fixture': path.name, 'expected_valid': expected, 'valid': actual,
                          'sha256': digest(path)})
    assert any(row['valid'] for row in decisions) and any(not row['valid'] for row in decisions)
    return decisions


def rejected(invoke, arguments, label, directory, diagnostic=None):
    """Require exact preflight rejection without touching protected outputs or leaving staging artifacts."""
    before = snapshot(directory)
    result = invoke(arguments, label, success=False)
    text = result.stdout + result.stderr
    if diagnostic:
        assert diagnostic in text, (label, text)
    assert snapshot(directory) == before, 'Rejected tooling changed files: ' + label
    assert residue(directory) == [], 'Rejected tooling left staging files: ' + label
    return {'label': label, 'returncode': result.returncode, 'diagnostic': text.strip(),
            'stdout': result.stdout, 'stderr': result.stderr,
            'protected_snapshot': before, 'staging_residue': []}


def enum_binding_document(directory, label):
    """Prove rejected native enum bindings have an independently valid XSD and XML instance."""
    from lxml import etree
    entry, fixture = directory / 'root.xsd', directory / 'valid.xml'
    parser = etree.XMLParser(no_network=True, resolve_entities=False, load_dtd=False)
    validator = etree.XMLSchema(etree.parse(str(entry), parser))
    validator.assertValid(etree.parse(str(fixture), parser))
    return {'label': label, 'valid': True, 'entry': str(entry), 'entry_sha256': digest(entry),
            'fixture': str(fixture), 'sha256': digest(fixture)}


def native_enum_codec(generated, exported):
    """Exercise actual generated enum-valued Python APIs against the independent exported XSD."""
    from lxml import etree
    module = runpy.run_path(str(generated))
    model, state, protocols = module['ToolingEntry'], module['ToolingState'], module['Protocol']
    value = model()
    assert value.value == state.Open and value.selection == () and value.trail == [] and value.is_open()
    value.value, value.selection, value.trail = state.InProgress, (state.Closed,), [state.Open, state.Closed]
    before = value.encode(protocols.BINARY_NONE)
    assert not value.is_open() and before == value.encode(protocols.BINARY_NONE)
    payload = value.encode_xml()
    parser = etree.XMLParser(no_network=True, resolve_entities=False, load_dtd=False)
    validator = etree.XMLSchema(etree.parse(str(exported), parser))
    validator.assertValid(etree.fromstring(payload, parser))
    assert model.decode_xml(payload).encode(protocols.BINARY_NONE) == before
    for protocol in protocols:
        assert model.decode(value.encode(protocol), protocol).encode(protocols.BINARY_NONE) == before
    for bad in (b'InProgress', b'1', b'unknown'):
        invalid = payload.replace(b'>in_progress<', b'>' + bad + b'<')
        assert invalid != payload
        try:
            model.decode_xml(invalid)
        except (ValueError, IndexError):
            pass
        else:
            raise AssertionError('Generated native enum reader accepted an undeclared wire name')
    return {'generated': str(generated), 'xml_sha256': hashlib.sha256(payload).hexdigest(),
            'native_sha256': hashlib.sha256(before).hexdigest(), 'invalid_wire_names_rejected': 3}


def constrained_enum_codec(generated, exported):
    """Check schema defaults/fixed values without replacing native enum defaults or readonly wire values."""
    from lxml import etree
    module = runpy.run_path(str(generated))
    state = module['EnumConstraintsState']
    default, fixed, text, fixed_text = (module['EnumConstraints' + name] for name in (
        'DefaultEntry', 'FixedEntry', 'TextEntry', 'FixedTextEntry'))
    assert default().value == state.Open and fixed().value == state.Closed and text().value == state.Open
    assert fixed_text().value == state.Closed
    prefix = b' xmlns="urn:tooling:enum-constraints"'
    assert default.decode_xml(b'<default_entry' + prefix + b'/>').value == state.InProgress
    assert fixed.decode_xml(b'<fixed_entry' + prefix + b'/>').value == state.Closed
    assert text.decode_xml(b'<text_entry' + prefix + b'/>').value == state.InProgress
    assert fixed_text.decode_xml(b'<fixed_text_entry' + prefix + b'/>').value == state.Closed
    validator = etree.XMLSchema(etree.parse(str(exported), etree.XMLParser(no_network=True, resolve_entities=False)))
    outputs = []
    for model in (default, fixed, text, fixed_text):
        value = model()
        payload = value.encode_xml()
        validator.assertValid(etree.fromstring(payload))
        assert model.decode_xml(payload).value == value.value
        outputs.append(hashlib.sha256(payload).hexdigest())
    invalid = fixed()
    invalid.value = state.Open
    try:
        invalid.encode_xml()
    except ValueError:
        pass
    else:
        raise AssertionError('Generated enum writer ignored a fixed value')
    try:
        fixed.decode_xml(b'<fixed_entry' + prefix + b' value="open"/>')
    except ValueError:
        pass
    else:
        raise AssertionError('Generated enum reader ignored a fixed value')
    invalid_text = fixed_text()
    invalid_text.value = state.Open
    try:
        invalid_text.encode_xml()
    except ValueError:
        pass
    else:
        raise AssertionError('Generated enum text writer ignored a fixed value')
    try:
        fixed_text.decode_xml(b'<fixed_text_entry' + prefix + b'>open</fixed_text_entry>')
    except ValueError:
        pass
    else:
        raise AssertionError('Generated enum text reader ignored a fixed value')
    return {'generated': str(generated), 'xml_sha256': outputs, 'schema_defaults_checked': 4,
            'native_defaults_unchanged': True, 'fixed_failures_rejected': 4}


def qualify(compiler: Path, directory: Path, invoke) -> dict:
    """Run complete supported export families and preserve authored models across export/rebind/import workflows.

    invoke prepends the actual compiler and captures each CompletedProcess. All
    created files stay in the caller's isolated directory; static fixtures remain
    immutable. Fresh imports deliberately receive independently chosen native IDs.
    """
    compiler = Path(compiler).resolve(strict=True)
    directory = Path(directory).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    caller_invoke, decision_ids = invoke, []

    def tracked_invoke(arguments, label, success=True):
        """Retain each actual CLI decision only after its expected process outcome has been checked."""
        completed = caller_invoke(arguments, label, success=success)
        assert (completed.returncode == 0) == success
        decision_ids.append(label)
        return completed

    invoke = tracked_invoke
    before = {path.relative_to(FIXTURES).as_posix(): digest(path)
              for path in sorted(FIXTURES.rglob('*')) if path.is_file()}
    result = {'status': 'passed', 'compiler_sha256': digest(compiler), 'families': [],
              'unsupported_profiles': [], 'negative_decisions': [], 'fixture_sha256': before,
              'enum_binding_profiles': [], 'enum_binding_controls': [], 'artifacts': {}}
    for name, expected in FAMILIES.items():
        case = directory / name
        shutil.copytree(FIXTURES / name, case)
        source = case / expected.get('source', 'model.serializer')
        generated, depfile = case / 'authored.py', case / 'authored.d'
        common = ['--input', source, '--xml.enabled', 'true']
        invoke(common + ['--language', 'python', '--output', generated, '--depfile', depfile], name + '-bind')
        native = native_models(generated)
        assert {model: row['fields'] for model, row in native.items()} == expected['models']
        dependencies = depfile.read_text(encoding='utf-8')
        if 'using schema' in source.read_text(encoding='utf-8'):
            assert all(path.name in dependencies for path in case.glob('*.xsd')), 'A transitive XSD dependency was omitted'
        if expected['mode'] == 'single':
            entry = case / 'exported.xsd'
            export = common + ['--xml.export-xsd', entry]
            invoke(export, name + '-single-export')
            exported_manifest = None
        else:
            target = case / 'exported'
            export = common + ['--xml.export-xsd-bundle', target]
            invoke(export, name + '-bundle-export')
            exported_manifest = bundle_inventory(target)
            entry = target / exported_manifest['entry']
        published = snapshot(case)
        invoke(export, name + '-idempotent-export')
        assert snapshot(case) == published, 'Identical export rewrote a published artifact'
        assert residue(case) == []
        original_decisions = independent_documents(case / 'root.xsd', case)
        exported_decisions = independent_documents(entry, case)
        decision_ids += [name + '-' + mode + '-' + Path(row['fixture']).stem
                         for mode, rows in (('original', original_decisions), ('exported', exported_decisions))
                         for row in rows]
        assert [(row['fixture'], row['valid']) for row in original_decisions] == [
            (row['fixture'], row['valid']) for row in exported_decisions]
        rebound = case / 'rebound'
        rebound.mkdir()
        replacement = '../' + entry.relative_to(case).as_posix()
        rebound_source = rebound / 'model.serializer'
        authored_source = source.read_text(encoding='utf-8')
        if 'using schema "root.xsd";' in authored_source:
            rebound_text = authored_source.replace('using schema "root.xsd";',
                                                   'using schema "' + replacement + '";')
        else:
            rebound_text = authored_source.replace('namespace tooling {',
                'namespace tooling {\n  using schema "' + replacement + '";')
        assert rebound_text != authored_source and 'using schema "' + replacement + '";' in rebound_text
        rebound_source.write_text(rebound_text, encoding='utf-8')
        rebound_output = rebound / 'model.py'
        invoke(['--input', rebound_source, '--language', 'python', '--output', rebound_output], name + '-rebind')
        assert native_models(rebound_output) == native, 'Export/rebind changed native IDs, order, defaults or behavior'
        enum_codec = []
        if name == 'native_enum':
            enum_codec = [native_enum_codec(path, entry) for path in (generated, rebound_output)]
            assert enum_codec[0]['native_sha256'] == enum_codec[1]['native_sha256']
        elif name == 'native_enum_constraints':
            enum_codec = [constrained_enum_codec(path, entry) for path in (generated, rebound_output)]
        if enum_codec:
            decision_ids += [name + '-native-codec-' + mode for mode in ('authored', 'rebound')]
        imports = []
        for index, root in enumerate(expected['roots']):
            imported = case / ('imported_' + str(index) + '.serializer')
            imported_depfile = imported.with_suffix('.d')
            arguments = ['--import-xsd', entry, '--namespace', 'fresh', '--root', root,
                         '--output', imported, '--depfile', imported_depfile]
            invoke(arguments, name + '-fresh-import-' + str(index))
            imported_before = snapshot(case)
            invoke(arguments, name + '-idempotent-import-' + str(index))
            assert snapshot(case) == imported_before, 'Identical declaration import rewrote a published artifact'
            imported_python = imported.with_suffix('.py')
            invoke(['--input', imported, '--language', 'python', '--output', imported_python],
                   name + '-generate-import-' + str(index))
            fresh = native_models(imported_python)
            assert all(field.startswith('xsd_') for row in fresh.values() for field, _ in row['fields'])
            imports.append({'root': root, 'source': str(imported), 'sha256': digest(imported),
                            'generated': str(imported_python), 'models': fresh,
                            'native_ids_are_independently_imported': True})
        # Ordinary export can replace ordinary files; protected edited-directory semantics belong to bundle export.
        guarded = case / 'reviewed_bundle'
        invoke(common + ['--xml.export-xsd-bundle', guarded], name + '-reviewed-bundle')
        guarded_manifest = bundle_inventory(guarded)
        reviewed = guarded / guarded_manifest['entry']
        reviewed.write_bytes(reviewed.read_bytes() + b'\n<!-- reviewed local edit -->\n')
        result['negative_decisions'].append(rejected(
            invoke, common + ['--xml.export-xsd-bundle', guarded], name + '-edited-directory', case))
        protected, protected_dep = case / 'protected.py', case / 'protected.d'
        protected.write_bytes(b'# Existing consumer must remain unchanged.\n')
        protected_dep.write_bytes(b'protected.py: reviewed.schema\n')
        occupied = case / 'occupied.xsd'
        occupied.mkdir()
        result['negative_decisions'].append(rejected(
            invoke, common + ['--language', 'python', '--output', protected, '--depfile', protected_dep,
                              '--xml.export-xsd', occupied], name + '-directory-destination', case))
        blocked = case / 'blocked'
        blocked.write_bytes(b'Existing ordinary file is not an export parent.\n')
        result['negative_decisions'].append(rejected(
            invoke, common + ['--xml.export-xsd-bundle', blocked / 'candidate'], name + '-invalid-parent', case))
        result['families'].append({'name': name, 'mode': expected['mode'], 'authored_source': str(source),
                                    'authored_sha256': digest(source), 'authored_models': native,
                                    'generated': [str(generated), str(rebound_output)],
                                    'exported_entry': str(entry), 'manifest': exported_manifest,
                                    'documents': exported_decisions, 'fresh_imports': imports,
                                    'native_enum_codec': enum_codec,
                                    'native_contract_unchanged_after_rebind': True, 'staging_residue': []})
    for name, diagnostic in UNSUPPORTED.items():
        case = directory / name
        shutil.copytree(FIXTURES / name, case)
        source = case / 'model.serializer'
        protected, depfile = case / 'protected.py', case / 'protected.d'
        protected.write_bytes(b'# Existing generated consumer.\n')
        depfile.write_bytes(b'protected.py: previous.schema\n')
        for mode, destination in (('--xml.export-xsd', case / 'unpublished.xsd'),
                                  ('--xml.export-xsd-bundle', case / 'unpublished_bundle')):
            arguments = ['--input', source, '--xml.enabled', 'true', mode, destination]
            if mode == '--xml.export-xsd':
                arguments += ['--language', 'python', '--output', protected, '--depfile', depfile]
            decision = rejected(invoke, arguments, name + '-' + mode.removeprefix('--xml.'), case, diagnostic)
            result['unsupported_profiles'].append({'profile': name, **decision})
    for name, diagnostic in BINDING_FAILURES.items():
        case = directory / name
        shutil.copytree(FIXTURES / name, case)
        source = case / 'model.serializer'
        independent = enum_binding_document(case, name + '-xsd-valid')
        decision_ids.append(independent['label'])
        result['artifacts'].update({str(path): digest(path) for path in
                                    (source, case / 'root.xsd', case / 'valid.xml')})
        generations = []
        for language, extension in (('cpp', '.hpp'), ('python', '.py')):
            for publication in ('fresh', 'protected'):
                output = case / (language + '-' + publication + extension)
                depfile = output.with_suffix('.d')
                if publication == 'protected':
                    output.write_bytes(b'Existing generated consumer must remain unchanged.\n')
                    depfile.write_bytes(b'consumer: previous.schema\n')
                arguments = ['--input', source, '--language', language, '--output', output, '--depfile', depfile]
                if language == 'cpp':
                    arguments += ['--cpp.format', 'false']
                decision = rejected(invoke, arguments, name + '-' + language + '-' + publication, case, diagnostic)
                assert output.exists() == depfile.exists() == (publication == 'protected')
                generations.append({**decision, 'language': language, 'publication': publication,
                                    'output_exists': output.exists(), 'depfile_exists': depfile.exists(),
                                    'after_snapshot': snapshot(case)})
        result['enum_binding_profiles'].append({'profile': name, 'expected_diagnostic': diagnostic,
                                                'independent_xsd': independent, 'generations': generations})
    # Match negative CLI paths with valid enum default/fixed/text bindings in both generated languages.
    for language, extension in (('cpp', '.hpp'), ('python', '.py')):
        case = directory / ('enum-binding-control-' + language)
        shutil.copytree(FIXTURES / 'native_enum_constraints', case)
        output, depfile = case / ('control' + extension), case / 'control.d'
        label = 'enum-binding-control-' + language
        completed = invoke(['--input', case / 'constraints.serializer', '--language', language,
                            '--output', output, '--depfile', depfile] +
                           (['--cpp.format', 'false'] if language == 'cpp' else []), label)
        assert output.is_file() and depfile.is_file() and residue(case) == []
        result['enum_binding_controls'].append({'label': label, 'language': language,
            'returncode': completed.returncode, 'generated': str(output), 'sha256': digest(output),
            'depfile': str(depfile), 'depfile_sha256': digest(depfile)})
        result['artifacts'].update({str(path): digest(path) for path in (output, depfile)})
    after = {path.relative_to(FIXTURES).as_posix(): digest(path)
             for path in sorted(FIXTURES.rglob('*')) if path.is_file()}
    assert after == before, 'Maintained fixture bytes changed'
    assert residue(directory) == []
    result['fixture_inputs_unchanged'] = True
    assert len(decision_ids) == len(set(decision_ids)) and set(decision_ids) == set(required_ids())
    result['decision_ids'] = decision_ids
    return result
