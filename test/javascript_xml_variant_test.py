# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated JS/TS XML variant selectors, QName inventories and native storage compatibility."""
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
XSI = 'http://www.w3.org/2001/XMLSchema-instance'


class javascript_xml_variant_test(unittest.TestCase):
    """Exercise complete tag/type selection through public streaming provider APIs only."""
    def run_node(self, body):
        """Execute a real generated-module probe while disabling every DOM decoder entry point."""
        source = self.directory / 'variant-probe.mjs'
        source.write_text("import assert from 'node:assert/strict';\nimport * as Model from './variants.mjs';\n"
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

    def test_tagged_legacy_private_storage_and_native_controls(self):
        """Keep owning discriminated unions and legacy selector/member APIs exactly as generated today."""
        self.run_node('''
        const cases=[['number','18446744073709551615','code',18446744073709551615n],['text','A&amp;B','text','A&B'],
          ['record','<amount>-12</amount>','record',-12]];
        for(const [tag,content,alias,expected] of cases){
          const document='<element_choice><choice><'+tag+'>'+content+'</'+tag+'></choice></element_choice>';
          for(const size of [1,2,7,document.length]){
            const owner=Model.JsVariantsElementChoice.decodeXmlChunks(chunks(document,size));
            assert.equal(owner.payload.kind,alias);assert.equal(alias==='record' ? owner.payload.value.amount : owner.payload.value,expected);
            for(const protocol of Object.values(Model.Protocol)){
              const bytes=owner.encode(protocol);assert.deepEqual(Native.JsVariantsElementChoice.decode(bytes,protocol).encode(protocol),bytes);
              assert.deepEqual(Model.JsVariantsElementChoice.decodeXml(owner.encodeXml()).encode(protocol),bytes);
            }
          }
        }
        const legacy=Model.JsVariantsLegacyChoice.decodeXml('<legacy_choice><choice><number>7</number></choice></legacy_choice>');
        assert.equal(legacy.payloadIndex,0);assert.equal(legacy.payloadCode,7n);legacy.payloadIndex=2;legacy.payloadRecord.amount=-12;
        assert.equal(Model.JsVariantsLegacyChoice.decodeXml(legacy.encodeXml()).payloadRecord.amount,-12);
        const privateValue=Model.JsVariantsPrivateChoice.decodeXml('<private_choice><choice><number>7</number></choice></private_choice>');
        assert.doesNotThrow(()=>Model.JsVariantsPrivateChoice.decodeXml(privateValue.encodeXml()));
        const privateLegacy=Model.JsVariantsPrivateLegacyChoice.decodeXml('<private_legacy_choice><choice><number>7</number></choice></private_legacy_choice>');
        assert.doesNotThrow(()=>Model.JsVariantsPrivateLegacyChoice.decodeXml(privateLegacy.encodeXml()));
        const invalid=new Model.JsVariantsElementChoice();invalid.payload={kind:'missing',value:1};assert.throws(()=>invalid.encodeXml());
        invalid.payload=null;assert.throws(()=>invalid.encodeXml());legacy.payloadIndex=99;assert.throws(()=>legacy.encodeXml());
        ''')

    def test_exact_element_and_xsi_type_inventory_agreement(self):
        """Reject missing, duplicate, unknown, conflicting and nil payload metadata before publication."""
        self.run_node('''
        const start='<typed_choice xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns:t="http://www.w3.org/2001/XMLSchema"><choice>';
        const document=start+'<choice i:type="t:unsignedLong">7</choice></choice></typed_choice>';
        for(const size of [1,2,7,document.length]){
          const owner=Model.JsVariantsTypedChoice.decodeXmlChunks(chunks(document,size));assert.equal(owner.payload.kind,'code');assert.equal(owner.payload.value,7n);
          assert.equal(Model.JsVariantsTypedChoice.decodeXml(owner.encodeXml()).payload.value,7n);
          owner.payload={kind:'record',value:new Model.JsVariantsPayload()};owner.payload.value.amount=-12;
          assert.equal(Model.JsVariantsTypedChoice.decodeXml(owner.encodeXml()).payload.value.amount,-12);
        }
        for(const payload of ['', '<choice>7</choice>', '<number i:type="t:unsignedLong">7</number>',
          '<choice i:type="unknown:unsignedLong">7</choice>','<choice i:type="t:missing">7</choice>',
          '<choice i:type="t:unsignedLong" i:nil="false">7</choice>',
          '<choice i:type="t:unsignedLong">7</choice><choice i:type="t:string">x</choice>',
          '<choice i:type="t:unsignedLong" extra="1">7</choice>', '<choice i:type="a:b:c">7</choice>',
          '<choice i:type=":unsignedLong">7</choice>', '<choice i:type="t:7name">7</choice>']){
          assert.throws(()=>Model.JsVariantsTypedChoice.decodeXmlChunks(chunks(start+payload+'</choice></typed_choice>',1)));
        }
        const prefix='<element_choice xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns:t="http://www.w3.org/2001/XMLSchema"><choice>';
        assert.equal(Model.JsVariantsElementChoice.decodeXml(prefix+'<number i:type="t:unsignedLong">7</number></choice></element_choice>').payload.value,7n);
        for(const payload of ['', '<missing>7</missing>', '<number i:type="t:string">7</number>',
          '<number i:nil="true"/>','<number>7</number><text>x</text>'])assert.throws(()=>Model.JsVariantsElementChoice.decodeXml(prefix+payload+'</choice></element_choice>'));
        ''')

    def test_unicode_scoped_qnames_and_empty_type_namespace_output(self):
        """Resolve shadowed Unicode prefixes and preserve empty type namespaces without changing payload names."""
        output = self.run_node('''
        const document='<unicode_choice xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns:π="urn:wrong"><choice>'+
          '<choice xmlns:π="urn:types" i:type="π:Record"><amount>-12</amount></choice></choice></unicode_choice>';
        const owner=Model.JsVariantsUnicodeChoice.decodeXmlChunks(chunks(document,1));assert.equal(owner.payload.kind,'record');assert.equal(owner.payload.value.amount,-12);
        assert.equal(Model.JsVariantsUnicodeChoice.decodeXml(owner.encodeXml()).payload.value.amount,-12);
        assert.throws(()=>Model.JsVariantsUnicodeChoice.decodeXml(document.replace('xmlns:π="urn:types"','xmlns:π="urn:bad"')));
        const empty='<e:empty_namespace_choice xmlns:e="urn:wrapper" xmlns:i="http://www.w3.org/2001/XMLSchema-instance">'+
          '<e:choice><e:choice i:type="Code">7</e:choice></e:choice></e:empty_namespace_choice>';
        const value=Model.JsVariantsEmptyNamespaceChoice.decodeXmlChunks(chunks(empty,1));assert.equal(value.payload.value,7n);
        const encoded=value.encodeXml();assert.equal(Model.JsVariantsEmptyNamespaceChoice.decodeXml(encoded).payload.value,7n);
        console.log(encoded);
        ''')
        root = etree.fromstring(output)
        self.assertEqual(root.tag, '{urn:wrapper}empty_namespace_choice')
        wrapper = root[0]
        self.assertEqual(wrapper.tag, '{urn:wrapper}choice')
        self.assertEqual(wrapper[0].tag, '{urn:wrapper}choice')
        self.assertEqual(wrapper[0].attrib['{' + XSI + '}type'], 'Code')

    def test_actual_typescript_discriminated_and_legacy_contracts(self):
        """Compile and execute an actual strict consumer that narrows owning aliases and edits legacy fields."""
        source = self.directory / 'typed.mts'
        source.write_text("import {JsVariantsElementChoice,JsVariantsLegacyChoice} from './variants.mjs';\n"
                          "const value=JsVariantsElementChoice.decodeXmlChunks([new TextEncoder().encode(" + json.dumps(
                          '<element_choice><choice><number>7</number></choice></element_choice>') + ")]);\n"
                          "if(value.payload.kind==='code'){const exact:bigint=value.payload.value;if(exact!==7n)throw new Error('typed bigint');}\n"
                          "const legacy=new JsVariantsLegacyChoice();legacy.payloadIndex=0;legacy.payloadCode=7n;\n"
                          "JsVariantsLegacyChoice.decodeXml(legacy.encodeXml());\n", encoding='utf-8')
        subprocess.run(['node', str(self.typescript), '--strict', '--target', 'ES2020', '--module', 'NodeNext',
                        '--moduleResolution', 'NodeNext', '--skipLibCheck', 'false', str(source)], check=True)
        subprocess.run(['node', str(source.with_suffix('.mjs'))], check=True)


def main():
    """Generate real JS/declaration/native-control artifacts and qualify exact installed provider versions."""
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
    with tempfile.TemporaryDirectory(prefix='javascript-xml-variants-', dir=build) as temporary:
        directory = Path(temporary)
        source, config = directory / 'variants.serializer', directory / 'variants.ini'
        declarations = ['class payload { public int16 amount(1); }']
        classes = ['element_choice', 'typed_choice', 'legacy_choice', 'private_choice', 'private_legacy_choice', 'unicode_choice', 'empty_namespace_choice']
        for name in classes:
            declarations.append('class ' + name + ' { ' + ('private' if name in ('private_choice', 'private_legacy_choice') else 'public') + ' '
                                + ('union' if name in ('legacy_choice', 'private_legacy_choice') else 'variant')
                                + '(uint64=code,string=text,payload=record) payload(1); }')
        source.write_text('serializer version 1.20.0; namespace js_variants {' + ''.join(declarations) + '}', encoding='utf-8')
        settings = ['[xml]', 'enabled=true']
        for name in classes:
            settings += ['[xml.class.js_variants::' + name + ']', 'wrapper.payload=choice']
            if name in ('typed_choice', 'unicode_choice', 'empty_namespace_choice'):
                settings += ['variant.payload=xsi_type']
                tags = ('{http://www.w3.org/2001/XMLSchema}unsignedLong', '{http://www.w3.org/2001/XMLSchema}string', '{urn:types}Payload')
                if name == 'unicode_choice':
                    tags = ('{urn:types}Code', '{urn:types}Text', '{urn:types}Record')
                if name == 'empty_namespace_choice':
                    settings += ['namespace=urn:wrapper']
                    tags = ('{}Code', '{}Text', '{}Payload')
                settings += ['variant.payload.tag.' + alias + '=' + tag for alias, tag in zip(('code', 'text', 'record'), tags)]
            else:
                settings += ['variant.payload=element', 'variant.payload.tag.code=number']
        config.write_text('\n'.join(settings) + '\n', encoding='utf-8')
        subprocess.run([str(args.compiler.resolve()), '--language', 'js,typescript', '--input', str(source),
                        '--config', str(config), '--js.output', str(directory / 'variants.mjs'),
                        '--typescript.output', str(directory / 'variants.d.mts')], check=True)
        subprocess.run([str(args.compiler.resolve()), '--language', 'js,typescript', '--input', str(source),
                        '--xml.enabled', 'false', '--js.output', str(directory / 'native.mjs'),
                        '--typescript.output', str(directory / 'native.d.mts')], check=True)
        javascript_xml_variant_test.directory, javascript_xml_variant_test.typescript = directory, modules / 'typescript/bin/tsc'
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(javascript_xml_variant_test))
        if not result.wasSuccessful():
            raise SystemExit(1)


if __name__ == '__main__':
    main()
