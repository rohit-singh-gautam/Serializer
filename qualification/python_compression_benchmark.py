"""Measure a reproducible generated-Python compression slice without performance pass thresholds."""
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
import zlib


ROOT = Path(__file__).resolve().parent.parent
WORKLOAD_BYTES = 1024 * 1024
WORKLOAD_SEED = 0x53455249414C495A
MEASURED_ITERATIONS = 5
WARMUP_ITERATIONS = 1
SCHEMA = '''serializer version 1.20.0;
class benchmark_record {
  public uint64 sequence (1);
  public array uint8 payload (2);
}
'''


def measure(operation, input_bytes):
    """Measure five isolated calls after warmup, then one separate traced-allocation call."""
    for _ in range(WARMUP_ITERATIONS):
        operation()
    elapsed_ns = []
    for _ in range(MEASURED_ITERATIONS):
        started = time.perf_counter_ns()
        result = operation()
        elapsed_ns.append(time.perf_counter_ns() - started)
        del result
    gc.collect()
    tracemalloc.start()
    try:
        result = operation()
        current_bytes, peak_bytes = tracemalloc.get_traced_memory()
    finally:
        tracemalloc.stop()
    del result
    median_ns = statistics.median(elapsed_ns)
    return {
        'elapsed_ns': elapsed_ns,
        'median_ns': median_ns,
        'throughput_input_mib_per_second': input_bytes / (1024 * 1024) / (median_ns / 1e9),
        'tracemalloc_current_bytes': current_bytes,
        'tracemalloc_peak_bytes': peak_bytes,
    }


def provider_operations(api, format):
    """Return independent provider-native operations for the same valid single-frame profile."""
    if format == api.Format.NONE:
        return lambda data: memoryview(data).tobytes(), lambda data: memoryview(data).tobytes()
    if format in (api.Format.GZIP, api.Format.ZLIB, api.Format.DEFLATE):
        window_bits = {api.Format.GZIP: 31, api.Format.ZLIB: 15, api.Format.DEFLATE: -15}[format]

        def encode(data):
            """Produce one standard provider-native member/stream at level six."""
            encoder = zlib.compressobj(level=6, wbits=window_bits)
            return encoder.compress(data) + encoder.flush()

        return encode, lambda data: zlib.decompress(data, window_bits)
    if format == api.Format.ZSTD:
        from compression import zstd
        options = {zstd.CompressionParameter.compression_level: 3,
                   zstd.CompressionParameter.checksum_flag: 1,
                   zstd.CompressionParameter.content_size_flag: 0}
        return lambda data: zstd.compress(data, options=options), zstd.decompress
    import lz4.frame
    return (lambda data: lz4.frame.compress(
        data, block_size=lz4.frame.BLOCKSIZE_MAX64KB, block_linked=True,
        compression_level=0, content_checksum=True, store_size=False), lz4.frame.decompress)


def generate(compiler, directory):
    """Compile and import an actual owning model using the selected real compiler executable."""
    schema = directory / 'benchmark.serializer'
    output = directory / 'benchmark.py'
    schema.write_text(SCHEMA, encoding='utf-8')
    subprocess.run([str(compiler), '--input', str(schema), '--language', 'python',
                    '--python.output', str(output)], check=True, capture_output=True, text=True)
    spec = importlib.util.spec_from_file_location('serializer_compression_benchmark_model', output)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module, output.read_bytes()


def provider_versions(api):
    """Record actual available provider versions without installing or selecting substitutes."""
    result = {'zlib_runtime': zlib.ZLIB_RUNTIME_VERSION, 'zlib_compiled': zlib.ZLIB_VERSION}
    if api.available(api.Format.ZSTD):
        from compression import zstd
        result['stdlib_zstd_runtime'] = list(zstd.zstd_version_info)
    if api.available(api.Format.LZ4):
        import lz4
        result['python_lz4'] = lz4.__version__
        result['liblz4'] = lz4.library_version_string()
    return result


def benchmark_workload(module, name, payload, formats):
    """Measure native model staging and compression on the same validated complete object bytes."""
    api = module.__serializer_compression__
    protocol = module.Protocol.BINARY_NONE
    value = module.BenchmarkRecord()
    value.sequence, value.payload = 1, payload
    plain = value.encode(protocol)
    native_limits = module.Limits(max_bytes=2*WORKLOAD_BYTES, max_elements=2*WORKLOAD_BYTES)
    decoded = module.BenchmarkRecord.decode(plain, protocol, native_limits)
    if decoded.sequence != value.sequence or bytes(decoded.payload) != payload:
        raise AssertionError('Generated native codec round trip failed')
    del decoded
    policy = api.EncodeLimits(max_input_bytes=2*WORKLOAD_BYTES, max_output_bytes=2*WORKLOAD_BYTES)
    result = {
        'name': name,
        'payload_bytes': len(payload),
        'payload_sha256': hashlib.sha256(payload).hexdigest(),
        'serialized_bytes': len(plain),
        'native_protocol': protocol.name,
        'native_encode': measure(lambda: value.encode(protocol), len(plain)),
        'native_decode': measure(lambda: module.BenchmarkRecord.decode(plain, protocol, native_limits), len(plain)),
        'formats': [],
    }
    for format in formats:
        native_encode, native_decode = provider_operations(api, format)
        options = api.DecodeOptions(format, max_input_bytes=2*WORKLOAD_BYTES,
                                    max_output_bytes=2*WORKLOAD_BYTES,
                                    max_window_bytes=8*WORKLOAD_BYTES)
        packed = api.compress(plain, format, limits=policy)
        independent = native_encode(plain)
        if native_decode(packed) != plain or api.decompress(independent, options) != plain:
            raise AssertionError('Independent provider frame exchange failed')
        if api.decode(module.BenchmarkRecord, packed, protocol, options, native_limits).sequence != value.sequence:
            raise AssertionError('Compressed generated-model round trip failed')
        helper_encode = measure(lambda: api.compress(plain, format, limits=policy), len(plain))
        helper_decode = measure(lambda: api.decompress(packed, options), len(plain))
        baseline_encode = measure(lambda: native_encode(plain), len(plain))
        baseline_decode = measure(lambda: native_decode(packed), len(plain))
        result['formats'].append({
            'format': format.name,
            'compressed_bytes': len(packed),
            'provider_baseline_compressed_bytes': len(independent),
            'compressed_to_serialized_ratio': len(packed) / len(plain),
            'helper_encode': helper_encode,
            'helper_decode': helper_decode,
            'provider_baseline_encode': baseline_encode,
            'provider_baseline_decode': baseline_decode,
        })
    return result


def main():
    """Generate scratch models, retain reproducible JSON evidence, and clean temporary source/output."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--provider-path', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    compiler, build, output = args.compiler.resolve(), args.build.resolve(), args.output.resolve()
    for selected in (build, output):
        if selected == ROOT or ROOT in selected.parents and 'out' not in selected.relative_to(ROOT).parts:
            parser.error('Repository benchmark output must remain under ignored out/')
    if args.provider_path is not None:
        sys.path.insert(0, str(args.provider_path.resolve()))
    build.mkdir(parents=True, exist_ok=True)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='python-compression-benchmark-', dir=build) as temporary:
        module, generated_source = generate(compiler, Path(temporary))
        api = module.__serializer_compression__
        formats = [format for format in api.Format if api.available(format)]
        pattern = b'Serializer fixed-seed compression benchmark 0123456789\n'
        repeated = (pattern * (WORKLOAD_BYTES // len(pattern) + 1))[:WORKLOAD_BYTES]
        random_bytes = random.Random(WORKLOAD_SEED).randbytes(WORKLOAD_BYTES)
        result = {
            'scope': 'Selected generated-Python owning native-codec/compression slice; XML Q05/Q06/Q07 remain open',
            'compiler': str(compiler),
            'compiler_version': subprocess.run([str(compiler), '--version'], capture_output=True,
                                                text=True, check=True).stdout.strip(),
            'python': sys.version,
            'platform': platform.platform(),
            'processor': platform.processor(),
            'providers': provider_versions(api),
            'formats': [format.name for format in formats],
            'unavailable_formats': [format.name for format in api.Format if format not in formats],
            'warmup_iterations': WARMUP_ITERATIONS,
            'measured_iterations': MEASURED_ITERATIONS,
            'allocation_measurements_per_operation': 1,
            'fixed_seed': WORKLOAD_SEED,
            'schema_source_bytes': len(SCHEMA.encode('utf-8')),
            'generated_source_bytes': len(generated_source),
            'generated_source_sha256': hashlib.sha256(generated_source).hexdigest(),
            'limits': {'serialized_input_bytes': 2*WORKLOAD_BYTES,
                       'compressed_input_bytes': 2*WORKLOAD_BYTES,
                       'compressed_output_bytes': 2*WORKLOAD_BYTES,
                       'expanded_output_bytes': 2*WORKLOAD_BYTES,
                       'window_bytes': 8*WORKLOAD_BYTES,
                       'native_max_bytes': 2*WORKLOAD_BYTES,
                       'native_max_elements': 2*WORKLOAD_BYTES},
            'timing_method': 'perf_counter_ns; median of five calls, tracing disabled; throughput uses serialized bytes',
            'baseline': 'Independent provider-native whole-span APIs on valid compatible one-frame profiles; no bounds, cancellation or exact-EOF validation; NONE copies via memoryview.tobytes',
            'allocation_method': 'One separate tracemalloc call after gc.collect; preexisting inputs excluded; retains returned output until peak sampled',
            'measurement_limits': [
                'Tracemalloc reports traced Python allocations, not native-provider malloc/RSS or a hard heap bound',
                'Logical codec byte/window bounds do not bound total process memory',
                'Concurrent machine activity, provider versions and host settings affect timings; no performance pass threshold',
                'Compression timing starts after real generated native serialization; native encode/decode measured separately',
                'No earlier compiler/source-size or XML-performance baseline is claimed',
            ],
            'workloads': [benchmark_workload(module, 'compressible_repeated', repeated, formats),
                          benchmark_workload(module, 'incompressible_fixed_seed_random', random_bytes, formats)],
        }
        output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(f'Wrote {output}')
    print(f'Generated source: {result["generated_source_bytes"]} bytes; providers: {", ".join(result["formats"])}')
    for workload in result['workloads']:
        for selected in workload['formats']:
            print(f'{workload["name"]} {selected["format"]}: {selected["compressed_bytes"]} bytes; '
                  f'encode {selected["helper_encode"]["throughput_input_mib_per_second"]:.1f} MiB/s; '
                  f'decode {selected["helper_decode"]["throughput_input_mib_per_second"]:.1f} MiB/s; '
                  f'Python peaks {selected["helper_encode"]["tracemalloc_peak_bytes"]}/'
                  f'{selected["helper_decode"]["tracemalloc_peak_bytes"]} bytes')


if __name__ == '__main__':
    main()
