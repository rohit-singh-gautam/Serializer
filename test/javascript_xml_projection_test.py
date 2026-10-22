# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify JS/TS readonly XML projection exports using live native handles and no target construction."""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parent.parent


class javascript_xml_projection_test(unittest.TestCase):
    """Use actual owning models, scoped getters, native wire controls and independent XML namespace checks."""
    def test_scoped_qname_namespace_and_byte_failure_recovery(self):
        """Keep view-only namespace presentation outside borrowed strings and every native wire format."""
        output=json.loads(self.run_node('''
        const source=new Model.XmlProjectionQualifiedSource();source.current.value='p:borrowed';
        const native=Object.values(Model.Protocol).map(protocol=>source.current.encode(protocol));
        const output=source.withProject(view=>{
          assert.equal(view.getValue(),source.current.value);assert.throws(()=>view.encodeXml());
          view.xmlBindNamespace('p','urn:borrowed');const output=view.encodeXml(),length=new TextEncoder().encode(output).length;
          assert.equal(view.encodeXml(new Model.XmlLimits({maxBytes:length})),output);
          let published='previous output';assert.throws(()=>{published=view.encodeXml(new Model.XmlLimits({maxBytes:length-1}));});
          assert.equal(published,'previous output');assert.equal(view.encodeXml(),output);
          for(const [index,protocol] of Object.values(Model.Protocol).entries())assert.deepEqual(view.encode(protocol),native[index]);
          return output;
        });console.log(JSON.stringify(output));
        '''))
        root=self.provider.fromstring(output.encode());prefix,local=root.text.split(':')
        self.assertEqual((root.nsmap[prefix],local),('urn:borrowed','borrowed'))

    def run_node(self,body):
        """Execute generated modules with the retained DOM reader disabled and explicit GC for weak-owner checks."""
        script=self.directory/'probe.mjs'
        script.write_text("import assert from 'node:assert/strict';import * as Model from './model.mjs';import * as Native from './native.mjs';\n"
                          "import * as Extra from './extra.mjs';import {DOMParser} from '@xmldom/xmldom';\n"
                          "DOMParser.prototype.parseFromString=()=>{throw new Error('DOM forbidden');};\n"
                          "for(const item of Object.values(Model))if(typeof item==='function'&&'_srlXmlRead' in item)item._srlXmlRead=()=>{throw new Error('DOM reader forbidden');};\n"
                          "const chunks=value=>Array.from(new TextEncoder().encode(value),byte=>Uint8Array.of(byte));\n"+body,encoding='utf-8')
        result=subprocess.run(['node','--expose-gc',str(script)],capture_output=True,text=True,encoding='utf-8')
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        return result.stdout

    def test_live_record_scalar_handles_and_native_protocol_controls(self):
        """Read the original selected values on each export while preserving all native binding and wire behavior."""
        output=json.loads(self.run_node('''
        const source=new Model.XmlProjectionSource();source.current.number=7;source.current.label='A<&"';
        const owner=new Model.XmlProjectionEnvelope();owner.value.bind(source);owner.label.bind(source);
        assert.equal(owner.value.readValue(),source.current);
        const control=new Native.XmlProjectionSource();control.current.number=7;control.current.label=source.current.label;
        const native=new Native.XmlProjectionEnvelope();native.value.bind(control);native.label.bind(control);
        const before=Object.values(Model.Protocol).map(protocol=>owner.encode(protocol));
        const direct=owner.value.encodeXml(),outer=owner.encodeXml();assert.equal(direct,source.current.encodeXml());
        for(const [index,protocol] of Object.values(Model.Protocol).entries()){
          assert.deepEqual(owner.encode(protocol),before[index]);assert.deepEqual(native.encode(protocol),before[index]);
        }
        source.current.number=0;source.current.label='updated';assert.equal(Model.XmlProjectionPayload.decodeXml(owner.value.encodeXml()).number,0);
        assert.throws(()=>new Model.XmlProjectionEnvelope().encodeXml());owner.value.unbind();assert.throws(()=>owner.encodeXml());
        console.log(JSON.stringify([direct,outer]));
        '''))
        direct,outer=[self.provider.fromstring(value.encode()) for value in output]
        self.assertEqual(direct.attrib,{'number':'7','label':'A<&"'})
        self.assertEqual(outer.attrib['label'],'A<&"');self.assertEqual(outer[0].attrib,direct.attrib)

    def test_scoped_translator_original_values_and_constructor_tripwires(self):
        """Keep arrays/nested objects borrowed; an isolated constructor trap detects accidental target materialization."""
        self.run_node('''
        const source=new Model.XmlProjectionSource();source.current.number=19;source.current.label='borrowed';
        assert.equal(source.withProject(view=>view.encodeXml()),source.current.encodeXml());
        for(const protocol of Object.values(Model.Protocol))assert.deepEqual(source.withProject(view=>view.encode(protocol)),source.current.encode(protocol));
        const owner=new Extra.ProjectionExtraSource();owner.values=[1,2];owner.nested.number=3;
        const exportView=view=>{assert.equal(view.getValues(),owner.values);assert.equal(view.getNested(),owner.nested);return view.encodeXml();};
        let value=Extra.ProjectionExtraTarget.decodeXml(owner.withProject(exportView));
        assert.deepEqual([value.values,value.nested.number,value.fallback],[[1,2],3,'default']);
        owner.values.push(4);owner.nested.number=5;value=Extra.ProjectionExtraTarget.decodeXml(owner.withProject(exportView));
        assert.deepEqual([value.values,value.nested.number],[[1,2,4],5]);
        const Guarded=await import('./extra-constructor-tripwires.mjs');
        const guarded=new Guarded.ProjectionExtraSource();guarded.values=[7,8];guarded.nested.number=9;
        globalThis.srlProjectionForbidTargetConstruction=true;
        try{assert(guarded.withProject(view=>view.encodeXml()).includes('>9<'));}
        finally{globalThis.srlProjectionForbidTargetConstruction=false;}
        ''' )

    def test_export_only_input_before_candidate_or_validator(self):
        """XML input cannot invent an external owner; the ordinary selected target still decodes normally."""
        self.run_node('''
        const source=new Model.XmlProjectionSource();source.current.number=7;source.current.label='x';
        const owner=new Model.XmlProjectionEnvelope();owner.value.bind(source);owner.label.bind(source);const output=owner.encodeXml();
        const marker=new Error('validator should not run');const validator={validate(){throw marker;}};
        assert.throws(()=>Model.XmlProjectionEnvelope.decodeXml(output,undefined,validator),/export-only/);
        assert.throws(()=>Model.XmlProjectionEnvelope.decodeXmlChunks(chunks(output)),/export-only/);
        assert.throws(()=>owner.value.constructor.decodeXml(source.current.encodeXml()),/export-only/);
        assert.throws(()=>owner.label.encodeXml(),/owning field mapping/);
        assert.equal(Model.XmlProjectionPayload.decodeXml(owner.value.encodeXml()).number,7);
        ''' )

    def test_large_scoped_array_storage_limits_and_recovery(self):
        """Keep 25,000 values borrowed through complete output, exact/one-tight limits and a fresh retry."""
        result=json.loads(self.run_node('''
        const Guarded=await import('./extra-constructor-tripwires.mjs');
        const owner=new Guarded.ProjectionExtraSource();owner.values=Array.from({length:25000},(_,index)=>index);owner.nested.number=17;
        const values=owner.values,nested=owner.nested;
        const native=Object.values(Guarded.Protocol).map(protocol=>owner.encode(protocol));
        const exportView=(view,limits)=>{assert.equal(view.getValues(),values);assert.equal(view.getNested(),nested);return view.encodeXml(limits);};
        let output;globalThis.srlProjectionForbidTargetConstruction=true;
        try{
          output=owner.withProject(view=>exportView(view));const length=new TextEncoder().encode(output).length;
          assert.equal(owner.withProject(view=>exportView(view,new Guarded.XmlLimits({maxBytes:length}))),output);
          assert.equal(owner.withProject(view=>exportView(view,new Guarded.XmlLimits({maxDepth:3}))),output);
          assert.throws(()=>owner.withProject(view=>exportView(view,new Guarded.XmlLimits({maxDepth:2}))));
          let published='previous output';assert.throws(()=>{published=owner.withProject(view=>exportView(view,new Guarded.XmlLimits({maxBytes:length-1})));});
          assert.equal(published,'previous output');assert.equal(owner.withProject(view=>exportView(view)),output);
        }finally{globalThis.srlProjectionForbidTargetConstruction=false;}
        for(const [index,protocol] of Object.values(Guarded.Protocol).entries())assert.deepEqual(owner.encode(protocol),native[index]);
        assert.equal(owner.values,values);assert.equal(owner.nested,nested);console.log(JSON.stringify(output));
        '''))
        root=self.provider.fromstring(result.encode())
        self.assertEqual([int(child.text) for child in root.findall('{urn:projection-extra}values')],list(range(25000)))
        self.assertEqual(root.find('{urn:projection-extra}nested')[0].text,'17')
        self.assertEqual(root.find('{urn:projection-extra}fallback').text,'default')

    def test_weak_qname_scope_nil_and_output_policies(self):
        """Preserve selected namespace state, native weak expiration, nil/empty distinctions and exact callback failures."""
        outputs=json.loads(self.run_node('''
        let source=new Model.XmlProjectionQualifiedSource();source.current.value='p:Thing';source.current.xmlBindNamespace('p','urn:qualified-value');
        const owner=new Model.XmlProjectionWeakEnvelope();owner.value.bind(source);const qualified=owner.encodeXml();
        assert.equal(Model.XmlProjectionQualifiedPayload.decodeXml(owner.value.encodeXml()).value,'p:Thing');
        source=null;for(let index=0;index<20;++index){await new Promise(resolve=>setImmediate(resolve));globalThis.gc();}
        assert.equal(owner.value.isBound(),false);assert.throws(()=>owner.encodeXml());
        const nilSource=new Model.XmlProjectionNilSource(),nilOwner=new Model.XmlProjectionNilEnvelope();nilOwner.value.bind(nilSource);
        const nil=nilOwner.encodeXml();nilSource.current.text=[''];const empty=nilOwner.encodeXml();
        const values=new Model.XmlProjectionSource();values.current.number=9;values.current.label='bounded';
        const output=values.withProject(view=>view.encodeXml()),length=new TextEncoder().encode(output).length;
        assert.equal(values.withProject(view=>view.encodeXml(new Model.XmlLimits({maxBytes:length}))),output);
        assert.throws(()=>values.withProject(view=>view.encodeXml(new Model.XmlLimits({maxBytes:length-1}))));
        const marker=new Error('application validator marker');
        try{values.withProject(view=>view.encodeXml(undefined,{validate(){throw marker;}}));assert.fail('validator ignored');}catch(error){assert.equal(error,marker);}
        console.log(JSON.stringify([qualified,nil,empty]));
        '''))
        qualified,nil,empty=[self.provider.fromstring(value.encode()) for value in outputs]
        self.assertEqual(qualified[0].text,'p:Thing');self.assertEqual(qualified[0].nsmap['p'],'urn:qualified-value')
        self.assertEqual(nil[0][0].attrib['{http://www.w3.org/2001/XMLSchema-instance}nil'],'true')
        self.assertNotIn('{http://www.w3.org/2001/XMLSchema-instance}nil',empty[0][0].attrib)

    def test_prepared_provider_completion_rejects_late_projection_cancellation(self):
        """A real prepared XSD may return after cancellation became observable to the readonly caller."""
        self.run_node('''
        const schema=await Model.XmlSchema.prepare(`<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="urn:serializer:xml-projection">
          <xs:element name="payload"><xs:complexType><xs:attribute name="number" type="xs:unsignedInt" use="required"/>
          <xs:attribute name="label" type="xs:string" use="required"/></xs:complexType></xs:element></xs:schema>`);
        try {
          const source=new Model.XmlProjectionSource();source.current.number=7;source.current.label='checked';
          let canceled=false;const marker=new Error('late readonly provider cancellation');
          const limits=new Model.XmlLimits({operation(){if(canceled)throw marker;}});
          const validator={validate(text){schema.validate(text);canceled=true;}};
          assert.throws(()=>source.withProject(view=>view.encodeXml(limits,validator)),error=>error===marker);
          assert.equal(canceled,true);
        } finally {schema.dispose();}
        ''')

    def test_actual_typescript_scoped_view_exports(self):
        """Compile a strict consumer with unchanged native getter/array/reference types and the XML view API."""
        source=self.directory/'typed.mts'
        source.write_text("import {XmlProjectionSource,XmlProjectionQualifiedSource,XmlProjectionEnvelope,XmlProjectionPayload,XmlLimits} from './model.mjs';\n"
                          "const source=new XmlProjectionSource();source.current.number=7;source.current.label='typed';\n"
                          "const output:string=source.withProject(view=>{const value:number=view.getNumber();return view.xmlBindNamespace('p','urn:typed').encodeXml(new XmlLimits());});\n"
                          "const owner=new XmlProjectionEnvelope();owner.value.bind(source);owner.label.bind(source);const selected:XmlProjectionPayload=owner.value.readValue();\n"
                          "if(selected.number!==7||XmlProjectionPayload.decodeXml(output).number!==7)throw new Error('typed projection');\n"
                          "const qualified=new XmlProjectionQualifiedSource();qualified.current.value='p:borrowed';\n"
                          "const names:string=qualified.withProject(view=>{const borrowed:string=view.getValue();if(borrowed!==qualified.current.value)throw new Error('typed QName getter');return view.xmlBindNamespace('p','urn:borrowed').encodeXml();});if(!names.includes('urn:borrowed'))throw new Error('typed QName namespace');\n"
                          "let canceled=false;let failed=false;const marker=new Error('typed late projection cancellation');\n"
                          "try{source.withProject(view=>view.encodeXml(new XmlLimits({operation(){if(canceled)throw marker;}}),{validate(){canceled=true;}}));}\n"
                          "catch(error){if(error!==marker)throw error;failed=true;}if(!canceled||!failed)throw new Error('typed projection escaped cancellation');\n",encoding='utf-8')
        subprocess.run(['node',str(self.typescript),'--strict','--target','ES2020','--module','NodeNext','--moduleResolution','NodeNext','--skipLibCheck','false',str(source)],check=True)
        subprocess.run(['node',str(source.with_suffix('.mjs'))],check=True)


def main():
    """Generate actual enabled/native modules; keep constructor instrumentation isolated from every ordinary SDK probe."""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',required=True,type=Path);parser.add_argument('--build',required=True,type=Path)
    parser.add_argument('--node-dependencies',type=Path);parser.add_argument('--python-dependencies',type=Path)
    args=parser.parse_args()
    if args.python_dependencies:sys.path.insert(0,str(args.python_dependencies.resolve()))
    from lxml import etree as provider
    build=args.build.resolve()
    if build==ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:parser.error('Repository scratch must remain under ignored out/')
    build.mkdir(parents=True,exist_ok=True);modules=build/'node_modules'
    if args.node_dependencies:
        if not modules.is_dir():shutil.copytree(args.node_dependencies.resolve(),modules)
    else:
        for name in ('package.json','package-lock.json'):shutil.copyfile(ROOT/'test/resources/xml_shapes/typescript'/name,build/name)
        subprocess.run(['npm.cmd' if sys.platform=='win32' else 'npm','ci','--ignore-scripts','--no-audit','--no-fund'],cwd=build,check=True)
    for package,version in [('saxes','6.0.0'),('xmlchars','2.2.0'),('typescript','5.9.3'),('@xmldom/xmldom','0.9.12')]:
        if json.loads((modules/package/'package.json').read_text(encoding='utf-8'))['version']!=version:raise RuntimeError('Actual Node SDK dependency differs from exact lock: '+package)
    with tempfile.TemporaryDirectory(prefix='javascript-xml-projection-',dir=build) as temporary:
        directory=Path(temporary);config=directory/'xml.ini'
        config.write_text('[xml]\nenabled=true\nruntime_xsd=true\nvalidation_scope=both\n'+(ROOT/'test/resources/xml_projection.ini').read_text(),encoding='utf-8')
        source=ROOT/'test/resources/xml_projection.serializer'
        for filename,extra in [('model',['--config',str(config)]),('native',['--xml.enabled','false'])]:
            subprocess.run([str(args.compiler.resolve()),'--language','js,typescript','--input',str(source),*extra,
                            '--js.output',str(directory/(filename+'.mjs')),'--typescript.output',str(directory/(filename+'.d.mts'))],check=True)
        extra=directory/'extra.serializer'
        extra.write_text('''serializer version 1.20.0; namespace projection_extra {
          xml(namespace("urn:projection-extra"));
          class child { public uint32 number(1); }
          class target { public array<uint32> values(1); public child nested(2); public string fallback(3) { "default" }; }
          class source { public array<uint32> values(1); public child nested(2);
            public translator target as project { values=values; nested=nested; }; }}''',encoding='utf-8')
        subprocess.run([str(args.compiler.resolve()),'--language','js,typescript','--input',str(extra),'--xml.enabled','true',
                        '--js.output',str(directory/'extra.mjs'),'--typescript.output',str(directory/'extra.d.mts')],check=True)
        instrumented=(directory/'extra.mjs').read_text(encoding='utf-8')
        for name in ('ProjectionExtraTarget','ProjectionExtraChild'):
            start=instrumented.index('export class '+name+' ')
            match=re.search(r'constructor\s*\(\s*\)\s*\{',instrumented[start:])
            if match is None:raise AssertionError('Expected actual generated constructor for tripwire: '+name)
            position=start+match.end()
            instrumented=instrumented[:position]+"if(globalThis.srlProjectionForbidTargetConstruction)throw new Error('Owning projection target constructed');"+instrumented[position:]
        (directory/'extra-constructor-tripwires.mjs').write_text(instrumented,encoding='utf-8')
        javascript_xml_projection_test.directory=directory;javascript_xml_projection_test.typescript=modules/'typescript/bin/tsc'
        javascript_xml_projection_test.provider=provider
        result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(javascript_xml_projection_test))
        if not result.wasSuccessful():raise SystemExit(1)


if __name__=='__main__':main()
