# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify selected-width floating XML facets and exact decimal-to-float32 rounding in Node/TypeScript."""
import argparse
from fractions import Fraction
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from xml.sax.saxutils import quoteattr

ROOT = Path(__file__).resolve().parent.parent
NS = 'urn:serializer:numeric-projection'


def rounded_single(value):
    """Choose the nearest exact binary32 neighbor using independent Python rational arithmetic and ties to even."""
    magnitude = abs(Fraction(value))
    approximate = abs(float(value))
    bits = struct.unpack('<I', struct.pack('<f', approximate))[0]
    choices = [(candidate, Fraction.from_float(struct.unpack('<f', struct.pack('<I', candidate))[0]))
               for candidate in (max(0, bits-1), bits, bits+1)]
    selected = min(choices, key=lambda item: (abs(item[1]-magnitude), item[0] & 1))[0]
    return struct.unpack('<f', struct.pack('<I', selected | (0x80000000 if value.startswith('-') else 0)))[0]


class javascript_xml_numeric_test(unittest.TestCase):
    """Compare real generated owning code against shared fixtures and independent exact numeric oracles."""
    def run_node(self, body):
        """Execute a real module while disabling every DOM input entry point."""
        source = self.directory / 'numeric-probe.mjs'
        source.write_text("import assert from 'node:assert/strict';\nimport * as Model from './numeric.mjs';\n"
                          "import * as Rounding from './rounding.mjs';\nimport {DOMParser} from '@xmldom/xmldom';\n"
                          "DOMParser.prototype.parseFromString=()=>{throw new Error('DOM forbidden');};\n"
                          "for(const item of [...Object.values(Model),...Object.values(Rounding)])"
                          "if(typeof item==='function'&&'_srlXmlRead' in item)item._srlXmlRead=()=>{throw new Error('DOM forbidden');};\n"
                          + body, encoding='utf-8')
        result = subprocess.run(['node', str(source)], capture_output=True, text=True, encoding='utf-8')
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        return result.stdout

    def test_floating_value_spaces_and_shared_restrictions(self):
        """Use the existing shared native fixture for NaN, selected-width facets, enums, fixed and union values."""
        good = dict(float_enum='0.1000000001', double_enum='0.100000000000000001',
                    float_min_inclusive='0.1', float_max_inclusive='0.1', double_min_inclusive='0.1', double_max_inclusive='0.1',
                    float_min_exclusive='0.2', float_max_exclusive='0.05', double_min_exclusive='0.2', double_max_exclusive='0.05',
                    nan_min_inclusive='NaN', nan_max_inclusive='NaN', float_nan_enum='NaN', double_nan_enum='NaN',
                    float_nan_fixed='NaN', double_nan_fixed='NaN', float_union='', double_union='0.05')
        bad = dict(float_enum='0.10000001', double_enum='0.1000000001', float_min_exclusive='0.1', float_max_exclusive='0.1',
                   double_min_exclusive='0.1', double_max_exclusive='0.1', nan_min_inclusive='0', nan_max_inclusive='0',
                   nan_min_exclusive='NaN', nan_max_exclusive='NaN', float_nan_enum='0', double_nan_fixed='0',
                   float_min_inclusive='NaN', double_max_inclusive='NaN', float_union='0.1', double_union='0.1')
        document = '<floating_sample xmlns="'+NS+'" '+' '.join(key+'='+quoteattr(value) for key, value in good.items())+'/>'
        negatives = ['<floating_sample xmlns="'+NS+'" '+key+'='+quoteattr(value)+'/>' for key, value in bad.items()]
        result = self.run_node('''
        const type=Model.XmlNumericProjectionFloatingSample,owner=type.decodeXml('''+json.dumps(document)+''');
        assert(Number.isNaN(owner.floatNanEnum[0]));assert(Number.isNaN(owner.doubleNanFixed[0]));
        for(const document of '''+json.dumps(negatives)+''')assert.throws(()=>type.decodeXml(document),document);
        const xml=owner.encodeXml();assert.deepEqual(type.decodeXml(xml).floatEnum,owner.floatEnum);
        owner.floatMaxExclusive=owner.floatEnum;assert.throws(()=>owner.encodeXml());
        console.log(JSON.stringify([xml]));
        ''')
        for output in json.loads(result):
            self.assertTrue(self.oracle.validate(self.provider.fromstring(output.encode())), self.oracle.error_log)

    def test_exact_binary32_midpoints_subnormals_and_signed_zero(self):
        """Avoid double rounding near decimal midpoints and preserve binary32 subnormal/negative-zero boundaries."""
        values = ['1.0000000596046448', '1.0000000596046447', '1.000000059604644775390625',
                  '1.0000001788139343', '-1.0000000596046448', '-1.0000000596046447',
                  '7.0064923216240854e-46', '7.0064923216240853e-46', '1e-999', '-1e-999', '0', '-0']
        cases = [[value, rounded_single(value)] for value in values]
        self.run_node('''
        const type=Rounding.JsRoundingSample;
        for(const [lexical,expected] of '''+json.dumps(cases)+'''){
          const document='<sample value="'+lexical+'"/>',owner=type.decodeXml(document);
          assert(Object.is(owner.value,expected),lexical+' did not round directly to binary32');
          assert(Object.is(type.decodeXml(owner.encodeXml()).value,expected));
          for(const protocol of Object.values(Model.Protocol))assert(Object.is(type.decode(owner.encode(protocol),protocol).value,expected));
        }
        ''')


def main():
    """Generate real SDK models and prepare the mature independent XSD provider for common floating restrictions."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--node-dependencies', type=Path)
    parser.add_argument('--python-dependencies', type=Path)
    args = parser.parse_args()
    if args.python_dependencies:
        sys.path.insert(0, str(args.python_dependencies.resolve()))
    from lxml import etree as provider
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
        subprocess.run(['npm.cmd' if sys.platform == 'win32' else 'npm', 'ci', '--ignore-scripts', '--no-audit', '--no-fund'], cwd=build, check=True)
    for package, version in (('saxes','6.0.0'),('xmlchars','2.2.0'),('typescript','5.9.3'),('@xmldom/xmldom','0.9.12')):
        if json.loads((modules/package/'package.json').read_text(encoding='utf-8'))['version'] != version:
            raise RuntimeError('Actual Node dependency does not match exact lock: '+package)
    with tempfile.TemporaryDirectory(prefix='javascript-xml-numeric-', dir=build) as temporary:
        directory = Path(temporary)
        source = directory/'rounding.serializer'
        source.write_text('serializer version 1.20.0; namespace js_rounding { class sample { public float value(1) xml(attribute); }}', encoding='utf-8')
        for filename, selected, extra in [('numeric', ROOT/'test/resources/xml_numeric_projection.serializer', []),
                                          ('rounding',source,['--xml.enabled','true'])]:
            subprocess.run([str(args.compiler.resolve()), '--language','js,typescript','--input',str(selected),*extra,
                            '--js.output',str(directory/(filename+'.mjs')),'--typescript.output',str(directory/(filename+'.d.mts'))], check=True)
        javascript_xml_numeric_test.directory = directory
        javascript_xml_numeric_test.provider = provider
        javascript_xml_numeric_test.oracle = provider.XMLSchema(provider.parse(str(ROOT/'test/resources/xml_numeric_projection.xsd')))
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(javascript_xml_numeric_test))
        if not result.wasSuccessful():
            raise SystemExit(1)


if __name__ == '__main__':
    main()
