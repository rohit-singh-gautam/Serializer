# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check real XSD import publication, dependencies and optional authored binding."""
from pathlib import Path
import importlib.util
import re
import subprocess
import sys
import tempfile


def invoke(compiler, *arguments, success=True):
    """Run one command and retain diagnostics for an unexpected exit status."""
    result = subprocess.run([compiler, *map(str, arguments)], capture_output=True, text=True)
    if (result.returncode == 0) != success:
        raise AssertionError(result.stdout + result.stderr)
    return result


def file_snapshot(directory):
    """Retain every ordinary input/output byte so a rejected operation cannot hide side effects."""
    return {str(path.relative_to(directory)): path.read_bytes()
            for path in directory.rglob('*') if path.is_file()}


def stable_id(identity):
    """Compute the published FNV-1a field identity independently of declaration positions."""
    value = 2166136261
    for byte in identity.encode('utf-8'):
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return (value & 0x1fffffff) or 1


def imported_fields(source):
    """Read explicit native names/IDs in emitted declaration order, without assuming a class suffix."""
    return [(name, int(identity)) for name, identity in
            re.findall(r'public\s+\S+\s+(xsd_\w+)\s+\((\d+)\)', source.read_text(encoding='utf-8'))]


def evolution_imports(compiler, root):
    """Qualify staged additions/renames as fresh reviewed imports, never inferred native migrations."""
    directory = root / 'evolution'
    directory.mkdir()
    xsd = directory / 'order.xsd'
    schema_prefix = '''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
      targetNamespace="urn:evolution" elementFormDefault="qualified">
      <xs:element name="order"><xs:complexType><xs:sequence>'''
    schema_suffix = '''</xs:sequence><xs:attribute name="id" type="xs:unsignedInt" use="required"/>
      </xs:complexType></xs:element></xs:schema>'''
    stages = [
        '<xs:element name="customer" type="xs:string"/>',
        '<xs:element name="note" type="xs:string" minOccurs="0"/>'
        '<xs:element name="customer" type="xs:string"/>',
        '<xs:element name="note" type="xs:string" minOccurs="0"/>'
        '<xs:element name="client" type="xs:string"/>',
    ]
    expected = [
        [('xsd_customer', stable_id('{urn:evolution}customer')), ('xsd_id', stable_id('@{}id'))],
        [('xsd_note', stable_id('{urn:evolution}note')),
         ('xsd_customer', stable_id('{urn:evolution}customer')), ('xsd_id', stable_id('@{}id'))],
        [('xsd_note', stable_id('{urn:evolution}note')),
         ('xsd_client', stable_id('{urn:evolution}client')), ('xsd_id', stable_id('@{}id'))],
    ]
    imported = []
    for index, body in enumerate(stages):
        xsd.write_text(schema_prefix + body + schema_suffix, encoding='utf-8')
        source = directory / ('stage' + str(index) + '.serializer')
        depfile = source.with_suffix('.serializer.d')
        command = ('--import-xsd', xsd, '--namespace', 'evolution', '--root', '{urn:evolution}order',
                   '--output', source, '--depfile', depfile)
        invoke(compiler, *command)
        assert imported_fields(source) == expected[index]
        before = file_snapshot(directory)
        invoke(compiler, *command)
        assert file_snapshot(directory) == before, 'Identical import changed bytes or created files'
        invoke(compiler, '--input', source, '--language', 'python', '--output',
               directory / ('stage' + str(index) + '.py'))
        if imported:
            protected, protected_depfile = imported[-1]
            before = file_snapshot(directory)
            failure = invoke(compiler, '--import-xsd', xsd, '--namespace', 'evolution',
                             '--root', '{urn:evolution}order', '--output', protected,
                             '--depfile', protected_depfile, success=False)
            assert 'Import output differs; generate to a new path and review stable field IDs and application edits' in failure.stderr
            assert file_snapshot(directory) == before, 'Changed XSD import modified protected files'
        pristine = source.read_bytes()
        source.write_bytes(pristine + b'\n// Authored application behavior remains protected.\n')
        before = file_snapshot(directory)
        failure = invoke(compiler, *command, success=False)
        assert 'Import output differs' in failure.stderr
        assert file_snapshot(directory) == before, 'Edited import or dependency bytes changed'
        source.write_bytes(pristine)
        imported.append((source, depfile))
    assert expected[1][1:] == expected[0]
    assert expected[2][1][1] != expected[0][0][1], 'Renamed XML identities must not imply native-ID reuse'


def ambiguous_associations(compiler, root):
    """Require exact importer/binder ambiguity failures before any existing artifact is published."""
    directory = root / 'ambiguity'
    directory.mkdir()
    xsd = directory / 'shared.xsd'
    xsd.write_text('''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
      xmlns:t="urn:shared" targetNamespace="urn:shared" elementFormDefault="qualified">
      <xs:complexType name="record"><xs:attribute name="id" type="xs:unsignedInt" use="required"/></xs:complexType>
      <xs:element name="first" type="t:record"/><xs:element name="second" type="t:record"/>
      </xs:schema>''', encoding='utf-8')
    source, depfile = directory / 'protected.serializer', directory / 'protected.d'
    source.write_bytes(b'// Existing authored source must remain unchanged.\n')
    depfile.write_bytes(b'existing target: existing dependency\n')
    before = file_snapshot(directory)
    failure = invoke(compiler, '--import-xsd', xsd, '--output', source, '--depfile', depfile,
                     success=False)
    assert 'Multiple XSD global roots share one complex type; select one root explicitly' in failure.stderr
    assert file_snapshot(directory) == before
    absent = directory / 'unpublished.serializer'
    failure = invoke(compiler, '--import-xsd', xsd, '--output', absent,
                     '--depfile', directory / 'unpublished.d', success=False)
    assert 'Multiple XSD global roots share one complex type' in failure.stderr
    assert file_snapshot(directory) == before, 'Ambiguous import created a partial output or sidecar'
    selected = directory / 'selected.serializer'
    invoke(compiler, '--import-xsd', xsd, '--root', '{urn:shared}first', '--output', selected)
    invoke(compiler, '--input', selected, '--language', 'python', '--output', directory / 'selected.py')

    for name in ('left', 'right'):
        (directory / (name + '.xsd')).write_text('''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
          targetNamespace="urn:''' + name + '''" elementFormDefault="qualified">
          <xs:element name="order"><xs:complexType>
          <xs:attribute name="id" type="xs:unsignedInt" use="required"/>
          </xs:complexType></xs:element></xs:schema>''', encoding='utf-8')
    (directory / 'bundle.xsd').write_text('''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
      <xs:import namespace="urn:left" schemaLocation="left.xsd"/>
      <xs:import namespace="urn:right" schemaLocation="right.xsd"/>
      </xs:schema>''', encoding='utf-8')
    authored = directory / 'manual.serializer'
    authored.write_text('''serializer version 1.20.0;
      namespace authored { using schema "bundle.xsd";
        class order stable_ids { public uint32 id (37) {7}; }
      }''', encoding='utf-8')
    output = directory / 'manual.py'
    output.write_bytes(b'# Existing generated consumer remains unchanged.\n')
    depfile.write_bytes(b'manual.py: previous.serializer previous.xsd\n')
    before = file_snapshot(directory)
    failure = invoke(compiler, '--input', authored, '--language', 'python', '--output', output,
                     '--depfile', depfile, success=False)
    assert 'Ambiguous XSD record; configure its namespace or unique root' in failure.stderr
    assert file_snapshot(directory) == before, 'Ambiguous association changed an existing file'
    authored.write_text(authored.read_text(encoding='utf-8').replace('class order stable_ids {',
                        'class order stable_ids { xml(namespace("urn:left"));'), encoding='utf-8')
    invoke(compiler, '--input', authored, '--language', 'python', '--output', output, '--depfile', depfile)
    assert b'Existing generated consumer' not in output.read_bytes()
    assert 'left.xsd' in depfile.read_text(encoding='utf-8')


def restricted_list_imports(compiler, root):
    """Generate every owning output and execute imported restricted/union lists in Python."""
    directory = root / 'restricted-lists'
    directory.mkdir()
    fixture = Path(__file__).resolve().parent / 'resources' / 'xml_shared_import' / 'root.xsd'
    source = directory / 'lists.serializer'
    invoke(compiler, '--import-xsd', fixture, '--namespace', 'imports', '--root', 'order',
           '--output', source)
    extensions = {'cpp': 'hpp', 'c': 'h', 'rust': 'rs', 'python': 'py', 'js': 'js',
                  'typescript': 'd.mts', 'java': 'java', 'csharp': 'cs', 'go': 'go',
                  'kotlin': 'kt', 'swift': 'swift'}
    for language, extension in extensions.items():
        arguments = ['--input', source, '--language', language,
                     '--output', directory / ('lists.' + extension)]
        if language == 'cpp':
            arguments += ['--cpp.format', 'false']
        invoke(compiler, *arguments)
    spec = importlib.util.spec_from_file_location('imported_restricted_lists', directory / 'lists.py')
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    try:
        spec.loader.exec_module(module)
        model = module.ImportsXsdOrder
        assert module._xml_token_capable(model), 'Imported ordered/namespace graph lacks a token codec'
        def reject_tree(*args, **kwargs):
            """Ensure the combined namespace/ordered profile never falls back to a document tree."""
            raise AssertionError('Unexpected whole-tree XML parser')
        module._xml_parse = reject_tree
        value = model.decode_xml(b"<order xmlns:p='urn:item' id='1' samples='2 8' "
                                 b"selections='p:item 7' codes='AB CD'/>")
        assert value.xsd_samples == ([2, 8],)
        assert value.xsd_codes == (['AB', 'CD'],)
        assert len(value.xsd_selections[0]) == 2
        restored = model.decode_xml(value.encode_xml())
        assert restored.xsd_samples == value.xsd_samples
        assert restored.xsd_codes == value.xsd_codes
        document = "<order xmlns:p='urn:Ω' id='1' selections='p:item 7'/>".encode()
        chunked = model.decode_xml_chunks((document[index:index + 1] for index in range(len(document))))
        assert chunked.xsd_selections == (['p:item', '7'],)
        assert 'urn:Ω'.encode() in chunked.encode_xml()
        # One element and two three-entry namespace snapshots: the namespace ledger,
        # rather than element count, distinguishes these exact/one-tight admissions.
        model.decode_xml(document, module.Limits(max_elements=6))
        try:
            model.decode_xml(document, module.Limits(max_elements=5))
        except ValueError as error:
            assert 'namespace retention' in str(error)
        else:
            raise AssertionError('Namespace snapshot exceeded its entry budget')
        assert model.decode_xml(document).xsd_selections == chunked.xsd_selections
        grouped = model.decode_xml(b"<order id='1'><schedule><price>2</price>"
            b"<item name='a'><price>3</price></item><item name='b'><price>4</price></item>"
            b"<price>5</price></schedule></order>")
        assert len(grouped.xsd_schedule[0].xsd_items) == 4
        assert len(model.decode_xml(grouped.encode_xml()).xsd_schedule[0].xsd_items) == 4
        grouped.xsd_schedule[0].xsd_items.pop()
        try:
            grouped.encode_xml()
        except (ValueError, TypeError):
            pass
        else:
            raise AssertionError('Edited imported ordered group published invalid XML')
        for attributes in ("samples='1 2'", "samples='2 9'", "samples='2'",
                           "samples='2 3 4'", "selections='missing:item'",
                           "selections='-1'", "codes='AB bad'", "codes='A'"):
            try:
                model.decode_xml(('<order id="1" ' + attributes + '/>').encode())
            except (ValueError, TypeError):
                continue
            raise AssertionError('Imported list constraint accepted ' + attributes)
        value.xsd_samples[0][0] = 1
        try:
            value.encode_xml()
        except (ValueError, TypeError):
            pass
        else:
            raise AssertionError('Edited restricted list published invalid XML')
    finally:
        sys.modules.pop(spec.name, None)


def main():
    """Import twice, protect manual edits and compile both authoring workflows."""
    compiler = str(Path(sys.argv[1]).resolve())
    scratch = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else Path(compiler).parent
    with tempfile.TemporaryDirectory(prefix="serializer-xsd-cli-", dir=scratch) as temporary:
        root = Path(temporary)
        schemas = root / "schemas"
        generated = root / "generated"
        schemas.mkdir()
        generated.mkdir()
        xsd = schemas / "order.xsd"
        xsd.write_text('''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
          targetNamespace="urn:order" elementFormDefault="qualified">
          <xs:element name="order"><xs:complexType>
            <xs:sequence><xs:element name="label" type="xs:string"/></xs:sequence>
            <xs:attribute name="id" type="xs:unsignedInt" use="required"/>
          </xs:complexType></xs:element></xs:schema>''', encoding="utf-8")
        source = generated / "order.serializer"
        depfile = generated / "order.serializer.d"
        command = ("--import-xsd", xsd, "--namespace", "orders", "--root", "order",
                   "--output", source, "--depfile", depfile)
        invoke(compiler, *command)
        first = source.read_bytes()
        assert b"serializer version 1.20.0;" in first
        assert b'using schema "../schemas/order.xsd";' in first
        assert "order.xsd" in depfile.read_text(encoding="utf-8")
        invoke(compiler, *command)
        assert source.read_bytes() == first
        header = generated / "order.hpp"
        invoke(compiler, "--input", source, "--language", "cpp", "--output", header,
               "--cpp.format", "false", "--depfile", generated / "order.hpp.d")
        assert "order.xsd" in (generated / "order.hpp.d").read_text(encoding="utf-8")
        source.write_bytes(first + b"\n// application behavior\n")
        previous = source.read_bytes()
        invoke(compiler, *command, success=False)
        assert source.read_bytes() == previous
        authored = schemas / "manual.serializer"
        authored.write_text('''serializer version 1.20.0;
          namespace orders {
            using schema "order.xsd";
            class order { public string label (7); public uint32 id (3); }
          }''', encoding="utf-8")
        invoke(compiler, "--input", authored, "--language", "python", "--output",
               generated / "manual.py")
        invoke(compiler, "--import-xsd", xsd, "--input", authored,
               "--output", generated / "invalid.serializer", success=False)
        assert not (generated / "invalid.serializer").exists()
        evolution_imports(compiler, root)
        ambiguous_associations(compiler, root)
        restricted_list_imports(compiler, root)
    print("XSD CLI imports, staged stable identities, protected files and exact ambiguity failures passed")


if __name__ == "__main__":
    main()
