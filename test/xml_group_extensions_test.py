# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify legal all/choice groups combined with wildcard attributes and end children."""
import copy
import sys

import xml_all_extensions_test as all_extensions
import xml_ordered_portable_test as ordered

CORE = 'urn:serializer:group-extension:core'
FOREIGN = 'urn:serializer:group-extension:foreign'


def configure(group='all', process='skip'):
    """Retain independent original XSD decisions while layering explicitly authored opaque permissions."""
    execute_sdk = ordered.execute
    all_extensions.configure()
    choice = group != 'all'
    explicit = group == 'choice-explicit'
    first = 'optional<string>' if choice else 'string'
    ordered.MODEL = f'''serializer version 1.21.0;
namespace ordered {{ using schema "groups.xsd"; xml(namespace("{CORE}"));
 class root {{ public {first} first; public optional<string> second; }}
 class fixed {{ public {first} first; public optional<string> second; }}
}}
'''
    ordered.CONFIG = '[xml]\nenabled=true\n'
    if explicit:
        permissions = f'''extension.suffix.kind=element
extension.suffix.namespaces={FOREIGN}
extension.suffix.policy=preserve
extension.suffix.minimum=1
extension.suffix.maximum=2
'''
        ordered.CONFIG += ''.join(f'[xml.class.ordered::{owner}]\n' + permissions for owner in ('root', 'fixed'))
    compositor = '<xs:choice><xs:element name="first" type="xs:string"/><xs:element name="second" type="xs:string"/></xs:choice>' if choice else (
        '<xs:all><xs:element name="first" type="xs:string"/><xs:element name="second" type="xs:string" minOccurs="0"/></xs:all>')
    if choice and not explicit:
        compositor = f'<xs:sequence>{compositor}<xs:any namespace="##other" processContents="{process}" minOccurs="1" maxOccurs="2"/></xs:sequence>'
    xsd = f'''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="{CORE}" elementFormDefault="qualified">
<xs:import namespace="{FOREIGN}" schemaLocation="foreign.xsd"/>''' + ''.join(
        f'<xs:element name="{owner}"><xs:complexType>{compositor}<xs:anyAttribute namespace="##other" processContents="{process}"/></xs:complexType></xs:element>'
        for owner in ('root', 'fixed')) + '</xs:schema>'
    foreign_xsd = f'''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="{FOREIGN}">
 <xs:simpleType name="Number"><xs:restriction base="xs:int"><xs:minInclusive value="1"/><xs:maxInclusive value="9"/></xs:restriction></xs:simpleType>
 <xs:element name="item" type="xs:int"/><xs:attribute name="flag" type="xs:int"/>
</xs:schema>'''

    def execute(args):
        """Check original and emitted documents with an XSD processor plus the independent explicit slot rule."""
        from lxml import etree as provider
        (args.build / 'groups.xsd').write_text(xsd, encoding='utf-8')
        (args.build / 'foreign.xsd').write_text(foreign_xsd, encoding='utf-8')
        validator = provider.XMLSchema(provider.parse(str(args.build / 'groups.xsd')))

        def accepted(root):
            """Project only separately authorized opaque suffixes before testing the original closed XSD."""
            root = copy.deepcopy(root)
            if explicit:
                children = list(root)
                if any((child.tail or '').strip(' \t\r\n') for child in children):
                    return False
                extras = [index for index, child in enumerate(children) if child.tag.startswith('{' + FOREIGN + '}')]
                if not 1 <= len(extras) <= 2 or extras != list(range(len(children) - len(extras), len(children))):
                    return False
                for index in reversed(extras):
                    root.remove(children[index])
            return validator.validate(root)

        for name, text, _, valid in ordered.cases():
            assert accepted(provider.fromstring(text.encode())) == valid, (group, process, name, validator.error_log)
        execute_sdk(args)
        for path in args.build.glob('*.valid.xml.*.xml'):
            assert accepted(provider.parse(str(path)).getroot()), (path, validator.error_log)

    ordered.execute = execute

    def cases():
        """Distinguish opaque authored slots from declaration-validating XSD wildcards."""
        result = []
        for owner in ('root', 'fixed'):
            def add(label, children, valid=True, attributes=''):
                """Use the provider's global attribute inventory without native schema-field shortcuts."""
                result.append((owner + '-' + label,
                    f'<c:{owner} xmlns:c="{CORE}" xmlns:f="{FOREIGN}" {attributes}>{children}</c:{owner}>', 32, valid))
            first, second = '<c:first>A</c:first>', '<c:second>B</c:second>'
            suffix = '<f:item>7</f:item>' if choice else ''
            add('first', first + suffix)
            add('both', first + second + suffix, not choice)
            add('permuted', second + first + suffix, not choice)
            add('second', second + suffix, choice)
            add('missing', suffix, False)
            add('duplicate', first + first + suffix, False)
            add('declared-attribute', first + suffix, attributes='f:flag="5"')
            add('bad-declared-attribute', first + suffix, process == 'skip', 'f:flag="bad"')
            add('unknown-attribute', first + suffix, process != 'strict', 'f:unknown="opaque"')
            add('core-attribute', first + suffix, False, 'c:unknown="opaque"')
            add('unknown-core-child', first + '<c:unknown/>' + suffix, False)
            add('late-text', first + suffix + 'late', False)
            if choice:
                add('two-suffixes', first + suffix + suffix)
                add('missing-suffix', first, False)
                add('excess-suffixes', first + suffix * 3, False)
                add('early-suffix', suffix + first, False)
                add('bad-declared-child', first + '<f:item>bad</f:item>', explicit or process == 'skip')
                add('unknown-child', first + '<f:unknown>opaque<f:nested/>tail</f:unknown>', explicit or process != 'strict')
            else:
                add('foreign-child', first + '<f:item>7</f:item>', False)
        return result

    ordered.cases = cases
    def observe(root):
        """Canonicalize only all-group order while retaining complete opaque subtree semantics."""
        def node(value):
            """Ignore presentation prefixes while comparing names, attributes and mixed-text boundaries."""
            return value.tag, dict(value.attrib), value.text or '', [(node(child), child.tail or '') for child in value]
        children = [(node(child), child.tail or '') for child in root]
        if not choice:
            children.sort(key=lambda child: child[0][0])
        return root.tag, dict(root.attrib), root.text or '', children

    ordered.observe = observe


if __name__ == '__main__':
    settings = {}
    for key in ('group', 'process'):
        option = '--' + key
        if option in sys.argv:
            index = sys.argv.index(option)
            settings[key] = sys.argv[index + 1]
            del sys.argv[index:index + 2]
    configure(**settings)
    ordered.main()
