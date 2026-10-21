# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify exact fixed-XSD bundle limits, canonical paths and independent identities."""

from collections import Counter
import hashlib
import os
from pathlib import Path
import re
import subprocess
from xml.sax.saxutils import quoteattr


_bundle_bytes = 16 * 1024 * 1024
_logical_instances = 64
_reference_depth = 32
_namespace = 'urn:serializer:xsd-tooling-limits'
_root_declaration = ('<xs:element name="order"><xs:complexType>'
                     '<xs:attribute name="id" type="xs:unsignedInt" use="required"/>'
                     '</xs:complexType></xs:element>')
_graph_limit_diagnostic = 'XSD bundle document/depth limit exceeded'
_byte_limit_diagnostic = 'XSD bundle exceeds aggregate schema byte limit'
_relative_path_diagnostic = 'XSD references require explicit relative local schemaLocation paths'
_escape_diagnostic = 'XSD schemaLocation escapes its pinned bundle directory'
_namespace_diagnostic = 'XSD include/import targetNamespace mismatch'


def _digest(path):
    """Hash complete fixture/output bytes without interpreting their encoding or wire format."""
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _schema(body='', namespace=_namespace, padding=0):
    """Build valid schema annotation padding; individual text nodes remain below parser-local caps."""
    target = (' targetNamespace=' + quoteattr(namespace)) if namespace else ''
    return ('<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"' + target +
            ' elementFormDefault="qualified"><xs:annotation><xs:documentation>' +
            'x' * padding + '</xs:documentation></xs:annotation>' + body + '</xs:schema>').encode('utf-8')


def _include(path):
    """Retain the exact authored schemaLocation rather than canonicalizing fixture input strings."""
    return '<xs:include schemaLocation=' + quoteattr(path) + '/>'


def _import(path, namespace):
    """Name an explicit local namespace import for the independently authored instance inventory."""
    return '<xs:import namespace=' + quoteattr(namespace) + ' schemaLocation=' + quoteattr(path) + '/>'


def _write(directory, documents):
    """Materialize only explicitly named fixture documents below the current case directory."""
    for name, payload in documents.items():
        path = directory / name
        assert path.resolve().is_relative_to(directory.resolve())
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(payload)


def _snapshot(directory):
    """Record regular bytes and link metadata without traversing a junction or symbolic link."""
    result = {}
    for current, directories, files in os.walk(directory, followlinks=False):
        current = Path(current)
        retained = []
        for name in sorted(directories):
            path = current / name
            key = path.relative_to(directory).as_posix()
            if path.is_symlink() or path.is_junction():
                result[key] = {'kind': 'junction' if path.is_junction() else 'symlink',
                               'target': os.readlink(path)}
            else:
                result[key] = {'kind': 'directory'}
                retained.append(name)
        directories[:] = retained
        for name in sorted(files):
            path = current / name
            key = path.relative_to(directory).as_posix()
            if path.is_symlink():
                result[key] = {'kind': 'symlink', 'target': os.readlink(path)}
            else:
                result[key] = {'kind': 'file', 'bytes': path.stat().st_size, 'sha256': _digest(path)}
    return result


def _identity(directory, instances):
    """Apply documented length framing to an authored logical inventory, not compiler metadata."""
    digest = hashlib.sha256(b'Serializer XSD bundle 1\n')
    counts = Counter(name for name, _ in instances)
    for name, namespace in sorted(instances):
        values = [name.encode('utf-8'), (directory / name).read_bytes()]
        if counts[name] > 1:
            values.append(namespace.encode('utf-8'))
        for value in values:
            digest.update(str(len(value)).encode('ascii') + b':' + value)
    return digest.hexdigest()


def _dependency_path(path):
    """Independently quote the documented absolute UTF-8 Make path contract."""
    result = ''
    for character in Path(path).absolute().as_posix():
        if character == '$':
            result += '$$'
        else:
            result += ('\\' if character in ' \t#:\\' else '') + character
    return result


class _qualification:
    """Own one fresh fixture tree and retain every compiler decision, including unexpected failures."""

    def __init__(self, compiler, directory, invoke):
        """Borrow the caller's command logger; keep all fixture mutations below its fresh directory."""
        self.compiler = Path(compiler).resolve(strict=True)
        self.compiler_sha256 = _digest(self.compiler)
        self.directory = Path(directory).resolve()
        assert not self.directory.exists() or not any(self.directory.iterdir()), 'Use a fresh limits directory'
        self.directory.mkdir(parents=True, exist_ok=True)
        self.invoke = invoke
        self.decisions = []
        self.failures = []
        self.links = []

    def case(self, name, documents):
        """Reserve one independent case with a bundle root and complete literal input inventory."""
        directory = self.directory / name
        directory.mkdir(parents=True)
        _write(directory / 'bundle', documents)
        return directory

    def decision(self, identifier, directory, arguments, diagnostic=None, check=None, unchanged=False):
        """Require one actual exit/diagnostic decision and retain before/after input/output snapshots."""
        assert all(row['id'] != identifier for row in self.decisions), 'Duplicate qualification decision'
        before = _snapshot(directory)
        row = {'id': identifier, 'expected': 'reject' if diagnostic else 'accept',
               'arguments': [str(value) for value in arguments], 'before': before, 'status': 'failed'}
        try:
            result = self.invoke(row['arguments'], identifier, success=diagnostic is None)
            row.update({'returncode': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr})
            assert (result.returncode == 0) == (diagnostic is None)
            if diagnostic:
                row['expected_diagnostic'] = diagnostic
                assert diagnostic in result.stderr, 'Missing exact expected diagnostic: ' + diagnostic
            if check:
                row.update(check(result))
            after = _snapshot(directory)
            row['after'] = after
            if diagnostic or unchanged:
                assert before == after, 'Rejected/idempotent operation changed the fixture inventory'
            assert not any(Path(name).name.startswith('.serializer-xsd-') for name in after), 'Staging residue'
            row['status'] = 'passed'
        except Exception as error:
            row['error'] = repr(error)
            row['after'] = _snapshot(directory)
            self.failures.append({'id': identifier, 'error': repr(error)})
        self.decisions.append(row)
        return row

    def import_case(self, identifier, directory, instances=None, diagnostic=None, unchanged=False):
        """Import a complete logical graph and verify its pin plus every ordered physical dependency."""
        source, depfile = directory / 'generated.serializer', directory / 'generated.d'
        if diagnostic and not source.exists():
            source.write_bytes(b'// Previously reviewed source must remain unchanged.\n')
            depfile.write_bytes(b'previous: dependency\n')
        arguments = ['--import-xsd', directory / 'bundle/root.xsd', '--namespace', 'limits',
                     '--root', 'order', '--output', source, '--depfile', depfile]

        def check(result):
            """Compare compiler identity/dependencies to the fixture's independently declared instances."""
            expected = _identity(directory / 'bundle', instances)
            match = re.search(r'XSD bundle SHA-256 ([0-9a-f]{64})', result.stdout)
            assert match and match.group(1) == expected, 'Documented bundle identity differs'
            expected_deps = _dependency_path(source) + ':'
            expected_deps += ''.join(' ' + _dependency_path(directory / 'bundle' / name)
                                     for name, _ in sorted(instances)) + '\n'
            assert depfile.read_text(encoding='utf-8') == expected_deps, 'Exact transitive depfile differs'
            assert b'using schema "bundle/root.xsd";' in source.read_bytes()
            return {'identity': expected, 'logical_instances': len(instances),
                    'physical_documents': len({name for name, _ in instances}),
                    'logical_source_bytes': sum((directory / 'bundle' / name).stat().st_size
                                                for name, _ in instances),
                    'physical_source_bytes': sum((directory / 'bundle' / name).stat().st_size
                                                 for name in {name for name, _ in instances}),
                    'dependencies_sha256': _digest(depfile)}

        return self.decision(identifier, directory, arguments, diagnostic,
                             check if diagnostic is None else None, unchanged)

    def pinned_bind(self, identifier, directory, instances, mismatch=False):
        """Carry an independently computed pin into real generation before publishing Python/dependencies."""
        if not (directory / 'generated.serializer').is_file():
            row = {'id': identifier, 'expected': 'accept', 'status': 'blocked',
                   'error': 'Prerequisite declaration import failed; no generated source exists'}
            self.decisions.append(row)
            self.failures.append({'id': identifier, 'error': row['error']})
            return row
        expected = _identity(directory / 'bundle', instances)
        pin = ('0' if expected[0] != '0' else '1') + expected[1:] if mismatch else expected
        output, depfile = directory / 'generated.py', directory / 'generated.py.d'
        if mismatch and not output.exists():
            output.write_bytes(b'# Previous generated output remains unchanged.\n')
            depfile.write_bytes(b'previous.py: dependency\n')
        diagnostic = ('XML XSD bundle identity mismatch: expected ' + pin + ', actual ' + expected) if mismatch else None
        return self.decision(identifier, directory,
                             ['--input', directory / 'generated.serializer', '--language', 'python',
                              '--output', output, '--depfile', depfile, '--xml.xsd', directory / 'bundle/root.xsd',
                              '--xml.bundle_sha256', pin], diagnostic)

    def link(self, alias, target):
        """Create a real local directory junction/symlink; both endpoints stay inside this owned tree."""
        assert alias.parent.resolve().is_relative_to(self.directory)
        assert target.resolve().is_relative_to(self.directory)
        if os.name == 'nt':
            arguments = [os.environ.get('COMSPEC', 'cmd.exe'), '/d', '/c', 'mklink', '/J', str(alias), str(target)]
            result = subprocess.run(arguments, capture_output=True, text=True, errors='replace')
            self.links.append({'kind': 'junction', 'arguments': arguments, 'returncode': result.returncode,
                               'stdout': result.stdout, 'stderr': result.stderr})
            assert result.returncode == 0 and alias.is_junction(), 'A real junction is required'
        else:
            alias.symlink_to(target, target_is_directory=True)
            self.links.append({'kind': 'symlink', 'alias': str(alias), 'target': str(target)})
            assert alias.is_symlink()


def _ordinary_instances(documents):
    """Declare one same-namespace instance per literal physical fixture document."""
    return [(name, _namespace) for name in documents]


def _graph_cases(suite):
    """Exercise exact logical instance/depth admission, deduplication and legal cycle boundaries."""
    for count in (_logical_instances, _logical_instances + 1):
        documents = {'part' + str(index) + '.xsd': _schema() for index in range(count - 1)}
        documents['root.xsd'] = _schema(''.join(_include(name) for name in documents) + _root_declaration)
        case = suite.case('instances-' + str(count), documents)
        suite.import_case('limits.instances.' + str(count), case, _ordinary_instances(documents),
                          _graph_limit_diagnostic if count > _logical_instances else None)
    for depth in (_reference_depth, _reference_depth + 1):
        documents = {'root.xsd': _schema(_include('chain1.xsd') + _root_declaration)}
        for index in range(1, depth + 1):
            documents['chain' + str(index) + '.xsd'] = _schema(
                _include('chain' + str(index + 1) + '.xsd') if index < depth else '')
        case = suite.case('depth-' + str(depth), documents)
        suite.import_case('limits.depth.' + str(depth), case, _ordinary_instances(documents),
                          _graph_limit_diagnostic if depth > _reference_depth else None)
        if depth == _reference_depth:
            documents['chain' + str(depth) + '.xsd'] = _schema(_include('root.xsd'))
            cyclic = suite.case('depth-32-with-backedge', documents)
            suite.import_case('limits.depth.cycle-backedge-after-32', cyclic, _ordinary_instances(documents))

    documents = {'part' + str(index) + '.xsd': _schema() for index in range(_logical_instances - 1)}
    documents['root.xsd'] = _schema(''.join(_include(name) for name in documents) +
                                    ''.join(_include('part0.xsd') for _ in range(80)) +
                                    _include('root.xsd') + _root_declaration)
    case = suite.case('dedup-at-limit', documents)
    suite.import_case('limits.instances.dedup-cycle-at-64', case, _ordinary_instances(documents))

    for count in (_logical_instances, _logical_instances + 1):
        documents = {'common.xsd': _schema(namespace='')}
        instances = [('root.xsd', _namespace)]
        imports = ''
        for index in range(30):
            namespace = 'urn:serializer:adopted:' + str(index)
            name = 'adopt' + str(index) + '.xsd'
            documents[name] = _schema(_include('common.xsd'), namespace)
            imports += _import(name, namespace)
            instances.extend(((name, namespace), ('common.xsd', namespace)))
        for index in range(count - len(instances)):
            name = 'filler' + str(index) + '.xsd'
            documents[name] = _schema()
            imports += _include(name)
            instances.append((name, _namespace))
        documents['root.xsd'] = _schema(imports + _root_declaration)
        case = suite.case('adopted-instances-' + str(count), documents)
        suite.import_case('limits.chameleon.instances.' + str(count), case, instances,
                          _graph_limit_diagnostic if count > _logical_instances else None)


def _byte_cases(suite):
    """Admit exact aggregate source bytes and charge repeated chameleon bytes per logical instance."""
    documents = {'root.xsd': _schema(''.join(_include('padding' + str(index) + '.xsd')
                                            for index in range(3)) + _root_declaration)}
    base = _schema()
    remainder = _bundle_bytes - len(documents['root.xsd']) - 3 * len(base)
    for index in range(3):
        amount = remainder // (3 - index)
        remainder -= amount
        documents['padding' + str(index) + '.xsd'] = _schema(padding=amount)
    assert sum(map(len, documents.values())) == _bundle_bytes
    case = suite.case('aggregate-bytes', documents)
    suite.import_case('limits.bytes.exact-16mib', case, _ordinary_instances(documents))
    documents['padding2.xsd'] = documents['padding2.xsd'].replace(b'<xs:documentation>', b'<xs:documentation>x', 1)
    over = suite.case('aggregate-bytes-one-over', documents)
    suite.import_case('limits.bytes.one-over-16mib', over, diagnostic=_byte_limit_diagnostic)

    documents = {
        'root.xsd': _schema(_import('left.xsd', 'urn:left') + _import('right.xsd', 'urn:right') + _root_declaration),
        'left.xsd': _schema(_include('common.xsd'), 'urn:left'),
        'right.xsd': _schema(_include('common.xsd'), 'urn:right'),
    }
    common_base = _schema(namespace='')
    fixed = sum(map(len, documents.values()))
    if (_bundle_bytes - fixed - 2 * len(common_base)) % 2:
        documents['root.xsd'] = documents['root.xsd'].replace(b'<xs:documentation>', b'<xs:documentation>x', 1)
        fixed += 1
    common_padding = (_bundle_bytes - fixed - 2 * len(common_base)) // 2
    documents['common.xsd'] = _schema(namespace='', padding=common_padding)
    instances = [('root.xsd', _namespace), ('left.xsd', 'urn:left'), ('right.xsd', 'urn:right'),
                 ('common.xsd', 'urn:left'), ('common.xsd', 'urn:right')]
    assert sum(len(documents[name]) for name, _ in instances) == _bundle_bytes
    case = suite.case('chameleon-bytes', documents)
    suite.import_case('limits.chameleon.bytes.exact-16mib', case, instances)
    documents['right.xsd'] = documents['right.xsd'].replace(b'<xs:documentation>', b'<xs:documentation>x', 1)
    assert sum(map(len, documents.values())) < _bundle_bytes
    assert sum(len(documents[name]) for name, _ in instances) == _bundle_bytes + 1
    over = suite.case('chameleon-bytes-one-over', documents)
    suite.import_case('limits.chameleon.bytes.one-over-16mib', over, diagnostic=_byte_limit_diagnostic)


def _path_cases(suite):
    """Require explicit local paths, real canonical containment and complete Unicode/space path handling."""
    for label, location, diagnostic in (
            ('https', 'https://example.invalid/no-fetch.xsd', _relative_path_diagnostic),
            ('file-uri', 'file:///no-fetch.xsd', _relative_path_diagnostic),
            ('drive', 'C:/no-fetch.xsd', _relative_path_diagnostic),
            ('absolute', '/no-fetch.xsd', 'Absolute XSD schemaLocation is prohibited'),
            ('percent', 'types%2exsd', _relative_path_diagnostic),
            ('query', 'types.xsd?query', _relative_path_diagnostic),
            ('fragment', 'types.xsd#fragment', _relative_path_diagnostic),
            ('backslash', 'sub\\types.xsd', _relative_path_diagnostic),
            ('missing-location', '', _relative_path_diagnostic)):
        documents = {'root.xsd': _schema(_include(location) + _root_declaration)}
        case = suite.case('path-' + label, documents)
        suite.import_case('paths.reject.' + label, case, diagnostic=diagnostic)

    case = suite.case('parent-escape', {'root.xsd': _schema(_include('../outside.xsd') + _root_declaration)})
    (case / 'outside.xsd').write_bytes(_schema())
    suite.import_case('paths.reject.parent-escape', case, diagnostic=_escape_diagnostic)

    for label, relative in (('spaces', 'typed documents/types.xsd'),
                            ('unicode-child', 'caf\u00e9-\u6f22.xsd')):
        documents = {'root.xsd': _schema(_include(relative) + _root_declaration), relative: _schema()}
        case = suite.case('path-' + label, documents)
        suite.import_case('paths.accept.' + label, case, _ordinary_instances(documents))
        suite.pinned_bind('paths.bind.' + label, case, _ordinary_instances(documents))

    documents = {'root.xsd': _schema(_include('types.xsd') + _root_declaration), 'types.xsd': _schema()}
    case = suite.case('root spaces caf\u00e9 \u6f22', documents)
    suite.import_case('paths.accept.unicode-cli-root', case, _ordinary_instances(documents))
    suite.pinned_bind('paths.bind.unicode-cli-root', case, _ordinary_instances(documents))

    documents = {'root.xsd': _schema(_include('./nested/../types.xsd') + _include('types.xsd') + _root_declaration),
                 'types.xsd': _schema()}
    case = suite.case('canonical-alias', documents)
    (case / 'bundle/nested').mkdir()
    suite.import_case('paths.accept.canonical-in-bundle-alias', case, _ordinary_instances(documents))

    for outside in (False, True):
        case = suite.case('link-' + ('outside' if outside else 'inside'),
                          {'root.xsd': _schema(_include('alias/types.xsd') + _root_declaration)})
        target = case / ('external' if outside else 'bundle/real')
        target.mkdir()
        (target / 'types.xsd').write_bytes(_schema())
        try:
            suite.link(case / 'bundle/alias', target)
            suite.import_case('paths.' + ('reject.canonical-link-escape' if outside else 'accept.canonical-link-alias'),
                              case, None if outside else [('root.xsd', _namespace), ('real/types.xsd', _namespace)],
                              _escape_diagnostic if outside else None)
        except Exception as error:
            identifier = 'paths.setup.' + ('outside' if outside else 'inside') + '-link'
            suite.decisions.append({'id': identifier, 'status': 'failed', 'error': repr(error)})
            suite.failures.append({'id': identifier, 'error': repr(error)})

    for label, body, child_namespace, diagnostic in (
            ('include-target', _include('types.xsd'), 'urn:other', _namespace_diagnostic),
            ('import-target', _import('types.xsd', 'urn:requested'), 'urn:other', _namespace_diagnostic),
            ('same-namespace-import', _import('types.xsd', _namespace), _namespace,
             'XSD import must select another namespace')):
        case = suite.case('namespace-' + label,
                          {'root.xsd': _schema(body + _root_declaration), 'types.xsd': _schema(namespace=child_namespace)})
        suite.import_case('paths.reject.' + label, case, diagnostic=diagnostic)

    documents = {
        'root.xsd': _schema(_import('foreign.xsd', 'urn:foreign') + _include('types.xsd') + _root_declaration),
        'foreign.xsd': _schema('<xs:simpleType name="Amount"><xs:restriction base="xs:unsignedInt"/></xs:simpleType>',
                               'urn:foreign'),
        'types.xsd': _schema('<xs:simpleType name="Alias" xmlns:f="urn:foreign">'
                             '<xs:restriction base="f:Amount"/></xs:simpleType>'),
    }
    case = suite.case('source-document-import', documents)
    suite.import_case('paths.reject.missing-source-document-import', case,
                      diagnostic='XSD QName namespace requires an explicit import in its source document')
    documents['types.xsd'] = _schema(_import('foreign.xsd', 'urn:foreign') +
                                    '<xs:simpleType name="Alias" xmlns:f="urn:foreign">'
                                    '<xs:restriction base="f:Amount"/></xs:simpleType>')
    explicit = suite.case('explicit-source-document-import', documents)
    suite.import_case('paths.accept.explicit-source-document-import', explicit, [
        ('root.xsd', _namespace), ('types.xsd', _namespace), ('foreign.xsd', 'urn:foreign')])


def _identity_cases(suite):
    """Check pins, moved-checkout stability, byte/path changes, exact deps and protected regeneration."""
    documents = {'root.xsd': _schema(_include('types.xsd') + _root_declaration), 'types.xsd': _schema()}
    instances = _ordinary_instances(documents)
    first = suite.case('identity-original', documents)
    suite.import_case('identity.original', first, instances)
    suite.import_case('identity.idempotent', first, instances, unchanged=True)
    suite.pinned_bind('identity.bind-pinned', first, instances)
    suite.pinned_bind('identity.reject-mismatched-pin', first, instances, mismatch=True)
    second = suite.case('identity-relocated', documents)
    suite.import_case('identity.moved-checkout', second, instances)
    assert _identity(first / 'bundle', instances) == _identity(second / 'bundle', instances)

    changed = dict(documents)
    changed['types.xsd'] += b'\n'
    case = suite.case('identity-byte-change', changed)
    suite.import_case('identity.source-byte-change', case, instances)
    assert _identity(first / 'bundle', instances) != _identity(case / 'bundle', instances)

    changed = {'root.xsd': _schema(_include('renamed.xsd') + _root_declaration), 'renamed.xsd': documents['types.xsd']}
    case = suite.case('identity-path-change', changed)
    suite.import_case('identity.relative-path-change', case, _ordinary_instances(changed))
    assert _identity(first / 'bundle', instances) != _identity(case / 'bundle', _ordinary_instances(changed))

    for label, namespace in (('left', 'urn:left'), ('right', 'urn:right')):
        documents = {'root.xsd': _schema(_import('adopt.xsd', namespace) + _root_declaration),
                     'adopt.xsd': _schema(_include('common.xsd'), namespace), 'common.xsd': _schema(namespace='')}
        case = suite.case('identity-adopted-' + label, documents)
        row = suite.import_case('identity.effective-namespace-' + label, case,
                                [('root.xsd', _namespace), ('adopt.xsd', namespace), ('common.xsd', namespace)])
        row['selected_effective_namespace'] = namespace


def qualify(compiler: Path, directory: Path, invoke) -> dict:
    """Run every exact boundary/path/pin case; return failures without skipping independent decisions.

    invoke(arguments, label, success=True) prepends the compiler, retains actual
    stdout/stderr and returns CompletedProcess. Fixture files and real local links
    remain below directory as reproducible evidence; no user paths are removed.
    """
    suite = _qualification(compiler, directory, invoke)
    for family, action in (('graphs', _graph_cases), ('bytes', _byte_cases),
                           ('paths', _path_cases), ('identities', _identity_cases)):
        try:
            action(suite)
        except Exception as error:
            suite.failures.append({'family': family, 'error': repr(error)})
    if _digest(suite.compiler) != suite.compiler_sha256:
        suite.failures.append({'id': 'compiler-unchanged', 'error': 'Compiler changed during qualification'})
    return {'status': 'failed' if suite.failures else 'passed',
            'compiler': str(suite.compiler), 'compiler_sha256': suite.compiler_sha256,
            'limits': {'logical_instances': _logical_instances, 'maximum_first_visit_depth': _reference_depth,
                       'aggregate_logical_source_bytes': _bundle_bytes},
            'decisions': suite.decisions, 'failures': suite.failures, 'links': suite.links}
