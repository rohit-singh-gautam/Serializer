# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated JS/TS typed lexical lists, unions and per-field QName scopes."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from xml.sax.saxutils import quoteattr

ROOT = Path(__file__).resolve().parent.parent
NS = 'urn:serializer:lexical-tokens'


class javascript_xml_lexical_test(unittest.TestCase):
    """Execute real owning codecs with every DOM decoder disabled and independent XSD checks."""
    def run_node(self, body):
        """Run one UTF-8 probe while preserving assertion diagnostics from the actual Node SDK."""
        source = self.directory / 'lexical-probe.mjs'
        source.write_text("import assert from 'node:assert/strict';\nimport * as Model from './lexical.mjs';\n"
                          "import * as Native from './native.mjs';\nimport {DOMParser} from '@xmldom/xmldom';\n"
                          "DOMParser.prototype.parseFromString=()=>{throw new Error('DOM forbidden');};\n"
                          "for(const item of Object.values(Model))if(typeof item==='function'&&'_srlXmlRead' in item)"
                          "item._srlXmlRead=()=>{throw new Error('DOM model reader forbidden');};\n"
                          "const chunks=(value,size)=>{const bytes=new TextEncoder().encode(value);"
                          "return Array.from({length:Math.ceil(bytes.length/size)},(_,i)=>bytes.subarray(i*size,(i+1)*size));};\n"
                          + body, encoding='utf-8')
        result = subprocess.run(['node', str(source)], capture_output=True, text=True, encoding='utf-8')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout

    def assert_documents(self, result):
        """Validate all returned complete outputs with the independently compiled XSD provider."""
        for document in json.loads(result):
            self.assertTrue(self.oracle.validate(self.provider.fromstring(document.encode())), self.oracle.error_log)

    def test_typed_lists_presence_fixed_values_and_native_controls(self):
        """Retain empty/present/absent native arrays, exact BigInts, fixed aliases and native wire bytes."""
        output = self.run_node('''
        const document='<lists xmlns="'''+NS+'''" values="+0007 -8" flags="1 false" words="a b" fixed="007 0008" large="18446744073709551615"/>';
        const outputs=[];
        for(const size of [1,2,7,document.length]){
          const owner=Model.JsLexicalLists.decodeXmlChunks(chunks(document,size));
          assert.deepEqual(owner.values,[7,-8]);assert.deepEqual(owner.flags,[[true,false]]);
          assert.deepEqual(owner.words,['a','b']);assert.deepEqual(owner.fixed,[[7,8]]);
          assert.deepEqual(owner.large,[18446744073709551615n]);assert.deepEqual(owner.missing,[]);
          for(const protocol of Object.values(Model.Protocol)){
            const bytes=owner.encode(protocol);assert.deepEqual(Native.JsLexicalLists.decode(bytes,protocol).encode(protocol),bytes);
            assert.deepEqual(Model.JsLexicalLists.decodeXml(owner.encodeXml()).encode(protocol),bytes);
          }
          outputs.push(owner.encodeXml());
        }
        const empty=Model.JsLexicalLists.decodeXml('<lists xmlns="'''+NS+'''" values="" flags="" words="" large=""/>');
        assert.deepEqual(empty.values,[]);assert.deepEqual(empty.flags,[[]]);assert.deepEqual(empty.fixed,[[7,8]]);
        assert.throws(()=>Model.JsLexicalLists.decodeXml(document.replace('007 0008','7 9')));
        empty.words=['two words'];assert.throws(()=>empty.encodeXml());empty.words=[''];assert.throws(()=>empty.encodeXml());
        console.log(JSON.stringify(outputs));
        ''')
        self.assert_documents(output)

    def test_qname_scopes_defaults_fixed_and_authored_bindings(self):
        """Keep child-local shadowing independent and resolve schema defaults in their schema namespace scope."""
        output = self.run_node('''
        const document='<qnames xmlns="'''+NS+'''" xmlns:q="urn:left" name="q:item"><left xmlns:q="urn:left">q:item</left><right xmlns:q="urn:right">q:item</right></qnames>';
        const outputs=[];
        for(const size of [1,3,19,document.length]){
          const owner=Model.JsLexicalQnames.decodeXmlChunks(chunks(document,size));
          assert.equal(owner.left,'q:item');assert.equal(owner.right,'q:item');assert.equal(owner.name,'q:item');
          assert.deepEqual(owner.category,['schema:item']);assert.deepEqual(owner.fixed,['schema:item']);
          const encoded=owner.encodeXml();outputs.push(encoded);assert.equal(Model.JsLexicalQnames.decodeXml(encoded).right,'q:item');
        }
        const authored=new Model.JsLexicalQnames();authored.name='l:item';authored.left='l:item';authored.right='r:item';
        assert.equal(authored.xmlBindNamespace('l','urn:left'),authored);authored.xmlBindNamespace('r','urn:right');
        outputs.push(authored.encodeXml());
        for(const [prefix,uri] of [['bad:name','urn:x'],['xmlns','urn:x'],['xml','urn:wrong'],['p',''],['p',' urn:x']])
          assert.throws(()=>authored.xmlBindNamespace(prefix,uri));
        assert.throws(()=>Model.JsLexicalQnames.decodeXml(document.replace('urn:right','urn:wrong')));
        assert.throws(()=>Model.JsLexicalQnames.decodeXml(document.replace('name="q:item"','name="missing:item"')));
        console.log(JSON.stringify(outputs));
        ''')
        self.assert_documents(output)

    def test_default_namespace_qnames_and_qname_lists(self):
        """Preserve an unprefixed QName's default identity even when the owning element uses a prefix."""
        output = self.run_node('''
        const simple=Model.JsLexicalSimple.decodeXml('<t:simple xmlns:t="'''+NS+'''" xmlns="urn:left">item</t:simple>');
        assert.equal(simple.value,'item');const encoded=simple.encodeXml();
        assert.equal(Model.JsLexicalSimple.decodeXml(encoded).value,'item');
        const list=Model.JsLexicalNames.decodeXml('<names xmlns="'''+NS+'''" xmlns:a="urn:left" xmlns:b="urn:right" values="a:item b:item"/>');
        assert.deepEqual(list.values,['a:item','b:item']);
        const native=new Model.JsLexicalNames();native.values=['a:item'];native.xmlBindNamespace('a','urn:left');
        assert.throws(()=>Model.JsLexicalNames.decodeXml('<names xmlns="'''+NS+'''" values="missing:item"/>'));
        const repeated=Model.JsLexicalRepeated.decodeXml('<repeated xmlns="'''+NS+'''"><values><value xmlns:q="urn:left">q:item</value><value xmlns:q="urn:right">q:item</value></values></repeated>');
        assert.deepEqual(repeated.values,['q:item','q:item']);
        const shadowed=repeated.encodeXml();assert.deepEqual(Model.JsLexicalRepeated.decodeXml(shadowed).values,repeated.values);
        console.log(JSON.stringify([encoded,list.encodeXml(),native.encodeXml(),shadowed]));
        ''')
        self.assert_documents(output)

    def test_lexical_union_and_unicode_name_oracles(self):
        """Compare union membership, Unicode XML names, prefixes and list length with independent XSD results."""
        cases = []
        for value in ('AB', '123', '18446744073709551615', '18446744073709551616', '-1', '', 'ab', 'ABC'):
            document = '<union xmlns="'+NS+'" value='+quoteattr(value)+'/>'
            cases.append(['JsLexicalUnion', document, self.oracle.validate(self.provider.fromstring(document.encode()))])
        for value in ('a', 'a:b', 'é', 'π:item', 'a b', '', '1a'):
            document = '<names_kind xmlns="'+NS+'" value='+quoteattr(value)+'/>'
            cases.append(['JsLexicalNamesKind', document, self.oracle.validate(self.provider.fromstring(document.encode()))])
        output = self.run_node('''
        const outputs=[];
        for(const [name,document,valid] of '''+json.dumps(cases)+'''){
          if(valid){const owner=Model[name].decodeXmlChunks(chunks(document,1));outputs.push(owner.encodeXml());}
          else assert.throws(()=>Model[name].decodeXml(document),document);
        }
        const namespace=Model.JsLexicalNamespaceUnion.decodeXml('<namespace_union xmlns="'''+NS+'''" xmlns:q="urn:left" value="q:item"/>');
        outputs.push(namespace.encodeXml());
        // libxml2 rejects this valid leading-plus/leading-zero unsignedLong alias; retain the normative value space.
        assert.equal(Model.JsLexicalUnion.decodeXml('<union xmlns="'''+NS+'''" value="+0007"/>').value,'+0007');
        const fallback=Model.JsLexicalFallback.decodeXml('<fallback xmlns="'''+NS+'''" value="ZZ"/>');assert.equal(fallback.value,'ZZ');
        const limits=new Model.XmlLimits({pattern:new Model.XmlPatternLimits({maxWorkUnits:0})});
        assert.throws(()=>Model.JsLexicalFallback.decodeXml('<fallback xmlns="'''+NS+'''" value="ZZ"/>',limits),/pattern work limit/);
        console.log(JSON.stringify(outputs));
        ''')
        self.assert_documents(output)

    def test_actual_typescript_list_and_namespace_api(self):
        """Compile and run an actual strict typed consumer of native arrays and XML namespace binding."""
        source = self.directory / 'typed.mts'
        source.write_text("import {JsLexicalLists,JsLexicalNames} from './lexical.mjs';\n"
                          "const lists=new JsLexicalLists();const values:number[]=lists.values;const big:bigint[]=lists.large;\n"
                          "lists.values=values;lists.large=big;const names=new JsLexicalNames();names.values=['p:item'];\n"
                          "const chained:JsLexicalNames=names.xmlBindNamespace('p','urn:left');\n"
                          "JsLexicalNames.decodeXmlChunks([new TextEncoder().encode(chained.encodeXml())]);\n", encoding='utf-8')
        subprocess.run(['node', str(self.typescript), '--strict', '--target', 'ES2020', '--module', 'NodeNext',
                        '--moduleResolution', 'NodeNext', '--skipLibCheck', 'false', str(source)], check=True)
        subprocess.run(['node', str(source.with_suffix('.mjs'))], check=True)


def main():
    """Generate real codecs/native controls, require locked SDKs and compile an independent schema oracle."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--node-dependencies', type=Path)
    parser.add_argument('--python-dependencies', type=Path)
    args = parser.parse_args()
    if args.python_dependencies:
        sys.path.insert(0, str(args.python_dependencies.resolve()))
    from lxml import etree as provider
    build = args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository scratch must remain under ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    modules = build / 'node_modules'
    if args.node_dependencies:
        if not modules.is_dir():
            shutil.copytree(args.node_dependencies.resolve(), modules)
    else:
        for filename in ('package.json', 'package-lock.json'):
            shutil.copyfile(ROOT / 'test/resources/xml_shapes/typescript' / filename, build / filename)
        subprocess.run(['npm.cmd' if sys.platform == 'win32' else 'npm', 'ci', '--ignore-scripts', '--no-audit', '--no-fund'],
                       cwd=build, check=True)
    for package, version in (('saxes', '6.0.0'), ('xmlchars', '2.2.0'), ('typescript', '5.9.3'), ('@xmldom/xmldom', '0.9.12')):
        if json.loads((modules / package / 'package.json').read_text(encoding='utf-8'))['version'] != version:
            raise RuntimeError('Actual Node SDK dependency does not match its exact lock: '+package)
    with tempfile.TemporaryDirectory(prefix='javascript-xml-lexical-', dir=build) as temporary:
        directory = Path(temporary)
        source, schema = directory / 'lexical.serializer', directory / 'lexical.xsd'
        source.write_text('''serializer version 1.20.0; namespace js_lexical { using schema "lexical.xsd";
          class lists { public array<int32> values(1) xml(attribute,list); public optional<array<bool>> flags(2) xml(attribute,list);
            public array<string> words(3) xml(attribute,list); public optional<array<int32>> fixed(4) xml(attribute,list);
            public array<uint64> large(5) xml(attribute,list); public optional<array<int32>> missing(6) xml(attribute,list); }
          class qnames { public string name(1); public optional<string> category(2); public optional<string> fixed(3);
            public string left(4); public string right(5); }
          class simple { public string value(1) xml(text); }
          class names { public array<string> values(1) xml(attribute,list); }
          class repeated { xml(namespace("'''+NS+'''")); public array<string> values(1) xml(name("value"),wrapper("values")); }
          class union { public string value(1); } class fallback { public string value(1); }
          class namespace_union { public string value(1); } class names_kind { public string value(1); }
        }''', encoding='utf-8')
        schema.write_text('''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="'''+NS+'''" xmlns:schema="urn:left"
          targetNamespace="'''+NS+'''" elementFormDefault="qualified">
          <xs:simpleType name="Ints"><xs:list itemType="xs:int"/></xs:simpleType><xs:simpleType name="Bools"><xs:list itemType="xs:boolean"/></xs:simpleType>
          <xs:simpleType name="Words"><xs:list itemType="xs:string"/></xs:simpleType><xs:simpleType name="Large"><xs:list itemType="xs:unsignedLong"/></xs:simpleType>
          <xs:simpleType name="QNameList"><xs:list itemType="xs:QName"/></xs:simpleType>
          <xs:simpleType name="Left"><xs:restriction base="xs:QName"><xs:enumeration value="schema:item"/></xs:restriction></xs:simpleType>
          <xs:simpleType name="Right"><xs:restriction base="xs:QName" xmlns:r="urn:right"><xs:enumeration value="r:item"/></xs:restriction></xs:simpleType>
          <xs:simpleType name="Code"><xs:restriction base="xs:string"><xs:pattern value="[A-Z]{2}"/></xs:restriction></xs:simpleType>
          <xs:simpleType name="Either"><xs:union memberTypes="t:Code xs:unsignedLong"/></xs:simpleType>
          <xs:simpleType name="Fallback"><xs:union memberTypes="t:Code xs:string"/></xs:simpleType>
          <xs:simpleType name="NamespaceUnion"><xs:union memberTypes="t:Left xs:unsignedInt"/></xs:simpleType>
          <xs:element name="lists"><xs:complexType><xs:attribute name="values" type="t:Ints" use="required"/>
          <xs:attribute name="flags" type="t:Bools"/><xs:attribute name="words" type="t:Words" use="required"/>
          <xs:attribute name="fixed" type="t:Ints" fixed="7 8"/><xs:attribute name="large" type="t:Large" use="required"/>
          <xs:attribute name="missing" type="t:Ints"/></xs:complexType></xs:element>
          <xs:element name="qnames"><xs:complexType><xs:sequence><xs:element name="left" type="t:Left"/>
          <xs:element name="right" type="t:Right"/></xs:sequence><xs:attribute name="name" type="t:Left" use="required"/>
          <xs:attribute name="category" type="t:Left" default="schema:item"/><xs:attribute name="fixed" type="t:Left" fixed="schema:item"/></xs:complexType></xs:element>
          <xs:element name="simple"><xs:complexType><xs:simpleContent><xs:extension base="t:Left"/></xs:simpleContent></xs:complexType></xs:element>
          <xs:element name="names"><xs:complexType><xs:attribute name="values" type="t:QNameList" use="required"/></xs:complexType></xs:element>
          <xs:element name="repeated"><xs:complexType><xs:sequence><xs:element name="values"><xs:complexType><xs:sequence>
          <xs:element name="value" type="xs:QName" minOccurs="0" maxOccurs="unbounded"/>
          </xs:sequence></xs:complexType></xs:element></xs:sequence></xs:complexType></xs:element>
          <xs:element name="union"><xs:complexType><xs:attribute name="value" type="t:Either" use="required"/></xs:complexType></xs:element>
          <xs:element name="fallback"><xs:complexType><xs:attribute name="value" type="t:Fallback" use="required"/></xs:complexType></xs:element>
          <xs:element name="namespace_union"><xs:complexType><xs:attribute name="value" type="t:NamespaceUnion" use="required"/></xs:complexType></xs:element>
          <xs:element name="names_kind"><xs:complexType><xs:attribute name="value" type="xs:NCName" use="required"/></xs:complexType></xs:element>
          </xs:schema>''', encoding='utf-8')
        oracle = provider.XMLSchema(provider.parse(str(schema)))
        for filename, extra in (('lexical', []), ('native', ['--xml.enabled', 'false'])):
            subprocess.run([str(args.compiler.resolve()), '--language', 'js,typescript', '--input', str(source), *extra,
                            '--js.output', str(directory / (filename+'.mjs')),
                            '--typescript.output', str(directory / (filename+'.d.mts'))], check=True)
        javascript_xml_lexical_test.directory = directory
        javascript_xml_lexical_test.typescript = modules / 'typescript/bin/tsc'
        javascript_xml_lexical_test.oracle, javascript_xml_lexical_test.provider = oracle, provider
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(javascript_xml_lexical_test))
        if not result.wasSuccessful():
            raise SystemExit(1)


if __name__ == '__main__':
    main()
