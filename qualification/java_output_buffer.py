# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Measure Java output-window allocation and throughput against a retained compiler."""
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('java_output_buffer_tests', ROOT / 'test/java_output_buffer_test.py')
tests = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(tests)
DEFAULT_ITERATIONS = 2000
DEFAULT_WARMUP_ITERATIONS = 3000
DEFAULT_SAMPLE_COUNT = 5
PAYLOAD_BYTES = 4096

JAVA = 'package ' + tests.PACKAGE + r''';
public final class Benchmark {
''' + tests.COMMON + r'''
  private static volatile byte[] published;
  private static volatile java.nio.ByteBuffer publishedNio;
  private static volatile long checksum;
  /** Execute real encoding with an observable result and a changing payload byte. */
  private static void repeat(Schema.BufferRecord value, byte[] storage, java.nio.ByteBuffer nio, int count) {
    for (int index = 0; index < count; ++index) {
      value.payload[0] = (byte)index;
      __ENCODE__
    }
  }
  /** Measure per-thread requested allocation and elapsed time after explicit JVM warmup. */
  public static void main(String[] args) {
    var value = sample(); value.payload = new byte[__PAYLOAD_BYTES__];
    int messageBytes = value.encode(Schema.Protocol.BINARY_NONE).length;
    byte[] storage = __ARRAY_STORAGE__;
    java.nio.ByteBuffer nio = __NIO_STORAGE__;
    int iterations = Integer.parseInt(args[0]), warmup = Integer.parseInt(args[1]), samples = Integer.parseInt(args[2]);
    var management = java.lang.management.ManagementFactory.getThreadMXBean();
    if (!(management instanceof com.sun.management.ThreadMXBean bean) || !bean.isThreadAllocatedMemorySupported()) {
      throw new IllegalStateException("Thread allocation measurement unavailable");
    }
    if (!bean.isThreadAllocatedMemoryEnabled()) bean.setThreadAllocatedMemoryEnabled(true);
    repeat(value, storage, nio, warmup);
    long thread = Thread.currentThread().getId();
    double[] nanos = new double[samples], allocations = new double[samples];
    for (int sample = 0; sample < samples; ++sample) {
      long beforeBytes = bean.getThreadAllocatedBytes(thread), beforeTime = System.nanoTime();
      repeat(value, storage, nio, iterations);
      long elapsed = System.nanoTime() - beforeTime;
      long allocated = bean.getThreadAllocatedBytes(thread) - beforeBytes;
      nanos[sample] = (double)elapsed / iterations;
      allocations[sample] = (double)allocated / iterations;
    }
    System.out.println("{\"message_bytes\":" + messageBytes + ",\"ns_per_operation\":" + java.util.Arrays.toString(nanos) +
        ",\"allocated_bytes_per_operation\":" + java.util.Arrays.toString(allocations) + ",\"checksum\":" + checksum + "}");
  }
}
'''


def run(command, capture=False):
    """Retain actual tool failures and capture only the benchmark's structured report when requested."""
    return subprocess.run([str(item) for item in command], check=True, timeout=300,
                          text=True, encoding='utf-8', stdout=subprocess.PIPE if capture else None)


def main():
    """Run each mode in a fresh fixed-heap JVM so ordinary encoding has an uncontaminated type profile."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--baseline-compiler', type=Path, required=True)
    parser.add_argument('--java', default='java')
    parser.add_argument('--javac', default='javac')
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--iterations', type=int, default=DEFAULT_ITERATIONS)
    parser.add_argument('--warmup', type=int, default=DEFAULT_WARMUP_ITERATIONS)
    parser.add_argument('--samples', type=int, default=DEFAULT_SAMPLE_COUNT)
    args = parser.parse_args()
    args.compiler, args.baseline_compiler, args.build = args.compiler.resolve(), args.baseline_compiler.resolve(), args.build.resolve()
    if not args.build.is_relative_to(ROOT / 'out'):
        parser.error('Qualification output must remain beneath the repository out/ directory')
    if min(args.iterations, args.warmup, args.samples) <= 0:
        parser.error('Iteration, warmup and sample counts must be positive')
    args.build.mkdir(parents=True, exist_ok=True)
    records = []
    for mode, compiler in [('baseline-owning', args.baseline_compiler), ('current-owning', args.compiler),
                           ('current-window', args.compiler), ('current-nio-heap', args.compiler),
                           ('current-nio-direct', args.compiler)]:
        directory = args.build / mode
        nio_mode = mode.startswith('current-nio')
        tests.generate(args, directory, compiler=compiler, reuse=mode == 'current-window', nio=nio_mode)
        if nio_mode:
            body = ('nio.position(0); int written = value.encode(Schema.Protocol.BINARY_NONE, nio); '
                    'publishedNio = nio; checksum += written + nio.get(written-1);')
        elif mode == 'current-window':
            body = ('int written = value.encode(Schema.Protocol.BINARY_NONE, storage, 0, storage.length); '
                    'published = storage; checksum += written + storage[written-1];')
        else:
            body = ('byte[] message = value.encode(Schema.Protocol.BINARY_NONE); published = message; '
                    'checksum += message.length + message[message.length-1];')
        array_storage = 'null' if mode == 'current-nio-direct' else 'new byte[messageBytes]'
        nio_storage = ('java.nio.ByteBuffer.allocateDirect(messageBytes)' if mode == 'current-nio-direct' else
                       'java.nio.ByteBuffer.wrap(storage)' if nio_mode else 'null')
        source = (JAVA.replace('__ENCODE__', body).replace('__PAYLOAD_BYTES__', str(PAYLOAD_BYTES))
                  .replace('__ARRAY_STORAGE__', array_storage).replace('__NIO_STORAGE__', nio_storage))
        path, classes = directory / 'Benchmark.java', directory / 'classes'
        path.write_text(source, encoding='utf-8')
        classes.mkdir(exist_ok=True)
        run([args.javac, '--release', '17', '-encoding', 'UTF-8', '-Xlint:all', '-Werror', '-d', classes,
             directory / 'Schema.java', path])
        result = run([args.java, '-Xms256m', '-Xmx256m', '-XX:+UseSerialGC', '-cp', classes,
                      tests.PACKAGE + '.Benchmark', args.iterations, args.warmup, args.samples], capture=True)
        (directory / 'stdout.log').write_text(result.stdout, encoding='utf-8')
        report = json.loads([line for line in result.stdout.splitlines() if line.startswith('{')][-1])
        report['mode'] = mode
        report['generated_source_bytes'] = (directory / 'Schema.java').stat().st_size
        records.append(report)
    if len({item['message_bytes'] for item in records}) != 1:
        raise AssertionError('Measured modes produced different message sizes')
    report = {'protocol': 'BINARY_NONE', 'payload_bytes': PAYLOAD_BYTES, 'iterations': args.iterations,
              'warmup_iterations': args.warmup, 'samples': args.samples, 'jvm_heap_bytes': 256 * 1024 * 1024,
              'measurement': 'per-thread JVM requested allocation; excludes retained caller buffer, JVM/process peaks and native allocations',
              'modes': records}
    (args.build / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__':
    main()
