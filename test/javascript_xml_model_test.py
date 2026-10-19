# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated JS/TS XML inheritance while preserving native composed base storage."""
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
DOCUMENT = '''<derived xmlns="urn:derived" token="18446744073709551615" label="base">
<ancestor>-12</ancestor><flag>false</flag><count>7</count></derived>'''


class javascript_xml_model_test(unittest.TestCase):
    """Use real generated native private base objects with all DOM decoding entry points disabled."""
    def run_node(self, body):
        """Execute one generated-module probe and preserve exact UTF-8 diagnostics and output."""
        source = self.directory / 'model-probe.mjs'
        source.write_text("import assert from 'node:assert/strict';\nimport * as Model from './model.mjs';\n"
                          "import * as Native from './native.mjs';\nimport {DOMParser} from '@xmldom/xmldom';\n"
                          "DOMParser.prototype.parseFromString=()=>{throw new Error('DOM forbidden');};\n"
                          "for(const item of Object.values(Model))if(typeof item==='function'&&'_srlXmlRead' in item)"
                          "item._srlXmlRead=()=>{throw new Error('DOM model reader forbidden');};\n"
                          "const document=" + json.dumps(DOCUMENT) + ";\n"
                          "const chunks=(value,size)=>{const bytes=new TextEncoder().encode(value);"
                          "return Array.from({length:Math.ceil(bytes.length/size)},(_,i)=>bytes.subarray(i*size,(i+1)*size));};\n"
                          + body, encoding='utf-8')
        result = subprocess.run(['node', str(source)], capture_output=True, text=True, encoding='utf-8')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout

    def test_private_deep_multiple_bases_and_native_wire_controls(self):
        """Reach private grandparent/parent/second-base fields through their own lexical native callbacks."""
        output = self.run_node('''
        let owner;
        for(const size of [1,2,7,document.length]){
          owner=Model.JsModelDerived.decodeXmlChunks(chunks(document,size));assert.equal(owner.count,7);
          const xml=owner.encodeXml();
          for(const protocol of Object.values(Model.Protocol)){
            const bytes=owner.encode(protocol);
            assert.deepEqual(Native.JsModelDerived.decode(bytes,protocol).encode(protocol),bytes);
            assert.deepEqual(Model.JsModelDerived.decodeXml(xml).encode(protocol),bytes);
          }
        }
        assert.equal(Object.hasOwn(owner,'ancestor'),false);assert.equal(Object.hasOwn(owner,'label'),false);
        console.log(owner.encodeXml());
        ''')
        root = etree.fromstring(output)
        self.assertEqual(root.attrib, {'token': '18446744073709551615', 'label': 'base'})
        self.assertEqual([(child.tag, child.text) for child in root], [
            ('{urn:derived}ancestor', '-12'), ('{urn:derived}flag', 'false'), ('{urn:derived}count', '7')])

    def test_closed_ordered_base_fields_and_namespace_ownership(self):
        """Reject wrong namespace/order/duplicate/missing base fields while retaining false and zero values."""
        self.run_node('''
        for(const invalid of [document.replace('<ancestor>-12</ancestor>',''),
          document.replace('<ancestor>-12</ancestor>','<ancestor>-12</ancestor><ancestor>7</ancestor>'),
          document.replace('<flag>false</flag><count>7</count>','<count>7</count><flag>false</flag>'),
          document.replace('urn:derived','urn:wrong'),document.replace('token="18446744073709551615"',''),
          document.replace('<ancestor>-12</ancestor>','<ancestor>32768</ancestor>'),
          document.replace('<flag>false</flag>','<flag>unknown</flag>')])assert.throws(()=>Model.JsModelDerived.decodeXmlChunks(chunks(invalid,1)));
        const owner=Model.JsModelDerived.decodeXml(document.replace('-12','0').replace('count>7','count>0'));
        assert.equal(owner.count,0);assert.doesNotThrow(()=>Model.JsModelDerived.decodeXml(owner.encodeXml()));
        ''')

    def test_inherited_collision_preflight_preserves_existing_artifacts(self):
        """Reject ambiguous flattened native field ownership before publication and retain native-only composition."""
        source = self.directory / 'collision.serializer'
        source.write_text('''serializer version 1.20.0;
          class left { public uint32 value(1); } class right { public string value(1); }
          class derived : public left ("left",1), public right ("right",2) {}''', encoding='utf-8')
        output = self.directory / 'collision.mjs'
        output.write_text('preserve existing output\n', encoding='utf-8')
        command = [str(self.compiler), '--language', 'js', '--input', str(source), '--js.output', str(output)]
        result = subprocess.run([*command, '--xml.enabled', 'true'], capture_output=True, text=True, encoding='utf-8')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(output.read_text(encoding='utf-8'), 'preserve existing output\n')
        subprocess.run([*command, '--xml.enabled', 'false'], check=True)

    def test_actual_typescript_keeps_public_base_composition(self):
        """Compile a consumer of base0 storage and require flattened native member access to stay unavailable."""
        source = self.directory / 'typed.mts'
        source.write_text("import {JsModelPublicDerived} from './model.mjs';\n"
                          "const owner=JsModelPublicDerived.decodeXmlChunks([new TextEncoder().encode(" + json.dumps(
                          '<public_derived><value>-7</value><own>12</own></public_derived>') + ")]);\n"
                          "const baseValue:number=owner.base0.value;if(baseValue!==-7||owner.own!==12)throw new Error('native composed base');\n"
                          "// @ts-expect-error XML flattening preserves the native base0 API\n"
                          "const unavailable=()=>owner.value;\n"
                          "JsModelPublicDerived.decodeXml(owner.encodeXml());\n", encoding='utf-8')
        subprocess.run(['node', str(self.typescript), '--strict', '--target', 'ES2020', '--module', 'NodeNext',
                        '--moduleResolution', 'NodeNext', '--skipLibCheck', 'false', str(source)], check=True)
        subprocess.run(['node', str(source.with_suffix('.mjs'))], check=True)


def main():
    """Generate XML/native controls with the real compiler and run exact installed Node/TS dependencies."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--node-dependencies', type=Path)
    args = parser.parse_args()
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
            raise RuntimeError('Actual Node SDK dependency does not match its exact lock: ' + package)
    with tempfile.TemporaryDirectory(prefix='javascript-xml-model-', dir=build) as temporary:
        directory = Path(temporary)
        source, config = directory / 'model.serializer', directory / 'model.ini'
        source.write_text('''serializer version 1.20.0; namespace js_model {
          class grand { private int16 ancestor(1); public uint64 token(2) xml(attribute); }
          class left : private grand ("grand",1) { private string label(3) xml(attribute); }
          class right { private bool flag(1); }
          class derived : private left ("left",1), public right ("right",2) { public uint32 count(3); }
          class public_base { public int32 value(1); }
          class public_derived : public public_base ("base",1) { public uint32 own(2); }
        }''', encoding='utf-8')
        config.write_text('[xml]\nenabled=true\n[xml.class.js_model::derived]\nnamespace=urn:derived\n', encoding='utf-8')
        subprocess.run([str(args.compiler.resolve()), '--language', 'js,typescript', '--input', str(source),
                        '--config', str(config), '--js.output', str(directory / 'model.mjs'),
                        '--typescript.output', str(directory / 'model.d.mts')], check=True)
        subprocess.run([str(args.compiler.resolve()), '--language', 'js,typescript', '--input', str(source),
                        '--xml.enabled', 'false', '--js.output', str(directory / 'native.mjs'),
                        '--typescript.output', str(directory / 'native.d.mts')], check=True)
        javascript_xml_model_test.directory, javascript_xml_model_test.compiler = directory, args.compiler.resolve()
        javascript_xml_model_test.typescript = modules / 'typescript/bin/tsc'
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(javascript_xml_model_test))
        if not result.wasSuccessful():
            raise SystemExit(1)


if __name__ == '__main__':
    main()
