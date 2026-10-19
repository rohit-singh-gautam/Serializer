# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated JS/TS ordered mixed native fragments without DOM decoding or known-element trees."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as etree

ROOT = Path(__file__).resolve().parent.parent
DOCUMENT = '<paragraph xmlns="urn:paragraph" id="p">head&amp;😀<f:em xmlns:f="urn:format">middle</f:em>tail<em xmlns="urn:format">last</em>end</paragraph>'
SCHEMA = '''serializer version 1.20.0; namespace js_mixed {
  class emphasis { public string text(1) xml(text); }
  class fragment { private variant(string = text, emphasis = emphasis) value(1); }
  class paragraph { public string id(1) xml(attribute); public array<fragment> content(2); }
}'''
CONFIG = '''[xml]
enabled=true
[xml.class.js_mixed::paragraph]
namespace=urn:paragraph
mixed=content
mixed.child.emphasis={urn:format}em
'''


class javascript_xml_mixed_test(unittest.TestCase):
    """Use the real compiler, private native fragment variants and arbitrary UTF-8 source chunks."""
    def run_node(self, body):
        """Execute generated owning codecs with all DOM input paths disabled."""
        source = self.directory/'mixed-probe.mjs'
        source.write_text("import assert from 'node:assert/strict';\nimport * as Model from './mixed.mjs';\n"
                          "import * as Native from './native.mjs';\nimport {DOMParser} from '@xmldom/xmldom';\n"
                          "DOMParser.prototype.parseFromString=()=>{throw new Error('DOM forbidden');};\n"
                          "for(const item of Object.values(Model))if(typeof item==='function'&&'_srlXmlRead' in item)"
                          "item._srlXmlRead=()=>{throw new Error('DOM model reader forbidden');};\n"
                          "const document="+json.dumps(DOCUMENT)+";\n"
                          "const chunks=(value,size)=>{const bytes=new TextEncoder().encode(value);"
                          "return Array.from({length:Math.ceil(bytes.length/size)},(_,i)=>bytes.subarray(i*size,(i+1)*size));};\n"
                          + body, encoding='utf-8')
        result = subprocess.run(['node', str(source)], capture_output=True, text=True, encoding='utf-8')
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        return result.stdout

    def test_ordered_fragments_chunk_independence_and_private_native_controls(self):
        """Retain one fragment per logical text run/child with original order and unchanged private native wire state."""
        result = self.run_node('''
        const factory=Model.JsMixedFragment._srlXmlMixed,outputs=[];
        for(const size of [1,2,7,document.length]){
          const owner=Model.JsMixedParagraph.decodeXmlChunks(chunks(document,size));
          assert.deepEqual(owner.content.map(item=>factory.get(item)[0]),[0,1,0,1,0]);
          assert.equal(factory.get(owner.content[0])[1],'head&😀');assert.equal(factory.get(owner.content[1])[1].text,'middle');
          assert.equal(Object.hasOwn(owner.content[0],'value'),false);
          const xml=owner.encodeXml();outputs.push(xml);
          for(const protocol of Object.values(Model.Protocol)){
            const bytes=owner.encode(protocol);assert.deepEqual(Native.JsMixedParagraph.decode(bytes,protocol).encode(protocol),bytes);
            assert.deepEqual(Model.JsMixedParagraph.decodeXml(xml).encode(protocol),bytes);
          }
        }
        const joined=Model.JsMixedParagraph.decodeXml('<paragraph xmlns="urn:paragraph" id="p">a<![CDATA[b]]><!--ignored-->c<?ignored x?>d</paragraph>');
        assert.equal(joined.content.length,1);assert.equal(factory.get(joined.content[0])[1],'abcd');
        console.log(JSON.stringify(outputs));
        ''')
        for output in json.loads(result):
            root = etree.fromstring(output)
            self.assertEqual(root.text, 'head&😀')
            self.assertEqual([(child.tag, child.text, child.tail) for child in root], [
                ('{urn:format}em','middle','tail'),('{urn:format}em','last','end')])

    def test_exact_bounds_empty_and_authored_adjacent_fragments(self):
        """Admit exact sequence/text limits, visit empty authored fragments and reject unknown children/alternatives."""
        self.run_node('''
        const limits=values=>new Model.XmlLimits({mixed:new Model.XmlMixedLimits(values)});
        const owner=Model.JsMixedParagraph.decodeXmlChunks(chunks(document,1),limits({maxFragments:5,maxTextBytes:16}));
        assert.doesNotThrow(()=>owner.encodeXml(limits({maxFragments:5,maxTextBytes:16})));
        for(const values of [{maxFragments:4},{maxTextBytes:15}]){
          assert.throws(()=>Model.JsMixedParagraph.decodeXml(document,limits(values)));
          assert.throws(()=>owner.encodeXml(limits(values)));
        }
        const empty=Model.JsMixedParagraph.decodeXml('<paragraph xmlns="urn:paragraph" id="p"/>',limits({maxFragments:0,maxTextBytes:0}));
        assert.deepEqual(empty.content,[]);assert.doesNotThrow(()=>empty.encodeXml(limits({maxFragments:0,maxTextBytes:0})));
        const factory=Model.JsMixedFragment._srlXmlMixed;
        const blank=factory.create(),adjacent=factory.create();factory.set(blank,[0,'']);factory.set(adjacent,[0,'adjacent']);
        owner.content.splice(1,0,blank,adjacent);assert.equal(owner.content.length,7);
        assert.throws(()=>owner.encodeXml(limits({maxFragments:6})));
        assert(owner.encodeXml().includes('head&amp;😀adjacent<'));
        assert.throws(()=>Model.JsMixedParagraph.decodeXml(document.replace('f:em','f:unknown')));
        assert.throws(()=>blank.encodeXml(),/standalone XML/);
        factory.set(blank,[999,'bad']);assert.throws(()=>owner.encodeXml());
        for(const values of [{maxFragments:-1},{maxTextBytes:0.5}])assert.throws(()=>new Model.XmlMixedLimits(values));
        const text='é😀'.repeat(4096),large='<paragraph xmlns="urn:paragraph" id="p">'+text+'</paragraph>';
        const exact=limits({maxFragments:1,maxTextBytes:new TextEncoder().encode(text).length});
        const whole=Model.JsMixedParagraph.decodeXml(large,exact),split=Model.JsMixedParagraph.decodeXmlChunks(chunks(large,1),exact);
        assert.deepEqual(whole.encode(Model.Protocol.BINARY_NONE),split.encode(Model.Protocol.BINARY_NONE));
        ''')

    def test_helper_misuse_preflight_preserves_existing_outputs(self):
        """Reject ordinary/map/variant uses of an implicit mixed helper before output publication."""
        for label, field, mapping in [('ordinary','fragment',''),('map','map<string,fragment>','map.payload=entry\nwrapper.payload=payload\n'),
                                      ('variant','variant(string = text, fragment = helper)','variant.payload=element\nwrapper.payload=payload\n')]:
            source, config, output = self.directory/(label+'.serializer'),self.directory/(label+'.ini'),self.directory/(label+'.mjs')
            source.write_text(SCHEMA.rsplit('}',1)[0]+'class holder { public '+field+' payload(1); }}',encoding='utf-8')
            config.write_text(CONFIG+'[xml.class.js_mixed::holder]\n'+mapping,encoding='utf-8')
            output.write_text('preserved\n',encoding='utf-8')
            result = subprocess.run([str(self.compiler),'--language','js','--input',str(source),'--config',str(config),
                                     '--js.output',str(output)],capture_output=True,text=True,encoding='utf-8')
            self.assertNotEqual(result.returncode,0)
            self.assertIn('mixed fragment storage',result.stderr)
            self.assertEqual(output.read_text(encoding='utf-8'),'preserved\n')

    def test_actual_typescript_native_fragment_arrays_and_policy(self):
        """Compile a real strict consumer while keeping private native fragment storage private."""
        source=self.directory/'typed.mts'
        source.write_text("import {JsMixedParagraph,JsMixedFragment,XmlLimits,XmlMixedLimits} from './mixed.mjs';\n"
                          "const owner=JsMixedParagraph.decodeXmlChunks([new TextEncoder().encode("+json.dumps(DOCUMENT)+")],"
                          "new XmlLimits({mixed:new XmlMixedLimits({maxFragments:5,maxTextBytes:16})}));\n"
                          "const fragments:JsMixedFragment[]=owner.content;if(fragments.length!==5)throw new Error('native fragment array');\n"
                          "// @ts-expect-error mixed XML retains the private native variant field\n"
                          "const unavailable=()=>fragments[0].value;\nJsMixedParagraph.decodeXml(owner.encodeXml());\n",encoding='utf-8')
        subprocess.run(['node',str(self.typescript),'--strict','--target','ES2020','--module','NodeNext',
                        '--moduleResolution','NodeNext','--skipLibCheck','false',str(source)],check=True)
        subprocess.run(['node',str(source.with_suffix('.mjs'))],check=True)


def main():
    """Generate real XML/native controls, require exact installed dependencies and execute actual SDK tests."""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',required=True,type=Path)
    parser.add_argument('--build',required=True,type=Path)
    parser.add_argument('--node-dependencies',type=Path)
    args=parser.parse_args()
    build=args.build.resolve()
    if build==ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository scratch must remain under ignored out/')
    build.mkdir(parents=True,exist_ok=True)
    modules=build/'node_modules'
    if args.node_dependencies:
        if not modules.is_dir():shutil.copytree(args.node_dependencies.resolve(),modules)
    else:
        for filename in ('package.json','package-lock.json'):
            shutil.copyfile(ROOT/'test/resources/xml_shapes/typescript'/filename,build/filename)
        subprocess.run(['npm.cmd' if sys.platform=='win32' else 'npm','ci','--ignore-scripts','--no-audit','--no-fund'],cwd=build,check=True)
    for package,version in [('saxes','6.0.0'),('xmlchars','2.2.0'),('typescript','5.9.3'),('@xmldom/xmldom','0.9.12')]:
        if json.loads((modules/package/'package.json').read_text(encoding='utf-8'))['version']!=version:
            raise RuntimeError('Actual Node SDK dependency does not match exact lock: '+package)
    with tempfile.TemporaryDirectory(prefix='javascript-xml-mixed-',dir=build) as temporary:
        directory=Path(temporary);source=directory/'mixed.serializer';config=directory/'mixed.ini'
        source.write_text(SCHEMA,encoding='utf-8');config.write_text(CONFIG,encoding='utf-8')
        for filename,extra in [('mixed',['--config',str(config)]),('native',['--xml.enabled','false'])]:
            subprocess.run([str(args.compiler.resolve()),'--language','js,typescript','--input',str(source),*extra,
                            '--js.output',str(directory/(filename+'.mjs')),'--typescript.output',str(directory/(filename+'.d.mts'))],check=True)
        javascript_xml_mixed_test.directory,javascript_xml_mixed_test.compiler=directory,args.compiler.resolve()
        javascript_xml_mixed_test.typescript=modules/'typescript/bin/tsc'
        result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(javascript_xml_mixed_test))
        if not result.wasSuccessful():raise SystemExit(1)


if __name__=='__main__':
    main()
