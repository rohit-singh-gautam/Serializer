# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated positioned Python extension slots without a retained tree for known values."""
import argparse
import copy
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import xml.etree.ElementTree as etree

ROOT = Path(__file__).resolve().parent.parent
NS = 'urn:serializer:extension-tokens'
DOCUMENT = ('''<root xmlns="'''+NS+'''" xmlns:v="urn:vendor" xmlns:e="urn:end" xmlns:s="urn:skip"
 xmlns:m="urn:metadata" xmlns:p="urn:parent" id="7" m:label="p:Thing">
 <v:before xmlns:p="urn:inner" xmlns:n0="urn:shadow" xmlns:n1="urn:another" m:again="n1:Thing">head<p:nest name="p:Thing">inside</p:nest>tail<plain xmlns=""/>last</v:before>
 <item>1</item><v:between/><item>2</item><s:discard><s:child>discarded</s:child></s:discard>
 <tail>Z</tail><e:after>kept</e:after></root>''').encode()


def load_module(path, name):
    """Load a real compiler output through the target Python SDK."""
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


class recording_context:
    """Record complete-operation logical charges while admitting all resource requests."""
    def __init__(self):
        """Initialize one independent operation inventory."""
        self.counts = {}
    def consume(self, resource, count):
        """Accumulate the codec's logical resource requests."""
        self.counts[resource] = self.counts.get(resource, 0) + count
    def check(self):
        """Allow provider-boundary interruption checks."""
    def check_buffer(self, amount):
        """Allow private output buffers while preserving this interface's accounting boundary."""


class python_xml_extension_test(unittest.TestCase):
    """Exercise exact shared slot permissions, namespace context, capabilities and owning state."""
    def policy(self, **limits):
        """Supply explicit semantic understanding independently of permitted opaque payloads."""
        return self.module.Limits(xml_extensions=self.module.XmlExtensionOptions(
            understood_namespaces=('urn:vendor', 'urn:parent'),
            limits=self.module.XmlExtensionLimits(**limits)))

    def test_foreign_only_capture_preserves_scope_text_and_native_wire(self):
        """Keep known members direct and replay only opaque foreign subtrees at their original positions."""
        module = self.module
        with mock.patch.object(module, '_xml_parse', side_effect=AssertionError('retained document tree')):
            with mock.patch.object(module._XmlBudget, 'node', side_effect=AssertionError('known XML node allocation')):
                model = module.ExtensionRoot.decode_xml_chunks((DOCUMENT[i:i+1] for i in range(len(DOCUMENT))), limits=self.policy())
        self.assertEqual((model.id, model.items, model.tail), (7, [1, 2], 'Z'))
        native = [model.encode(protocol) for protocol in module.Protocol]
        with mock.patch.object(module, '_xml_write', side_effect=AssertionError('retained output tree')):
            output = model.encode_xml(self.policy())
        root = self.provider.fromstring(output)
        self.assertTrue(self.oracle.validate(root), str(self.oracle.error_log))
        self.assertEqual(root.attrib['{urn:metadata}label'], 'p:Thing')
        self.assertEqual(root.nsmap['p'], 'urn:parent')
        self.assertEqual([child.tag for child in root],
                         ['{urn:vendor}before', '{'+NS+'}item', '{urn:vendor}between',
                          '{'+NS+'}item', '{'+NS+'}tail', '{urn:end}after'])
        foreign = root[0]
        self.assertEqual(foreign.attrib['{urn:metadata}again'], 'n1:Thing')
        self.assertEqual(foreign.nsmap['n1'], 'urn:another')
        self.assertEqual((foreign.text, foreign[0].tail, foreign[1].tail), ('head', 'tail', 'last'))
        self.assertEqual(foreign[0].tag, '{urn:inner}nest')
        self.assertEqual(foreign[0].nsmap['p'], 'urn:inner')
        self.assertEqual(foreign[0].attrib['name'], 'p:Thing')
        self.assertEqual(foreign[1].tag, 'plain')
        self.assertEqual([model.encode(protocol) for protocol in module.Protocol], native)
        self.assertEqual(module.ExtensionRoot.decode_xml(output, self.policy()).items, [1, 2])
        model.items.append(3)
        self.assertEqual(module.ExtensionRoot.decode_xml(model.encode_xml(self.policy()), self.policy()).items, [1, 2, 3])
        model.items = []
        with self.assertRaisesRegex(ValueError, 'edited field position'):
            model.encode_xml(self.policy())

    def test_reject_skip_positions_counts_and_capabilities(self):
        """Opaque preservation never establishes understanding; every unknown core/foreign name needs its slot."""
        module = self.module
        with mock.patch.object(module.ExtensionRoot, '__init__', side_effect=AssertionError('constructed')):
            with self.assertRaisesRegex(ValueError, 'not understood'):
                module.ExtensionRoot.decode_xml(DOCUMENT)
            for options in (module.XmlExtensionOptions(understood_namespaces=('urn:vendor',), required_prefixes='p'),
                            module.XmlExtensionOptions(understood_namespaces=('urn:vendor',), required_prefixes='missing')):
                with self.assertRaises(ValueError):
                    module.ExtensionRoot.decode_xml(DOCUMENT, module.Limits(xml_extensions=options))
        policy = module.Limits(xml_extensions=module.XmlExtensionOptions(
            understood_namespaces=('urn:vendor', 'urn:parent'), required_prefixes='p'))
        value = module.ExtensionRoot.decode_xml(DOCUMENT, policy)
        module.ExtensionRoot.decode_xml(value.encode_xml(policy), policy)
        invalid = (
            DOCUMENT.replace(b'<v:between/>', b'<intruder/>'),
            DOCUMENT.replace(b'<v:between/>', b'<q:extra xmlns:q="urn:unlisted"/>'),
            DOCUMENT.replace(b'<v:between/>', b'<v:one/><v:two/>'),
            DOCUMENT.replace(b'<v:between/>', b'').replace(DOCUMENT[DOCUMENT.index(b'<v:before '):DOCUMENT.index(b'</v:before>')+len(b'</v:before>')], b''),
            DOCUMENT.replace(b'</root>', b'<r:denied xmlns:r="urn:reject"/></root>'),
            DOCUMENT.replace(b'<tail>Z</tail>', b'<tail>Z</tail><v:late/>'),
            DOCUMENT.replace(b'id="7"', b'id="7" unknown="x"'),
            DOCUMENT.replace(b'<e:after>kept</e:after>', b'<x:extra xmlns:x="urn:serializer:xml:model"/>'),
        )
        for data in invalid:
            with self.subTest(data=data):
                with self.assertRaises(ValueError):
                    module.ExtensionRoot.decode_xml(data, self.policy())
        skip = ('<only_skip xmlns="'+NS+'" xmlns:s="urn:skip"><value>1</value><s:discard>'+
                'discarded' * 4096 + '</s:discard></only_skip>').encode()
        value = module.ExtensionOnlySkip.decode_xml_chunks((skip,),
            module.Limits(xml_extensions=module.XmlExtensionOptions(limits=module.XmlExtensionLimits(max_preserved_bytes=0))))
        self.assertIsNone(value._srl_xml_state)
        self.assertEqual([child.tag for child in etree.fromstring(value.encode_xml())], ['{'+NS+'}value'])

    def test_exact_bounds_chunk_independence_and_context_failures(self):
        """Admit exact logical slot/item/byte bounds and preserve all application callback failures."""
        module = self.module
        first, fragmented = recording_context(), recording_context()
        value = module.ExtensionRoot.decode_xml(DOCUMENT, self.policy(), context=first)
        module.ExtensionRoot.decode_xml_chunks((DOCUMENT[i:i+1] for i in range(len(DOCUMENT))),
            self.policy(), context=fragmented)
        self.assertEqual(first.counts['preserved_bytes'], fragmented.counts['preserved_bytes'])
        size = first.counts['preserved_bytes']
        exact = self.policy(max_slots=5, max_items=5, max_preserved_bytes=size)
        module.ExtensionRoot.decode_xml(DOCUMENT, exact)
        for unused in range(3):
            value = module.ExtensionRoot.decode_xml(value.encode_xml(exact), exact)
        for limits in ({'max_slots': 4}, {'max_items': 4}, {'max_preserved_bytes': size - 1}, {'max_preserved_bytes': 0}):
            with self.subTest(limits=limits):
                with self.assertRaises(ValueError):
                    module.ExtensionRoot.decode_xml(DOCUMENT, self.policy(**limits))
        marker = ValueError('application preserved-byte callback marker')
        class rejecting_context:
            """Interrupt one foreign-payload allocation before an owning candidate can escape."""
            def consume(self, resource, count):
                """Raise the exact original application exception on preservation admission."""
                if resource == 'preserved_bytes':
                    raise marker
        with self.assertRaises(ValueError) as failure:
            module.ExtensionRoot.decode_xml(DOCUMENT, self.policy(), context=rejecting_context())
        self.assertIs(failure.exception, marker)
        with self.assertRaises(ValueError):
            module.XmlExtensionLimits(max_items=-1)
        with self.assertRaises(ValueError):
            module.XmlExtensionOptions(required_prefixes='a' * (module._XML_EXTENSION_METADATA_MAX_BYTES + 1))
        with self.assertRaises(ValueError):
            module.ExtensionRoot.decode_xml(DOCUMENT, module.Limits(xml_extensions=module.XmlExtensionOptions(
                understood_namespaces=('urn:vendor', 'urn:parent'), required_prefixes='p ' * 6,
                limits=module.XmlExtensionLimits(max_items=5))))

    def test_edited_opaque_state_is_revalidated(self):
        """Reject namespace injection, malformed names, mixed-text corruption and invalid ordering on output."""
        module = self.module
        model = module.ExtensionRoot.decode_xml(DOCUMENT, self.policy())
        original = copy.deepcopy(model._srl_xml_state)
        model._srl_xml_state[0]['bad prefix'] = 'urn:bad'
        with self.assertRaises(ValueError):
            model.encode_xml(self.policy())
        model._srl_xml_state = copy.deepcopy(original)
        model._srl_xml_state[2][0][2][0] = '{urn:vendor}bad name'
        with self.assertRaises(ValueError):
            model.encode_xml(self.policy())
        model._srl_xml_state = copy.deepcopy(original)
        model._srl_xml_state[2][0][2][5] = [(0, 'wrong')]
        with self.assertRaises(ValueError):
            model.encode_xml(self.policy())
        model._srl_xml_state = copy.deepcopy(original)
        model._srl_xml_state[2].reverse()
        with self.assertRaises(ValueError):
            model.encode_xml(self.policy())
        model._srl_xml_state = copy.deepcopy(original)
        model._srl_xml_state[1].clear()
        model._srl_xml_state[1]['{urn:serializer:xml:model}unknown'] = 'x'
        with self.assertRaises(ValueError):
            model.encode_xml(self.policy())
        model._srl_xml_state = copy.deepcopy(original)
        model._srl_xml_state[2][-1][2][0] = '{urn:serializer:xml:model}unknown'
        with self.assertRaises(ValueError):
            model.encode_xml(self.policy())


def main():
    """Generate a complete typed fixture and require an independent provider for its allowed XML vocabulary."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--python-dependencies', type=Path)
    args = parser.parse_args()
    if args.python_dependencies:
        sys.path.insert(0, str(args.python_dependencies.resolve()))
    from lxml import etree as provider
    build = args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository scratch must remain under ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='python-xml-extensions-', dir=build) as temporary:
        directory = Path(temporary)
        source, config, output = directory / 'extensions.serializer', directory / 'extensions.ini', directory / 'extensions.py'
        source.write_text('''serializer version 1.20.0; namespace extension {
          class root { public uint32 id(1) xml(attribute); public array<uint32> items(2); public string tail(3); }
          class only_skip { public uint32 value(1); }}''', encoding='utf-8')
        config.write_text('''[xml]
enabled=true
[xml.class.extension::root]
namespace='''+NS+'''
name.items=item
extension.attributes.kind=attribute
extension.attributes.namespaces=urn:metadata urn:serializer:xml:model
extension.attributes.policy=preserve
extension.attributes.maximum=1
extension.before.kind=element
extension.before.before=items
extension.before.namespaces=urn:vendor
extension.before.policy=preserve
extension.before.minimum=1
extension.before.maximum=2
extension.end.kind=element
extension.end.namespaces=urn:end urn:serializer:xml:model
extension.end.policy=preserve
extension.end.maximum=1
extension.skip.kind=element
extension.skip.before=tail
extension.skip.namespaces=urn:skip
extension.skip.policy=skip
extension.reject.kind=element
extension.reject.namespaces=urn:reject
extension.reject.policy=reject
extensions.required=urn:vendor
[xml.class.extension::only_skip]
namespace='''+NS+'''
extension.skip.kind=element
extension.skip.namespaces=urn:skip
extension.skip.policy=skip
''', encoding='utf-8')
        subprocess.run([str(args.compiler.resolve()), '--language', 'python', '--input', str(source),
                        '--config', str(config), '--python.output', str(output)], check=True)
        python_xml_extension_test.module = load_module(output, 'serializer_python_xml_extension_fixture')
        python_xml_extension_test.provider = provider
        python_xml_extension_test.oracle = provider.XMLSchema(provider.fromstring(('''
        <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="'''+NS+'''" elementFormDefault="qualified">
        <xs:element name="root"><xs:complexType><xs:sequence>
        <xs:choice minOccurs="0" maxOccurs="unbounded"><xs:any namespace="urn:vendor" processContents="skip"/>
        <xs:element name="item" type="xs:unsignedInt"/></xs:choice>
        <xs:element name="tail" type="xs:string"/><xs:any namespace="urn:end" processContents="skip" minOccurs="0" maxOccurs="1"/>
        </xs:sequence><xs:attribute name="id" type="xs:unsignedInt" use="required"/>
        <xs:anyAttribute namespace="urn:metadata" processContents="skip"/></xs:complexType></xs:element></xs:schema>''').encode()))
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(python_xml_extension_test))
        if not result.wasSuccessful():
            raise SystemExit(1)


if __name__ == '__main__':
    main()
