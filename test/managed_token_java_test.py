# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Compile actual Java token outputs, run strict checks and exchange independent bytes with C++."""
import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def run(command, success=True):
    """Bound each compiler/consumer process and retain useful diagnostics on failure."""
    result = subprocess.run([str(value) for value in command], capture_output=True, text=True, timeout=180)
    if (result.returncode == 0) != success:
        raise AssertionError(f'{command}\n{result.stdout}\n{result.stderr}')
    return result.stdout


def main():
    """Qualify both ID widths and encodings while preserving generated destinations on rejection."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--javac', default='javac')
    parser.add_argument('--java', default='java')
    parser.add_argument('--exchange', action='append', default=[])
    args = parser.parse_args()
    args.build.mkdir(parents=True, exist_ok=True)
    exchanges = {}
    for entry in args.exchange:
        width, policy, executable = entry.split(':', 2)
        exchanges[int(width), policy] = executable
    for width in (4, 8):
        for policy in ('fixed', 'compact_varint'):
            folder = args.build / f'uint{width * 8}_{policy}'
            folder.mkdir(parents=True, exist_ok=True)
            output = folder / 'Model.java'
            command = [args.compiler, '--input', ROOT / 'test/resources/managed_token_codecs.serializer',
                       '--language', 'java', '--output', output, '--java.managed_token_codecs', 'true',
                       '--managed.id_type', f'uint{width * 8}', '--managed.id_encoding', policy]
            run(command)
            run([args.javac, '--release', '17', '-Xlint:all', '-Werror', '-d', folder,
                 output, ROOT / 'test/managed_token_java_test.java'])
            print(run([args.java, '-cp', folder, 'ManagedTokenJavaTest', width,
                       str(policy == 'compact_varint').lower(), folder]).strip())
            if (width, policy) in exchanges:
                for incoming in sorted(folder.glob('corpus-*.bin')):
                    protocol = incoming.stem.split('-')[1]
                    outgoing = folder / 'cpp-exchange.bin'
                    run([exchanges[width, policy], protocol, incoming, outgoing])
                    if outgoing.read_bytes() != incoming.read_bytes():
                        raise AssertionError(f'C++ and Java wire bytes differ: {incoming}')
    sentinel = args.build / 'Preserved.java'
    sentinel.write_text('preserve this output\n', encoding='utf-8')
    command = [args.compiler, '--input', ROOT / 'test/resources/managed_token_codecs.serializer',
               '--language', 'java', '--output', sentinel]
    for settings in ([], ['--java.managed_token_codecs', 'false'],
                     ['--java.managed_token_codecs', 'invalid']):
        run(command + settings, False)
        if sentinel.read_text(encoding='utf-8') != 'preserve this output\n':
            raise AssertionError('Rejected Java output replaced the existing destination')
    configuration = args.build / 'token.ini'
    configuration.write_text('[java]\nmanaged_token_codecs=true\n[managed]\nid_type=uint64\n'
                             'id_encoding=fixed\n', encoding='utf-8')
    run(command + ['--config', configuration])
    run(command + ['--config', configuration, '--java.managed_token_codecs', 'false'], False)
    print(f'Java managed tokens: four generated profiles; {len(exchanges)} C++ exchange profiles')


if __name__ == '__main__':
    main()
