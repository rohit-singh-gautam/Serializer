# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated JS/TS exact revision/magic layouts and owning digest XML projections."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parent.parent
NS='urn:serializer:js-lifecycle'
MODEL='''serializer version 1.20.0; namespace lifecycle {
  xml(namespace("'''+NS+'''"));
  class numeric { public magic(99) { 'SRL' };
    public version uint64 ver(1) { 9007199254740995 } compatibility { 9007199254740993 };
    public uint32 kept(2) xml(attribute);
    obsolete(9007199254740994) public string old(3) { "old-default" };
    created(9007199254740994) public string current(4) { "new-default" }; }
  class dotted { public version version3 ver(1) { "1.10.0" } compatibility { "1.2.0" };
    obsolete("1.3.0") public uint16 old(2); created("1.3.0") public uint16 current(3); }
  class floating { public version float ver(1) { 1.0 } compatibility { 0.5 };
    obsolete(0.75) public uint32 old(2); created(0.75) public uint32 current(3); }
  class base { private version uint16 ver(1) { 3 } compatibility { 1 }; created(2) private uint32 kept(2) { 7 }; }
  class derived : private base { public string extra(10) { "future" }; }
  class typed { public magic uint16(99) { 4660 }; public uint32 value(1); }
  class plain { public uint32 value(1); }
  class envelope { public numeric value(1); }
  class digests { public digest raw(1) xml(attribute); public digest[2] exact(2) xml(attribute);
    public digest(md5) algorithm(3); public array digest[2] items(4); public optional<digest[2]> maybe(5) xml(attribute); }
}'''


class javascript_xml_lifecycle_test(unittest.TestCase):
    """Exercise exact shared model contracts through actual Node and strict TypeScript codecs."""
    def run_node(self,body):
        """Execute private candidates and direct output after disabling every retained DOM entry point."""
        source=self.directory/'probe.mjs'
        source.write_text("import assert from 'node:assert/strict';\nimport * as Model from './models.mjs';\nimport * as Native from './native.mjs';\n"
                          "import {DOMParser} from '@xmldom/xmldom';DOMParser.prototype.parseFromString=()=>{throw new Error('DOM forbidden');};\n"
                          "for(const item of Object.values(Model))if(typeof item==='function'&&'_srlXmlRead' in item)item._srlXmlRead=()=>{throw new Error('DOM reader forbidden');};\n"
                          "const document=(root,revision,payload,magic=null,attributes='')=>'<'+root+' xmlns=\""+NS+"\" xmlns:m=\"urn:serializer:xml:model\"'+"
                          "(revision===null ? '' : ' m:revision=\"'+revision+'\"')+(magic===null ? '' : ' m:magic=\"'+magic+'\"')+attributes+'>'+payload+'</'+root+'>';\n"
                          "const chunks=value=>Array.from(new TextEncoder().encode(value),byte=>Uint8Array.of(byte));\n"
                          "const native=value=>{for(const protocol of Object.values(Model.Protocol)){const bytes=value.encode(protocol);"
                          "const control=Native[value.constructor.name].decode(bytes,protocol,new Native.Limits(undefined,undefined,undefined,undefined,Native.ReadPolicy.FLEXIBLE));"
                          "assert.deepEqual(control.encode(protocol),bytes);}};\n"+body,encoding='utf-8')
        result=subprocess.run(['node',str(source)],capture_output=True,text=True,encoding='utf-8')
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        return result.stdout

    def test_exact_historical_uint64_magic_and_native_protocols(self):
        """Keep revision values above floating precision and omit inactive fields without changing native codecs."""
        self.run_node('''
        for(const [revision,payload,old,current] of [['9007199254740993','<old>retained</old>','retained','new-default'],
          ['9007199254740994','<current>selected</current>','old-default','selected'],
          ['9007199254740995','<current>selected</current>','old-default','selected']]){
          const value=Model.LifecycleNumeric.decodeXmlChunks(chunks(document('numeric',revision,payload,'53524c',' kept="7"')));
          assert.deepEqual([value.ver,value.kept,value.old,value.current],[BigInt(revision),7,old,current]);native(value);
          const output=value.encodeXml();assert(output.includes('revision="'+revision+'"'));assert(output.includes('magic="53524c"'));
          assert.equal(Model.LifecycleNumeric.decodeXml(output).ver,BigInt(revision));
        }
        const typed=Model.LifecycleTyped.decodeXml(document('typed',null,'<value>9</value>','4660'));
        assert(typed.encodeXml().includes('magic="4660"'));native(typed);
        ''' )

    def test_dotted_floating_inherited_and_nested_model_metadata(self):
        """Order dotted components numerically and use native float32 rounding for active field boundaries."""
        self.run_node('''
        for(const [revision,field] of [['1.2.0','old'],['1.2.9','old'],['1.3.0','current'],['1.9.65535','current'],['1.10.0','current']]){
          const value=Model.LifecycleDotted.decodeXml(document('dotted',revision,'<'+field+'>5</'+field+'>'));
          assert.equal(value.ver,revision);assert.equal(Model.LifecycleDotted.decodeXml(value.encodeXml()).ver,revision);native(value);
        }
        for(const [revision,field,expected] of [['0.5','old',0.5],['0.74999999','current',0.75],['0.75','current',0.75],['1','current',1]]){
          const value=Model.LifecycleFloating.decodeXml(document('floating',revision,'<'+field+'>5</'+field+'>'));
          assert.equal(value.ver,expected);assert.equal(Model.LifecycleFloating.decodeXml(value.encodeXml()).ver,expected);native(value);
        }
        const derived=Model.LifecycleDerived.decodeXml(document('derived','1','<extra>present</extra>'));
        assert.equal(Object.hasOwn(derived,'base0'),false);assert.equal(derived.extra,'present');assert(!derived.encodeXml().includes('<kept>'));native(derived);
        const envelope=new Model.LifecycleEnvelope();envelope.value.ver=9007199254740995n;envelope.value.kept=7;envelope.value.current='selected';
        const output=envelope.encodeXml();assert.equal(Model.LifecycleEnvelope.decodeXmlChunks(chunks(output)).value.current,'selected');
        assert.throws(()=>Model.LifecycleEnvelope.decodeXml(output.replace('53524c','53524d')));
        ''' )

    def test_reserved_metadata_and_inactive_fields_reject(self):
        """Reject missing/unknown/wrong lifecycle metadata, unsupported layouts and noncanonical revisions."""
        self.run_node('''
        const valid=document('numeric','9007199254740995','<current>x</current>','53524c',' kept="7"');
        for(const invalid of [valid.replace(' m:revision="9007199254740995"',''),valid.replace(' m:magic="53524c"',''),
          valid.replace('53524c','53524C'),valid.replace('m:revision=','revision='),valid.replace(' m:magic=',' m:unknown="x" m:magic='),
          valid.replace('9007199254740995','9007199254740992'),valid.replace('9007199254740995','9007199254740996'),
          valid.replace('9007199254740995','+9007199254740995'),valid.replace('9007199254740995','09007199254740995'),
          valid.replace('<current>x</current>','<old>x</old>'),valid.replace('<current>x</current>',''),valid+'<extra/>'])assert.throws(()=>Model.LifecycleNumeric.decodeXmlChunks(chunks(invalid)));
        for(const revision of ['1.11.0','1.02.0','1.2','1.2.0.0','65536.1.2',' 1.2.0'])assert.throws(()=>Model.LifecycleDotted.decodeXml(document('dotted',revision,'<old>5</old>')));
        for(const revision of ['0.49','1.01','-0','.5','NaN','INF','1e999','1e-999'])assert.throws(()=>Model.LifecycleFloating.decodeXml(document('floating',revision,'<old>5</old>')));
        for(const attribute of ['m:revision="1"','m:magic="x"','m:unknown="x"'])assert.throws(()=>Model.LifecyclePlain.decodeXml(document('plain',null,'<value>1</value>',null,' '+attribute)));
        ''' )

    def test_exact_digest_hex_extent_and_owning_native_storage(self):
        """Project bytes without hashing and check canonical output with independent XSD hexBinary semantics."""
        output=self.run_node('''
        const source='<digests xmlns="'''+NS+'''" raw="" exact="00fF" maybe="AAbb"><algorithm>'+'01'.repeat(16)+'</algorithm><items><item>AaBb</item><item>0000</item></items></digests>';
        const value=Model.LifecycleDigests.decodeXmlChunks(chunks(source));
        assert.deepEqual([value.raw,value.exact,value.algorithm,value.items,value.maybe],[[],[0,255],Array(16).fill(1),[[170,187],[0,0]],[[170,187]]]);native(value);
        const output=value.encodeXml();assert(output.includes('exact="00ff"'));assert(output.includes('maybe="aabb"'));
        for(const invalid of [source.replace('00fF','00'),source.replace('00fF','00gg'),source.replace('00fF','00 fF'),
          source.replace('AaBb','abc'),source.replace('01'.repeat(16),'01'.repeat(15))])assert.throws(()=>Model.LifecycleDigests.decodeXml(invalid));
        for(const bad of [[0],[true,0],[-1,0],[0,256]]){value.exact=bad;assert.throws(()=>value.encodeXml());}
        value.exact=[0,255];value.maybe=[];assert(!value.encodeXml().includes('maybe='));
        value.raw=Array(1048576).fill(255);assert.equal(Model.LifecycleDigests.decodeXml(value.encodeXml()).raw.length,1048576);
        value.raw.push(0);assert.throws(()=>value.encodeXml());
        assert.throws(()=>Model.LifecycleDigests.decodeXml(output,new Model.XmlLimits({maxTextBytes:8})));
        console.log(output);
        ''')
        self.assertTrue(self.oracle.validate(self.provider.fromstring(output.encode())),str(self.oracle.error_log))

    def test_actual_typescript_lifecycle_and_digest_types(self):
        """Compile a strict consumer without weakening native bigint, dotted-string or byte-array types."""
        source=self.directory/'typed.mts'
        source.write_text("import {LifecycleNumeric,LifecycleDotted,LifecycleDigests} from './models.mjs';\n"
                          "const numeric=new LifecycleNumeric();numeric.ver=9007199254740995n;numeric.kept=7;numeric.current='current';\n"
                          "const version:bigint=LifecycleNumeric.decodeXml(numeric.encodeXml()).ver;\n"
                          "const dotted=new LifecycleDotted();dotted.ver='1.10.0';dotted.current=4;const spelling:string=LifecycleDotted.decodeXml(dotted.encodeXml()).ver;\n"
                          "const digest=new LifecycleDigests();digest.exact=[0,255];const bytes:number[]=LifecycleDigests.decodeXml(digest.encodeXml()).exact;\n"
                          "if(version!==9007199254740995n||spelling!=='1.10.0'||bytes[1]!==255)throw new Error('typed lifecycle');\n",encoding='utf-8')
        subprocess.run(['node',str(self.typescript),'--strict','--target','ES2020','--module','NodeNext','--moduleResolution','NodeNext','--skipLibCheck','false',str(source)],check=True)
        subprocess.run(['node',str(source.with_suffix('.mjs'))],check=True)


def main():
    """Generate actual enabled/native controls, require locked SDKs and independently validate digest values."""
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
    with tempfile.TemporaryDirectory(prefix='javascript-xml-lifecycle-',dir=build) as temporary:
        directory=Path(temporary);source=directory/'models.serializer';config=directory/'models.ini'
        source.write_text(MODEL,encoding='utf-8');config.write_text('[xml]\nenabled=true\n[xml.class.lifecycle::digests]\nwrapper.items=items\nname.items=item\n',encoding='utf-8')
        for filename,extra in [('models',['--config',str(config)]),('native',['--xml.enabled','false'])]:
            subprocess.run([str(args.compiler.resolve()),'--language','js,typescript','--input',str(source),*extra,'--js.output',str(directory/(filename+'.mjs')),'--typescript.output',str(directory/(filename+'.d.mts'))],check=True)
        javascript_xml_lifecycle_test.directory=directory;javascript_xml_lifecycle_test.typescript=modules/'typescript/bin/tsc'
        javascript_xml_lifecycle_test.provider=provider
        javascript_xml_lifecycle_test.oracle=provider.XMLSchema(provider.fromstring(('''
        <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="'''+NS+'''" targetNamespace="'''+NS+'''" elementFormDefault="qualified">
        <xs:simpleType name="Pair"><xs:restriction base="xs:hexBinary"><xs:length value="2"/></xs:restriction></xs:simpleType>
        <xs:simpleType name="Md5"><xs:restriction base="xs:hexBinary"><xs:length value="16"/></xs:restriction></xs:simpleType>
        <xs:element name="digests"><xs:complexType><xs:sequence><xs:element name="algorithm" type="t:Md5"/>
        <xs:element name="items"><xs:complexType><xs:sequence><xs:element name="item" type="t:Pair" minOccurs="0" maxOccurs="unbounded"/>
        </xs:sequence></xs:complexType></xs:element></xs:sequence><xs:attribute name="raw" type="xs:hexBinary" use="required"/>
        <xs:attribute name="exact" type="t:Pair" use="required"/><xs:attribute name="maybe" type="t:Pair"/>
        </xs:complexType></xs:element></xs:schema>''').encode()))
        result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(javascript_xml_lifecycle_test))
        if not result.wasSuccessful():raise SystemExit(1)


if __name__=='__main__':main()
