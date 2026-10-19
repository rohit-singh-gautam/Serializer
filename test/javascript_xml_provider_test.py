# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify real optional prepared libxml2-wasm XSD validation through generated JS/TS APIs."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parent.parent
XSD='''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="urn:prepared" elementFormDefault="qualified">
<xs:element name="sample"><xs:complexType><xs:sequence><xs:element name="value"><xs:simpleType>
<xs:restriction base="xs:unsignedInt"><xs:maxInclusive value="9"/></xs:restriction></xs:simpleType></xs:element></xs:sequence>
<xs:attribute name="code" use="required"><xs:simpleType><xs:restriction base="xs:string"><xs:minLength value="2"/>
</xs:restriction></xs:simpleType></xs:attribute></xs:complexType></xs:element></xs:schema>'''
DOCUMENT='<sample xmlns="urn:prepared" code="ok"><value>7</value></sample>'


class javascript_xml_provider_test(unittest.TestCase):
    """Exercise real prepared handles, independent operation state, no-fetch and optional dependency boundaries."""
    def run_node(self,body):
        """Execute the generated prepared adapter with the DOM path disabled and the exact installed WASM provider."""
        script=self.directory/'provider-probe.mjs'
        script.write_text("import assert from 'node:assert/strict';import * as Model from './schema.mjs';\n"
                          "import * as Provider from 'libxml2-wasm';import {DOMParser} from '@xmldom/xmldom';\n"
                          "DOMParser.prototype.parseFromString=()=>{throw new Error('DOM preflight forbidden');};\n"
                          "Model.ProviderSample._srlXmlRead=()=>{throw new Error('DOM model reader forbidden');};\n"
                          "const xsd="+json.dumps(XSD)+",document="+json.dumps(DOCUMENT)+";\n"+body,encoding='utf-8')
        result=subprocess.run(['node',str(script)],capture_output=True,text=True,encoding='utf-8')
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        return result.stdout

    def test_real_preparation_declared_roots_and_both_generated_directions(self):
        """Compile once, validate changing native values and refuse undeclared or malformed complete documents."""
        self.run_node('''
        let compilations=0;const prepare=Provider.XsdValidator.fromDoc;
        Provider.XsdValidator.fromDoc=function(...values){++compilations;return prepare.apply(this,values);};
        const schema=await Model.XmlSchema.prepare(xsd);
        for(const value of [0,7,9]){
          const source=document.replace('>7<','>'+value+'<');const owner=Model.ProviderSample.decodeXml(source,undefined,schema);
          assert.equal(owner.value,value);schema.validate(owner.encodeXml(undefined,schema));
        }
        assert.equal(compilations,1);
        const wrong=document.replace('>7<','>10<');assert.equal(Model.ProviderSample.decodeXml(wrong).value,10);
        assert.throws(()=>Model.ProviderSample.decodeXml(wrong,undefined,schema));
        const owner=new Model.ProviderSample();owner.code='ok';owner.value=10;assert.throws(()=>owner.encodeXml(undefined,schema));
        for(const invalid of [document.replace('sample','other'),document.replace('urn:prepared','urn:wrong'),
          document+'<extra/>',document.replace('</sample>',''),document.replace('code="ok"','code="x"')])assert.throws(()=>schema.validate(invalid));
        schema.dispose();schema.dispose();assert.throws(()=>schema.validate(document),/disposed/);
        assert.throws(()=>new Model.XmlSchema());
        ''' )

    def test_exact_schema_and_document_limits(self):
        """Enforce the same bounded SAX policy before provider preparation or document tree allocation."""
        self.run_node('''
        const schema=await Model.XmlSchema.prepare(xsd,new Model.XmlLimits({maxBytes:new TextEncoder().encode(xsd).length}));
        await assert.rejects(()=>Model.XmlSchema.prepare(xsd,new Model.XmlLimits({maxBytes:new TextEncoder().encode(xsd).length-1})));
        let allocations=0;const parse=Provider.XmlDocument.fromString;
        Provider.XmlDocument.fromString=function(...values){++allocations;return parse.apply(this,values);};
        schema.validate(document,new Model.XmlLimits({maxBytes:new TextEncoder().encode(document).length,maxNodes:2,maxDepth:2,maxAttributes:2}));
        assert.equal(allocations,1);
        for(const bounds of [{maxBytes:new TextEncoder().encode(document).length-1},{maxNodes:1},{maxDepth:1},{maxAttributes:1},{maxTextBytes:1}]){
          assert.throws(()=>schema.validate(document,new Model.XmlLimits(bounds)));assert.equal(allocations,1);
        }
        for(const bounds of [{maxNodes:1},{maxDepth:1},{maxAttributes:0},{maxTextBytes:1}])await assert.rejects(()=>Model.XmlSchema.prepare(xsd,new Model.XmlLimits(bounds)));
        assert.equal(allocations,1);schema.dispose();
        ''' )

    def test_no_fetch_and_external_directives_fail_before_provider(self):
        """Reject schema loaders/DTDs without invoking any registered filesystem, URL or provider input hook."""
        self.run_node('''
        let matches=0,opens=0,parses=0;const parse=Provider.XmlDocument.fromString;
        Provider.XmlDocument.fromString=function(...values){++parses;return parse.apply(this,values);};
        assert(Provider.xmlRegisterInputProvider({match(){++matches;return true;},open(){++opens;throw new Error('Unexpected external read');},read(){throw new Error('Unexpected external read');},close(){return true;}}));
        globalThis.fetch=()=>{throw new Error('Unexpected fetch');};
        try{
          for(const kind of ['include','import','redefine','override'])for(const uri of ['file:///private/schema.xsd','http://127.0.0.1:9/schema.xsd']){
            const external=xsd.replace('<xs:element name="sample">','<xs:'+kind+' schemaLocation="'+uri+'"/><xs:element name="sample">');
            await assert.rejects(()=>Model.XmlSchema.prepare(external));assert.equal(parses,0);
          }
          await assert.rejects(()=>Model.XmlSchema.prepare('<!DOCTYPE xs:schema SYSTEM "file:///private/schema.dtd">'+xsd));assert.equal(parses,0);
          const schema=await Model.XmlSchema.prepare(xsd);const before=parses;
          assert.throws(()=>schema.validate('<!DOCTYPE sample [<!ENTITY x SYSTEM "file:///private/value">]>'+document));assert.equal(parses,before);
          const hints=document.replace('code="ok"','code="ok" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="urn:prepared http://127.0.0.1:9/never.xsd"');
          schema.validate(hints);assert.equal(matches,0);assert.equal(opens,0);schema.dispose();
        }finally{Provider.xmlCleanupInputProvider();}
        ''' )

    def test_reuse_across_scheduled_calls_and_parallel_worker_isolates(self):
        """Reuse one compiled schema per isolate; concurrent workers never share mutable native WASM handles."""
        worker=self.directory/'provider-worker.mjs'
        worker.write_text("import {parentPort,workerData} from 'node:worker_threads';import {XmlSchema} from './schema.mjs';\n"
                          "const schema=await XmlSchema.prepare(workerData.xsd);for(let i=0;i<32;++i){schema.validate(workerData.document);"
                          "let rejected=false;try{schema.validate(workerData.document.replace('>7<','>10<'));}catch{rejected=true;}if(!rejected)throw new Error('Invalid worker value');}"
                          "schema.dispose();parentPort.postMessage(true);\n",encoding='utf-8')
        self.run_node('''
        const schema=await Model.XmlSchema.prepare(xsd);
        await Promise.all(Array.from({length:64},(_,index)=>Promise.resolve().then(()=>{
          if(index%2)assert.throws(()=>schema.validate(document.replace('>7<','>10<')));else schema.validate(document);
        })));
        const {Worker}=await import('node:worker_threads');
        await Promise.all(Array.from({length:4},()=>new Promise((resolve,reject)=>{
          const worker=new Worker(new URL('./provider-worker.mjs',import.meta.url),{workerData:{xsd,document}});
          worker.on('message',value=>{assert.equal(value,true);resolve();});worker.on('error',reject);worker.on('exit',code=>{if(code)reject(new Error('Worker exit '+code));});
        })));
        schema.validate(document);schema.dispose();
        ''' )

    def test_actual_typescript_async_preparation_and_optional_absence(self):
        """Compile the typed prepared API and prove ordinary XML works without the optional WASM package."""
        source=self.directory/'typed.mts'
        source.write_text("import {XmlSchema,XmlValidator,ProviderSample,XmlLimits} from './schema.mjs';\n"
                          "const schema:XmlSchema=await XmlSchema.prepare("+json.dumps(XSD)+",new XmlLimits());\n"
                          "const validator:XmlValidator=schema;const value:ProviderSample=ProviderSample.decodeXml("+json.dumps(DOCUMENT)+",undefined,validator);\n"
                          "schema.validate(value.encodeXml(undefined,validator));schema.dispose();\n",encoding='utf-8')
        subprocess.run(['node',str(self.typescript),'--strict','--target','ES2022','--module','NodeNext','--moduleResolution','NodeNext','--skipLibCheck','false',str(source)],check=True)
        subprocess.run(['node',str(source.with_suffix('.mjs'))],check=True)
        missing=self.directory/'without-provider';missing.mkdir(exist_ok=True)
        modules=missing/'node_modules';modules.mkdir(exist_ok=True)
        for package in ('saxes','xmlchars','@xmldom/xmldom'):
            target=modules/package;target.parent.mkdir(parents=True,exist_ok=True)
            if not target.exists():shutil.copytree(self.modules/package,target)
        # The deliberately present invalid package shadows every ancestor's optional installation.
        blocked=modules/'libxml2-wasm';blocked.mkdir(exist_ok=True)
        (blocked/'package.json').write_text('{"name":"libxml2-wasm","type":"module","exports":"./unavailable.mjs"}',encoding='utf-8')
        shutil.copyfile(self.directory/'schema.mjs',missing/'schema.mjs')
        (missing/'main.mjs').write_text("import assert from 'node:assert/strict';import {XmlSchema,ProviderSample} from './schema.mjs';\n"
                                     "assert.equal(ProviderSample.decodeXml("+json.dumps(DOCUMENT)+").value,7);\n"
                                     "await assert.rejects(()=>XmlSchema.prepare("+json.dumps(XSD)+"),/requires optional libxml2-wasm0.7.1/);\n",encoding='utf-8')
        result=subprocess.run(['node',str(missing/'main.mjs')],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)


def main():
    """Generate actual JS/TS codecs and require the independently pinned real prepared provider installation."""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',required=True,type=Path);parser.add_argument('--build',required=True,type=Path)
    parser.add_argument('--node-dependencies',type=Path)
    args=parser.parse_args();build=args.build.resolve()
    if build==ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:parser.error('Repository scratch must remain under ignored out/')
    build.mkdir(parents=True,exist_ok=True);modules=build/'node_modules'
    if args.node_dependencies:
        if not modules.is_dir():shutil.copytree(args.node_dependencies.resolve(),modules)
    else:
        for name in ('package.json','package-lock.json'):shutil.copyfile(ROOT/'test/resources/xml_provider_js'/name,build/name)
        subprocess.run(['npm.cmd' if sys.platform=='win32' else 'npm','ci','--ignore-scripts','--no-audit','--no-fund'],cwd=build,check=True)
    for package,version in [('saxes','6.0.0'),('xmlchars','2.2.0'),('typescript','5.9.3'),('@xmldom/xmldom','0.9.12'),('libxml2-wasm','0.7.1')]:
        if json.loads((modules/package/'package.json').read_text(encoding='utf-8'))['version']!=version:raise RuntimeError('Actual provider/SDK dependency differs from exact lock: '+package)
    with tempfile.TemporaryDirectory(prefix='javascript-xml-provider-',dir=build) as temporary:
        directory=Path(temporary);source=directory/'schema.serializer'
        source.write_text('serializer version 1.20.0; namespace provider { xml(namespace("urn:prepared")); class sample { public string code(1) xml(attribute); public uint32 value(2); }}',encoding='utf-8')
        subprocess.run([str(args.compiler.resolve()),'--language','js,typescript','--input',str(source),'--xml.enabled','true','--xml.runtime_xsd','true','--xml.validation_scope','both','--js.output',str(directory/'schema.mjs'),'--typescript.output',str(directory/'schema.d.mts')],check=True)
        javascript_xml_provider_test.directory=directory;javascript_xml_provider_test.modules=modules;javascript_xml_provider_test.typescript=modules/'typescript/bin/tsc'
        result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(javascript_xml_provider_test))
        if not result.wasSuccessful():raise SystemExit(1)


if __name__=='__main__':main()
