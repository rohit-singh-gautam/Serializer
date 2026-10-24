"""Qualify abrupt native database interruption and exact retry in fresh processes."""

import argparse
import pathlib
import subprocess
import tempfile


def run(arguments, expected=0):
    """Run a bounded helper process and retain diagnostics on an unexpected outcome."""
    result = subprocess.run(arguments, capture_output=True, text=True, timeout=60, check=False)
    if result.returncode != expected:
        raise RuntimeError(
            f"Unexpected exit {result.returncode}, expected {expected}: {arguments}\n"
            f"{result.stdout}{result.stderr}"
        )


def main():
    """Create isolated logs and bypass C++ destruction at every durable decision boundary."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--helper", required=True)
    parser.add_argument("--build", required=True)
    options = parser.parse_args()
    build = pathlib.Path(options.build)
    build.mkdir(parents=True, exist_ok=True)
    for event in ("before_write", "after_write", "after_flush", "before_publish"):
        with tempfile.TemporaryDirectory(prefix="native-database-", dir=build) as temporary:
            path = str(pathlib.Path(temporary) / "state.sdb")
            run([options.helper, "create", path])
            run([options.helper, "crash", path, event], expected=73)
            expected_revision = "0" if event == "before_write" else "1"
            run([options.helper, "recover", path, expected_revision])
            # A second recovery proves the retried/newly accepted outcome is durable too.
            run([options.helper, "recover", path, "1"])
    print("Native database subprocess crash/reopen/exact-retry matrix passed")
    for event in ("after_rebaseline_archive", "before_rebaseline_replace", "after_rebaseline_replace"):
        with tempfile.TemporaryDirectory(prefix="native-migration-", dir=build) as temporary:
            path = str(pathlib.Path(temporary) / "state.sdb")
            run([options.helper, "create", path])
            run([options.helper, "crash_migrate", path, event], expected=73)
            expected_epoch = "43" if event == "after_rebaseline_replace" else "42"
            run([options.helper, "recover_migration", path, expected_epoch])
            run([options.helper, "recover_migration", path, expected_epoch])
    print("Native migration subprocess archive/replace/reopen matrix passed")


if __name__ == "__main__":
    main()
