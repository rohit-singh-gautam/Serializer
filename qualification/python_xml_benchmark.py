# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Measure generated Python XML and ordinary native codecs on reproducible owning workloads."""
import argparse
import gc
import hashlib
import importlib.util
import json
from pathlib import Path
import platform
import random
import statistics
import subprocess
import sys
import tempfile
import time
import tracemalloc
import xml.etree.ElementTree as etree
import xml.parsers.expat as expat


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'example/xml'
MEASURED_ITERATIONS = 5
WORKLOAD_SEED = 0x584D4C50455246
XML_NAMESPACE = 'urn:serializer:xml-example'


def digest(data):
    """Identify exact source, input and encoded output bytes in the retained report."""
    return hashlib.sha256(data).hexdigest()


def measure(operation, input_bytes):
    """Warm once, retain five wall/CPU samples, then measure a separate traced allocation call."""
    operation()
    wall, cpu = [], []
    for _ in range(MEASURED_ITERATIONS):
        wall_start, cpu_start = time.perf_counter_ns(), time.process_time_ns()
        result = operation()
        cpu.append(time.process_time_ns() - cpu_start)
        wall.append(time.perf_counter_ns() - wall_start)
        del result
    gc.collect()
    tracemalloc.start()
    try:
        result = operation()
        current, peak = tracemalloc.get_traced_memory()
    finally:
        tracemalloc.stop()
    del result
    median = statistics.median(wall)
    return {'wall_ns': wall, 'cpu_ns': cpu, 'median_wall_ns': median,
            'median_cpu_ns': statistics.median(cpu),
            'input_mib_per_second': input_bytes / (1024 * 1024) / (median / 1e9),
            'tracemalloc_current_bytes': current, 'tracemalloc_peak_bytes': peak}


def generate(compiler, directory, enabled):
    """Invoke the actual compiler and import a distinct XML-enabled or native-only module."""
    name = 'xml_enabled' if enabled else 'native_only'
    output = directory / (name + '.py')
    command = [str(compiler), '--input', str(SOURCE / 'model.serializer'), '--language', 'python',
               '--python.output', str(output), '--xml.enabled', str(enabled).lower()]
    if enabled:
        command += ['--config', str(SOURCE / 'codegen.ini')]
    completed = subprocess.run(command, check=True, capture_output=True, text=True,
                               encoding='utf-8', timeout=60)
    (directory / (name + '-generation.log')).write_text(completed.stdout + completed.stderr, encoding='utf-8')
    spec = importlib.util.spec_from_file_location('serializer_xml_benchmark_' + name, output)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module, {'bytes': output.stat().st_size, 'sha256': digest(output.read_bytes()),
                    'command': command}


def owning_value(module, tags):
    """Build equivalent owning models without aliasing the workload's mutable tag list."""
    value = module.XmlExampleOrder()
    value.id, value.customer, value.count = 7, 'R&D\t\r\n\U0001f600', len(tags)
    value.tags = list(tags)
    return value


def check_xml(document, tags):
    """Require independently parsed expanded names, order and exact escaped Unicode values."""
    root = etree.fromstring(document)
    prefix = '{' + XML_NAMESPACE + '}'
    if root.tag != prefix + 'order' or root.attrib != {'id': '7'}:
        raise AssertionError('Independent XML root/attribute mismatch')
    expected = [(prefix + 'customer', 'R&D\t\r\n\U0001f600'),
                (prefix + 'count', str(len(tags)))] + [(prefix + 'tag', tag) for tag in tags]
    if [(child.tag, child.text or '') for child in root] != expected:
        raise AssertionError('Independent XML ordered-value mismatch')
    return root


def workload(name, tags, enabled, native, directory):
    """Measure equal native values, independent XML parsing and actual generated owning XML codecs."""
    value, baseline = owning_value(enabled, tags), owning_value(native, tags)
    document = value.encode_xml()
    tree = check_xml(document, tags)
    canonical = value.encode(enabled.Protocol.BINARY_NONE)
    if enabled.XmlExampleOrder.decode_xml(document).encode(enabled.Protocol.BINARY_NONE) != canonical:
        raise AssertionError('Generated XML owning roundtrip mismatch')
    if baseline.encode(native.Protocol.BINARY_NONE) != canonical:
        raise AssertionError('XML generation changed ordinary binary bytes')
    # Parse the independent provider output with the generated codec as a separate semantic oracle.
    # Literal CR in element text is normalized by XML readers; emit a reference to retain its value.
    provider_encode = lambda: etree.tostring(tree, encoding='utf-8').replace(b'\r', b'&#13;')
    provider_document = provider_encode()
    if enabled.XmlExampleOrder.decode_xml(provider_document).encode(enabled.Protocol.BINARY_NONE) != canonical:
        raise AssertionError('Independent XML producer mismatch')
    (directory / (name + '.xml')).write_bytes(document)
    result = {'name': name, 'tags': len(tags), 'logical_utf8_value_bytes':
              sum(len(tag.encode('utf-8')) for tag in tags),
              'xml_bytes': len(document), 'xml_sha256': digest(document),
              'xml_encode': measure(value.encode_xml, len(document)),
              'xml_decode': measure(lambda: enabled.XmlExampleOrder.decode_xml(document), len(document)),
              'elementtree_decode_baseline': measure(lambda: etree.fromstring(document), len(document)),
              'elementtree_encode_baseline': measure(provider_encode, len(document)),
              'native_protocols': []}
    for protocol in enabled.Protocol:
        if protocol.name not in ('JSON', 'BINARY_NONE', 'BINARY_INTEGER', 'BINARY_STRING'):
            continue
        plain = value.encode(protocol)
        native_protocol = getattr(native.Protocol, protocol.name)
        if baseline.encode(native_protocol) != plain:
            raise AssertionError('XML-enabled/native-only wire mismatch: ' + protocol.name)
        limits = enabled.Limits()
        native_limits = native.Limits()
        if enabled.XmlExampleOrder.decode(plain, protocol, limits).encode_xml() != document:
            raise AssertionError('Native protocol changed XML values')
        result['native_protocols'].append({
            'protocol': protocol.name, 'encoded_bytes': len(plain), 'sha256': digest(plain),
            'xml_enabled_encode': measure(lambda: value.encode(protocol), len(plain)),
            'native_only_encode': measure(lambda: baseline.encode(native_protocol), len(plain)),
            'xml_enabled_decode': measure(lambda: enabled.XmlExampleOrder.decode(plain, protocol, limits), len(plain)),
            'native_only_decode': measure(lambda: native.XmlExampleOrder.decode(plain, native_protocol, native_limits), len(plain))})
    return result


def main():
    """Retain a complete scoped report and encoded fixtures while cleaning generated benchmark scratch."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    compiler, build = args.compiler.resolve(), args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('In-repository output must remain beneath ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    randomizer = random.Random(WORKLOAD_SEED)
    alphabet = 'abcXYZ012 &<>\t\r\n\U0001f600\u03a9'
    workloads = [('small', ['A', 'B']),
                 ('repeated', ['repeated & <escaped> \U0001f600'] * 20000),
                 ('fixed_seed', [''.join(randomizer.choices(alphabet, k=32)) for _ in range(20000)])]
    with tempfile.TemporaryDirectory(prefix='python-xml-benchmark-', dir=build) as temporary:
        directory = Path(temporary)
        enabled, enabled_source = generate(compiler, directory, True)
        native, native_source = generate(compiler, directory, False)
        report = {'status': 'passed', 'scope': 'Selected generated-Python owning XML/native codec measurements',
                  'compiler_sha256': digest(compiler.read_bytes()),
                  'compiler_version': subprocess.run([str(compiler), '--version'], check=True, capture_output=True,
                                                     text=True, encoding='utf-8', timeout=30).stdout.strip(),
                  'runner_sha256': digest(Path(__file__).read_bytes()),
                  'python': sys.version, 'platform': platform.platform(),
                  'expat': expat.EXPAT_VERSION, 'elementtree': etree.VERSION,
                  'workload_seed': WORKLOAD_SEED, 'warmup_iterations': 1,
                  'measured_iterations': MEASURED_ITERATIONS,
                  'source': {'xml_enabled': enabled_source, 'native_only': native_source},
                  'measurement_limits': [
                      'Timings exclude generation, imports, input construction and disk I/O.',
                      'CPU samples are this Python process only; no performance pass threshold is imposed.',
                      'tracemalloc covers traced Python allocation, not native parser workspace or process RSS.',
                      'ElementTree retains a generic tree and does not construct the same typed model.',
                      'Owning codecs stage full messages; these are not streaming or larger-than-RAM claims.'],
                  'workloads': [workload(name, tags, enabled, native, build) for name, tags in workloads]}
        for name in ('xml_enabled-generation.log', 'native_only-generation.log'):
            (build / name).write_bytes((directory / name).read_bytes())
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('PASS: three XML workloads, four native baselines, independent XML and traced allocation measurements')


if __name__ == '__main__':
    main()
