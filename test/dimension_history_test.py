"""Qualify dimension containment and sidecar recovery across independent native processes."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    """Keep all mutations in a private temporary fixture directory."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--helper', required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='serializer-dimensions-') as directory:
        path = Path(directory) / 'document'
        for mode in ('create', 'budget', 'recover'):
            subprocess.run([args.helper, mode, str(path)], check=True)
        # Recovery above navigates and persists history; rebuild the expected baseline for interruption.
        with tempfile.TemporaryDirectory(prefix='serializer-dimensions-crash-') as crash_directory:
            crash_path = Path(crash_directory) / 'document'
            subprocess.run([args.helper, 'create', str(crash_path)], check=True)
            crashed = subprocess.run([args.helper, 'crash', str(crash_path)], check=False)
            if crashed.returncode != 86:
                raise RuntimeError(f'Unexpected interruption result: {crashed.returncode}')
            subprocess.run([args.helper, 'recover', str(crash_path)], check=True)
        data = bytearray(path.read_bytes())
        data[0] ^= 0xff
        path.write_bytes(data)
        subprocess.run([args.helper, 'reject', str(path)], check=True)
        malformed = Path(directory) / 'malformed'
        subprocess.run([args.helper, 'malformed', str(malformed)], check=True)
        subprocess.run([args.helper, 'reject', str(malformed)], check=True)


if __name__ == '__main__':
    main()
