# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Author independent signed/unsigned ZIP32/ZIP64 descriptors, including an ambiguous CRC."""
import io
from pathlib import Path
import struct
import zipfile
import zlib


def archive(payload, wide, signed, bad_crc=False):
    """Encode literal ZIP fields without using either Serializer writer or its reader."""
    name, flags, method, sentinel = b'data/model.json', 0x808, 8, 0xffffffff
    encoder = zlib.compressobj(wbits=-15)
    compressed = encoder.compress(payload) + encoder.flush()
    crc = zlib.crc32(payload)
    extra = struct.pack('<HHQQ', 1, 16, 0, 0) if wide else b''
    output = struct.pack('<4s5H3I2H', b'PK\x03\x04', 45 if wide else 20, flags, method, 0, 33,
                         0, sentinel if wide else 0, sentinel if wide else 0, len(name), len(extra))
    output += name + extra + compressed
    output += b'PK\x07\x08' if signed else b''
    output += struct.pack('<IQQ' if wide else '<III', crc ^ int(bad_crc), len(compressed), len(payload))
    offset = len(output)
    extra = struct.pack('<HHQQ', 1, 16, len(payload), len(compressed)) if wide else b''
    directory = struct.pack('<4s6H3I5H2I', b'PK\x01\x02', 45, 45 if wide else 20,
                            flags, method, 0, 33, crc, sentinel if wide else len(compressed),
                            sentinel if wide else len(payload), len(name), len(extra), 0, 0, 0, 0, 0)
    directory += name + extra
    return output + directory + struct.pack('<4s4H2IH', b'PK\x05\x06', 0, 0, 1, 1,
                                             len(directory), offset, 0)


def main():
    """Check twelve valid archives independently and retain four descriptor-only CRC mismatches."""
    directory = Path(__file__).with_suffix('')
    directory.mkdir(exist_ok=True)
    samples = {'empty': b'', 'model': b'{"count":7}', 'signature_crc': bytes.fromhex('ac0a7ad5')}
    assert zlib.crc32(samples['signature_crc']) == 0x08074b50
    for wide in (False, True):
        for signed in (False, True):
            prefix = ('zip64_' if wide else 'zip32_') + ('signed_' if signed else 'unsigned_')
            for label, payload in samples.items():
                encoded = archive(payload, wide, signed)
                with zipfile.ZipFile(io.BytesIO(encoded)) as independent:
                    assert independent.testzip() is None
                    assert independent.read('data/model.json') == payload
                (directory / (prefix + label + '.zip')).write_bytes(encoded)
            (directory / (prefix + 'bad_crc.zip')).write_bytes(archive(samples['model'], wide, signed, True))


if __name__ == '__main__':
    main()
