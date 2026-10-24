"""Verify immutable paged-index generation publication across abrupt process interruptions."""

import argparse
from pathlib import Path
import subprocess
import tempfile


def run(arguments, expected=0):
    """Run a fresh helper process and preserve exact failing diagnostics."""
    result = subprocess.run(arguments, capture_output=True, text=True, timeout=90, check=False)
    if result.returncode != expected:
        raise RuntimeError(
            f"Expected exit {expected}, got {result.returncode}: {arguments}\n"
            f"{result.stdout}{result.stderr}"
        )


def main():
    """Ignore unselected staging files and reopen only the exact old/new source binding."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--helper", required=True)
    parser.add_argument("--build", required=True)
    options = parser.parse_args()
    build = Path(options.build)
    build.mkdir(parents=True, exist_ok=True)
    for boundary in ("before_page_write", "after_flush", "before_publish", "after_publish"):
        with tempfile.TemporaryDirectory(prefix="paged-index-crash-", dir=build) as temporary:
            path = str(Path(temporary) / "index.spi")
            run([options.helper, "create", path])
            run([options.helper, "crash", path, boundary], expected=73)
            revision = "2" if boundary == "after_publish" else "1"
            run([options.helper, "recover", path, revision])
            run([options.helper, "recover", path, revision])
    print("Paged index subprocess page/flush/replacement matrix passed")


if __name__ == "__main__":
    main()
