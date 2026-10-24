# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify isolated Python native ownership, journal and authority contracts before generator integration."""
import base64
from dataclasses import replace
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_loader('serializer_managed_test_runtime', loader=None)
runtime = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = runtime
template = (ROOT / 'src/python_managed_engine_runtime.inc').read_text(encoding='utf-8')
exec(compile(''.join(re.findall(r'R"python\((.*?)\)python"', template, re.S)),
             'python_managed_engine_runtime.inc', 'exec'), runtime.__dict__)


class Token:
    """An independent codec's ordinary mutable four-integer identity record."""
    def __init__(self, high=0, low=0, tag=0, identity=0):
        self.document_high, self.document_low, self.class_id, self.object_id = high, low, tag, identity


class Item:
    """Minimal generated-style owning record used by the generic descriptor adapter."""
    def __init__(self):
        self.persistent_id, self.text, self.link = 0, '', ()


class Model:
    """Independent exact codec adapter; runtime state logic never knows these field names."""
    def __init__(self):
        self.persistent_id, self.title, self.items, self.by_key, self.ordinary, self.focus = 0, '', [], {}, Item(), ()

    def encode(self, protocol):
        """Use an independently defined stable fixture format, rather than native engine internals."""
        def token(value):
            return [value.document_high, value.document_low, value.class_id, value.object_id]
        def item(value):
            return [value.persistent_id, value.text, [token(edge) for edge in value.link]]
        return json.dumps([self.persistent_id, self.title, [item(value) for value in self.items],
                           {key: item(value) for key, value in self.by_key.items()}, item(self.ordinary),
                           [token(edge) for edge in self.focus]], separators=(',', ':'), sort_keys=True).encode()

    @staticmethod
    def decode(data, protocol):
        """Decode fresh ordinary objects with no retained alias to engine state."""
        values = json.loads(data)
        def item(value):
            result = Item()
            result.persistent_id, result.text, tokens = value
            result.link = tuple(Token(*edge) for edge in tokens)
            return result
        result = Model()
        result.persistent_id, result.title = values[:2]
        result.items = [item(value) for value in values[2]]
        result.by_key = {key: item(value) for key, value in values[3].items()}
        result.ordinary = item(values[4])
        result.focus = tuple(Token(*edge) for edge in values[5])
        return result


def schema(**limits):
    """Describe generated storage using the same generic metadata contract for every field."""
    field, record = runtime.ManagedField, runtime.ManagedRecord
    return runtime.ManagedSchema(Model, 2, 'test.native.python.v1', {
        Item: record(11, (field('text', str), field('link', ('optional', ('token', Item))))),
        Model: record(12, (field('title', str), field('items', ('array', Item), True),
            field('by_key', ('map', str, Item), True), field('ordinary', Item),
            field('focus', ('optional', ('token', Item)))))}, reference_type=Token, **limits)


def initial():
    """Construct ordinary caller-owned input to test explicit ownership transfer by copying."""
    value = Model()
    value.title = 'baseline'
    value.items = [Item()]
    value.items[0].text = 'first'
    value.by_key['key'] = Item()
    value.by_key['key'].text = 'mapped'
    return value


class ManagedEngineTest(unittest.TestCase):
    """Native runtime tests intentionally make no claim about compiler-hook qualification."""
    def store(self, **options):
        """Use an exact known document so reference scope and journal fixtures are deterministic."""
        return runtime.ManagedStore(schema(), initial(), document=(31, 32), **options)

    def test_runtime_template_tokens_fit_supported_compilers(self):
        """Adjacent source literals preserve Python text while respecting established MSVC source-token limits."""
        tokens = re.findall(r'R"python\(.*?\)python"', template, re.S)
        self.assertGreater(len(tokens), 1)
        self.assertTrue(all(len(token.encode('utf-8')) <= 16380 for token in tokens))

    def test_owned_immutable_pins_and_escaped_drafts(self):
        """Caller aliases, retained drafts and all collection wrappers cannot mutate accepted pins."""
        source = initial()
        store = runtime.ManagedStore(schema(), source, document=(31, 32))
        source.title = 'foreign'
        source.items[0].text = 'foreign'
        before = store.read()
        self.assertEqual(before.root.title, 'baseline')
        self.assertEqual(before.root.items[0].text, 'first')
        with self.assertRaises(AttributeError):
            before.root.title = 'illegal'
        with self.assertRaises(TypeError):
            before.root.by_key['key'] = 0
        escaped = []
        def edit(action):
            escaped.append(action)
            escaped.append(action.root)
            action.root.title = 'accepted'
        after = store.transact(edit)
        escaped[1].title = 'late mutation'
        self.assertEqual(after.root.title, 'accepted')
        self.assertEqual(before.root.title, 'baseline')
        with self.assertRaises(runtime.ManagedError):
            _ = escaped[0].root
        store.close()
        self.assertEqual(after.root.title, 'accepted')

    def test_failure_noop_and_nested_mutation_are_atomic(self):
        """Thrown and caught nested errors cannot publish partial changes or introduce history."""
        store = self.store()
        before = store.read()
        def fail(edit):
            edit.root.title = 'partial'
            raise ValueError('callback failure')
        with self.assertRaises(ValueError):
            store.transact(fail)
        self.assertIs(store.read(), before)
        self.assertIs(store.transact(lambda edit: None), before)
        def nested(edit):
            edit.root.title = 'partial'
            with self.assertRaises(runtime.ManagedError):
                store.transact(lambda child: None)
        with self.assertRaises(runtime.ManagedError):
            store.transact(nested)
        self.assertIs(store.read(), before)
        with self.assertRaises(runtime.ManagedError):
            store.undo()

    def test_monotonic_identity_and_typed_references(self):
        """Logical IDs survive undo, ordinary occurrences stay unbound and old pins retain resolution."""
        store = self.store(initialize=lambda edit: setattr(edit.root, 'focus', (edit.token(edit.root.items[0]),)))
        before = store.read()
        self.assertEqual(before.root.persistent_id, 1)
        self.assertEqual(before.root.items[0].persistent_id, 2)
        self.assertEqual(before.root.by_key['key'].persistent_id, 3)
        self.assertEqual(before.root.ordinary.persistent_id, 0)
        self.assertIs(before.resolve(before.root.focus[0]), before.root.items[0])
        with self.assertRaises(runtime.ManagedError):
            before.resolve(runtime.ManagedToken(999, 32, 11, 2))
        with self.assertRaises(runtime.ManagedError):
            before.resolve(runtime.ManagedToken(31, 32, 12, 2))
        store.transact(lambda edit: edit.root.items.append(Item()))
        inserted = store.read().root.items[-1].persistent_id
        store.undo()
        store.transact(lambda edit: edit.root.items.append(Item()))
        self.assertGreater(store.read().root.items[-1].persistent_id, inserted)
        with self.assertRaises(runtime.ManagedError):
            store.redo()
        with self.assertRaises(runtime.ManagedError):
            store.transact(lambda edit: setattr(edit.root.items[0], 'persistent_id', 999))
        with self.assertRaises(runtime.ManagedError):
            store.transact(lambda edit: edit.root.items.pop(0))
        self.assertEqual(before.resolve(before.root.focus[0]).text, 'first')

    def test_reference_initialization_and_failed_reservations(self):
        """New edge creation follows assignment and exposed reservations are never recycled after failure."""
        store = self.store()
        captured = []
        def references(edit):
            token = edit.token(edit.root.items[-1])
            captured.append(token.object_id)
            edit.root.focus = (token,)
            raise ValueError('abort after observing ID')
        with self.assertRaises(ValueError):
            store.transact(lambda edit: edit.root.items.append(Item()), references)
        store.transact(lambda edit: edit.root.items.append(Item()),
            lambda edit: setattr(edit.root, 'focus', (edit.token(edit.root.items[-1]),)))
        self.assertGreater(store.read().root.focus[0].object_id, captured[0])
        self.assertEqual(len(store.read().root.items), 2)

    def test_thread_contract_and_immutable_cross_thread_pins(self):
        """Reads share immutable pins while mutation and closing remain confined to the native owner."""
        store = self.store()
        outcomes = []
        def other():
            outcomes.append(store.read().root.title)
            try:
                store.transact(lambda edit: setattr(edit.root, 'title', 'wrong thread'))
            except runtime.ManagedError:
                outcomes.append('confined')
        thread = threading.Thread(target=other)
        thread.start(); thread.join()
        self.assertEqual(outcomes, ['baseline', 'confined'])

    def test_history_and_resource_admission_bounds(self):
        """Finite history eviction and byte admission are distinct from process peak memory limits."""
        store = self.store(max_history_entries=2)
        for title in ('one', 'two', 'three'):
            store.transact(lambda edit: setattr(edit.root, 'title', title))
        store.undo()
        self.assertEqual(store.read().root.title, 'two')
        with self.assertRaises(runtime.ManagedError):
            store.undo()
        limited = runtime.ManagedStore(schema(max_bytes=128), initial(), document=(1, 2))
        before = limited.read()
        with self.assertRaises(runtime.ManagedError):
            limited.transact(lambda edit: setattr(edit.root, 'title', 'x' * 256))
        self.assertIs(limited.read(), before)

    def test_positive_limits_and_request_ids_reject_inexact_types(self):
        """Every accepted constructor policy can be replayed under the same exact integer contract."""
        for option in ('max_history_entries', 'max_history_bytes', 'max_receipts'):
            for invalid in (True, 1.5, 0, -1):
                with self.assertRaises(ValueError):
                    self.store(**{option: invalid})
        for option in ('max_bytes', 'max_entities'):
            for invalid in (True, 1.5, 0, -1):
                with self.assertRaises(ValueError):
                    schema(**{option: invalid})
        with self.assertRaises(ValueError):
            schema(id_bytes=4.0)
        store = self.store()
        client = runtime.ManagedClient(schema(), store.read(), 'client')
        for invalid in (True, 7, (), ''):
            with self.assertRaises(runtime.ManagedError):
                client.propose(invalid, lambda draft: None)
        with self.assertRaises(runtime.ManagedError):
            client.retry()

    def test_synchronous_callbacks_reject_return_values_and_coroutines(self):
        """Partial drafts and unexecuted async functions cannot report successful synchronous completion."""
        store = self.store()
        before = store.read()
        calls = []
        async def asynchronous(value):
            calls.append('must not run')
        def returned_value(edit):
            edit.root.title = 'partial'
            return True
        for callback in (asynchronous, returned_value):
            with self.assertRaises(runtime.ManagedError):
                store.transact(callback)
            self.assertIs(store.read(), before)
        for initializer in (asynchronous, lambda edit: True):
            with self.assertRaises(runtime.ManagedError):
                self.store(initialize=initializer)
            with self.assertRaises(runtime.ManagedError):
                store.transact(lambda edit: setattr(edit.root, 'title', 'partial'), initializer)
            self.assertIs(store.read(), before)
        client = runtime.ManagedClient(schema(), before, 'client')
        for callback in (asynchronous, lambda draft: True):
            with self.assertRaises(runtime.ManagedError):
                client.propose('request', callback)
            with self.assertRaises(runtime.ManagedError):
                client.retry()
            self.assertEqual(client.read().data, before.data)
        self.assertEqual(calls, [])

    def test_ordinary_ancestors_keep_all_owned_descendants_unbound(self):
        """Managed eligibility on a nested type never crosses an ordinary occurrence boundary."""
        class Branch:
            def __init__(self):
                self.persistent_id, self.leaf = 0, Item()
        class Graph:
            def __init__(self):
                self.persistent_id, self.ordinary, self.owned = 0, Branch(), Branch()
            def encode(self, protocol):
                return json.dumps([self.persistent_id, self.ordinary.persistent_id,
                    self.ordinary.leaf.persistent_id, self.ordinary.leaf.text,
                    self.owned.persistent_id, self.owned.leaf.persistent_id, self.owned.leaf.text]).encode()
            @staticmethod
            def decode(data, protocol):
                values = json.loads(data)
                result = Graph()
                (result.persistent_id, result.ordinary.persistent_id,
                 result.ordinary.leaf.persistent_id, result.ordinary.leaf.text,
                 result.owned.persistent_id, result.owned.leaf.persistent_id, result.owned.leaf.text) = values
                return result
        field, record = runtime.ManagedField, runtime.ManagedRecord
        contract = runtime.ManagedSchema(Graph, 2, 'ordinary.boundary', {
            Item: record(11, (field('text', str),)),
            Branch: record(13, (field('leaf', Item, True),)),
            Graph: record(14, (field('ordinary', Branch), field('owned', Branch, True)))})
        store = runtime.ManagedStore(contract, Graph(), document=(31, 32))
        before = store.read()
        self.assertEqual((before.root.persistent_id, before.root.ordinary.persistent_id,
            before.root.ordinary.leaf.persistent_id, before.root.owned.persistent_id,
            before.root.owned.leaf.persistent_id), (1, 0, 0, 2, 3))
        store.transact(lambda edit: setattr(edit.root.ordinary.leaf, 'text', 'ordinary edit'))
        self.assertEqual(store.read().root.ordinary.leaf.persistent_id, 0)
        store.undo()
        self.assertEqual(store.read().root.ordinary.leaf.text, '')
        store.redo()
        self.assertEqual(store.read().root.owned.leaf.persistent_id, 3)
        with self.assertRaises(runtime.ManagedError):
            store.transact(lambda edit: setattr(edit.root.ordinary.leaf, 'persistent_id', 7))

    def test_float_shapes_normalize_native_integer_defaults_and_reject_bools(self):
        """Ordinary generated double defaults use integer zero, while accepted snapshots decode as float."""
        class Scalars:
            def __init__(self):
                self.persistent_id, self.value = 0, 0
            def encode(self, protocol):
                return json.dumps([self.persistent_id, self.value]).encode()
            @staticmethod
            def decode(data, protocol):
                result = Scalars()
                result.persistent_id, result.value = json.loads(data)
                return result
        contract = runtime.ManagedSchema(Scalars, 2, 'native.float.defaults', {
            Scalars: runtime.ManagedRecord(15, (runtime.ManagedField('value', float),))})
        store = runtime.ManagedStore(contract, Scalars(), document=(31, 32))
        self.assertIs(type(store.read().root.value), float)
        store.transact(lambda edit: setattr(edit.root, 'value', 7))
        before = store.read()
        self.assertEqual(before.root.value, 7.0)
        for invalid in (True, 10 ** 1000):
            with self.assertRaises(runtime.ManagedError):
                store.transact(lambda edit: setattr(edit.root, 'value', invalid))
            self.assertIs(store.read(), before)

    def test_native_journal_history_and_writer_lock_recovery(self):
        """Durable native records recover without callbacks, preserving history and writer exclusion."""
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'state.srlpmg'
            store = self.store()
            store.create_journal(path)
            with self.assertRaises((OSError, RuntimeError)):
                runtime.ManagedStore.open_journal(schema(), path)
            store.transact(lambda edit: setattr(edit.root, 'title', 'one'))
            store.transact(lambda edit: setattr(edit.root, 'title', 'two'))
            store.undo()
            expected = store.read()
            store.close()
            restored = runtime.ManagedStore.open_journal(schema(), path)
            self.assertEqual(restored.read().data, expected.data)
            self.assertEqual(restored.read().epoch, expected.epoch)
            restored.redo()
            self.assertEqual(restored.read().root.title, 'two')
            restored.close()
            self.assertTrue(Path(str(path) + '.lock').exists())

    def test_complete_recovery_flushes_before_exposing_exact_outcomes(self):
        """A complete unacknowledged frame must reach fsync even when there is no incomplete tail."""
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'complete'
            store = self.store(); store.create_journal(path)
            client = runtime.ManagedClient(schema(), store.read(), 'client')
            proposal = client.propose('request', lambda draft: setattr(draft, 'title', 'accepted'))
            receipt = runtime.ManagedAuthority(store).accept(proposal)
            store.close()
            original = path.read_bytes()
            with patch.object(runtime._mg_os, 'fsync', wraps=os.fsync) as synced:
                reopened = runtime.ManagedStore.open_journal(schema(), path)
                synced.assert_called_once()
                self.assertEqual(runtime.ManagedAuthority(reopened).accept(proposal), receipt)
                reopened.close()
            with patch.object(runtime._mg_os, 'fsync', side_effect=OSError('recovery flush failure')):
                with self.assertRaises(runtime.ManagedIndeterminate):
                    runtime.ManagedStore.open_journal(schema(), path)
            self.assertEqual(path.read_bytes(), original)
            reopened = runtime.ManagedStore.open_journal(schema(), path)
            self.assertEqual(runtime.ManagedAuthority(reopened).accept(proposal), receipt)
            reopened.close()

    def test_native_journal_partial_tail_and_complete_corruption(self):
        """Only incomplete final frames are discarded; complete corrupted bytes are preserved and rejected."""
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / 'source'
            store = self.store(); store.create_journal(source)
            store.transact(lambda edit: setattr(edit.root, 'title', 'one'))
            store.close()
            original = source.read_bytes()
            for suffix in (b'\x04', struct.pack('<I', 10) + b'partial'):
                path = Path(folder) / ('partial-' + str(len(suffix)))
                path.write_bytes(original + suffix)
                restored = runtime.ManagedStore.open_journal(schema(), path)
                self.assertEqual(restored.read().root.title, 'one')
                restored.close()
                self.assertEqual(path.read_bytes(), original)
            path = Path(folder) / 'corrupt'
            damaged = bytearray(original); damaged[-1] ^= 1
            path.write_bytes(damaged)
            with self.assertRaises(runtime.ManagedError):
                runtime.ManagedStore.open_journal(schema(), path)
            self.assertEqual(path.read_bytes(), damaged)

    def test_native_journal_uncertain_flush_fences_until_reopen(self):
        """A reported flush failure cannot claim rollback or permit a second mutation on uncertain state."""
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'state'
            store = self.store(); store.create_journal(path)
            before = store.read()
            with patch.object(runtime._mg_os, 'fsync', side_effect=OSError('injected fsync failure')):
                with self.assertRaises(runtime.ManagedIndeterminate):
                    store.transact(lambda edit: setattr(edit.root, 'title', 'uncertain'))
            self.assertIs(store.read(), before)
            with self.assertRaises(runtime.ManagedIndeterminate):
                store.transact(lambda edit: None)
            store.close()
            restored = runtime.ManagedStore.open_journal(schema(), path)
            self.assertEqual(restored.read().root.title, 'uncertain')
            restored.close()

    def test_authority_exact_retries_conflicts_and_native_reopen(self):
        """Accepted outcomes and stale-base conflicts survive authority restart with exact original bytes."""
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'authority'
            store = self.store(); store.create_journal(path)
            authority = runtime.ManagedAuthority(store)
            first = runtime.ManagedClient(schema(), store.read(), 'one')
            second = runtime.ManagedClient(schema(), store.read(), 'two')
            calls = []
            def proposed_edit(draft):
                calls.append('once')
                draft.title = 'accepted'
            proposal = first.propose('request', proposed_edit)
            self.assertIs(first.retry(), proposal)
            self.assertEqual(first.read().root.title, 'baseline')
            receipt = authority.accept(proposal)
            self.assertIs(authority.accept(proposal), receipt)
            first.acknowledge(receipt)
            self.assertEqual(first.read().root.title, 'accepted')
            conflict_proposal = second.propose('request', lambda draft: setattr(draft, 'title', 'stale'))
            conflict = authority.accept(conflict_proposal)
            self.assertEqual(conflict.status, 'conflict')
            second.acknowledge(conflict)
            self.assertEqual(second.read().root.title, 'accepted')
            with self.assertRaises(runtime.ManagedError):
                authority.accept(replace(proposal, data=proposal.data + b' '))
            store.close()
            restored = runtime.ManagedStore.open_journal(schema(), path)
            reopened = runtime.ManagedAuthority(restored)
            self.assertEqual(reopened.accept(proposal), receipt)
            self.assertEqual(reopened.accept(conflict_proposal), conflict)
            self.assertEqual(calls, ['once'])
            restored.close()

    def test_recovery_repair_waits_for_complete_semantic_validation(self):
        """A wrong schema or valid-checksum invalid identity cannot cause even trailing bytes to change."""
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'state'
            store = self.store(); store.create_journal(path)
            store.close()
            original = path.read_bytes() + b'partial'
            path.write_bytes(original)
            other = schema(); other.schema_id = 'another.schema'
            with self.assertRaises(runtime.ManagedError):
                runtime.ManagedStore.open_journal(other, path)
            self.assertEqual(path.read_bytes(), original)
            # Build an independently checksummed complete acceptance with an illegal root ID.
            record = {'kind': 'accept', 'sequence': 1, 'epoch': 1, 'revision': 1, 'base_epoch': 0,
                      'data': base64.b64encode(initial().encode(2)).decode()}
            payload = json.dumps(record, sort_keys=True, separators=(',', ':')).encode()
            prefix = struct.pack('<I', len(payload))
            complete = original[:-len(b'partial')] + prefix + hashlib.sha256(prefix).digest() + payload + hashlib.sha256(payload).digest()
            original = complete + b'partial'
            path.write_bytes(original)
            with self.assertRaises(runtime.ManagedError):
                runtime.ManagedStore.open_journal(schema(), path)
            self.assertEqual(path.read_bytes(), original)

    def test_frame_length_corruption_is_not_mistaken_for_a_partial_tail(self):
        """The fixed length header has its own checksum, preventing accepted-frame loss after a bit flip."""
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'state'
            store = self.store(); store.create_journal(path)
            store.transact(lambda edit: setattr(edit.root, 'title', 'accepted'))
            store.close()
            original = bytearray(path.read_bytes())
            first_size, = struct.unpack_from('<I', original, len(runtime._ManagedJournal.MAGIC))
            second_header = len(runtime._ManagedJournal.MAGIC) + 4 + 32 + first_size + 32
            original[second_header] ^= 128
            path.write_bytes(original)
            with self.assertRaises(runtime.ManagedError):
                runtime.ManagedStore.open_journal(schema(), path)
            self.assertEqual(path.read_bytes(), original)

    def test_journal_total_and_record_bounds_precede_write(self):
        """Full log admission rejects before append and permits later safe mutation after limits recover."""
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'state'
            store = self.store(); store.create_journal(path)
            original = path.read_bytes()
            before = store.read()
            for limit in ('MAXIMUM_BYTES', 'MAXIMUM_RECORDS'):
                bound = len(original) if limit == 'MAXIMUM_BYTES' else 1
                with patch.object(runtime._ManagedJournal, limit, bound):
                    with self.assertRaises(runtime.ManagedError):
                        store.transact(lambda edit: setattr(edit.root, 'title', 'blocked'))
                self.assertEqual(path.read_bytes(), original)
                self.assertIs(store.read(), before)
            store.transact(lambda edit: setattr(edit.root, 'title', 'allowed'))
            self.assertEqual(store.read().root.title, 'allowed')
            store.close()

    def test_client_receipt_transitions_and_reentrancy(self):
        """Malformed counter types, backwards outcomes and caught nested client edits preserve pending state."""
        store = self.store()
        client = runtime.ManagedClient(schema(), store.read(), 'client')
        def nested(draft):
            with self.assertRaises(runtime.ManagedError):
                client.propose('nested', lambda value: None)
        with self.assertRaises(runtime.ManagedError):
            client.propose('request', nested)
        with self.assertRaises(runtime.ManagedError):
            client.retry()
        proposal = client.propose('request', lambda value: setattr(value, 'title', 'accepted'))
        receipt = runtime.ManagedAuthority(store).accept(proposal)
        for changes in ({'epoch': False}, {'revision': True}, {'epoch': 0}, {'epoch': 2},
                        {'revision': 0}, {'status': 'conflict', 'epoch': 0}):
            with self.assertRaises(runtime.ManagedError):
                client.acknowledge(replace(receipt, **changes))
            self.assertIs(client.retry(), proposal)
        client.acknowledge(receipt)
        self.assertEqual(client.read().root.title, 'accepted')

    def test_authority_rejects_root_replacement_before_identity_reservation(self):
        """A forged fresh root cannot replace the document root or consume durable reservations."""
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'root-binding'
            store = self.store(); store.create_journal(path)
            before, highwater, original = store.read(), store._highwater, path.read_bytes()
            value = store.schema.decode(before.data)
            value.persistent_id = 0
            proposal = runtime.ManagedProposal('client', 'replacement', store.document,
                store.schema.schema_id, before.epoch, store.schema.encode(value))
            try:
                with self.assertRaises(runtime.ManagedError):
                    runtime.ManagedAuthority(store).accept(proposal)
                self.assertIs(store.read(), before)
                self.assertEqual(store._highwater, highwater)
                self.assertEqual(path.read_bytes(), original)
            finally:
                store.close()

    def test_recovery_rejects_root_replacement_before_tail_repair(self):
        """Checksummed reserved IDs cannot change root lifetime during semantic journal replay."""
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'root-recovery'
            store = self.store(); store.create_journal(path)
            value = store.schema.decode(store.read().data)
            value.persistent_id = store._highwater + 1
            store._journal.append('reserve', {'highwater': value.persistent_id})
            store._journal.append('accept', {'base_epoch': 0, 'epoch': 1, 'revision': 1,
                'data': base64.b64encode(store.schema.encode(value)).decode('ascii')})
            store.close()
            with path.open('ab') as output:
                output.write(b'\x01')
            original = path.read_bytes()
            restored = None
            try:
                with self.assertRaises(runtime.ManagedError):
                    restored = runtime.ManagedStore.open_journal(schema(), path)
                self.assertEqual(path.read_bytes(), original)
            finally:
                if restored is not None:
                    restored.close()

    def test_client_rejects_changed_root_without_consuming_pending_request(self):
        """A malformed accepted receipt cannot replace the confirmed root in one document."""
        store = self.store()
        client = runtime.ManagedClient(schema(), store.read(), 'client')
        proposal = client.propose('request', lambda value: setattr(value, 'title', 'accepted'))
        receipt = runtime.ManagedAuthority(store).accept(proposal)
        invalid = store.schema.decode(receipt.data)
        invalid.persistent_id = store._highwater + 1
        before = client.read()
        with self.assertRaises(runtime.ManagedError):
            client.acknowledge(replace(receipt, data=store.schema.encode(invalid)))
        self.assertIs(client.read(), before)
        self.assertIs(client.retry(), proposal)
        client.acknowledge(receipt)
        self.assertEqual(client.read().root.title, 'accepted')
        store.close()

    def test_post_fsync_publication_failure_fences_and_replays(self):
        """Even failure between durable append and Python pointer publication requires reopen."""
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'state'
            store = self.store(); store.create_journal(path)
            before = store.read()
            append = runtime._ManagedJournal.append
            def interrupted(journal, kind, values):
                append(journal, kind, values)
                raise MemoryError('injected post-fsync interruption')
            with patch.object(runtime._ManagedJournal, 'append', interrupted):
                with self.assertRaises(runtime.ManagedIndeterminate):
                    store.transact(lambda edit: setattr(edit.root, 'title', 'durable'))
            self.assertIs(store.read(), before)
            with self.assertRaises(runtime.ManagedIndeterminate):
                store.undo()
            store.close()
            reopened = runtime.ManagedStore.open_journal(schema(), path)
            self.assertEqual(reopened.read().root.title, 'durable')
            reopened.close()

    def test_real_process_crash_boundaries_release_native_writer_lock(self):
        """OS process death releases locks and replays only complete accepted frames at every write boundary."""
        with tempfile.TemporaryDirectory() as folder:
            for phase in ('before_write', 'partial_header', 'partial_payload', 'partial_digest', 'after_flush'):
                path = Path(folder) / phase
                store = self.store(); store.create_journal(path); store.close()
                completed = subprocess.run([sys.executable, '-B', __file__, '--native-crash-helper', str(path), phase],
                                           capture_output=True, text=True, timeout=30)
                self.assertEqual(completed.returncode, 75, completed.stdout + completed.stderr)
                restored = runtime.ManagedStore.open_journal(schema(), path)
                self.assertEqual(restored.read().root.title, 'crashed' if phase == 'after_flush' else 'baseline')
                restored.transact(lambda edit: setattr(edit.root, 'title', 'post-recovery'))
                restored.close()


def crash_helper(path, phase):
    """Terminate the actual writer process without running destructors or closing lock handles."""
    store = runtime.ManagedStore.open_journal(schema(), path)
    append = runtime._ManagedJournal.append
    def interrupt(journal, kind, values):
        if phase == 'before_write':
            os._exit(75)
        if phase == 'after_flush':
            append(journal, kind, values)
            os._exit(75)
        record = dict(values, kind=kind, sequence=journal.sequence)
        payload = json.dumps(record, sort_keys=True, separators=(',', ':')).encode()
        prefix = struct.pack('<I', len(payload))
        frame = prefix + hashlib.sha256(prefix).digest() + payload + hashlib.sha256(payload).digest()
        end = {'partial_header': len(prefix) + 11,
               'partial_payload': len(prefix) + 32 + len(payload) // 2,
               'partial_digest': len(frame) - 7}[phase]
        journal.file.seek(0, 2)
        journal.file.write(frame[:end]); journal.file.flush(); os.fsync(journal.file.fileno())
        os._exit(75)
    runtime._ManagedJournal.append = interrupt
    store.transact(lambda edit: setattr(edit.root, 'title', 'crashed'))
    raise AssertionError('Crash helper unexpectedly returned')


if __name__ == '__main__':
    if len(sys.argv) == 4 and sys.argv[1] == '--native-crash-helper':
        crash_helper(sys.argv[2], sys.argv[3])
    else:
        unittest.main(verbosity=2)
