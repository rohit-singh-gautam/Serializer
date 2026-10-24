// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Standalone native kernel checkpoint; generated JavaScript/TypeScript SDK integration is pending.
R"jsManaged(
/** A native managed contract failed before acceptance; callback failures retain their own identity. */
export class ManagedError extends Error {
  constructor(message, cause) { super(message, cause === undefined ? undefined : {cause}); this.name = 'ManagedError'; }
}
/** An append or publication is uncertain; close and reopen before attempting another mutation. */
export class ManagedIndeterminate extends ManagedError {
  constructor(message, cause) { super(message, cause); this.name = 'ManagedIndeterminate'; }
}

const _mgMaxU64 = (1n << 64n) - 1n;
const _mgMaxU32 = 0xffffffff;
const _mgMaxFrame = 0x7ffffffe;
const _mgKey = Object.freeze({});
const _mgSchemas = new WeakMap();
const _mgPins = new WeakMap();
const _mgEdits = new WeakMap();
const _mgStores = new WeakMap();
const _mgClients = new WeakMap();
const _mgProposals = new WeakMap();
const _mgReceipts = new WeakMap();
const _mgDocuments = new WeakSet();
const _mgTokens = new WeakSet();
const _mgOptions = new WeakSet();
const _mgEncoder = new TextEncoder();
const _mgDecoder = new TextDecoder('utf-8', {fatal: true, ignoreBOM: true});
const _mgOptionNames = Object.freeze(['maxHistoryEntries', 'maxHistoryBytes', 'maxReceipts',
  'maxReceiptBytes', 'maxFrameBytes', 'maxJournalBytes', 'maxJournalRecords']);

/** Reject coercions, unsafe arithmetic and native extents before allocating or indexing. */
function _mgInteger(value, minimum, maximum, label) {
  if (typeof value !== 'number' || !Number.isSafeInteger(value) || value < minimum || value > maximum) {
    throw new ManagedError(`Invalid managed ${label}`);
  }
  return value;
}
/** All document/class/generation coordinates preserve exact unsigned uint64 bits. */
function _mgU64(value, label, nonzero = false) {
  if (typeof value !== 'bigint' || value < (nonzero ? 1n : 0n) || value > _mgMaxU64) {
    throw new ManagedError(`Invalid managed ${label}`);
  }
  return value;
}
/** UTF-8 metadata never silently replaces malformed UTF-16 identities. */
function _mgText(value, maximum, label) {
  if (typeof value !== 'string' || value.length === 0 || value.length > maximum) {
    throw new ManagedError(`Invalid managed ${label} text`);
  }
  for (let index = 0; index < value.length; ++index) {
    const code = value.charCodeAt(index);
    if (code >= 0xd800 && code <= 0xdbff) {
      const low = value.charCodeAt(++index);
      if (!(low >= 0xdc00 && low <= 0xdfff)) throw new ManagedError('Malformed managed metadata Unicode');
    } else if (code >= 0xdc00 && code <= 0xdfff) { throw new ManagedError('Malformed managed metadata Unicode'); }
  }
  if (_mgEncoder.encode(value).length > maximum) throw new ManagedError(`Managed ${label} byte bound exceeded`);
  return value;
}
/** Copy only native byte arrays; accepted state never retains a caller buffer or SharedArrayBuffer. */
function _mgBytes(value, maximum, label) {
  if (!(value instanceof Uint8Array) || value.length > maximum) throw new ManagedError(`Invalid managed ${label} bytes`);
  return Uint8Array.from(value);
}
/** Compare owned exact bytes without exposing their private arrays. */
function _mgEqual(first, second) {
  if (first.length !== second.length) return false;
  for (let index = 0; index < first.length; ++index) if (first[index] !== second[index]) return false;
  return true;
}
/** Check a runtime-created opaque object rather than accepting mutable structural lookalikes. */
function _mgGet(table, value, label) {
  const state = table.get(value);
  if (state === undefined) throw new ManagedError(`Invalid native managed ${label}`);
  return state;
}
/** Keep compound request and typed-entity keys collision-free even when strings contain delimiters. */
function _mgRequest(client, request) { return JSON.stringify([client, request]); }
/** Exact native coordinates have unique canonical string keys within one document. */
function _mgEntity(tag, id) { return `${tag}:${id}`; }
/** Document comparisons use frozen exact scalar metadata, without invoking host-defined equality hooks. */
function _mgSameDocument(first, second) { return first.high === second.high && first.low === second.low; }

/** Immutable namespace; ordinary raw metadata does not create a managed document implicitly. */
export class ManagedDocument {
  constructor(high, low) {
    this.high = _mgU64(high, 'document high'); this.low = _mgU64(low, 'document low');
    if ((high | low) === 0n) throw new ManagedError('Managed document namespace is zero');
    _mgDocuments.add(this); Object.freeze(this);
  }
}
/** Exact nonowning coordinates; constructing a token does not prove ownership, type or liveness. */
export class ManagedToken {
  constructor(document, classId, objectId) {
    if (!_mgDocuments.has(document)) throw new ManagedError('Invalid managed document');
    this.document = document; this.classId = _mgU64(classId, 'class ID', true);
    this.objectId = typeof objectId === 'number' ? _mgInteger(objectId, 1, _mgMaxU32, 'object ID') :
      _mgU64(objectId, 'object ID', true);
    _mgTokens.add(this); Object.freeze(this);
  }
}

/** Immutable finite admission/work bounds; these are not a process heap or RSS ceiling. */
export class ManagedOptions {
  constructor(input = {}) {
    if (!input || Object.getPrototypeOf(input) !== Object.prototype ||
        Reflect.ownKeys(input).some(name => !_mgOptionNames.includes(name))) {
      throw new ManagedError('Invalid managed admission options');
    }
    const values = {maxHistoryEntries: 256, maxHistoryBytes: 64 * 1024 * 1024, maxReceipts: 4096,
      maxReceiptBytes: 64 * 1024 * 1024, maxFrameBytes: 64 * 1024 * 1024,
      maxJournalBytes: 256 * 1024 * 1024, maxJournalRecords: 65536, ...input};
    for (const name of _mgOptionNames) {
      const maximum = name === 'maxFrameBytes' ? _mgMaxFrame :
        ['maxHistoryEntries', 'maxReceipts', 'maxJournalRecords'].includes(name) ? _mgMaxU32 : Number.MAX_SAFE_INTEGER;
      this[name] = _mgInteger(values[name], 1, maximum, name);
    }
    _mgOptions.add(this); Object.freeze(this);
  }
}

/** Bind pure fresh exact native codecs and generated preorder occurrence metadata, independent of business names. */
export class ManagedSchema {
  constructor(rootType, schemaId, idBytes, codec, limits = {}) {
    if (!limits || Object.getPrototypeOf(limits) !== Object.prototype ||
        Reflect.ownKeys(limits).some(name => !['maxSnapshotBytes', 'maxEntities'].includes(name))) {
      throw new ManagedError('Invalid generated schema limits');
    }
    const {maxSnapshotBytes = 64 * 1024 * 1024, maxEntities = 1000000} = limits;
    if (typeof rootType !== 'function' || !rootType.prototype || !codec ||
        ['encode', 'decode', 'visit'].some(name => typeof codec[name] !== 'function')) {
      throw new ManagedError('Invalid managed generated codec');
    }
    if (idBytes !== 4 && idBytes !== 8) throw new ManagedError('Managed ID width must be 4 or 8 bytes');
    const state = Object.freeze({rootType, schemaId: _mgText(schemaId, 4096, 'schema'), idBytes,
      maxSnapshotBytes: _mgInteger(maxSnapshotBytes, 1, _mgMaxFrame, 'snapshot byte bound'),
      maxEntities: _mgInteger(maxEntities, 1, _mgMaxU32, 'entity work bound'),
      codec: Object.freeze({encode: codec.encode, decode: codec.decode, visit: codec.visit})});
    _mgSchemas.set(this, state); Object.freeze(this);
  }
  /** Read the exact native scalar width without lending mutable descriptor state. */
  get idBytes() { return _mgGet(_mgSchemas, this, 'schema').idBytes; }
  /** Expose the frozen logical binding for an explicit host transport. */
  get schemaId() { return _mgGet(_mgSchemas, this, 'schema').schemaId; }
  /** Import an untrusted complete snapshot through exact codecs, canonical bytes and logical identity validation. */
  importSnapshot(document, epoch, revision, data) {
    const schema = _mgGet(_mgSchemas, this, 'schema');
    if (!_mgDocuments.has(document)) throw new ManagedError('Invalid imported document');
    _mgU64(epoch, 'snapshot epoch'); _mgU64(revision, 'snapshot revision');
    if (revision > epoch) throw new ManagedError('Snapshot revision exceeds epoch');
    const bytes = _mgBytes(data, schema.maxSnapshotBytes, 'snapshot');
    const value = _mgDecode(schema, bytes);
    const inventory = _mgInventory(schema, value);
    let highwater = _mgZero(schema);
    for (const item of inventory.entities) {
      const id = item.getId();
      if (item.managed && _mgValidId(schema, id) && id > highwater) highwater = id;
    }
    _mgIdentify(schema, value, document, highwater, null, null, false, true);
    if (!_mgEqual(bytes, _mgEncode(schema, value))) throw new ManagedError('Imported snapshot is not canonical');
    return new ManagedSnapshot(_mgKey, {schema, document, epoch, revision, data: bytes});
  }
}

/** Keep uint32 numbers distinct from uint64 BigInts; never accept a rounded Number as uint64 metadata. */
function _mgZero(schema) { return schema.idBytes === 4 ? 0 : 0n; }
/** Test the exact configured ID representation, allowing zero only before assignment or for ordinary values. */
function _mgValidId(schema, id) {
  return schema.idBytes === 4 ? typeof id === 'number' && Number.isInteger(id) && id >= 0 && id <= _mgMaxU32 :
    typeof id === 'bigint' && id >= 0n && id <= _mgMaxU64;
}
/** Clone fresh codec output before an adapter can reuse or mutate its storage. */
function _mgEncode(schema, value) {
  if (!value || Object.getPrototypeOf(value) !== schema.rootType.prototype) throw new ManagedError('Managed root type differs');
  try { return _mgBytes(schema.codec.encode(value), schema.maxSnapshotBytes, 'snapshot'); }
  catch (error) { if (error instanceof ManagedError) throw error; throw new ManagedError('Managed native encoding failed', error); }
}
/** Decode from private input bytes; pure generated providers must return a fresh exact ordinary graph. */
function _mgDecode(schema, bytes) {
  try {
    const value = schema.codec.decode(_mgBytes(bytes, schema.maxSnapshotBytes, 'snapshot'));
    if (!value || Object.getPrototypeOf(value) !== schema.rootType.prototype) throw new ManagedError('Managed decoded root differs');
    return value;
  } catch (error) { if (error instanceof ManagedError) throw error; throw new ManagedError('Managed native decoding failed', error); }
}
/** Collect preorder logical occurrences and reference edges with independent finite work bounds. */
function _mgInventory(schema, value) {
  const entities = [], edges = [], objects = new Map(), tags = new Map(), classes = new Map();
  const visitor = Object.freeze({
    /** Every owning record is a distinct object, including ordinary descendants without IDs. */
    entity(object, classId, getId, setId, managed) {
      if (!object || typeof object !== 'object' || typeof getId !== 'function' || typeof setId !== 'function' ||
          typeof managed !== 'boolean' || entities.length >= schema.maxEntities) throw new ManagedError('Invalid or excessive managed occurrences');
      _mgU64(classId, 'class ID');
      if (objects.has(object)) throw new ManagedError('Owning alias or cycle in managed graph');
      if (classId !== 0n) {
        const prototype = Object.getPrototypeOf(object);
        if ((tags.has(classId) && tags.get(classId) !== prototype) || (classes.has(prototype) && classes.get(prototype) !== classId)) {
          throw new ManagedError('Managed class tag collides or changes within generated metadata');
        }
        tags.set(classId, prototype); classes.set(prototype, classId);
      }
      const item = {value: object, classId, getId, setId, managed}; objects.set(object, item); entities.push(item);
    },
    /** Missing optional tokens are omitted; present edges must match an exact live managed target. */
    reference(document, classId, objectId, expectedClassId) {
      if (edges.length >= schema.maxEntities || !_mgDocuments.has(document)) throw new ManagedError('Invalid or excessive managed references');
      _mgU64(classId, 'reference class', true); _mgU64(expectedClassId, 'expected class', true);
      edges.push({document, classId, objectId, expectedClassId});
    }
  });
  try { schema.codec.visit(value, visitor); }
  catch (error) { if (error instanceof ManagedError) throw error; throw new ManagedError('Managed generated traversal failed', error); }
  const root = objects.get(value);
  if (!root || !root.managed || root.classId === 0n) throw new ManagedError('Generated metadata omits managed root');
  return {entities, edges, objects};
}
/** Bind existing actual draft objects before callbacks can import or exchange identities. */
function _mgBindings(schema, value) {
  return new Map(_mgInventory(schema, value).entities.filter(item => item.managed).map(item => [item.value, item.getId()]));
}
/** Temporary entity lookups never escape their detached decoded graph. */
function _mgEntities(schema, value) {
  return new Map(_mgInventory(schema, value).entities.filter(item => item.managed).map(item => [_mgEntity(item.classId, item.getId()), item.value]));
}
/** Allocate monotonic identities or validate a complete graph without recycling deleted/reserved IDs. */
function _mgIdentify(schema, value, document, highwater, bindings, existing, allocate, validateReferences) {
  const inventory = _mgInventory(schema, value), ids = new Set(), targets = new Set();
  const zero = _mgZero(schema), maximum = schema.idBytes === 4 ? _mgMaxU32 : _mgMaxU64;
  for (const item of inventory.entities) {
    let id = item.getId();
    if (!_mgValidId(schema, id)) throw new ManagedError('Managed identity exceeds configured width or native scalar type');
    if (!item.managed) { if (id !== zero) throw new ManagedError('Ordinary occurrence has a managed identity'); continue; }
    if (item.classId === 0n) throw new ManagedError('Managed occurrence has no class tag');
    if (allocate) {
      if (bindings && id !== (bindings.get(item.value) ?? zero)) throw new ManagedError('Edit changed or imported managed identity');
      if (!bindings && id !== zero && (!existing || !existing.has(_mgEntity(item.classId, id)))) {
        throw new ManagedError('Proposal imported unknown managed identity');
      }
      if (id === zero) {
        if (highwater === maximum) throw new ManagedError('Managed IDs exhausted');
        highwater += schema.idBytes === 4 ? 1 : 1n; id = highwater; item.setId(id);
      }
    }
    if (id === zero || id > highwater || ids.has(id)) throw new ManagedError('Duplicate, unassigned or future managed identity');
    ids.add(id); targets.add(_mgEntity(item.classId, id));
  }
  if (validateReferences) _mgValidateEdges(schema, document, inventory.edges, targets);
  return highwater;
}
/** Validate scope, selected native width, exact class and current liveness before publication. */
function _mgValidateEdges(schema, document, edges, targets) {
  for (const edge of edges) {
    if (!_mgSameDocument(document, edge.document) || edge.classId !== edge.expectedClassId ||
        !_mgValidId(schema, edge.objectId) || edge.objectId === _mgZero(schema) || !targets.has(_mgEntity(edge.classId, edge.objectId))) {
      throw new ManagedError('Managed reference scope, native type or liveness differs');
    }
  }
}

/** Accepted pins expose immutable coordinates and fresh byte/model copies, never accepted mutable storage. */
export class ManagedSnapshot {
  constructor(key, state) {
    if (key !== _mgKey) throw new ManagedError('Use a schema to import native snapshots');
    _mgPins.set(this, state); this.document = state.document; this.epoch = state.epoch; this.revision = state.revision;
    Object.freeze(this);
  }
  /** Own a detached ordinary value; changing it cannot affect any accepted pin. */
  copyValue() { const pin = _mgGet(_mgPins, this, 'snapshot'); return _mgDecode(pin.schema, pin.data); }
)jsManaged"
R"jsManaged(  /** Return independent bytes, including a new underlying ArrayBuffer. */
  bytes() { return Uint8Array.from(_mgGet(_mgPins, this, 'snapshot').data); }
  /** Resolve within one newly decoded graph with exact generated target type and document scope. */
  resolve(token, targetType) {
    const pin = _mgGet(_mgPins, this, 'snapshot');
    if (!_mgTokens.has(token) || !_mgSameDocument(pin.document, token.document) || !_mgValidId(pin.schema, token.objectId)) {
      throw new ManagedError('Invalid managed reference namespace or native ID type');
    }
    const value = _mgEntities(pin.schema, this.copyValue()).get(_mgEntity(token.classId, token.objectId));
    if (!value || typeof targetType !== 'function' || Object.getPrototypeOf(value) !== targetType.prototype) {
      throw new ManagedError('Managed reference target is absent or differs');
    }
    return value;
  }
}
/** Read root lifetime through generated metadata independently of retained history or object addresses. */
function _mgRoot(pin) {
  const state = _mgGet(_mgPins, pin, 'snapshot'), value = pin.copyValue();
  return _mgInventory(state.schema, value).objects.get(value).getId();
}
/** Normalize codec clones before accepting bytes, including restored map/container ordering. */
function _mgSnapshot(schema, value, document, highwater, epoch, revision) {
  const decoded = _mgDecode(schema, _mgEncode(schema, value));
  _mgIdentify(schema, decoded, document, highwater, null, null, false, true);
  return new ManagedSnapshot(_mgKey, {schema, document, epoch, revision, data: _mgEncode(schema, decoded)});
}
/** Check logical descriptor compatibility before transferring a foreign immutable pin. */
function _mgImport(schema, pin) {
  const source = _mgGet(_mgPins, pin, 'snapshot');
  if (source.schema.rootType !== schema.rootType || source.schema.schemaId !== schema.schemaId || source.schema.idBytes !== schema.idBytes) {
    throw new ManagedError('Managed schema differs from snapshot binding');
  }
  const descriptor = new ManagedSchema(schema.rootType, schema.schemaId, schema.idBytes, schema.codec,
    {maxSnapshotBytes: schema.maxSnapshotBytes, maxEntities: schema.maxEntities});
  return descriptor.importSnapshot(source.document, source.epoch, source.revision, source.data);
}

/** Scoped callback draft; escaped ordinary objects remain detached from accepted byte pins. */
export class ManagedEdit {
  constructor(key, store, root) {
    if (key !== _mgKey) throw new ManagedError('Use a managed transaction to create an edit');
    _mgEdits.set(this, {store, root, active: true, identified: null}); Object.freeze(this);
  }
  /** Borrow the isolated draft only during its synchronous callback scope. */
  get root() { const edit = _mgGet(_mgEdits, this, 'edit'); if (!edit.active) throw new ManagedError('Managed callback ended'); return edit.root; }
  /** Create an exact typed token after IDs have been durably reserved. */
  token(target) {
    const edit = _mgGet(_mgEdits, this, 'edit');
    if (!edit.active || !edit.identified || !edit.identified.has(target)) throw new ManagedError('Token creation requires post-ID initialization');
    const item = _mgInventory(edit.store.schema, edit.root).objects.get(target);
    if (!item || !item.managed) throw new ManagedError('Token target is not currently owned');
    return new ManagedToken(edit.store.document, item.classId, edit.identified.get(target));
  }
}
/** Synchronous means no result, promise, generator or asynchronous function is accepted as an edit. */
function _mgCallback(callback, argument) {
  if (typeof callback !== 'function' || ['[object AsyncFunction]', '[object GeneratorFunction]', '[object AsyncGeneratorFunction]'].
      includes(Object.prototype.toString.call(callback))) throw new ManagedError('Managed callbacks must be synchronous ordinary functions');
  const result = callback(argument);
  if (result !== undefined) {
    // Intrinsic brand checks include native promises from another realm; instanceof does not.
    // A non-promise fails before invoking host thenables. No application callback exception is swallowed.
    try { Promise.prototype.then.call(result, () => undefined, () => undefined); }
    catch (error) { if (!(error instanceof TypeError)) throw error; }
    throw new ManagedError('Managed synchronous callback must return undefined');
  }
}
/** The post-ID phase cannot replace, remove, import or exchange any identified managed occurrence. */
function _mgVerifyIdentified(store, value, expected, highwater) {
  _mgIdentify(store.schema, value, store.document, highwater, null, null, false, true);
  const actual = _mgBindings(store.schema, value);
  if (actual.size !== expected.size) throw new ManagedError('Reference initialization changed ownership');
  for (const [object, id] of expected) if (actual.get(object) !== id) throw new ManagedError('Reference initialization changed identity');
}
/** Build every revision/ledger wrapper before issuing any durable append. */
function _mgState(highwater, epoch, revision, history, cursor, current, receipts) {
  return Object.freeze({highwater, epoch, revision, history: Object.freeze([...history]), cursor, current, receipts: new Map(receipts)});
}
/** Poison caught nested mutations; only one callback or asynchronous lock transition owns the store. */
function _mgCheck(store) {
  if (store.fenced) throw new ManagedIndeterminate('Managed store is closed/fenced; reopen required');
  if (store.busy) { store.poison = new ManagedError('Nested or overlapping managed mutation'); throw store.poison; }
}
/** Advance only prebuilt accepted state after fsync; any uncertain append/publication fences later mutation. */
function _mgPublish(store, operation, next) {
  const sequence = store.journal?.sequence;
  try {
    if (store.journal) store.journal.append(operation, _mgEncodeState(store, next, store.journal.host));
    store.state = next;
  } catch (error) {
    if (store.journal?.indeterminate || store.journal?.sequence !== sequence) store.fenced = true;
    throw error;
  }
}
/** Observable reservations survive failed callbacks and never rewind through undo or history pruning. */
function _mgReserve(store, highwater) {
  const old = store.state;
  if (highwater !== old.highwater) _mgPublish(store, 'reserve', _mgState(highwater, old.epoch, old.revision,
    old.history, old.cursor, old.current, old.receipts));
}
/** Prepare canonical bytes and bounded linear history without mutating the accepted generation. */
function _mgPrepare(store, value) {
  const old = store.state;
  if (old.epoch === _mgMaxU64 || old.revision === _mgMaxU64) throw new ManagedError('Managed generation space exhausted');
  const pin = _mgSnapshot(store.schema, value, store.document, old.highwater, old.epoch + 1n, old.revision + 1n);
  const history = old.history.slice(0, old.cursor + 1); history.push(pin);
  let bytes = 0;
  for (const entry of history) {
    const size = _mgPins.get(entry).data.length;
    if (size > Number.MAX_SAFE_INTEGER - bytes) throw new ManagedError('Managed history arithmetic exceeds native bounds');
    bytes += size;
  }
  if (_mgPins.get(pin).data.length > store.options.maxHistoryBytes) throw new ManagedError('Managed history snapshot exceeds bound');
  while (history.length > store.options.maxHistoryEntries || bytes > store.options.maxHistoryBytes) {
    bytes -= _mgPins.get(history.shift()).data.length;
  }
  return _mgState(old.highwater, pin.epoch, pin.revision, history, history.length - 1, pin, old.receipts);
}

/** Native isolate-owned engine; reads are immutable byte pins and drafts are synchronous private codec clones. */
export class ManagedStore {
  constructor(schema, initial, document, options = new ManagedOptions(), initialize) {
    const descriptor = _mgGet(_mgSchemas, schema, 'schema');
    if (!_mgDocuments.has(document) || !_mgOptions.has(options)) throw new ManagedError('Invalid managed store binding');
    const store = {schema: descriptor, document, options, state: null, busy: true, poison: null, fenced: false, journal: null};
    _mgStores.set(this, store); Object.freeze(this);
    const value = _mgDecode(descriptor, _mgEncode(descriptor, initial));
    const highwater = _mgIdentify(descriptor, value, document, _mgZero(descriptor), new Map(), null, true, false);
    const edit = new ManagedEdit(_mgKey, store, value), scope = _mgEdits.get(edit);
    try {
      scope.identified = _mgBindings(descriptor, value);
      if (initialize !== undefined) _mgCallback(initialize, edit);
      if (store.poison) throw store.poison;
      _mgVerifyIdentified(store, value, scope.identified, highwater);
      const pin = _mgSnapshot(descriptor, value, document, highwater, 0n, 0n);
      if (_mgPins.get(pin).data.length > options.maxHistoryBytes) throw new ManagedError('Initial managed history exceeds bound');
      store.state = _mgState(highwater, 0n, 0n, [pin], 0, pin, new Map());
    } finally { scope.active = false; store.busy = false; }
  }
  /** Pin one accepted immutable generation without touching the journal or lending payload aliases. */
  read() { return _mgGet(_mgStores, this, 'store').state.current; }
  /** Read the non-undoable allocation watermark in its exact configured native scalar type. */
  allocatedId() { return _mgGet(_mgStores, this, 'store').state.highwater; }
  /** Execute exactly one isolated synchronous draft action, with an optional post-ID reference phase. */
  transact(callback, initialize) {
    const store = _mgGet(_mgStores, this, 'store'); _mgCheck(store); store.busy = true; store.poison = null;
    let scope;
    try {
      const value = store.state.current.copyValue(), bindings = _mgBindings(store.schema, value);
      const edit = new ManagedEdit(_mgKey, store, value); scope = _mgEdits.get(edit);
      _mgCallback(callback, edit); if (store.poison) throw store.poison;
      const highwater = _mgIdentify(store.schema, value, store.document, store.state.highwater, bindings, null, true, false);
      _mgReserve(store, highwater); scope.identified = _mgBindings(store.schema, value);
      if (initialize !== undefined) _mgCallback(initialize, edit);
      if (store.poison) throw store.poison;
      _mgVerifyIdentified(store, value, scope.identified, highwater);
      const next = _mgPrepare(store, value);
      if (_mgEqual(_mgPins.get(next.current).data, _mgPins.get(store.state.current).data)) return store.state.current;
      _mgPublish(store, 'accept', next); return next.current;
    } finally { if (scope) scope.active = false; store.busy = false; }
  }
  /** Restore one retained revision through a new acceptance epoch without rerunning callbacks. */
  undo() { return _mgNavigate(_mgGet(_mgStores, this, 'store'), -1); }
  /** Restore one retained redo destination while preserving the global action/identity allocators. */
  redo() { return _mgNavigate(_mgGet(_mgStores, this, 'store'), 1); }
  /** Acquire native Windows path/object pipe leases asynchronously before writing an initial journal generation. */
  createJournal(path) {
    const store = _mgGet(_mgStores, this, 'store'); _mgCheck(store);
    if (store.journal || store.state.epoch !== 0n || store.state.receipts.size) throw new ManagedError('Create the journal before accepting edits/requests');
    store.busy = true; store.poison = null;
    return _mgCreateJournal(store, path);
  }
  /** Reopen only after complete semantic replay, incomplete-tail repair and a recovery fsync. */
  static openJournal(schema, path, options = new ManagedOptions()) {
    return _mgRecoverStore(_mgGet(_mgSchemas, schema, 'schema'), path, options);
  }
  /** Fence immediately; close data synchronously and await actual native pipe-release completion. */
  close() {
    const store = _mgGet(_mgStores, this, 'store');
    if (store.busy) { store.poison = new ManagedError('Cannot close an active managed callback or lock transition'); throw store.poison; }
    store.fenced = true; const journal = store.journal; store.journal = null;
    return journal ? journal.close() : Promise.resolve();
  }
}
/** Adjacent retained navigation uses a new immutable pin, leaving older read pins valid independently. */
function _mgNavigate(store, offset) {
  _mgCheck(store); const old = store.state, cursor = old.cursor + offset;
  if (cursor < 0 || cursor >= old.history.length) throw new ManagedError('No retained history destination');
  if (old.epoch === _mgMaxU64) throw new ManagedError('Managed epoch space exhausted');
  const pin = _mgPins.get(old.history[cursor]);
  const current = new ManagedSnapshot(_mgKey, {...pin, epoch: old.epoch + 1n});
  _mgPublish(store, 'navigate', _mgState(old.highwater, current.epoch, old.revision, old.history, cursor, current, old.receipts));
  return current;
}

/** Exact immutable request coordinates/bytes can be transported explicitly without lending pending mutable storage. */
export class ManagedProposal {
  constructor(clientId, requestId, document, schemaId, baseEpoch, data) {
    if (!_mgDocuments.has(document)) throw new ManagedError('Invalid proposal document');
    this.clientId = _mgText(clientId, 256, 'client'); this.requestId = _mgText(requestId, 256, 'request');
    this.document = document; this.schemaId = _mgText(schemaId, 4096, 'schema'); this.baseEpoch = _mgU64(baseEpoch, 'base epoch');
    _mgProposals.set(this, {data: _mgBytes(data, _mgMaxFrame, 'proposal')}); Object.freeze(this);
  }
  /** Return independent exact original request bytes for a host-selected transport. */
  bytes() { return Uint8Array.from(_mgGet(_mgProposals, this, 'proposal').data); }
}
/** Compare complete immutable request identities without rerunning callbacks or normalizing their bytes. */
function _mgSameProposal(first, second) {
  return first.clientId === second.clientId && first.requestId === second.requestId &&
    _mgSameDocument(first.document, second.document) && first.schemaId === second.schemaId && first.baseEpoch === second.baseEpoch &&
    _mgEqual(_mgGet(_mgProposals, first, 'proposal').data, _mgGet(_mgProposals, second, 'proposal').data);
}
/** Validated immutable accepted/conflict outcome; host authorization remains outside this metadata envelope. */
export class ManagedReceipt {
  constructor(proposal, status, snapshot) {
    _mgGet(_mgProposals, proposal, 'proposal'); const pin = _mgGet(_mgPins, snapshot, 'snapshot');
    if (!['accepted', 'conflict'].includes(status) || proposal.schemaId !== pin.schema.schemaId ||
        !_mgSameDocument(proposal.document, pin.document) ||
        (status === 'accepted' && (proposal.baseEpoch === _mgMaxU64 || pin.epoch !== proposal.baseEpoch + 1n)) ||
        (status === 'conflict' && pin.epoch <= proposal.baseEpoch)) throw new ManagedError('Invalid managed receipt relationship');
    this.proposal = proposal; this.status = status; this.snapshot = snapshot;
    _mgReceipts.set(this, true); Object.freeze(this);
  }
}
/** Serialize exact authority decisions on one isolate-owned store, retaining original durable retry outcomes. */
export class ManagedAuthority {
  constructor(store) { _mgGet(_mgStores, store, 'store'); this.store = store; Object.freeze(this); }
  /** Validate binding/root lifetime before reservations; accept a canonical request or retain a stale-base conflict. */
  accept(proposal) {
    const store = _mgGet(_mgStores, this.store, 'store'); _mgCheck(store);
    const request = _mgGet(_mgProposals, proposal, 'proposal'), old = store.state;
    if (!_mgSameDocument(store.document, proposal.document) || proposal.schemaId !== store.schema.schemaId ||
        request.data.length > store.schema.maxSnapshotBytes) throw new ManagedError('Authority request binding differs');
    const key = _mgRequest(proposal.clientId, proposal.requestId), previous = old.receipts.get(key);
)jsManaged"
R"jsManaged(    if (previous) { if (!_mgSameProposal(previous.proposal, proposal)) throw new ManagedError('Retry ID reused with different bytes/base'); return previous; }
    if (old.receipts.size >= store.options.maxReceipts) throw new ManagedError('Managed retry ledger is full');
    if (proposal.baseEpoch > old.epoch) throw new ManagedError('Future authority epoch');
    store.busy = true; store.poison = null;
    try {
      let next = old, operation = 'receipt', status = 'conflict';
      if (proposal.baseEpoch === old.epoch) {
        const value = _mgDecode(store.schema, request.data), root = _mgInventory(store.schema, value).objects.get(value);
        if (root.getId() !== _mgRoot(old.current)) throw new ManagedError('Authority proposal replaces document root');
        if (!_mgEqual(request.data, _mgEncode(store.schema, value))) throw new ManagedError('Authority proposal is not canonical');
        const highwater = _mgIdentify(store.schema, value, store.document, old.highwater, null,
          _mgEntities(store.schema, old.current.copyValue()), true, true);
        _mgReserve(store, highwater); next = _mgPrepare(store, value); operation = 'accept'; status = 'accepted';
      }
      const receipt = new ManagedReceipt(proposal, status, next.current), receipts = new Map(next.receipts); receipts.set(key, receipt);
      let bytes = 0;
      for (const item of receipts.values()) {
        const size = _mgProposals.get(item.proposal).data.length + _mgPins.get(item.snapshot).data.length;
        if (size > store.options.maxReceiptBytes - bytes) throw new ManagedError('Managed retry byte bound exceeded'); bytes += size;
      }
      if (store.poison) throw store.poison;
      _mgPublish(store, operation, _mgState(next.highwater, next.epoch, next.revision, next.history, next.cursor, next.current, receipts));
      return receipt;
    } finally { store.busy = false; }
  }
}

/** One immutable pending request; local drafts never masquerade as accepted authority state. */
export class ManagedClient {
  constructor(schema, baseline, clientId) {
    const descriptor = _mgGet(_mgSchemas, schema, 'schema');
    _mgClients.set(this, {schema: descriptor, clientId: _mgText(clientId, 256, 'client'),
      confirmed: _mgImport(descriptor, baseline), pending: null, busy: false, poison: null}); Object.freeze(this);
  }
  /** Read the last validated immutable authority pin. */
  read() { return _mgGet(_mgClients, this, 'client').confirmed; }
  /** Run one synchronous callback, keeping fresh IDs zero until authority allocation. */
  propose(requestId, callback) {
    const client = _mgGet(_mgClients, this, 'client'); _mgClientCheck(client);
    if (client.pending) throw new ManagedError('Managed client already has a pending request');
    _mgText(requestId, 256, 'request'); const value = client.confirmed.copyValue(), bindings = _mgBindings(client.schema, value);
    client.busy = true; client.poison = null;
    try { _mgCallback(callback, value); if (client.poison) throw client.poison; }
    finally { client.busy = false; }
    const inventory = _mgInventory(client.schema, value), targets = new Set(), zero = _mgZero(client.schema);
    for (const item of inventory.entities) {
      const id = item.getId();
      if (!_mgValidId(client.schema, id) || (!item.managed && id !== zero) ||
          (item.managed && id !== (bindings.get(item.value) ?? zero))) throw new ManagedError('Client changed or imported authoritative identity');
      if (item.managed && id !== zero) targets.add(_mgEntity(item.classId, id));
    }
    _mgValidateEdges(client.schema, client.confirmed.document, inventory.edges, targets);
    const canonical = _mgEncode(client.schema, _mgDecode(client.schema, _mgEncode(client.schema, value)));
    client.pending = new ManagedProposal(client.clientId, requestId, client.confirmed.document,
      client.schema.schemaId, client.confirmed.epoch, canonical); return client.pending;
  }
  /** Return the identical exact pending request without rerunning callbacks or codecs. */
  retry() { const client = _mgGet(_mgClients, this, 'client'); _mgClientCheck(client); if (!client.pending) throw new ManagedError('No pending managed request'); return client.pending; }
  /** Validate exact request/status/counters/root lifetime before consuming the pending proposal. */
  acknowledge(receipt) {
    const client = _mgGet(_mgClients, this, 'client'); _mgClientCheck(client); _mgGet(_mgReceipts, receipt, 'receipt');
    const pin = _mgGet(_mgPins, receipt.snapshot, 'snapshot');
    if (!client.pending || !_mgSameProposal(client.pending, receipt.proposal) ||
        !_mgSameDocument(client.confirmed.document, pin.document) || pin.epoch < client.confirmed.epoch ||
        (receipt.status === 'accepted' && (pin.epoch !== client.pending.baseEpoch + 1n || pin.revision <= client.confirmed.revision)) ||
        (receipt.status === 'conflict' && pin.epoch <= client.pending.baseEpoch)) throw new ManagedError('Receipt is not a coherent pending-request outcome');
    const imported = _mgImport(client.schema, receipt.snapshot);
    if (_mgRoot(imported) !== _mgRoot(client.confirmed)) throw new ManagedError('Receipt replaces document root');
    client.confirmed = imported; client.pending = null;
  }
}
/** Caught nested local-client attempts poison the current callback just like store transactions. */
function _mgClientCheck(client) { if (client.busy) { client.poison = new ManagedError('Nested managed client mutation'); throw client.poison; } }

let _mgNodeLoading;
/** Load Node services only for an explicitly requested native journal; memory/browser modules import none eagerly. */
async function _mgNode() {
  if (!_mgNodeLoading) _mgNodeLoading = (async () => {
    let fs, path, crypto, net, buffer, process;
    try { [fs, path, crypto, net, buffer, process] = await Promise.all([
      import('node:fs'), import('node:path'), import('node:crypto'), import('node:net'), import('node:buffer'), import('node:process')]); }
    catch (error) { throw new ManagedError('Native Node journal services are unavailable', error); }
    if (process.platform !== 'win32') throw new ManagedError('Native named-pipe journal locking currently requires Windows');
    return Object.freeze({fs, path, crypto, net, Buffer: buffer.Buffer});
  })();
  return _mgNodeLoading;
}
/** Acquire a process-lifetime Windows named-pipe lease; actual exclusion/crash behavior requires SDK qualification. */
async function _mgLease(host, identity) {
  const digest = host.crypto.createHash('sha256').update('serializer.native.js.lock.v1:' + identity).digest('hex');
  const name = '\\\\?\\pipe\\SerializerManaged-' + digest;
  const server = host.net.createServer(socket => { socket.destroy(); });
  const lease = {server, lost: null, closed: false};
  server.on('error', error => { lease.lost = error; });
  try {
    await new Promise((resolve, reject) => {
      /** Stop the initial wait on either OS bind failure or a successful native listen. */
      const failed = error => { server.removeListener('listening', ready); reject(error); };
      /** Remove the temporary failure waiter while retaining the permanent lost-lease observer. */
      const ready = () => { server.removeListener('error', failed); resolve(); };
      server.once('error', failed); server.once('listening', ready); server.listen({path: name, exclusive: true});
    });
    return lease;
  } catch (error) {
    // Failed bind handles are closed by Node; no data file has been written under an unacquired lease.
    if (server.listening) await _mgReleaseLease(lease);
    throw new ManagedError('Native managed journal writer lease is unavailable', error);
  }
}
/** Wait for the native OS handle to close before permitting a new cooperating writer. */
function _mgReleaseLease(lease) {
  if (lease.closed) return Promise.resolve();
  lease.closed = true;
  return new Promise((resolve, reject) => {
    lease.server.close(error => { if (error) reject(new ManagedError('Cannot release native writer lease', error)); else resolve(); });
  });
}
/** Reject malformed native metadata before converting strings to unsigned BigInts. */
function _mgNativeU64(value, label) {
  if (typeof value !== 'string' || value.length > 20 || !/^(0|[1-9][0-9]*)$/.test(value)) throw new ManagedError(`Invalid native ${label}`);
  return _mgU64(BigInt(value), label);
}
/** Convert only exact bounded saved watermarks to the configured Number/BigInt representation. */
function _mgNativeId(schema, value) {
  const id = _mgNativeU64(value, 'identity watermark');
  if (schema.idBytes === 4 && id > BigInt(_mgMaxU32)) throw new ManagedError('Native identity watermark exceeds selected width');
  return schema.idBytes === 4 ? Number(id) : id;
}
/** Require each closed positional native record to have its exact declared cardinality. */
function _mgArray(value, count, label) {
  if (!Array.isArray(value) || value.length !== count) throw new ManagedError(`Invalid native ${label} record`);
  return value;
}
/** Base64 fields are bounded and canonical; whitespace, alternate padding and truncated bytes never alias exact requests. */
function _mgNativeBytes(host, value, maximum) {
  if (typeof value !== 'string' || value.length > Math.ceil(maximum / 3) * 4 || value.length % 4 !== 0) {
    throw new ManagedError('Native managed byte field exceeds bound');
  }
  const data = host.Buffer.from(value, 'base64');
  if (data.length > maximum || data.toString('base64') !== value) throw new ManagedError('Native managed bytes are not canonical');
  return Uint8Array.from(data);
}
/** Serialize one independently complete bounded state; immutable pin bytes are never handed to external adapters. */
function _mgEncodeState(store, state, host) {
  const encodeBytes = value => host.Buffer.from(value).toString('base64');
  const history = state.history.map(pin => { const entry = _mgPins.get(pin); return [String(pin.epoch), String(pin.revision), encodeBytes(entry.data)]; });
  const receipts = [...state.receipts.values()].map(receipt => [receipt.proposal.clientId, receipt.proposal.requestId,
    String(receipt.proposal.baseEpoch), encodeBytes(_mgProposals.get(receipt.proposal).data), receipt.status,
    String(receipt.snapshot.epoch), String(receipt.snapshot.revision), encodeBytes(_mgPins.get(receipt.snapshot).data)]);
  const metadata = [store.schema.schemaId, store.schema.idBytes, [String(store.document.high), String(store.document.low)],
    _mgOptionNames.map(name => store.options[name]), String(state.highwater), String(state.epoch), String(state.revision),
    state.cursor, history, receipts];
  const result = _mgEncoder.encode(JSON.stringify(metadata));
  if (result.length > store.options.maxFrameBytes) throw new ManagedError('Native managed state exceeds frame bound');
  return result;
}
/** Decode and validate every retained pin before any incomplete tail may be repaired. */
function _mgDecodeState(schema, options, bytes, host) {
  let metadata;
  try { metadata = JSON.parse(_mgDecoder.decode(bytes)); }
  catch (error) { throw new ManagedError('Invalid complete native managed metadata', error); }
  if (!_mgEqual(bytes, _mgEncoder.encode(JSON.stringify(metadata)))) throw new ManagedError('Native metadata is not canonical JSON');
  _mgArray(metadata, 10, 'generation');
  if (metadata[0] !== schema.schemaId || metadata[1] !== schema.idBytes) throw new ManagedError('Native managed schema binding differs');
  const namespace = _mgArray(metadata[2], 2, 'document');
  const document = new ManagedDocument(_mgNativeU64(namespace[0], 'document high'), _mgNativeU64(namespace[1], 'document low'));
  const savedOptions = _mgArray(metadata[3], _mgOptionNames.length, 'limits');
  if (savedOptions.some((value, index) => value !== options[_mgOptionNames[index]])) throw new ManagedError('Native recovery limits differ from saved contract');
  const highwater = _mgNativeId(schema, metadata[4]), epoch = _mgNativeU64(metadata[5], 'epoch'), revision = _mgNativeU64(metadata[6], 'revision');
  if (revision > epoch) throw new ManagedError('Native revision exceeds acceptance epoch');
  const entries = metadata[8], savedReceipts = metadata[9];
  if (!Array.isArray(entries) || entries.length < 1 || entries.length > options.maxHistoryEntries ||
      !Array.isArray(savedReceipts) || savedReceipts.length > options.maxReceipts) throw new ManagedError('Native history/retry count exceeds bound');
  const cursor = _mgInteger(metadata[7], 0, entries.length - 1, 'history cursor');
  const history = [], types = new Map();
  let historyBytes = 0, rootId, previousEpoch, previousRevision;
  for (const saved of entries) {
    _mgArray(saved, 3, 'history pin');
    const pinEpoch = _mgNativeU64(saved[0], 'pin epoch'), pinRevision = _mgNativeU64(saved[1], 'pin revision');
    const data = _mgNativeBytes(host, saved[2], schema.maxSnapshotBytes);
    if (data.length > options.maxHistoryBytes - historyBytes || pinEpoch > epoch || pinRevision > revision || pinRevision > pinEpoch ||
        (history.length && (pinEpoch <= previousEpoch || pinRevision <= previousRevision))) throw new ManagedError('Native retained history metadata differs');
    const value = _mgDecode(schema, data); _mgIdentify(schema, value, document, highwater, null, null, false, true);
    if (!_mgEqual(data, _mgEncode(schema, value))) throw new ManagedError('Native snapshot is not canonical');
    const inventory = _mgInventory(schema, value), id = inventory.objects.get(value).getId();
    if (rootId === undefined) rootId = id;
    if (id !== rootId) throw new ManagedError('Native history replaces root identity');
    for (const item of inventory.entities) {
      if (!item.managed) continue;
      const key = item.getId();
      if (types.has(key) && types.get(key) !== item.classId) throw new ManagedError('Native history retypes an entity');
      types.set(key, item.classId);
    }
    history.push(new ManagedSnapshot(_mgKey, {schema, document, epoch: pinEpoch, revision: pinRevision, data}));
    historyBytes += data.length; previousEpoch = pinEpoch; previousRevision = pinRevision;
  }
  const selected = _mgPins.get(history[cursor]);
  const current = new ManagedSnapshot(_mgKey, {...selected, epoch});
  const receipts = new Map(); let receiptBytes = 0;
  for (const saved of savedReceipts) {
    _mgArray(saved, 8, 'retry outcome');
    const client = _mgText(saved[0], 256, 'client'), request = _mgText(saved[1], 256, 'request');
    const base = _mgNativeU64(saved[2], 'request base'), proposed = _mgNativeBytes(host, saved[3], schema.maxSnapshotBytes);
    const acceptedEpoch = _mgNativeU64(saved[5], 'receipt epoch'), acceptedRevision = _mgNativeU64(saved[6], 'receipt revision');
    const data = _mgNativeBytes(host, saved[7], schema.maxSnapshotBytes), size = proposed.length + data.length;
    if (size > options.maxReceiptBytes - receiptBytes || acceptedEpoch > epoch || acceptedRevision > revision || acceptedRevision > acceptedEpoch) {
      throw new ManagedError('Native receipt generation/byte bound differs');
    }
    const value = _mgDecode(schema, data); _mgIdentify(schema, value, document, highwater, null, null, false, true);
    if (_mgInventory(schema, value).objects.get(value).getId() !== rootId || !_mgEqual(data, _mgEncode(schema, value))) {
      throw new ManagedError('Native receipt replaces root or canonical bytes');
    }
    const proposal = new ManagedProposal(client, request, document, schema.schemaId, base, proposed);
    const receipt = new ManagedReceipt(proposal, saved[4], new ManagedSnapshot(_mgKey,
      {schema, document, epoch: acceptedEpoch, revision: acceptedRevision, data}));
    const key = _mgRequest(client, request);
    if (receipts.has(key)) throw new ManagedError('Duplicate native retry identity');
)jsManaged"
R"jsManaged(    receipts.set(key, receipt); receiptBytes += size;
  }
  return {document, state: _mgState(highwater, epoch, revision, history, cursor, current, receipts)};
}
/** Compare immutable native pins without allocating another payload buffer. */
function _mgSamePin(first, second) {
  const a = _mgPins.get(first), b = _mgPins.get(second);
  return _mgSameDocument(a.document, b.document) && a.epoch === b.epoch && a.revision === b.revision &&
    a.schema.schemaId === b.schema.schemaId && _mgEqual(a.data, b.data);
}
/** Preserve every independently retained history pin exactly across native operations. */
function _mgSameHistory(first, second) { return first.length === second.length && first.every((pin, index) => _mgSamePin(pin, second[index])); }
/** Exact retries retain the original request, original status and original accepted/conflict pin. */
function _mgSameReceipt(first, second) {
  return _mgSameProposal(first.proposal, second.proposal) && first.status === second.status && _mgSamePin(first.snapshot, second.snapshot);
}
/** Validate identity/history/receipt transition semantics independently from checksums and before repair/fsync/exposure. */
function _mgTransition(store, before, after, operation, floor) {
  if (operation === 'baseline') throw new ManagedError('Unexpected second native baseline');
  if (after.receipts.size < before.receipts.size || after.receipts.size > before.receipts.size + 1) throw new ManagedError('Native retry ledger size changed unexpectedly');
  let added;
  for (const [key, receipt] of before.receipts) {
    if (!after.receipts.has(key) || !_mgSameReceipt(receipt, after.receipts.get(key))) throw new ManagedError('Native retry outcome changed');
  }
  for (const [key, receipt] of after.receipts) if (!before.receipts.has(key)) added = receipt;
  if (operation === 'reserve') {
    if (after.highwater <= before.highwater || after.epoch !== before.epoch || after.revision !== before.revision ||
        after.cursor !== before.cursor || added || !_mgSameHistory(before.history, after.history) || !_mgSamePin(before.current, after.current)) {
      throw new ManagedError('Invalid native identity reservation');
    }
    return;
  }
  if (after.highwater !== before.highwater) throw new ManagedError('Native operation changes unreserved identity watermark');
  if (operation === 'receipt') {
    if (!added || added.status !== 'conflict' || added.proposal.baseEpoch >= before.epoch || !_mgSamePin(added.snapshot, before.current) ||
        after.epoch !== before.epoch || after.revision !== before.revision || after.cursor !== before.cursor ||
        !_mgSameHistory(before.history, after.history) || !_mgSamePin(before.current, after.current)) throw new ManagedError('Invalid native conflict receipt');
    return;
  }
  if (before.epoch === _mgMaxU64 || after.epoch !== before.epoch + 1n) throw new ManagedError('Native acceptance epoch differs');
  if (operation === 'navigate') {
    if (after.revision !== before.revision || Math.abs(after.cursor - before.cursor) !== 1 || added ||
        !_mgSameHistory(before.history, after.history)) throw new ManagedError('Invalid native history navigation');
    return;
  }
  if (operation !== 'accept' || before.revision === _mgMaxU64 || after.revision !== before.revision + 1n ||
      after.current.revision !== after.revision || after.cursor !== after.history.length - 1) throw new ManagedError('Invalid native acceptance revision');
  const expected = before.history.slice(0, before.cursor + 1); expected.push(after.current);
  let bytes = 0;
  for (const pin of expected) {
    const size = _mgPins.get(pin).data.length;
    if (size > Number.MAX_SAFE_INTEGER - bytes) throw new ManagedError('Native history arithmetic exceeds bounds');
    bytes += size;
  }
  while (expected.length > store.options.maxHistoryEntries || bytes > store.options.maxHistoryBytes) bytes -= _mgPins.get(expected.shift()).data.length;
  if (!_mgSameHistory(expected, after.history)) throw new ManagedError('Native acceptance changes retained history');
  if (_mgRoot(before.current) !== _mgRoot(after.current)) throw new ManagedError('Native acceptance replaces root identity');
  const old = _mgEntities(store.schema, before.current.copyValue());
  const value = after.current.copyValue();
  for (const item of _mgInventory(store.schema, value).entities) {
    if (item.managed && !old.has(_mgEntity(item.classId, item.getId())) && (floor === null || item.getId() <= floor)) {
      throw new ManagedError('Native acceptance reuses/retypes an unreserved identity');
    }
  }
  if (added) {
    if (added.status !== 'accepted' || added.proposal.baseEpoch !== before.epoch || !_mgSamePin(added.snapshot, after.current)) {
      throw new ManagedError('Native accepted receipt binding differs');
    }
    const request = _mgProposals.get(added.proposal).data, requested = _mgDecode(store.schema, request);
    if (!_mgEqual(request, _mgEncode(store.schema, requested))) throw new ManagedError('Native accepted request is not canonical');
    const highwater = _mgIdentify(store.schema, requested, store.document, floor === null ? before.highwater : floor, null, old, true, true);
    if (highwater !== after.highwater || !_mgEqual(_mgEncode(store.schema, requested), _mgPins.get(after.current).data)) {
      throw new ManagedError('Native receipt does not reconstruct original exact request');
    }
  }
}

const _mgMagic = Uint8Array.of(83, 82, 76, 74, 83, 77, 71, 1); // SRLJSMG + native format revision 1.
const _mgHeaderBytes = 8;
const _mgDigestBytes = 32;
const _mgOperationTags = Object.freeze({baseline: 1, reserve: 2, accept: 3, navigate: 4, receipt: 5});

/** Windows native full-state journal with canonical-name and native-file-object process leases. */
class _ManagedJournal {
  constructor(host, options, fd, leases, create) {
    this.host = host; this.options = options; this.fd = fd; this.leases = leases;
    this.sequence = 0; this.offset = create ? 0 : _mgMagic.length; this.incomplete = false;
    this.indeterminate = false; this.create = create; this.observer = null;
  }
  /** Open handles and acquire both native leases before issuing a single data-file write. */
  static async open(path, options, create) {
    const host = await _mgNode();
    if (typeof path !== 'string' || !path || path.includes('\0')) throw new ManagedError('Native journal requires a file path');
    const leases = []; let fd = null;
    try {
      const absolute = host.path.resolve(path);
      if (!/^[a-zA-Z]:\\/.test(absolute)) throw new ManagedError('Native journal requires a local Windows drive path');
      const parent = host.fs.realpathSync.native(host.path.dirname(absolute));
      const selected = create ? host.path.join(parent, host.path.basename(absolute)) : host.fs.realpathSync.native(absolute);
      // Pipe leases are machine-local. UNC/device paths cannot claim cross-host writer exclusion.
      if (!/^[a-zA-Z]:\\/.test(selected)) throw new ManagedError('Native journal requires a local Windows drive path');
      leases.push(await _mgLease(host, 'path:' + selected.toLowerCase()));
      fd = host.fs.openSync(selected, create ? 'wx+' : 'r+');
      const stat = host.fs.fstatSync(fd, {bigint: true});
      if (!stat.isFile() || stat.ino <= 0n || stat.dev <= 0n) throw new ManagedError('Native journal file identity is unavailable');
      leases.push(await _mgLease(host, `object:${stat.dev}:${stat.ino}`));
      return new _ManagedJournal(host, options, fd, leases, create);
    } catch (error) {
      const temporary = new _ManagedJournal(host, options, fd, leases, create);
      const failure = error instanceof ManagedError ? error : new ManagedError('Cannot open native managed journal', error);
      await _mgCloseAfterFailure(temporary, failure); throw failure;
    }
  }
  /** Keep all filesystem offsets exactly representable under the selected finite native log bound. */
  size() {
    const size = this.host.fs.fstatSync(this.fd, {bigint: true}).size;
    if (size < 0n || size > BigInt(this.options.maxJournalBytes)) throw new ManagedError('Native journal byte bound exceeded');
    return Number(size);
  }
  /** A lost native lease fences mutation instead of pretending another writer is still excluded. */
  assertLeases() {
    if (this.indeterminate || this.leases.some(lease => lease.lost || lease.closed || !lease.server.listening)) {
      this.indeterminate = true; throw new ManagedIndeterminate('Native writer lease lost; reopen required');
    }
  }
  /** Test-only lexical observer; no application callback is part of durable publication. */
  event(phase) { if (this.observer) this.observer(phase); }
  /** Write every prepared byte through one retained native descriptor, detecting short/no-progress writes. */
  write(bytes, position) {
    let written = 0;
    while (written < bytes.length) {
      const count = this.host.fs.writeSync(this.fd, bytes, written, bytes.length - written, position + written);
      if (!Number.isInteger(count) || count <= 0) throw new ManagedError('Native journal write made no progress'); written += count;
    }
    return position + written;
  }
  /** Read a bounded exact extent; no native descriptor or private buffer escapes to a caller. */
  read(count, position) {
    const bytes = new Uint8Array(count); let read = 0;
    while (read < count) {
      const size = this.host.fs.readSync(this.fd, bytes, read, count - read, position + read);
      if (size === 0) throw new ManagedError('Native journal changed or truncated during locked recovery');
      if (!Number.isInteger(size) || size < 0) throw new ManagedError('Native journal read made no progress'); read += size;
    }
    return bytes;
  }
  /** Prepare all frame bytes/hashes and perform admission before the first possibly observable append. */
  append(operation, payload) {
    this.assertLeases();
    const tag = _mgOperationTags[operation];
    if (!tag || payload.length > this.options.maxFrameBytes || this.sequence >= this.options.maxJournalRecords) {
      throw new ManagedError('Native journal operation/frame/count exceeds bound');
    }
    const body = new Uint8Array(payload.length + 1); body[0] = tag; body.set(payload, 1);
    const header = new Uint8Array(_mgHeaderBytes), words = new DataView(header.buffer);
    words.setUint32(0, body.length, false); words.setUint32(4, this.sequence, false);
    const hashHeader = this.host.crypto.createHash('sha256').update(header).digest();
    const hashBody = this.host.crypto.createHash('sha256').update(body).digest();
    const fileSize = this.size(), prefix = this.create ? _mgMagic.length : 0;
    const frameBytes = prefix + _mgHeaderBytes + 2 * _mgDigestBytes + body.length;
    if (fileSize !== this.offset) { this.indeterminate = true; throw new ManagedIndeterminate('Native journal changed outside its writer'); }
    if (frameBytes > this.options.maxJournalBytes - fileSize) throw new ManagedError('Native journal byte capacity exhausted');
    this.event('BEFORE_WRITE'); this.assertLeases();
    try {
      let position = fileSize;
      if (this.create) position = this.write(_mgMagic, position);
      position = this.write(header, position); this.event('AFTER_LENGTH');
      position = this.write(hashHeader, position); this.event('AFTER_HEADER');
      position = this.write(body, position); this.event('AFTER_PAYLOAD');
      position = this.write(hashBody, position); this.event('AFTER_CHECKSUM');
      this.host.fs.fsyncSync(this.fd);
      this.offset = position; ++this.sequence; this.create = false; this.event('AFTER_FORCE');
    } catch (error) { this.indeterminate = true; throw new ManagedIndeterminate('Native journal acceptance is uncertain; reopen required', error); }
  }
  /** Verify format and total size before allocating or decoding the first complete generation. */
  beginRecovery() {
    this.assertLeases();
    if (this.size() < _mgMagic.length || !_mgEqual(this.read(_mgMagic.length, 0), _mgMagic)) throw new ManagedError('Unknown native JavaScript journal format');
  }
  /** Complete header corruption is not an incomplete tail; fixed header checksums protect lengths/sequences. */
  next() {
    const remaining = this.size() - this.offset;
    if (remaining === 0) return null;
    if (remaining < _mgHeaderBytes + _mgDigestBytes) { this.incomplete = true; return null; }
    const header = this.read(_mgHeaderBytes, this.offset), expected = this.read(_mgDigestBytes, this.offset + _mgHeaderBytes);
    const actual = this.host.crypto.createHash('sha256').update(header).digest();
    if (!_mgEqual(expected, actual)) throw new ManagedError('Native journal header checksum differs');
    const words = new DataView(header.buffer, header.byteOffset, header.byteLength);
    const size = words.getUint32(0, false), sequence = words.getUint32(4, false);
    if (size < 1 || size - 1 > this.options.maxFrameBytes || sequence !== this.sequence || this.sequence >= this.options.maxJournalRecords) {
      throw new ManagedError('Native journal frame metadata exceeds bounds');
    }
    const frameBytes = _mgHeaderBytes + 2 * _mgDigestBytes + size;
    if (frameBytes > remaining) { this.incomplete = true; return null; }
    const body = this.read(size, this.offset + _mgHeaderBytes + _mgDigestBytes);
    const digest = this.read(_mgDigestBytes, this.offset + _mgHeaderBytes + _mgDigestBytes + size);
    if (!_mgEqual(digest, this.host.crypto.createHash('sha256').update(body).digest())) throw new ManagedError('Native complete frame checksum differs');
    const operation = Object.keys(_mgOperationTags).find(name => _mgOperationTags[name] === body[0]);
    if (!operation) throw new ManagedError('Unknown native journal operation');
    this.offset += frameBytes; ++this.sequence;
    return {operation, payload: body.slice(1)};
  }
  /** Repair only after semantic replay, then force even complete unacknowledged generations before exposing outcomes. */
  finishRecovery() {
    this.assertLeases();
    try {
      if (this.incomplete) this.host.fs.ftruncateSync(this.fd, this.offset);
      this.event('RECOVERY_BEFORE_FORCE'); this.host.fs.fsyncSync(this.fd); this.event('RECOVERY_AFTER_FORCE');
    } catch (error) { this.indeterminate = true; throw new ManagedIndeterminate('Native recovery flush is uncertain', error); }
  }
  /** Close the data descriptor before releasing object/name leases, attempting all cleanup and preserving every failure. */
  async close() {
    const errors = [], fd = this.fd; this.fd = null;
    if (fd !== null) { try { this.host.fs.closeSync(fd); } catch (error) { errors.push(error); } }
    for (const lease of this.leases.slice().reverse()) {
      try { await _mgReleaseLease(lease); } catch (error) { errors.push(error); }
    }
    if (errors.length) throw new AggregateError(errors, 'Cannot close native managed journal resources');
  }
}
/** Never hide either the original contract/IO failure or a subsequent independent cleanup failure. */
async function _mgCloseAfterFailure(journal, original) {
  try { await journal.close(); }
  catch (closing) { throw new AggregateError([original, closing], 'Native operation and resource cleanup both failed', {cause: original}); }
}
/** Attach a freshly forced generation only after both asynchronous native leases have been acquired. */
async function _mgCreateJournal(store, path) {
  let journal;
  try {
    journal = await _ManagedJournal.open(path, store.options, true);
    if (store.poison) throw store.poison;
    journal.append('baseline', _mgEncodeState(store, store.state, journal.host)); store.journal = journal;
  } catch (error) {
    if (journal?.indeterminate) store.fenced = true;
    if (journal) await _mgCloseAfterFailure(journal, error);
    throw error;
  } finally { store.busy = false; }
}
)jsManaged"
R"jsManaged(/** Recover privately under both native leases; invalid complete records preserve every byte including any later tail. */
async function _mgRecoverStore(schema, path, options, observer = null) {
  if (!_mgOptions.has(options)) throw new ManagedError('Invalid native recovery limits');
  const journal = await _ManagedJournal.open(path, options, false); journal.observer = observer;
  try {
    journal.beginRecovery(); let store = null, floor = null, frame;
    while ((frame = journal.next()) !== null) {
      const decoded = _mgDecodeState(schema, options, frame.payload, journal.host), next = decoded.state;
      if (store === null) {
        if (frame.operation !== 'baseline' || next.epoch !== 0n || next.revision !== 0n || next.cursor !== 0 ||
            next.history.length !== 1 || next.receipts.size) throw new ManagedError('Native journal lacks exact initial baseline');
        store = {schema, document: decoded.document, options, state: next, busy: false, poison: null, fenced: false, journal: null};
      } else {
        if (!_mgSameDocument(store.document, decoded.document)) throw new ManagedError('Native document namespace changed');
        _mgTransition(store, store.state, next, frame.operation, floor);
        floor = frame.operation === 'reserve' ? store.state.highwater : null; store.state = next;
      }
    }
    if (store === null) throw new ManagedError('Native journal lacks a complete baseline');
    journal.finishRecovery(); store.journal = journal;
    const result = Object.create(ManagedStore.prototype); _mgStores.set(result, store); Object.freeze(result); return result;
  } catch (error) { await _mgCloseAfterFailure(journal, error); throw error; }
}

// Runtime classes expose no writable implementation hooks; ordinary generated payload classes remain mutable copies.
for (const type of [ManagedDocument, ManagedToken, ManagedOptions, ManagedSchema, ManagedSnapshot, ManagedEdit,
  ManagedStore, ManagedProposal, ManagedReceipt, ManagedAuthority, ManagedClient]) {
  Object.freeze(type.prototype); Object.freeze(type);
}
)jsManaged"
