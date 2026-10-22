# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify declaration-validating C++ legacy repeated choices without forcing a direct-token mapping."""
import sys

import xml_wildcard_skip_cpp_test as shared
import xml_wildcard_validation_test as validation

CORE = 'urn:serializer:legacy-wildcard:core'
FOREIGN = 'urn:serializer:legacy-wildcard:foreign'


def configure(process='strict', retention='preserve'):
    """Retain the established tree grammar while validating declarations, capabilities and local bounds."""
    ordered = shared.ordered
    execute_sdk = ordered.execute
    ordered.MODEL = f'''serializer version 1.21.0;
namespace ordered {{ using schema "validation.xsd";
xml(namespace("{CORE}"){', unknown(skip)' if retention == 'skip' else ''});
class root {{ public array<string> first; public string second; }}
class fixed {{ public array<string> first; public string second; }}
}}
'''
    ordered.CONFIG = '[xml]\nenabled=true\n' + ''.join(
        f'[xml.class.ordered::{owner}]\nextensions.required={FOREIGN}\n' for owner in ('root', 'fixed'))
    xsd = f'''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="{CORE}" elementFormDefault="qualified">
<xs:import namespace="{FOREIGN}" schemaLocation="foreign.xsd"/>''' + ''.join(
        f'''<xs:element name="{owner}"><xs:complexType><xs:sequence>
<xs:choice minOccurs="1" maxOccurs="3"><xs:element name="first" type="xs:string"/>
<xs:any namespace="##other" processContents="{process}"/></xs:choice>
<xs:element name="second" type="xs:string"/></xs:sequence><xs:anyAttribute namespace="##other" processContents="{process}"/>
</xs:complexType></xs:element>''' for owner in ('root', 'fixed')) + '</xs:schema>'
    foreign = f'''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:f="{FOREIGN}" targetNamespace="{FOREIGN}" elementFormDefault="qualified">
<xs:simpleType name="Number"><xs:restriction base="xs:int"><xs:minInclusive value="1"/><xs:maxInclusive value="9"/></xs:restriction></xs:simpleType>
<xs:element name="item" type="f:Number"/><xs:attribute name="flag" type="f:Number"/>
<xs:element name="packet"><xs:complexType><xs:sequence><xs:element name="count" type="f:Number"/></xs:sequence></xs:complexType></xs:element>
</xs:schema>'''

    def cases():
        """Separate whole-choice repetition from per-branch maxima and keep declaration errors independent."""
        result = []
        for owner in ('root', 'fixed'):
            def add(label, content, valid=True, attributes=''):
                """Each document has an independently classified complete owner."""
                result.append((owner + '-' + label, f'<c:{owner} xmlns:c="{CORE}" xmlns:f="{FOREIGN}" {attributes}>{content}</c:{owner}>', 64, valid))
            first, second = '<c:first>A</c:first>', '<c:second>Z</c:second>'
            add('known', first + second)
            add('interleaved', first + '<f:item>7</f:item>' + first + second)
            add('aggregate', first + '<f:item>7</f:item><f:item>8</f:item>' + second)
            add('excess', first + '<f:item>7</f:item>' * 3 + second, False)
            add('scalar-invalid', first + '<f:item>0</f:item>' + second, False)
            add('unknown', first + '<f:unknown/>' + second, process == 'lax')
            add('wrong-core', first + '<c:unknown/>' + second, False)
            add('late', first + second + '<f:item>7</f:item>', False)
            add('missing-tail', first, False)
            add('attribute', first + second, attributes='f:flag="7"')
            add('attribute-invalid', first + second, False, 'f:flag="0"')
            add('attribute-unknown', first + second, process == 'lax', 'f:unknown="opaque"')
            add('packet', first + '<f:packet><f:count>7</f:count></f:packet>' + second)
            add('packet-invalid', first + '<f:packet><f:count>0</f:count></f:packet>' + second, False)
        return result

    def execute(args):
        """Compare original and emitted documents with the pinned independent processor."""
        from lxml import etree
        (args.build / 'validation.xsd').write_text(xsd, encoding='utf-8')
        (args.build / 'foreign.xsd').write_text(foreign, encoding='utf-8')
        provider = etree.XMLSchema(etree.parse(str(args.build / 'validation.xsd')))
        for name, document, _, valid in cases():
            assert provider.validate(etree.fromstring(document.encode())) == valid, (name, provider.error_log)
        execute_sdk(args)

    def observe(node):
        """Discard only selected skipped foreign content while preserving known occurrence order."""
        return (node.tag, {key: value for key, value in node.attrib.items() if retention != 'skip' or not key.startswith('{' + FOREIGN + '}')},
                node.text or '', [observe(child) for child in node if retention != 'skip' or not child.tag.startswith('{' + FOREIGN + '}')])

    ordered.execute, ordered.cases, ordered.observe = execute, cases, observe


def main():
    """Compile the actual generated legacy graph with strict diagnostics and four native protocols."""
    validation.configure = configure
    helpers_begin = shared.CPP.index('// A private declared owner')
    helpers_end = shared.CPP.index('// Keep mutable caller-owned counters')
    shared.CPP = shared.CPP[:helpers_begin] + shared.CPP[helpers_end:]
    shared.CPP = shared.CPP.replace('if(path.ends_with("sequence-nested.valid.xml")){admission_probe(input,value);}', '')
    shared.CPP = shared.CPP.replace('if(path.ends_with("key-valid.valid.xml")){identity_probe(input,value);}', '')
    shared.CPP = shared.CPP.replace('#include <string>', '#include <string>\n#include <vector>')
    shared.CPP = shared.CPP.replace('xml::serialize_tokens(value,{},&context)', 'xml::serialize(value,{},nullptr,&context)')
    shared.CPP = shared.CPP.replace('xml::deserialize_tokens<Model>(input,{},&context)', 'xml::deserialize<Model>(input,{},nullptr,&context)')
    shared.CPP = shared.CPP.replace('for(bool token:{false,true})', 'for(bool token:{false})')
    shared.CPP = shared.CPP.replace('xml::deserialize_tokens<Model>', 'xml::deserialize<Model>').replace('xml::serialize_tokens', 'xml::serialize')
    shared.CPP = shared.CPP.replace('constexpr bool preserve=!RETENTION_SKIP;',
                                    f'const std::vector<std::string> understood{{"{FOREIGN}"}};\nconstexpr bool preserve=!RETENTION_SKIP;')
    shared.CPP = shared.CPP.replace('context.extension_bounds=&extensions;',
                                    'context.extension_bounds=&extensions;context.understood_namespaces=&understood;')
    shared.CPP = shared.CPP.replace('for(bool token:{false}){bool rejected=false;try',
                                    'for(bool token:{false}){xml::operation_context edited_context{};edited_context.understood_namespaces=&understood;bool rejected=false;try')
    shared.CPP = shared.CPP.replace('xml::serialize(edited)', 'xml::serialize(edited,{},nullptr,&edited_context)')
    marker = 'template<typename Model>void check(const std::string& input,const std::string& path,bool valid){'
    shared.CPP = shared.CPP.replace(marker, marker + f'''const std::vector<std::string> understood{{"{FOREIGN}"}};
xml::operation_context context{{}};context.understood_namespaces=&understood;
xml::limits previous{{}};context.document_bounds=&previous;
''')
    shared.CPP = shared.CPP.replace('xml::deserialize<Model>(input)', 'xml::deserialize<Model>(input,{},nullptr,&context)')
    shared.CPP = shared.CPP.replace('xml::deserialize<Model>(output)', 'xml::deserialize<Model>(output,{},nullptr,&context)')
    shared.CPP = shared.CPP.replace('xml::serialize(value)', 'xml::serialize(value,{},nullptr,&context)')
    shared.CPP = shared.CPP.replace('xml::serialize(value,tight)', 'xml::serialize(value,tight,nullptr,&context)')
    extra = r'''
if(context.document_bounds!=&previous)throw std::runtime_error{"Document bounds escaped their scope"};
bool denied=false;try{(void)xml::deserialize<Model>(input);}catch(const std::invalid_argument&){denied=true;}
if(!denied)throw std::runtime_error{"Unknown required vocabulary accepted"};
denied=false;try{(void)xml::serialize(value);}catch(const std::invalid_argument&){denied=true;}
if(!denied)throw std::runtime_error{"Unknown required vocabulary published"};
if(path.find("-packet.valid")!=std::string::npos){
  admission_probe(input,value);
  xml::extension_limits foreign{};foreign.document.max_nodes=1U;context.extension_bounds=&foreign;
  denied=false;try{(void)xml::deserialize<Model>(input,{},nullptr,&context);}catch(const std::length_error&){denied=true;}
  if(!denied)throw std::runtime_error{"Independent foreign document limit reset on decode"};
  if(!SKIP){denied=false;try{(void)xml::serialize(value,{},nullptr,&context);}catch(const std::length_error&){denied=true;}
  if(!denied)throw std::runtime_error{"Independent foreign document limit reset on encode"};}
  foreign.document.max_nodes=2U;(void)xml::deserialize<Model>(input,{},nullptr,&context);
  context.extension_bounds=nullptr;
  if(context.document_bounds!=&previous)throw std::runtime_error{"Failed declaration changed document limits"};
}
'''
    retention = sys.argv[sys.argv.index('--retention') + 1] if '--retention' in sys.argv else 'preserve'
    shared.CPP = shared.CPP.replace('std::ofstream file{path+', extra.replace('SKIP', str(retention == 'skip').lower()) + '\nstd::ofstream file{path+')
    shared.print = lambda *values, **kwargs: print(*(str(value).replace('both XML paths', 'legacy tree XML path') for value in values), **kwargs)
    if '--process' not in sys.argv:
        sys.argv += ['--process', 'strict']
    shared.main()


if __name__ == '__main__':
    main()
