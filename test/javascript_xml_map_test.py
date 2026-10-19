# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated JavaScript/TypeScript XML maps with exact native keys and bounded direct input."""
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
DOCUMENT = '''<root><numbers><entry id="+0007"><data>seven</data></entry>
<entry id="18446744073709551615"><data>maximum</data></entry></numbers>
<flags><entry><key>1</key><data>yes</data></entry><entry><key>false</key><data>no</data></entry></flags>
<values><entry key="__proto__"><value><amount>-32768</amount></value></entry>
<entry key="constructor"><value><amount>7</amount></value></entry></values>
<states><entry><key>first</key><value>one</value></entry></states></root>'''


class javascript_xml_map_test(unittest.TestCase):
    """Use real generated Map storage while forbidding every DOM decoder entry point."""
    def run_node(self, body):
        """Execute one independent actual Node probe and preserve UTF-8 diagnostics and output."""
        source = self.directory / 'map-probe.mjs'
        source.write_text("import assert from 'node:assert/strict';\nimport * as Model from './maps.mjs';\n"
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

    def test_native_typed_keys_records_optional_maps_and_wire_controls(self):
        """Keep exact bigint/Boolean/enum keys, owning values and safe prototype-shaped string keys."""
        output = self.run_node('''
        let value;
        for(const size of [1,2,7,31,document.length]){
          value=Model.JsMapsRoot.decodeXmlChunks(chunks(document,size));
          assert.deepEqual(Array.from(value.numbers),[[7n,'seven'],[18446744073709551615n,'maximum']]);
          assert.deepEqual(Array.from(value.flags),[[true,'yes'],[false,'no']]);
          assert.equal(value.optionalValues.length,1);assert(value.optionalValues[0] instanceof Map);
          assert.equal(value.optionalValues[0].get('__proto__').amount,-32768);
          assert.equal(value.optionalValues[0].get('constructor').amount,7);
          assert.equal(value.states.get(Model.JsMapsState.First),'one');
          for(const protocol of Object.values(Model.Protocol)){
            const bytes=value.encode(protocol);
            assert.deepEqual(Native.JsMapsRoot.decode(bytes,protocol).encode(protocol),bytes);
            assert.deepEqual(Model.JsMapsRoot.decodeXml(value.encodeXml()).encode(protocol),bytes);
          }
        }
        const absent=Model.JsMapsRoot.decodeXml('<root><numbers/><flags/><states/></root>');
        assert.deepEqual(absent.optionalValues,[]);assert.equal(absent.numbers.size,0);
        const empty=Model.JsMapsRoot.decodeXml('<root><numbers/><flags/><values/><states/></root>');
        assert.equal(empty.optionalValues.length,1);assert.equal(empty.optionalValues[0].size,0);
        assert.equal(Model.JsMapsRoot.decodeXml(empty.encodeXml()).optionalValues.length,1);
        console.log(value.encodeXml());
        ''')
        root = etree.fromstring(output)
        self.assertEqual([entry.attrib['id'] for entry in root.find('numbers')], ['7', '18446744073709551615'])
        self.assertEqual([entry.attrib['key'] for entry in root.find('values')], ['__proto__', 'constructor'])

    def test_duplicate_aliases_complete_entries_and_closed_membership(self):
        """Reject duplicate normalized keys and every incomplete, extra or out-of-order entry payload."""
        self.run_node('''
        const cases=[
          document.replace('id="18446744073709551615"','id="7"'),
          document.replace('<key>false</key>','<key>true</key>'),
          document.replace('<key>first</key>','<key>unknown</key>'),
          document.replace('<data>seven</data>',''),
          document.replace('<data>seven</data>','<data>a</data><data>b</data>'),
          document.replace('id="+0007"',''),
          document.replace('id="+0007"','id="18446744073709551616"'),
          document.replace('id="+0007"','id="-1"'),
          document.replace('id="+0007"','id="+0007" extra="1"'),
          document.replace('<key>1</key><data>yes</data>','<data>yes</data><key>1</key>'),
          document.replace('<numbers>','<numbers extra="1">'),
          document.replace('<numbers>','<numbers>text'),
          document.replace('<numbers>','<numbers><unknown/>'),
          document.replace('<amount>7</amount>','<amount>32768</amount>'),
          document.replace('<states><entry><key>first</key><value>one</value></entry></states>','')
        ];
        for(const invalid of cases)for(const size of [1,invalid.length])assert.throws(()=>Model.JsMapsRoot.decodeXmlChunks(chunks(invalid,size)));
        for(const value of [{},[],null]){const owner=new Model.JsMapsRoot();owner.numbers=value;assert.throws(()=>owner.encodeXml());}
        const owner=new Model.JsMapsRoot();owner.numbers.set(7,'wrong native bigint');assert.throws(()=>owner.encodeXml());
        ''')

    def test_exact_map_entry_and_lexical_key_byte_budgets(self):
        """Charge the larger original/canonical UTF-8 key size and accept equality in both directions."""
        self.run_node('''
        const bounded='<root><numbers><entry id="+0007"><data>x</data></entry></numbers><flags/><states/></root>';
        const limits=(entries,bytes)=>new Model.XmlLimits({map:new Model.XmlMapLimits({maxEntries:entries,maxKeyBytes:bytes})});
        for(const size of [1,3,bounded.length]){
          const value=Model.JsMapsRoot.decodeXmlChunks(chunks(bounded,size),limits(1,5));
          assert.equal(value.numbers.get(7n),'x');assert.doesNotThrow(()=>value.encodeXml(limits(1,1)));
          assert.throws(()=>value.encodeXml(limits(1,0)));assert.throws(()=>value.encodeXml(limits(0,1)));
          assert.throws(()=>Model.JsMapsRoot.decodeXmlChunks(chunks(bounded,size),limits(1,4)));
          assert.throws(()=>Model.JsMapsRoot.decodeXmlChunks(chunks(bounded,size),limits(0,5)));
        }
        const unicode='<root><numbers/><flags/><values><entry key="é"><value><amount>1</amount></value></entry></values><states/></root>';
        assert.doesNotThrow(()=>Model.JsMapsRoot.decodeXml(unicode,limits(1,2)));
        assert.throws(()=>Model.JsMapsRoot.decodeXml(unicode,limits(1,1)));
        for(const values of [{maxEntries:-1},{maxKeyBytes:0.5},{maxEntries:Infinity}])assert.throws(()=>new Model.XmlMapLimits(values));
        ''')

    def test_actual_typescript_map_contract(self):
        """Compile and execute a strict TypeScript consumer of native Map and explicit map limits."""
        source = self.directory / 'typed.mts'
        source.write_text("import {JsMapsRoot,XmlLimits,XmlMapLimits} from './maps.mjs';\n"
                          "const value=JsMapsRoot.decodeXmlChunks([new TextEncoder().encode(" + json.dumps(DOCUMENT) + ")],"
                          "new XmlLimits({map:new XmlMapLimits({maxEntries:2,maxKeyBytes:1024})}));\n"
                          "const keys:bigint[]=[...value.numbers.keys()];\n"
                          "const optional:[]|[Map<string,import('./maps.mjs').JsMapsPayload>]=value.optionalValues;\n"
                          "if(keys.length!==2||optional.length!==1)throw new Error('typed Map contract');\n",
                          encoding='utf-8')
        subprocess.run(['node', str(self.typescript), '--strict', '--target', 'ES2020', '--module', 'NodeNext',
                        '--moduleResolution', 'NodeNext', '--skipLibCheck', 'false', str(source)], check=True)
        subprocess.run(['node', str(source.with_suffix('.mjs'))], check=True)


def main():
    """Generate real XML/native control artifacts and qualify the exact locked Node/TS dependencies."""
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
    with tempfile.TemporaryDirectory(prefix='javascript-xml-maps-', dir=build) as temporary:
        directory = Path(temporary)
        source, config = directory / 'maps.serializer', directory / 'maps.ini'
        source.write_text('''serializer version 1.20.0; namespace js_maps {
          enum state { first, second }
          class payload { public int16 amount(1); }
          class root { public map<uint64,string> numbers(1); public map<bool,string> flags(2);
            public optional<map<string,payload>> optional_values(3); public map<state,string> states(4); }}''', encoding='utf-8')
        config.write_text('''[xml]
enabled=true
[xml.class.js_maps::root]
wrapper.numbers=numbers
map.numbers=entry
map.numbers.key=id
map.numbers.key_placement=attribute
map.numbers.value=data
wrapper.flags=flags
map.flags=entry
map.flags.value=data
wrapper.optional_values=values
map.optional_values=entry
map.optional_values.key_placement=attribute
wrapper.states=states
map.states=entry
''', encoding='utf-8')
        subprocess.run([str(args.compiler.resolve()), '--language', 'js,typescript', '--input', str(source),
                        '--config', str(config), '--js.output', str(directory / 'maps.mjs'),
                        '--typescript.output', str(directory / 'maps.d.mts')], check=True)
        subprocess.run([str(args.compiler.resolve()), '--language', 'js,typescript', '--input', str(source),
                        '--xml.enabled', 'false', '--js.output', str(directory / 'native.mjs'),
                        '--typescript.output', str(directory / 'native.d.mts')], check=True)
        javascript_xml_map_test.directory, javascript_xml_map_test.typescript = directory, modules / 'typescript/bin/tsc'
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(javascript_xml_map_test))
        if not result.wasSuccessful():
            raise SystemExit(1)


if __name__ == '__main__':
    main()
