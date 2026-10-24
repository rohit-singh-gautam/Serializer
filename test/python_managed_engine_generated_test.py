# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify the actual generated native Python SDK, descriptors and identity policies."""
import argparse
from dataclasses import replace
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CHECKS = 0


def check(value):
    """Keep real generated-output checks active under Python optimization."""
    global CHECKS
    CHECKS += 1
    if not value:
        raise AssertionError(f'Generated native Python check {CHECKS} failed')


def run(command, success=True):
    """Run compiler entry points sequentially and retain diagnostics on failure."""
    result = subprocess.run([str(value) for value in command], capture_output=True, text=True, timeout=180)
    check((result.returncode == 0) == success)
    if success and result.returncode:
        raise AssertionError(result.stdout + result.stderr)


def qualify(args, width, policy, separate=False):
    """Exercise actual generated storage and codecs through every native runtime mechanism."""
    suffix = '_separate' if separate else ''
    folder = args.build / f'uint{width * 8}_{policy}{suffix}'
    folder.mkdir(parents=True, exist_ok=True)
    output = folder / (f'model_uint{width * 8}_{policy}.py' if separate else 'model.py')
    source = ROOT / 'test/resources/python_managed_engine.serializer'
    if separate:
        entry = folder / 'entry.serializer'
        relative = Path(os.path.relpath(source, folder)).as_posix()
        entry.write_text('serializer version 1.20.0;\ninclude ' + relative + ';\n'
                         'class entry_anchor { public uint32 marker; }\n', encoding='utf-8')
        source = entry
    run([args.compiler, '--input', source,
         '--language', 'python', '--output', output, '--python.managed_engine', 'true',
         '--managed.id_type', f'uint{width * 8}', '--managed.id_encoding', policy,
         '--include-mode', 'separate' if separate else 'inline'])
    if separate:
        links = [line.split(': ', 1)[1] for line in output.read_text(encoding='utf-8').splitlines()
                 if line.startswith('# Serializer output: ')]
        check(len(links) >= 3)
        check(all((output.parent / path).is_file() for path in links))
    name = f'generated_native_engine_{width}_{policy}{suffix}'
    spec = importlib.util.spec_from_file_location(name, output)
    sdk = importlib.util.module_from_spec(spec)
    sys.modules[name] = sdk
    sys.path.insert(0, str(folder.resolve()))
    try:
        spec.loader.exec_module(sdk)
    finally:
        sys.path.remove(str(folder.resolve()))
    schema = sdk.managed_schema(sdk.EngineGraphModel)
    check(schema.id_bytes == width)
    check(policy in schema.schema_id)
    blank = sdk.ManagedStore(schema, sdk.EngineGraphModel(), document=(17, 19))
    check(type(blank.read().root.precise) is float and blank.read().root.precise == 0.0)
    check(type(blank.read().root.ratio) is float and blank.read().root.ratio == 0.0)
    check(blank.read().root.fingerprint == (0,) * 32)
    blank.close()
    value = sdk.EngineGraphModel()
    value.title = 'native baseline'
    value.children = [sdk.EngineGraphItem()]
    value.children[0].label = 'owned child'
    value.mapped = {7: sdk.EngineGraphItem()}
    value.ordinary.label = 'ordinary value'
    value.ordinary_branch.leaves = [sdk.EngineGraphItem()]
    value.branches = [sdk.EngineGraphBranch()]
    value.branches[0].leaves = [sdk.EngineGraphItem()]
    value.precise, value.ratio, value.enabled, value.balance = 7, 0.25, True, -19
    value.fingerprint[0] = 255
    store = sdk.ManagedStore(schema, value, document=(17, 18),
        initialize=lambda edit: setattr(edit.root, 'selected', (edit.token(edit.root.children[0]),)))
    journal = Path(tempfile.mkdtemp(prefix='native-journal-', dir=folder)) / 'state.srlpmg'
    store.create_journal(journal)
    value.title = 'caller mutation'
    old = store.read()
    check(old.root.title == 'native baseline')
    check(old.root.persistent_id == 1 and old.root.children[0].persistent_id == 2)
    check(old.root.mapped[7].persistent_id == 3 and old.root.ordinary.persistent_id == 0)
    check(old.root.ordinary_branch.persistent_id == 0 and old.root.ordinary_branch.leaves[0].persistent_id == 0)
    check(old.root.branches[0].persistent_id == 4 and old.root.branches[0].leaves[0].persistent_id == 5)
    check(old.resolve(old.root.selected[0]) is old.root.children[0])
    check(old.root.fixed == ('', '') and old.root.note == ())
    check(old.root.precise == 7.0 and old.root.ratio == 0.25 and old.root.enabled and old.root.balance == -19)
    check(old.root.fingerprint == (255,) + (0,) * 31 and old.root.mode == sdk.EngineGraphKind.First)
    for invalid in (True, 10 ** 1000):
        try:
            store.transact(lambda edit: setattr(edit.root, 'precise', invalid))
        except (sdk.ManagedError, ValueError, OverflowError):
            check(store.read() is old)
        else:
            raise AssertionError('Invalid generated float value was accepted')
    try:
        old.root.title = 'illegal'
    except AttributeError:
        check(True)
    else:
        raise AssertionError('Generated immutable pin allowed mutation')
    captured = []
    def edit(action):
        captured.append(action.root)
        action.root.title = 'committed'
        action.root.children.append(sdk.EngineGraphItem())
        action.root.note = ('optional native text',)
    store.transact(edit, lambda action: setattr(action.root, 'selected', (action.token(action.root.children[-1]),)))
    captured[0].title = 'escaped mutation'
    check(store.read().root.title == 'committed' and old.root.title == 'native baseline')
    check(store.read().root.selected[0].object_id == 6)
    store.undo()
    check(store.read().root.title == 'native baseline' and len(store.read().root.children) == 1)
    store.redo()
    check(store.read().root.title == 'committed' and store.read().root.note == ('optional native text',))
    client = sdk.ManagedClient(schema, store.read(), 'one')
    stale = sdk.ManagedClient(schema, store.read(), 'two')
    proposal = client.propose('request', lambda draft: setattr(draft, 'title', 'authority accepted'))
    authority = sdk.ManagedAuthority(store)
    before = store.read()
    allocated, durable_bytes = store._highwater, journal.read_bytes()
    invalid = schema.decode(proposal.data)
    invalid.persistent_id = 0
    try:
        authority.accept(replace(proposal, request_id='replacement', data=schema.encode(invalid)))
    except sdk.ManagedError:
        check(store.read() is before and store._highwater == allocated and journal.read_bytes() == durable_bytes)
    else:
        raise AssertionError('Generated authority accepted replacement root identity')
    receipt = authority.accept(proposal)
    check(authority.accept(client.retry()) is receipt)
    invalid = schema.decode(receipt.data)
    invalid.persistent_id = store._highwater + 1
    try:
        client.acknowledge(replace(receipt, data=schema.encode(invalid)))
    except sdk.ManagedError:
        check(client.retry() is proposal and client.read() is not store.read())
    else:
        raise AssertionError('Generated client accepted replacement root identity')
    client.acknowledge(receipt)
    check(client.read().root.title == 'authority accepted')
    conflict_request = stale.propose('request', lambda draft: setattr(draft, 'title', 'stale'))
    conflict = authority.accept(conflict_request)
    check(conflict.status == 'conflict')
    stale.acknowledge(conflict)
    check(stale.read().root.title == 'authority accepted')
    expected = store.read()
    store.close()
    reopened = sdk.ManagedStore.open_journal(schema, journal)
    check(reopened.read().data == expected.data and reopened.read().epoch == expected.epoch)
    check(sdk.ManagedAuthority(reopened).accept(proposal) == receipt)
    check(sdk.ManagedAuthority(reopened).accept(conflict_request) == conflict)
    reopened.close()
    # Identity widths are exercised through real generated integer fields and exact codecs.
    maximum = (1 << (width * 8)) - 1
    identified = sdk.EngineGraphModel()
    identified.persistent_id = maximum
    pin = schema.snapshot(identified, (17, 18), maximum, 0, 0)
    check(pin.root.persistent_id == maximum)
    check(schema.decode(pin.data).persistent_id == maximum)


def main():
    """Run every identity profile in inline and included dependency bundles, then verify explicit selection."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    args.build.mkdir(parents=True, exist_ok=True)
    for width in (4, 8):
        for policy in ('fixed', 'compact_varint'):
            qualify(args, width, policy)
            qualify(args, width, policy, separate=True)
    output = args.build / 'preserved.py'
    output.write_text('preserve destination\n', encoding='utf-8')
    base = [args.compiler, '--input', ROOT / 'test/resources/python_managed_engine.serializer',
            '--language', 'python', '--output', output]
    for flags in ([], ['--python.managed_engine', 'false'], ['--python.managed_engine', 'invalid']):
        run(base + flags, False)
        check(output.read_text(encoding='utf-8') == 'preserve destination\n')
    config = args.build / 'engine.ini'
    config.write_text('[python]\nmanaged_engine=true\n', encoding='utf-8')
    run(base + ['--config', config])
    check('def managed_schema(' in output.read_text(encoding='utf-8'))
    run(base + ['--config', config, '--python.managed_engine', 'false'], False)
    print(f'Generated native Python engine: {CHECKS} checks across four identity profiles and two layouts')


if __name__ == '__main__':
    main()
