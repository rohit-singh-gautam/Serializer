# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated JS/TS foreign-only extension capture, exact slots and namespace context."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

from python_xml_extension_test import DOCUMENT, NS

ROOT = Path(__file__).resolve().parent.parent
SCHEMA = '''serializer version 1.20.0; namespace extension {
  class root { public uint32 id(1) xml(attribute); public array<uint32> items(2); public string tail(3); }
  class only_skip { public uint32 value(1); }}'''
CONFIG = '''[xml]
enabled=true
[xml.class.extension::root]
namespace='''+NS+'''
name.items=item
extension.attributes.kind=attribute
extension.attributes.namespaces=urn:metadata
extension.attributes.policy=preserve
extension.attributes.maximum=1
extension.before.kind=element
extension.before.before=items
extension.before.namespaces=urn:vendor
extension.before.policy=preserve
extension.before.minimum=1
extension.before.maximum=2
extension.end.kind=element
extension.end.namespaces=urn:end urn:serializer:xml:model
extension.end.policy=preserve
extension.end.maximum=1
extension.skip.kind=element
extension.skip.before=tail
extension.skip.namespaces=urn:skip
extension.skip.policy=skip
extension.reject.kind=element
extension.reject.namespaces=urn:reject
extension.reject.policy=reject
extensions.required=urn:vendor
[xml.class.extension::only_skip]
namespace='''+NS+'''
extension.skip.kind=element
extension.skip.namespaces=urn:skip
extension.skip.policy=skip
'''


class javascript_xml_extension_test(unittest.TestCase):
    """Run actual owning native models with every retained-DOM reader disabled."""
    def run_node(self, body):
        """Execute generated extension policy through the real Node streaming provider."""
        script=self.directory/'probe.mjs'
        script.write_text("import assert from 'node:assert/strict';\nimport * as Model from './extensions.mjs';\n"
                          "import * as Native from './native.mjs';\nimport {DOMParser} from '@xmldom/xmldom';\n"
                          "DOMParser.prototype.parseFromString=()=>{throw new Error('DOM forbidden');};\n"
                          "for(const item of Object.values(Model))if(typeof item==='function'&&'_srlXmlRead' in item)"
                          "item._srlXmlRead=()=>{throw new Error('DOM model reader forbidden');};\n"
                          "const document="+json.dumps(DOCUMENT.decode())+";\n"
                          "const chunks=(value,size)=>{const bytes=new TextEncoder().encode(value);"
                          "return Array.from({length:Math.ceil(bytes.length/size)},(_,i)=>bytes.subarray(i*size,(i+1)*size));};\n"
                          "const policy=(limits={},extra={})=>new Model.XmlLimits({extensions:new Model.XmlExtensionOptions({"
                          "understoodNamespaces:['urn:vendor','urn:parent'],limits:new Model.XmlExtensionLimits(limits),...extra})});\n"
                          +body,encoding='utf-8')
        result=subprocess.run(['node',str(script)],capture_output=True,text=True,encoding='utf-8')
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        return result.stdout

    def test_ordered_foreign_namespaces_and_native_controls(self):
        """Keep unknown payloads opaque while known edits and all ordinary native wire values remain exact."""
        outputs=json.loads(self.run_node('''
        const outputs=[];
        for(const size of [1,2,7,document.length]){
          const value=Model.ExtensionRoot.decodeXmlChunks(chunks(document,size),policy());
          assert.deepEqual([value.id,value.items,value.tail],[7,[1,2],'Z']);
          const before=Object.values(Model.Protocol).map(protocol=>value.encode(protocol));
          const output=value.encodeXml(policy());outputs.push(output);
          for(const [index,protocol] of Object.values(Model.Protocol).entries()){
            assert.deepEqual(Native.ExtensionRoot.decode(before[index],protocol).encode(protocol),before[index]);
            assert.deepEqual(value.encode(protocol),before[index]);
            assert.deepEqual(Model.ExtensionRoot.decodeXml(output,policy()).encode(protocol),before[index]);
          }
          value.items.push(3);assert.deepEqual(Model.ExtensionRoot.decodeXml(value.encodeXml(policy()),policy()).items,[1,2,3]);
          value.items=[];assert.throws(()=>value.encodeXml(policy()),/edited field position/);
        }
        console.log(JSON.stringify(outputs));
        '''))
        for output in outputs:
            root=self.provider.fromstring(output.encode())
            self.assertTrue(self.oracle.validate(root),str(self.oracle.error_log))
            self.assertEqual(root.attrib['{urn:metadata}label'],'p:Thing')
            self.assertEqual(root.nsmap['p'],'urn:parent')
            self.assertEqual([child.tag for child in root],['{urn:vendor}before','{'+NS+'}item',
                             '{urn:vendor}between','{'+NS+'}item','{'+NS+'}tail','{urn:end}after'])
            foreign=root[0]
            self.assertEqual((foreign.text,foreign[0].tail,foreign[1].tail),('head','tail','last'))
            self.assertEqual((foreign.nsmap['n1'],foreign.attrib['{urn:metadata}again']),('urn:another','n1:Thing'))
            self.assertEqual((foreign[0].tag,foreign[0].nsmap['p'],foreign[0].attrib['name']),
                             ('{urn:inner}nest','urn:inner','p:Thing'))
            self.assertEqual(foreign[1].tag,'plain')

    def test_reject_skip_slots_and_required_understanding(self):
        """Preservation cannot establish understanding; skip payloads need no retained-byte budget."""
        self.run_node(r'''
        assert.throws(()=>Model.ExtensionRoot.decodeXml(document),/not understood/);
        assert.throws(()=>Model.ExtensionRoot.decodeXml(document,policy({}, {understoodNamespaces:['urn:vendor'],requiredPrefixes:'p'})),/not understood/);
        assert.throws(()=>Model.ExtensionRoot.decodeXml(document,policy({}, {requiredPrefixes:'missing'})));
        const required=policy({}, {requiredPrefixes:'p'}),value=Model.ExtensionRoot.decodeXml(document,required);
        assert.doesNotThrow(()=>Model.ExtensionRoot.decodeXml(value.encodeXml(required),required));
        const without=document.replace(/<v:before [\s\S]*?<\/v:before>/,'').replace('<v:between/>','');
        for(const invalid of [
          document.replace('<v:between/>','<intruder/>'),
          document.replace('<v:between/>','<q:extra xmlns:q="urn:unlisted"/>'),
          document.replace('<v:between/>','<v:one/><v:two/>'),without,
          document.replace('</root>','<r:denied xmlns:r="urn:reject"/></root>'),
          document.replace('<tail>Z</tail>','<tail>Z</tail><v:late/>'),
          document.replace('id="7"','id="7" unknown="x"'),document+'<extra/>'])assert.throws(()=>Model.ExtensionRoot.decodeXml(invalid,policy()));
        assert.throws(()=>Model.ExtensionRoot.decodeXml(document.replace('<e:after>kept</e:after>','<x:extra xmlns:x="urn:serializer:xml:model"/>'),policy()));
        const skip='<only_skip xmlns="'''+NS+'''" xmlns:s="urn:skip"><value>1</value><s:discard>'+ 'discarded'.repeat(4096)+'</s:discard></only_skip>';
        const stripped=Model.ExtensionOnlySkip.decodeXmlChunks(chunks(skip,1),policy({maxPreservedBytes:0}));
        assert(!stripped.encodeXml().includes('discard'));assert.equal(stripped.value,1);
        assert.throws(()=>new Model.XmlExtensionLimits({maxItems:-1}));
        assert.throws(()=>new Model.XmlExtensionOptions({requiredPrefixes:'p'.repeat(8388609)}));
        assert.throws(()=>Model.ExtensionRoot.decodeXml(document,policy({maxItems:5},{requiredPrefixes:'p '.repeat(6)})));
        ''' )

    def test_exact_bounds_and_chunk_stable_namespace_roundtrips(self):
        """Preserved semantic bytes and prefixes stay stable across provider chunks and repeated replay."""
        self.run_node('''
        let low=0,high=65536;
        while(low<high){const middle=Math.floor((low+high)/2);try{Model.ExtensionRoot.decodeXml(document,policy({maxPreservedBytes:middle}));high=middle;}catch(error){low=middle+1;}}
        const exact=policy({maxSlots:5,maxItems:5,maxPreservedBytes:low});
        let value=Model.ExtensionRoot.decodeXmlChunks(chunks(document,1),exact),previous=null;
        for(let index=0;index<4;++index){const output=value.encodeXml(exact);if(previous!==null)assert.equal(output,previous);
          previous=output;value=Model.ExtensionRoot.decodeXmlChunks(chunks(output,index+1),exact);}
        assert.throws(()=>Model.ExtensionRoot.decodeXml(document,policy({maxItems:4})));
        for(const limits of [{maxSlots:4},{maxItems:3},{maxPreservedBytes:low-1},{maxPreservedBytes:0}]){
          assert.throws(()=>Model.ExtensionRoot.decodeXml(document,policy(limits)));
          assert.throws(()=>value.encodeXml(policy(limits)));
        }
        const utf8=document.replace('head','headé😀');
        const whole=Model.ExtensionRoot.decodeXml(utf8,policy()),split=Model.ExtensionRoot.decodeXmlChunks(chunks(utf8,1),policy());
        assert.equal(whole.encodeXml(policy()),split.encodeXml(policy()));
        value.xmlBindNamespace('p','urn:edited');assert.throws(()=>value.encodeXml(policy()),/conflicts with preserved/);
        ''' )

    def test_actual_typescript_extension_policy(self):
        """Compile and execute the public policy and native owning model with strict TypeScript."""
        script=self.directory/'typed.mts'
        script.write_text("import {ExtensionRoot,XmlLimits,XmlExtensionLimits,XmlExtensionOptions} from './extensions.mjs';\n"
                          "const policy=new XmlLimits({extensions:new XmlExtensionOptions({understoodNamespaces:['urn:vendor','urn:parent'],"
                          "requiredPrefixes:'p',limits:new XmlExtensionLimits({maxSlots:5,maxItems:5})})});\n"
                          "const value=ExtensionRoot.decodeXmlChunks([new TextEncoder().encode("+json.dumps(DOCUMENT.decode())+")],policy);\n"
                          "const known:number[]=value.items;known.push(3);ExtensionRoot.decodeXml(value.encodeXml(policy),policy);\n",
                          encoding='utf-8')
        subprocess.run(['node',str(self.typescript),'--strict','--target','ES2020','--module','NodeNext',
                        '--moduleResolution','NodeNext','--skipLibCheck','false',str(script)],check=True)
        subprocess.run(['node',str(script.with_suffix('.mjs'))],check=True)


def main():
    """Generate XML and native controls and require actual pinned Node/TS and independent XSD providers."""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',required=True,type=Path)
    parser.add_argument('--build',required=True,type=Path)
    parser.add_argument('--node-dependencies',type=Path)
    parser.add_argument('--python-dependencies',type=Path)
    args=parser.parse_args()
    if args.python_dependencies:sys.path.insert(0,str(args.python_dependencies.resolve()))
    from lxml import etree as provider
    build=args.build.resolve()
    if build==ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository scratch must remain under ignored out/')
    build.mkdir(parents=True,exist_ok=True);modules=build/'node_modules'
    if args.node_dependencies:
        if not modules.is_dir():shutil.copytree(args.node_dependencies.resolve(),modules)
    else:
        for name in ('package.json','package-lock.json'):shutil.copyfile(ROOT/'test/resources/xml_shapes/typescript'/name,build/name)
        subprocess.run(['npm.cmd' if sys.platform=='win32' else 'npm','ci','--ignore-scripts','--no-audit','--no-fund'],cwd=build,check=True)
    for package,version in [('saxes','6.0.0'),('xmlchars','2.2.0'),('typescript','5.9.3'),('@xmldom/xmldom','0.9.12')]:
        if json.loads((modules/package/'package.json').read_text(encoding='utf-8'))['version']!=version:
            raise RuntimeError('Actual Node SDK dependency differs from exact lock: '+package)
    with tempfile.TemporaryDirectory(prefix='javascript-xml-extensions-',dir=build) as temporary:
        directory=Path(temporary);source=directory/'extensions.serializer';config=directory/'extensions.ini'
        source.write_text(SCHEMA,encoding='utf-8');config.write_text(CONFIG,encoding='utf-8')
        for filename,extra in [('extensions',['--config',str(config)]),('native',['--xml.enabled','false'])]:
            subprocess.run([str(args.compiler.resolve()),'--language','js,typescript','--input',str(source),*extra,
                            '--js.output',str(directory/(filename+'.mjs')),'--typescript.output',str(directory/(filename+'.d.mts'))],check=True)
        javascript_xml_extension_test.directory=directory;javascript_xml_extension_test.typescript=modules/'typescript/bin/tsc'
        javascript_xml_extension_test.provider=provider
        javascript_xml_extension_test.oracle=provider.XMLSchema(provider.fromstring(('''
        <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="'''+NS+'''" elementFormDefault="qualified">
        <xs:element name="root"><xs:complexType><xs:sequence>
        <xs:choice minOccurs="0" maxOccurs="unbounded"><xs:any namespace="urn:vendor" processContents="skip"/>
        <xs:element name="item" type="xs:unsignedInt"/></xs:choice><xs:element name="tail" type="xs:string"/>
        <xs:any namespace="urn:end" processContents="skip" minOccurs="0" maxOccurs="1"/></xs:sequence>
        <xs:attribute name="id" type="xs:unsignedInt" use="required"/><xs:anyAttribute namespace="urn:metadata" processContents="skip"/>
        </xs:complexType></xs:element></xs:schema>''').encode()))
        result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(javascript_xml_extension_test))
        if not result.wasSuccessful():raise SystemExit(1)


if __name__=='__main__':
    main()
