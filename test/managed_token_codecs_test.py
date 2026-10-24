# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Exercise explicit Python managed wire records, independent bytes and C++ exchanges."""
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
NAMES = ('document_high', 'document_low', 'class_id', 'object_id')
CHECKS = 0


def check(condition):
    """Count checks and keep assertions active under Python optimization."""
    global CHECKS
    CHECKS += 1
    if not condition:
        raise AssertionError(f'Managed token check {CHECKS} failed')


def reject(action):
    """Require a documented data/resource failure rather than accepting malformed input."""
    try:
        action()
    except (ValueError, UnicodeError, OverflowError):
        check(True)
        return
    raise AssertionError('Expected token rejection')


def invoke(command, success=True):
    """Run the actual compiler or independently compiled consumer with bounded execution."""
    result = subprocess.run([str(value) for value in command], capture_output=True, text=True, timeout=180)
    check((result.returncode == 0) == success)
    if success and result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    return result


def varint(value):
    """Construct canonical unsigned base-128 bytes independently of generated support."""
    output = bytearray()
    while value >= 128:
        output.append((value & 127) | 128)
        value >>= 7
    output.append(value)
    return bytes(output)


def fixture(protocol, values, width, compact, metadata=None):
    """Build independent four-field envelope bytes with short native keys."""
    fields = list(enumerate(values, 1)) if metadata is None else metadata
    if protocol == 0:
        body = ','.join(json.dumps(NAMES[index - 1]) + ':' + str(value) for index, value in fields)
        return ('{"target":{' + body + '}}').encode()
    body = bytearray()
    for index, value in fields:
        if protocol == 2:
            body.append(index)
        elif protocol == 3:
            name = NAMES[index - 1].encode()
            body.extend(bytes((len(name),)) + name)
        body.extend(varint(value) if index == 4 and compact else value.to_bytes(width if index == 4 else 8, 'little'))
    if protocol == 1:
        return bytes(body)
    return (b'\x07' if protocol == 2 else b'\x06target') + bytes(body) + b'\0\0'


def qualify(args, width, policy, exchange):
    """Generate one native ID profile and qualify every supported field wrapper and protocol."""
    folder = args.build / f'uint{width * 8}_{policy}'
    folder.mkdir(parents=True, exist_ok=True)
    output = folder / 'model.py'
    command = [args.compiler, '--input', ROOT / 'test/resources/managed_token_codecs.serializer',
               '--language', 'python', '--output', output, '--python.managed_token_codecs', 'true',
               '--managed.id_type', f'uint{width * 8}', '--managed.id_encoding', policy]
    invoke(command)
    spec = importlib.util.spec_from_file_location(f'token_wire_{width}_{policy}', output)
    model = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(model)
    compact = policy == 'compact_varint'
    check(model.TokenWireItem.managed_class_id == 101)
    for protocol in model.Protocol:
        for object_id in (1, 127, 128, (1 << (width * 8)) - 1):
            values = ((1 << 64) - 1, 2, 101, object_id)
            token = model.ManagedReference(*values)
            envelope = model.TokenWireEnvelope()
            envelope.target = token
            expected = fixture(protocol, values, width, compact)
            encoded = envelope.encode(protocol)
            check(encoded == expected)
            decoded = model.TokenWireEnvelope.decode(expected, protocol)
            check(tuple(getattr(decoded.target, name) for name in NAMES) == values)
            check(decoded.encode(protocol) == expected)
            if exchange:
                incoming, outgoing = folder / 'input.bin', folder / 'output.bin'
                incoming.write_bytes(encoded)
                invoke([exchange, int(protocol), incoming, outgoing])
                check(outgoing.read_bytes() == expected)
                check(model.TokenWireEnvelope.decode(outgoing.read_bytes(), protocol).encode(protocol) == expected)
            for end in range(len(expected)):
                reject(lambda: model.TokenWireEnvelope.decode(expected[:end], protocol))
            reject(lambda: model.TokenWireEnvelope.decode(expected + b'!', protocol))
            wrapped = model.TokenWireContainers()
            wrapped.selected, wrapped.links, wrapped.lookup = (token,), [token, token], {7: token}
            restored = model.TokenWireContainers.decode(wrapped.encode(protocol), protocol)
            check(restored.selected[0].object_id == object_id)
            check(restored.links[1].class_id == 101 and restored.lookup[7].document_high == values[0])
            for invalid in ((0, 0, 101, 1), (1, 0, 102, 1), (1, 0, 101, 0)):
                reject(lambda: model.TokenWireEnvelope.decode(fixture(protocol, invalid, width, compact), protocol))
                envelope.target = model.ManagedReference(*invalid)
                reject(lambda: envelope.encode(protocol))
            for invalid_id in (-1, 1 << (width * 8), True):
                envelope.target = model.ManagedReference(1, 0, 101, invalid_id)
                reject(lambda: envelope.encode(protocol))
            if protocol != 1:
                fields = list(enumerate(values, 1))
                for malformed in (fields[:-1], fields + [fields[0]], list(reversed(fields)) + [fields[-1]]):
                    reject(lambda: model.TokenWireEnvelope.decode(fixture(protocol, values, width, compact, malformed), protocol))
        reject(lambda: model.TokenWireEnvelope.decode(expected, protocol, model.Limits(max_bytes=0)))
        reject(lambda: model.TokenWireEnvelope.decode(expected, protocol, model.Limits(max_depth=1)))
    # Self tokens remain phantom edges; neither default construction nor decoding follows a target.
    item = model.TokenWireItem()
    item.persistent_id = 42
    item.next = (model.ManagedReference(1, 2, 101, 42),)
    for protocol in model.Protocol:
        restored = model.TokenWireItem.decode(item.encode(protocol), protocol)
        check(restored.persistent_id == 42 and restored.next[0].object_id == 42)


def main():
    """Run every ID representation and ensure opt-out/invalid settings preserve existing files."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--exchange', action='append', default=[], help='width:policy:executable')
    args = parser.parse_args()
    args.build.mkdir(parents=True, exist_ok=True)
    exchanges = {}
    for entry in args.exchange:
        width, policy, executable = entry.split(':', 2)
        exchanges[int(width), policy] = executable
    for width in (4, 8):
        for policy in ('fixed', 'compact_varint'):
            qualify(args, width, policy, exchanges.get((width, policy)))
    sentinel = args.build / 'preserved.py'
    sentinel.write_text('preserve this output\n', encoding='utf-8')
    base = [args.compiler, '--input', ROOT / 'test/resources/managed_token_codecs.serializer',
            '--language', 'python', '--output', sentinel]
    for settings in ([], ['--python.managed_token_codecs', 'false'], ['--python.managed_token_codecs', 'invalid']):
        invoke(base + settings, False)
        check(sentinel.read_text(encoding='utf-8') == 'preserve this output\n')
    config = args.build / 'token.ini'
    config.write_text('[python]\nmanaged_token_codecs = true\n[managed]\nid_type = uint64\nid_encoding = fixed\n', encoding='utf-8')
    invoke(base + ['--config', config])
    check('ManagedReference._read(input, 101, 8, False)' in sentinel.read_text(encoding='utf-8'))
    invoke(base + ['--config', config, '--python.managed_token_codecs', 'false'], False)
    print(f'Managed token codecs: {CHECKS} checks; {len(exchanges)} native C++ exchange profiles')


if __name__ == '__main__':
    main()
