# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify actual generated JS/TS ordinary HTML against independent HTML parsers."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

from html_interop_test import trace, tree_trace

ROOT=Path(__file__).resolve().parent.parent
FIXTURES=[
    '<P>H<SUB title="index">2</SUB>O; x<SUP>2<em>nd</em></SUP> &amp; y<sub><sup>n</sup></sub></P>',
    '<DIV hidden class=card><P>A&nbsp;&amp; B<br><IMG src="x.png" alt=picture></P></DIV>',
    '<section><h1 title="A &quot;quote&quot;">Title</h1><p>Unicode: λ 🙂</p></section>',
    '<ul>\n<li>first <b>bold</b></li><li><a href="a?x=1&amp;y=2">link</a></li></ul>',
    'prefix<!-- discarded --><span hidden=hidden title="&#x1F642;">x\r\ny&#13;&lt;</span>suffix',
    '<article><hr/><p><em>one</em><br />two</p><blockquote>  literal\nspace</blockquote></article>',
    '<img src=x/ alt="a &gt; b">',
]


class javascript_html_test(unittest.TestCase):
    """Keep HTML syntax and semantics independent of every XML DOM and token provider."""
    def run_node(self,body):
        """Run an actual generated module after disabling its available XML parsing entry points."""
        source=self.directory/'probe.mjs'
        source.write_text("import assert from 'node:assert/strict';\nimport {Html,HtmlLimits,HtmlEvent,HtmlAttribute} from './html.mjs';\n"
                          "import {DOMParser} from '@xmldom/xmldom';import {SaxesParser} from 'saxes';\n"
                          "DOMParser.prototype.parseFromString=()=>{throw new Error('XML DOM forbidden');};\n"
                          "SaxesParser.prototype.write=()=>{throw new Error('XML token parser forbidden');};\n"
                          "const utf8=value=>new TextEncoder().encode(value);\n"+body,encoding='utf-8')
        result=subprocess.run(['node',str(source)],capture_output=True,text=True,encoding='utf-8')
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        return result.stdout

    def test_independent_html_traces_and_canonical_replay(self):
        """Compare actual ordinary HTML with separate token and fragment-tree implementations."""
        outputs=json.loads(self.run_node('const fixtures='+json.dumps(FIXTURES)+''';
          const outputs=fixtures.map(source=>Html.writeFragment(Html.parseFragment(utf8(source))));
          for(const output of outputs)assert.equal(Html.writeFragment(Html.parseFragment(utf8(output))),output);
          assert(Html.supportedSubset().includes('no doctype'));console.log(JSON.stringify(outputs));
        '''))
        for source,output in zip(FIXTURES,outputs):
            self.assertEqual(trace(source),trace(output))
            self.assertEqual(tree_trace(source),tree_trace(output))

    def test_exact_resource_limits_and_authored_events(self):
        """Admit exact byte/text/event/depth/attribute limits and reject each one-unit-short policy."""
        self.run_node(r'''
        const source='<DIV hidden title="x">é&amp;😀<br></DIV>',events=Html.parseFragment(utf8(source));
        const output=Html.writeFragment(events),limits={maxInputBytes:utf8(source).length,maxOutputBytes:utf8(output).length,
          maxEvents:events.length,maxDepth:2,maxAttributesPerElement:2,maxTextBytes:7};
        assert.equal(Html.writeFragment(Html.parseFragment(utf8(source),limits),limits),output);
        for(const key of Object.keys(limits)){
          const short={...limits,[key]:limits[key]-1};
          if(key!=='maxOutputBytes')assert.throws(()=>Html.parseFragment(utf8(source),short));
          if(key!=='maxInputBytes')assert.throws(()=>Html.writeFragment(events,short));
        }
        const authored=[new HtmlEvent('start_fragment'),new HtmlEvent('start_element','span',[new HtmlAttribute('hidden','',true)]),
          new HtmlEvent('text','',[],'<&\r"'),new HtmlEvent('end_element','span'),new HtmlEvent('end_fragment')];
        assert.equal(Html.writeFragment(authored),'<span hidden>&lt;&amp;&#13;"</span>');
        assert.throws(()=>new HtmlLimits({maxEvents:-1}));assert.throws(()=>new HtmlLimits({maxDepth:0.5}));
        assert.throws(()=>Html.parseFragment(Uint8Array.from([0xc3,0x28])));
        assert.throws(()=>Html.parseFragment('<p>requires UTF-8 bytes</p>'));
        ''' )

    def test_closed_subset_rejects_browser_recovery_and_injection(self):
        """Reject foreign/raw-text content, implied closes, malformed entities and unsupported attributes."""
        self.run_node('''
        for(const source of ['<sub><div>x</div></sub>','<sup><p>x</p></sup>','<sub/>','<sup>unclosed',
          '<sub onclick=x>x</sub>',
          '<!DOCTYPE html>','<svg></svg>','<script>x</script>','<p><div>x</div></p>',
          '<ul><span>x</span></ul>','<ul>text</ul>','<li>x</li>','<a><a>x</a></a>','<span/>','<p>unclosed',
          '<br></br>','<p hidden=wrong>x</p>','<p title>x</p>','<p onclick="x">x</p>','<p id=x ID=y>x</p>',
          '<p>&copy;</p>','<p>&amp</p>','<p>&#0;</p>','<p>&#xD800;</p>','<p>&#x110000;</p>',
          '<!--a--b-->','<p title="x"id="y"></p>','<p title=x`y></p>','<?xml version="1.0"?>'])assert.throws(()=>Html.parseFragment(utf8(source)));
        const events=Html.parseFragment(utf8('<p>x</p>'));events[2].text='\\ud800';assert.throws(()=>Html.writeFragment(events));
        const attributes=Html.parseFragment(utf8('<p hidden>x</p>'));attributes[1].attributes[0].boolean=false;
        assert.throws(()=>Html.writeFragment(attributes));
        ''' )

    def test_actual_typescript_html_api(self):
        """Compile and run the public typed fragment adapter with the real TypeScript SDK."""
        source=self.directory/'typed.mts'
        source.write_text("import {Html,HtmlLimits,HtmlEvent,HtmlAttribute} from './html.mjs';\n"
                          "const limits=new HtmlLimits({maxDepth:2});const events:HtmlEvent[]=Html.parseFragment(new TextEncoder().encode('<p hidden>x<br></p>'),limits);\n"
                          "const attribute:HtmlAttribute=events[1].attributes[0];if(!attribute.boolean)throw new Error('Boolean hidden');\n"
                          "const output:string=Html.writeFragment(events,limits);if(output!=='<p hidden>x<br></p>')throw new Error(output);\n",
                          encoding='utf-8')
        subprocess.run(['node',str(self.typescript),'--strict','--target','ES2020','--module','NodeNext',
                        '--moduleResolution','NodeNext','--skipLibCheck','false',str(source)],check=True)
        subprocess.run(['node',str(source.with_suffix('.mjs'))],check=True)


def main():
    """Generate a real opted-in module, require exact Node/TS locks and load independent HTML oracles."""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',required=True,type=Path)
    parser.add_argument('--build',required=True,type=Path)
    parser.add_argument('--node-dependencies',type=Path)
    parser.add_argument('--python-dependencies',type=Path)
    args=parser.parse_args()
    if args.python_dependencies:sys.path.insert(0,str(args.python_dependencies.resolve()))
    from lxml import html as provider
    provider.fragment_fromstring('<p>required independent HTML tree</p>')
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
    with tempfile.TemporaryDirectory(prefix='javascript-html-',dir=build) as temporary:
        directory=Path(temporary);source=directory/'html.serializer'
        source.write_text('serializer version 1.20.0; namespace js_html { class sample { public uint32 value(1); }}',encoding='utf-8')
        subprocess.run([str(args.compiler.resolve()),'--language','js,typescript','--input',str(source),'--xml.enabled','true',
                        '--js.output',str(directory/'html.mjs'),'--typescript.output',str(directory/'html.d.mts')],check=True)
        javascript_html_test.directory=directory;javascript_html_test.typescript=modules/'typescript/bin/tsc'
        result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(javascript_html_test))
        if not result.wasSuccessful():raise SystemExit(1)
        report={'status':'passed','tests':result.testsRun,'fixtures':len(FIXTURES),
                'compiler_sha256':hashlib.sha256(args.compiler.resolve().read_bytes()).hexdigest(),
                'generated_sha256':{path.name:hashlib.sha256(path.read_bytes()).hexdigest()
                                    for path in (directory/'html.mjs',directory/'html.d.mts')},
                'typescript':json.loads((modules/'typescript/package.json').read_text(encoding='utf-8'))['version']}
        (build/'verification.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':
    main()
