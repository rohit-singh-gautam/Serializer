# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify one bounded foreign fallback across large owning records and explicit slot boundaries."""
import sys

import xml_all_extensions_test as all_extensions
import xml_ordered_portable_test as ordered

CORE = 'urn:serializer:unknown-policy:core'
FOREIGN = 'urn:serializer:unknown-policy:foreign'
POSITIONED = 'urn:serializer:unknown-policy:positioned'
DENIED = 'urn:serializer:unknown-policy:denied'


def configure(field_count=5, retention='preserve'):
    """Keep fallback permissions foreign-only and give explicit URI slots precedence everywhere."""
    execute_sdk = ordered.execute
    all_extensions.configure()
    middle = field_count // 2
    fields = f'public uint32 id xml(attribute,namespace("{FOREIGN}"));' + ''.join(
        f'public string field{index}' + (f' xml(namespace("{FOREIGN}"))' if index == 0 else '') + ';'
        for index in range(field_count))
    ordered.MODEL = f'''serializer version 1.21.0;
namespace ordered {{
 xml(namespace("{CORE}"), unknown({retention}));
 class root {{{fields}}}
 class fixed {{{fields}}}
}}
'''
    slots = f'''extension.positioned.kind=element
extension.positioned.before=field{middle}
extension.positioned.namespaces={POSITIONED}
extension.positioned.policy=preserve
extension.positioned.maximum=1
extension.denied.kind=element
extension.denied.before=field{middle}
extension.denied.namespaces={DENIED}
extension.denied.policy=reject
extension.denied_attributes.kind=attribute
extension.denied_attributes.namespaces={DENIED}
extension.denied_attributes.policy=reject
'''
    ordered.CONFIG = '[xml]\nenabled=true\n' + ''.join(
        f'[xml.class.ordered::{owner}]\n' + slots for owner in ('root', 'fixed'))

    def execute(args):
        """Check retained opaque QName default scopes independently of generated semantic-node APIs."""
        from lxml import etree as provider
        execute_sdk(args)
        for output in args.build.glob('*.valid.xml.*.xml'):
            original = output.with_name(output.name.split('.valid.xml.', 1)[0] + '.valid.xml')
            before, after = provider.parse(str(original)).getroot(), provider.parse(str(output)).getroot()

            def opaque_scopes(root):
                """Ignore known primitive fields while observing every retained unknown subtree's original scope."""
                result = []
                if any(key != '{' + FOREIGN + '}id' and key.startswith('{' + FOREIGN + '}') for key in root.attrib):
                    result.append(('attribute-owner', root.nsmap.get(None, '')))
                for child in root:
                    if child.tag == '{' + FOREIGN + '}field0':
                        continue
                    if child.tag.startswith('{' + POSITIONED + '}') or retention == 'preserve' and child.tag.startswith('{' + FOREIGN + '}'):
                        result.extend((node.tag, node.nsmap.get(None, '')) for node in child.iter())
                return result

            if retention == 'skip':
                for name in list(before.attrib):
                    if name != '{' + FOREIGN + '}id' and name.startswith('{' + FOREIGN + '}'):
                        del before.attrib[name]
            assert opaque_scopes(before) == opaque_scopes(after), output

    ordered.execute = execute

    def cases():
        """Exercise namespace closure, arbitrary boundaries, declared slot policy and native input atomicity."""
        result = []
        known = [f'<{ "f" if index == 0 else "c"}:field{index}>value{index}</{ "f" if index == 0 else "c"}:field{index}>' for index in range(field_count)]
        foreign = '<f:extra f:attr="unbound:opaque">head<f:nested/>tail</f:extra>'
        for owner in ('root', 'fixed'):
            def add(label, content, valid=True, attributes=''):
                """Construct explicit decisions without consulting generated metadata or native values."""
                result.append((owner + '-' + label,
                    f'<c:{owner} xmlns:c="{CORE}" xmlns:f="{FOREIGN}" xmlns:p="{POSITIONED}" xmlns:d="{DENIED}" f:id="7" {attributes}>'
                    + content + f'</c:{owner}>', 1024, valid))
            base = ''.join(known)
            add('plain', base)
            for name, position in (('prefix', 0), ('middle', middle), ('suffix', field_count)):
                add('foreign-' + name, ''.join(known[:position]) + foreign + ''.join(known[position:]))
                add('positioned-' + name, ''.join(known[:position]) + '<p:item/>' + ''.join(known[position:]), position == middle)
                add('denied-' + name, ''.join(known[:position]) + '<d:item/>' + ''.join(known[position:]), False)
            add('all-boundaries', foreign + foreign.join(known) + foreign)
            add('foreign-attribute', base, attributes='f:note="unbound:opaque"')
            add('denied-attribute', base, False, 'd:note="opaque"')
            add('local-attribute', base, False, 'unknown="opaque"')
            add('core-attribute', base, False, 'c:unknown="opaque"')
            add('local-child', base + '<unknown/>', False)
            add('core-child', base + '<c:unknown/>', False)
            add('core-duplicate', base + known[-1], False)
            add('missing-native', ''.join(known[:-1]) + foreign, False)
            add('misordered-native', known[-1] + ''.join(known[:-1]), False)
            add('foreign-name-known', base + '<f:field1/>')
            add('child-named-attribute', base + '<f:id/>')
            add('attribute-named-child', base, attributes='f:field0="opaque"')
            add('explicit-overflow', ''.join(known[:middle]) + '<p:item/><p:item/>' + ''.join(known[middle:]), False)
        return result

    ordered.cases = cases

    def observe(root):
        """Observe fallback discard independently while retaining explicit preservation permissions."""
        def node(value):
            """Compare expanded identities and opaque mixed text through namespace-prefix changes."""
            children = list(value)
            attributes = dict(value.attrib)
            if value is root and retention == 'skip':
                children = [child for child in children if child.tag == '{' + FOREIGN + '}field0' or not child.tag.startswith('{' + FOREIGN + '}')]
                attributes = {key: text for key, text in attributes.items() if key == '{' + FOREIGN + '}id' or not key.startswith('{' + FOREIGN + '}')}
            return value.tag, attributes, value.text or '', [(node(child), child.tail or '') for child in children]
        return node(root)

    ordered.observe = observe


if __name__ == '__main__':
    settings = {}
    for key in ('field-count', 'retention'):
        option = '--' + key
        if option in sys.argv:
            index = sys.argv.index(option)
            settings[key.replace('-', '_')] = int(sys.argv[index + 1]) if key == 'field-count' else sys.argv[index + 1]
            del sys.argv[index:index + 2]
    configure(**settings)
    ordered.main()
