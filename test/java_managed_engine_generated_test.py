# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify actual generated Java native managed SDKs and strict profile selection."""
import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def run(command, success=True):
    """Bound each compiler/JVM invocation and preserve complete diagnostics on failure."""
    result = subprocess.run([str(value) for value in command], capture_output=True,
                            text=True, timeout=180)
    if (result.returncode == 0) != success:
        raise AssertionError(f'{command}\n{result.stdout}\n{result.stderr}')
    return result.stdout


def selection(args):
    """Exercise real CLI/INI activation, override precedence and rejection without overwriting outputs."""
    folder = args.build / 'selection'
    folder.mkdir(parents=True, exist_ok=True)
    schema = ROOT / 'test/resources/java_managed_engine.serializer'
    output = folder / 'EngineSdk.java'
    command = [args.compiler, '--input', schema, '--language', 'java', '--output', output]
    config = folder / 'engine.ini'
    config.write_text('[java]\nmanaged_engine=true\n', encoding='utf-8')
    run(command + ['--config', config])
    if 'public static final class ManagedStore' not in output.read_text(encoding='utf-8'):
        raise AssertionError('Java INI engine activation did not emit a native engine')
    previous = output.read_bytes()
    run(command + ['--config', config, '--java.managed_engine', 'false'], success=False)
    if output.read_bytes() != previous:
        raise AssertionError('Rejected Java engine selection overwrote the previous output')
    run(command + ['--java.managed_engine', 'maybe'], success=False)
    run(command + ['--java.managed_token_codecs', 'true'])
    text = output.read_text(encoding='utf-8')
    if 'class ManagedStore' in text or 'class ManagedReference' not in text:
        raise AssertionError('Token-only output activated a native managed engine')


def qualify(args, width, encoding, protocols):
    """Compile and execute each real generated identity/codec profile independently."""
    folder = args.build / f'uint{width}_{encoding}_{protocols}'
    folder.mkdir(parents=True, exist_ok=True)
    generated = folder / 'EngineSdk.java'
    command = [args.compiler, '--input', ROOT / 'test/resources/java_managed_engine.serializer',
         '--language', 'java', '--output', generated, '--java.managed_engine', 'true',
         '--managed.id_type', f'uint{width}', '--managed.id_encoding', encoding,
         '--protocols', protocols]
    if (width, encoding, protocols) == (32, 'compact_varint', 'all'):
        command += ['--java.stream_output', 'true', '--java.output_buffer_reuse', 'true',
                    '--java.nio_output_buffer_reuse', 'true']
    run(command)
    template = (ROOT / 'test/java_managed_engine_generated_test.java').read_text(encoding='utf-8')
    source = folder / 'JavaManagedEngineGeneratedTest.java'
    protocol = 'BINARY_NONE' if protocols == 'binary_none' else 'BINARY_INTEGER'
    source.write_text(template.replace('@ID_CAST@', '(int) ' if width == 32 else '')
                      .replace('@PROTOCOL@', protocol), encoding='utf-8')
    run([args.javac, '--release', '17', '-Xlint:all', '-Werror', '-d', folder, generated, source])
    output = run([args.java, '-cp', folder, 'JavaManagedEngineGeneratedTest', folder])
    print(f'uint{width} {encoding} {protocol}: {output.strip()}', flush=True)
    match = re.search(r'Generated native Java engine checks: (\d+)', output)
    if not match:
        raise AssertionError('Actual generated Java fixture did not report its executed checks')
    return int(match.group(1))


def main():
    """Keep generation, javac and native JVM qualification serial for low resource use."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--javac', default='javac')
    parser.add_argument('--java', default='java')
    args = parser.parse_args()
    args.build.mkdir(parents=True, exist_ok=True)
    selection(args)
    checks = 0
    for width in (32, 64):
        for encoding in ('compact_varint', 'fixed'):
            for protocols in ('all', 'binary_none'):
                checks += qualify(args, width, encoding, protocols)
    print(f'Actual generated native Java managed checks: {checks}; eight profiles')


if __name__ == '__main__':
    main()
