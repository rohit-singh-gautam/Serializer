"""Qualify recovery across abrupt process exits at native journal I/O boundaries."""

import argparse
from pathlib import Path
import struct
import subprocess
import tempfile


def crc64(data, crc=0):
    """Independent bitwise CRC-64/ECMA reader, checked against the standard check vector."""
    for byte in data:
        crc ^= byte << 56
        for _ in range(8):
            crc = ((crc << 1) ^ (0x42F0E1EBA9EA3693 if crc >> 63 else 0)) & ((1 << 64) - 1)
    return crc


def verify_container(path, mode):
    """Read the documented wire layout independently of the production C++ framing implementation."""
    base = path.read_bytes()
    header = struct.unpack_from("<9Q", base)
    assert header[:4] == (0x00314C4E4A5A5253, 2, 1 if mode == "appended" else 2, 1)
    assert header[8] == crc64(base[:64])
    sequence = header[6]

    def frame(data, offset, expected_sequence, previous):
        """Validate one exact frame and return its end and chain digest."""
        length, check = struct.unpack_from("<2Q", data, offset)
        seed = crc64(struct.pack("<2Q", expected_sequence, previous))
        assert check == crc64(data[offset:offset + 8], seed)
        end = offset + 16 + length
        digest, = struct.unpack_from("<Q", data, end)
        assert digest == crc64(data[offset:end], seed)
        return end + 8, digest

    offset, digest = frame(base, 72, sequence, 0)
    assert digest == header[7]
    if mode == "sidecar":
        assert offset == len(base)
        companion = Path(f"{path}.journal-{header[4]}-{header[5]}").read_bytes()
        other = struct.unpack_from("<9Q", companion)
        assert other[:3] == header[:3] and other[3] == 2 and other[4:8] == header[4:8]
        assert other[8] == crc64(companion[:64])
        base, offset = companion, 72
    while offset < len(base):
        sequence += 1
        offset, digest = frame(base, offset, sequence, digest)
    assert offset == len(base)
    return sequence


def main():
    """Create isolated fixtures and verify the selected durable decision after every interruption."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--helper", required=True)
    parser.add_argument("--directory", required=True)
    args = parser.parse_args()
    root = Path(args.directory)
    root.mkdir(parents=True, exist_ok=True)
    assert crc64(b"123456789") == 0x6C40DF5F0B497347
    checks = 0
    with tempfile.TemporaryDirectory(prefix="crash-", dir=root) as temporary:
        for mode in ("appended", "sidecar"):
            def run(path, operation, point="before_append", expected=0):
                """Run without a shell; an injected crash must exit with its distinctive status."""
                result = subprocess.run(
                    [args.helper, str(path), mode, operation, point],
                    capture_output=True, text=True, timeout=30, check=False)
                assert result.returncode == expected, (operation, point, result.returncode,
                                                       result.stdout, result.stderr)
                return result.stdout.strip()

            def inspect(path, expected):
                """Repeated recovery must not replay the same operation twice."""
                nonlocal checks
                for _ in range(2):
                    assert run(path, "inspect") == expected, (path, expected)
                    assert verify_container(path, mode) == int(expected.split()[1])
                    checks += 1

            append_points = ("before_append", "after_header", "after_payload",
                             "after_commit", "after_flush")
            for operation in ("append", "reserve", "undo"):
                for point in append_points:
                    path = Path(temporary) / f"{mode}-{operation}-{point}"
                    run(path, "create")
                    if operation == "undo":
                        run(path, "edit")
                    run(path, operation, point, expected=86)
                    committed = point in ("after_commit", "after_flush")
                    if operation == "append":
                        expected = "Changed 1 1 1" if committed else "Base 0 0 1"
                    elif operation == "reserve":
                        expected = "Base 1 0 2" if committed else "Base 0 0 1"
                    else:
                        expected = "Base 2 0 1" if committed else "Changed 1 1 1"
                    inspect(path, expected)

            save_points = ["after_base_flush", "before_replace", "after_replace",
                           "after_publish", "before_cleanup"]
            if mode == "sidecar":
                save_points.insert(0, "after_sidecar_flush")
            for point in save_points:
                path = Path(temporary) / f"{mode}-save-{point}"
                run(path, "create")
                run(path, "edit")
                run(path, "save", point, expected=86)
                published = point in ("after_replace", "after_publish", "before_cleanup")
                inspect(path, "Changed 1 0 1" if published else "Changed 1 1 1")
                # A new write must target the selected generation, never a retired companion.
                run(path, "undo", "before_append", expected=86)
                inspect(path, "Changed 1 0 1" if published else "Changed 1 1 1")

            for point in ("before_tail_repair", "after_tail_repair"):
                path = Path(temporary) / f"{mode}-repair-{point}"
                run(path, "create")
                run(path, "append", "after_payload", expected=86)
                run(path, "recover", point, expected=86)
                inspect(path, "Base 0 0 1")
                run(path, "edit")
                inspect(path, "Changed 1 1 1")
    print(f"{checks} crash recovery checks passed")


if __name__ == "__main__":
    main()
