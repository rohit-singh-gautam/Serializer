# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Measure generated Python XML and ordinary native codecs on reproducible owning workloads."""
import argparse
import ctypes
import gc
import hashlib
import importlib.util
import json
import os
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
MAX_WORKLOAD_TAGS = 75000


def suppress_windows_error_dialogs():
    """Let failing native child processes return diagnostics without interactive error dialogs."""
    if os.name == 'nt':
        ctypes.windll.kernel32.SetErrorMode(0x8003)


def process_peak_memory():
    """Report the current process's OS residency high-water, separate from traced Python allocations."""
    if os.name == 'nt':
        class memory_counters(ctypes.Structure):
            """Match PROCESS_MEMORY_COUNTERS on either Windows pointer width."""
            _fields_ = [('cb', ctypes.c_ulong), ('faults', ctypes.c_ulong)] + [
                (name, ctypes.c_size_t) for name in ('peak_working_set', 'working_set', 'quota_peak_paged',
                    'quota_paged', 'quota_peak_nonpaged', 'quota_nonpaged', 'pagefile', 'peak_pagefile')]
        kernel, psapi = ctypes.WinDLL('kernel32', use_last_error=True), ctypes.WinDLL('psapi', use_last_error=True)
        kernel.GetCurrentProcess.restype = ctypes.c_void_p
        psapi.GetProcessMemoryInfo.argtypes = (ctypes.c_void_p, ctypes.POINTER(memory_counters), ctypes.c_ulong)
        psapi.GetProcessMemoryInfo.restype = ctypes.c_int
        counters = memory_counters()
        counters.cb = ctypes.sizeof(counters)
        if not psapi.GetProcessMemoryInfo(kernel.GetCurrentProcess(), ctypes.byref(counters), counters.cb):
            raise ctypes.WinError(ctypes.get_last_error())
        return {'category': 'Windows PeakWorkingSetSize, whole benchmark process through measurement completion',
                'bytes': counters.peak_working_set, 'peak_commit_bytes': counters.peak_pagefile}
    if platform.system() == 'Linux':
        import resource
        return {'category': 'Linux getrusage(RUSAGE_SELF).ru_maxrss, whole benchmark process through measurement completion',
                'bytes': resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024}
    return {'category': 'OS process residency unavailable; traced Python allocation measurements remain separate'}


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


def interruption_controls(module, value, document, limits):
    """Cancel before/mid owning work, expire a host deadline, then prove a fresh call and source recover."""
    class interrupted(RuntimeError):
        """Retain one exact caller-owned failure object through native XML callbacks."""
    class context:
        """Borrow one callback policy without resetting its counter during nested traversal."""
        def __init__(self, threshold, deadline):
            """Select a bounded poll count or an already expired monotonic host deadline."""
            self.threshold, self.deadline = threshold, deadline
            self.polls, self.marker = 0, interrupted('Caller interrupted XML')
        def check(self):
            """Preserve the caller's exact exception instead of classifying it as malformed XML."""
            self.polls += 1
            if self.polls >= self.threshold or self.deadline is not None and time.monotonic_ns() >= self.deadline:
                raise self.marker
        def consume(self, resource, amount):
            """Poll each native admission before retaining more source/model storage."""
            self.check()
        def check_buffer(self, amount):
            """Poll native parser/emitter buffer admissions as part of the same operation."""
            self.check()
    canonical = value.encode(module.Protocol.BINARY_NONE)
    rows = []
    for writing in (False, True):
        for threshold, deadline in ((1, None), (1000, None), (sys.maxsize, 0)):
            policy = context(threshold, deadline)
            try:
                if writing:
                    value.encode_xml(limits, context=policy)
                else:
                    module.XmlExampleOrder.decode_xml(document, limits, context=policy)
            except interrupted as failure:
                if failure is not policy.marker:
                    raise AssertionError('XML replaced the caller interruption object')
            else:
                raise AssertionError('Interrupted XML operation returned a candidate')
            rows.append({'direction': 'encode' if writing else 'decode',
                         'policy': 'deadline' if deadline is not None else 'cancellation',
                         'threshold': threshold if deadline is None else None, 'polls': policy.polls})
    if value.encode(module.Protocol.BINARY_NONE) != canonical:
        raise AssertionError('Interrupted encoding changed source values')
    if module.XmlExampleOrder.decode_xml(document, limits).encode(module.Protocol.BINARY_NONE) != canonical:
        raise AssertionError('Fresh decode failed after interruption')
    if value.encode_xml(limits) != document:
        raise AssertionError('Fresh encode failed after interruption')
    return rows


def workload(name, tags, enabled, native, directory, maximum_xml_entries=1000000, measurements=True):
    """Measure equal native values, independent XML parsing and actual generated owning XML codecs."""
    value, baseline = owning_value(enabled, tags), owning_value(native, tags)
    xml_limits = enabled.Limits(max_bytes=16 * 1024 * 1024, max_string_bytes=16 * 1024 * 1024,
                                max_elements=maximum_xml_entries)
    document = value.encode_xml(xml_limits)
    tree = check_xml(document, tags)
    canonical = value.encode(enabled.Protocol.BINARY_NONE)
    if enabled.XmlExampleOrder.decode_xml(document, xml_limits).encode(enabled.Protocol.BINARY_NONE) != canonical:
        raise AssertionError('Generated XML owning roundtrip mismatch')
    if baseline.encode(native.Protocol.BINARY_NONE) != canonical:
        raise AssertionError('XML generation changed ordinary binary bytes')
    # Parse the independent provider output with the generated codec as a separate semantic oracle.
    # Literal CR in element text is normalized by XML readers; emit a reference to retain its value.
    provider_encode = lambda: etree.tostring(tree, encoding='utf-8').replace(b'\r', b'&#13;')
    provider_document = provider_encode()
    if enabled.XmlExampleOrder.decode_xml(provider_document, xml_limits).encode(enabled.Protocol.BINARY_NONE) != canonical:
        raise AssertionError('Independent XML producer mismatch')
    (directory / (name + '.xml')).write_bytes(document)
    interruptions = interruption_controls(enabled, value, document, xml_limits) if len(tags) > 2 else []
    if not measurements:
        return {'name': name, 'tags': len(tags), 'xml_bytes': len(document), 'xml_sha256': digest(document),
                'interruption_controls': interruptions}
    result = {'name': name, 'tags': len(tags), 'logical_utf8_value_bytes':
              sum(len(tag.encode('utf-8')) for tag in tags),
              'xml_bytes': len(document), 'xml_sha256': digest(document),
              'xml_encode': measure(lambda: value.encode_xml(xml_limits), len(document)),
              'xml_decode': measure(lambda: enabled.XmlExampleOrder.decode_xml(document, xml_limits), len(document)),
              'elementtree_decode_baseline': measure(lambda: etree.fromstring(document), len(document)),
              'elementtree_encode_baseline': measure(provider_encode, len(document)),
              'native_protocols': [], 'interruption_controls': interruptions}
    for protocol in enabled.Protocol:
        if protocol.name not in ('JSON', 'BINARY_NONE', 'BINARY_INTEGER', 'BINARY_STRING'):
            continue
        plain = value.encode(protocol)
        native_protocol = getattr(native.Protocol, protocol.name)
        if baseline.encode(native_protocol) != plain:
            raise AssertionError('XML-enabled/native-only wire mismatch: ' + protocol.name)
        limits = enabled.Limits()
        native_limits = native.Limits()
        if enabled.XmlExampleOrder.decode(plain, protocol, limits).encode_xml(xml_limits) != document:
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
    parser.add_argument('--tags', type=int, default=20000)
    parser.add_argument('--xml-entries', type=int, default=1000000,
                        help='Bound XML nodes and Expat text fragments independently from input byte limits')
    parser.add_argument('--interruptions-only', action='store_true', help='Run source-preserving large cancellation/deadline/recovery controls without timing replay')
    args = parser.parse_args()
    suppress_windows_error_dialogs()
    if not 1 <= args.tags <= MAX_WORKLOAD_TAGS:
        parser.error(f'--tags must be between 1 and {MAX_WORKLOAD_TAGS} for this owning workload')
    if not 1 <= args.xml_entries <= 2000000:
        parser.error('--xml-entries must be between 1 and 2000000 for this selected profile')
    compiler, build = args.compiler.resolve(), args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('In-repository output must remain beneath ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    randomizer = random.Random(WORKLOAD_SEED)
    alphabet = 'abcXYZ012 &<>\t\r\n\U0001f600\u03a9'
    workloads = [('small', ['A', 'B']),
                 ('repeated', ['repeated & <escaped> \U0001f600'] * args.tags),
                 ('fixed_seed', [''.join(randomizer.choices(alphabet, k=32)) for _ in range(args.tags)])]
    with tempfile.TemporaryDirectory(prefix='python-xml-benchmark-', dir=build) as temporary:
        directory = Path(temporary)
        enabled, enabled_source = generate(compiler, directory, True)
        native, native_source = generate(compiler, directory, False)
        report = {'status': 'running', 'scope': 'Selected generated-Python owning XML/native codec measurements',
                  'compiler_sha256': digest(compiler.read_bytes()),
                  'compiler_version': subprocess.run([str(compiler), '--version'], check=True, capture_output=True,
                                                     text=True, encoding='utf-8', timeout=30).stdout.strip(),
                  'runner_sha256': digest(Path(__file__).read_bytes()),
                  'python': sys.version, 'platform': platform.platform(),
                  'expat': expat.EXPAT_VERSION, 'elementtree': etree.VERSION,
                  'workload_seed': WORKLOAD_SEED, 'warmup_iterations': 0 if args.interruptions_only else 1,
                  'measured_iterations': 0 if args.interruptions_only else MEASURED_ITERATIONS,
                  'xml_policy': {'max_bytes': 16 * 1024 * 1024, 'max_string_bytes': 16 * 1024 * 1024,
                                 'max_elements_and_fragments': args.xml_entries},
                  'source': {'xml_enabled': enabled_source, 'native_only': native_source},
                  'measurement_limits': [
                      'Timings exclude generation, imports, input construction and disk I/O.',
                      'CPU samples are this Python process only; no performance pass threshold is imposed.',
                      'tracemalloc covers traced Python allocation, not native parser workspace or process RSS.',
                      'ElementTree retains a generic tree and does not construct the same typed model.',
                      'Owning codecs stage full messages; these are not streaming or larger-than-RAM claims.'],
                  'workloads': []}
        for name, tags in workloads:
            try:
                report['workloads'].append(workload(name, tags, enabled, native, build, args.xml_entries,
                                                     not args.interruptions_only))
            except Exception as error:
                report['status'], report['failure'] = 'failed', str(error)
                (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
                raise
            (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        report['status'] = 'passed'
        report['scope'] = ('Selected generated-Python owning XML cancellation/deadline/recovery controls'
                           if args.interruptions_only else report['scope'])
        report['process_peak_memory'] = process_peak_memory()
        for name in ('xml_enabled-generation.log', 'native_only-generation.log'):
            (build / name).write_bytes((directory / name).read_bytes())
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('PASS: three XML inputs, twelve large cancellation/deadline controls and fresh-call recovery'
          if args.interruptions_only else
          'PASS: three XML workloads, four native baselines, independent XML and traced allocation measurements')


if __name__ == '__main__':
    main()
