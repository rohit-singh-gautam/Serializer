"""Check Serializer adapters against standalone zstd/LZ4 and Python zlib/gzip."""

import argparse
import gzip
from pathlib import Path
import random
import subprocess
import tempfile
import zlib


def run():
    """Exercise empty, expanding, and multi-chunk streams in both directions."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--helper", required=True)
    parser.add_argument("--format", required=True)
    parser.add_argument("--tool")
    arguments = parser.parse_args()
    generator = random.Random(12345)
    cases = [b"", b"Serializer compression fixture\n", b"abc" * 100000,
             generator.randbytes(200000)]
    with tempfile.TemporaryDirectory(prefix="serializer-compression-") as directory:
        root = Path(directory)
        original, encoded, decoded = [root / name for name in ("input", "encoded", "decoded")]
        for content in cases:
            original.write_bytes(content)
            subprocess.run([arguments.helper, "encode", arguments.format, original, encoded], check=True)
            if arguments.tool:
                external = subprocess.run([arguments.tool, "-d", "-c", str(encoded)],
                                          check=True, stdout=subprocess.PIPE).stdout
            elif arguments.format == "gzip":
                external = gzip.decompress(encoded.read_bytes())
            else:
                bits = -15 if arguments.format == "deflate" else 15
                external = zlib.decompress(encoded.read_bytes(), bits)
            assert external == content, "External decoder disagrees with Serializer output"
            if arguments.tool:
                encoded.write_bytes(subprocess.run([arguments.tool, "-c", str(original)],
                                                   check=True, stdout=subprocess.PIPE).stdout)
            elif arguments.format == "gzip":
                encoded.write_bytes(gzip.compress(content, mtime=0))
            else:
                bits = -15 if arguments.format == "deflate" else 15
                encoder = zlib.compressobj(wbits=bits)
                encoded.write_bytes(encoder.compress(content) + encoder.flush())
            subprocess.run([arguments.helper, "decode", arguments.format, encoded, decoded], check=True)
            assert decoded.read_bytes() == content, "Serializer disagrees with external encoder"
    print(f"Passed 8 independent {arguments.format} interoperability checks")


if __name__ == "__main__":
    run()
