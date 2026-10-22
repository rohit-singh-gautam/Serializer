# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Exercise positioned foreign companions across wrapped, nil, mixed and lifecycle graphs."""
from lxml import etree
import copy
from html import escape

import native_xml_namespace_graph_test as graph


def configure():
    """Extend the namespace graph with exact foreign permissions and independent observations."""
    original_probes = graph.probes
    original_identity = graph.identity
    graph.MODEL = graph.MODEL.replace('  class ordered { public array<ordered_fragment> content; }',
        '  class ordered { public array<ordered_fragment> content; }\n'
        '  class maps { public map(string) q values; }\n'
        '  class maps_attr { public map(string) q values; }\n'
        '  class maps_integer { public map(int64) q values; }\n'
        '  class maps_unsigned { public map(uint64) q values; }\n'
        '  class maps_boolean { public map(bool) q values; }\n'
        '  enum map_key { alpha, beta }\n'
        '  class maps_enum { public map(map_key) q values; }\n'
        '  class selected { public variant(q=record,uint32=number) value; }\n'
        '  class typed { public variant(q=record,uint32=number) value; }')
    graph.KINDS += [('maps', 'GraphMaps', 'graph_maps'), ('selected', 'GraphSelected', 'graph_selected'),
                    ('typed', 'GraphTyped', 'graph_typed'),
                    ('maps_attr', 'GraphMapsAttr', 'graph_maps_attr'),
                    ('maps_integer', 'GraphMapsInteger', 'graph_maps_integer'),
                    ('maps_unsigned', 'GraphMapsUnsigned', 'graph_maps_unsigned'),
                    ('maps_boolean', 'GraphMapsBoolean', 'graph_maps_boolean'),
                    ('maps_enum', 'GraphMapsEnum', 'graph_maps_enum')]
    config = graph.CONFIG + '''[xml.class.graph::maps]
wrapper.values=values
map.values=entry
map.values.value=data
[xml.class.graph::selected]
wrapper.value=selection
variant.value=element
[xml.class.graph::typed]
wrapper.value=selection
variant.value=xsi_type
variant.value.tag.record={urn:types}Q
variant.value.tag.number={http://www.w3.org/2001/XMLSchema}unsignedInt
'''
    for kind in ('maps_attr', 'maps_integer', 'maps_unsigned', 'maps_boolean', 'maps_enum'):
        config += f'[xml.class.graph::{kind}]\nwrapper.values=values\nmap.values=entry\nmap.values.value=data\n'
        if kind in ('maps_attr', 'maps_unsigned', 'maps_boolean'):
            config += 'map.values.key_placement=attribute\n'
    owners = ['graph::q', 'graph::wrapped', 'graph::nullable', 'graph::scalar',
              'graph::life', 'graph::marked', 'graph::mixed', 'graph::ordered',
              'graph::maps', 'graph::selected', 'graph::typed', 'graph::maps_attr',
              'graph::maps_integer', 'graph::maps_unsigned', 'graph::maps_boolean', 'graph::maps_enum']
    # Append properties inside existing sections rather than duplicate INI section names.
    for owner in owners:
        properties = ('extension.foreign.kind=attribute\n'
                      'extension.foreign.namespaces=urn:foreign\n'
                      'extension.foreign.policy=preserve\n'
                      'extension.foreign.maximum=1\n')
        before = {'graph::wrapped': 'values', 'graph::nullable': 'value',
                  'graph::scalar': 'value', 'graph::life': 'value',
                  'graph::marked': 'value'}.get(owner)
        if before:
            properties += ('extension.payload.kind=element\n'
                           'extension.payload.namespaces=urn:foreign\n'
                           'extension.payload.policy=preserve\n'
                           'extension.payload.minimum=1\n'
                           'extension.payload.maximum=2\n'
                           f'extension.payload.before={before}\n')
        if owner in ('graph::mixed', 'graph::ordered'):
            properties += ('extension.suffix.kind=element\n'
                           'extension.suffix.namespaces=urn:foreign\n'
                           'extension.suffix.policy=preserve\n'
                           'extension.suffix.minimum=1\n'
                           'extension.suffix.maximum=2\n')
        section = f'[xml.class.{owner}]\n'
        if section in config:
            config = config.replace(section, section + properties)
        else:
            config += section + properties
    graph.CONFIG = config

    def probes():
        """Require expanded URI admission while retaining opaque text and exact child positions."""
        result = []
        source = original_probes()
        head = '<g:{kind} xmlns:g="urn:graph" xmlns:a="urn:attribute">'
        qname = ('<g:{name} xmlns:p="{uri}" a:名称="p:Attribute">p:Text</g:{name}>')
        source += [('maps', head.format(kind='maps') + '<g:values>' + ''.join(
            '<g:entry><g:key>' + key + '</g:key>' + qname.format(name='data', uri=uri) + '</g:entry>'
            for key, uri in [('one', 'urn:first'), ('two', 'urn:second')]) + '</g:values></g:maps>', True),
            ('selected', head.format(kind='selected') + '<g:selection>' +
             qname.format(name='record', uri='urn:variant') + '</g:selection></g:selected>', True),
            ('selected', head.format(kind='selected') + '<g:selection><g:number>7</g:number></g:selection></g:selected>', True)]
        reversed_map = etree.fromstring(source[-3][1].encode())
        reversed_map[0][:] = list(reversed(list(reversed_map[0])))
        source.append(('maps', etree.tostring(reversed_map, encoding='unicode'), True))
        # Reordered keys must carry their own opaque attributes and QName vocabularies.
        for kind, keys, attribute in [
                ('maps_attr', ['Ω & value', ''], True),
                ('maps_integer', ['+0002', '-0001'], False),
                ('maps_unsigned', ['+018446744073709551615', '0000'], True),
                ('maps_boolean', ['1', 'false'], True),
                ('maps_enum', ['beta', 'alpha'], False)]:
            entries = []
            for key, uri in zip(keys, ('urn:key-first', 'urn:key-second')):
                begin = '<g:entry key="' + escape(key, quote=True) + '">' if attribute else '<g:entry><g:key>' + escape(key) + '</g:key>'
                entries.append(begin + qname.format(name='data', uri=uri) + '</g:entry>')
            source.append((kind, head.format(kind=kind) + '<g:values>' + ''.join(entries) + '</g:values></g:' + kind + '>', True))
        # Alias collisions and native width failures reject before publishing the companion.
        for kind, keys, attribute in [
                ('maps_integer', ['+0', '-0000'], False),
                ('maps_unsigned', ['18446744073709551616', '0'], True),
                ('maps_boolean', ['true', '1'], True),
                ('maps_enum', ['unknown', 'alpha'], False)]:
            entries = []
            for key, uri in zip(keys, ('urn:key-first', 'urn:key-second')):
                begin = '<g:entry key="' + key + '">' if attribute else '<g:entry><g:key>' + key + '</g:key>'
                entries.append(begin + qname.format(name='data', uri=uri) + '</g:entry>')
            source.append((kind, head.format(kind=kind) + '<g:values>' + ''.join(entries) + '</g:values></g:' + kind + '>', False))
        typed_head = head.format(kind='typed').replace('>',
            ' xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:t="urn:types" xmlns:srltype="urn:shadow">')
        source += [('typed', typed_head + '<g:selection>' + qname.format(name='selection', uri='urn:variant').replace(
            ' a:名称=', ' xsi:type="t:Q" a:名称=') + '</g:selection></g:typed>', True),
            ('typed', typed_head + '<g:selection><g:selection xmlns:xs="http://www.w3.org/2001/XMLSchema" xsi:type="xs:unsignedInt">7</g:selection></g:selection></g:typed>', True)]
        for kind, document, valid in source:
            if kind in ('all', 'list'):
                continue
            root = etree.fromstring(document.encode())
            root.set('{urn:foreign}note', 'opaque root')
            for node in root.iter():
                if node is not root and '{urn:attribute}' in ''.join(node.attrib):
                    node.set('{urn:foreign}note', 'opaque child')
            if kind in ('wrapped', 'nullable', 'scalar', 'life', 'marked'):
                payload = etree.Element('{urn:foreign}payload')
                payload.text = 'head'
                nested = etree.SubElement(payload, '{urn:foreign}nested')
                nested.set('lexical', 'unbound:opaque')
                nested.tail = 'tail'
                root.insert(0, payload)
            elif kind in ('mixed', 'ordered'):
                payload = etree.SubElement(root, '{urn:foreign}suffix')
                payload.text = 'opaque suffix'
            encoded = etree.tostring(root, encoding='unicode')
            result.append((kind, encoded, valid))
            if valid:
                result.append((kind, encoded.replace('urn:foreign', 'urn:unpermitted'), False))
                # Core vocabulary remains closed even when it is listed as understood.
                unknown = etree.Element('{urn:graph}unknown')
                root.append(unknown)
                result.append((kind, etree.tostring(root, encoding='unicode'), False))
                root.remove(unknown)
                if kind in ('mixed', 'ordered'):
                    suffix = root[-1]
                    root.extend([copy.deepcopy(suffix), copy.deepcopy(suffix)])
                    result.append((kind, etree.tostring(root, encoding='unicode'), False))
                    root.remove(root[-1])
                    root.remove(root[-1])
                    root.append(copy.deepcopy(root[0]))
                    result.append((kind, etree.tostring(root, encoding='unicode'), False))
                    root.remove(root[-1])
                    suffix.tail = 'late text'
                    result.append((kind, etree.tostring(root, encoding='unicode'), False))
                    suffix.tail = None
                    root.remove(suffix)
                    result.append((kind, etree.tostring(root, encoding='unicode'), False))
                elif kind == 'typed':
                    result.append((kind, encoded.replace('urn:types', 'urn:wrong-types').replace(
                        'http://www.w3.org/2001/XMLSchema"', 'urn:wrong-xs"'), False))
                if kind in ('wrapped', 'nullable', 'scalar', 'life', 'marked'):
                    root.remove(root[0])
                    result.append((kind, etree.tostring(root, encoding='unicode'), False))
        return result

    def identity(document):
        """Compare opaque expanded names/text plus the independently resolved known QName payloads."""
        root = etree.fromstring(document)
        if etree.QName(root).localname.startswith('maps'):
            # Native maps may canonicalize entry order. Scope identity must follow the native key.
            entries = []
            for entry in root.find('{urn:graph}values'):
                key = entry.get('key')
                if key is None:
                    key = entry.find('{urn:graph}key').text or ''
                kind = etree.QName(root).localname
                if kind in ('maps_integer', 'maps_unsigned'):
                    key = str(int(key))
                elif kind == 'maps_boolean':
                    key = 'true' if key in ('true', '1') else 'false'
                value = entry.find('{urn:graph}data')
                entries.append((key, original_identity(etree.tostring(value, encoding='utf-8')),
                                sorted(value.attrib.items())))
            return sorted(entries), sorted(root.attrib.items())
        foreign = []
        for node in root.iter():
            foreign.extend((node.tag, name, value) for name, value in node.attrib.items()
                           if name.startswith('{urn:foreign}'))
            if node.tag.startswith('{urn:foreign}'):
                foreign.append((node.tag, '#text', node.text or '', node.tail or '',
                                tuple(sorted(node.attrib.items()))))
        # Remove foreign children before the existing known-value observer, which treats text as QName.
        for node in list(root.iter()):
            if node.tag.startswith('{urn:foreign}') and node.getparent() is not None:
                node.getparent().remove(node)
            elif node.tag in ('{urn:graph}number', '{urn:graph}key') or (
                    node.tag == '{urn:graph}selection' and node.text == '7'):
                node.text = None
        return original_identity(etree.tostring(root, encoding='utf-8')), foreign

    graph.probes, graph.identity = probes, identity


if __name__ == '__main__':
    configure()
    graph.main()
