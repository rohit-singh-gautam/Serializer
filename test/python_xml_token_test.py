# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated Python direct XML tokens, incremental input, facets and provider behavior."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import importlib.util
from pathlib import Path
import subprocess
import struct
import sys
import tempfile
import unittest
from unittest import mock
import xml.etree.ElementTree as etree

ROOT = Path(__file__).resolve().parent.parent
NS = 'urn:serializer:tokens'
DOCUMENT = f'''<root xmlns="{NS}" id="18446744073709551615"><label>A&amp;B&#13;😀</label>
  <flag>1</flag><single unit="x&#9;y">-2147483648</single><children>
  <child unit="USD">7</child><child unit="EUR">9</child></children>
  <tags><tag>first</tag><tag></tag></tags></root>'''.encode()


def load_module(path, name):
    """Load the real compiler output through the target SDK's ordinary import path."""
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


class python_xml_token_test(unittest.TestCase):
    """Exercise complete generated owning codecs while making tree entry points unavailable."""
    def test_direct_record_and_scalar_output(self):
        """Use independent expanded-name checks and fail if the legacy tree path is reached."""
        module = self.module
        with mock.patch.object(module, '_xml_parse', side_effect=AssertionError('retained tree')):
            model = module.TokensRoot.decode_xml(DOCUMENT)
        with mock.patch.object(module, '_xml_write', side_effect=AssertionError('retained tree')):
            with mock.patch.object(module._XmlBudget, 'node', side_effect=AssertionError('node allocation')):
                output = model.encode_xml()
        independent = etree.fromstring(output)
        self.assertEqual(independent.tag, '{' + NS + '}root')
        self.assertEqual(independent.attrib['id'], '18446744073709551615')
        self.assertEqual(independent.find('{' + NS + '}label').text, 'A&B\r😀')
        self.assertEqual(independent.find('{' + NS + '}single').attrib['unit'], 'x\ty')
        self.assertEqual([child.value for child in model.children], [7, 9])
        self.assertEqual(model.tags, ['first', ''])
        self.assertEqual(module.TokensRoot.decode_xml(output).encode_xml(), output)

    def test_arbitrary_byte_boundaries_and_exact_eof(self):
        """Split UTF-8, entity, attribute and scalar tokens; refuse a valid root with a bad suffix."""
        model = self.module.TokensRoot
        baseline = model.decode_xml(DOCUMENT).encode(self.module.Protocol.BINARY_NONE)
        for size in (1, 2, 3, 7, 31, len(DOCUMENT)):
            with self.subTest(size=size):
                value = model.decode_xml_chunks(DOCUMENT[index:index + size] for index in range(0, len(DOCUMENT), size))
                self.assertEqual(value.encode(self.module.Protocol.BINARY_NONE), baseline)
        for suffix in (b'<second/>', b'x', b'<!--', b'\x00'):
            with self.subTest(suffix=suffix):
                with self.assertRaises((ValueError, self.module._xml_expat.ExpatError)):
                    model.decode_xml_chunks((DOCUMENT, suffix))

    def test_transactional_shape_and_resource_failures(self):
        """Reject malformed values/order/unknowns and cumulative resource failures before returning."""
        module = self.module
        bad = (
            DOCUMENT.replace(b'18446744073709551615', b'18446744073709551616'),
            DOCUMENT.replace(b'-2147483648', b'-2147483649'),
            DOCUMENT.replace(b'<flag>1</flag>', b'<flag>truex</flag>'),
            DOCUMENT.replace(b'<tags>', b'<unknown/><tags>'),
            DOCUMENT.replace(b'<tag></tag>', b''),
            DOCUMENT.replace(b'</label>', b'<nested/></label>'),
            DOCUMENT.replace(b'<root ', b'<root other="1" '),
            DOCUMENT.replace(b'<tags>', b'<tags forbidden="1">'),
            b'<!DOCTYPE root [<!ENTITY injected "x">]>' + DOCUMENT,
            DOCUMENT.replace(b'urn:serializer:tokens', b'urn:wrong'),
        )
        for data in bad:
            with self.subTest(data=data):
                with self.assertRaises((ValueError, module._xml_expat.ExpatError)):
                    module.TokensRoot.decode_xml_chunks((data,))
        for limits in (module.Limits(max_bytes=len(DOCUMENT) - 1), module.Limits(max_depth=2),
                       module.Limits(max_elements=4), module.Limits(max_string_bytes=8)):
            with self.subTest(limits=limits):
                with self.assertRaises(ValueError):
                    module.TokensRoot.decode_xml_chunks((DOCUMENT,), limits)
        model = module.TokensRoot.decode_xml(DOCUMENT)
        size = len(model.encode_xml())
        self.assertEqual(len(model.encode_xml(module.Limits(max_bytes=size))), size)
        with self.assertRaises(ValueError):
            model.encode_xml(module.Limits(max_bytes=size - 1))
        width = struct.calcsize('I')
        padded = DOCUMENT + b' ' * ((-len(DOCUMENT)) % width)
        view = memoryview(padded).cast('I')
        module.TokensRoot.decode_xml_chunks((view,))
        with self.assertRaises(ValueError):
            module.TokensRoot.decode_xml_chunks((view,), module.Limits(max_bytes=len(padded) - 1))
        with self.assertRaises(ValueError):
            module.TokensRoot.decode_xml_chunks((memoryview(padded)[::2],))

    def test_optional_absence_empty_wrapper_and_native_independence(self):
        """Keep optional absence distinct from false and preserve required empty collection wrappers."""
        document = DOCUMENT.replace(b'<flag>1</flag>', b'')
        start = document.index(b'<children>')
        end = document.index(b'</children>') + len(b'</children>')
        document = document[:start] + b'<children/>' + document[end:]
        value = self.module.TokensRoot.decode_xml_chunks((document,))
        self.assertEqual(value.flag, ())
        self.assertEqual(value.children, [])
        for protocol in self.module.Protocol:
            native = value.encode(protocol)
            self.assertEqual(self.module.TokensRoot.decode(native, protocol).encode(protocol), native)
        with self.assertRaises(ValueError):
            self.module.TokensRoot.decode_xml(document.replace(b'<children/>', b''))

    def test_all_permutations_and_singleton_choice(self):
        """Accept every all permutation; reject duplicates and zero/two selected choice branches."""
        module = self.all_module
        for content in (b'<left>7</left><right>x</right>', b'<right>x</right><left>7</left>'):
            value = module.PermutationsRoot.decode_xml_chunks((b'<root>', content, b'</root>'))
            self.assertEqual((value.left, value.right), (7, 'x'))
        for content in (b'<left>7</left>', b'<left>7</left><right>x</right><left>8</left>'):
            with self.assertRaises(ValueError):
                module.PermutationsRoot.decode_xml(b'<root>' + content + b'</root>')
        for content in (b'<left>7</left>', b'<right>x</right>'):
            self.choice_module.AlternativesRoot.decode_xml_chunks((b'<root>', content, b'</root>'))
        for content in (b'', b'<left>7</left><right>x</right>'):
            with self.assertRaises(ValueError):
                self.choice_module.AlternativesRoot.decode_xml(b'<root>' + content + b'</root>')

    def test_explicit_nil_absent_empty_and_collection_states(self):
        """Round-trip all nested optional states, including nil versus an empty owning wrapper."""
        module = self.presence_module
        prefix = b'<root xmlns:x="http://www.w3.org/2001/XMLSchema-instance">'
        for label, expected in ((b'', ()), (b'<label x:nil="true"/>', ((),)),
                                (b'<label/>', (('',),)), (b'<label>text</label>', (('text',),))):
            for items, collection in ((b'', ()), (b'<items x:nil="1"/>', ((),)),
                                      (b'<items/>', (([],),)), (b'<items><item>x</item></items>', ((['x'],),))):
                with self.subTest(label=label, items=items):
                    data = prefix + label + b'<mandatory x:nil="true"/>' + items + b'</root>'
                    value = module.PresenceRoot.decode_xml_chunks(data[index:index+1] for index in range(len(data)))
                    self.assertEqual(value.label, expected)
                    self.assertEqual(value.mandatory, ())
                    self.assertEqual(value.items, collection)
                    copy = module.PresenceRoot.decode_xml(value.encode_xml())
                    self.assertEqual((copy.label, copy.mandatory, copy.items), (expected, (), collection))
        value = module.PresenceRoot.decode_xml(prefix + b'<mandatory x:nil="0"/></root>')
        self.assertEqual(value.mandatory, ('',))
        for body in (b'', b'<mandatory x:nil="true"> </mandatory>', b'<mandatory x:nil="other"/>',
                     b'<mandatory x:nil="true"><nested/></mandatory>',
                     b'<mandatory/><items><item x:nil="true"/></items>'):
            with self.subTest(body=body):
                with self.assertRaises(ValueError):
                    module.PresenceRoot.decode_xml(prefix + body + b'</root>')
        record = module.PresenceRecordRoot.decode_xml(b'<record_root xmlns:x="http://www.w3.org/2001/XMLSchema-instance"><nested x:nil="1"/></record_root>')
        self.assertEqual(record.nested, ())
        self.assertEqual(module.PresenceRecordRoot.decode_xml(record.encode_xml()).nested, ())
        record = module.PresenceRecordRoot.decode_xml(b'<record_root><nested>text</nested></record_root>')
        self.assertEqual(record.nested[0].value, 'text')

    def test_map_native_key_identity_optional_and_record_payloads(self):
        """Validate explicit map placement, aliases, native duplicate detection and complete entries."""
        module = self.containers_module
        data = b'''<maps><numbers><entry id="+007"><data>seven</data></entry></numbers>
          <flags><entry><data>yes</data><key>1</key></entry></flags>
          <values><entry key="k"><value><amount>12</amount></value></entry></values>
          <states><entry><key>first</key><value>one</value></entry></states></maps>'''
        value = module.ContainersMaps.decode_xml_chunks(data[index:index+1] for index in range(len(data)))
        self.assertEqual(value.numbers, {7: 'seven'})
        self.assertEqual(value.flags, {True: 'yes'})
        self.assertEqual(value.optional_values[0]['k'].amount, 12)
        self.assertEqual(value.states, {module.ContainersState.First: 'one'})
        copy = module.ContainersMaps.decode_xml(value.encode_xml())
        self.assertEqual(copy.numbers, value.numbers)
        self.assertEqual(copy.optional_values[0]['k'].amount, 12)
        absent = module.ContainersMaps.decode_xml(b'<maps><numbers/><flags/><states/></maps>')
        empty = module.ContainersMaps.decode_xml(b'<maps><numbers/><flags/><values/><states/></maps>')
        self.assertEqual(absent.optional_values, ())
        self.assertEqual(empty.optional_values, ({},))
        for replacement in (b'<entry id="7"><data>x</data></entry><entry id="+007"><data>y</data></entry>',
                            b'<entry id="4294967296"><data>x</data></entry>',
                            b'<entry id="7"><data>x</data><data>y</data></entry>',
                            b'<entry id="7" extra="x"><data>x</data></entry>',
                            b'<entry><data>x</data></entry>',
                            b'<entry id="7"/>'):
            bad = b'<maps><numbers>' + replacement + b'</numbers><flags/><states/></maps>'
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                module.ContainersMaps.decode_xml(bad)
        boolean_duplicate = b'<maps><numbers/><flags><entry><key>1</key><data>a</data></entry><entry><key>true</key><data>b</data></entry></flags><states/></maps>'
        with self.assertRaises(ValueError):
            module.ContainersMaps.decode_xml(boolean_duplicate)
        bounded = b'<maps><numbers><entry id="+007"><data>x</data></entry></numbers><flags/><states/></maps>'
        module.ContainersMaps.decode_xml(bounded, module.Limits(xml_map=module.XmlMapLimits(max_entries=1, max_key_bytes=4)))
        for limits in (module.XmlMapLimits(max_entries=0), module.XmlMapLimits(max_key_bytes=3)):
            with self.assertRaises(ValueError):
                module.ContainersMaps.decode_xml_chunks((bounded,), module.Limits(xml_map=limits))
        with self.assertRaises(ValueError):
            value.encode_xml(module.Limits(xml_map=module.XmlMapLimits(max_entries=0)))

    def test_variant_tag_and_qname_selection(self):
        """Select exact native alternatives only after wrapper, tag and optional xsi:type agreement."""
        module = self.containers_module
        ordinary = module.ContainersElementChoice.decode_xml(b'<element_choice><choice><number>7</number></choice></element_choice>')
        self.assertEqual((ordinary.payload_index, ordinary.payload_code), (0, 7))
        typed = b'<typed_choice xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns:t="http://www.w3.org/2001/XMLSchema"><choice><choice i:type="t:unsignedInt">7</choice></choice></typed_choice>'
        value = module.ContainersTypedChoice.decode_xml_chunks(typed[index:index+1] for index in range(len(typed)))
        self.assertEqual((value.payload_index, value.payload_code), (0, 7))
        self.assertEqual(module.ContainersTypedChoice.decode_xml(value.encode_xml()).payload_code, 7)
        value.payload_index = 2
        value.payload_record.amount = -12
        copy = module.ContainersTypedChoice.decode_xml(value.encode_xml())
        self.assertEqual((copy.payload_index, copy.payload_record.amount), (2, -12))
        for payload in (b'', b'<number>7</number><text>x</text>', b'<missing>7</missing>',
                        b'<number xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns:t="http://www.w3.org/2001/XMLSchema" i:type="t:string">7</number>',
                        b'<number xmlns:i="http://www.w3.org/2001/XMLSchema-instance" i:nil="false">7</number>'):
            with self.subTest(payload=payload), self.assertRaises(ValueError):
                module.ContainersElementChoice.decode_xml(b'<element_choice><choice>' + payload + b'</choice></element_choice>')
        for changed in (typed.replace(b'i:type="t:unsignedInt"', b''),
                        typed.replace(b't:unsignedInt', b'missing:unsignedInt'),
                        typed.replace(b'http://www.w3.org/2001/XMLSchema"', b'urn:wrong"'),
                        typed.replace(b'<choice i:type', b'<number i:type').replace(b'7</choice>', b'7</number>')):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                module.ContainersTypedChoice.decode_xml(changed)
        ordinary.payload_index = 99
        with self.assertRaises(ValueError):
            ordinary.encode_xml()

    def test_inherited_fields_use_existing_composed_base_storage(self):
        """Flatten XML in depth-first base order while keeping native model ownership and layout."""
        module = self.containers_module
        data = b'<derived xmlns="urn:derived" label="base"><ancestor>-12</ancestor><flag>true</flag><count>7</count></derived>'
        value = module.ContainersDerived.decode_xml_chunks(data[index:index+1] for index in range(len(data)))
        self.assertEqual(value.base0.base0.ancestor, -12)
        self.assertEqual(value.base0.label, 'base')
        self.assertTrue(value.base1.flag)
        self.assertEqual(value.count, 7)
        with mock.patch.object(module._XmlBudget, 'node', side_effect=AssertionError('inherited tree')):
            copy = module.ContainersDerived.decode_xml(value.encode_xml())
        self.assertEqual(copy.base0.base0.ancestor, -12)
        self.assertEqual([child.tag for child in etree.fromstring(value.encode_xml())],
                         ['{urn:derived}ancestor', '{urn:derived}flag', '{urn:derived}count'])
        for protocol in module.Protocol:
            native = value.encode(protocol)
            self.assertEqual(module.ContainersDerived.decode(native, protocol).encode(protocol), native)
        for changed in (data.replace(b'<ancestor>-12</ancestor><flag>true</flag>', b'<flag>true</flag><ancestor>-12</ancestor>'),
                        data.replace(b'</ancestor>', b'</ancestor><ancestor>2</ancestor>')):
            with self.assertRaises(ValueError):
                module.ContainersDerived.decode_xml(changed)

    def test_mixed_order_chunk_independence_and_admission_bounds(self):
        """Keep ordered text/record values, join only parser-split text runs, and visit authored fragments."""
        module = self.containers_module
        data = '<paragraph xmlns="urn:paragraph" id="p">head&amp;😀<f:em xmlns:f="urn:format">middle</f:em>tail<em xmlns="urn:format">last</em>end</paragraph>'.encode()
        value = module.ContainersParagraph.decode_xml(data)
        self.assertEqual([fragment.value_index for fragment in value.content], [0, 1, 0, 1, 0])
        self.assertEqual(value.content[0].value_text, 'head&😀')
        self.assertEqual(value.content[1].value_emphasis.text, 'middle')
        native = value.encode(module.Protocol.BINARY_NONE)
        for size in (1, 2, 7, len(data)):
            copy = module.ContainersParagraph.decode_xml_chunks(data[index:index+size] for index in range(0, len(data), size))
            self.assertEqual(copy.encode(module.Protocol.BINARY_NONE), native)
        module.ContainersParagraph.decode_xml(data, module.Limits(xml_mixed=module.XmlMixedLimits(max_fragments=5, max_text_bytes=16)))
        for bounds in (module.XmlMixedLimits(max_fragments=4), module.XmlMixedLimits(max_text_bytes=15)):
            with self.assertRaises(ValueError):
                module.ContainersParagraph.decode_xml_chunks((data,), module.Limits(xml_mixed=bounds))
            with self.assertRaises(ValueError):
                value.encode_xml(module.Limits(xml_mixed=bounds))
        with self.assertRaises(ValueError):
            module.ContainersParagraph.decode_xml(data.replace(b'f:em', b'f:unknown'))
        empty, adjacent = module.ContainersFragment(), module.ContainersFragment()
        empty.value_text, adjacent.value_text = '', 'adjacent'
        value.content[1:1] = [empty, adjacent]
        seen, original = [], module._XmlTokenOutput.text
        def observe(writer, text):
            """Observe the direct output trace without changing writer validation or output."""
            seen.append(text)
            return original(writer, text)
        with mock.patch.object(module._XmlTokenOutput, 'text', new=observe):
            with mock.patch.object(module._XmlBudget, 'node', side_effect=AssertionError('mixed tree')):
                output = value.encode_xml()
        self.assertEqual(seen[:3], ['head&😀', '', 'adjacent'])
        self.assertEqual(etree.fromstring(output).text, 'head&😀adjacent')
        with self.assertRaisesRegex(ValueError, 'standalone XML'):
            empty.encode_xml()
        class accounting:
            """Compare the cumulative logical resource contract across arbitrary provider splits."""
            def __init__(self):
                """Start one independent complete-message accounting context."""
                self.totals = {}
            def consume(self, resource, count):
                """Retain only aggregate work/allocation counts, independently of generated models."""
                self.totals[resource] = self.totals.get(resource, 0) + count
        text = ('é😀' * 4096).encode()
        long_document = b'<paragraph xmlns="urn:paragraph" id="p">' + text + b'</paragraph>'
        limits = module.Limits(xml_mixed=module.XmlMixedLimits(max_fragments=1, max_text_bytes=len(text)))
        whole, split = accounting(), accounting()
        one = module.ContainersParagraph.decode_xml_chunks((long_document,), limits, context=whole)
        many = module.ContainersParagraph.decode_xml_chunks((long_document[index:index+1] for index in range(len(long_document))), limits, context=split)
        self.assertEqual(one.content[0].value_text, many.content[0].value_text)
        self.assertEqual(whole.totals, split.totals)
        for label, field, mapping in (
                ('map', 'map<string,fragment>', 'map.payload = entry'),
                ('variant', 'variant(string = text, fragment = helper)', 'variant.payload = element')):
            source = self.directory / ('invalid_helper_' + label + '.serializer')
            source.write_text('''serializer version 1.20.0; namespace helper_probe {
              class emphasis { public string text(1) xml(text); }
              class fragment { public variant(string = text, emphasis = emphasis) value(1); }
              class paragraph { public array<fragment> content(1); }
              class holder { public ''' + field + ''' payload(1); }
            }''', encoding='utf-8')
            config = self.directory / ('invalid_helper_' + label + '.ini')
            config.write_text('''[xml]
enabled = true
[xml.class.helper_probe::paragraph]
mixed = content
[xml.class.helper_probe::holder]
wrapper.payload = payload
''' + mapping + '\n', encoding='utf-8')
            destination = self.directory / ('invalid_helper_' + label + '.py')
            destination.write_text('preserved\n', encoding='utf-8')
            completed = subprocess.run([str(self.compiler), '--input', str(source), '--config', str(config),
                                        '--language', 'python', '--python.output', str(destination)], capture_output=True, text=True)
            self.assertNotEqual(completed.returncode, 0)
            self.assertIn('mixed fragment storage', completed.stderr)
            self.assertEqual(destination.read_text(encoding='utf-8'), 'preserved\n')

    def test_prepared_provider_root_limits_concurrency_and_no_fetch(self):
        """Reuse one real XSD provider across concurrent calls and refuse undeclared/malformed roots."""
        module = self.module
        schema_text = b'''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
          <xs:element name="amount" type="xs:int"/><xs:element name="other" type="xs:string"/>
        </xs:schema>'''
        provider = module.XmlSchema(schema_text)
        with ThreadPoolExecutor(max_workers=4) as workers:
            self.assertEqual(list(workers.map(lambda unused: provider.validate(b'<amount>7</amount>'), range(24))), [None] * 24)
        for data in (b'<missing/>', b'<amount>2147483648</amount>', b'<amount>1</amount>x',
                     b'<!DOCTYPE amount SYSTEM "http://127.0.0.1:1/never"><amount>1</amount>'):
            with self.subTest(data=data):
                with self.assertRaises((ValueError, module._xml_expat.ExpatError)):
                    provider.validate(data)
        with self.assertRaises(ValueError):
            provider.validate(b'<amount>7</amount>', module.Limits(max_bytes=4))
        with self.assertRaises(ValueError):
            module.XmlSchema(schema_text.replace(b'<xs:element', b'<xs:include schemaLocation="http://127.0.0.1:1/never"/><xs:element', 1))
        with mock.patch.dict(sys.modules, {'lxml': None}):
            with self.assertRaisesRegex(RuntimeError, 'requires lxml'):
                module.XmlSchema(schema_text)
        with mock.patch.object(module.TokensRoot, '__init__', side_effect=AssertionError('candidate constructed before root validation')):
            with self.assertRaises(ValueError):
                module.TokensRoot.decode_xml(b'<amount>7</amount>', schema=provider)

    def test_html_is_independent_of_xml_and_matches_html_parser(self):
        """Handle ordinary HTML void/unquoted/Boolean syntax with the approved shared subset."""
        from html.parser import HTMLParser
        module = self.module
        source = b'<DIV hidden class=sample><p>A&amp;B<br>tail&nbsp;&#128512;</p><img src="image.png" alt=sample></DIV>'
        with mock.patch.object(module, '_xml_parse', side_effect=AssertionError('HTML used XML')):
            events = module.Html.parse_fragment(source)
            output = module.Html.write_fragment(events)
        class independent_parser(HTMLParser):
            """Collect a standard-library HTML trace independently of the generated tokenizer."""
            def __init__(self):
                """Enable ordinary HTML entity expansion for semantic comparisons."""
                super().__init__(convert_charrefs=True)
                self.trace = []
            def handle_starttag(self, tag, attrs):
                """Retain standard HTML start/void tags and attributes."""
                self.trace.append(('start', tag, attrs))
            def handle_endtag(self, tag):
                """Retain standard HTML explicit closing tags."""
                self.trace.append(('end', tag))
            def handle_data(self, data):
                """Retain HTML semantic text after independent entity expansion."""
                self.trace.append(('text', data))
        original, canonical = independent_parser(), independent_parser()
        original.feed(source.decode())
        canonical.feed(output.decode())
        self.assertEqual(original.trace, canonical.trace)
        self.assertIn(b'<br>', output)
        self.assertIn(b' hidden', output)
        for invalid in (b'<p><div>x</div></p>', b'<script>x</script>', b'<p>x', b'<p/>',
                        b'<li>x</li>', b'<p>&copy;</p>', b'<!DOCTYPE html><p>x</p>',
                        b'<a href=x href=y></a>', b'<p hidden=false></p>', b'<p>&#128;</p>'):
            with self.subTest(invalid=invalid):
                with self.assertRaises(ValueError):
                    module.Html.parse_fragment(invalid)
        for limits in (module.HtmlLimits(max_input_bytes=2), module.HtmlLimits(max_output_bytes=2),
                       module.HtmlLimits(max_events=2), module.HtmlLimits(max_depth=1),
                       module.HtmlLimits(max_attributes_per_element=0), module.HtmlLimits(max_text_bytes=1)):
            with self.assertRaises(ValueError):
                module.Html.parse_fragment(source, limits)


def main():
    """Generate isolated fixtures, then run actual Python SDK checks without retaining scratch."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    build = args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository scratch must remain under ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='python-xml-tokens-', dir=build) as temporary:
        directory = Path(temporary)
        python_xml_token_test.directory, python_xml_token_test.compiler = directory, args.compiler.resolve()
        (directory / 'tokens.xsd').write_text('''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
          xmlns:t="urn:serializer:tokens" targetNamespace="urn:serializer:tokens" elementFormDefault="qualified">
          <xs:complexType name="child"><xs:simpleContent><xs:extension base="xs:int">
            <xs:attribute name="unit" type="xs:string" use="required"/>
          </xs:extension></xs:simpleContent></xs:complexType>
          <xs:element name="root"><xs:complexType><xs:sequence>
            <xs:element name="label" type="xs:string"/><xs:element name="flag" type="xs:boolean" minOccurs="0"/>
            <xs:element name="single" type="t:child"/>
            <xs:element name="children"><xs:complexType><xs:sequence><xs:element name="child" type="t:child" minOccurs="0" maxOccurs="unbounded"/></xs:sequence></xs:complexType></xs:element>
            <xs:element name="tags"><xs:complexType><xs:sequence><xs:element name="tag" type="xs:string" minOccurs="2" maxOccurs="2"/></xs:sequence></xs:complexType></xs:element>
          </xs:sequence><xs:attribute name="id" type="xs:unsignedLong" use="required"/></xs:complexType></xs:element>
        </xs:schema>''', encoding='utf-8')
        source = directory / 'tokens.serializer'
        source.write_text('''serializer version 1.20.0;
namespace tokens {
  xml(namespace("urn:serializer:tokens"));
  using schema "tokens.xsd";
  class child {
    public string unit(1) xml(attribute);
    public int32 value(2) xml(text);
  }
  class root {
    public uint64 id(1) xml(attribute);
    public string label(2);
    public optional<bool> flag(3);
    public child single(4);
    public array<child> children(5);
    public array[2] string tags(6);
  }
}''', encoding='utf-8')
        config = directory / 'tokens.ini'
        config.write_text('''[xml.class.tokens::root]
wrapper.children = children
name.children = child
wrapper.tags = tags
name.tags = tag
''', encoding='utf-8')
        generated = directory / 'tokens.py'
        subprocess.run([str(args.compiler.resolve()), '--language', 'python', '--input', str(source),
                        '--config', str(config), '--xml.runtime_xsd', 'true', '--python.output', str(generated)], check=True)
        python_xml_token_test.module = load_module(generated, 'serializer_python_tokens')
        presence_source = directory / 'presence.serializer'
        presence_source.write_text('''serializer version 1.20.0; namespace presence {
          class detail { public string value(1) xml(text); }
          class record_root { public optional<detail> nested(1); }
          class root {
          public optional<optional<string>> label(1);
          public optional<string> mandatory(2);
          public optional<optional<array<string>>> items(3);
        }}''', encoding='utf-8')
        presence_config = directory / 'presence.ini'
        presence_config.write_text('''[xml]
enabled = true
[xml.class.presence::root]
presence.label = absent_or_nil
presence.mandatory = nil
presence.items = absent_or_nil
wrapper.items = items
name.items = item
[xml.class.presence::record_root]
presence.nested = nil
''', encoding='utf-8')
        presence_output = directory / 'presence.py'
        subprocess.run([str(args.compiler.resolve()), '--language', 'python', '--input', str(presence_source),
                        '--config', str(presence_config), '--python.output', str(presence_output)], check=True)
        python_xml_token_test.presence_module = load_module(presence_output, 'serializer_python_presence')
        container_source = directory / 'containers.serializer'
        container_source.write_text('''serializer version 1.20.0; namespace containers {
          enum state { first, second }
          class payload { public int16 amount(1); }
          class maps {
            public map<uint32,string> numbers(1);
            public map<bool,string> flags(2);
            public optional<map<string,payload>> optional_values(3);
            public map<state,string> states(4);
          }
          class element_choice { public variant(uint32 = code, string = text, payload = record) payload(1); }
          class typed_choice { public variant(uint32 = code, string = text, payload = record) payload(1); }
          class grand { public int16 ancestor(1); }
          class left : public grand ("grand", 1) { public string label(2) xml(attribute); }
          class right { public bool flag(1); }
          class derived : public left ("left", 1), public right ("right", 2) { public uint32 count(3); }
          class emphasis { public string text(1) xml(text); }
          class fragment { public variant(string = text, emphasis = emphasis) value(1); }
          class paragraph { public string id(1) xml(attribute); public array<fragment> content(2); }
        }''', encoding='utf-8')
        container_config = directory / 'containers.ini'
        container_config.write_text('''[xml]
enabled = true
[xml.class.containers::maps]
wrapper.numbers = numbers
map.numbers = entry
map.numbers.key = id
map.numbers.key_placement = attribute
map.numbers.value = data
wrapper.flags = flags
map.flags = entry
map.flags.value = data
wrapper.optional_values = values
map.optional_values = entry
map.optional_values.key_placement = attribute
wrapper.states = states
map.states = entry
[xml.class.containers::element_choice]
wrapper.payload = choice
variant.payload = element
variant.payload.tag.code = number
variant.payload.type.record = {urn:containers}payload
[xml.class.containers::typed_choice]
wrapper.payload = choice
variant.payload = xsi_type
variant.payload.tag.code = {http://www.w3.org/2001/XMLSchema}unsignedInt
variant.payload.tag.text = {http://www.w3.org/2001/XMLSchema}string
variant.payload.tag.record = {urn:containers}payload
[xml.class.containers::derived]
namespace = urn:derived
[xml.class.containers::paragraph]
namespace = urn:paragraph
mixed = content
mixed.child.emphasis = {urn:format}em
''', encoding='utf-8')
        container_output = directory / 'containers.py'
        subprocess.run([str(args.compiler.resolve()), '--language', 'python', '--input', str(container_source),
                        '--config', str(container_config), '--python.output', str(container_output)], check=True)
        python_xml_token_test.containers_module = load_module(container_output, 'serializer_python_containers')
        for label, compositor in (('permutations', 'all'), ('alternatives', 'choice')):
            xsd = directory / (label + '.xsd')
            xsd.write_text('''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="root"><xs:complexType><xs:'''
                + compositor + '''><xs:element name="left" type="xs:int"/><xs:element name="right" type="xs:string"/></xs:'''
                + compositor + '''></xs:complexType></xs:element></xs:schema>''', encoding='utf-8')
            source = directory / (label + '.serializer')
            optional = 'optional<' if compositor == 'choice' else ''
            closing = '>' if compositor == 'choice' else ''
            source.write_text('serializer version 1.20.0; namespace ' + label + ' { using schema "' + label + '.xsd"; class root {'
                + 'public ' + optional + 'int32' + closing + ' left(1); public ' + optional + 'string' + closing + ' right(2); }}', encoding='utf-8')
            generated = directory / (label + '.py')
            subprocess.run([str(args.compiler.resolve()), '--language', 'python', '--input', str(source),
                            '--python.output', str(generated)], check=True)
            setattr(python_xml_token_test, 'all_module' if compositor == 'all' else 'choice_module',
                    load_module(generated, 'serializer_python_' + label))
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(python_xml_token_test))
        if not result.wasSuccessful():
            raise SystemExit(1)


if __name__ == '__main__':
    main()
