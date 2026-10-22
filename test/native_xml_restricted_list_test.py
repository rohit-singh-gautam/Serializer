# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify restricted atomic/union list items through native namespace companions."""
import native_xml_namespace_graph_test as graph
import portable_xml_restricted_list_test as restricted

graph.MODEL = restricted.shared.MODEL.replace('union.xsd', 'all.xsd')
graph.CONFIG = ''
graph.ALL_XSD = restricted.shared.XSD
graph.ORACLE_XSD = True
graph.KINDS = [('probe', 'OuterProbe', 'outer_probe')]


def probes():
    """Share independently specified item-facet and first-successful member decisions."""
    return [('probe', document, valid) for document, valid in restricted.cases()]


def identity(document):
    """Compare selected item families, values and scoped names without generated metadata."""
    from lxml import etree
    root = etree.fromstring(document)
    result = []
    for node in root:
        text = node.text or ''
        if node.tag == 'preserved':
            result.append((node.tag, text))
            continue
        if node.tag in ('fixed_value', 'default_value'):
            if not text:
                tail = 'literal' if node.tag == 'fixed_value' else 'missing:item'
                values = [('QName', ('urn:allowed', 'item')), ('string', tail)]
            else:
                values = []
                for token in text.split():
                    parts = token.split(':')
                    uri = node.nsmap.get(parts[0], '') if len(parts) == 2 else node.nsmap.get(None, '')
                    name = (uri, parts[-1])
                    values.append(('QName', name) if name == ('urn:allowed', 'item') else ('string', token))
        else:
            values = []
            for token in text.split():
                boolean = token in ('true', 'false') or node.tag == 'boolean_value' and token in ('0', '1')
                values.append(('boolean', token in ('true', '1')) if boolean else ('integer', int(token)))
        result.append((node.tag, values))
    return result


graph.probes = probes
graph.identity = identity

if __name__ == '__main__':
    graph.main()
