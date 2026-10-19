# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated JS/TS explicit presence, singleton choices and unordered all groups."""
import argparse
import itertools
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


class javascript_xml_content_test(unittest.TestCase):
    """Exercise direct SAX paths with the DOM reader disabled on every generated model."""
    def run_node(self, body):
        """Execute real generated modules and report the exact target exception on failure."""
        source = self.directory / 'content-probe.mjs'
        source.write_text("import assert from 'node:assert/strict';\n"
                          "import * as Presence from './presence.mjs';\n"
                          "import * as Xsd from './presence_xsd.mjs';\n"
                          "import * as All from './permutations.mjs';\n"
                          "import * as Choice from './alternatives.mjs';\n"
                          "import {DOMParser} from '@xmldom/xmldom';\n"
                          "DOMParser.prototype.parseFromString=()=>{throw new Error('DOM forbidden');};\n"
                          "for(const module of [Presence,Xsd,All,Choice])for(const item of Object.values(module))"
                          "if(typeof item==='function'&&'_srlXmlRead' in item)"
                          "item._srlXmlRead=()=>{throw new Error('DOM reader forbidden');};\n"
                          "const chunks=(value,size)=>{const bytes=new TextEncoder().encode(value);"
                          "return Array.from({length:Math.ceil(bytes.length/size)},(_,i)=>bytes.subarray(i*size,(i+1)*size));};\n"
                          + body, encoding='utf-8')
        result = subprocess.run(['node', str(source)], capture_output=True, text=True, encoding='utf-8')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout

    def test_presence_native_states_and_independent_output(self):
        """Keep absence, explicit nil, empty text and empty collections distinct in native arrays."""
        output = self.run_node('''
        const start='<root xmlns:x="http://www.w3.org/2001/XMLSchema-instance">';
        const labels=[['',[]],['<label x:nil="true"/>',[[]]],['<label/>',[[""]]],['<label>text</label>',[["text"]]]];
        const collections=[['',[]],['<items x:nil="1"/>',[[]]],['<items/>',[[[]]]],['<items><item/></items>',[[[""]]]]];
        const documents=[];
        for(const [label,expectedLabel] of labels)for(const [items,expectedItems] of collections){
          const document=start+label+'<mandatory x:nil="true"/>'+items+'</root>';
          for(const size of [1,2,7,document.length]){
            const value=Presence.PresenceRoot.decodeXmlChunks(chunks(document,size));
            assert.deepEqual(value.label,expectedLabel);assert.deepEqual(value.mandatory,[]);assert.deepEqual(value.items,expectedItems);
            const encoded=value.encodeXml();documents.push(encoded);
            const again=Presence.PresenceRoot.decodeXml(encoded);
            assert.deepEqual([again.label,again.mandatory,again.items],[expectedLabel,[],expectedItems]);
            for(const protocol of Object.values(Presence.Protocol))assert.deepEqual(
              Presence.PresenceRoot.decode(value.encode(protocol),protocol).encode(protocol),value.encode(protocol));
          }
        }
        console.log(JSON.stringify(documents));
        ''')
        for document in json.loads(output):
            root = etree.fromstring(document)
            mandatory = root.find('mandatory')
            self.assertIsNotNone(mandatory)
            self.assertEqual(mandatory.attrib, {'{' + XSI + '}nil': 'true'})
            for node in root:
                if node.attrib.get('{' + XSI + '}nil') == 'true':
                    self.assertIsNone(node.text)
                    self.assertEqual(len(node), 0)

    def test_nil_record_payload_false_marker_and_invalid_content(self):
        """Validate every nil marker before constructing or accepting its scalar/record payload."""
        self.run_node('''
        const start='<root xmlns:x="http://www.w3.org/2001/XMLSchema-instance">';
        const value=Presence.PresenceRoot.decodeXml(start+'<mandatory x:nil="0"/></root>');
        assert.deepEqual(value.mandatory,['']);
        const record=Presence.PresenceRecordRoot.decodeXml('<record_root xmlns:q="http://www.w3.org/2001/XMLSchema-instance"><nested q:nil="1"/></record_root>');
        assert.deepEqual(record.nested,[]);assert.deepEqual(Presence.PresenceRecordRoot.decodeXml(record.encodeXml()).nested,[]);
        for(const body of ['', '<mandatory x:nil="true"> </mandatory>', '<mandatory x:nil="other"/>',
          '<mandatory x:nil="true"><nested/></mandatory>', '<mandatory/><items><item x:nil="true"/></items>']) {
          assert.throws(()=>Presence.PresenceRoot.decodeXmlChunks(chunks(start+body+'</root>',1)));
        }
        for(const native of [null,[[],[]],[null],[[null]],[[[],[]]]]) {
          const owner=new Presence.PresenceRoot();owner.label=native;assert.throws(()=>owner.encodeXml());
        }
        assert.throws(()=>Presence.PresenceRoot.decodeXml('<root xmlns:z="foreign"><mandatory z:nil="true"/></root>'));
        ''')

    def test_all_permutations_duplicates_missing_and_output(self):
        """Accept every field permutation while rejecting duplicates and missing required members."""
        cases = ['<root>' + ''.join(items) + '</root>' for items in itertools.permutations(
            ('<left>7</left>', '<right>x</right>', '<ready>false</ready>'))]
        output = self.run_node('''
        const outputs=[];
        for(const document of ''' + json.dumps(cases) + ''')for(const size of [1,3,document.length]){
          const value=All.PermutationsRoot.decodeXmlChunks(chunks(document,size));
          assert.equal(value.left,7);assert.equal(value.right,'x');assert.equal(value.ready,false);
          outputs.push(value.encodeXml());
        }
        for(const document of ['<root/>','<root><left>7</left><right>x</right></root>',
          '<root><ready>1</ready><left>7</left><right>x</right><left>8</left></root>',
          '<root><left>7</left><right>x</right><ready>1</ready><unknown/></root>'])assert.throws(()=>All.PermutationsRoot.decodeXml(document));
        console.log(JSON.stringify(outputs));
        ''')
        for document in json.loads(output):
            self.assertEqual([child.tag for child in etree.fromstring(document)], ['left', 'right', 'ready'])

    def test_singleton_choice_input_and_authored_output(self):
        """Reject both zero and two branch selections on input and on edited native owners."""
        self.run_node('''
        for(const body of ['<left>7</left>','<right>x</right>']){
          const value=Choice.AlternativesRoot.decodeXmlChunks(chunks('<root>'+body+'</root>',1));
          assert.equal(value.left.length+value.right.length,1);
          assert.doesNotThrow(()=>Choice.AlternativesRoot.decodeXml(value.encodeXml()));
        }
        for(const body of ['', '<left>7</left><right>x</right>', '<right>x</right><left>7</left>',
          '<left>7</left><left>8</left>'])assert.throws(()=>Choice.AlternativesRoot.decodeXml('<root>'+body+'</root>'));
        const value=new Choice.AlternativesRoot();assert.throws(()=>value.encodeXml());
        value.left=[7];value.right=['x'];assert.throws(()=>value.encodeXml());
        value.right=[];assert.doesNotThrow(()=>value.encodeXml());
        ''')

    def test_authoritative_xsd_nillability_and_transactional_mismatch(self):
        """Admit exact explicit XSD nil/occurrence contracts and reject a mismatched schema before publication."""
        self.run_node('''
        const document='<root xmlns:x="http://www.w3.org/2001/XMLSchema-instance"><label x:nil="true"/>'+
          '<mandatory x:nil="true"/><items/></root>';
        const owner=Xsd.PresenceXsdRoot.decodeXmlChunks(chunks(document,1));
        assert.deepEqual(owner.label,[[]]);assert.deepEqual(owner.mandatory,[]);assert.deepEqual(owner.items,[[[]]]);
        assert.doesNotThrow(()=>Xsd.PresenceXsdRoot.decodeXml(owner.encodeXml()));
        assert.throws(()=>Xsd.PresenceXsdRoot.decodeXml(document.replace('<mandatory x:nil="true"/>','')));
        ''')
        original = self.xsd.read_text(encoding='utf-8')
        output = self.directory / 'mismatch.mjs'
        output.write_text('preserve existing output\n', encoding='utf-8')
        try:
            self.xsd.write_text(original.replace('name="mandatory" type="xs:string" nillable="true"',
                                                 'name="mandatory" type="xs:string" nillable="false"'), encoding='utf-8')
            result = subprocess.run([str(self.compiler), '--language', 'js', '--input', str(self.xsd_source),
                                     '--config', str(self.xsd_config), '--js.output', str(output)],
                                    capture_output=True, text=True, encoding='utf-8')
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(output.read_text(encoding='utf-8'), 'preserve existing output\n')
        finally:
            self.xsd.write_text(original, encoding='utf-8')


def main():
    """Generate target fixtures with the real compiler and use the pinned installed Node SDK."""
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
    with tempfile.TemporaryDirectory(prefix='javascript-xml-content-', dir=build) as temporary:
        directory = Path(temporary)
        source, config = directory / 'presence.serializer', directory / 'presence.ini'
        source.write_text('''serializer version 1.20.0; namespace presence {
          class detail { public string value(1) xml(text); }
          class record_root { public optional<detail> nested(1); }
          class root { public optional<optional<string>> label(1); public optional<string> mandatory(2);
            public optional<optional<array<string>>> items(3); }}''', encoding='utf-8')
        config.write_text('''[xml]
enabled=true
[xml.class.presence::root]
presence.label=absent_or_nil
presence.mandatory=nil
presence.items=absent_or_nil
wrapper.items=items
name.items=item
[xml.class.presence::record_root]
presence.nested=nil
''', encoding='utf-8')
        subprocess.run([str(args.compiler.resolve()), '--language', 'js,typescript', '--input', str(source),
                        '--config', str(config), '--js.output', str(directory / 'presence.mjs'),
                        '--typescript.output', str(directory / 'presence.d.mts')], check=True)
        xsd = directory / 'presence.xsd'
        xsd.write_text('''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="root"><xs:complexType><xs:sequence>
          <xs:element name="label" type="xs:string" minOccurs="0" nillable="true"/>
          <xs:element name="mandatory" type="xs:string" nillable="true"/>
          <xs:element name="items" minOccurs="0" nillable="true"><xs:complexType><xs:sequence>
          <xs:element name="item" type="xs:string" minOccurs="0" maxOccurs="unbounded"/>
          </xs:sequence></xs:complexType></xs:element></xs:sequence></xs:complexType></xs:element></xs:schema>''', encoding='utf-8')
        xsd_source, xsd_config = directory / 'presence_xsd.serializer', directory / 'presence_xsd.ini'
        xsd_source.write_text('''serializer version 1.20.0; namespace presence_xsd { using schema "presence.xsd";
          class root { public optional<optional<string>> label(1); public optional<string> mandatory(2);
          public optional<optional<array<string>>> items(3); }}''', encoding='utf-8')
        xsd_config.write_text('''[xml]
enabled=true
[xml.class.presence_xsd::root]
presence.label=absent_or_nil
presence.mandatory=nil
presence.items=absent_or_nil
wrapper.items=items
name.items=item
''', encoding='utf-8')
        subprocess.run([str(args.compiler.resolve()), '--language', 'js,typescript', '--input', str(xsd_source),
                        '--config', str(xsd_config), '--js.output', str(directory / 'presence_xsd.mjs'),
                        '--typescript.output', str(directory / 'presence_xsd.d.mts')], check=True)
        for label, compositor in (('permutations', 'all'), ('alternatives', 'choice')):
            fields = [('left', 'int'), ('right', 'string')]
            if compositor == 'all':
                fields.append(('ready', 'boolean'))
            xsd = directory / (label + '.xsd')
            xsd.write_text('<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="root">'
                           '<xs:complexType><xs:' + compositor + '>' + ''.join(
                           '<xs:element name="' + name + '" type="xs:' + kind + '"/>' for name, kind in fields)
                           + '</xs:' + compositor + '></xs:complexType></xs:element></xs:schema>', encoding='utf-8')
            optional, closing = ('optional<', '>') if compositor == 'choice' else ('', '')
            source = directory / (label + '.serializer')
            source.write_text('serializer version 1.20.0; namespace ' + label + ' { using schema "' + label
                              + '.xsd"; class root {' + ''.join('public ' + optional
                              + {'int': 'int32', 'string': 'string', 'boolean': 'bool'}[kind] + closing + ' '
                              + name + '(' + str(index) + ');' for index, (name, kind) in enumerate(fields, 1))
                              + '}}', encoding='utf-8')
            subprocess.run([str(args.compiler.resolve()), '--language', 'js,typescript', '--input', str(source),
                            '--js.output', str(directory / (label + '.mjs')),
                            '--typescript.output', str(directory / (label + '.d.mts'))], check=True)
        javascript_xml_content_test.directory = directory
        javascript_xml_content_test.compiler = args.compiler.resolve()
        javascript_xml_content_test.xsd = directory / 'presence.xsd'
        javascript_xml_content_test.xsd_source, javascript_xml_content_test.xsd_config = xsd_source, xsd_config
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(javascript_xml_content_test))
        if not result.wasSuccessful():
            raise SystemExit(1)


if __name__ == '__main__':
    main()
