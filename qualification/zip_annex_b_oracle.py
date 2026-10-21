"""Independent ECMA-376-2 fifth-edition Annex B ZIP producer and physical replay oracle."""
import argparse
import ctypes
import io
import json
from pathlib import Path
import struct
import subprocess
import zipfile
import zlib

CT = b'<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="bin" ContentType="application/octet-stream"/></Types>'


def tlv(identifier, payload):
    """Frame opaque test metadata with APPNOTE's explicit little-endian TLV layout."""
    return struct.pack('<HH', identifier, len(payload)) + payload


def archive(*, method=0, flags=0, wide=False, signed=False, opaque=b'', comment=b'entry',
            archive_comment=b'archive', extension=b'', signature=None, version=20,
            mismatch=None, opc=False, big_comment=None, entry_name='part.bin'):
    """Produce ZIP records directly; no Serializer writer participates in the fixture oracle."""
    entries = [('[Content_Types].xml', CT), (entry_name, b'Independent payload\x00\xff' * 3)] if opc else [(entry_name, b'Independent payload\x00\xff' * 3)]
    local, central = bytearray(), bytearray()
    for name, raw in entries:
        name = name.encode('ascii')
        crc = zlib.crc32(raw)
        compressor = zlib.compressobj(wbits=-15)
        data = raw if method == 0 else compressor.compress(raw) + compressor.flush()
        offset = len(local)
        needed = 45 if wide else version
        extra = (tlv(1, struct.pack('<QQ', len(raw), len(data))) if wide else b'') + opaque
        sizes = (0xffffffff, 0xffffffff) if wide else ((0, 0) if flags & 8 else (len(data), len(raw)))
        local_version = 46 if mismatch == 'version' else needed
        local_time = 0x1235 if mismatch == 'time' else 0x1234
        local_date = 0x5679 if mismatch == 'date' else 0x5678
        local += struct.pack('<IHHHHHIIIHH', 0x04034b50, local_version, flags, method,
                             local_time, local_date, 0 if flags & 8 else crc, *sizes, len(name), len(extra)) + name + extra + data
        if flags & 8:
            local += (struct.pack('<I', 0x08074b50) if signed else b'')
            local += struct.pack('<IQQ' if wide else '<III', crc, len(data), len(raw))
        central_extra = extra
        selected_comment = b'x' * big_comment if big_comment is not None else comment
        central += struct.pack('<IHHHHHHIIIHHHHHII', 0x02014b50, 0x0300 | needed,
                               needed, flags, method, 0x1234, 0x5678, crc,
                               0xffffffff if wide else len(data), 0xffffffff if wide else len(raw),
                               len(name), len(central_extra), len(selected_comment), 0, 1,
                               0x81a40000, offset) + name + central_extra + selected_comment
    if signature is not None:
        central += struct.pack('<IH', 0x05054b50, len(signature)) + signature
    result = local + central
    if wide:
        zip64_offset = len(result)
        result += struct.pack('<IQHHIIQQQQ', 0x06064b50, 44 + len(extension), 45, 45,
                              0, 0, len(entries), len(entries), len(central), len(local)) + extension
        result += struct.pack('<IIQI', 0x07064b50, 0, zip64_offset, 1)
    result += struct.pack('<IHHHHIIH', 0x06054b50, 0, 0, len(entries), len(entries),
                          len(central), len(local), len(archive_comment)) + archive_comment
    return bytes(result)


def physical(data):
    """Parse independent stable physical fields without normalizing timestamps/attributes/comments."""
    end = data.rfind(b'PK\x05\x06')
    _, _, _, count, _, size, offset, comment_size = struct.unpack_from('<IHHHHIIH', data, end)
    result = {'comment': data[end + 22:end + 22 + comment_size], 'entries': []}
    if end >= 20 and data[end - 20:end - 16] == b'PK\x06\x07':
        zip64_offset = struct.unpack_from('<Q', data, end - 12)[0]
        record_size, made, needed = struct.unpack_from('<QHH', data, zip64_offset + 4)
        result['zip64'] = (made, needed, data[zip64_offset + 56:zip64_offset + 12 + record_size])
    cursor = offset
    for _ in range(count):
        values = struct.unpack_from('<IHHHHHHIIIHHHHHII', data, cursor)
        _, made, needed, flags, method, time, date, _, _, _, name_size, extra_size, note_size, disk, internal, external, local = values
        variable = cursor + 46
        name = data[variable:variable + name_size]
        extra = data[variable + name_size:variable + name_size + extra_size]
        note = data[variable + name_size + extra_size:variable + name_size + extra_size + note_size]
        local_values = struct.unpack_from('<IHHHHHIIIHH', data, local)
        ln, le = local_values[-2:]
        local_extra = data[local + 30 + ln:local + 30 + ln + le]
        def opaque_fields(fields):
            """Exclude regenerated ZIP64 sizes/offsets, including legitimate provider recompression differences."""
            result, at = bytearray(), 0
            while at < len(fields):
                identifier, length = struct.unpack_from('<HH', fields, at)
                if identifier != 1:
                    result += fields[at:at + 4 + length]
                at += 4 + length
            return bytes(result)
        # Size/CRC/offset fields are dependent data and are deliberately regenerated on edits.
        result['entries'].append((name, made, needed, flags, method, time, date, disk,
                                  internal, external, opaque_fields(extra), note, local_values[1:6], opaque_fields(local_extra)))
        cursor += 46 + name_size + extra_size + note_size
    assert cursor <= offset + size
    return result


def checked_payloads(data):
    """Require Python's independent CRC/decompression consumer to accept every emitted payload."""
    with zipfile.ZipFile(io.BytesIO(data)) as reader:
        assert reader.testzip() is None
        return {item.filename: reader.read(item) for item in reader.infolist()}


def main():
    """Run record variants, physical replay, changed-payload CRC checks and strict invalid controls."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--stored-only', action='store_true')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    if hasattr(ctypes, 'windll'):
        ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)
    counter = 0

    def invoke(data, mode='validate', accepted=True):
        """Keep every generated case available while asserting the native process status precisely."""
        nonlocal counter
        counter += 1
        source = args.output / f'{counter:03d}.zip'
        destination = args.output / f'{counter:03d}-out.zip'
        source.write_bytes(data)
        command = [str(args.probe.resolve()), mode, str(source.resolve())]
        if mode in ('rewrite', 'edit'):
            command.append(str(destination.resolve()))
        result = subprocess.run(command, capture_output=True)
        assert (result.returncode == 0) == accepted, (counter, mode, result.returncode, result.stderr.decode(errors='replace'))
        return destination.read_bytes() if accepted and destination.exists() else None

    # Annex B.6/B.7 listed extras, including vendor-specific IDs, remain uninterpreted complete TLVs.
    opaque_ids = [0x0007, 0x0008, 0x0009, 0x000a, 0x000c, 0x000d, 0x000f,
                  0x0014, 0x0015, 0x0016, 0x0017, 0x0065, 0x0066, 0x4690,
                  0x07c8, 0x2605, 0x2705, 0x2805, 0x334d, 0x4154, 0x4341,
                  0x4453, 0x4704, 0x470f, 0x4854, 0x4b46, 0x4c41, 0x4d49,
                  0x4f4c, 0x5356, 0x5455, 0x554e, 0x5855, 0x6542, 0x756e,
                  0x7855, 0xa220, 0xbeef]
    opaque = b''.join(tlv(identifier, b'\x00opaque\xff') for identifier in opaque_ids)
    variants = 0
    for method in ([0] if args.stored_only else [0, 8]):
        for wide in (False, True):
            for descriptor in (False, True):
                for signed in ((False, True) if descriptor else (False,)):
                    for encoding in (0, 0x800):
                        flags = encoding | (8 if descriptor else 0) | (6 if method == 8 else 0)
                        data = archive(method=method, flags=flags, wide=wide, signed=signed,
                                       opaque=opaque, extension=b'opaque ZIP64 extensible data' if wide else b'')
                        expected = checked_payloads(data)
                        rewritten = invoke(data, 'rewrite')
                        assert physical(rewritten) == physical(data)
                        assert checked_payloads(rewritten) == expected
                        edited = invoke(data, 'edit')
                        # ZIP64 extra sizes depend on changed payload sizes, while opaque fields must survive.
                        before, after = physical(data), physical(edited)
                        for record in before['entries'] + after['entries']:
                            assert opaque in record[10] and opaque in record[-1]
                        assert checked_payloads(edited) == {name: payload + b'!' for name, payload in expected.items()}
                        variants += 1
    for version in (10, 11, 20, 45):
        invoke(archive(version=version), 'opc-validate', accepted=False)  # no OPC manifest
        invoke(archive(version=version, opc=True), 'opc-validate')
    # Signature record consumption is required. Its data is ignored; it is never OPC XML-signature verification.
    for signature in (b'', b'independent central-directory signature data'):
        invoke(archive(opc=True, signature=signature), 'opc-validate')
    for mismatch in ('version', 'time', 'date'):
        invoke(archive(opc=True, mismatch=mismatch), 'opc-validate', accepted=False)
    # Exact complete central-header bound, not three independent uint16 bounds.
    exact_comment = 65535 - 46 - len('[Content_Types].xml')
    invoke(archive(opc=True, big_comment=exact_comment), 'opc-validate')
    invoke(archive(opc=True, big_comment=exact_comment + 1), 'opc-validate', accepted=False)
    for flags in (1, 0x20, 0x40, 0x2000):
        invoke(archive(flags=flags), accepted=False)
    for opaque_bad in (b'\x02', b'\x02\x00\x07\x00', tlv(1, b'') + tlv(1, b'')):
        invoke(archive(opaque=opaque_bad), accepted=False)
    bad_signature = bytearray(archive(signature=b'abc'))
    at = bad_signature.index(b'PK\x05\x05')
    struct.pack_into('<H', bad_signature, at + 4, 4)
    invoke(bad_signature, accepted=False)
    summary = {'physical_replay_variants': variants, 'native_invocations': counter,
               'opaque_extra_ids': len(opaque_ids), 'independent_consumer': 'Python zipfile+zlib',
               'mandatory_signature_records': 2, 'complete_header_boundary': '65535 pass / 65536 reject'}
    (args.output / 'result.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary))


if __name__ == '__main__':
    main()
