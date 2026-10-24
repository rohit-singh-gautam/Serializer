# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify actual generated JS native engines and strict matching TS declarations."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

from javascript_managed_engine_test import stopped_child

ROOT = Path(__file__).resolve().parents[1]


def run(command, success=True):
    """Run one compiler/SDK process at a time and preserve ordinary diagnostics."""
    result = subprocess.run([str(value) for value in command], capture_output=True,
                            text=True, timeout=180)
    if (result.returncode == 0) != success:
        raise AssertionError(f'{command}\n{result.stdout}\n{result.stderr}')
    return result.stdout + result.stderr


def declarations(args, folder, width):
    """Typecheck positive APIs plus independently rejected unsafe scalar/callback/target contracts."""
    source = folder / 'typed.mts'
    source.write_text((ROOT / 'test/javascript_managed_engine_generated_test.mts').read_text(encoding='utf-8')
                      .replace('@ID_TYPE@', 'number' if width == 32 else 'bigint')
                      .replace('@ZERO@', '0' if width == 32 else '0n'), encoding='utf-8')
    command = [args.node, args.tsc, '--strict', '--target', 'ES2022', '--module', 'NodeNext',
               '--moduleResolution', 'NodeNext', '--noErrorTruncation']
    run(command + [source])
    print(run([args.node, '--unhandled-rejections=strict', folder / 'typed.mjs']).strip(), flush=True)
    preamble = '''import {EngineGraphModel, EngineGraphItem, EngineGraphBranch, EngineIdentityFirst, EngineIdentitySecond, ManagedStore, ManagedDocument,
  ManagedReference, ManagedToken, ManagedReceipt, ManagedProposal, managedSchema} from './model.mjs';
const schema = managedSchema(EngineGraphModel);
const store = new ManagedStore(schema, new EngineGraphModel(), new ManagedDocument(1n, 2n));
'''
    inverse = 'bigint' if width == 32 else 'number'
    native = 'number' if width == 32 else 'bigint'
    identity = '1' if width == 32 else '1n'
    cases = [
        ('async-callback', 'store.transact(async edit => { edit.root.title = "async"; });', 'TS2345'),
        ('returned-value', 'store.transact(() => 7);', 'TS2322'),
        ('wrong-root', 'new ManagedStore(schema, new EngineGraphItem(), new ManagedDocument(1n, 2n));', 'TS2740'),
        ('wrong-width', f'const id: {inverse} = store.allocatedId();', 'TS2322'),
        ('wrong-document', 'new ManagedDocument(1, 2n);', 'TS2345'),
        ('immutable-pin', 'store.read().epoch = 7n;', 'TS2540'),
        ('wrong-token-target', f'store.read().resolve(new ManagedToken<EngineGraphItem, {native}>(new ManagedDocument(1n, 2n), 1n, {identity}), EngineGraphBranch);', 'TS2345'),
        ('same-layout-reference', f'const first = new ManagedReference<EngineIdentityFirst, {native}>(1n, 2n, 404n, {identity}); const second: ManagedReference<EngineIdentitySecond, {native}> = first;', 'TS2322'),
        ('same-layout-token', f'const first = new ManagedToken<EngineIdentityFirst, {native}>(new ManagedDocument(1n, 2n), 404n, {identity}); const second: ManagedToken<EngineIdentitySecond, {native}> = first;', 'TS2322'),
        ('same-layout-resolution', f'store.read().resolve(new ManagedToken<EngineIdentityFirst, {native}>(new ManagedDocument(1n, 2n), 404n, {identity}), EngineIdentitySecond);', 'TS2345'),
        ('unbranded-document', 'const fake: ManagedDocument = {high: 1n, low: 2n};', 'TS2741'),
        ('unbranded-receipt', f'''const fake: ManagedReceipt<EngineGraphModel, {native}> = {{proposal:
new ManagedProposal<EngineGraphModel, {native}>('client', 'request', new ManagedDocument(1n, 2n), schema.schemaId, 0n, new Uint8Array()),
status: 'accepted', snapshot: store.read()}};''', 'TS2741'),
    ]
    negative_sources = []
    for name, body, diagnostic in cases:
        negative = folder / f'negative-{name}.mts'
        negative.write_text(preamble + body + '\n', encoding='utf-8')
        negative_sources.append(negative)
    # One serial tsc invocation still independently proves every named contract,
    # while sharing its parsed SDK/standard libraries instead of repeatedly loading them.
    output = run(command + ['--noEmit'] + negative_sources, False)
    decisions = re.findall(r'negative-([a-z-]+)\.mts\(\d+,\d+\): error (TS\d+):', output)
    expected = {name: diagnostic for name, _, diagnostic in cases}
    if any(name not in expected or expected[name] != diagnostic for name, diagnostic in decisions):
        raise AssertionError(f'Unexpected negative TypeScript diagnostic:\n{output}')
    for name, _, diagnostic in cases:
        if (name, diagnostic) not in decisions:
            raise AssertionError(f'Negative TS case {name} did not reject the intended contract:\n{output}')
    return len(cases)


def qualify(args, directory, width, encoding, protocols, native_windows):
    """Generate both actual SDKs and run native ownership/recovery against their unchanged outputs."""
    folder = directory / f'uint{width}_{encoding}_{protocols}'
    folder.mkdir()
    for language, filename in (('js', 'model.mjs'), ('typescript', 'model.d.mts')):
        run([args.compiler, '--input', ROOT / 'test/resources/javascript_managed_engine.serializer',
             '--language', language, '--output', folder / filename,
             f'--{language}.managed_engine', 'true', '--managed.id_type', f'uint{width}',
             '--managed.id_encoding', encoding, '--protocols', protocols])
    source = folder / 'runner.mjs'
    source.write_text((ROOT / 'test/javascript_managed_engine_generated_test.mjs').read_text(encoding='utf-8'), encoding='utf-8')
    protocol = 'BINARY_NONE' if protocols == 'binary_none' else 'BINARY_INTEGER'
    command = [args.node, '--unhandled-rejections=strict', source, folder, width, protocol]
    output = run(command)
    print(f'uint{width} {encoding} {protocol}: {output.strip()}', flush=True)
    match = re.search(r'Actual generated JavaScript engine checks: (\d+)', output)
    if not match:
        raise AssertionError('Generated fixture did not report executed checks')
    negatives = declarations(args, folder, width)
    if (width, encoding, protocols) in ((32, 'compact_varint', 'all'), (64, 'fixed', 'binary_none')):
        print(run([args.node, '--experimental-vm-modules', '--unhandled-rejections=strict',
                   ROOT / 'test/javascript_managed_engine_browser_test.mjs', folder / 'model.mjs']).strip(), flush=True)
        if native_windows:
            ready = folder / 'child.ready'
            stopped_child(command + ['--hold', ready], ready, lambda: None)
            run(command + ['--recover-child'])
    return int(match.group(1)), negatives


def selection(args, directory):
    """Exercise strict INI/CLI switches, token-only boundaries and transactional output preservation."""
    schema = ROOT / 'test/resources/javascript_managed_engine.serializer'
    for language, filename in (('js', 'selection.mjs'), ('typescript', 'selection.d.mts')):
        output = directory / filename
        output.write_text('preserved destination\n', encoding='utf-8')
        command = [args.compiler, '--input', schema, '--language', language, '--output', output]
        for settings in ([], [f'--{language}.managed_engine', 'invalid'], [f'--{language}.managed_engine', 'false']):
            run(command + settings, False)
            if output.read_text(encoding='utf-8') != 'preserved destination\n':
                raise AssertionError('Rejected managed output replaced the destination')
        configuration = directory / f'{language}.ini'
        configuration.write_text(f'[{language}]\nmanaged_engine=true\n', encoding='utf-8')
        run(command + ['--config', configuration])
        previous = output.read_bytes()
        run(command + ['--config', configuration, f'--{language}.managed_engine', 'false'], False)
        if output.read_bytes() != previous:
            raise AssertionError('Rejected engine override replaced the destination')
        run(command + [f'--{language}.managed_token_codecs', 'true'])
        text = output.read_text(encoding='utf-8')
        if 'class ManagedStore' in text or 'class ManagedReference' not in text:
            raise AssertionError('Token-only selection activated a state engine')


def main():
    """Keep compiler, Node and tsc work serial; preserve useful outputs under the ignored build directory."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--node', default='node')
    parser.add_argument('--tsc', type=Path, required=True)
    args = parser.parse_args()
    args.build.mkdir(parents=True, exist_ok=True)
    print(run([args.node, '--version']).strip())
    platform = run([args.node, '-p', 'process.platform']).strip()
    print(run([args.node, args.tsc, '--version']).strip())
    directory = Path(tempfile.mkdtemp(prefix='generated-js-', dir=args.build))
    print(f'Generated SDK evidence directory: {directory}', flush=True)
    selection(args, directory)
    assertions, negatives = 0, 0
    for width in (32, 64):
        for encoding in ('compact_varint', 'fixed'):
            for protocols in ('all', 'binary_none'):
                count, rejected = qualify(args, directory, width, encoding, protocols, platform == 'win32')
                assertions += count
                negatives += rejected
    deaths = 2 if platform == 'win32' else 0
    print(f'Actual generated JS managed checks: {assertions}; eight profiles; strict TS negative contracts: {negatives}; generated process-death replays: {deaths}')


if __name__ == '__main__':
    main()
