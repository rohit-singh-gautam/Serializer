# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify XSD wildcard namespace predicates, kind dispatch, counts and positions with actual SDKs."""
import sys

import xml_all_extensions_test as all_extensions
import xml_ordered_portable_test as ordered

CORE = 'urn:serializer:wildcard:core'
FOREIGN = 'urn:serializer:wildcard:foreign'


def matches(uri, rule):
    """Classify fixture namespaces independently from Serializer's predicate implementation."""
    if rule == '##any':
        return True
    if rule == '##other':
        return bool(uri) and uri != CORE
    return uri in [CORE if token == '##targetNamespace' else '' if token == '##local' else token
                   for token in rule.split()]


def configure(rule, position='suffix'):
    """Bind both native owners to the same checked XSD skip predicate without authored foreign permissions."""
    execute_sdk = ordered.execute
    all_extensions.configure()
    ordered.MODEL = f'''serializer version 1.21.0;
namespace ordered {{
  using schema "wildcards.xsd";
  xml(namespace("{CORE}"));
  class root {{ public uint32 id xml(attribute,namespace("{CORE}")); public string first; public string second; }}
  class fixed {{ public uint32 id xml(attribute,namespace("{CORE}")); public string first; public string second; }}
}}
'''
    ordered.CONFIG = '[xml]\nenabled=true\n'
    maximum = 1 if position != 'suffix' and matches(CORE, rule) else 2
    fields = ['<xs:element name="first" type="xs:string"/>', '<xs:element name="second" type="xs:string"/>']
    fields.insert({'prefix': 0, 'middle': 1, 'suffix': 2}[position],
        f'<xs:any namespace="{rule}" processContents="skip" minOccurs="1" maxOccurs="{maximum}"/>')
    xsd = f'<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="{CORE}" elementFormDefault="qualified" attributeFormDefault="qualified">' + ''.join(
        f'''<xs:element name="{owner}"><xs:complexType><xs:sequence>{''.join(fields)}
</xs:sequence><xs:attribute name="id" type="xs:unsignedInt" use="required"/>
<xs:anyAttribute namespace="{rule}" processContents="skip"/>
</xs:complexType></xs:element>''' for owner in ('root', 'fixed')) + '</xs:schema>'

    def execute(args):
        """Keep each generated schema and actual SDK build inside the runner's ignored temporary tree."""
        (args.build / 'wildcards.xsd').write_text(xsd, encoding='utf-8')
        execute_sdk(args)

    ordered.execute = execute

    def cases():
        """Require exact namespace predicates while checking independent XSD acceptance for every fixture."""
        from lxml import etree as provider
        validator = provider.XMLSchema(provider.fromstring(xsd.encode()))
        namespaces = ['', CORE, FOREIGN, 'urn:serializer:wildcard:else']
        permitted = next(uri for uri in namespaces if matches(uri, rule))
        def child(uri, name='extra'):
            """Retain opaque nested text and attributes without implying vocabulary understanding."""
            return f'<x:{name} xmlns:x="{uri}" x:opaque="unbound:value">opaque<x:nested/>tail</x:{name}>' if uri else f'<{name} opaque="unbound:value">opaque<nested/>tail</{name}>'
        suffix = child(permitted)
        result = []
        for owner in ('root', 'fixed'):
            def add(name, content, valid=True, extra_attributes='', identifier='7'):
                """Use declared core attributes as known fields and all extra names as wildcard candidates."""
                document = f'<c:{owner} xmlns:c="{CORE}" c:id="{identifier}" {extra_attributes}>{content}</c:{owner}>'
                assert validator.validate(provider.fromstring(document.encode())) == valid, (rule, name, validator.error_log)
                result.append((owner + '-' + name, document, 32, valid))
            first, second = '<c:first>A&amp;B</c:first>', '<c:second>C</c:second>'
            known = first + second
            head, tail = ('', known) if position == 'prefix' else (first, second) if position == 'middle' else (known, '')
            def body(foreign):
                """Place each admitted opaque root at the exact XSD particle boundary."""
                return head + foreign + tail
            add('one', body(suffix))
            add('two', body(suffix + suffix), maximum == 2)
            add('missing', known, False)
            add('excess', body(suffix * (maximum + 1)), False)
            add('wrong-position', known + suffix if position != 'suffix' else suffix + known, False)
            add('known-wildcard', body(first), matches(CORE, rule))
            add('extra-known-wildcard', body(suffix + first), matches(CORE, rule) and maximum == 2)
            add('missing-known', suffix, False)
            add('late-text', body(suffix) + 'late', False)
            add('bad-known', body(suffix), False, identifier='not-an-integer')
            for index, uri in enumerate(namespaces):
                add('child-' + str(index), body(child(uri)), matches(uri, rule))
                attribute = 'note="opaque"' if not uri else f'xmlns:a="{uri}" a:note="opaque"'
                add('attribute-' + str(index), body(suffix), matches(uri, rule), attribute)
            if matches(CORE, rule):
                add('attribute-named-child', body(suffix), True, 'c:first="opaque"')
                add('child-named-attribute', body(child(CORE, 'id')))
        return result

    ordered.cases = cases

    def observe(root):
        """Compare expanded names, opaque subtree order and scalar values without prefix spelling."""
        def node(value):
            """Retain semantic opaque text and nested-child tails exactly."""
            return value.tag, dict(value.attrib), value.text or '', [(node(child), child.tail or '') for child in value]
        return node(root)

    ordered.observe = observe


if __name__ == '__main__':
    selected = '##any'
    placement = 'suffix'
    if '--position' in sys.argv:
        position = sys.argv.index('--position')
        placement = sys.argv[position + 1]
        del sys.argv[position:position + 2]
    if '--namespace-rule' in sys.argv:
        position = sys.argv.index('--namespace-rule')
        selected = sys.argv[position + 1]
        del sys.argv[position:position + 2]
    configure(selected, placement)
    ordered.main()
