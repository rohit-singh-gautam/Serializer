# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify strict/lax wildcard dispatch when the pinned foreign declaration inventory is empty."""
import sys

import xml_all_extensions_test as all_extensions
import xml_ordered_portable_test as ordered

CORE = 'urn:serializer:wildcard:empty:core'
FOREIGN = 'urn:serializer:wildcard:empty:foreign'


def configure(process='strict', retention='preserve'):
    """Reuse owning SDK consumers with no imported schema or foreign declaration DTOs."""
    execute_sdk = ordered.execute
    all_extensions.configure()
    ordered.MODEL = f'''serializer version 1.21.0;
namespace ordered {{
 using schema "validation.xsd";
 xml(namespace("{CORE}"), unknown({retention}));
 class root {{ public string first; public string second; }}
 class fixed {{ public string first; public string second; }}
}}
'''
    ordered.CONFIG = '[xml]\nenabled=true\n'
    xsd = f'<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="{CORE}" elementFormDefault="qualified">' + ''.join(
        f'''<xs:element name="{owner}"><xs:complexType><xs:sequence>
 <xs:element name="first" type="xs:string"/>
 <xs:any namespace="##other" processContents="{process}" minOccurs="0" maxOccurs="2"/>
 <xs:element name="second" type="xs:string"/>
 </xs:sequence><xs:anyAttribute namespace="##other" processContents="{process}"/>
 </xs:complexType></xs:element>''' for owner in ('root', 'fixed')) + '</xs:schema>'

    def cases():
        """Separate declaration absence from known content, position and occurrence validity."""
        result = []
        first, second = '<c:first>A&amp;B</c:first>', '<c:second>C</c:second>'
        foreign = '<f:opaque f:note="unbound:value">head<f:nested/>tail</f:opaque>'
        for owner in ('root', 'fixed'):
            def add(label, children, valid=True, attributes=''):
                """Author each input without relying on generated names or decoder decisions."""
                result.append((owner + '-' + label,
                    f'<c:{owner} xmlns:c="{CORE}" xmlns:f="{FOREIGN}" {attributes}>{children}</c:{owner}>', 64, valid))
            add('known', first + second)
            add('foreign-element', first + foreign + second, process == 'lax')
            add('foreign-attribute', first + second, process == 'lax', 'f:note="unbound:opaque"')
            add('foreign-both', first + foreign * 2 + second, process == 'lax', 'f:note="opaque"')
            add('core-unknown', first + '<c:unknown/>' + second, False)
            add('misplaced', foreign + first + second, False)
            add('excess', first + foreign * 3 + second, False)
            add('missing-known', first + foreign, False)
        return result

    ordered.cases = cases

    def observe(root):
        """Compare retained opaque XML independently while omitting explicit discard results."""
        def node(value):
            """Retain expanded identities and mixed content through generated prefix changes."""
            children = list(value)
            attributes = dict(value.attrib)
            if value is root and retention == 'skip':
                children = [child for child in children if child.tag.startswith('{' + CORE + '}')]
                attributes = {}
            return value.tag, attributes, value.text or '', [(node(child), child.tail or '') for child in children]
        return node(root)

    ordered.observe = observe

    def execute(args):
        """Check original and generated XML using the same independent import-free XSD."""
        from lxml import etree as provider
        (args.build / 'validation.xsd').write_text(xsd, encoding='utf-8')
        validator = provider.XMLSchema(provider.fromstring(xsd.encode()))
        for name, document, _, valid in cases():
            assert validator.validate(provider.fromstring(document.encode())) == valid, (name, validator.error_log)
        execute_sdk(args)
        for output in args.build.glob('*.valid.xml.*.xml'):
            assert validator.validate(provider.parse(str(output))), (output, validator.error_log)

    ordered.execute = execute


if __name__ == '__main__':
    settings = {}
    for key in ('process', 'retention'):
        option = '--' + key
        if option in sys.argv:
            index = sys.argv.index(option)
            settings[key] = sys.argv[index + 1]
            del sys.argv[index:index + 2]
    configure(**settings)
    ordered.main()
