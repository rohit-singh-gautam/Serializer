# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Verify native QName restrictions with schema-owned identities and independent XSD decisions."""
import native_xml_namespace_graph_test as graph

graph.MODEL = '''serializer version 1.21.0;
namespace names {
  using schema "all.xsd";
  xml(namespace("urn:constraints"));
  class scalar { public string value xml(text); public string name xml(attribute); }
  class lexical { public string value xml(text); }
  class typed { public array<string> values xml(attribute,list); }
  class repeated { public array<scalar> values xml(name("item")); }
}
'''
graph.CONFIG = ''
graph.ALL_XSD = '''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
 xmlns:t="urn:constraints" xmlns:q="urn:allowed" targetNamespace="urn:constraints" elementFormDefault="qualified">
 <xs:simpleType name="Name"><xs:restriction base="xs:QName">
   <xs:enumeration value="q:Item"/><xs:enumeration xmlns:q="urn:second" value="q:Other"/>
 </xs:restriction></xs:simpleType>
 <xs:simpleType name="Names"><xs:list itemType="xs:QName"/></xs:simpleType>
 <xs:simpleType name="NameList"><xs:restriction base="t:Names">
   <xs:enumeration value="q:Item q:Other"/>
   <xs:enumeration xmlns="" value="Local q:Item"/>
 </xs:restriction></xs:simpleType>
 <xs:complexType name="Scalar"><xs:simpleContent><xs:extension base="t:Name">
   <xs:attribute name="name" type="t:Name" use="required"/>
 </xs:extension></xs:simpleContent></xs:complexType>
 <xs:element name="scalar" type="t:Scalar"/>
 <xs:element name="lexical"><xs:complexType><xs:simpleContent><xs:extension base="t:NameList"/></xs:simpleContent></xs:complexType></xs:element>
 <xs:element name="typed"><xs:complexType><xs:attribute name="values" type="t:NameList" use="required"/></xs:complexType></xs:element>
 <xs:element name="repeated"><xs:complexType><xs:sequence><xs:element name="item" type="t:Scalar" minOccurs="0" maxOccurs="unbounded"/></xs:sequence></xs:complexType></xs:element>
 </xs:schema>'''
graph.KINDS = [('scalar', 'NamesScalar', 'names_scalar'), ('lexical', 'NamesLexical', 'names_lexical'),
               ('typed', 'NamesTyped', 'names_typed'), ('repeated', 'NamesRepeated', 'names_repeated')]


def probes():
    """Exercise aliases, local shadowing, ordered list identities, empty and no-namespace values."""
    head = '<t:{kind} xmlns:t="urn:constraints" xmlns:p="urn:allowed" xmlns:a="urn:allowed"'
    documents = []
    for text in ['p:Item', 'a:Item', 'p:Other', 'missing:Item', 'Item']:
        documents.append(('scalar', head.format(kind='scalar') + f' name="a:Item">{text}</t:scalar>'))
    documents += [
        ('scalar', head.format(kind='scalar') + ' xmlns:q="urn:second" name="q:Other">q:Other</t:scalar>'),
        ('scalar', head.format(kind='scalar') + ' name="p:Other">a:Item</t:scalar>'),
        ('repeated', head.format(kind='repeated') + '><t:item name="p:Item">a:Item</t:item><t:item xmlns:p="urn:second" name="p:Other">p:Other</t:item></t:repeated>'),
        ('repeated', head.format(kind='repeated') + '><t:item xmlns:p="urn:second" name="p:Other">p:Item</t:item></t:repeated>'),
    ]
    for text in ['p:Item a:Other', 'a:Item p:Other', '', 'Local p:Item',
                 'p:Other p:Item', 'p:Item p:Wrong', 'p:Item', 'missing:Item p:Other']:
        documents += [
            ('lexical', head.format(kind='lexical') + f'>{text}</t:lexical>'),
            ('typed', head.format(kind='typed') + f' values="{text}"/>'),
        ]
    from lxml import etree
    oracle = etree.XMLSchema(etree.fromstring(graph.ALL_XSD.encode()))
    result = []
    for kind, document in documents:
        node = etree.fromstring(document.encode())
        if 'Local p:Item' in document:
            # libxml2 rejects no-namespace QName list enumerations. The separate
            # expanded-name oracle checks this known provider boundary directly.
            value = node.text if kind == 'lexical' else node.attrib['values']
            valid = graph.shared.expanded(node, value, False) == [('', 'Local'), ('urn:allowed', 'Item')]
        else:
            valid = oracle.validate(node)
        result.append((kind, document, valid))
    return result


def identity(document):
    """Observe QName values in their actual per-occurrence namespace scope after replay."""
    from lxml import etree
    result = []
    for node in etree.fromstring(document).iter():
        if node.text and node.text.strip():
            result.append((node.tag, '#text', [graph.shared.expanded(node, value, False) for value in node.text.split()]))
        for name, value in node.attrib.items():
            result.append((node.tag, name, [graph.shared.expanded(node, token, False) for token in value.split()]))
    return result


graph.probes = probes
graph.identity = identity
original_rust_consumer, original_c_consumer = graph.rust_consumer, graph.c_consumer


def rust_consumer():
    """Reject edited QName values before output while retaining the reusable companion."""
    return original_rust_consumer().replace('let document=match loaded', 'let mut document=match loaded', 1).replace(
        'drop(input);let output=', '''drop(input);
let saved=std::mem::replace(&mut document.value.value,"p:Wrong".to_owned());
assert!(document.value.encode_xml_with_extensions(&document.extensions,limits,None,&options).is_err());
document.value.value=saved;let output=''', 1)


def c_consumer():
    """Require failed edited output to retain an empty caller buffer and the original native value."""
    return original_c_consumer().replace('else{srl_buffer output={0};', '''else{
const srl_string saved=document.value.value;srl_string invalid={0};
require(srl_string_set(&invalid,"p:Wrong",7u)==srl_ok,"Edited QName allocation");
document.value.value=invalid;srl_buffer rejected={0};
require(names_scalar_encode_xml_document(&document,&rejected,NULL,NULL)!=srl_ok&&!rejected.data,"Edited QName rejected");
document.value.value=saved;srl_string_free(&invalid);srl_buffer output={0};''', 1)


graph.rust_consumer, graph.c_consumer = rust_consumer, c_consumer

if __name__ == '__main__':
    graph.main()
