# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Replay the selected shared inventory through actual generated Python models."""
import importlib.util
import json
from pathlib import Path
import sys


def main():
    """Require every selected frame/limit decision and exact owning model exchange."""
    spec = importlib.util.spec_from_file_location('shared_model', Path(__file__).with_name('schema.py'))
    model = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = model
    spec.loader.exec_module(model)
    c = model.__serializer_compression__
    mode, directory = sys.argv[1], Path(sys.argv[2])
    formats = tuple(c.Format)
    protocols = tuple(model.Protocol)
    if not all(c.available(format) for format in formats):
        raise RuntimeError('Missing mandatory Python provider')
    if mode == 'produce':
        value = model.AccountRecord()
        value.account_id = 42
        value.label = 'shared \u03bb \U0001f642'
        for protocol in protocols:
            plain = value.encode(protocol)
            for format in formats:
                frame = c.encode(value, protocol, format,
                                 level=None if format == c.Format.NONE else 0 if format == c.Format.LZ4 else 3)
                level = None if format == c.Format.NONE else 0 if format == c.Format.LZ4 else 3
                assert c.encode(value, protocol, format, level=level,
                                limits=c.EncodeLimits(len(plain), len(frame))) == frame
                for input_cap, output_cap in ((len(plain)-1, len(frame)), (len(plain), len(frame)-1)):
                    try:
                        c.encode(value, protocol, format, level=level, limits=c.EncodeLimits(input_cap, output_cap))
                        raise AssertionError('One-tight encoding limit accepted')
                    except c.Error as failure:
                        assert failure.code == 'resource_limit'
                stem = f'python-{protocol.name}-{format.name}'
                (directory / (stem + '.frame')).write_bytes(frame)
                (directory / (stem + '.plain')).write_bytes(plain)
        print(json.dumps({'produced': 24, 'encode_bounds': 72}))
        return
    decisions = exchanges = 0
    for line in (directory / 'cases.tsv').read_text(encoding='utf-8').splitlines():
        case, format, source, oracle, wanted, max_input, max_output, window = line.split('\t')
        observed = 'ok'
        try:
            actual = c.decompress((directory/source).read_bytes(), c.DecodeOptions(
                c.Format[format], int(max_input), int(max_output), int(window)))
            if wanted == "ok":
                assert actual == (directory/oracle).read_bytes(), case
        except c.Error as failure:
            observed = failure.code
        assert observed == wanted, (case, observed, wanted)
        decisions += 1
    for line in (directory / 'exchange.tsv').read_text(encoding='utf-8').splitlines():
        case, protocol, format, frame, plain = line.split('\t')
        value = c.decode(model.AccountRecord, (directory/frame).read_bytes(), model.Protocol[protocol],
                         c.DecodeOptions(c.Format[format]))
        assert value.account_id == 42 and value.label == 'shared \u03bb \U0001f642', case
        assert c.decompress((directory/frame).read_bytes(), c.DecodeOptions(c.Format[format])) == (directory/plain).read_bytes(), case
        exchanges += 1
    print(json.dumps({'decisions': decisions, 'exchanges': exchanges}))


if __name__ == '__main__':
    main()
