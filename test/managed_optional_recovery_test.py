"""Qualify standard optional history and identity recovery across fresh native processes."""

import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    """Create isolated appended/sidecar journals and recover each in a separate process."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--helper", required=True)
    parser.add_argument("--directory", required=True)
    args = parser.parse_args()
    root = Path(args.directory)
    root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="optional-", dir=root) as directory:
        for mode in ("appended", "sidecar"):
            path = Path(directory) / mode
            for operation in ("create", "recover"):
                subprocess.run([args.helper, str(path), mode, operation], check=True, timeout=30)


if __name__ == "__main__":
    main()
