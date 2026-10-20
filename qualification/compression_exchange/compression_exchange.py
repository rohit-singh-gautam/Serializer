# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Consume the common compression decisions using actual generated Python models/helpers."""
import importlib.util
import json
from pathlib import Path
import sys


PROTOCOLS = ('JSON', 'BINARY_NONE', 'BINARY_INTEGER', 'BINARY_STRING')
FORMATS = ('NONE', 'GZIP', 'ZLIB', 'DEFLATE')


def load_module(path):
    """Import the selected real compiler output under its own stable module identity."""
    spec = importlib.util.spec_from_file_location('serializer_common_compression_model', path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def same_model(actual, expected):
    """Require every owning field to match after exact native decoding."""
    if actual.code != expected.code or actual.text != expected.text:
        raise AssertionError('Shared generated model differs')


def main():
    """Produce all selected frames or replay every mandatory exchange/corpus row without skips."""
    module = load_module(sys.argv[1])
    api = module.__serializer_compression__
    mode, expected_path = sys.argv[2:4]
    expected = module.ExchangeRecord.decode(Path(expected_path).read_bytes(), module.Protocol.JSON)
    for name in FORMATS:
        if not api.available(api.Format[name]):
            raise RuntimeError('Required Python provider is unavailable: ' + name)
    if mode == 'produce':
        directory = Path(sys.argv[4])
        for protocol in PROTOCOLS:
            for format in FORMATS:
                packed = api.encode(expected, module.Protocol[protocol], api.Format[format])
                (directory / f'python-{protocol}-{format}.bin').write_bytes(packed)
        print(json.dumps({'produced': len(PROTOCOLS) * len(FORMATS)}))
        return
    if mode != 'check':
        raise ValueError('Unknown compression qualification mode')
    jobs, corpus, output = map(Path, sys.argv[4:7])
    output.mkdir(parents=True, exist_ok=True)
    exchanges = 0
    for row in jobs.read_text(encoding='utf-8').splitlines():
        protocol, format, spelling = row.split('\t')
        actual = api.decode(module.ExchangeRecord, Path(spelling).read_bytes(),
                            module.Protocol[protocol], api.DecodeOptions(api.Format[format]))
        same_model(actual, expected)
        exchanges += 1
    decisions = 0
    for row in corpus.read_text(encoding='utf-8').splitlines():
        case, operation, format, source, oracle, wanted, input_bytes, output_bytes, window = row.split('\t')
        format = api.Format[format]
        data = Path(source).read_bytes()
        observed = 'ok'
        try:
            if operation == 'encode_bounds':
                packed = api.compress(data, format)
                exact = api.EncodeLimits(max_input_bytes=len(data), max_output_bytes=len(packed))
                if api.compress(data, format, limits=exact) != packed:
                    raise AssertionError('Exact output admission changed the frame')
                try:
                    api.compress(data, format, limits=api.EncodeLimits(
                        max_input_bytes=len(data), max_output_bytes=len(packed)-1))
                except api.Error as error:
                    if error.code != 'resource_limit':
                        raise
                else:
                    raise AssertionError('One-tight encoding output bound was accepted')
                (output / (case + '.bin')).write_bytes(packed)
            elif operation == 'encode':
                api.compress(data, format, limits=api.EncodeLimits(int(input_bytes), int(output_bytes)))
            elif operation == 'inner':
                api.decode(module.ExchangeRecord, data, module.Protocol[oracle], api.DecodeOptions(
                    format, int(input_bytes), int(output_bytes), int(window)))
            else:
                actual = api.decompress(data, api.DecodeOptions(
                    format, int(input_bytes), int(output_bytes), int(window)))
                if actual != Path(oracle).read_bytes():
                    raise AssertionError('Independent compression fixture differs')
        except api.Error as error:
            observed = error.code
        except ValueError:
            observed = 'native_error'
        if observed != wanted:
            raise AssertionError(f'{case}: expected {wanted}, observed {observed}')
        decisions += 1
    print(json.dumps({'model_exchanges': exchanges, 'corpus_decisions': decisions}))


if __name__ == '__main__':
    main()
