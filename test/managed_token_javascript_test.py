# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify actual JS/TS managed wire outputs with independent bytes and C++ exchanges."""
import argparse
import json
from pathlib import Path
import subprocess

from managed_token_codecs_test import fixture

ROOT = Path(__file__).resolve().parents[1]


def run(command, success=True):
    """Bound each compiler/SDK/exchange process and retain unexpected native diagnostics."""
    result = subprocess.run([str(value) for value in command], capture_output=True, text=True, timeout=180)
    if (result.returncode == 0) != success:
        raise AssertionError(f'{command}\n{result.stdout}\n{result.stderr}')
    return result.stdout + result.stderr


def corpus(folder, width, compact):
    """Author independent complete, malformed and incomplete metadata before loading generated code."""
    rows = []

    def add(data, protocol, valid=False, identity=None):
        """Persist one exact independent decision and its selected unsigned ID."""
        name = f'fixture-{len(rows)}.bin'
        (folder / name).write_bytes(data)
        rows.append({'file': name, 'protocol': protocol, 'valid': valid, 'id': str(identity)})

    for protocol in range(4):
        for identity in (1, 127, 128, (1 << (width * 8)) - 1):
            values = ((1 << 64) - 1, 2, 101, identity)
            add(fixture(protocol, values, width, compact), protocol, True, identity)
        for values in ((0, 0, 101, 1), (1, 2, 102, 1), (1, 2, 101, 0)):
            add(fixture(protocol, values, width, compact), protocol)
        if protocol != 1:
            values = (1, 2, 101, 1)
            for fields in ([(1, 1), (2, 2), (3, 101)], [(1, 1), (2, 2), (3, 101), (4, 1), (4, 1)]):
                add(fixture(protocol, values, width, compact, fields), protocol)
        if protocol == 0:
            add(b'{"target":{"document_high":1,"document_low":2,"class_id":101,"object_id":1,"extra":3}}', protocol)
        elif protocol == 2:
            add(b'\x07\x05\x00\x00', protocol)
        elif protocol == 3:
            add(b'\x06target\x05extra\x00\x00', protocol)
    (folder / 'manifest.json').write_text(json.dumps(rows), encoding='utf-8')


def main():
    """Run both native width/encoding SDK pairs and exact C++ exchange controls serially."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--node', default='node')
    parser.add_argument('--tsc', type=Path, required=True)
    parser.add_argument('--exchange', action='append', default=[])
    args = parser.parse_args()
    args.build.mkdir(parents=True, exist_ok=True)
    exchanges = {}
    for entry in args.exchange:
        width, policy, executable = entry.split(':', 2)
        exchanges[int(width), policy] = executable
    schema = args.build / 'tokens.serializer'
    schema.write_text((ROOT / 'test/resources/managed_token_codecs.serializer').read_text(encoding='utf-8') +
                      '\nclass token_holder stable_ids { public token_wire::envelope child (1); }\n', encoding='utf-8')
    for width in (4, 8):
        for policy in ('fixed', 'compact_varint'):
            folder = args.build / f'uint{width * 8}_{policy}'
            folder.mkdir(parents=True, exist_ok=True)
            common = [args.compiler, '--input', schema, '--managed.id_type', f'uint{width * 8}', '--managed.id_encoding', policy]
            for language, filename in (('js', 'model.mjs'), ('typescript', 'model.d.mts')):
                run(common + ['--language', language, '--output', folder / filename, f'--{language}.managed_token_codecs', 'true'])
            corpus(folder, width, policy == 'compact_varint')
            source = folder / 'runner.mjs'
            source.write_text((ROOT / 'test/managed_token_javascript_test.mjs').read_text(encoding='utf-8'), encoding='utf-8')
            print(f'uint{width * 8} {policy}: ' + run([args.node, '--unhandled-rejections=strict', source, folder, width]).strip(), flush=True)
            native = 'number' if width == 4 else 'bigint'
            identity = '1' if width == 4 else '1n'
            typed = folder / 'typed.mts'
            typed.write_text(f'''import {{ManagedReference, TokenWireItem, TokenWireOther, TokenWireEnvelope, Protocol}} from './model.mjs';
const reference: ManagedReference<TokenWireItem, {native}> = new ManagedReference(1n, 2n, 101n, {identity});
const value = new TokenWireEnvelope(); value.target = reference;
if (TokenWireEnvelope.decode(value.encode(Protocol.BINARY_INTEGER), Protocol.BINARY_INTEGER).target.objectId !== {identity}) throw new Error('Typed wire round trip');
''', encoding='utf-8')
            tsc = [args.node, args.tsc, '--strict', '--target', 'ES2022', '--module', 'NodeNext', '--moduleResolution', 'NodeNext']
            run(tsc + [typed]); run([args.node, folder / 'typed.mjs'])
            negative = folder / 'negative.mts'
            negative.write_text(typed.read_text(encoding='utf-8') + f'const wrong: ManagedReference<TokenWireOther, {native}> = reference;\n', encoding='utf-8')
            if 'TS2322' not in run(tsc + ['--noEmit', negative], False):
                raise AssertionError('Typed wire target mismatch did not produce TS2322')
            if (width, policy) in exchanges:
                for incoming in sorted(folder.glob('corpus-*.bin')):
                    protocol = incoming.stem.split('-')[1]
                    outgoing = folder / 'cpp-exchange.bin'
                    run([exchanges[width, policy], protocol, incoming, outgoing])
                    if outgoing.read_bytes() != incoming.read_bytes():
                        raise AssertionError(f'C++ and JS wire bytes differ: {incoming}')
    print(f'Actual JavaScript/TypeScript managed wire profiles: four; C++ exchange profiles: {len(exchanges)}')


if __name__ == '__main__':
    main()
