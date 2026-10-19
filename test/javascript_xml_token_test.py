# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated JavaScript/TypeScript SAX input with the actual Node and TypeScript SDKs."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import xml.dom.minidom as minidom
import xml.etree.ElementTree as etree

ROOT = Path(__file__).resolve().parent.parent
NS = 'urn:serializer:js-tokens'
DOCUMENT = '''<root xmlns="'''+NS+'''" id="18446744073709551615" flag="0" stage="one" secret="7">
<label>A&amp;B&#13;😀</label><sample>-0</sample><price unit="x&#9;y">-2147483648</price>
<children><child ready="1"><text>first</text></child><child ready="false"><text>second</text></child></children>
<tags><tag>one</tag><tag/></tags></root>'''


def logical_bounds(document):
    """Count normalized XML names/attributes/text through an independent mature parser."""
    nodes, attributes, text, depth = 0, 0, 0, 0
    def size(value):
        """Measure the shared UTF-8 byte quantity without presentation-prefix assumptions."""
        return len(value.encode('utf-8'))
    def visit(node, level):
        """Mirror the documented resource definition using independent semantic DOM events."""
        nonlocal nodes, attributes, text, depth
        if node.nodeType == node.ELEMENT_NODE:
            level += 1
            nodes, depth = nodes + 1, max(depth, level)
            text += size(node.localName) + size(node.namespaceURI or '')
            for attribute in node.attributes.values():
                attributes += 1
                text += size(attribute.value)
                if attribute.namespaceURI == 'http://www.w3.org/2000/xmlns/':
                    text += size(attribute.localName if attribute.prefix == 'xmlns' else '')
                else:
                    text += size(attribute.localName) + size(attribute.namespaceURI or '')
        elif node.nodeType in (node.TEXT_NODE, node.CDATA_SECTION_NODE):
            text += size(node.data)
        for child in node.childNodes:
            visit(child, level)
    visit(minidom.parseString(document), 0)
    return dict(maxBytes=len(document.encode()), maxNodes=nodes, maxAttributes=attributes,
                maxTextBytes=text, maxDepth=depth)


class javascript_xml_token_test(unittest.TestCase):
    """Exercise whole-document and arbitrary-byte input without permitting DOM codec entry points."""
    def run_node(self, body):
        """Execute one independent real generated-module SDK probe and require success."""
        program = self.directory / 'probe.mjs'
        program.write_text("import assert from 'node:assert/strict';\nimport * as Model from './schema.mjs';\n"
                           "import * as Native from './native.mjs';\nimport {DOMParser} from '@xmldom/xmldom';\n"
                           "const document=" + json.dumps(DOCUMENT) + ";\n"
                           "DOMParser.prototype.parseFromString=()=>{throw new Error('DOM parsing forbidden');};\n"
                           "for(const item of Object.values(Model))if(typeof item==='function'&&'_srlXmlRead' in item)"
                           "item._srlXmlRead=()=>{throw new Error('DOM model reader forbidden');};\n" + body,
                           encoding='utf-8')
        result = subprocess.run(['node', str(program)], capture_output=True, text=True, encoding='utf-8')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout

    def test_direct_typed_input_private_fields_and_native_controls(self):
        """Decode through generated private-safe setters and compare exact native bytes with XML disabled."""
        output = self.run_node('''
        const value=Model.JsTokensRoot.decodeXml(document);
        assert.equal(value.id,18446744073709551615n); assert.deepEqual(value.flag,[false]);
        assert.equal(value.label,'A&B\\r😀'); assert(Object.is(value.sample,-0));
        assert.deepEqual(value.children.map(x=>[x.ready,x.text]),[[true,'first'],[false,'second']]);
        assert.equal(value.price.value,-2147483648); assert.equal(value.price.unit,'x\\ty');
        assert.deepEqual(value.tags,['one','']);
        for(const protocol of Object.values(Model.Protocol)) {
          const bytes=value.encode(protocol);
          assert.deepEqual(Native.JsTokensRoot.decode(bytes,protocol).encode(protocol),bytes);
          assert.deepEqual(Model.JsTokensRoot.decodeXml(value.encodeXml()).encode(protocol),bytes);
        }
        console.log(value.encodeXml());
        ''')
        root = etree.fromstring(output)
        self.assertEqual(root.attrib['secret'], '7')
        self.assertEqual(root.find('{'+NS+'}label').text, 'A&B\r😀')
        self.assertEqual(root.find('{'+NS+'}price').attrib['unit'], 'x\ty')

    def test_arbitrary_utf8_chunks_entities_and_exact_eof(self):
        """Split UTF-8/entity/attribute tokens at every small byte boundary and validate final EOF."""
        self.run_node('''
        const bytes=new TextEncoder().encode(document),baseline=Model.JsTokensRoot.decodeXml(document).encode(Model.Protocol.BINARY_NONE);
        for(const size of [1,2,3,7,31,bytes.length]) {
          function* slices(){for(let index=0;index<bytes.length;index+=size)yield bytes.subarray(index,index+size);}
          assert.deepEqual(Model.JsTokensRoot.decodeXmlChunks(slices()).encode(Model.Protocol.BINARY_NONE),baseline);
        }
        const bom=new Uint8Array(bytes.length+3);bom.set([239,187,191]);bom.set(bytes,3);
        assert.deepEqual(Model.JsTokensRoot.decodeXmlChunks([bom]).encode(Model.Protocol.BINARY_NONE),baseline);
        for(const suffix of ['<second/>','x','<!--','<?unfinished'])
          assert.throws(()=>Model.JsTokensRoot.decodeXmlChunks([bytes,new TextEncoder().encode(suffix)]));
        for(const suffix of [[0xc2],[0xf0,0x9f],[0xff],[0xc0,0x80]])
          assert.throws(()=>Model.JsTokensRoot.decodeXmlChunks([bytes,new Uint8Array(suffix)]));
        assert.throws(()=>Model.JsTokensRoot.decodeXmlChunks(['not bytes']));
        const cdata=document.replace('A&amp;B&#13;😀','<![CDATA[<!DOCTYPE harmless>]]>');
        assert.equal(Model.JsTokensRoot.decodeXml(cdata).label,'<!DOCTYPE harmless>');
        ''')

    def test_invalid_xml_values_namespaces_and_closed_membership(self):
        """Reject malformed XML/lexemes, alias duplicates, DTDs and object-prototype-shaped unknown names."""
        self.run_node('''
        const invalid=[
          document.replace('18446744073709551615','18446744073709551616'),
          document.replace('-2147483648','-2147483649'), document.replace('ready="1"','ready="yes"'),
          document.replace('stage="one"','stage="missing"'), document.replace('<price ','<price unexpected="x" '),
          document.replace('<tags>','<tags unexpected="x">'),document.replace('<tag/>','<other/>'),
          document.replace('<sample>','<sample><child/>'),document.replace('id="18446744073709551615"',''),
          document.replace('id="','__proto__="x" id="'),document.replace('<label>','<constructor/><label>'),
          document.replace('urn:serializer:js-tokens','urn:wrong'),
          document.replace('urn:serializer:js-tokens','urn:serializer:js-tokens '),
          document.replace('A&amp;B&#13;😀','&unbound;'), document.replace('A&amp;B&#13;😀','&#0;'),
          '<?xml version="1.1"?>'+document, '<?xml version="1.0" encoding="UTF-16"?>'+document,
          '<!DOCTYPE root SYSTEM "https://example.invalid/not-fetched">'+document,
          '<!DOCTYPE root [<!ENTITY value "injected">]>'+document,
          document.replace('id="','xmlns:a="urn:dup" xmlns:b="urn:dup" a:x="1" b:x="2" id="'),
          document.replace('<label>','<label xmlns:unbound="">')
        ];
        for(const text of invalid)assert.throws(()=>Model.JsTokensRoot.decodeXml(text),undefined,text);
        assert.equal(Object.prototype.polluted,undefined);
        assert.throws(()=>Model.JsTokensRoot.decodeXml(document+'\\ud800'));
        ''')

    def test_resource_equalities_validator_boundary_and_output_limits(self):
        """Check every logical bound against an independent oracle and retain transactional validation."""
        bounds = logical_bounds(DOCUMENT)
        self.run_node('''
        const bounds='''+json.dumps(bounds)+''',bytes=new TextEncoder().encode(document);
        const value=Model.JsTokensRoot.decodeXmlChunks([bytes],new Model.XmlLimits(bounds));
        for(const key of Object.keys(bounds)) {
          const policy={...bounds,[key]:bounds[key]-1};
          assert.throws(()=>Model.JsTokensRoot.decodeXmlChunks([bytes],new Model.XmlLimits(policy)),undefined,key);
          assert.throws(()=>Model.JsTokensRoot.decodeXml(document,new Model.XmlLimits(policy)),undefined,key);
        }
        const output=value.encodeXml(),size=new TextEncoder().encode(output).length;
        assert.equal(value.encodeXml(new Model.XmlLimits({maxBytes:size})),output);
        assert.throws(()=>value.encodeXml(new Model.XmlLimits({maxBytes:size-1})));
        let calls=0;const validator={validate(text){assert.equal(typeof text,'string');++calls;}};
        Model.JsTokensRoot.decodeXml(document,undefined,validator);assert.equal(calls,1);
        const marker=new Error('validator marker');
        assert.throws(()=>Model.JsTokensRoot.decodeXml(document,undefined,{validate(){throw marker;}}),error=>error===marker);
        assert.throws(()=>Model.JsTokensRoot.decodeXmlChunks([bytes],undefined,validator));
        ''')

    def test_actual_typescript_declarations(self):
        """Compile an actual strict TypeScript consumer with byte-chunk and bigint/optional model types."""
        source = self.directory / 'typed.mts'
        source.write_text('''import {JsTokensRoot,XmlLimits,Protocol} from './schema.mjs';
          const bytes:Uint8Array=new TextEncoder().encode('''+json.dumps(DOCUMENT)+''');
          const value:JsTokensRoot=JsTokensRoot.decodeXmlChunks([bytes],new XmlLimits());
          const exact:bigint=value.id; const flag:[]|[boolean]=value.flag;
          const result:Uint8Array=value.encode(Protocol.BINARY_NONE);
          if(exact!==18446744073709551615n||flag[0]!==false||!result.length)throw new Error('typed model mismatch');
          // @ts-expect-error XML chunks are bytes, never arbitrary strings.
          const invalid:()=>JsTokensRoot=()=>JsTokensRoot.decodeXmlChunks(['wrong']);
          void invalid;
        ''', encoding='utf-8')
        subprocess.run(['node', str(self.typescript), '--strict', '--target', 'ES2020', '--module', 'NodeNext',
                        '--moduleResolution', 'NodeNext', str(source)], check=True)
        subprocess.run(['node', str(source.with_suffix('.mjs'))], check=True)

    def test_generated_method_collisions_preserve_outputs(self):
        """Reject new XML API collisions while leaving XML-disabled native generation unchanged."""
        source = self.directory / 'collision.serializer'
        output = self.directory / 'collision.mjs'
        command = [str(self.compiler), '--input', str(source), '--language', 'js', '--js.output', str(output)]
        for identifier in ('decodeXmlChunks', 'xmlBindNamespace'):
            source.write_text('serializer version 1.20.0; class sample { public uint32 '+identifier+'(1); }', encoding='utf-8')
            output.write_text('preserve existing output\n', encoding='utf-8')
            result = subprocess.run([*command, '--xml.enabled', 'true'], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('collision', result.stderr)
            self.assertEqual(output.read_text(encoding='utf-8'), 'preserve existing output\n')
            subprocess.run([*command, '--xml.enabled', 'false'], check=True)
            self.assertNotIn('saxes', output.read_text(encoding='utf-8'))
        for identifier in ('XmlExtensionLimits','XmlExtensionOptions','Html','HtmlLimits','HtmlEvent','HtmlAttribute','XmlSchema','Set','WeakMap','Symbol','Promise'):
            source.write_text('serializer version 1.20.0; class '+identifier+' { public uint32 value(1); }',encoding='utf-8')
            output.write_text('preserve existing output\n',encoding='utf-8')
            result=subprocess.run([*command,'--xml.enabled','true'],capture_output=True,text=True)
            self.assertNotEqual(result.returncode,0,identifier)
            self.assertEqual(output.read_text(encoding='utf-8'),'preserve existing output\n')
            subprocess.run([*command,'--xml.enabled','false'],check=True)


def main():
    """Generate real JS/declaration artifacts, install exact locked SDKs and execute all target probes."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
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
    with tempfile.TemporaryDirectory(prefix='javascript-xml-tokens-', dir=build) as temporary:
        directory = Path(temporary)
        source, config = directory / 'tokens.serializer', directory / 'tokens.ini'
        source.write_text('''serializer version 1.20.0; namespace js_tokens {
          enum state { zero, one }
          class amount { public string unit(1) xml(attribute); public int32 value(2) xml(text); }
          class child { public bool ready(1) xml(attribute); public string text(2); }
          class root { public uint64 id(1) xml(attribute); public string label(2); public float sample(3);
            public amount price(4); public array<child> children(5); public array<string> tags(6);
            public optional<bool> flag(7) xml(attribute); public state stage(8) xml(attribute); private uint32 secret(9) xml(attribute); }}''', encoding='utf-8')
        config.write_text('[xml]\nenabled=true\nruntime_xsd=true\n'
                          '[xml.class.js_tokens::root]\nnamespace='+NS+'\nwrapper.children=children\nname.children=child\nwrapper.tags=tags\nname.tags=tag\n'
                          '[xml.class.js_tokens::amount]\nnamespace='+NS+'\n[xml.class.js_tokens::child]\nnamespace='+NS+'\n', encoding='utf-8')
        command = [str(args.compiler.resolve()), '--input', str(source), '--config', str(config), '--language', 'js,typescript']
        subprocess.run([*command, '--js.output', str(directory / 'schema.mjs'),
                        '--typescript.output', str(directory / 'schema.d.mts')], check=True)
        subprocess.run([str(args.compiler.resolve()), '--input', str(source), '--language', 'js,typescript',
                        '--xml.enabled', 'false', '--js.output', str(directory / 'native.mjs'),
                        '--typescript.output', str(directory / 'native.d.mts')], check=True)
        if 'saxes' in (directory / 'native.mjs').read_text(encoding='utf-8'):
            raise RuntimeError('XML-disabled native source acquired an XML provider import')
        javascript_xml_token_test.directory, javascript_xml_token_test.compiler = directory, args.compiler.resolve()
        javascript_xml_token_test.typescript = modules / 'typescript/bin/tsc'
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(javascript_xml_token_test))
        if not result.wasSuccessful():
            raise SystemExit(1)


if __name__ == '__main__':
    main()
