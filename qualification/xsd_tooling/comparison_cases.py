# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify directional fixed-XSD comparisons through actual compiler invocations."""
from collections import Counter
import hashlib
from pathlib import Path
import sys
import sysconfig
import types
import xml.etree.ElementTree as ET


MODEL = '''// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
serializer version 1.20.0;
namespace comparison {
  class order stable_ids {
    public uint32 id (37) {7};
    public string customer (11) {"native default"};
    public optional<string> note (83);
    public array string tags (97);
    public function bool has_id(uint32 expected) readonly expression(id == expected);
  }
}
'''
SINGLETON_MODEL = MODEL.replace('public array string tags (97);', 'public optional<string> tags (97);')
XML_NAMESPACE = 'http://www.w3.org/2001/XMLSchema'
COMPATIBLE = 'XML compatibility: current reader accepts the previous bound contract'
OCCURRENCE = 'Current reader narrows the accepted occurrence interval'
PLACEMENT = 'Expanded name or attribute/element/text/wrapper placement changed'
SCALAR = 'Scalar restrictions or default/fixed semantics changed'
NATIVE_HEX = {
    'binary_none': '070000000352264401046d656d6f0201410142',
    'binary_integer': '25070000000b03522644405301046d656d6f4061020141014200',
    'binary_string': '0269640700000008637573746f6d657203522644046e6f746501046d656d6f0474616773020141014200',
    'defaults': '070000000e6e61746976652064656661756c740000',
}
SINGLETON_NATIVE_HEX = {
    'binary_none': '070000000352264401046d656d6f010141',
    'binary_integer': '25070000000b03522644405301046d656d6f406101014100',
    'binary_string': '0269640700000008637573746f6d657203522644046e6f746501046d656d6f047461677301014100',
    'defaults': NATIVE_HEX['defaults'],
}


def _sha(path):
    """Fingerprint exact fixtures, published contracts and protected generated files."""
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _snapshot(directory):
    """Observe bytes, timestamps and directory inventory so rejection cannot hide publication residue."""
    files, directories = {}, []
    for path in sorted(directory.rglob('*')):
        relative = path.relative_to(directory).as_posix()
        if path.is_file():
            files[relative] = (_sha(path), path.stat().st_mtime_ns)
        elif path.is_dir():
            directories.append(relative)
    return {'files': files, 'directories': directories}


def _schema(*, namespace='urn:comparison', root='order', compositor='sequence',
            order=('customer', 'note', 'tags'), minimum=0, maximum=2,
            customer_name='customer', customer_form=None, customer_default=None,
            customer_fixed=None, customer_length=None, customer_type='xs:string',
            note_minimum=0, omit_note=False, id_element=False, id_use='required',
            id_fixed=None, extra_required=False):
    """Author independent schemas while retaining one unchanged native field inventory."""
    properties = ' name="' + customer_name + '"'
    if customer_form is not None:
        properties += ' form="' + customer_form + '"'
    if customer_default is not None:
        properties += ' default="' + customer_default + '"'
    if customer_fixed is not None:
        properties += ' fixed="' + customer_fixed + '"'
    if customer_length is None:
        customer = '<xs:element' + properties + ' type="' + customer_type + '"/>'
    else:
        customer = ('<xs:element' + properties + '><xs:simpleType><xs:restriction base="xs:string">'
                    '<xs:maxLength value="' + str(customer_length) + '"/>'
                    '</xs:restriction></xs:simpleType></xs:element>')
    fields = {
        'customer': customer,
        'note': '' if omit_note else '<xs:element name="note" type="xs:string" minOccurs="' + str(note_minimum) + '"/>',
        'tags': '<xs:element name="tags" type="xs:string" minOccurs="' + str(minimum) + '" maxOccurs="' + str(maximum) + '"/>',
    }
    children = ''.join(fields[name] for name in order)
    if id_element:
        children = '<xs:element name="id" type="xs:unsignedInt"/>' + children
    if extra_required:
        children += '<xs:element name="mandatory" type="xs:string"/>'
    identity = '' if id_element else '<xs:attribute name="id" type="xs:unsignedInt" use="' + id_use + '"'
    if identity:
        if id_fixed is not None:
            identity += ' fixed="' + id_fixed + '"'
        identity += '/>'
    return ('<?xml version="1.0" encoding="UTF-8"?>\n'
            '<xs:schema xmlns:xs="' + XML_NAMESPACE + '" targetNamespace="' + namespace + '" elementFormDefault="qualified">'
            '<xs:element name="' + root + '"><xs:complexType><xs:' + compositor + '>' + children +
            '</xs:' + compositor + '>' + identity + '</xs:complexType></xs:element></xs:schema>\n')


def _native_contract(path, singleton=False):
    """Execute actual generated native APIs and compare their bytes to independent literals."""
    name = '_serializer_xsd_comparison_contract'
    module = types.ModuleType(name)
    module.__file__ = str(path)
    previous = sys.modules.get(name)
    sys.modules[name] = module
    try:
        # Compiling source directly avoids bytecode cache residue in the observed fixture tree.
        exec(compile(path.read_text(encoding='utf-8'), str(path), 'exec'), module.__dict__)
        value = module.ComparisonOrder()
        assert value.id == 7 and value.customer == 'native default'
        assert value.note == () and value.tags == (() if singleton else [])
        assert value.has_id(7) and not value.has_id(8)
        defaults = value.encode(module.Protocol.BINARY_NONE)
        expected = SINGLETON_NATIVE_HEX if singleton else NATIVE_HEX
        assert defaults.hex() == expected['defaults'], 'Native defaults or declaration order changed'
        value.customer, value.note = 'R&D', ('memo',)
        value.tags = ('A',) if singleton else ['A', 'B']
        assert value.has_id(7) and not value.has_id(8)
        observed = {'defaults': hashlib.sha256(defaults).hexdigest()}
        for label, protocol in (('binary_none', module.Protocol.BINARY_NONE),
                                ('binary_integer', module.Protocol.BINARY_INTEGER),
                                ('binary_string', module.Protocol.BINARY_STRING)):
            payload = value.encode(protocol)
            assert payload.hex() == expected[label], 'Native identity/order changed: ' + label
            observed[label] = hashlib.sha256(payload).hexdigest()
        return {'native_fields': [['id', 37], ['customer', 11], ['note', 83], ['tags', 97]],
                'model_profile': 'singleton_particles' if singleton else 'collection_occurrence',
                'readonly_checks': 4, 'wire_sha256': observed}
    finally:
        if previous is None:
            sys.modules.pop(name, None)
        else:
            sys.modules[name] = previous


def qualify(compiler: Path, directory: Path, invoke) -> dict:
    """Require supported reader directions and precise nonpublishing preflight/capability failures.

    invoke prepends the actual compiler and captures its diagnostics. All created
    inputs and useful generated artifacts remain below directory; no external
    schema or model is changed. A fresh owned subdirectory prevents stale outputs
    from satisfying successful publication or hiding a rejected partial result.
    """
    compiler, directory = Path(compiler).resolve(strict=True), Path(directory).resolve()
    work = directory / 'directional-comparison'
    assert not work.exists(), 'Use a fresh directional-comparison evidence directory'
    work.mkdir(parents=True)
    decisions = []
    runtime_paths = [Path(sys.executable).resolve(strict=True)]
    if sys.platform == 'win32':
        runtime_paths.append(Path(sys.base_prefix) / f'python{sys.version_info.major}{sys.version_info.minor}.dll')
    else:
        library_directory = sysconfig.get_config_var('LIBDIR')
        library_name = sysconfig.get_config_var('LDLIBRARY')
        if library_directory and library_name:
            candidate = Path(library_directory) / library_name
            if candidate.is_file():
                runtime_paths.append(candidate.resolve(strict=True))
    runtime_artifacts = {str(path): _sha(path) for path in runtime_paths}

    def paths(identity, singleton=False):
        """Create one isolated invocation scope with unchanged authored native source."""
        target = work / identity
        target.mkdir()
        source = target / 'model.serializer'
        source.write_text(SINGLETON_MODEL if singleton else MODEL, encoding='utf-8', newline='\n')
        return target, source, target / 'model.hpp', target / 'model.py', target / 'model.d'

    def generate(source, schema, cpp, python, depfile, identity, *extra):
        """Prepare both real generated contracts and a depfile through one atomic preflight batch."""
        return invoke(['--input', source, '--xml.enabled', 'true', '--xml.xsd', schema,
                       '--language', 'cpp,python', '--cpp.output', cpp, '--python.output', python,
                       '--cpp.format', 'false', '--depfile', depfile, *extra], 'comparison-' + identity)

    def record(identity, classification, result, proof, **extra):
        """Retain unique semantic decision IDs and exact captured compiler diagnostics."""
        decision_id = 'comparison.' + identity
        assert decision_id not in {row['id'] for row in decisions}, 'Duplicate comparison decision ID'
        decisions.append({'id': decision_id, 'classification': classification, 'returncode': result.returncode,
                          'stdout': result.stdout, 'stderr': result.stderr, 'native_contract': proof, **extra})

    # Export/rebind is checked in both reader directions, including exact re-export bytes.
    target, source, cpp, python, depfile = paths('exact-export-rebind')
    original, exported, repeated = target / 'original.xsd', target / 'exported.xsd', target / 'reexported.xsd'
    original.write_text(_schema(), encoding='utf-8', newline='\n')
    generate(source, original, cpp, python, depfile, 'exact-export', '--xml.export-xsd', exported)
    namespace = '{' + XML_NAMESPACE + '}'
    tree = ET.fromstring(exported.read_bytes())
    assert tree.attrib['targetNamespace'] == 'urn:comparison'
    assert [element.attrib['name'] for element in tree.findall(namespace + 'element')] == ['order']
    proof = _native_contract(python)
    for identity, current, previous in (('export-rebind-forward', exported, original),
                                       ('export-rebind-reverse', original, exported)):
        result = generate(source, current, cpp, python, depfile, identity,
                          '--xml.compare-xsd', previous, '--xml.export-xsd', repeated)
        assert COMPATIBLE in result.stdout and 'XML incompatibility:' not in result.stderr
        assert _native_contract(python) == proof
        assert repeated.read_bytes() == exported.read_bytes(), 'Export/rebind changed the exact contract'
        assert current.name in depfile.read_text(encoding='utf-8') and previous.name in depfile.read_text(encoding='utf-8')
        record(identity, 'compatible', result, proof, direction='current accepts previous')

    cases = []

    def case(identity, previous, current, classification='compatible', diagnostic=None, singleton=False):
        """Declare a literal supported direction without consulting generated compatibility output."""
        cases.append((identity, previous, current, classification, diagnostic, singleton))

    base = _schema()
    bounded = _schema(maximum=5)
    unbounded = _schema(maximum='unbounded')
    case('occurrence-max-widen', base, bounded)
    case('occurrence-max-tighten', bounded, base, 'incompatible', '.tags: ' + OCCURRENCE)
    case('occurrence-unbounded-widen', bounded, unbounded)
    case('occurrence-unbounded-tighten', unbounded, bounded, 'incompatible', '.tags: ' + OCCURRENCE)
    required_tags = _schema(minimum=1, maximum=5)
    case('occurrence-min-tighten', bounded, required_tags, 'incompatible', '.tags: ' + OCCURRENCE)
    case('occurrence-min-widen', required_tags, bounded)
    sequence, all_group = _schema(maximum=1), _schema(compositor='all', maximum=1)
    reordered_all = _schema(compositor='all', maximum=1, order=('note', 'customer', 'tags'))
    case('sequence-to-all', sequence, all_group, singleton=True)
    case('all-to-sequence', all_group, sequence, 'incompatible', 'Unordered all-group membership or occurrence changed', singleton=True)
    case('all-reorder-forward', all_group, reordered_all, singleton=True)
    case('all-reorder-reverse', reordered_all, all_group, singleton=True)
    reordered_sequence = _schema(order=('note', 'customer', 'tags'))
    case('sequence-reorder-forward', base, reordered_sequence, 'incompatible', 'Element sequence order changed')
    case('sequence-reorder-reverse', reordered_sequence, base, 'incompatible', 'Element sequence order changed')
    case('root-namespace-forward', base, _schema(namespace='urn:comparison:changed'), 'incompatible', 'Root expanded name changed')
    case('root-namespace-reverse', _schema(namespace='urn:comparison:changed'), base, 'incompatible', 'Root expanded name changed')
    case('member-namespace-forward', base, _schema(customer_form='unqualified'), 'incompatible', '.customer: ' + PLACEMENT)
    case('member-namespace-reverse', _schema(customer_form='unqualified'), base, 'incompatible', '.customer: ' + PLACEMENT)
    case('placement-attribute-to-element', base, _schema(id_element=True), 'incompatible', '.id: ' + PLACEMENT)
    case('placement-element-to-attribute', _schema(id_element=True), base, 'incompatible', '.id: ' + PLACEMENT)
    for label, before, after in (
            ('default', _schema(customer_default='before'), _schema(customer_default='after')),
            ('fixed', _schema(customer_fixed='before'), _schema(customer_fixed='after')),
            ('default-to-fixed', _schema(customer_default='same'), _schema(customer_fixed='same')),
            ('facet', _schema(customer_length=32), _schema(customer_length=64))):
        case(label + '-forward', before, after, 'incompatible', '.customer: ' + SCALAR)
        case(label + '-reverse', after, before, 'incompatible', '.customer: ' + SCALAR)
    optional_id = _schema(id_use='optional', id_fixed='7')
    required_id = _schema(id_fixed='7')
    case('required-attribute-tighten', optional_id, required_id, 'incompatible', '.id: ' + OCCURRENCE)
    case('required-attribute-widen', required_id, optional_id)
    for identity, current, diagnostic in (
            ('renamed-member-unavailable', _schema(customer_name='client'), 'XSD member binding is missing or ambiguous: client'),
            ('renamed-root-unavailable', _schema(root='renamed'), 'No XSD record matches: order'),
            ('missing-member-unavailable', _schema(omit_note=True), 'Serializer member is absent from XSD contract'),
            ('new-required-member-unavailable', _schema(extra_required=True), 'XSD member binding is missing or ambiguous: mandatory'),
            ('required-optional-member-unavailable', _schema(note_minimum=1), 'XSD presence/cardinality differs from Serializer member: note'),
            ('scalar-type-unavailable', _schema(customer_type='xs:int'), 'Optimized XSD field type is unsupported or differs from Serializer: customer')):
        case(identity, base, current, 'association_unavailable', diagnostic)
    # This case guards the real sequence-to-unordered widening independently of declaration order.
    case('sequence-to-reordered-all', sequence, reordered_all, singleton=True)

    for identity, previous_text, current_text, classification, diagnostic, singleton in cases:
        target, source, cpp, python, depfile = paths(identity, singleton)
        previous, current = target / 'previous.xsd', target / 'current.xsd'
        previous.write_text(previous_text, encoding='utf-8', newline='\n')
        current.write_text(current_text, encoding='utf-8', newline='\n')
        generate(source, previous, cpp, python, depfile, identity + '-seed')
        proof = _native_contract(python, singleton)
        before = _snapshot(target)
        result = invoke(['--input', source, '--xml.enabled', 'true', '--xml.xsd', current,
                         '--xml.compare-xsd', previous, '--language', 'cpp,python', '--cpp.output', cpp,
                         '--python.output', python, '--cpp.format', 'false', '--depfile', depfile,
                         '--xml.export-xsd', target / 'attempted-export.xsd'],
                        'comparison-' + identity, success=classification == 'compatible')
        if classification == 'compatible':
            assert COMPATIBLE in result.stdout and 'XML incompatibility:' not in result.stderr
            assert _native_contract(python, singleton) == proof
            assert current.name in depfile.read_text(encoding='utf-8') and previous.name in depfile.read_text(encoding='utf-8')
        else:
            assert diagnostic in result.stderr, (identity, result.stderr)
            assert ('XML incompatibility:' in result.stderr) == (classification == 'incompatible')
            assert _snapshot(target) == before, 'Rejected comparison changed outputs, depfiles or staging inventory: ' + identity
        record(identity, classification, result, proof, expected_diagnostic=diagnostic,
               publication_unchanged=classification != 'compatible')

    # Binding an all group does not imply an emitted repeated-storage codec for each backend.
    for language, suffix in (('cpp', 'hpp'), ('java', 'java'), ('csharp', 'cs'), ('go', 'go'), ('kotlin', 'kt'),
                             ('swift', 'swift'), ('rust', 'rs'), ('c', 'h')):
        identity = 'all-repeated-storage-' + language + '-unavailable'
        target, source, cpp, python, depfile = paths(identity)
        current = target / 'all.xsd'
        seed = target / 'seed.xsd'
        current.write_text(all_group, encoding='utf-8', newline='\n')
        seed.write_text(base, encoding='utf-8', newline='\n')
        generate(source, seed, cpp, python, depfile, identity + '-seed')
        proof = _native_contract(python)
        before = _snapshot(target)
        outputs = ['--language', 'cpp', '--cpp.output', cpp] if language == 'cpp' else [
            '--language', 'cpp,' + language, '--cpp.output', cpp,
            '--' + language + '.output', target / ('unpublished.' + suffix)]
        result = invoke(['--input', source, '--xml.enabled', 'true', '--xml.xsd', current,
                         *outputs, '--cpp.format', 'false', '--depfile', depfile],
                        'comparison-' + identity, success=False)
        expected = ('This C++ XML contract requires an unsupported direct token mapping combination: comparison::order'
                    if language == 'cpp' else
                    'XSD all is compiled; generated unordered-child decoding is not implemented for ' + language)
        assert expected in result.stderr and 'XML incompatibility:' not in result.stderr
        assert _snapshot(target) == before, 'Capability rejection changed generated outputs or left staging residue'
        record(identity, 'backend_unavailable', result, proof, expected_diagnostic=expected,
               supported_profile='singleton_particles', rejected_backend=language, publication_unchanged=True)

    artifacts = {str(path): _sha(path) for path in work.rglob('*') if path.is_file()}
    assert all(_sha(path) == digest for path, digest in runtime_artifacts.items()), 'Native proof runtime changed'
    artifacts.update(runtime_artifacts)
    return {'status': 'passed', 'compiler_sha256': _sha(compiler), 'decision_count': len(decisions),
            'classification_counts': dict(Counter(row['classification'] for row in decisions)),
            'decisions': decisions, 'artifacts': artifacts,
            'native_proof_runtime': {'implementation': sys.implementation.name, 'version': sys.version},
            'scope': 'Directional supported bound contracts; unavailable associations and backend capabilities remain distinct. '
                     'Native storage is held fixed within each singleton-particle or collection-occurrence profile. '
                     'Literal native defaults, names, IDs, declaration order and readonly APIs are retained; no rename migration is inferred.'}
