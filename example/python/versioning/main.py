"""Import a JSON document, edit its typed model, and round-trip four protocols."""
from pathlib import Path
import sys
from schema import ExampleModel, Protocol, Limits, ReadPolicy, Uint16Version, Uint32Version, Uint64Version, FloatVersion, DoubleVersion, Dotted2Version, Dotted3Version, Dotted4Version


def main():
    """Check complete positional bytes after each protocol round trip."""
    typed = Uint16Version()
    assert typed.encode(Protocol.BINARY_NONE) == bytes.fromhex("2c01")
    for protocol in Protocol:
        assert Uint16Version.decode(typed.encode(protocol), protocol).encode(Protocol.BINARY_NONE) == typed.encode(Protocol.BINARY_NONE)
    typed = Uint32Version()
    assert typed.encode(Protocol.BINARY_NONE) == bytes.fromhex("70110100")
    for protocol in Protocol:
        assert Uint32Version.decode(typed.encode(protocol), protocol).encode(Protocol.BINARY_NONE) == typed.encode(Protocol.BINARY_NONE)
    typed = Uint64Version()
    assert typed.encode(Protocol.BINARY_NONE) == bytes.fromhex("ffffffffffffffff")
    for protocol in Protocol:
        assert Uint64Version.decode(typed.encode(protocol), protocol).encode(Protocol.BINARY_NONE) == typed.encode(Protocol.BINARY_NONE)
    typed = FloatVersion()
    assert typed.encode(Protocol.BINARY_NONE) == bytes.fromhex("cdcccc3d")
    for protocol in Protocol:
        assert FloatVersion.decode(typed.encode(protocol), protocol).encode(Protocol.BINARY_NONE) == typed.encode(Protocol.BINARY_NONE)
    typed = DoubleVersion()
    assert typed.encode(Protocol.BINARY_NONE) == bytes.fromhex("0000000000000440")
    for protocol in Protocol:
        assert DoubleVersion.decode(typed.encode(protocol), protocol).encode(Protocol.BINARY_NONE) == typed.encode(Protocol.BINARY_NONE)
    typed = Dotted2Version()
    assert typed.encode(Protocol.BINARY_NONE) == bytes.fromhex("01000a00")
    for protocol in Protocol:
        assert Dotted2Version.decode(typed.encode(protocol), protocol).encode(Protocol.BINARY_NONE) == typed.encode(Protocol.BINARY_NONE)
    typed = Dotted3Version()
    assert typed.encode(Protocol.BINARY_NONE) == bytes.fromhex("01000a000000")
    for protocol in Protocol:
        assert Dotted3Version.decode(typed.encode(protocol), protocol).encode(Protocol.BINARY_NONE) == typed.encode(Protocol.BINARY_NONE)
    typed = Dotted4Version()
    assert typed.encode(Protocol.BINARY_NONE) == bytes.fromhex("01000a0000000400")
    for protocol in Protocol:
        assert Dotted4Version.decode(typed.encode(protocol), protocol).encode(Protocol.BINARY_NONE) == typed.encode(Protocol.BINARY_NONE)
    value = ExampleModel.decode(Path(sys.argv[1]).read_bytes(), Protocol.JSON, Limits(read_policy=ReadPolicy.COMPATIBLE))
    old = ExampleModel.decode(value.encode(Protocol.BINARY_NONE), Protocol.BINARY_NONE, Limits(read_policy=ReadPolicy.COMPATIBLE))
    assert old.version == 8 and old.old_name == "Ada"
    value.name = value.old_name
    value.version = 10
    canonical = value.encode(Protocol.BINARY_NONE)
    for protocol in Protocol:
        copy = ExampleModel.decode(value.encode(protocol), protocol)
        assert copy.encode(Protocol.BINARY_NONE) == canonical
    Path(sys.argv[2]).write_bytes(value.encode(Protocol.JSON))
    print('Version migration and four protocols passed')


if __name__ == '__main__':
    main()
