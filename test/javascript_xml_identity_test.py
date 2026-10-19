# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated JS/TS tree-free identity indexes, exact tuples and forward references."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
NS = 'urn:serializer:identity-test'
DOCUMENT = '<registry xmlns="'+NS+'"><ref id="0007"/><ref id="7"/><entry id="7" label="ab"/></registry>'


class javascript_xml_identity_test(unittest.TestCase):
    """Use actual generated models and a mature independent XSD oracle for complete owner scopes."""
    def run_node(self, body):
        """Execute generated token observers with all DOM decoding paths disabled."""
        source=self.directory/'identity-probe.mjs'
        source.write_text("import assert from 'node:assert/strict';\nimport * as Model from './identity.mjs';\n"
                          "import * as Values from './values.mjs';\nimport * as Native from './native.mjs';\n"
                          "import {DOMParser} from '@xmldom/xmldom';\n"
                          "DOMParser.prototype.parseFromString=()=>{throw new Error('DOM forbidden');};\n"
                          "for(const item of [...Object.values(Model),...Object.values(Values)])"
                          "if(typeof item==='function'&&'_srlXmlRead' in item)item._srlXmlRead=()=>{throw new Error('DOM forbidden');};\n"
                          "const document="+json.dumps(DOCUMENT)+";\n"
                          "const chunks=(value,size)=>{const bytes=new TextEncoder().encode(value);"
                          "return Array.from({length:Math.ceil(bytes.length/size)},(_,i)=>bytes.subarray(i*size,(i+1)*size));};\n"
                          +body,encoding='utf-8')
        result=subprocess.run(['node',str(source)],capture_output=True,text=True,encoding='utf-8')
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        return result.stdout

    def test_forward_references_exact_aliases_and_native_wire_controls(self):
        """Resolve requests before keys while preserving native optional values and all protocol bytes."""
        output=self.run_node('''
        const outputs=[];
        for(const size of [1,2,7,document.length]){
          const owner=Model.IdentityXmlRegistry.decodeXmlChunks(chunks(document,size));
          assert.deepEqual(owner.requests[0].id,[7]);assert.deepEqual(owner.entries[0].label,['ab']);
          const encoded=owner.encodeXml();outputs.push(encoded);
          for(const protocol of Object.values(Model.Protocol)){
            const bytes=owner.encode(protocol);assert.deepEqual(Native.IdentityXmlRegistry.decode(bytes,protocol).encode(protocol),bytes);
            assert.deepEqual(Model.IdentityXmlRegistry.decodeXml(encoded).encode(protocol),bytes);
          }
        }
        assert.deepEqual(Model.IdentityXmlRegistry.decodeXml(document.replace('0007','+0007')).requests[0].id,[7]);
        assert(Object.isFrozen(Model.IdentityXmlRegistry._srlXmlToken.identity.constraints[0].fields[0].path.steps));
        console.log(JSON.stringify(outputs));
        ''')
        for document in json.loads(output):
            self.assertTrue(self.oracle.validate(self.provider.fromstring(document.encode())),self.oracle.error_log)

    def test_duplicate_missing_unresolved_and_incomplete_tuples(self):
        """Keys are complete/unique, normalized unique values compare by value, and incomplete references are omitted."""
        bodies=['<entry id="7"/><entry id="0007"/>','<entry/>','<ref id="8"/><entry id="7"/>',
                '<entry id="7" label="a b"/><entry id="8" label=" a&#9;b "/>']
        self.run_node('''
        const type=Model.IdentityXmlRegistry;
        for(const body of '''+json.dumps(bodies)+''')assert.throws(()=>type.decodeXmlChunks(chunks('<registry xmlns="'''+NS+'''">'+body+'</registry>',1)));
        const incomplete=type.decodeXml('<registry xmlns="'''+NS+'''"><ref/><entry id="7"/><entry id="8"/></registry>');
        assert.deepEqual(incomplete.requests[0].id,[]);assert.doesNotThrow(()=>incomplete.encodeXml());
        assert.throws(()=>type.decodeXml(document+'<extra/>'));
        ''')

    def test_output_rejects_invalid_aggregate_before_returning_xml(self):
        """Direct writer observers enforce identical key/reference invariants after native model edits."""
        self.run_node('''
        const owner=Model.IdentityXmlRegistry.decodeXml(document);owner.requests[0].id=[8];assert.throws(()=>owner.encodeXml());
        owner.requests[0].id=[7];owner.entries.push(owner.entries[0]);assert.throws(()=>owner.encodeXml());
        owner.entries.pop();owner.entries[0].id=[];assert.throws(()=>owner.encodeXml());
        ''')

    def test_qname_decimal_tuples_and_nil_field_semantics(self):
        """Use expanded QName/numeric equality and omit nil unique fields while rejecting nil keys."""
        output=self.run_node('''
        const tupleDocument='<tuple xmlns="urn:identity-values"><ref xmlns:p="urn:left" a="p:item" b="1.00"/><entry xmlns:q="urn:left" a="q:item" b="+01"/></tuple>';
        const owner=Values.JsIdentityTuple.decodeXmlChunks(chunks(tupleDocument,1)),encoded=owner.encodeXml();
        assert.doesNotThrow(()=>Values.JsIdentityTuple.decodeXml(encoded));
        const duplicate=tupleDocument.replace('</tuple>','<entry xmlns:z="urn:left" a="z:item" b="001.000"/></tuple>');
        assert.throws(()=>Values.JsIdentityTuple.decodeXml(duplicate));
        assert.throws(()=>Values.JsIdentityTuple.decodeXml(tupleDocument.replace('xmlns:q="urn:left"','xmlns:q="urn:right"')));
        const nil='<value xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:nil="true"/>';
        assert.throws(()=>Values.JsIdentityKeyed.decodeXml('<keyed xmlns="urn:identity-values">'+nil+'</keyed>'));
        const unique=Values.JsIdentityUniqueValue.decodeXml('<unique_value xmlns="urn:identity-values">'+nil+'</unique_value>');
        assert.deepEqual(unique.value,[]);assert.doesNotThrow(()=>unique.encodeXml());
        for(const lexical of ['','present'])assert.throws(()=>Values.JsIdentityKeyed.decodeXml('<keyed xmlns="urn:identity-values"><value>'+lexical+'</value></keyed>'),/nillable/);
        const authored=new Values.JsIdentityKeyed();authored.value=['present'];assert.throws(()=>authored.encodeXml(),/nillable/);
        assert.equal(Values.JsIdentityKeyed._srlXmlToken.identity.constraints[0].fields[0].nillable,true);
        const unselected=Values.JsIdentityKeyedOptional.decodeXml('<keyed_optional xmlns="urn:identity-values"/>');
        assert.deepEqual(unselected.value,[]);assert.doesNotThrow(()=>unselected.encodeXml());
        const empty=Values.JsIdentityUniqueValue.decodeXml('<unique_value xmlns="urn:identity-values"><value/></unique_value>');
        assert.deepEqual(empty.value,['']);assert.doesNotThrow(()=>empty.encodeXml());
        console.log(JSON.stringify([encoded,unique.encodeXml(),empty.encodeXml()]));
        ''')
        for document in json.loads(output):
            root=self.provider.fromstring(document.encode())
            valid=self.values_oracle.validate(root)
            if root.tag=='{urn:identity-values}unique_value' and not valid:
                # libxml2 rejects nil unique fields without a precomputed value; the independent JDK XSD oracle admits them.
                self.assertIn('No precomputed value',str(self.values_oracle.error_log))
            else:
                self.assertTrue(valid,str(self.values_oracle.error_log)+'\n'+document)

    def test_exact_identity_bounds_and_chunk_independent_work(self):
        """Enforce exact owner-local constraint/field/node/tuple/byte/depth/work admission on input and output."""
        self.run_node('''
        const type=Model.IdentityXmlRegistry,limits=identity=>new Model.XmlLimits({identity:new Model.XmlIdentityLimits(identity)});
        const exact={maxConstraints:3,maxFields:1,maxDepth:2,maxSelectedNodes:4,maxTuples:4,maxTupleBytes:5};
        const owner=type.decodeXmlChunks(chunks(document,1),limits(exact));assert.doesNotThrow(()=>owner.encodeXml(limits(exact)));
        for(const [key,value] of Object.entries(exact)){
          const lowered={...exact,[key]:value-1};assert.throws(()=>type.decodeXml(document,limits(lowered)),key);
          assert.throws(()=>owner.encodeXml(limits(lowered)),key);
        }
        let low=0,high=65536;
        while(low+1<high){const middle=Math.floor((low+high)/2);try{type.decodeXml(document,limits({maxWorkUnits:middle}));high=middle;}catch{low=middle;}}
        assert.doesNotThrow(()=>type.decodeXmlChunks(chunks(document,1),limits({maxWorkUnits:high})));
        assert.throws(()=>type.decodeXmlChunks(chunks(document,1),limits({maxWorkUnits:high-1})));
        const empty=type.decodeXml('<registry xmlns="'''+NS+'''"/>',limits({maxSelectedNodes:0,maxTuples:0,maxTupleBytes:0}));
        assert.doesNotThrow(()=>empty.encodeXml(limits({maxSelectedNodes:0,maxTuples:0,maxTupleBytes:0})));
        for(const policy of [{maxWorkUnits:0},{maxConstraints:0},{maxFields:0},{maxDepth:0}])assert.throws(()=>type.decodeXml(document,limits(policy)));
        for(const policy of [{maxTuples:-1},{maxTupleBytes:0.5},{maxWorkUnits:Infinity}])assert.throws(()=>new Model.XmlIdentityLimits(policy));
        ''')

    def test_actual_typescript_owner_local_policy(self):
        """Compile and run an actual strict consumer of generated optional IDs and explicit identity bounds."""
        source=self.directory/'typed.mts'
        source.write_text("import {IdentityXmlRegistry,XmlLimits,XmlIdentityLimits} from './identity.mjs';\n"
                          "const owner=IdentityXmlRegistry.decodeXmlChunks([new TextEncoder().encode("+json.dumps(DOCUMENT)+")],"
                          "new XmlLimits({identity:new XmlIdentityLimits({maxConstraints:3,maxFields:1,maxDepth:2,maxSelectedNodes:4,maxTuples:4,maxTupleBytes:5})}));\n"
                          "const id:number[]=owner.entries[0].id;if(id[0]!==7)throw new Error('native optional ID');\n"
                          "IdentityXmlRegistry.decodeXml(owner.encodeXml());\n",encoding='utf-8')
        subprocess.run(['node',str(self.typescript),'--strict','--target','ES2020','--module','NodeNext',
                        '--moduleResolution','NodeNext','--skipLibCheck','false',str(source)],check=True)
        subprocess.run(['node',str(source.with_suffix('.mjs'))],check=True)


def main():
    """Generate complete SDK fixtures and native controls while requiring exact locked dependencies and lxml."""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',required=True,type=Path)
    parser.add_argument('--build',required=True,type=Path)
    parser.add_argument('--node-dependencies',type=Path)
    parser.add_argument('--python-dependencies',type=Path)
    args=parser.parse_args()
    if args.python_dependencies:sys.path.insert(0,str(args.python_dependencies.resolve()))
    from lxml import etree as provider
    build=args.build.resolve()
    if build==ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:parser.error('Repository scratch must remain under ignored out/')
    build.mkdir(parents=True,exist_ok=True);modules=build/'node_modules'
    if args.node_dependencies:
        if not modules.is_dir():shutil.copytree(args.node_dependencies.resolve(),modules)
    else:
        for filename in ('package.json','package-lock.json'):shutil.copyfile(ROOT/'test/resources/xml_shapes/typescript'/filename,build/filename)
        subprocess.run(['npm.cmd' if sys.platform=='win32' else 'npm','ci','--ignore-scripts','--no-audit','--no-fund'],cwd=build,check=True)
    for package,version in [('saxes','6.0.0'),('xmlchars','2.2.0'),('typescript','5.9.3'),('@xmldom/xmldom','0.9.12')]:
        if json.loads((modules/package/'package.json').read_text(encoding='utf-8'))['version']!=version:raise RuntimeError('Node dependency does not match exact lock: '+package)
    with tempfile.TemporaryDirectory(prefix='javascript-xml-identity-',dir=build) as temporary:
        directory=Path(temporary);source=directory/'values.serializer';schema=directory/'values.xsd';config=directory/'values.ini'
        source.write_text('''serializer version 1.20.0; namespace js_identity {
          class item { public optional<string> a(1); public optional<string> b(2); }
          class tuple { public array<item> requests(1); public array<item> entries(2); }
          class keyed { public optional<string> value(1); }
          class keyed_optional { public optional<optional<string>> value(1); }
          class unique_value { public optional<string> value(1); }
        }''',encoding='utf-8')
        schema.write_text('''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:identity-values" targetNamespace="urn:identity-values" elementFormDefault="qualified">
          <xs:complexType name="item"><xs:attribute name="a" type="xs:QName"/><xs:attribute name="b" type="xs:decimal"/></xs:complexType>
          <xs:element name="tuple"><xs:complexType><xs:sequence><xs:element name="ref" type="t:item" minOccurs="0" maxOccurs="unbounded"/>
          <xs:element name="entry" type="t:item" minOccurs="0" maxOccurs="unbounded"/></xs:sequence></xs:complexType>
          <xs:keyref name="refs" refer="t:ids"><xs:selector xpath="t:ref"/><xs:field xpath="@a"/><xs:field xpath="@b"/></xs:keyref>
          <xs:key name="ids"><xs:selector xpath=".//t:entry"/><xs:field xpath="@a"/><xs:field xpath="@b"/></xs:key></xs:element>
          <xs:element name="keyed"><xs:complexType><xs:sequence><xs:element name="value" type="xs:string" nillable="true"/></xs:sequence></xs:complexType>
          <xs:key name="keys"><xs:selector xpath="t:value"/><xs:field xpath="."/></xs:key></xs:element>
          <xs:element name="keyed_optional"><xs:complexType><xs:sequence><xs:element name="value" type="xs:string" nillable="true" minOccurs="0"/></xs:sequence></xs:complexType>
          <xs:key name="optional_keys"><xs:selector xpath="t:value"/><xs:field xpath="."/></xs:key></xs:element>
          <xs:element name="unique_value"><xs:complexType><xs:sequence><xs:element name="value" type="xs:string" nillable="true"/></xs:sequence></xs:complexType>
          <xs:unique name="uniques"><xs:selector xpath="t:value"/><xs:field xpath="."/></xs:unique></xs:element></xs:schema>''',encoding='utf-8')
        config.write_text('''[xml]
enabled=true
xsd=values.xsd
[xml.class.js_identity::tuple]
name.requests=ref
name.entries=entry
[xml.class.js_identity::keyed]
presence.value=nil
[xml.class.js_identity::keyed_optional]
presence.value=absent_or_nil
[xml.class.js_identity::unique_value]
presence.value=nil
''',encoding='utf-8')
        shared=ROOT/'test/resources/xml_identity_generated.serializer'
        for filename,selected,extra in [('identity',shared,['--config',str(ROOT/'test/resources/xml_identity_generated.ini')]),
                                         ('native',shared,['--xml.enabled','false']),('values',source,['--config',str(config)])]:
            subprocess.run([str(args.compiler.resolve()),'--language','js,typescript','--input',str(selected),*extra,
                            '--js.output',str(directory/(filename+'.mjs')),'--typescript.output',str(directory/(filename+'.d.mts'))],check=True)
        javascript_xml_identity_test.directory=directory;javascript_xml_identity_test.typescript=modules/'typescript/bin/tsc'
        javascript_xml_identity_test.provider=provider
        javascript_xml_identity_test.oracle=provider.XMLSchema(provider.parse(str(ROOT/'test/resources/xml_identity_generated.xsd')))
        javascript_xml_identity_test.values_oracle=provider.XMLSchema(provider.parse(str(schema)))
        result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(javascript_xml_identity_test))
        if not result.wasSuccessful():raise SystemExit(1)


if __name__=='__main__':main()
