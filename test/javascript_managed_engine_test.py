# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify the independent JS kernel with real Node processes, not a generated SDK.

The native journal profile currently targets a local Windows drive and cooperating
Node writers. It uses machine-local OS pipe leases plus native file identity;
The drive-letter gate is syntactic: mapped drives are not a volume-type probe.
Network/distributed exclusion, rebaseline, migration and a JS heap ceiling are not
part of this finite prototype. TypeScript declarations remain a later SDK step.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
TOKEN_LIMIT_BYTES = 16380


def run(command):
    """Run one bounded SDK process and retain the exact implementation failure."""
    result = subprocess.run([str(value) for value in command], capture_output=True,
                            text=True, timeout=120)
    if result.returncode:
        raise AssertionError(f'{command}\n{result.stdout}\n{result.stderr}')
    if result.stderr:
        print(result.stderr.strip())
    return result.stdout.strip()


def stopped_child(command, ready, action):
    """Wait for an observable owned lock/boundary, run the contender, then kill externally."""
    child = subprocess.Popen([str(value) for value in command], stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE, text=True)
    try:
        deadline = time.monotonic() + 30
        while not ready.exists():
            if child.poll() is not None:
                output, error = child.communicate()
                raise AssertionError(f'Child exited before ready: {command}\n{output}\n{error}')
            if time.monotonic() > deadline:
                raise AssertionError(f'Child did not reach boundary: {command}')
            time.sleep(0.025)
        action()
    finally:
        if child.poll() is None:
            child.kill()
        output, error = child.communicate(timeout=15)
        if output:
            print(output.strip())
        if error:
            print(error.strip())


def main():
    """Extract the production template and qualify finite contracts serially under the actual SDK."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--node', default='node')
    args = parser.parse_args()
    args.build.mkdir(parents=True, exist_ok=True)
    template = (ROOT / 'src/javascript_managed_engine_runtime.inc').read_text(encoding='utf-8')
    tokens = re.findall(r'R"jsManaged\((.*?)\)jsManaged"', template, re.S)
    if not tokens or any(len(('R"jsManaged(' + part + ')jsManaged"').encode('utf-8')) >
                         TOKEN_LIMIT_BYTES for part in tokens):
        raise AssertionError('JS runtime literal exceeds the supported MSVC source-token limit')
    runtime = ''.join(tokens)
    fixture = (ROOT / 'test/javascript_managed_engine_test.mjs').read_text(encoding='utf-8')
    marker = '/*SERIALIZER_MANAGED_ENGINE_RUNTIME*/'
    if fixture.count(marker) != 1:
        raise AssertionError('Standalone runtime insertion boundary differs')
    source = args.build / 'javascript_managed_engine_test.mjs'
    source.write_text(fixture.replace(marker, runtime), encoding='utf-8')
    command = [args.node, '--unhandled-rejections=strict', source]
    print(run([args.node, '--version']))
    platform = run([args.node, '-p', 'process.platform'])
    # A separate realm has no Node process/Buffer/require and forbids every dynamic
    # import. This is a memory-module dependency proof, not browser SDK qualification.
    memory_fixture = fixture[fixture.index('let checks = 0;'):fixture.index('/** Hold a native writer')]
    isolated_source = runtime + memory_fixture + '\nawait memory(4); await memory(8); console.log(`Node-free VM memory checks: ${checks}`);\n'
    isolated = args.build / 'javascript_managed_engine_memory_vm.mjs'
    isolated.write_text('''import {SourceTextModule, createContext} from 'node:vm';
import {TextEncoder, TextDecoder} from 'node:util';
const context = createContext({TextEncoder, TextDecoder, Uint8Array, ArrayBuffer,
  SharedArrayBuffer, structuredClone, console});
const module = new SourceTextModule(''' + json.dumps(isolated_source) + ''', {
  context, importModuleDynamically() { throw new Error('Memory kernel attempted Node import'); }
});
await module.link(() => { throw new Error('Memory kernel attempted static import'); });
await module.evaluate();
''', encoding='utf-8')
    print(run([args.node, '--experimental-vm-modules', '--unhandled-rejections=strict', isolated]))
    with tempfile.TemporaryDirectory(prefix='native-js-', dir=args.build) as selected:
        directory = Path(selected)
        print(run(command + [directory]))
        if platform != 'win32':
            print(f'Windows native journal crash/alias campaigns unavailable on {platform}; memory and unavailable-profile checks executed')
            return
        for phase in ('BEFORE_WRITE', 'AFTER_LENGTH', 'AFTER_HEADER', 'AFTER_PAYLOAD',
                      'AFTER_CHECKSUM', 'AFTER_FORCE'):
            case = directory / phase.lower()
            case.mkdir()
            path, ready = case / 'state.srjsmg', case / 'child.ready'
            run(command + ['--seed', path])
            stopped_child(command + ['--crash', path, phase, ready], ready, lambda: None)
            run(command + ['--verify-crash', path, phase])
        print('Native JavaScript process-death checks: 6 boundaries; OS writer leases released')
        case = directory / 'cross-process-alias'
        case.mkdir()
        path, alias, ready = case / 'state.srjsmg', case / 'hard-link.srjsmg', case / 'writer.ready'
        run(command + ['--seed', path])
        alias.hardlink_to(path)

        def contenders():
            """Both canonical-name and hard-link writers must fail before any byte changes."""
            original = path.read_bytes()
            run(command + ['--locked', path])
            run(command + ['--locked', alias])
            if path.read_bytes() != original:
                raise AssertionError('A rejected writer modified the journal')

        stopped_child(command + ['--hold-writer', path, ready], ready, contenders)
        run(command + ['--verify-crash', alias, 'BEFORE_WRITE'])
        print('Native JavaScript cross-process canonical/hard-link exclusion: PASS')


if __name__ == '__main__':
    main()
