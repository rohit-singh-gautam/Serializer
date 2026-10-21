"""Share an independently authored mandatory Annex B corpus across actual generated SDK consumers."""
import importlib.util
from pathlib import Path
import struct

_source = Path(__file__).resolve().parents[1] / 'qualification/zip_annex_b_oracle.py'
_specification = importlib.util.spec_from_file_location('serializer_annex_b_oracle', _source)
_oracle = importlib.util.module_from_spec(_specification)
_specification.loader.exec_module(_oracle)
PAYLOAD = b'Independent payload\x00\xff' * 3


def provider_bytes(data):
    """Remove only the independently validated directory-signature record unsupported by Python zipfile."""
    end = data.rfind(b'PK\x05\x06')
    if end < 0:
        return data
    size, offset = struct.unpack_from('<II', data, end + 12)
    start = data.rfind(b'PK\x05\x05', offset, offset + size)
    if start >= 0 and start + 6 + struct.unpack_from('<H', data, start + 4)[0] == offset + size:
        length = offset + size - start
        result = bytearray(data[:start] + data[start + length:])
        struct.pack_into('<I', result, end - length + 12, size - length)
        return bytes(result)
    return data


def cases():
    """Return explicit structural decisions with raw ZIP framing independent of generated writers."""
    opaque = b''.join(_oracle.tlv(identifier, b'opaque') for identifier in
                      (7, 8, 9, 10, 12, 13, 15, 20, 21, 22, 23, 0x5455, 0x7855, 0xa220, 0xbeef))
    for method in (0, 8):
        for wide in (False, True):
            for flags in (0, 8, 0x800, 0x808):
                for signed in ((False, True) if flags & 8 else (False,)):
                    name = f'annex-records-{method}-{int(wide)}-{flags}-{int(signed)}'
                    yield name, _oracle.archive(method=method, flags=flags, wide=wide, signed=signed,
                                                opaque=opaque, entry_name='data.bin'), True
    for signature in (b'', b'ignored signature bytes'):
        yield f'annex-signature-{len(signature)}', _oracle.archive(signature=signature, entry_name='data.bin'), True
    for mismatch in ('version', 'time', 'date'):
        yield 'annex-conflicting-' + mismatch, _oracle.archive(mismatch=mismatch, entry_name='data.bin'), False
    for size, accepted in ((65535, True), (65536, False)):
        yield f'annex-header-{size}', _oracle.archive(big_comment=size - 46 - len('data.bin'), entry_name='data.bin'), accepted
