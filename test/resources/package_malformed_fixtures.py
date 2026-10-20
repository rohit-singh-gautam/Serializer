# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Regenerate independent, small ZIP32 offset and exact-DEFLATE-framing fixtures."""
import io
from pathlib import Path
import struct
import zipfile


DIRECTORY = Path(__file__).with_suffix('')
MODEL_NAME = 'Alternate/root.xml'


def independent_archive():
    """Use zipfile for a complete valid OPC control with one DEFLATE model and stored metadata."""
    parts = {
        MODEL_NAME: b'<model xmlns="urn:serializer:package-test" name="independent"><point x="1" y="2" z="3"/></model>',
        'Textures/raw.bin': b'\x00\x01\xfe\xff',
        '_rels/.rels': b'<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            b'<Relationship Id="main" Type="urn:serializer:main-model" Target="Alternate/root.xml"/></Relationships>',
        'Alternate/_rels/root.xml.rels': b'<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            b'<Relationship Id="texture" Type="urn:serializer:attachment" Target="../Textures/raw.bin"/></Relationships>',
        '[Content_Types].xml': b'<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
            b'<Default Extension="bin" ContentType="application/octet-stream"/>'
            b'<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
            b'<Override PartName="/Alternate/root.xml" ContentType="application/vnd.serializer.test-model+xml"/></Types>',
    }
    output = io.BytesIO()
    with zipfile.ZipFile(output, 'w') as archive:
        for name, data in parts.items():
            info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            info.create_system, info.external_attr = 0, 0x20
            info.compress_type = zipfile.ZIP_DEFLATED if name == MODEL_NAME else zipfile.ZIP_STORED
            archive.writestr(info, data)
    data = output.getvalue()
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        assert archive.testzip() is None
        assert {name: archive.read(name) for name in archive.namelist()} == parts
    return data


def structure(data):
    """Read independent ZIP32 positions without scanning compressed bytes for header signatures."""
    end = len(data) - 22
    assert data[end:end + 4] == b'PK\x05\x06'
    count, size, start = struct.unpack_from('<HII', data, end + 10)
    entries, position = {}, start
    for _ in range(count):
        assert data[position:position + 4] == b'PK\x01\x02'
        name_size, extra_size, comment_size = struct.unpack_from('<3H', data, position + 28)
        name = data[position + 46:position + 46 + name_size].decode('ascii')
        entries[name] = position
        position += 46 + name_size + extra_size + comment_size
    assert position == start + size == end
    return end, start, entries


def replace_model_payload(original, replacement):
    """Repair every affected ZIP size/offset while changing only the model's DEFLATE byte sequence."""
    end, start, entries = structure(original)
    central = entries[MODEL_NAME]
    compressed = struct.unpack_from('<I', original, central + 20)[0]
    local = struct.unpack_from('<I', original, central + 42)[0]
    name_size, extra_size = struct.unpack_from('<2H', original, local + 26)
    begin = local + 30 + name_size + extra_size
    after = begin + compressed
    delta = len(replacement) - compressed
    changed = bytearray(original[:begin] + replacement + original[after:])
    struct.pack_into('<I', changed, local + 18, len(replacement))
    struct.pack_into('<I', changed, central + delta + 20, len(replacement))
    for position in entries.values():
        offset = struct.unpack_from('<I', original, position + 42)[0]
        struct.pack_into('<I', changed, position + delta + 42, offset + (delta if offset >= after else 0))
    struct.pack_into('<I', changed, end + delta + 16, start + delta)
    structure(changed)
    return bytes(changed)


def main():
    """Write one valid control and ten named mutations consumed unchanged by C++ and Python."""
    DIRECTORY.mkdir(exist_ok=True)
    original = independent_archive()
    end, start, entries = structure(original)
    central = entries[MODEL_NAME]
    local = struct.unpack_from('<I', original, central + 42)[0]
    cases = []

    def changed(name, fields):
        """Mutate only declared fixed-width ZIP fields, preserving all independent payload bytes."""
        data = bytearray(original)
        for pattern, offset, value in fields:
            struct.pack_into(pattern, data, offset, value)
        cases.append(('index', name, bytes(data)))

    changed('directory_offset_outside', [('<I', end + 16, len(original) + 1)])
    directory_size = struct.unpack_from('<I', original, end + 12)[0]
    changed('directory_size_crosses_end', [('<I', end + 12, directory_size + 1)])
    changed('local_header_in_directory', [('<I', central + 42, start)])
    changed('local_name_length_overflow', [('<H', local + 26, 0xffff)])
    changed('central_extra_length_overflow', [('<H', central + 30, 0xffff)])
    count = len(entries) + 1
    changed('directory_entry_count_mismatch', [('<H', end + 8, count), ('<H', end + 10, count)])

    for name, local_field, central_field in (('crc_mismatch', 14, 16), ('expanded_size_mismatch', 22, 24)):
        data = bytearray(original)
        value = struct.unpack_from('<I', original, local + local_field)[0] + 1
        struct.pack_into('<I', data, local + local_field, value)
        struct.pack_into('<I', data, central + central_field, value)
        cases.append(('integrity', name, bytes(data)))
    name_size, extra_size = struct.unpack_from('<2H', original, local + 26)
    begin = local + 30 + name_size + extra_size
    compressed = struct.unpack_from('<I', original, local + 18)[0]
    payload = original[begin:begin + compressed]
    cases.append(('integrity', 'deflate_trailing_data', replace_model_payload(original, payload + b'trailing')))
    cases.append(('integrity', 'deflate_truncated', replace_model_payload(original, payload[:-1])))

    (DIRECTORY / 'valid.zip').write_bytes(original)
    for _, name, data in cases:
        (DIRECTORY / (name + '.zip')).write_bytes(data)
    (DIRECTORY / 'cases.txt').write_text(''.join(stage + ' ' + name + '.zip\n' for stage, name, _ in cases), encoding='ascii')
    print(f'Wrote {len(cases)} malformed fixtures and one independently validated control to {DIRECTORY}')


if __name__ == '__main__':
    main()
