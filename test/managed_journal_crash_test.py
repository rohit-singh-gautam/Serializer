"""Qualify recovery across abrupt process exits at native journal I/O boundaries."""

import argparse
from pathlib import Path
import struct
import subprocess
import tempfile


def crc64_table():
    """Build the independent ECMA lookup table from the documented polynomial."""
    values = []
    for byte in range(256):
        crc = byte << 56
        for _ in range(8):
            crc = ((crc << 1) ^ (0x42F0E1EBA9EA3693 if crc >> 63 else 0)) & ((1 << 64) - 1)
        values.append(crc)
    return tuple(values)


CRC64_TABLE = crc64_table()
DELTA_MAGIC = 0x3141544C444A5253
MAX_RECORD_BYTES = 64 * 1024 * 1024


def crc64(data, crc=0):
    """Independent CRC-64/ECMA reader; table form keeps large delta fixtures practical."""
    for byte in data:
        crc = CRC64_TABLE[(crc >> 56) ^ byte] ^ ((crc << 8) & ((1 << 64) - 1))
    return crc


def decode_delta(patch, baseline):
    """Independently expand a version-one sidecar patch against its exact immutable saved bytes."""
    assert 40 <= len(patch) <= MAX_RECORD_BYTES
    magic, base_bytes, base_digest, target_bytes, count = struct.unpack_from("<5Q", patch)
    assert (magic, base_bytes, base_digest) == (DELTA_MAGIC, len(baseline), crc64(baseline))
    assert target_bytes <= MAX_RECORD_BYTES
    assert count <= target_bytes and count <= (len(patch) - 40) // 9
    cursor = 40
    output = bytearray()

    def word():
        """Read a fixed wire integer only after checking its complete physical extent."""
        nonlocal cursor
        assert cursor <= len(patch) and len(patch) - cursor >= 8
        value, = struct.unpack_from("<Q", patch, cursor)
        cursor += 8
        return value

    for _ in range(count):
        assert cursor < len(patch)
        opcode = patch[cursor]
        cursor += 1
        if opcode == 1:
            length = word()
            assert 0 < length <= target_bytes - len(output)
            assert length <= len(patch) - cursor
            output.extend(patch[cursor:cursor + length])
            cursor += length
        else:
            assert opcode in (2, 3)
            offset, length = word(), word()
            source = baseline if opcode == 2 else output
            assert 0 < length <= target_bytes - len(output)
            assert offset <= len(source) and length <= len(source) - offset
            # Output references must be entirely produced before this instruction, without overlap.
            output.extend(source[offset:offset + length])
    assert len(output) == target_bytes and cursor == len(patch)
    return bytes(output)


def verify_container(path, mode, return_tags=False):
    """Read the documented wire layout independently of the production C++ framing implementation."""
    base = path.read_bytes()
    header = struct.unpack_from("<9Q", base)
    assert header[0] == 0x00314C4E4A5A5253
    assert header[1] in ((2,) if mode == "appended" else (2, 3))
    assert header[2:4] == (1 if mode == "appended" else 2, 1)
    assert header[8] == crc64(base[:64])
    sequence = header[6]

    def frame(data, offset, expected_sequence, previous):
        """Validate one exact frame and return its end, chain digest, and logical payload bytes."""
        length, check = struct.unpack_from("<2Q", data, offset)
        seed = crc64(struct.pack("<2Q", expected_sequence, previous))
        assert check == crc64(data[offset:offset + 8], seed)
        end = offset + 16 + length
        digest, = struct.unpack_from("<Q", data, end)
        assert digest == crc64(data[offset:end], seed)
        return end + 8, digest, data[offset + 16:end]

    offset, digest, baseline = frame(base, 72, sequence, 0)
    assert digest == header[7]
    if mode == "sidecar":
        assert offset == len(base)
        companion = Path(f"{path}.journal-{header[4]}-{header[5]}").read_bytes()
        other = struct.unpack_from("<9Q", companion)
        assert other[:3] == header[:3] and other[3] == 2 and other[4:8] == header[4:8]
        assert other[8] == crc64(companion[:64])
        base, offset = companion, 72
    tags = []
    while offset < len(base):
        sequence += 1
        offset, digest, payload = frame(base, offset, sequence, digest)
        if header[1] == 3:
            payload = decode_delta(payload, baseline)
        assert payload
        tags.append(payload[0])
    assert offset == len(base)
    return (sequence, tags) if return_tags else sequence


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
        for mode, delta in (("appended", False), ("sidecar", False),
                            ("appended", True), ("sidecar", True)):
            def run(path, operation, point="before_append", expected=0):
                """Run without a shell; an injected crash must exit with its distinctive status."""
                result = subprocess.run(
                    [args.helper, str(path), mode, operation, point] + (["--delta"] if delta else []),
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

            fixture_prefix = f"{mode}-{'delta' if delta else 'full'}"
            append_points = ("before_append", "after_header", "after_payload",
                             "after_commit", "after_flush")
            for operation in ("append", "reserve", "undo"):
                for point in append_points:
                    path = Path(temporary) / f"{fixture_prefix}-{operation}-{point}"
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
                    if delta and operation == "append" and committed:
                        assert 6 in verify_container(path, mode, return_tags=True)[1]

            save_points = ["after_base_flush", "before_replace", "after_replace",
                           "after_publish", "before_cleanup"]
            if mode == "sidecar":
                save_points.insert(0, "after_sidecar_flush")
            for point in save_points:
                path = Path(temporary) / f"{fixture_prefix}-save-{point}"
                run(path, "create")
                run(path, "edit")
                run(path, "save", point, expected=86)
                published = point in ("after_replace", "after_publish", "before_cleanup")
                inspect(path, "Changed 1 0 1" if published else "Changed 1 1 1")
                # A new write must target the selected generation, never a retired companion.
                run(path, "undo", "before_append", expected=86)
                inspect(path, "Changed 1 0 1" if published else "Changed 1 1 1")

            for point in ("before_tail_repair", "after_tail_repair"):
                path = Path(temporary) / f"{fixture_prefix}-repair-{point}"
                run(path, "create")
                run(path, "append", "after_payload", expected=86)
                run(path, "recover", point, expected=86)
                inspect(path, "Base 0 0 1")
                run(path, "edit")
                inspect(path, "Changed 1 1 1")
    print(f"{checks} crash recovery checks passed")


if __name__ == "__main__":
    main()
