# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify scalar prefix text followed by an exact foreign extension suffix."""
import xml_all_extensions_test as all_extensions
import xml_ordered_portable_test as ordered


def configure():
    """Retain the all test's native companion consumers and independent opaque observations."""
    all_extensions.configure()
    ordered.MODEL = '''serializer version 1.21.0;
namespace ordered {
  class root { public string value xml(text); }
  class fixed { public string value xml(text); }
}
'''

    def cases():
        """Text after a foreign child cannot move into the owning scalar on replay."""
        result = []
        for owner in ('root', 'fixed'):
            def add(name, content, valid=True, attributes='f:note="opaque"'):
                """Render independent fixture decisions without using generated serializers."""
                result.append((owner + '-' + name,
                    f'<{owner} xmlns:f="urn:foreign" {attributes}>{content}</{owner}>', 16, valid))
            suffix = '<f:suffix f:attr="opaque">unbound:opaque<f:nested/>tail</f:suffix>'
            add('text', 'A&amp;B' + suffix)
            add('empty', suffix)
            add('space', ' \t\n' + suffix)
            add('split', 'A<![CDATA[B]]><!--ignored--><?ignored x?>C' + suffix)
            add('missing-suffix', 'A', False)
            add('excess-suffix', 'A' + suffix + suffix, False)
            add('late-text', 'A' + suffix + 'late', False)
            add('late-space', 'A' + suffix + ' ', False)
            add('unknown-child', 'A<unknown/>' + suffix, False)
            add('unknown-attribute', 'A' + suffix, False, 'core="unknown"')
            add('wrong-uri', 'A<x:suffix xmlns:x="urn:wrong"/>', False)
        return result

    ordered.cases = cases

    def observe(root):
        """Observe scalar bytes independently from recursively retained opaque payloads."""
        assert len(root) == 1 and root[0].tag == '{urn:foreign}suffix'
        assert not root[0].tail
        def node(value):
            """Compare expanded names and opaque data without requiring prefix spelling."""
            return value.tag, dict(value.attrib), value.text or '', [
                (node(child), child.tail or '') for child in value]
        return root.tag, dict(root.attrib), root.text or '', node(root[0])

    ordered.observe = observe


if __name__ == '__main__':
    configure()
    ordered.main()
