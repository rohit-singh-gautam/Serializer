"""Check native OPC metadata against independent fifth-edition MCE reference-model decisions."""
import argparse
import ctypes
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'test'))
import package_mce_corpus
import package_opc_uri_corpus
import package_iri_corpus
import package_utf16_corpus
import python_package_test as independent


def main():
    """Retain original input bytes and require the public reader's exact acceptance decision."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--corpus', choices=('mce','uri','iri','utf16'), default='mce')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    if hasattr(ctypes, 'windll'):
        ctypes.windll.kernel32.SetErrorMode(0x8003)
    count = 0
    selected = {'mce':package_mce_corpus,'uri':package_opc_uri_corpus,'iri':package_iri_corpus}.get(args.corpus)
    rows = selected.cases() if selected else ((name,independent.independent_zip(parts),expected) for name,parts,expected in package_utf16_corpus.cases(independent.independent_parts()))
    for name, data, expected in rows:
        path = args.output / (name + '.opc'); path.write_bytes(data)
        result = subprocess.run([str(args.probe.resolve()), 'opc-validate', str(path.resolve())], capture_output=True)
        assert (result.returncode == 0) == expected, (name, result.returncode, result.stderr.decode(errors='replace'))
        count += 1
    print(f'PASS: {count} independent fifth-edition {args.corpus} decisions')


if __name__ == '__main__':
    main()
