"""Independently qualify canonical ZIP bytes and ZIP64/OPC standard framing."""

import binascii
import io
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as xml
import zipfile


def independent_stored_zip(parts):
    """Emit the documented fixed-metadata ZIP policy without using Serializer."""
    output = io.BytesIO()
    directory = []
    for name, payload in sorted(parts.items()):
        encoded = name.encode("utf-8")
        checksum = binascii.crc32(payload)
        offset = output.tell()
        output.write(struct.pack(
            "<I5H3I2H", 0x04034B50, 20, 0x800, 0, 0, 0x21,
            checksum, len(payload), len(payload), len(encoded), 0
        ))
        output.write(encoded)
        output.write(payload)
        directory.append(struct.pack(
            "<I6H3I5H2I", 0x02014B50, 20, 20, 0x800, 0, 0, 0x21,
            checksum, len(payload), len(payload), len(encoded), 0, 0, 0, 0, 0, offset
        ) + encoded)
    offset = output.tell()
    for record in directory:
        output.write(record)
    size = output.tell() - offset
    output.write(struct.pack(
        "<I4H2IH", 0x06054B50, 0, 0, len(directory), len(directory), size, offset, 0
    ))
    return output.getvalue()


def main():
    """Run the native producer and check it with independent Python implementations."""
    with tempfile.TemporaryDirectory(prefix="serializer-package-interop-") as temporary:
        directory = Path(temporary)
        subprocess.run([sys.argv[1], str(directory)], check=True)
        parts = {"a.json": b'{"count":7}', "z.bin": b"a\0b"}
        first = (directory / "canonical-first.zip").read_bytes()
        assert first == (directory / "canonical-second.zip").read_bytes()
        assert first == independent_stored_zip(parts)
        for name in ("canonical-first.zip", "canonical-second.zip", "forced-zip64.zip"):
            with zipfile.ZipFile(directory / name) as archive:
                assert archive.testzip() is None
                assert {name: archive.read(name) for name in archive.namelist()} == parts
                for item in archive.infolist():
                    assert item.date_time == (1980, 1, 1, 0, 0, 0)
                    assert item.external_attr == 0
        first_opc = directory / "canonical-first.opc"
        if first_opc.exists():
            assert first_opc.read_bytes() == (directory / "canonical-second.opc").read_bytes()
            with zipfile.ZipFile(first_opc) as archive:
                assert archive.testzip() is None
                manifest = xml.fromstring(archive.read("[Content_Types].xml"))
                assert manifest.tag == "{http://schemas.openxmlformats.org/package/2006/content-types}Types"
                assert {item.attrib["PartName"] for item in manifest} == {
                    "/a.json", "/z.bin", "/_rels/.rels", "/_rels/a.json.rels"
                }
                root_links = xml.fromstring(archive.read("_rels/.rels"))
                assert root_links[0].attrib == {
                    "Id": "main", "Type": "urn:test:main", "Target": "a.json"
                }
                for name, payload in parts.items():
                    assert archive.read(name) == payload
        print("Independent canonical ZIP bytes, reordered output, ZIP64 and OPC passed")


if __name__ == "__main__":
    main()
