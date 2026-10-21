# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Consume independently authored XSD-evolution fixtures with actual generated Python APIs."""
from pathlib import Path
import sys

from schema import EvolutionOrder, Protocol


def check(value, defaults=False):
    """Verify application defaults, every stored field and the readonly native operation."""
    assert value.id == 7 and value.has_id(7) and not value.has_id(8)
    assert value.customer == ('native default' if defaults else 'R&D')
    assert value.note == (() if defaults else ('memo',))


def main():
    """Publish results only after the complete candidate model has passed validation."""
    variant, mode, source, *destination = sys.argv[1:]
    assert variant in ('v1', 'v2')
    if mode == 'emit':
        assert not destination
        defaults = EvolutionOrder()
        check(defaults, defaults=True)
        value = EvolutionOrder()
        value.customer, value.note = 'R&D', ('memo',)
        check(value)
        document = value.encode_xml()
        check(EvolutionOrder.decode_xml(document))
        directory = Path(source)
        directory.mkdir(parents=True, exist_ok=True)
        for name, payload in (
                ('authored.xml', document), ('authored.bin', value.encode(Protocol.BINARY_NONE)),
                ('authored-integer.bin', value.encode(Protocol.BINARY_INTEGER)),
                ('authored-string.bin', value.encode(Protocol.BINARY_STRING)),
                ('default.bin', defaults.encode(Protocol.BINARY_NONE)),
                ('xml-projection.bin', value.encode(Protocol.BINARY_NONE))):
            (directory / name).write_bytes(payload)
    else:
        assert len(destination) == 1
        payload = Path(source).read_bytes()
        if mode == 'check-xml':
            value = EvolutionOrder.decode_xml(payload)
            check(value)
            result = value.encode(Protocol.BINARY_NONE)
        elif mode == 'check-native':
            value = EvolutionOrder.decode(payload, Protocol.BINARY_NONE)
            check(value)
            result = value.encode_xml()
        else:
            raise ValueError('Unknown consumer mode')
        Path(destination[0]).write_bytes(result)


if __name__ == '__main__':
    main()
