# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Keep a foreign schema QName default distinct from an explicit no-namespace instance value."""
import portable_xml_outer_union_test as shared

shared.USE_JAXP = True
shared.XSD = shared.XSD.replace(
    '<xs:element name="default_value" type="NumberFirst" default="1"/>',
    '<xs:element name="default_value" xmlns="urn:allowed" default="item">'
    '<xs:simpleType><xs:union memberTypes="xs:QName xs:string"/></xs:simpleType></xs:element>')


def cases():
    """Empty children use schema scope; explicit lexical values keep their own instance scope."""
    baseline = dict(boolean_value='true', numeric_value='01', list_value='01 2',
                    preserved=' raw ', fixed_value='true', default_value='')
    result = []
    for text in ('', 'p:item', 'item', 'literal text', 'missing:item'):
        values = dict(baseline, default_value=text)
        document = '<probe xmlns:p="urn:allowed">' + ''.join(
            f'<{key}>{value}</{key}>' for key, value in values.items()) + '</probe>'
        result.append((document, True))
    result.append((result[0][0].replace('xmlns:p="urn:allowed"', 'xmlns:p="urn:wrong"'), True))
    result.append((result[0][0].replace('<default_value>', '<default_value unexpected="1">'), False))
    return result


def check_output(source, output):
    """Check expanded QName or ordinary string identity independently of generated scope metadata."""
    from lxml import etree
    def identity(data):
        node = etree.fromstring(data).find('default_value')
        text = node.text or ''
        if not text:
            return ('QName', 'urn:allowed', 'item')
        parts = text.split(':')
        if len(parts) == 1 and text.isidentifier():
            return ('QName', node.nsmap.get(None, ''), text)
        if len(parts) == 2 and parts[0] in node.nsmap:
            return ('QName', node.nsmap[parts[0]], parts[1])
        return ('string', text)
    assert identity(source) == identity(output), (source, output)


shared.cases = cases
shared.CHECK_OUTPUT = check_output
if __name__ == '__main__':
    shared.main()
