# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Compare token and retained-tree costs for an imported namespace/ordered owning graph."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import platform
import subprocess
import sys
import xml.etree.ElementTree as etree

import python_xml_benchmark as measurements

ROOT = Path(__file__).resolve().parents[1]


def load_model(path):
    """Load one actual generated module without publishing it outside this benchmark process."""
    spec = importlib.util.spec_from_file_location('namespace_ordered_model', path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def worker(args):
    """Measure one profile in a fresh process; keep native values and wire hashes identical."""
    module = load_model(args.model)
    model = module.BenchmarkXsdOrder
    if args.mode == 'tree':
        # The optional ordered child is absent in this workload. Force the established
        # tree adapter for the same scalar/collection values, without timing a different model.
        module._xml_token_capable = lambda *unused: False
        module._xml_token_required = lambda *unused: False
    else:
        assert module._xml_token_capable(model)
        def reject_tree(*unused):
            """Require the measured token operations to avoid the whole-document parser."""
            raise AssertionError('Token benchmark entered a retained-tree helper')
        module._xml_parse = reject_tree
    child = '<item name="Bolt &amp; Ω"><price unit="USD">2</price></item>'
    document = ("<order xmlns:p='urn:item' id='9007199254740993' samples='2 8' "
                "selections='p:item 7' codes='AB CD'>" + child * args.items + '</order>').encode()
    value = model.decode_xml(document)
    assert len(value.xsd_item) == args.items
    encoded = value.encode_xml()
    observed = etree.fromstring(encoded)
    assert len(observed) == args.items
    assert all(item.attrib['name'] == 'Bolt & Ω' and item[0].text == '2' for item in observed)
    assert model.decode_xml(encoded).xsd_samples == value.xsd_samples
    native = {protocol.name: hashlib.sha256(value.encode(protocol)).hexdigest()
              for protocol in module.Protocol}
    report = {'mode': args.mode, 'items': args.items,
              'input_bytes': len(document), 'output_bytes': len(encoded),
              'input_sha256': hashlib.sha256(document).hexdigest(), 'native_sha256': native,
              'encode': measurements.measure(value.encode_xml, len(document)),
              'decode': measurements.measure(lambda: model.decode_xml(document), len(document)),
              'process_peak': measurements.process_peak_memory()}
    print(json.dumps(report))


def main():
    """Generate through the real compiler and retain reproducible isolated measurement evidence."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path)
    parser.add_argument('--build', type=Path)
    parser.add_argument('--items', type=int, default=20000)
    parser.add_argument('--model', type=Path)
    parser.add_argument('--mode', choices=('token', 'tree'))
    args = parser.parse_args()
    if args.items < 1 or args.items > measurements.MAX_WORKLOAD_TAGS:
        parser.error('Choose 1 through 75000 owning items')
    if args.mode:
        worker(args)
        return
    if args.compiler is None or args.build is None:
        parser.error('Provide --compiler and --build')
    compiler, build = args.compiler.resolve(strict=True), args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository output must stay beneath ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    schema, model = build / 'model.serializer', build / 'model.py'
    subprocess.run([str(compiler), '--import-xsd', str(ROOT / 'test/resources/xml_shared_import/root.xsd'),
                    '--namespace', 'benchmark', '--root', 'order', '--output', str(schema)], check=True)
    subprocess.run([str(compiler), '--input', str(schema), '--language', 'python',
                    '--output', str(model)], check=True)
    results = []
    for count in sorted({2, args.items}):
        for mode in ('token', 'tree'):
            result = subprocess.run([sys.executable, '-B', str(Path(__file__).resolve()),
                '--model', str(model), '--mode', mode, '--items', str(count)],
                capture_output=True, text=True, encoding='utf-8', check=True)
            results.append(json.loads(result.stdout))
        assert results[-1]['input_sha256'] == results[-2]['input_sha256']
        assert results[-1]['native_sha256'] == results[-2]['native_sha256']
    report = {'python': sys.version, 'platform': platform.platform(),
              'compiler_sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(),
              'generated_bytes': model.stat().st_size,
              'generated_sha256': hashlib.sha256(model.read_bytes()).hexdigest(),
              'results': results,
              'scope': 'Absent optional ordered child; root QName/list attributes plus owning records. '
                       'Five wall/CPU samples and separate traced allocation calls. '
                       'Process peaks include setup and verification; no hard memory ceiling or speed threshold.'}
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('PASS: isolated namespace/ordered token and tree costs with equal native wire hashes')


if __name__ == '__main__':
    main()
