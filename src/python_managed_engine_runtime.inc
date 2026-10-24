// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"python(
import base64 as _mg_base64
import hashlib as _mg_hashlib
import inspect as _mg_inspect
import json as _mg_json
import os as _mg_os
from pathlib import Path as _mg_Path
import struct as _mg_struct
import threading as _mg_threading
from dataclasses import dataclass as _mg_dataclass
from types import MappingProxyType as _mg_readonly_map
import uuid as _mg_uuid


def _mg_unsigned(value, bits):
    """Check exact integer identity/counter metadata without admitting bools or rounded numbers."""
    return type(value) is int and 0 <= value < (1 << bits)


def _mg_document(value):
    """Validate a nonzero native document namespace independently of serialized payload data."""
    return type(value) is tuple and len(value) == 2 and any(value) and all(_mg_unsigned(part, 64) for part in value)


def _mg_callback(callback, value):
    """Require synchronous void completion; reject awaitables before any candidate is accepted."""
    result = callback(value)
    if result is not None:
        if _mg_inspect.iscoroutine(result):
            result.close()
        raise ManagedError('Synchronous managed callbacks must return None')


def _mg_json_object(pairs):
    """Reject duplicate native journal metadata instead of silently selecting its final occurrence."""
    result = {}
    for key, value in pairs:
        if key in result:
            raise ManagedError('Duplicate managed journal metadata')
        result[key] = value
    return result


class ManagedError(RuntimeError):
    """A native managed-state contract failed without publishing a candidate."""


class ManagedIndeterminate(ManagedError):
    """Durable acceptance is uncertain; close and reopen before any further mutation."""


class ManagedConflict(ManagedError):
    """The proposal's exact document epoch is no longer current."""


@_mg_dataclass(frozen=True, slots=True)
class ManagedField:
    """Generated field metadata: scalar/record type or optional/array/map/token shape."""
    name: str
    shape: object
    managed: bool = False


@_mg_dataclass(frozen=True, slots=True)
class ManagedRecord:
    """Generated durable record metadata; class_id is zero for ordinary records."""
    class_id: int
    fields: tuple


@_mg_dataclass(frozen=True, slots=True)
class ManagedToken:
    """Immutable native identity view; possession does not grant editing permission."""
    document_high: int
    document_low: int
    class_id: int
    object_id: int


class _ManagedFrozenRecord:
    __slots__ = ('_fields',)

    def __init__(self, fields):
        """Retain an immutable graph containing no original mutable record aliases."""
        object.__setattr__(self, '_fields', _mg_readonly_map(fields))

    def __getattr__(self, name):
        """Expose declared generated fields only."""
        try:
            return self._fields[name]
        except KeyError:
            raise AttributeError(name) from None

    def __setattr__(self, name, value):
        """Reject mutation of every retained read pin."""
        raise AttributeError('Managed snapshots are immutable')

    def __delattr__(self, name):
        """Reject field removal from retained read pins."""
        raise AttributeError('Managed snapshots are immutable')


@_mg_dataclass(frozen=True, slots=True)
class ManagedSnapshot:
    """An immutable coherent pin; its graph and bytes outlive its originating store."""
    root: object
    epoch: int
    revision: int
    document: tuple
    schema_id: str
    data: bytes
    _entities: object

    def resolve(self, token):
        """Resolve exact document/type/object identity in this snapshot; stale edges fail."""
        if (not all(_mg_unsigned(value, 64) for value in
                    (token.document_high, token.document_low, token.class_id, token.object_id)) or
                not token.class_id or not token.object_id):
            raise ManagedError('Managed reference contains invalid native integer metadata')
        if (token.document_high, token.document_low) != self.document:
            raise ManagedError('Managed reference belongs to another document')
        try:
            return self._entities[token.class_id, token.object_id]
        except KeyError:
            raise ManagedError('Managed reference is not live in this snapshot') from None


@_mg_dataclass(frozen=True, slots=True)
class ManagedProposal:
    """One immutable retryable wire request; bytes and base epoch are never recomputed."""
    client_id: str
    request_id: str
    document: tuple
    schema_id: str
    base_epoch: int
    data: bytes


@_mg_dataclass(frozen=True, slots=True)
class ManagedReceipt:
    """A durable original outcome, retained across exact retries and authority reopen."""
    client_id: str
    request_id: str
    status: str
    base_epoch: int
    epoch: int
    revision: int
    document: tuple
    schema_id: str
    data: bytes


class ManagedSchema:
    def __init__(self, root_type, protocol, schema_id, records, reference_type=None,
                 id_bytes=4, max_bytes=64 * 1024 * 1024, max_entities=1000000, limits=None):
        """Bind generated shapes and native codecs; callbacks/opaque custom storage are excluded."""
        if (type(id_bytes) is not int or id_bytes not in (4, 8) or type(schema_id) is not str or not schema_id or
                any(type(value) is not int or value < 1 for value in (max_bytes, max_entities))):
            raise ValueError('Invalid managed schema limits or identity contract')
        copied = {}
        tags = set()
        for record_type, record in records.items():
            if not isinstance(record, ManagedRecord) or not isinstance(record.fields, tuple):
                raise ValueError('Managed records require immutable generated metadata')
            if not _mg_unsigned(record.class_id, 64):
                raise ValueError('Managed class identity requires an exact unsigned uint64')
            if record.class_id:
                if record.class_id in tags or not 0 < record.class_id < 1 << 64:
                    raise ValueError('Managed class identity collides or exceeds uint64')
                tags.add(record.class_id)
            names = [field.name for field in record.fields]
            if (len(set(names)) != len(names) or (record.class_id and 'persistent_id' in names) or
                    any(name == '_fields' or name.startswith('__') for name in names)):
                raise ValueError('Managed field metadata collides with identity')
            copied[record_type] = record
        if root_type not in copied or not copied[root_type].class_id:
            raise ValueError('Managed root requires exact generated class metadata')
        self.root_type, self.protocol, self.schema_id = root_type, protocol, schema_id
        self.records = _mg_readonly_map(copied)
        self.reference_type, self.id_bytes = reference_type, id_bytes
        self.max_bytes, self.max_entities, self.limits = max_bytes, max_entities, limits

    def clone(self, value):
        """Copy declared owning fields only, rejecting cycles and all unknown mutable values."""
        active = set()
        def copy(item, shape):
            if isinstance(shape, tuple):
                kind = shape[0]
                if kind == 'token':
                    if self.reference_type is None or type(item) is not self.reference_type:
                        raise ManagedError('Expected generated managed reference token')
                    return self.reference_type(item.document_high, item.document_low, item.class_id, item.object_id)
                if kind == 'optional':
                    if type(item) is not tuple or len(item) > 1:
                        raise ManagedError('Invalid generated optional')
                    return tuple(copy(child, shape[1]) for child in item)
                if kind == 'array':
                    if type(item) is not list or (len(shape) == 3 and len(item) != shape[2]):
                        raise ManagedError('Invalid generated array')
                    return [copy(child, shape[1]) for child in item]
                if kind == 'map':
                    if type(item) is not dict:
                        raise ManagedError('Invalid generated map')
                    return {copy(key, shape[1]): copy(child, shape[2]) for key, child in item.items()}
                raise ManagedError('Unknown generated managed shape')
            if shape in self.records:
                if type(item) is not shape or id(item) in active:
                    raise ManagedError('Unexpected record type or recursive owning value')
                active.add(id(item))
                try:
                    result = shape()
                    metadata = self.records[shape]
                    if metadata.class_id:
                        result.persistent_id = item.persistent_id
                    for field in metadata.fields:
                        setattr(result, field.name, copy(getattr(item, field.name), field.shape))
                    return result
                finally:
                    active.remove(id(item))
            if shape is float and type(item) in (int, float):
                try:
                    return float(item)
                except OverflowError as error:
                    raise ManagedError('Managed floating-point value exceeds native range') from error
            if type(item) is not shape or not isinstance(item, (str, bytes, int, float, bool)):
                raise ManagedError('Unsupported native managed scalar metadata')
            return item
        return copy(value, self.root_type)

    def encode(self, value):
        """Normalize exact declared shapes before invoking generated codecs and checking snapshot bytes."""
        data = bytes(self.clone(value).encode(self.protocol))
        if len(data) > self.max_bytes:
            raise ManagedError('Managed snapshot exceeds byte bound')
        return data

    def decode(self, data):
        """Decode one exact generated message under native codec and snapshot limits."""
        if type(data) is not bytes or len(data) > self.max_bytes:
            raise ManagedError('Managed snapshot exceeds byte bound or is not immutable bytes')
        value = self.root_type.decode(data, self.protocol, self.limits) if self.limits is not None else self.root_type.decode(data, self.protocol)
        return self.clone(value)

    def walk(self, value):
        """Visit logical owned occurrences, independently of container addresses or storage reuse."""
        active = set()
        def descend(item, shape, managed):
            if isinstance(shape, tuple):
                kind = shape[0]
                if kind == 'token':
                    yield ('token', item, shape[1], False)
                elif kind in ('array', 'optional'):
                    for child in item:
                        yield from descend(child, shape[1], managed)
                elif kind == 'map':
                    for child in item.values():
                        yield from descend(child, shape[2], managed)
                else:
                    raise ManagedError('Unknown generated managed shape')
            elif shape in self.records:
                if id(item) in active or type(item) is not shape:
                    raise ManagedError('Recursive ownership or mismatched record type')
                active.add(id(item))
                try:
                    yield ('record', item, shape, managed)
                    for field in self.records[shape].fields:
                        yield from descend(getattr(item, field.name), field.shape, managed and field.managed)
                finally:
                    active.remove(id(item))
        yield from descend(value, self.root_type, True)

    def identify(self, value, document, highwater, bindings=None, existing=None, allocate=False,
                 validate_references=True):
        """Assign fresh logical IDs and reject forged, duplicate or dangling typed identities."""
        entities, references, object_ids = {}, [], set()
        maximum = (1 << (8 * self.id_bytes)) - 1
        for kind, item, record_type, managed in self.walk(value):
            if kind == 'token':
                references.append((item, record_type))
                continue
            metadata = self.records[record_type]
            if not metadata.class_id:
                if managed:
                    raise ManagedError('Managed occurrence lacks a concrete class identity')
                continue
            identity = item.persistent_id
            if type(identity) is not int or not 0 <= identity <= maximum:
                raise ManagedError('Managed object ID exceeds selected native width')
            if not managed:
                if identity:
                    raise ManagedError('Ordinary value occurrence cannot retain a managed identity')
                continue
            if allocate:
                if bindings is not None:
                    expected = bindings.get(id(item), 0)
                    if identity != expected:
                        raise ManagedError('Edit changed or imported a managed identity')
                elif identity and (existing is None or (metadata.class_id, identity) not in existing):
                    raise ManagedError('Proposal imported an unknown managed identity')
                if not identity:
                    if highwater >= maximum:
                        raise ManagedError('Managed object ID space exhausted')
                    highwater += 1
                    identity = item.persistent_id = highwater
            if identity == 0 or identity > highwater or (metadata.class_id, identity) in entities:
                raise ManagedError('Unassigned, repeated or future managed identity')
            if identity in object_ids:
                raise ManagedError('Managed object IDs are document-wide unique')
            object_ids.add(identity)
            entities[metadata.class_id, identity] = item
            if len(entities) > self.max_entities:
                raise ManagedError('Managed entity count exceeds bound')
        for token, target in references if validate_references else ():
            if (not all(_mg_unsigned(value, 64) for value in
                        (token.document_high, token.document_low, token.class_id)) or
                    not _mg_unsigned(token.object_id, 8 * self.id_bytes) or not token.object_id or
                    (token.document_high, token.document_low) != document or
                    token.class_id != self.records[target].class_id or
                    (token.class_id, token.object_id) not in entities):
                raise ManagedError('Managed token scope, exact type or liveness is invalid')
        return highwater, entities

    def snapshot(self, value, document, highwater, epoch, revision):
        """Freeze a canonical decoded graph without lending any candidate-owned mutable alias."""
        data = self.encode(value)
        decoded = self.decode(data)
        _, entities = self.identify(decoded, document, highwater)
        frozen_by_object = {}
        def freeze(item, shape):
            if isinstance(shape, tuple):
                if shape[0] == 'token':
                    return ManagedToken(item.document_high, item.document_low, item.class_id, item.object_id)
                if shape[0] in ('array', 'optional'):
                    return tuple(freeze(child, shape[1]) for child in item)
                return _mg_readonly_map({key: freeze(child, shape[2]) for key, child in item.items()})
            if shape not in self.records:
                return item
            metadata = self.records[shape]
)python"
R"python(            fields = {field.name: freeze(getattr(item, field.name), field.shape) for field in metadata.fields}
            if metadata.class_id:
                fields['persistent_id'] = item.persistent_id
            result = _ManagedFrozenRecord(fields)
            frozen_by_object[id(item)] = result
            return result
        root = freeze(decoded, self.root_type)
        registry = _mg_readonly_map({key: frozen_by_object[id(item)] for key, item in entities.items()})
        return ManagedSnapshot(root, epoch, revision, document, self.schema_id, data, registry)


class _ManagedJournal:
    MAGIC = b'SRLPYMG1'
    HEADER = _mg_struct.Struct('<I')
    DIGEST_BYTES = _mg_hashlib.sha256().digest_size
    MAXIMUM_BYTES = 256 * 1024 * 1024
    MAXIMUM_RECORDS = 65536

    def __init__(self, path, maximum, create=False):
        """Acquire one OS writer lock; process termination releases it without deleting user files."""
        self.path = _mg_Path(path).resolve()
        self.maximum, self.sequence, self.file, self.lock = maximum, 0, None, None
        self._repair_offset = None
        self.lock = open(str(self.path) + '.lock', 'a+b')
        try:
            if _mg_os.name == 'nt':
                import msvcrt
                self.lock.seek(0, 2)
                if self.lock.tell() == 0:
                    self.lock.write(b'\0'); self.lock.flush()
                self.lock.seek(0)
                msvcrt.locking(self.lock.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(self.lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            self.file = open(self.path, 'x+b' if create else 'r+b')
            if create:
                self.file.write(self.MAGIC); self.file.flush(); _mg_os.fsync(self.file.fileno())
        except BaseException:
            self.close()
            raise

    def append(self, kind, values):
        """Persist a bounded canonical frame; any write/flush uncertainty requires reopen."""
        record = dict(values, kind=kind, sequence=self.sequence)
        payload = _mg_json.dumps(record, sort_keys=True, separators=(',', ':'), ensure_ascii=True, allow_nan=False).encode('ascii')
        if len(payload) > self.maximum:
            raise ManagedError('Managed journal frame exceeds bound')
        prefix = self.HEADER.pack(len(payload))
        frame = prefix + _mg_hashlib.sha256(prefix).digest() + payload + _mg_hashlib.sha256(payload).digest()
        self.file.seek(0, 2)
        if self.sequence >= self.MAXIMUM_RECORDS or self.file.tell() + len(frame) > self.MAXIMUM_BYTES:
            raise ManagedError('Managed journal capacity exhausted; explicit rebaseline is required')
        try:
            self.file.seek(0, 2)
            if self.file.write(frame) != len(frame):
                raise OSError('Short managed journal write')
            self.file.flush()
            _mg_os.fsync(self.file.fileno())
            self.sequence += 1
        except BaseException as error:
            raise ManagedIndeterminate('Managed journal acceptance is uncertain; reopen required') from error

    def recover(self):
        """Verify bounded complete frames without changing any byte before semantic replay succeeds."""
        self.file.seek(0)
        if _mg_os.fstat(self.file.fileno()).st_size > self.MAXIMUM_BYTES:
            raise ManagedError('Managed journal exceeds recovery file bound')
        if self.file.read(len(self.MAGIC)) != self.MAGIC:
            raise ManagedError('Unknown native Python managed journal format')
        records = []
        complete = self.file.tell()
        while True:
            prefix = self.file.read(self.HEADER.size)
            if not prefix:
                break
            if len(prefix) != self.HEADER.size:
                self._repair_offset = complete; break
            prefix_digest = self.file.read(self.DIGEST_BYTES)
            if len(prefix_digest) != self.DIGEST_BYTES:
                self._repair_offset = complete; break
            if prefix_digest != _mg_hashlib.sha256(prefix).digest():
                raise ManagedError('Managed journal frame header checksum mismatch')
            size, = self.HEADER.unpack(prefix)
            if size > self.maximum:
                raise ManagedError('Managed journal frame exceeds recovery bound')
            payload = self.file.read(size)
            digest = self.file.read(self.DIGEST_BYTES)
            if len(payload) != size or len(digest) != self.DIGEST_BYTES:
                self._repair_offset = complete; break
            if digest != _mg_hashlib.sha256(payload).digest():
                raise ManagedError('Managed journal complete frame checksum mismatch')
            if len(records) >= self.MAXIMUM_RECORDS:
                raise ManagedError('Managed journal exceeds recovery record bound')
            record = _mg_json.loads(payload, object_pairs_hook=_mg_json_object)
            if type(record) is not dict or not _mg_unsigned(record.get('sequence'), 64) or record['sequence'] != len(records):
                raise ManagedError('Managed journal sequence mismatch')
            records.append(record)
            complete = self.file.tell()
        self.sequence = len(records)
        return records

    def repair(self):
        """Flush the fully validated recovered generation before exposing any retained acceptance outcome."""
        try:
            if self._repair_offset is not None:
                self.file.truncate(self._repair_offset)
            self.file.flush()
            _mg_os.fsync(self.file.fileno())
            self._repair_offset = None
        except BaseException as error:
            raise ManagedIndeterminate('Managed recovery flush is uncertain; reopen required') from error

    def close(self):
        """Release file and native lock handles; preserve the stable writer-lock companion."""
        file, self.file = self.file, None
        try:
            if file is not None:
                file.close()
        finally:
            lock, self.lock = self.lock, None
            if lock is not None:
                lock.close()


class ManagedEdit:
    __slots__ = ('_store', '_root', '_active', '_entities')

    def __init__(self, store, root):
        """Bind one synchronous isolated draft; references retained by callers remain detached."""
        self._store, self._root, self._active, self._entities = store, root, True, None

    @property
    def root(self):
        """Borrow this callback's mutable draft, never the accepted snapshot graph."""
        if not self._active:
            raise ManagedError('Managed edit callback has ended')
        return self._root

    def token(self, value):
        """Build exact typed references during the post-identity reference-initialization phase."""
        if not self._active or self._entities is None or self._store.schema.reference_type is None:
            raise ManagedError('Managed token builder requires the reference-initialization phase')
        for (tag, identity), item in self._entities.items():
            if item is value:
                return self._store.schema.reference_type(*self._store.document, tag, identity)
        raise ManagedError('Managed token target is not a live owned occurrence')


class ManagedStore:
    def __init__(self, schema, initial, document=None, max_history_entries=256,
                 max_history_bytes=64 * 1024 * 1024, max_receipts=4096, initialize=None):
        """Own an isolated native graph; mutation is thread-confined, immutable read pins are shareable."""
        if any(type(value) is not int or value < 1 for value in (max_history_entries, max_history_bytes, max_receipts)):
            raise ValueError('Managed store bounds must be positive')
        if document is None:
            bits = _mg_uuid.uuid4().int
            document = (bits >> 64, bits & ((1 << 64) - 1))
        if not _mg_document(document):
            raise ValueError('Managed document requires a nonzero uint64 pair')
        self.schema, self.document = schema, document
        self._owner = _mg_threading.current_thread()
        self._lock = _mg_threading.Lock()
        self._busy, self._poison, self._fenced, self._journal = False, None, False, None
        self._max_entries, self._max_history, self._max_receipts = max_history_entries, max_history_bytes, max_receipts
        self._receipts, self._epoch, self._revision, self._highwater = {}, 0, 0, 0
        value = schema.clone(initial)
        self._highwater, entities = schema.identify(value, document, 0, bindings={}, allocate=True,
                                                  validate_references=False)
        if initialize is not None:
            edit = ManagedEdit(self, value); edit._entities = entities
            try:
                _mg_callback(initialize, edit)
            finally:
                edit._active = False
            self._check_reference_phase(value, entities)
        current = schema.snapshot(value, document, self._highwater, 0, 0)
        if len(current.data) > self._max_history:
            raise ManagedError('Initial snapshot exceeds history bound')
        self._history, self._cursor, self._current = [current], 0, current

    def _check_mutation(self):
        """Reject cross-thread, nested or uncertain-durability mutation before user code runs."""
        if _mg_threading.current_thread() is not self._owner:
            raise ManagedError('Managed mutations are confined to the creating thread')
        if self._fenced:
            raise ManagedIndeterminate('Managed store is fenced; close and reopen required')
        if self._busy:
            self._poison = ManagedError('Nested managed mutation is forbidden')
            raise self._poison

    def _append(self, kind, values):
        """Fence only uncertain durable writes; all encoding/admission failures precede append."""
        if self._journal is not None:
            try:
                self._journal.append(kind, values)
            except ManagedIndeterminate:
                self._fenced = True
                raise

    def _publish_prepared(self, kind, values, apply):
        """Fence interruption after durable acceptance; publication callbacks are private prepared swaps only."""
        sequence = self._journal.sequence if self._journal is not None else None
        try:
            self._append(kind, values)
            apply()
        except ManagedIndeterminate:
            self._fenced = True
            raise
        except BaseException as error:
            if self._journal is None or self._journal.sequence != sequence:
                self._fenced = True
                if self._journal is not None:
                    raise ManagedIndeterminate('Managed publication interrupted after durable acceptance; reopen required') from error
                raise ManagedError('Managed in-memory publication interrupted; store fenced') from error
            raise

    def _check_reference_phase(self, value, entities):
        """Reference initialization may set edges, but cannot alter owned lifetimes or IDs."""
        _, actual = self.schema.identify(value, self.document, self._highwater)
        if actual.keys() != entities.keys() or any(actual[key] is not entities[key] for key in actual):
            raise ManagedError('Reference initialization changed logical ownership')

    def read(self):
        """Return one immutable coherent pin; pins remain valid after edits and store close."""
        with self._lock:
            return self._current

    def _reserve(self, highwater):
        """Persist newly observable ID reservations separately, so failed edits never reuse them."""
        if highwater != self._highwater:
            def apply():
                """Publish the already durable logical ID high-water mark."""
                self._highwater = highwater
            self._publish_prepared('reserve', {'highwater': highwater}, apply)

    def _prepare(self, value):
        """Prepare immutable publication and bounded linear history before any acceptance write."""
        if value.persistent_id != self._current.root.persistent_id:
            raise ManagedError('Managed acceptance cannot replace the document root identity')
        if self._epoch >= (1 << 64) - 1 or self._revision >= (1 << 64) - 1:
            raise ManagedError('Managed acceptance epoch or revision space exhausted')
        snapshot = self.schema.snapshot(value, self.document, self._highwater, self._epoch + 1, self._revision + 1)
        if len(snapshot.data) > self._max_history:
            raise ManagedError('Managed snapshot exceeds history bound')
        history = self._history[:self._cursor + 1] + [snapshot]
        while len(history) > self._max_entries or sum(len(item.data) for item in history) > self._max_history:
            history.pop(0)
        return snapshot, history

    def _publish(self, snapshot, history, receipt_entry=None):
        """Append state and optional exact retry receipt atomically, then swap prepared in-memory state."""
        receipts = dict(self._receipts)
        values = {'data': _mg_base64.b64encode(snapshot.data).decode('ascii'), 'epoch': snapshot.epoch,
                  'revision': snapshot.revision, 'base_epoch': self._epoch}
        if receipt_entry is not None:
            proposal, receipt = receipt_entry
            values['receipt'] = self._receipt_wire(proposal, receipt)
            receipts[proposal.client_id, proposal.request_id] = receipt_entry
        cursor = len(history) - 1
        def apply():
            """Publish only preconstructed graph/history/receipt objects after acceptance."""
            with self._lock:
                self._history, self._cursor = history, cursor
                self._current, self._epoch, self._revision, self._receipts = snapshot, snapshot.epoch, snapshot.revision, receipts
        self._publish_prepared('accept', values, apply)
        return snapshot

    def transact(self, callback, initialize_references=None):
        """Execute once on an isolated draft; failure preserves accepted state/history and burns reserved IDs."""
        self._check_mutation()
        self._busy, self._poison = True, None
        edit = None
        try:
            value = self.schema.decode(self._current.data)
            bindings = {id(item): item.persistent_id for kind, item, _, managed in self.schema.walk(value)
                        if kind == 'record' and managed}
            edit = ManagedEdit(self, value)
            _mg_callback(callback, edit)
            if self._poison is not None:
                raise self._poison
            highwater, entities = self.schema.identify(value, self.document, self._highwater,
                bindings=bindings, allocate=True, validate_references=False)
            self._reserve(highwater)
            if initialize_references is not None:
                edit._entities = entities
                _mg_callback(initialize_references, edit)
                self._check_reference_phase(value, entities)
            if self._poison is not None:
                raise self._poison
            snapshot, history = self._prepare(value)
            if snapshot.data == self._current.data:
                return self._current
            return self._publish(snapshot, history)
        finally:
            if edit is not None:
                edit._active = False
            self._busy = False

    def _navigate(self, offset):
        """Restore a retained snapshot through a new acceptance epoch without recycling identity counters."""
        self._check_mutation()
        if offset not in (-1, 1):
            raise ManagedError('Managed navigation requires one retained adjacent revision')
        if self._epoch >= (1 << 64) - 1:
)python"
R"python(            raise ManagedError('Managed acceptance epoch space exhausted')
        cursor = self._cursor + offset
        if not 0 <= cursor < len(self._history):
            raise ManagedError('No retained managed history destination')
        previous = self._history[cursor]
        snapshot = ManagedSnapshot(previous.root, self._epoch + 1, previous.revision, self.document,
                                   self.schema.schema_id, previous.data, previous._entities)
        def apply():
            """Swap the preconstructed retained navigation pin and monotonic acceptance epoch."""
            with self._lock:
                self._cursor, self._current, self._epoch = cursor, snapshot, snapshot.epoch
        self._publish_prepared('navigate', {'base_epoch': self._epoch, 'epoch': snapshot.epoch, 'cursor': cursor}, apply)
        return snapshot

    def undo(self):
        """Undo one retained linear revision, preserving immutable old pins and monotonically increasing epochs."""
        return self._navigate(-1)

    def redo(self):
        """Redo one retained linear revision; a new acceptance after undo discards the old redo branch."""
        return self._navigate(1)

    @staticmethod
    def _receipt_wire(proposal, receipt):
        """Retain exact immutable request bytes and the original outcome in the same durable frame."""
        return {'client': proposal.client_id, 'request': proposal.request_id, 'base_epoch': proposal.base_epoch,
                'request_data': _mg_base64.b64encode(proposal.data).decode('ascii'), 'status': receipt.status,
                'epoch': receipt.epoch, 'revision': receipt.revision,
                'data': _mg_base64.b64encode(receipt.data).decode('ascii')}

    def _record_receipt(self, proposal, receipt):
        """Publish one prepared rejection receipt durably without changing document epoch or history."""
        receipts = dict(self._receipts)
        receipts[proposal.client_id, proposal.request_id] = proposal, receipt
        values = {'receipt': self._receipt_wire(proposal, receipt)}
        def apply():
            """Swap the preallocated exact retry ledger after receipt acceptance."""
            self._receipts = receipts
        self._publish_prepared('receipt', values, apply)

    def create_journal(self, path):
        """Create a new native baseline; never overwrite an existing destination or its recoverable generation."""
        self._check_mutation()
        if self._journal is not None or self._epoch or len(self._receipts):
            raise ManagedError('Create a native journal before the first edit or authority request')
        maximum = 3 * self.schema.max_bytes + 65536
        journal = _ManagedJournal(path, maximum, create=True)
        try:
            journal.append('baseline', {'schema': self.schema.schema_id, 'document': self.document,
                'data': _mg_base64.b64encode(self._current.data).decode('ascii'), 'highwater': self._highwater,
                'max_entries': self._max_entries, 'max_history': self._max_history, 'max_receipts': self._max_receipts})
        except BaseException:
            journal.close()
            raise
        self._journal = journal

    @classmethod
    def open_journal(cls, schema, path):
        """Recover exact complete native records without rerunning transactions or authority callbacks."""
        journal = _ManagedJournal(path, 3 * schema.max_bytes + 65536)
        try:
            records = journal.recover()
            if not records or records[0].get('kind') != 'baseline':
                raise ManagedError('Managed journal lacks a complete baseline')
            first = records[0]
            if first['schema'] != schema.schema_id:
                raise ManagedError('Managed journal schema binding mismatch')
            document = tuple(first['document'])
            if (not _mg_document(document) or not _mg_unsigned(first['highwater'], 8 * schema.id_bytes) or
                    any(type(first[key]) is not int or first[key] < 1 for key in ('max_entries', 'max_history', 'max_receipts'))):
                raise ManagedError('Invalid managed journal baseline limits or identity metadata')
            value = schema.decode(_mg_base64.b64decode(first['data'], validate=True))
            # Construction allocates IDs only for fresh values; recovered values already carry them.
            restored = cls.__new__(cls)
            restored.schema, restored.document = schema, document
            restored._owner, restored._lock = _mg_threading.current_thread(), _mg_threading.Lock()
            restored._busy, restored._poison, restored._fenced, restored._journal = False, None, False, None
            restored._max_entries, restored._max_history, restored._max_receipts = first['max_entries'], first['max_history'], first['max_receipts']
            restored._receipts, restored._epoch, restored._revision, restored._highwater = {}, 0, 0, first['highwater']
            restored._current = schema.snapshot(value, document, restored._highwater, 0, 0)
            if len(restored._current.data) > restored._max_history:
                raise ManagedError('Managed journal baseline exceeds retained history bound')
            restored._history, restored._cursor = [restored._current], 0
            pending_identity_floor = None
            for record in records[1:]:
                kind = record['kind']
                if kind == 'reserve':
                    if not _mg_unsigned(record['highwater'], 8 * schema.id_bytes) or record['highwater'] <= restored._highwater:
                        raise ManagedError('Managed journal identity reservation is not monotonic')
                    pending_identity_floor = restored._highwater
                    restored._highwater = record['highwater']
                elif kind == 'accept':
                    if (not all(_mg_unsigned(record[key], 64) for key in ('base_epoch', 'epoch', 'revision')) or
                            record['base_epoch'] != restored._epoch or record['epoch'] != restored._epoch + 1 or record['revision'] != restored._revision + 1):
                        raise ManagedError('Managed journal acceptance sequence mismatch')
                    value = schema.decode(_mg_base64.b64decode(record['data'], validate=True))
                    snapshot, history = restored._prepare(value)
                    if any(pending_identity_floor is None or key[1] <= pending_identity_floor
                           for key in snapshot._entities.keys() - restored._current._entities.keys()):
                        raise ManagedError('Managed journal acceptance reused an unreserved logical identity')
                    restored._publish(snapshot, history)
                    pending_identity_floor = None
                elif kind == 'navigate':
                    if (not all(_mg_unsigned(record[key], 64) for key in ('base_epoch', 'epoch', 'cursor')) or
                            record['base_epoch'] != restored._epoch or record['epoch'] != restored._epoch + 1):
                        raise ManagedError('Managed journal navigation sequence mismatch')
                    restored._navigate(record['cursor'] - restored._cursor)
                    pending_identity_floor = None
                elif kind != 'receipt':
                    raise ManagedError('Unknown managed journal record')
                if 'receipt' in record:
                    entry = record['receipt']
                    proposal = ManagedProposal(entry['client'], entry['request'], document, schema.schema_id,
                        entry['base_epoch'], _mg_base64.b64decode(entry['request_data'], validate=True))
                    receipt = ManagedReceipt(entry['client'], entry['request'], entry['status'], entry['base_epoch'],
                        entry['epoch'], entry['revision'], document, schema.schema_id,
                        _mg_base64.b64decode(entry['data'], validate=True))
                    if (not all(_mg_unsigned(entry[key], 64) for key in ('base_epoch', 'epoch', 'revision')) or
                            entry['epoch'] != restored._epoch or entry['revision'] != restored._current.revision or
                            receipt.data != restored._current.data or len(proposal.data) > schema.max_bytes or
                            not all(type(entry[key]) is str and 0 < len(entry[key]) <= 256 for key in ('client', 'request')) or
                            (kind == 'accept' and (receipt.status != 'accepted' or receipt.epoch != receipt.base_epoch + 1)) or
                            (kind == 'receipt' and (receipt.status not in ('conflict', 'rejected') or
                                (receipt.status == 'conflict' and receipt.epoch <= receipt.base_epoch)))):
                        raise ManagedError('Managed journal receipt binding mismatch')
                    key = proposal.client_id, proposal.request_id
                    if key in restored._receipts or len(restored._receipts) >= restored._max_receipts:
                        raise ManagedError('Managed journal retry ledger collision or limit')
                    restored._receipts[key] = proposal, receipt
            journal.repair()
            restored._journal = journal
            return restored
        except BaseException:
            journal.close()
            raise

    def close(self):
        """Release writer handles on the owner thread; immutable read pins retain their own values."""
        if _mg_threading.current_thread() is not self._owner or self._busy:
            raise ManagedError('Managed close requires an idle owner thread')
        journal, self._journal = self._journal, None
        try:
            if journal is not None:
                journal.close()
        finally:
            self._fenced = True


class ManagedAuthority:
    def __init__(self, store):
        """Serialize proposals through one native owner; host authentication/authorization remains external."""
        self.store = store

    def accept(self, proposal):
        """Durably accept once or return the exact retained original receipt without rerunning edits."""
        store = self.store
        store._check_mutation()
        if (type(proposal) is not ManagedProposal or proposal.document != store.document or
                proposal.schema_id != store.schema.schema_id or
                not all(type(key) is str and 0 < len(key) <= 256 for key in (proposal.client_id, proposal.request_id)) or
                not _mg_unsigned(proposal.base_epoch, 64) or
                type(proposal.data) is not bytes or len(proposal.data) > store.schema.max_bytes):
            raise ManagedError('Invalid authority request binding or bound')
        key = proposal.client_id, proposal.request_id
        if key in store._receipts:
            previous, receipt = store._receipts[key]
            if proposal != previous:
                raise ManagedError('Retry ID was reused with different request bytes or base')
            return receipt
        if len(store._receipts) >= store._max_receipts:
            raise ManagedError('Authority exact retry ledger is full')
        if proposal.base_epoch > store._epoch:
            raise ManagedError('Authority proposal selects an unobserved future epoch')
        if proposal.base_epoch != store._epoch:
            current = store.read()
            receipt = ManagedReceipt(*key, 'conflict', proposal.base_epoch, current.epoch, current.revision,
                                     store.document, store.schema.schema_id, current.data)
            store._record_receipt(proposal, receipt)
            return receipt
        store._busy, store._poison = True, None
        try:
            value = store.schema.decode(proposal.data)
            if value.persistent_id != store.read().root.persistent_id:
                raise ManagedError('Authority proposal cannot replace the document root identity')
            highwater, _ = store.schema.identify(value, store.document, store._highwater,
                existing=store.read()._entities, allocate=True)
            store._reserve(highwater)
            snapshot, history = store._prepare(value)
            receipt = ManagedReceipt(*key, 'accepted', proposal.base_epoch, snapshot.epoch, snapshot.revision,
                                     store.document, store.schema.schema_id, snapshot.data)
            store._publish(snapshot, history, (proposal, receipt))
            return receipt
        finally:
            store._busy = False


class ManagedClient:
    def __init__(self, schema, snapshot, client_id):
        """Own one confirmed immutable baseline and at most one exact, explicitly pending proposal."""
        if (type(snapshot) is not ManagedSnapshot or snapshot.schema_id != schema.schema_id or
                type(client_id) is not str or not 0 < len(client_id) <= 256 or
                not _mg_document(snapshot.document) or
                not all(_mg_unsigned(value, 64) for value in (snapshot.epoch, snapshot.revision))):
            raise ManagedError('Client baseline or identity is invalid')
        value = schema.decode(snapshot.data)
        highwater = max((item.persistent_id for kind, item, _, managed in schema.walk(value)
                         if kind == 'record' and managed), default=0)
        owned = schema.snapshot(value, snapshot.document, highwater, snapshot.epoch, snapshot.revision)
        self.schema, self.client_id = schema, client_id
        self._snapshot, self._pending, self._owner = owned, None, _mg_threading.current_thread()
        self._busy, self._poison = False, None

    def _check(self):
        """Confine local draft and pending-request mutation to the creating thread."""
        if _mg_threading.current_thread() is not self._owner:
            raise ManagedError('Managed client mutation is thread-confined')
        if self._busy:
            self._poison = ManagedError('Nested managed client mutation is forbidden')
            raise self._poison

    def read(self):
        """Return the confirmed immutable state; pending proposals never appear accepted early."""
        return self._snapshot

    def propose(self, request_id, callback):
        """Run local editing once and retain exact bytes for retry; no automatic rebase or resend occurs."""
        self._check()
        if self._pending is not None or type(request_id) is not str or not request_id or len(request_id) > 256:
            raise ManagedError('Client already has a pending proposal or request ID is invalid')
        value = self.schema.decode(self._snapshot.data)
        bindings = {id(item): item.persistent_id for kind, item, _, managed in self.schema.walk(value)
                    if kind == 'record' and managed}
        # The callback receives a detached ordinary draft; authoritative IDs are assigned by acceptance.
        self._busy, self._poison = True, None
        try:
            _mg_callback(callback, value)
            if self._poison is not None:
                raise self._poison
        finally:
            self._busy = False
        for kind, item, _, managed in self.schema.walk(value):
            if kind == 'record' and managed and item.persistent_id != bindings.get(id(item), 0):
                raise ManagedError('Client edit changed or imported an authoritative identity')
        proposal = ManagedProposal(self.client_id, request_id, self._snapshot.document,
                                   self.schema.schema_id, self._snapshot.epoch, self.schema.encode(value))
        self._pending = proposal
        return proposal

    def retry(self):
        """Return the original pending request object without invoking user code or encoding again."""
        self._check()
        if self._pending is None:
)python"
R"python(            raise ManagedError('Client has no pending proposal')
        return self._pending

    def acknowledge(self, receipt):
        """Accept the exact request's confirmed outcome; conflicts require explicit subsequent editing."""
        self._check()
        pending = self._pending
        if (pending is None or type(receipt) is not ManagedReceipt or
                (receipt.client_id, receipt.request_id, receipt.base_epoch, receipt.document, receipt.schema_id) !=
                (pending.client_id, pending.request_id, pending.base_epoch, pending.document, pending.schema_id) or
                receipt.status not in ('accepted', 'conflict')):
            raise ManagedError('Receipt does not bind the exact pending request')
        if (not all(_mg_unsigned(value, 64) for value in (receipt.base_epoch, receipt.epoch, receipt.revision)) or
                receipt.epoch < self._snapshot.epoch or
                (receipt.status == 'accepted' and (receipt.epoch != pending.base_epoch + 1 or
                                                 receipt.revision <= self._snapshot.revision)) or
                (receipt.status == 'conflict' and receipt.epoch <= pending.base_epoch)):
            raise ManagedError('Receipt epoch, revision or outcome transition is invalid')
        value = self.schema.decode(receipt.data)
        if value.persistent_id != self._snapshot.root.persistent_id:
            raise ManagedError('Authority receipt cannot replace the confirmed document root identity')
        highwater = max((item.persistent_id for kind, item, _, managed in self.schema.walk(value)
                         if kind == 'record' and managed), default=0)
        snapshot = self.schema.snapshot(value, receipt.document, highwater, receipt.epoch, receipt.revision)
        self._snapshot, self._pending = snapshot, None
        return snapshot
)python"
