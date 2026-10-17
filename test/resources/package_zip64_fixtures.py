"""Regenerate independent Python zipfile ZIP64 and ZIP64 descriptor fixtures."""

import io
from pathlib import Path
import struct
import zipfile


class Nonseekable(io.BytesIO):
    """Force Python's independent writer to emit a data descriptor."""

    def seek(self, *_args):
        raise io.UnsupportedOperation("nonseekable fixture")


def archive(stream):
    """Write a small member using standard ZIP64 local size fields."""
    with zipfile.ZipFile(stream, "w") as writer:
        with writer.open("data/model.json", "w", force_zip64=True) as member:
            member.write(b'{"count":7}')
    return stream.getvalue()


def main():
    """Emit ZIP64 end-directory framing and a separate signed ZIP64 descriptor."""
    directory = Path(__file__).parent
    data = archive(io.BytesIO())
    end_offset = data.rfind(b"PK\x05\x06")
    _, _, local_count, total_count, size, offset, _ = struct.unpack_from(
        "<4H2IH", data, end_offset + 4
    )
    # Python uses a narrow directory for tiny members. Expand only the end records
    # according to APPNOTE, leaving its independently generated member bytes intact.
    extended = struct.pack(
        "<IQHHIIQQQQ", 0x06064B50, 44, 45, 45, 0, 0,
        local_count, total_count, size, offset
    )
    locator = struct.pack("<IIQI", 0x07064B50, 0, end_offset, 1)
    end = struct.pack(
        "<I4H2IH", 0x06054B50, 0, 0, 0xFFFF, 0xFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0
    )
    (directory / "package_python_zip64.zip").write_bytes(
        data[:end_offset] + extended + locator + end
    )
    (directory / "package_python_zip64_descriptor.zip").write_bytes(archive(Nonseekable()))


if __name__ == "__main__":
    main()
