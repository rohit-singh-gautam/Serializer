// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Runtime-only checkpoint: generated Java SDK integration and compilation are pending.
R"javaEngine(
  /** A native managed contract failed before acceptance. */
  public static class ManagedError extends java.lang.RuntimeException {
    private static final long serialVersionUID = 1L;
    public ManagedError(java.lang.String message) { super(message); }
    public ManagedError(java.lang.String message, java.lang.Throwable cause) { super(message, cause); }
  }

  /** Close/reopen after uncertain durable acceptance; the last proven read pin remains available. */
  public static final class ManagedIndeterminate extends ManagedError {
    private static final long serialVersionUID = 1L;
    public ManagedIndeterminate(java.lang.String message, java.lang.Throwable cause) { super(message, cause); }
  }

  /** Document halves preserve the complete unsigned uint64 bit patterns. */
  public record ManagedDocument(long high, long low) {
    public ManagedDocument {
      if ((high | low) == 0) throw new ManagedError("Managed document namespace is zero");
    }
  }

  /** Immutable metadata; resolution separately verifies exact type, scope and current liveness. */
  public record ManagedToken(ManagedDocument document, long classId, long objectId) {
    public ManagedToken {
      java.util.Objects.requireNonNull(document);
      if (classId == 0 || objectId == 0) throw new ManagedError("Managed token identity is zero");
    }
  }

  /** Fresh exact native codecs and generated logical-occurrence traversal; adapters must not cache mutable values. */
  public interface ManagedCodec<T> {
    byte[] encode(T value);
    T decode(byte[] data);
    void visit(T value, ManagedVisitor visitor);
  }

  /** Generated traversal supplies ordinary descendants as ordinary, including below an ordinary ancestor. */
  public interface ManagedVisitor {
    void entity(java.lang.Object value, long classId, java.util.function.LongSupplier identity,
                java.util.function.LongConsumer setIdentity, boolean managed);
    void reference(ManagedDocument document, long classId, long objectId, long expectedClassId);
  }

  private record ManagedEntityKey(long classId, long objectId) {}
  private record ManagedOccurrence(java.lang.Object value, long classId,
      java.util.function.LongSupplier identity, java.util.function.LongConsumer setIdentity, boolean managed) {}
  private record ManagedEdge(ManagedDocument document, long classId, long objectId, long expectedClassId) {}

  /** Schema limits are admission/work boundaries, not a Java process memory ceiling. */
  public static final class ManagedSchema<T> {
    private final java.lang.Class<T> rootType;
    private final java.lang.String schemaId;
    private final int idBytes;
    private final int maxSnapshotBytes;
    private final int maxEntities;
    private final ManagedCodec<T> codec;

    /** Bind exact generated types and frozen schema/class tags independently of ordinary constructors. */
    public ManagedSchema(java.lang.Class<T> rootType, java.lang.String schemaId, int idBytes,
        int maxSnapshotBytes, int maxEntities, ManagedCodec<T> codec) {
      this.rootType = java.util.Objects.requireNonNull(rootType);
      this.schemaId = checkedText(schemaId, 4096, "schema");
      if ((idBytes != 4 && idBytes != 8) || maxSnapshotBytes < 1 || maxEntities < 1) {
        throw new ManagedError("Invalid native managed schema limits");
      }
      this.idBytes = idBytes;
      this.maxSnapshotBytes = maxSnapshotBytes;
      this.maxEntities = maxEntities;
      this.codec = java.util.Objects.requireNonNull(codec);
    }

    /** Return independent bytes even when an adapter reuses its own output buffer. */
    private byte[] encode(T value) {
      if (value == null || value.getClass() != rootType) throw new ManagedError("Managed root type differs");
      try {
        byte[] data = java.util.Objects.requireNonNull(codec.encode(value));
        if (data.length > maxSnapshotBytes) throw new ManagedError("Managed snapshot byte bound exceeded");
        return data.clone();
      } catch (ManagedError error) { throw error; }
      catch (java.lang.RuntimeException error) { throw new ManagedError("Managed native encoding failed", error); }
    }

    /** Pass a private buffer to the ordinary exact decoder; accepted bytes never become adapter-owned. */
    private T decode(byte[] data) {
      if (data.length > maxSnapshotBytes) throw new ManagedError("Managed snapshot byte bound exceeded");
      try {
        T value = codec.decode(data.clone());
        if (value == null || value.getClass() != rootType) throw new ManagedError("Managed decoded root differs");
        return value;
      } catch (ManagedError error) { throw error; }
      catch (java.lang.RuntimeException error) { throw new ManagedError("Managed native decoding failed", error); }
    }

    /** Collect bounded descriptors without retaining mutable application values in accepted state. */
    private ManagedInventory inventory(T value) {
      ManagedInventory result = new ManagedInventory(maxEntities);
      try { codec.visit(value, result); }
      catch (ManagedError error) { throw error; }
      catch (java.lang.RuntimeException error) { throw new ManagedError("Managed generated traversal failed", error); }
      if (!result.objects.containsKey(value)) throw new ManagedError("Generated metadata omits managed root");
      ManagedOccurrence root = result.objects.get(value);
      if (!root.managed() || root.classId() == 0) throw new ManagedError("Root is not a managed occurrence");
      return result;
    }

    /** Assign monotonic identities to fresh owned occurrences and validate every reference/lifetime. */
    private long identify(T value, ManagedDocument document, long highwater,
        java.util.IdentityHashMap<java.lang.Object, java.lang.Long> bindings,
        java.util.Map<ManagedEntityKey, java.lang.Object> existing, boolean allocate,
        boolean validateReferences) {
      ManagedInventory inventory = inventory(value);
      java.util.HashSet<java.lang.Long> identities = new java.util.HashSet<>();
      java.util.HashSet<ManagedEntityKey> keys = new java.util.HashSet<>();
      for (ManagedOccurrence occurrence : inventory.entities) {
        long id = occurrence.identity().getAsLong();
        if (!validId(id)) throw new ManagedError("Managed identity exceeds configured width");
        if (!occurrence.managed()) {
          if (id != 0) throw new ManagedError("Ordinary occurrence has a managed identity");
          continue;
        }
        if (occurrence.classId() == 0) throw new ManagedError("Managed occurrence has no class tag");
        if (allocate) {
          if (bindings != null && id != bindings.getOrDefault(occurrence.value(), 0L)) {
            throw new ManagedError("Edit changed or imported a managed identity");
          }
          if (bindings == null && id != 0 && (existing == null ||
              !existing.containsKey(new ManagedEntityKey(occurrence.classId(), id)))) {
            throw new ManagedError("Proposal imported an unknown managed identity");
          }
          if (id == 0) {
            if (highwater == (idBytes == 4 ? 0xffffffffL : -1L)) throw new ManagedError("Managed IDs exhausted");
            id = ++highwater;
            occurrence.setIdentity().accept(id);
          }
        }
        if (id == 0 || java.lang.Long.compareUnsigned(id, highwater) > 0 || !identities.add(id)) {
          throw new ManagedError("Duplicate, unassigned or future managed identity");
        }
        keys.add(new ManagedEntityKey(occurrence.classId(), id));
      }
      if (validateReferences) {
        for (ManagedEdge edge : inventory.edges) {
          if (!document.equals(edge.document()) || edge.classId() != edge.expectedClassId() ||
              edge.objectId() == 0 || !validId(edge.objectId()) ||
              !keys.contains(new ManagedEntityKey(edge.classId(), edge.objectId()))) {
            throw new ManagedError("Managed reference scope, exact type or liveness differs");
          }
        }
      }
      return highwater;
    }

    /** Native uint64 metadata is held as raw long bits; uint32 remains a nonnegative long. */
    private boolean validId(long value) { return idBytes == 8 || (value >= 0 && value <= 0xffffffffL); }

    /** Equivalent generated descriptors retain the same logical contract across authority reopen. */
    private boolean sameContract(ManagedSchema<?> other) {
      return other != null && rootType == other.rootType && idBytes == other.idBytes && schemaId.equals(other.schemaId);
    }

    /** Produce temporary lookup metadata only for a decoded mutable copy. */
    private java.util.Map<ManagedEntityKey, java.lang.Object> entities(T value) {
      java.util.HashMap<ManagedEntityKey, java.lang.Object> result = new java.util.HashMap<>();
      for (ManagedOccurrence occurrence : inventory(value).entities) {
        if (occurrence.managed()) result.put(new ManagedEntityKey(occurrence.classId(),
            occurrence.identity().getAsLong()), occurrence.value());
      }
      return result;
    }

    /** Record existing draft object identities before user callbacks can alter them. */
    private java.util.IdentityHashMap<java.lang.Object, java.lang.Long> bindings(T value) {
      java.util.IdentityHashMap<java.lang.Object, java.lang.Long> result = new java.util.IdentityHashMap<>();
      for (ManagedOccurrence occurrence : inventory(value).entities) {
        if (occurrence.managed()) result.put(occurrence.value(), occurrence.identity().getAsLong());
      }
      return result;
    }
  }

  private static final class ManagedInventory implements ManagedVisitor {
    private final int maximum;
    private final java.util.ArrayList<ManagedOccurrence> entities = new java.util.ArrayList<>();
    private final java.util.ArrayList<ManagedEdge> edges = new java.util.ArrayList<>();
    private final java.util.IdentityHashMap<java.lang.Object, ManagedOccurrence> objects = new java.util.IdentityHashMap<>();
    private final java.util.HashMap<java.lang.Long, java.lang.Class<?>> classes = new java.util.HashMap<>();
    private final java.util.HashMap<java.lang.Class<?>, java.lang.Long> tags = new java.util.HashMap<>();
    private ManagedInventory(int maximum) { this.maximum = maximum; }
    /** Every owning occurrence must have its own Java object, even before IDs are assigned. */
    @Override public void entity(java.lang.Object value, long classId,
        java.util.function.LongSupplier identity, java.util.function.LongConsumer setIdentity, boolean managed) {
      if (value == null || identity == null || setIdentity == null || entities.size() >= maximum) {
        throw new ManagedError("Invalid or excessive managed descriptor occurrences");
      }
      ManagedOccurrence occurrence = new ManagedOccurrence(value, classId, identity, setIdentity, managed);
      if (classId != 0) {
        java.lang.Class<?> oldClass = classes.putIfAbsent(classId, value.getClass());
        java.lang.Long oldTag = tags.putIfAbsent(value.getClass(), classId);
        if ((oldClass != null && oldClass != value.getClass()) || (oldTag != null && oldTag.longValue() != classId)) {
          throw new ManagedError("Managed class tag collides or changes within the descriptor");
        }
      }
      if (objects.put(value, occurrence) != null) throw new ManagedError("Owning alias or cycle in managed graph");
      entities.add(occurrence);
    }
    /** Missing optional tokens are omitted by generated traversal; present envelopes are checked exactly. */
    @Override public void reference(ManagedDocument document, long classId, long objectId, long expectedClassId) {
      if (edges.size() >= maximum) throw new ManagedError("Managed reference work bound exceeded");
      edges.add(new ManagedEdge(document, classId, objectId, expectedClassId));
    }
  }

  /** An accepted pin owns only immutable private bytes; each mutable read is a fresh decoded copy. */
  public static final class ManagedSnapshot<T> {
    private final ManagedSchema<T> schema;
    private final ManagedDocument document;
    private final long epoch;
    private final long revision;
    private final byte[] data;
    private ManagedSnapshot(ManagedSchema<T> schema, ManagedDocument document, long epoch, long revision, byte[] data) {
      this.schema = schema; this.document = document; this.epoch = epoch; this.revision = revision; this.data = data;
    }
    /** Read the document namespace without accessing native storage. */
    public ManagedDocument document() { return document; }
    /** Read the monotonic acceptance generation as unsigned long bits. */
    public long epoch() { return epoch; }
    /** Read the retained history action number as unsigned long bits. */
    public long revision() { return revision; }
    /** Own a detached ordinary model; changes never affect this or another accepted pin. */
    public T copyValue() { return schema.decode(data); }
    /** Return independent bytes; callers never receive the accepted private array. */
    public byte[] bytes() { return data.clone(); }
    /** Root lifetime is fixed within one document namespace, including across remote receipt adoption. */
    private long rootIdentity() {
      T value = copyValue();
      return schema.inventory(value).objects.get(value).identity().getAsLong();
    }
    /** Resolve to an entity inside a newly decoded detached graph, with exact class/scope validation. */
    public <U> U resolve(ManagedToken token, java.lang.Class<U> targetType) {
      if (!document.equals(token.document())) throw new ManagedError("Managed reference belongs to another document");
      java.lang.Object value = schema.entities(copyValue()).get(new ManagedEntityKey(token.classId(), token.objectId()));
      if (value == null || value.getClass() != targetType) throw new ManagedError("Managed reference target is absent or differs");
      return targetType.cast(value);
    }
  }

  /** One synchronous draft view; token creation is available only after identities have been reserved. */
  public static final class ManagedEdit<T> {
    private final ManagedStore<T> store;
    private final T root;
    private boolean active = true;
    private java.util.IdentityHashMap<java.lang.Object, java.lang.Long> identified;
    private ManagedEdit(ManagedStore<T> store, T root) { this.store = store; this.root = root; }
    /** Borrow a detached draft until the synchronous callback returns. */
    public T root() {
      if (!active) throw new ManagedError("Managed edit callback ended");
      return root;
    }
    /** Build exact typed metadata for one currently owned post-identity occurrence. */
    public ManagedToken token(java.lang.Object target) {
      if (!active || identified == null || !identified.containsKey(target)) {
        throw new ManagedError("Token creation requires the reference-initialization phase");
      }
      ManagedOccurrence occurrence = store.schema.inventory(root).objects.get(target);
      return new ManagedToken(store.document, occurrence.classId(), identified.get(target));
    }
  }

  /** Immutable native limits; full-state journal frames deliberately favor simple independent recovery. */
  public record ManagedOptions(int maxHistoryEntries, long maxHistoryBytes, int maxReceipts,
      long maxReceiptBytes, int maxFrameBytes, long maxJournalBytes, int maxJournalRecords) {
    public ManagedOptions {
      if (maxHistoryEntries < 1 || maxHistoryBytes < 1 || maxReceipts < 1 || maxReceiptBytes < 1 ||
          maxFrameBytes < 1 || maxFrameBytes == java.lang.Integer.MAX_VALUE ||
          maxJournalBytes < 1 || maxJournalRecords < 1) {
        throw new ManagedError("Native managed limits must be positive");
      }
    }
)javaEngine"
R"javaEngine(    /** Select finite standalone admission limits without claiming a hard JVM heap ceiling. */
    public static ManagedOptions defaults() {
      return new ManagedOptions(256, 64L * 1024 * 1024, 4096, 64L * 1024 * 1024,
          64 * 1024 * 1024, 256L * 1024 * 1024, 65536);
    }
  }

  /** Exact immutable request bytes remain pending until an authority receipt is acknowledged. */
  public static final class ManagedProposal {
    private final java.lang.String clientId;
    private final java.lang.String requestId;
    private final ManagedDocument document;
    private final java.lang.String schemaId;
    private final long baseEpoch;
    private final byte[] data;
    public ManagedProposal(java.lang.String clientId, java.lang.String requestId, ManagedDocument document,
        java.lang.String schemaId, long baseEpoch, byte[] data) {
      this.clientId = checkedText(clientId, 256, "client"); this.requestId = checkedText(requestId, 256, "request");
      this.document = java.util.Objects.requireNonNull(document);
      this.schemaId = checkedText(schemaId, 4096, "schema"); this.baseEpoch = baseEpoch;
      this.data = java.util.Objects.requireNonNull(data).clone();
    }
    /** Retries compare every binding and exact byte, rather than a mutable caller request object. */
    private boolean same(ManagedProposal other) {
      return clientId.equals(other.clientId) && requestId.equals(other.requestId) && document.equals(other.document) &&
          schemaId.equals(other.schemaId) && baseEpoch == other.baseEpoch && java.util.Arrays.equals(data, other.data);
    }
  }

  public enum ManagedStatus { ACCEPTED, CONFLICT }

  /** An authority outcome owns its immutable accepted pin and original exact request binding. */
  public static final class ManagedReceipt<T> {
    private final ManagedProposal proposal;
    private final ManagedStatus status;
    private final ManagedSnapshot<T> snapshot;
    private ManagedReceipt(ManagedProposal proposal, ManagedStatus status, ManagedSnapshot<T> snapshot) {
      this.proposal = proposal; this.status = status; this.snapshot = snapshot;
    }
    /** Inspect acceptance versus a stale-base conflict without treating local edits as confirmation. */
    public ManagedStatus status() { return status; }
    /** Pin the original immutable authority outcome even after the authority advances. */
    public ManagedSnapshot<T> snapshot() { return snapshot; }
  }

  private record ManagedRequestKey(java.lang.String clientId, java.lang.String requestId) {}

  private static final class ManagedState<T> {
    private final long highwater;
    private final long epoch;
    private final long revision;
    private final java.util.List<ManagedSnapshot<T>> history;
    private final int cursor;
    private final ManagedSnapshot<T> current;
    private final java.util.Map<ManagedRequestKey, ManagedReceipt<T>> receipts;
    private ManagedState(long highwater, long epoch, long revision, java.util.List<ManagedSnapshot<T>> history,
        int cursor, ManagedSnapshot<T> current, java.util.Map<ManagedRequestKey, ManagedReceipt<T>> receipts) {
      this.highwater = highwater; this.epoch = epoch; this.revision = revision;
      this.history = java.util.List.copyOf(history); this.cursor = cursor; this.current = current;
      this.receipts = java.util.Collections.unmodifiableMap(new java.util.LinkedHashMap<>(receipts));
    }
  }

  /** Owner-thread native engine; read pins cross threads and never lazily touch the journal. */
  public static final class ManagedStore<T> implements java.lang.AutoCloseable {
    private final ManagedSchema<T> schema;
    private final ManagedDocument document;
    private final ManagedOptions options;
    private final java.lang.Thread owner = java.lang.Thread.currentThread();
    private volatile ManagedState<T> state;
    private boolean busy;
    private boolean fenced;
    private ManagedError poison;
    private ManagedJournal journal;

    /** Explicitly transfer by native codec cloning; a raw model constructor never activates an engine. */
    public ManagedStore(ManagedSchema<T> schema, T initial, ManagedDocument document, ManagedOptions options) {
      this(schema, initial, document, options, null);
    }
    /** Initialize new references only after assigning IDs; callbacks remain synchronous and thread-confined. */
    public ManagedStore(ManagedSchema<T> schema, T initial, ManagedDocument document, ManagedOptions options,
        java.util.function.Consumer<ManagedEdit<T>> initialize) {
      this.schema = java.util.Objects.requireNonNull(schema); this.document = java.util.Objects.requireNonNull(document);
      this.options = java.util.Objects.requireNonNull(options);
      T value = schema.decode(schema.encode(initial));
      long highwater = schema.identify(value, document, 0, new java.util.IdentityHashMap<>(), null, true, false);
      ManagedEdit<T> edit = new ManagedEdit<>(this, value);
      busy = true;
      try {
        edit.identified = schema.bindings(value);
        if (initialize != null) initialize.accept(edit);
        verifyIdentified(value, edit.identified, highwater);
        if (poison != null) throw poison;
        ManagedSnapshot<T> snapshot = snapshot(value, highwater, 0, 0);
        if (snapshot.data.length > options.maxHistoryBytes()) throw new ManagedError("Initial history bound exceeded");
        state = new ManagedState<>(highwater, 0, 0, java.util.List.of(snapshot), 0, snapshot, java.util.Map.of());
      } finally { edit.active = false; busy = false; }
    }
    /** Recovery constructs all state privately before attaching the locked durable file. */
    private ManagedStore(ManagedSchema<T> schema, ManagedDocument document, ManagedOptions options, ManagedState<T> state) {
      this.schema = schema; this.document = document; this.options = options; this.state = state;
    }
    /** Return one coherent immutable pin; a fenced engine exposes only its last proven local state. */
    public ManagedSnapshot<T> read() { return state.current; }
    /** Read non-undoable allocation metadata on the writer thread. */
    public long allocatedId() { checkOwner(); return state.highwater; }
    /** Enforce actual Thread object affinity rather than a recyclable numeric thread identifier. */
    private void checkOwner() {
      if (java.lang.Thread.currentThread() != owner) throw new ManagedError("Managed mutation belongs to its owner thread");
    }
    /** Caught nested attempts poison the whole outer action before any candidate acceptance. */
    private void checkMutation() {
      checkOwner();
      if (fenced) throw new ManagedIndeterminate("Managed store is fenced; close/reopen required", null);
      if (busy) { poison = new ManagedError("Nested managed mutation is forbidden"); throw poison; }
    }
    /** Validate fresh canonical bytes and all exact live reference targets before making an immutable pin. */
    private ManagedSnapshot<T> snapshot(T value, long highwater, long epoch, long revision) {
      byte[] data = schema.encode(value);
      T decoded = schema.decode(data);
      schema.identify(decoded, document, highwater, null, null, false, true);
      // Ordinary decoding restores schema container order, independently of a draft's custom comparator.
      byte[] canonical = schema.encode(decoded);
      return new ManagedSnapshot<>(schema, document, epoch, revision, canonical);
    }
    /** Reference initializers cannot remove, replace, exchange or retype an identified occurrence. */
    private void verifyIdentified(T value, java.util.IdentityHashMap<java.lang.Object, java.lang.Long> expected,
        long highwater) {
      schema.identify(value, document, highwater, null, null, false, true);
      java.util.IdentityHashMap<java.lang.Object, java.lang.Long> actual = schema.bindings(value);
      if (actual.size() != expected.size()) throw new ManagedError("Reference initialization changed ownership");
      for (java.util.Map.Entry<java.lang.Object, java.lang.Long> entry : expected.entrySet()) {
        if (!entry.getValue().equals(actual.get(entry.getKey()))) throw new ManagedError("Reference initialization changed identity");
      }
    }
    /** Persist observable reservations separately so failed callbacks cannot recycle any assigned identity. */
    private void reserve(long highwater) {
      ManagedState<T> before = state;
      if (highwater != before.highwater) publish(ManagedOperation.RESERVE, new ManagedState<>(highwater,
          before.epoch, before.revision, before.history, before.cursor, before.current, before.receipts));
    }
    /** Prepare every history allocation and cursor before writing a single acceptance frame. */
    private ManagedState<T> prepare(T value) {
      ManagedState<T> before = state;
      if (before.epoch == -1L || before.revision == -1L) throw new ManagedError("Managed generation space exhausted");
      ManagedSnapshot<T> next = snapshot(value, before.highwater, before.epoch + 1, before.revision + 1);
      if (next.data.length > options.maxHistoryBytes()) throw new ManagedError("Managed history snapshot exceeds bound");
      java.util.ArrayList<ManagedSnapshot<T>> history = new java.util.ArrayList<>(before.history.subList(0, before.cursor + 1));
      history.add(next);
      long bytes = 0;
      for (ManagedSnapshot<T> pin : history) bytes = java.lang.Math.addExact(bytes, pin.data.length);
      while (history.size() > options.maxHistoryEntries() || bytes > options.maxHistoryBytes()) {
        bytes -= history.remove(0).data.length;
      }
      return new ManagedState<>(before.highwater, next.epoch, next.revision, history,
          history.size() - 1, next, before.receipts);
    }
    /** After force only one prebuilt volatile-state assignment remains; any interrupted append fences mutation. */
    private void publish(ManagedOperation operation, ManagedState<T> next) {
      int sequence = journal == null ? -1 : journal.sequence;
      try {
        if (journal != null) journal.append(operation, encodeState(next));
        state = next;
      } catch (java.lang.RuntimeException | java.lang.Error error) {
        if (journal == null || journal.indeterminate || journal.sequence != sequence) fenced = true;
        throw error;
      }
    }
    /** Execute one isolated synchronous action; escaped mutable drafts stay detached from accepted bytes. */
    public ManagedSnapshot<T> transact(java.util.function.Consumer<ManagedEdit<T>> callback) {
      return transact(callback, null);
    }
    /** Assign IDs before the optional token-initialization callback and burn failed visible reservations. */
    public ManagedSnapshot<T> transact(java.util.function.Consumer<ManagedEdit<T>> callback,
        java.util.function.Consumer<ManagedEdit<T>> initializeReferences) {
      checkMutation(); busy = true; poison = null;
      ManagedEdit<T> edit = null;
      try {
        T value = state.current.copyValue();
        java.util.IdentityHashMap<java.lang.Object, java.lang.Long> bindings = schema.bindings(value);
        edit = new ManagedEdit<>(this, value);
        callback.accept(edit);
        if (poison != null) throw poison;
        long highwater = schema.identify(value, document, state.highwater, bindings, null, true, false);
        reserve(highwater);
        edit.identified = schema.bindings(value);
        if (initializeReferences != null) initializeReferences.accept(edit);
        verifyIdentified(value, edit.identified, highwater);
        if (poison != null) throw poison;
        ManagedState<T> next = prepare(value);
        if (java.util.Arrays.equals(next.current.data, state.current.data)) return state.current;
        publish(ManagedOperation.ACCEPT, next);
        return next.current;
      } finally { if (edit != null) edit.active = false; busy = false; }
    }
    /** Restore an adjacent immutable revision through a new epoch without recycling action or entity IDs. */
    private ManagedSnapshot<T> navigate(int offset) {
      checkMutation();
      int cursor = state.cursor + offset;
      if (cursor < 0 || cursor >= state.history.size()) throw new ManagedError("No retained history destination");
      if (state.epoch == -1L) throw new ManagedError("Managed epoch space exhausted");
      ManagedSnapshot<T> old = state.history.get(cursor);
      ManagedSnapshot<T> pin = new ManagedSnapshot<>(schema, document, state.epoch + 1, old.revision, old.data);
      publish(ManagedOperation.NAVIGATE, new ManagedState<>(state.highwater, pin.epoch, state.revision,
          state.history, cursor, pin, state.receipts));
      return pin;
    }
    /** Undo exactly one retained action; new edits discard the previous redo suffix. */
    public ManagedSnapshot<T> undo() { return navigate(-1); }
    /** Redo exactly one retained action, without rerunning callbacks. */
    public ManagedSnapshot<T> redo() { return navigate(1); }

    /** Create a new independent journal before any ordinary edit or authority request. */
    public void createJournal(java.nio.file.Path path) {
      checkMutation();
      if (journal != null || state.epoch != 0 || !state.receipts.isEmpty()) {
        throw new ManagedError("Create the native journal before editing or accepting requests");
      }
      ManagedJournal created = new ManagedJournal(path, options, true);
      try { created.append(ManagedOperation.BASELINE, encodeState(state)); }
      catch (java.lang.RuntimeException | java.lang.Error error) {
        if (created.indeterminate) fenced = true;
        try { created.close(); } catch (java.lang.RuntimeException closing) { error.addSuppressed(closing); }
        throw error;
      }
      journal = created;
    }

    /** Close only on an idle owner thread; accepted byte pins require no file/resource handles. */
    @Override public void close() {
      checkOwner();
      if (busy) { poison = new ManagedError("Cannot close an active managed callback"); throw poison; }
      ManagedJournal selected = journal; journal = null; fenced = true;
      if (selected != null) selected.close();
    }

    // Full-state journal encoding/recovery is implemented below; each record is independently bounded.
    private byte[] encodeState(ManagedState<T> selected) { return encodeManagedState(this, selected); }

    /** Reopen under explicit matching limits; recovery never silently adopts larger saved limits. */
    public static <U> ManagedStore<U> openJournal(ManagedSchema<U> schema, java.nio.file.Path path, ManagedOptions options) {
      return recoverManagedStore(java.util.Objects.requireNonNull(schema), path, java.util.Objects.requireNonNull(options));
    }
    /** Reopen the default bounded native profile without running application callbacks. */
    public static <U> ManagedStore<U> openJournal(ManagedSchema<U> schema, java.nio.file.Path path) {
      return openJournal(schema, path, ManagedOptions.defaults());
    }
  }

  /** Serialize authority decisions on the owning store, retaining exact original accepted/conflict outcomes. */
  public static final class ManagedAuthority<T> {
    private final ManagedStore<T> store;
    public ManagedAuthority(ManagedStore<T> store) { this.store = java.util.Objects.requireNonNull(store); }
    /** Apply one exact proposal or return its original durable receipt without reexecuting user code. */
    public ManagedReceipt<T> accept(ManagedProposal proposal) {
      store.checkMutation();
      if (!store.document.equals(proposal.document) || !store.schema.schemaId.equals(proposal.schemaId) ||
          proposal.data.length > store.schema.maxSnapshotBytes) throw new ManagedError("Authority request binding differs");
      ManagedRequestKey key = new ManagedRequestKey(proposal.clientId, proposal.requestId);
)javaEngine"
R"javaEngine(      ManagedReceipt<T> retained = store.state.receipts.get(key);
      if (retained != null) {
        if (!retained.proposal.same(proposal)) throw new ManagedError("Retry identity reused with different bytes/base");
        return retained;
      }
      if (store.state.receipts.size() >= store.options.maxReceipts()) throw new ManagedError("Native retry ledger is full");
      if (java.lang.Long.compareUnsigned(proposal.baseEpoch, store.state.epoch) > 0) throw new ManagedError("Future authority epoch");
      ManagedState<T> prepared;
      ManagedReceipt<T> receipt;
      ManagedOperation operation;
      store.busy = true; store.poison = null;
      try {
        if (proposal.baseEpoch != store.state.epoch) {
          prepared = store.state;
          receipt = new ManagedReceipt<>(proposal, ManagedStatus.CONFLICT, prepared.current);
          operation = ManagedOperation.RECEIPT;
        } else {
          T candidate = store.schema.decode(proposal.data);
          if (!java.util.Arrays.equals(proposal.data, store.schema.encode(candidate))) {
            throw new ManagedError("Authority proposal payload is not canonical");
          }
          long highwater = store.schema.identify(candidate, store.document, store.state.highwater, null,
              store.schema.entities(store.state.current.copyValue()), true, true);
          T previousRoot = store.state.current.copyValue();
          if (store.schema.inventory(candidate).objects.get(candidate).identity().getAsLong() !=
              store.schema.inventory(previousRoot).objects.get(previousRoot).identity().getAsLong()) {
            throw new ManagedError("Authority proposal changes the document root identity");
          }
          store.reserve(highwater);
          prepared = store.prepare(candidate);
          receipt = new ManagedReceipt<>(proposal, ManagedStatus.ACCEPTED, prepared.current);
          operation = ManagedOperation.ACCEPT;
        }
        java.util.LinkedHashMap<ManagedRequestKey, ManagedReceipt<T>> receipts = new java.util.LinkedHashMap<>(prepared.receipts);
        receipts.put(key, receipt);
        long bytes = 0;
        for (ManagedReceipt<T> item : receipts.values()) {
          bytes = java.lang.Math.addExact(bytes, (long) item.proposal.data.length + item.snapshot.data.length);
        }
        if (bytes > store.options.maxReceiptBytes()) throw new ManagedError("Managed retry byte bound exceeded");
        prepared = new ManagedState<>(prepared.highwater, prepared.epoch, prepared.revision,
            prepared.history, prepared.cursor, prepared.current, receipts);
        store.publish(operation, prepared);
        return receipt;
      } finally { store.busy = false; }
    }
  }

  /** One owner-thread pending proposal; retries preserve exact bytes and callbacks execute only once. */
  public static final class ManagedClient<T> {
    private final ManagedSchema<T> schema;
    private final java.lang.String clientId;
    private final java.lang.Thread owner = java.lang.Thread.currentThread();
    private volatile ManagedSnapshot<T> confirmed;
    private ManagedProposal pending;
    private boolean busy;
    private ManagedError poison;
    public ManagedClient(ManagedSchema<T> schema, ManagedSnapshot<T> baseline, java.lang.String clientId) {
      this.schema = schema; this.clientId = checkedText(clientId, 256, "client");
      this.confirmed = importManagedSnapshot(schema, baseline);
    }
    /** A pending draft never masquerades as an accepted authority state. */
    public ManagedSnapshot<T> read() { return confirmed; }
    /** Confine local draft/pending mutation and poison caught nested attempts. */
    private void check() {
      if (java.lang.Thread.currentThread() != owner) throw new ManagedError("Managed client belongs to its owner thread");
      if (busy) { poison = new ManagedError("Nested managed client mutation"); throw poison; }
    }
    /** Prepare a detached ordinary draft; newly inserted IDs remain zero until authority acceptance. */
    public ManagedProposal propose(java.lang.String requestId, java.util.function.Consumer<T> callback) {
      check();
      if (pending != null) throw new ManagedError("Managed client already has a pending proposal");
      checkedText(requestId, 256, "request");
      T value = confirmed.copyValue();
      java.util.IdentityHashMap<java.lang.Object, java.lang.Long> bindings = schema.bindings(value);
      busy = true; poison = null;
      try { callback.accept(value); if (poison != null) throw poison; }
      finally { busy = false; }
      ManagedInventory inventory = schema.inventory(value);
      java.util.HashSet<ManagedEntityKey> targets = new java.util.HashSet<>();
      for (ManagedOccurrence item : inventory.entities) {
        long id = item.identity().getAsLong();
        if (!schema.validId(id) || (!item.managed() && id != 0) ||
            (item.managed() && id != bindings.getOrDefault(item.value(), 0L))) {
          throw new ManagedError("Client edit changed or imported authoritative identity");
        }
        if (item.managed() && id != 0) targets.add(new ManagedEntityKey(item.classId(), id));
      }
      for (ManagedEdge edge : inventory.edges) {
        if (!confirmed.document.equals(edge.document()) || edge.classId() != edge.expectedClassId() ||
            edge.objectId() == 0 || !schema.validId(edge.objectId()) ||
            !targets.contains(new ManagedEntityKey(edge.classId(), edge.objectId()))) {
          throw new ManagedError("Client reference scope, type or liveness differs");
        }
      }
      // Clone through generated codecs before the second encode so proposals share the authority's canonical order.
      byte[] canonical = schema.encode(schema.decode(schema.encode(value)));
      pending = new ManagedProposal(clientId, requestId, confirmed.document, schema.schemaId,
          confirmed.epoch, canonical);
      return pending;
    }
    /** Return the same immutable request identity without rerunning a callback or codec. */
    public ManagedProposal retry() {
      check();
      if (pending == null) throw new ManagedError("Managed client has no pending request");
      return pending;
    }
    /** Accept only the pending request's coherent original outcome; conflicts require a subsequent explicit edit. */
    public void acknowledge(ManagedReceipt<T> receipt) {
      check();
      if (pending == null || !receipt.proposal.same(pending) || !receipt.snapshot.document.equals(confirmed.document) ||
          !schema.sameContract(receipt.snapshot.schema) || java.lang.Long.compareUnsigned(receipt.snapshot.epoch, confirmed.epoch) < 0 ||
          (receipt.status == ManagedStatus.ACCEPTED && (pending.baseEpoch == -1L ||
              receipt.snapshot.epoch != pending.baseEpoch + 1 ||
              java.lang.Long.compareUnsigned(receipt.snapshot.revision, confirmed.revision) <= 0)) ||
          (receipt.status == ManagedStatus.CONFLICT &&
              java.lang.Long.compareUnsigned(receipt.snapshot.epoch, pending.baseEpoch) <= 0)) {
        throw new ManagedError("Managed receipt is not a coherent pending-request outcome");
      }
      ManagedSnapshot<T> imported = importManagedSnapshot(schema, receipt.snapshot);
      if (imported.rootIdentity() != confirmed.rootIdentity()) throw new ManagedError("Managed receipt replaces the document root");
      confirmed = imported; pending = null;
    }
  }

  /** Validate encoded metadata length; Java character counts alone do not bound native UTF-8 storage. */
  private static java.lang.String checkedText(java.lang.String value, int maximumBytes, java.lang.String kind) {
    if (value == null || value.isEmpty() || value.length() > maximumBytes || managedUtf8(value).length > maximumBytes) {
      throw new ManagedError("Invalid managed " + kind + " text");
    }
    return value;
  }

  /** Metadata identities reject malformed UTF-16 rather than silently substituting UTF-8 replacement bytes. */
  private static byte[] managedUtf8(java.lang.String value) {
    try {
      java.nio.ByteBuffer encoded = java.nio.charset.StandardCharsets.UTF_8.newEncoder()
          .onMalformedInput(java.nio.charset.CodingErrorAction.REPORT)
          .onUnmappableCharacter(java.nio.charset.CodingErrorAction.REPORT).encode(java.nio.CharBuffer.wrap(value));
      byte[] result = new byte[encoded.remaining()]; encoded.get(result); return result;
    } catch (java.nio.charset.CharacterCodingException error) { throw new ManagedError("Malformed managed metadata Unicode", error); }
  }

  /** Own and validate a foreign immutable pin through the receiving descriptor before accepting it. */
  private static <T> ManagedSnapshot<T> importManagedSnapshot(ManagedSchema<T> schema, ManagedSnapshot<T> pin) {
    if (!schema.sameContract(pin.schema)) throw new ManagedError("Client schema differs from its baseline");
    T value = schema.decode(pin.data);
    long highwater = 0;
    for (ManagedOccurrence item : schema.inventory(value).entities) {
      if (item.managed() && java.lang.Long.compareUnsigned(item.identity().getAsLong(), highwater) > 0) {
        highwater = item.identity().getAsLong();
      }
    }
    schema.identify(value, pin.document, highwater, null, null, false, true);
    if (!java.util.Arrays.equals(pin.data, schema.encode(value))) throw new ManagedError("Client baseline is not canonical");
    return new ManagedSnapshot<>(schema, pin.document, pin.epoch, pin.revision, pin.data.clone());
  }

  private enum ManagedOperation { BASELINE, RESERVE, ACCEPT, NAVIGATE, RECEIPT }

  private enum ManagedJournalEvent { BEFORE_WRITE, AFTER_LENGTH, AFTER_HEADER, AFTER_PAYLOAD, AFTER_CHECKSUM,
    AFTER_FORCE, RECOVERY_BEFORE_FORCE, RECOVERY_AFTER_FORCE }
  private record ManagedFrame(ManagedOperation operation, byte[] payload) {}

  /** Native format is independent of C++ storage; complete checksummed frames precede force/publication. */
  private static final class ManagedJournal implements java.lang.AutoCloseable {
    private static final byte[] MAGIC = {'S', 'R', 'L', 'J', 'V', 'M', 'G', 1};
    private static final int HEADER_BYTES = 2 * java.lang.Integer.BYTES;
    private static final int DIGEST_BYTES = 32;
    private final ManagedOptions options;
    private java.nio.channels.FileChannel file;
    private java.nio.channels.FileChannel lockFile;
    private java.nio.channels.FileLock lock;
    private java.nio.channels.FileLock dataLock;
    private int sequence;
    private long completeOffset;
    private boolean incompleteTail;
    private boolean indeterminate;
    private java.util.function.Consumer<ManagedJournalEvent> observer;

    /** Retain one stable OS writer-lock companion; never replace or delete another journal generation. */
    private ManagedJournal(java.nio.file.Path path, ManagedOptions options, boolean create) {
      this.options = options;
      java.nio.file.Path requested = java.util.Objects.requireNonNull(path).toAbsolutePath().normalize();
      try {
        if (requested.getParent() == null || requested.getFileName() == null) {
          throw new ManagedError("Native journal must name a file beneath an existing directory");
        }
        // Existing symlink aliases select the same stable writer companion as their native target.
        // Creation resolves only the parent: CREATE_NEW must still reject an existing final entry.
        java.nio.file.Path selected = create ? requested.getParent().toRealPath().resolve(requested.getFileName()) :
            requested.toRealPath();
        lockFile = java.nio.channels.FileChannel.open(selected.resolveSibling(selected.getFileName() + ".writer.lock"),
            java.nio.file.StandardOpenOption.CREATE, java.nio.file.StandardOpenOption.WRITE);
        lock = lockFile.tryLock();
        if (lock == null) throw new ManagedError("Managed journal already has a writer");
        file = create ? java.nio.channels.FileChannel.open(selected, java.nio.file.StandardOpenOption.CREATE_NEW,
            java.nio.file.StandardOpenOption.READ, java.nio.file.StandardOpenOption.WRITE) :
            java.nio.channels.FileChannel.open(selected, java.nio.file.StandardOpenOption.READ,
                java.nio.file.StandardOpenOption.WRITE);
        // A second native-object lock also excludes hard-link aliases with another companion name.
        dataLock = file.tryLock();
        if (dataLock == null) throw new ManagedError("Managed journal native file already has a writer");
        if (create) { write(java.nio.ByteBuffer.wrap(MAGIC)); file.force(true); }
        completeOffset = MAGIC.length;
      } catch (java.io.IOException | java.lang.RuntimeException error) {
        try { close(); } catch (java.lang.RuntimeException closing) { error.addSuppressed(closing); }
        throw new ManagedError("Cannot open native managed journal", error);
      }
    }

    /** SHA-256 is mandatory in Java 17; callers prepare hashes before issuing any write. */
    private static byte[] digest(byte[] data) {
      try { return java.security.MessageDigest.getInstance("SHA-256").digest(data); }
      catch (java.security.NoSuchAlgorithmException error) { throw new ManagedError("SHA-256 unavailable", error); }
    }

    /** Write a prepared buffer completely; short/no-progress writes never report successful acceptance. */
    private void write(java.nio.ByteBuffer buffer) throws java.io.IOException {
      while (buffer.hasRemaining()) {
        if (file.write(buffer) <= 0) throw new java.io.IOException("Native journal write made no progress");
      }
    }

    /** Read a bounded prepared buffer, reporting only genuine EOF as an incomplete final frame. */
    private boolean read(java.nio.ByteBuffer buffer) throws java.io.IOException {
      while (buffer.hasRemaining()) {
        int count = file.read(buffer);
        if (count < 0) return false;
        if (count == 0) throw new java.io.IOException("Native journal read made no progress");
      }
      return true;
    }

    /** Observers are test fault boundaries and must not perform application effects or reenter mutation. */
    private void event(ManagedJournalEvent value) { if (observer != null) observer.accept(value); }

    /** All encoding/bounds/hash/buffer allocations finish before any append can become observable. */
    private void append(ManagedOperation operation, byte[] payload) {
      if (indeterminate) throw new ManagedIndeterminate("Native journal requires recovery", null);
      if (payload.length > options.maxFrameBytes() || sequence >= options.maxJournalRecords()) {
        throw new ManagedError("Native managed journal frame/record bound exceeded");
      }
      byte[] body = new byte[payload.length + 1];
      body[0] = operationTag(operation);
      java.lang.System.arraycopy(payload, 0, body, 1, payload.length);
      byte[] header = java.nio.ByteBuffer.allocate(HEADER_BYTES).putInt(body.length).putInt(sequence).array();
      byte[] headerDigest = digest(header);
      byte[] bodyDigest = digest(body);
      java.nio.ByteBuffer headerBuffer = java.nio.ByteBuffer.wrap(header);
      java.nio.ByteBuffer headerDigestBuffer = java.nio.ByteBuffer.wrap(headerDigest);
      java.nio.ByteBuffer bodyBuffer = java.nio.ByteBuffer.wrap(body);
      java.nio.ByteBuffer bodyDigestBuffer = java.nio.ByteBuffer.wrap(bodyDigest);
      try {
        long frameBytes = (long) HEADER_BYTES + 2L * DIGEST_BYTES + body.length;
        if (frameBytes > options.maxJournalBytes() || file.size() > options.maxJournalBytes() - frameBytes) {
          throw new ManagedError("Native managed journal byte capacity exhausted");
        }
        event(ManagedJournalEvent.BEFORE_WRITE);
        file.position(file.size());
)javaEngine"
R"javaEngine(      } catch (java.io.IOException error) { throw new ManagedError("Cannot prepare native journal append", error); }
      try {
        write(headerBuffer); event(ManagedJournalEvent.AFTER_LENGTH);
        write(headerDigestBuffer); event(ManagedJournalEvent.AFTER_HEADER);
        write(bodyBuffer); event(ManagedJournalEvent.AFTER_PAYLOAD);
        write(bodyDigestBuffer); event(ManagedJournalEvent.AFTER_CHECKSUM);
        file.force(true);
        ++sequence;
        event(ManagedJournalEvent.AFTER_FORCE);
      } catch (java.io.IOException | java.lang.RuntimeException error) {
        indeterminate = true;
        throw new ManagedIndeterminate("Native journal acceptance is uncertain; reopen required", error);
      } catch (java.lang.Error error) { indeterminate = true; throw error; }
    }

    /** Inspect the bounded file before parsing; no corruption/semantic failure can trigger tail repair. */
    private void beginRecovery() {
      try {
        if (file.size() > options.maxJournalBytes()) throw new ManagedError("Native journal recovery byte bound exceeded");
        file.position(0);
        byte[] magic = new byte[MAGIC.length];
        if (!read(java.nio.ByteBuffer.wrap(magic)) || !java.util.Arrays.equals(magic, MAGIC)) {
          throw new ManagedError("Unknown native Java managed journal format");
        }
      } catch (java.io.IOException error) { throw new ManagedError("Cannot read native journal header", error); }
    }

    /** Complete header corruption cannot masquerade as a shorter log; only true final truncation is repairable. */
    private ManagedFrame next() {
      try {
        if (file.position() == file.size()) return null;
        byte[] header = new byte[HEADER_BYTES];
        if (!read(java.nio.ByteBuffer.wrap(header))) { incompleteTail = true; return null; }
        byte[] headerDigest = new byte[DIGEST_BYTES];
        if (!read(java.nio.ByteBuffer.wrap(headerDigest))) { incompleteTail = true; return null; }
        if (!java.util.Arrays.equals(digest(header), headerDigest)) throw new ManagedError("Native journal header checksum differs");
        java.nio.ByteBuffer words = java.nio.ByteBuffer.wrap(header);
        int length = words.getInt();
        int expectedSequence = words.getInt();
        if (length < 1 || length - 1 > options.maxFrameBytes() || expectedSequence != sequence ||
            sequence >= options.maxJournalRecords()) throw new ManagedError("Native journal frame metadata exceeds bounds");
        if ((long) length + DIGEST_BYTES > file.size() - file.position()) { incompleteTail = true; return null; }
        byte[] body = new byte[length];
        byte[] bodyDigest = new byte[DIGEST_BYTES];
        if (!read(java.nio.ByteBuffer.wrap(body)) || !read(java.nio.ByteBuffer.wrap(bodyDigest))) {
          incompleteTail = true; return null;
        }
        if (!java.util.Arrays.equals(digest(body), bodyDigest)) throw new ManagedError("Native journal complete frame checksum differs");
        ManagedOperation operation = operationFromTag(body[0]);
        ++sequence;
        completeOffset = file.position();
        return new ManagedFrame(operation, java.util.Arrays.copyOfRange(body, 1, body.length));
      } catch (java.io.IOException error) { throw new ManagedError("Cannot read native journal frame", error); }
    }

    /** Flush even a complete recovered generation before exposing exact outcomes from unacknowledged writes. */
    private void finishRecovery() {
      try {
        if (incompleteTail) file.truncate(completeOffset);
        event(ManagedJournalEvent.RECOVERY_BEFORE_FORCE);
        file.force(true);
        event(ManagedJournalEvent.RECOVERY_AFTER_FORCE);
        file.position(completeOffset);
      } catch (java.io.IOException | java.lang.RuntimeException error) {
        throw new ManagedIndeterminate("Native recovery flush is uncertain", error);
      }
    }

    /** Release all handles even when close itself fails; stable writer-lock companions are never deleted. */
    @Override public void close() {
      java.io.IOException failure = null;
      try { if (dataLock != null) dataLock.close(); } catch (java.io.IOException error) { failure = error; }
      finally { dataLock = null; }
      try { if (file != null) file.close(); } catch (java.io.IOException error) {
        if (failure == null) failure = error; else failure.addSuppressed(error);
      } finally {
        file = null;
        try { if (lock != null) lock.close(); } catch (java.io.IOException error) {
          if (failure == null) failure = error; else failure.addSuppressed(error);
        } finally {
          lock = null;
          try { if (lockFile != null) lockFile.close(); } catch (java.io.IOException error) {
            if (failure == null) failure = error; else failure.addSuppressed(error);
          } finally { lockFile = null; }
        }
      }
      if (failure != null) throw new ManagedError("Cannot close native journal handles", failure);
    }
  }

  /** Native wire tags are explicit, independent of Java enum ordinal/layout. */
  private static byte operationTag(ManagedOperation operation) {
    return switch (operation) {
      case BASELINE -> 1; case RESERVE -> 2; case ACCEPT -> 3; case NAVIGATE -> 4; case RECEIPT -> 5;
    };
  }
  /** Reject unknown native operations rather than treating future records as a partial tail. */
  private static ManagedOperation operationFromTag(byte tag) {
    return switch (tag) {
      case 1 -> ManagedOperation.BASELINE; case 2 -> ManagedOperation.RESERVE;
      case 3 -> ManagedOperation.ACCEPT; case 4 -> ManagedOperation.NAVIGATE; case 5 -> ManagedOperation.RECEIPT;
      default -> throw new ManagedError("Unknown native Java journal operation");
    };
  }

  private static final class ManagedBoundedOutput extends java.io.ByteArrayOutputStream {
    private final int maximum;
    private ManagedBoundedOutput(int maximum) { this.maximum = maximum; }
    /** Admission precedes every backing-buffer growth; this is not a total JVM heap allocation bound. */
    @Override public synchronized void write(int value) {
      if (count >= maximum) throw new ManagedError("Native managed state frame exceeds bound");
      super.write(value);
    }
    /** Check the requested logical byte extent before delegating to the standard backing buffer. */
    @Override public synchronized void write(byte[] value, int offset, int length) {
      java.util.Objects.checkFromIndexSize(offset, length, value.length);
      if (length > maximum - count) throw new ManagedError("Native managed state frame exceeds bound");
      super.write(value, offset, length);
    }
  }

  /** Length-prefix exact owned bytes independently of mutable model layouts. */
  private static void writeManagedBytes(java.io.DataOutputStream output, byte[] value) throws java.io.IOException {
    output.writeInt(value.length); output.write(value);
  }
  /** Bound lengths before allocating; incomplete inner records are complete-frame semantic corruption. */
  private static byte[] readManagedBytes(java.io.DataInputStream input, int maximum) throws java.io.IOException {
    int length = input.readInt();
    if (length < 0 || length > maximum || length > input.available()) throw new ManagedError("Native managed byte field exceeds bound");
    byte[] result = new byte[length]; input.readFully(result); return result;
  }
  /** Metadata text has fixed UTF-8 byte bounds and never depends on modified-UTF Java serialization. */
  private static void writeManagedText(java.io.DataOutputStream output, java.lang.String value) throws java.io.IOException {
    writeManagedBytes(output, managedUtf8(value));
  }
  /** Reject malformed UTF-8 instead of replacing distinct metadata identities with the same text. */
  private static java.lang.String readManagedText(java.io.DataInputStream input, int maximum) throws java.io.IOException {
    byte[] bytes = readManagedBytes(input, maximum);
    java.lang.String text = java.nio.charset.StandardCharsets.UTF_8.newDecoder()
        .onMalformedInput(java.nio.charset.CodingErrorAction.REPORT)
        .onUnmappableCharacter(java.nio.charset.CodingErrorAction.REPORT)
        .decode(java.nio.ByteBuffer.wrap(bytes)).toString();
    return checkedText(text, maximum, "journal");
  }

  /** Store complete independent states: no replay dependency chain or pruned-snapshot retention is introduced. */
  private static <T> byte[] encodeManagedState(ManagedStore<T> store, ManagedState<T> state) {
    ManagedBoundedOutput bytes = new ManagedBoundedOutput(store.options.maxFrameBytes());
    try {
      java.io.DataOutputStream output = new java.io.DataOutputStream(bytes);
      writeManagedText(output, store.schema.schemaId); output.writeInt(store.schema.idBytes);
      output.writeLong(store.document.high()); output.writeLong(store.document.low());
      output.writeInt(store.options.maxHistoryEntries()); output.writeLong(store.options.maxHistoryBytes());
      output.writeInt(store.options.maxReceipts()); output.writeLong(store.options.maxReceiptBytes());
      output.writeInt(store.options.maxFrameBytes()); output.writeLong(store.options.maxJournalBytes());
      output.writeInt(store.options.maxJournalRecords());
      output.writeLong(state.highwater); output.writeLong(state.epoch); output.writeLong(state.revision);
      output.writeInt(state.cursor); output.writeInt(state.history.size());
      for (ManagedSnapshot<T> pin : state.history) {
        output.writeLong(pin.epoch); output.writeLong(pin.revision); writeManagedBytes(output, pin.data);
      }
      output.writeInt(state.receipts.size());
      for (ManagedReceipt<T> receipt : state.receipts.values()) {
        writeManagedText(output, receipt.proposal.clientId); writeManagedText(output, receipt.proposal.requestId);
        output.writeLong(receipt.proposal.baseEpoch); writeManagedBytes(output, receipt.proposal.data);
        output.writeByte(receipt.status == ManagedStatus.ACCEPTED ? 1 : 2);
        output.writeLong(receipt.snapshot.epoch); output.writeLong(receipt.snapshot.revision);
        writeManagedBytes(output, receipt.snapshot.data);
      }
      output.flush(); return bytes.toByteArray();
    } catch (java.io.IOException error) { throw new ManagedError("Cannot encode native managed state", error); }
  }

  private record ManagedRecovered<T>(ManagedDocument document, ManagedState<T> state) {}

  /** Decode one complete generation under caller-selected bounds, validating every retained graph and receipt. */
  private static <T> ManagedRecovered<T> decodeManagedState(ManagedSchema<T> schema, ManagedOptions limits, byte[] bytes) {
    if (bytes.length > limits.maxFrameBytes()) throw new ManagedError("Native state exceeds frame bound");
    try {
      java.io.DataInputStream input = new java.io.DataInputStream(new java.io.ByteArrayInputStream(bytes));
      if (!readManagedText(input, 4096).equals(schema.schemaId) || input.readInt() != schema.idBytes) {
        throw new ManagedError("Native managed schema binding differs");
      }
      ManagedDocument document = new ManagedDocument(input.readLong(), input.readLong());
      ManagedOptions saved = new ManagedOptions(input.readInt(), input.readLong(), input.readInt(),
          input.readLong(), input.readInt(), input.readLong(), input.readInt());
      if (!saved.equals(limits)) throw new ManagedError("Native recovery limits differ from saved contract");
      long highwater = input.readLong(); long epoch = input.readLong(); long revision = input.readLong();
      if (!schema.validId(highwater) || java.lang.Long.compareUnsigned(revision, epoch) > 0) {
        throw new ManagedError("Invalid native identity/generation watermark");
      }
      int cursor = input.readInt(); int count = input.readInt();
      if (count < 1 || count > limits.maxHistoryEntries() || cursor < 0 || cursor >= count) {
        throw new ManagedError("Invalid native retained history cursor/count");
      }
      java.util.ArrayList<ManagedSnapshot<T>> history = new java.util.ArrayList<>();
      java.util.HashMap<java.lang.Long, java.lang.Long> types = new java.util.HashMap<>();
      long historyBytes = 0; long rootId = 0; long lastRevision = 0; long lastEpoch = 0;
      for (int index = 0; index < count; ++index) {
        long pinEpoch = input.readLong(); long pinRevision = input.readLong();
        byte[] data = readManagedBytes(input, schema.maxSnapshotBytes);
        if (data.length > limits.maxHistoryBytes() - historyBytes ||
            java.lang.Long.compareUnsigned(pinEpoch, epoch) > 0 ||
            java.lang.Long.compareUnsigned(pinRevision, revision) > 0 ||
            java.lang.Long.compareUnsigned(pinRevision, pinEpoch) > 0 ||
            (index != 0 && (java.lang.Long.compareUnsigned(pinRevision, lastRevision) <= 0 ||
              java.lang.Long.compareUnsigned(pinEpoch, lastEpoch) <= 0))) {
          throw new ManagedError("Native retained history metadata exceeds bounds");
        }
        historyBytes += data.length; lastRevision = pinRevision; lastEpoch = pinEpoch;
        T value = schema.decode(data);
        schema.identify(value, document, highwater, null, null, false, true);
        if (!java.util.Arrays.equals(data, schema.encode(value))) throw new ManagedError("Native snapshot bytes are not canonical");
        ManagedInventory inventory = schema.inventory(value);
        long id = inventory.objects.get(value).identity().getAsLong();
        if (index == 0) rootId = id;
        if (id != rootId) throw new ManagedError("Native history changes root identity");
        for (ManagedOccurrence item : inventory.entities) {
          if (!item.managed()) continue;
          java.lang.Long old = types.putIfAbsent(item.identity().getAsLong(), item.classId());
          if (old != null && old.longValue() != item.classId()) throw new ManagedError("Native history retypes an entity");
        }
        history.add(new ManagedSnapshot<>(schema, document, pinEpoch, pinRevision, data));
      }
      ManagedSnapshot<T> selected = history.get(cursor);
      ManagedSnapshot<T> current = new ManagedSnapshot<>(schema, document, epoch, selected.revision, selected.data);
      int receiptCount = input.readInt();
      if (receiptCount < 0 || receiptCount > limits.maxReceipts()) throw new ManagedError("Native retry ledger count exceeds bound");
      java.util.LinkedHashMap<ManagedRequestKey, ManagedReceipt<T>> receipts = new java.util.LinkedHashMap<>();
      long receiptBytes = 0;
      for (int index = 0; index < receiptCount; ++index) {
        java.lang.String client = readManagedText(input, 256); java.lang.String request = readManagedText(input, 256);
        long baseEpoch = input.readLong(); byte[] proposed = readManagedBytes(input, schema.maxSnapshotBytes);
        int tag = input.readUnsignedByte();
        if (tag != 1 && tag != 2) throw new ManagedError("Unknown native authority status");
        long acceptedEpoch = input.readLong(); long acceptedRevision = input.readLong();
        byte[] accepted = readManagedBytes(input, schema.maxSnapshotBytes);
        long size = (long) proposed.length + accepted.length;
        if (size > limits.maxReceiptBytes() - receiptBytes ||
            java.lang.Long.compareUnsigned(acceptedEpoch, epoch) > 0 ||
            java.lang.Long.compareUnsigned(acceptedRevision, revision) > 0 ||
            java.lang.Long.compareUnsigned(acceptedRevision, acceptedEpoch) > 0 ||
            (tag == 1 && (baseEpoch == -1L || acceptedEpoch != baseEpoch + 1)) ||
            (tag == 2 && java.lang.Long.compareUnsigned(acceptedEpoch, baseEpoch) <= 0)) {
          throw new ManagedError("Native receipt generation or byte bound differs");
        }
        receiptBytes += size;
)javaEngine"
R"javaEngine(        T acceptedValue = schema.decode(accepted);
        schema.identify(acceptedValue, document, highwater, null, null, false, true);
        if (schema.inventory(acceptedValue).objects.get(acceptedValue).identity().getAsLong() != rootId ||
            !java.util.Arrays.equals(accepted, schema.encode(acceptedValue))) {
          throw new ManagedError("Native receipt changes root or canonical payload");
        }
        ManagedProposal proposal = new ManagedProposal(client, request, document, schema.schemaId, baseEpoch, proposed);
        ManagedReceipt<T> receipt = new ManagedReceipt<>(proposal, tag == 1 ? ManagedStatus.ACCEPTED : ManagedStatus.CONFLICT,
            new ManagedSnapshot<>(schema, document, acceptedEpoch, acceptedRevision, accepted));
        if (receipts.put(new ManagedRequestKey(client, request), receipt) != null) throw new ManagedError("Duplicate native retry identity");
      }
      if (input.available() != 0) throw new ManagedError("Trailing native state metadata");
      return new ManagedRecovered<>(document, new ManagedState<>(highwater, epoch, revision, history, cursor, current, receipts));
    } catch (java.io.IOException | java.lang.ArithmeticException error) {
      throw new ManagedError("Invalid complete native managed state", error);
    }
  }

  /** Compare complete immutable pins without decoding or allocating a second payload. */
  private static boolean samePin(ManagedSnapshot<?> first, ManagedSnapshot<?> second) {
    return first.document.equals(second.document) && first.epoch == second.epoch && first.revision == second.revision &&
        first.schema.schemaId.equals(second.schema.schemaId) && java.util.Arrays.equals(first.data, second.data);
  }
  /** Full-state journal records must preserve every retained independent history entry exactly. */
  private static boolean sameHistory(java.util.List<? extends ManagedSnapshot<?>> first,
      java.util.List<? extends ManagedSnapshot<?>> second) {
    if (first.size() != second.size()) return false;
    for (int index = 0; index < first.size(); ++index) if (!samePin(first.get(index), second.get(index))) return false;
    return true;
  }
  /** Exact receipts keep the original request and original outcome even after later authority changes. */
  private static boolean sameReceipt(ManagedReceipt<?> first, ManagedReceipt<?> second) {
    return first.proposal.same(second.proposal) && first.status == second.status && samePin(first.snapshot, second.snapshot);
  }

  /** Validate operation-specific history, identity, receipt and epoch transitions before any tail repair. */
  private static <T> void validateManagedTransition(ManagedStore<T> store, ManagedState<T> before,
      ManagedState<T> after, ManagedOperation operation, java.lang.Long reservationFloor) {
    if (operation == ManagedOperation.BASELINE) throw new ManagedError("Unexpected second native baseline");
    ManagedReceipt<T> added = null;
    if (after.receipts.size() < before.receipts.size() || after.receipts.size() > before.receipts.size() + 1) {
      throw new ManagedError("Native retry ledger size changed unexpectedly");
    }
    for (java.util.Map.Entry<ManagedRequestKey, ManagedReceipt<T>> entry : before.receipts.entrySet()) {
      ManagedReceipt<T> value = after.receipts.get(entry.getKey());
      if (value == null || !sameReceipt(entry.getValue(), value)) throw new ManagedError("Native retry outcome was changed");
    }
    for (java.util.Map.Entry<ManagedRequestKey, ManagedReceipt<T>> entry : after.receipts.entrySet()) {
      if (!before.receipts.containsKey(entry.getKey())) added = entry.getValue();
    }
    if (operation == ManagedOperation.RESERVE) {
      if (java.lang.Long.compareUnsigned(after.highwater, before.highwater) <= 0 || after.epoch != before.epoch ||
          after.revision != before.revision || after.cursor != before.cursor || added != null ||
          !sameHistory(before.history, after.history) || !samePin(before.current, after.current)) {
        throw new ManagedError("Invalid native identity reservation transition");
      }
      return;
    }
    if (after.highwater != before.highwater) throw new ManagedError("Native operation changed unreserved identity watermark");
    if (operation == ManagedOperation.RECEIPT) {
      if (added == null || added.status != ManagedStatus.CONFLICT || !samePin(added.snapshot, before.current) ||
          java.lang.Long.compareUnsigned(added.proposal.baseEpoch, before.epoch) >= 0 ||
          after.epoch != before.epoch || after.revision != before.revision || after.cursor != before.cursor ||
          !sameHistory(before.history, after.history) || !samePin(before.current, after.current)) {
        throw new ManagedError("Invalid native conflict receipt transition");
      }
      return;
    }
    if (before.epoch == -1L || after.epoch != before.epoch + 1) throw new ManagedError("Native acceptance epoch differs");
    if (operation == ManagedOperation.NAVIGATE) {
      if (after.revision != before.revision || java.lang.Math.abs(after.cursor - before.cursor) != 1 ||
          added != null || !sameHistory(before.history, after.history)) {
        throw new ManagedError("Invalid native retained navigation transition");
      }
      return;
    }
    if (operation != ManagedOperation.ACCEPT || before.revision == -1L || after.revision != before.revision + 1 ||
        after.current.revision != after.revision || after.cursor != after.history.size() - 1) {
      throw new ManagedError("Invalid native acceptance revision transition");
    }
    java.util.ArrayList<ManagedSnapshot<T>> expected = new java.util.ArrayList<>(before.history.subList(0, before.cursor + 1));
    expected.add(after.current);
    long bytes = 0;
    for (ManagedSnapshot<T> pin : expected) bytes += pin.data.length;
    while (expected.size() > store.options.maxHistoryEntries() || bytes > store.options.maxHistoryBytes()) {
      bytes -= expected.remove(0).data.length;
    }
    if (!sameHistory(expected, after.history)) throw new ManagedError("Native acceptance discarded or changed retained history");
    T previousValue = before.current.copyValue(); T nextValue = after.current.copyValue();
    ManagedInventory previousInventory = store.schema.inventory(previousValue);
    ManagedInventory nextInventory = store.schema.inventory(nextValue);
    if (previousInventory.objects.get(previousValue).identity().getAsLong() !=
        nextInventory.objects.get(nextValue).identity().getAsLong()) throw new ManagedError("Native acceptance changed root identity");
    java.util.Map<ManagedEntityKey, java.lang.Object> old = store.schema.entities(previousValue);
    for (ManagedEntityKey key : store.schema.entities(nextValue).keySet()) {
      if (!old.containsKey(key) && (reservationFloor == null ||
          java.lang.Long.compareUnsigned(key.objectId(), reservationFloor.longValue()) <= 0)) {
        throw new ManagedError("Native acceptance reused or retyped an unreserved identity");
      }
    }
    if (added != null) {
      if (added.status != ManagedStatus.ACCEPTED || added.proposal.baseEpoch != before.epoch ||
          !samePin(added.snapshot, after.current)) throw new ManagedError("Native accepted receipt binding differs");
      T requested = store.schema.decode(added.proposal.data);
      if (!java.util.Arrays.equals(added.proposal.data, store.schema.encode(requested))) {
        throw new ManagedError("Native accepted request is not canonical");
      }
      long highwater = store.schema.identify(requested, store.document,
          reservationFloor == null ? before.highwater : reservationFloor.longValue(), null, old, true, true);
      if (highwater != after.highwater || !java.util.Arrays.equals(store.schema.encode(requested), after.current.data)) {
        throw new ManagedError("Native accepted receipt does not reconstruct its original exact request");
      }
    }
  }

  /** Recover a locked native generation; all semantic replay and recovery force precede store exposure. */
  private static <T> ManagedStore<T> recoverManagedStore(ManagedSchema<T> schema, java.nio.file.Path path,
      ManagedOptions options) {
    return recoverManagedStore(schema, path, options, null);
  }

  /** Private fault observations allow recovery-force qualification without exposing an application effect hook. */
  private static <T> ManagedStore<T> recoverManagedStore(ManagedSchema<T> schema, java.nio.file.Path path,
      ManagedOptions options, java.util.function.Consumer<ManagedJournalEvent> observer) {
    ManagedJournal journal = new ManagedJournal(path, options, false);
    journal.observer = observer;
    try {
      journal.beginRecovery();
      ManagedStore<T> restored = null;
      java.lang.Long floor = null;
      ManagedFrame frame;
      while ((frame = journal.next()) != null) {
        ManagedRecovered<T> decoded = decodeManagedState(schema, options, frame.payload());
        ManagedState<T> next = decoded.state();
        if (restored == null) {
          if (frame.operation() != ManagedOperation.BASELINE || next.epoch != 0 || next.revision != 0 ||
              next.cursor != 0 || next.history.size() != 1 || !next.receipts.isEmpty()) {
            throw new ManagedError("Native journal lacks an exact initial baseline");
          }
          restored = new ManagedStore<>(schema, decoded.document(), options, next);
        } else {
          if (!restored.document.equals(decoded.document())) throw new ManagedError("Native document namespace changed");
          validateManagedTransition(restored, restored.state, next, frame.operation(), floor);
          floor = frame.operation() == ManagedOperation.RESERVE ? restored.state.highwater : null;
          restored.state = next;
        }
      }
      if (restored == null) throw new ManagedError("Native journal lacks a complete baseline");
      journal.finishRecovery();
      restored.journal = journal;
      return restored;
    } catch (java.lang.RuntimeException | java.lang.Error error) {
      try { journal.close(); } catch (java.lang.RuntimeException closing) { error.addSuppressed(closing); }
      throw error;
    }
  }
)javaEngine"
