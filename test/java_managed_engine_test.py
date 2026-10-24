# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify a standalone Java native runtime adapter; this is not a generated Java SDK.

The finite fixture uses independent ordinary codecs and generated-style logical
occurrence metadata. Native full-state journal frames intentionally favor simple
independent recovery. They have explicit frame/file/count/history/retry bounds;
this profile has no compaction, rebaseline, schema migration or JVM heap cap.
"""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
TOKEN_LIMIT_BYTES = 16380


def run(command, expected=0):
    """Bound each serial JVM/compiler process and preserve the exact failure output."""
    result = subprocess.run([str(value) for value in command], capture_output=True,
                            text=True, timeout=180)
    if result.returncode != expected:
        raise AssertionError(f'{command}\n{result.stdout}\n{result.stderr}')
    return result.stdout.strip()


def main():
    """Compile the real runtime template and exercise native process-death recovery independently."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--javac', default='javac')
    parser.add_argument('--java', default='java')
    args = parser.parse_args()
    args.build.mkdir(parents=True, exist_ok=True)
    template = (ROOT / 'src/java_managed_engine_runtime.inc').read_text(encoding='utf-8')
    tokens = re.findall(r'R"javaEngine\((.*?)\)javaEngine"', template, re.S)
    if not tokens or any(len(('R"javaEngine(' + part + ')javaEngine"').encode('utf-8')) >
                         TOKEN_LIMIT_BYTES for part in tokens):
        raise AssertionError('Native Java runtime source tokens exceed the supported MSVC cap')
    fixture = (ROOT / 'test/java_managed_engine_test.java').read_text(encoding='utf-8')
    marker = '  /*SERIALIZER_MANAGED_ENGINE_RUNTIME*/'
    if fixture.count(marker) != 1:
        raise AssertionError('Standalone fixture runtime insertion boundary differs')
    source = args.build / 'JavaManagedEngineTest.java'
    source.write_text(fixture.replace(marker, ''.join(tokens)), encoding='utf-8')
    run([args.javac, '--release', '17', '-Xlint:all', '-Werror', '-d', args.build, source])
    command = [args.java, '-cp', args.build, 'JavaManagedEngineTest']
    print(run(command + [args.build]))
    for phase in ('BEFORE_WRITE', 'AFTER_LENGTH', 'AFTER_HEADER', 'AFTER_PAYLOAD',
                  'AFTER_CHECKSUM', 'AFTER_FORCE'):
        directory = Path(tempfile.mkdtemp(prefix=f'crash-{phase.lower()}-', dir=args.build))
        path = directory / 'state.srjvmg'
        run(command + ['--seed', path])
        run(command + ['--crash', path, phase], expected=75)
        run(command + ['--verify-crash', path, phase])
    print('Standalone native Java process-crash checks: 6 boundaries; OS writer locks released')
    directory = Path(tempfile.mkdtemp(prefix='native-writer-alias-', dir=args.build))
    path = directory / 'state.srjvmg'
    alias = directory / 'hard-link.srjvmg'
    ready = directory / 'writer.ready'
    run(command + ['--seed', path])
    alias.hardlink_to(path)
    holder = subprocess.Popen([str(value) for value in command + ['--hold-writer', path, ready]],
                              stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              text=True)
    try:
        deadline = time.monotonic() + 30
        while not ready.exists():
            if holder.poll() is not None or time.monotonic() >= deadline:
                raise AssertionError('Native JVM writer did not establish its held lock')
            time.sleep(0.05)
        run(command + ['--writer-locked', path])
        run(command + ['--writer-locked', alias])
    finally:
        try:
            output, errors = holder.communicate('\n', timeout=30)
        except subprocess.TimeoutExpired:
            holder.kill()
            output, errors = holder.communicate()
            raise AssertionError(f'Native JVM writer failed to stop: {output}\n{errors}')
        if holder.returncode != 0:
            raise AssertionError(f'Native JVM writer failed: {output}\n{errors}')
    run(command + ['--verify-crash', path, 'BEFORE_WRITE'])
    print('Standalone native Java writer exclusion: two separate-JVM paths, including a hard-link alias')


if __name__ == '__main__':
    main()
