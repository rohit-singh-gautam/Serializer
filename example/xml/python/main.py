"""Use generated XML and binary codecs with the same owning Python model."""
from pathlib import Path
import sys
from xml.parsers.expat import ExpatError
from schema import Limits, Protocol, XmlExampleOrder


def rejected(data, limits=None):
    """Require malformed or over-budget input to leave no published owning value."""
    try:
        XmlExampleOrder.decode_xml(data, limits)
    except (ValueError, OverflowError, ExpatError):
        return
    raise AssertionError('Invalid XML was accepted')


def main():
    """Check a shared fixture, generated roundtrip, binary side channel, and hostile input."""
    if len(sys.argv) != 3:
        raise ValueError('Expected INPUT OUTPUT')
    source = Path(sys.argv[1])
    if source.stat().st_size > 1024 * 1024:
        raise ValueError('Example input exceeds one MiB')
    data = source.read_bytes()
    value = XmlExampleOrder.decode_xml(data)
    assert (value.id, value.customer, value.count, value.tags) == (7, 'R&D', 3, ['A', 'B'])
    canonical = value.encode(Protocol.BINARY_NONE)
    encoded = value.encode_xml()
    assert XmlExampleOrder.decode_xml(encoded).encode(Protocol.BINARY_NONE) == canonical
    assert XmlExampleOrder.decode(canonical, Protocol.BINARY_NONE).encode_xml() == encoded
    probe = b'<order xmlns="urn:serializer:xml-example" id="7"><customer>R&amp;D</customer><count>3</count><tag>A</tag><tag>B</tag></order>'
    for invalid in (
        probe.replace(b'urn:serializer:xml-example', b'urn:wrong'),
        probe.replace(b'<order ', b'<wrong ').replace(b'</order>', b'</wrong>'),
        probe.replace(b'<count>3</count>', b'<count>2147483648</count>'),
        probe.replace(b'<count>3</count>', b'<count>+-1</count>'),
        b'<!DOCTYPE order [<!ENTITY attack "bad">]><order>&attack;</order>',
        encoded + b'<extra/>',
    ):
        rejected(invalid)
    rejected(data, Limits(max_bytes=1))
    Path(sys.argv[2]).write_bytes(encoded)
    print('Generated Python XML and binary side-channel checks passed')


if __name__ == '__main__':
    main()
