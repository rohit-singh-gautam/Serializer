// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Compiled against actual generated EngineSdk.java, never an inserted runtime or fixture codec.
final class JavaManagedEngineGeneratedTest {
  private static int checks;
  private static final EngineSdk.ManagedDocument DOCUMENT = new EngineSdk.ManagedDocument(17, 18);

  /** Require contracts independently of optional JVM assertions. */
  private static void check(boolean value) {
    ++checks;
    if (!value) throw new java.lang.AssertionError("Generated Java native managed check " + checks);
  }

  /** Native contract failures remain explicit and leave the accepted state reviewable. */
  private static void reject(java.lang.Runnable action) {
    try { action.run(); }
    catch (EngineSdk.ManagedError expected) { ++checks; return; }
    throw new java.lang.AssertionError("Expected generated native Java rejection");
  }

  /** Read unsigned uint32 generated storage without sign extension. */
  private static long identity(int value) { return java.lang.Integer.toUnsignedLong(value); }

  /** Preserve every unsigned uint64 bit in its native long representation. */
  private static long identity(long value) { return value; }

  /** Construct a typed generated wire envelope using explicitly initialized native identity metadata. */
  private static EngineSdk.ManagedReference<EngineSdk.EngineGraph.Item> reference(EngineSdk.ManagedToken token) {
    return new EngineSdk.ManagedReference<>(token.document().high(), token.document().low(), token.classId(), token.objectId());
  }

  /** Convert an immutable generated envelope to the runtime's exact typed-resolution coordinates. */
  private static EngineSdk.ManagedToken token(EngineSdk.ManagedReference<?> reference) {
    return new EngineSdk.ManagedToken(new EngineSdk.ManagedDocument(reference.documentHigh, reference.documentLow),
        reference.classId, reference.objectId);
  }

  /** Build actual generated fields spanning managed maps/arrays, ordinary ancestors and scalar codecs. */
  private static EngineSdk.EngineGraph.Model initial() {
    EngineSdk.EngineGraph.Model value = new EngineSdk.EngineGraph.Model();
    value.title = "native baseline";
    value.children = new EngineSdk.EngineGraph.Item[] {new EngineSdk.EngineGraph.Item()};
    value.children[0].label = "owned child";
    value.mapped.put(7, new EngineSdk.EngineGraph.Item());
    value.ordinary.label = "ordinary";
    value.ordinaryBranch.leaves = new EngineSdk.EngineGraph.Item[] {new EngineSdk.EngineGraph.Item()};
    value.branches = new EngineSdk.EngineGraph.Branch[] {new EngineSdk.EngineGraph.Branch()};
    value.branches[0].leaves = new EngineSdk.EngineGraph.Item[] {new EngineSdk.EngineGraph.Item()};
    value.fixed[0] = "alpha"; value.fixed[1] = "\u03b2";
    value.note = java.util.Optional.of("note");
    value.mode = EngineSdk.EngineGraph.Kind.SECOND;
    value.precise = 7.5; value.ratio = 0.25f; value.enabled = true; value.balance = -19;
    value.fingerprint[0] = (byte) 255;
    return value;
  }

  /** Append fresh zero-ID objects without replacing any existing managed occurrence. */
  private static void append(EngineSdk.EngineGraph.Model value) {
    int count = value.children.length;
    value.children = java.util.Arrays.copyOf(value.children, count + 1);
    value.children[count] = new EngineSdk.EngineGraph.Item();
  }

  /** Immutable accepted bytes, detached drafts and logical participation use the actual native generated codecs. */
  private static void ownershipAndHistory() throws java.lang.InterruptedException {
    EngineSdk.ManagedSchema<EngineSdk.EngineGraph.Model> schema = EngineSdk.managedSchema(EngineSdk.EngineGraph.Model.class);
    EngineSdk.EngineGraph.Model input = initial();
    try (EngineSdk.ManagedStore<EngineSdk.EngineGraph.Model> store = new EngineSdk.ManagedStore<>(schema, input,
        DOCUMENT, EngineSdk.ManagedOptions.defaults(), edit ->
            edit.root().selected = java.util.Optional.of(reference(edit.token(edit.root().children[0]))))) {
      EngineSdk.ManagedSnapshot<EngineSdk.EngineGraph.Model> pin = store.read();
      input.title = "caller mutation"; input.fingerprint[0] = 0; input.children[0].label = "caller child";
      EngineSdk.EngineGraph.Model copy = pin.copyValue();
      check(copy.title.equals("native baseline") && copy.children[0].label.equals("owned child"));
      check(copy.fingerprint[0] == (byte) 255 && copy.precise == 7.5 && copy.ratio == 0.25f && copy.balance == -19);
      check(copy.fixed[1].equals("\u03b2") && copy.note.orElseThrow().equals("note") && copy.enabled);
      check(copy.mode == EngineSdk.EngineGraph.Kind.SECOND);
      check(identity(copy.persistentId) != 0 && identity(copy.children[0].persistentId) != 0);
      check(identity(copy.mapped.get(7).persistentId) != 0 && identity(copy.branches[0].persistentId) != 0);
      check(identity(copy.branches[0].leaves[0].persistentId) != 0);
      check(identity(copy.ordinary.persistentId) == 0 && identity(copy.ordinaryBranch.persistentId) == 0);
      check(identity(copy.ordinaryBranch.leaves[0].persistentId) == 0);
      EngineSdk.ManagedToken selected = token(copy.selected.orElseThrow());
      check(selected.classId() == -1L);
      check(identity(pin.resolve(selected, EngineSdk.EngineGraph.Item.class).persistentId) == selected.objectId());
      reject(() -> pin.resolve(selected, EngineSdk.EngineGraph.Branch.class));
      reject(() -> pin.resolve(new EngineSdk.ManagedToken(new EngineSdk.ManagedDocument(4, 5), -1L,
          selected.objectId()), EngineSdk.EngineGraph.Item.class));
      copy.title = "detached"; copy.fingerprint[0] = 0;
      byte[] bytes = pin.bytes(); bytes[0] ^= 1;
      check(pin.copyValue().title.equals("native baseline") && pin.copyValue().fingerprint[0] == (byte) 255);
      java.util.ArrayList<EngineSdk.ManagedEdit<EngineSdk.EngineGraph.Model>> escaped = new java.util.ArrayList<>();
      java.util.ArrayList<EngineSdk.EngineGraph.Model> drafts = new java.util.ArrayList<>();
      store.transact(edit -> { escaped.add(edit); drafts.add(edit.root()); edit.root().title = "accepted"; });
      drafts.get(0).title = "escaped";
      reject(() -> escaped.get(0).root());
      check(store.read().copyValue().title.equals("accepted") && pin.copyValue().title.equals("native baseline"));
      EngineSdk.ManagedSnapshot<EngineSdk.EngineGraph.Model> before = store.read();
      reject(() -> store.transact(edit -> { edit.root().title = "partial"; throw new EngineSdk.ManagedError("abort"); }));
      reject(() -> store.transact(edit -> { reject(() -> store.transact(inner -> {})); edit.root().title = "nested"; }));
      reject(() -> store.transact(edit -> { reject(store::close); edit.root().title = "closed"; }));
      check(store.read() == before && store.transact(edit -> {}) == before);
      reject(() -> store.transact(edit -> edit.root().persistentId++));
      reject(() -> store.transact(edit -> edit.root().ordinary.persistentId++));
      reject(() -> store.transact(edit -> edit.root().children = new EngineSdk.EngineGraph.Item[] {
          edit.root().children[0], edit.root().children[0]}));
      reject(() -> store.transact(edit -> edit.root().selected = java.util.Optional.of(
          new EngineSdk.ManagedReference<>(17, 18, 403, selected.objectId()))));
      check(store.read() == before);
      java.util.ArrayList<java.lang.Long> burned = new java.util.ArrayList<>();
      reject(() -> store.transact(edit -> append(edit.root()), edit -> {
        burned.add(edit.token(edit.root().children[1]).objectId()); throw new EngineSdk.ManagedError("after ID exposure");
      }));
      check(store.allocatedId() == burned.get(0));
      store.transact(edit -> append(edit.root()), edit -> edit.root().selected =
          java.util.Optional.of(reference(edit.token(edit.root().children[1]))));
      long next = store.read().copyValue().selected.orElseThrow().objectId;
      check(java.lang.Long.compareUnsigned(next, burned.get(0)) > 0);
      store.undo(); check(store.read().copyValue().children.length == 1);
      store.redo(); check(store.read().copyValue().selected.orElseThrow().objectId == next);
      EngineSdk.ManagedToken retained = token(store.read().copyValue().selected.orElseThrow());
      EngineSdk.ManagedSnapshot<EngineSdk.EngineGraph.Model> beforeDelete = store.read();
      store.transact(edit -> {
        edit.root().selected = java.util.Optional.empty();
        edit.root().children = new EngineSdk.EngineGraph.Item[0];
      });
      reject(() -> store.read().resolve(retained, EngineSdk.EngineGraph.Item.class));
      check(identity(beforeDelete.resolve(retained, EngineSdk.EngineGraph.Item.class).persistentId) == next);
      store.undo(); check(identity(store.read().resolve(retained, EngineSdk.EngineGraph.Item.class).persistentId) == next);
      store.redo(); check(store.read().copyValue().children.length == 0);
      store.undo(); store.transact(edit -> append(edit.root()));
      check(java.lang.Long.compareUnsigned(identity(store.read().copyValue().children[2].persistentId), next) > 0);
      reject(store::redo);
      java.util.ArrayList<java.lang.String> result = new java.util.ArrayList<>();
      java.lang.Thread reader = new java.lang.Thread(() -> {
        result.add(pin.copyValue().title);
        try { store.transact(edit -> {}); }
        catch (EngineSdk.ManagedError expected) { result.add("confined"); }
      });
      reader.start(); reader.join();
      check(result.equals(java.util.List.of("native baseline", "confined")));
    }
    reject(() -> EngineSdk.managedSchema(java.lang.String.class));
    reject(() -> new EngineSdk.ManagedStore<>(EngineSdk.managedSchema(EngineSdk.EngineGraph.Model.class, 1, 100),
        initial(), DOCUMENT, EngineSdk.ManagedOptions.defaults()));
    reject(() -> new EngineSdk.ManagedStore<>(EngineSdk.managedSchema(EngineSdk.EngineGraph.Model.class, 1048576, 1),
        initial(), DOCUMENT, EngineSdk.ManagedOptions.defaults()));
  }

  /** Persist exact original authority outcomes and restore IDs/history using newly created generated descriptors. */
  private static void journalAndCollaboration(java.nio.file.Path folder) throws java.io.IOException {
    java.nio.file.Path path = java.nio.file.Files.createTempDirectory(folder, "generated-journal-").resolve("state.srjvmg");
    EngineSdk.ManagedSchema<EngineSdk.EngineGraph.Model> schema = EngineSdk.managedSchema(EngineSdk.EngineGraph.Model.class);
    EngineSdk.ManagedProposal acceptedRequest;
    EngineSdk.ManagedProposal conflictRequest;
    byte[] acceptedBytes;
    try (EngineSdk.ManagedStore<EngineSdk.EngineGraph.Model> store = new EngineSdk.ManagedStore<>(schema,
        initial(), DOCUMENT, EngineSdk.ManagedOptions.defaults())) {
      store.createJournal(path);
      reject(() -> EngineSdk.ManagedStore.openJournal(schema, path));
      EngineSdk.ManagedClient<EngineSdk.EngineGraph.Model> first = new EngineSdk.ManagedClient<>(
          EngineSdk.managedSchema(EngineSdk.EngineGraph.Model.class), store.read(), "first");
      EngineSdk.ManagedClient<EngineSdk.EngineGraph.Model> stale = new EngineSdk.ManagedClient<>(schema, store.read(), "stale");
      java.util.ArrayList<java.lang.String> callbacks = new java.util.ArrayList<>();
      acceptedRequest = first.propose("request", draft -> { callbacks.add("once"); draft.title = "authority accepted"; append(draft); });
      check(first.retry() == acceptedRequest && first.read().copyValue().title.equals("native baseline"));
      EngineSdk.ManagedAuthority<EngineSdk.EngineGraph.Model> authority = new EngineSdk.ManagedAuthority<>(store);
      EngineSdk.ManagedReceipt<EngineSdk.EngineGraph.Model> accepted = authority.accept(acceptedRequest);
      check(accepted.status() == EngineSdk.ManagedStatus.ACCEPTED && authority.accept(first.retry()) == accepted);
      check(identity(accepted.snapshot().copyValue().children[1].persistentId) != 0);
      first.acknowledge(accepted);
      check(first.read().copyValue().title.equals("authority accepted") && callbacks.equals(java.util.List.of("once")));
      conflictRequest = stale.propose("request", draft -> draft.title = "stale");
      EngineSdk.ManagedReceipt<EngineSdk.EngineGraph.Model> conflict = authority.accept(conflictRequest);
      check(conflict.status() == EngineSdk.ManagedStatus.CONFLICT && authority.accept(conflictRequest) == conflict);
      stale.acknowledge(conflict); check(stale.read().copyValue().title.equals("authority accepted"));
      reject(stale::retry);
      reject(() -> first.propose("replace-root", draft -> draft.persistentId = 0));
      check(store.read().copyValue().title.equals("authority accepted"));
      acceptedBytes = accepted.snapshot().bytes();
      store.transact(edit -> edit.root().title = "later authority");
      check(java.util.Arrays.equals(authority.accept(acceptedRequest).snapshot().bytes(), acceptedBytes));
    }
    try (EngineSdk.ManagedStore<EngineSdk.EngineGraph.Model> restored = EngineSdk.ManagedStore.openJournal(
        EngineSdk.managedSchema(EngineSdk.EngineGraph.Model.class), path)) {
      check(restored.read().copyValue().title.equals("later authority"));
      EngineSdk.ManagedAuthority<EngineSdk.EngineGraph.Model> authority = new EngineSdk.ManagedAuthority<>(restored);
      check(java.util.Arrays.equals(authority.accept(acceptedRequest).snapshot().bytes(), acceptedBytes));
      check(authority.accept(conflictRequest).status() == EngineSdk.ManagedStatus.CONFLICT);
      restored.undo(); check(restored.read().copyValue().title.equals("authority accepted"));
      restored.redo(); check(restored.read().copyValue().title.equals("later authority"));
    }
    java.nio.file.Files.write(path, new byte[] {1, 2, 3}, java.nio.file.StandardOpenOption.APPEND);
    try (EngineSdk.ManagedStore<EngineSdk.EngineGraph.Model> restored = EngineSdk.ManagedStore.openJournal(schema, path)) {
      check(restored.read().copyValue().title.equals("later authority"));
    }
  }

  /** Prove generated storage/codecs preserve unsigned ID bits independently from managed allocation. */
  private static void unsignedStorage() {
    EngineSdk.EngineGraph.Item value = new EngineSdk.EngineGraph.Item();
    value.persistentId = @ID_CAST@0xffffffffL;
    byte[] bytes = value.encode(EngineSdk.Protocol.@PROTOCOL@);
    EngineSdk.EngineGraph.Item copy = EngineSdk.EngineGraph.Item.decode(bytes, EngineSdk.Protocol.@PROTOCOL@);
    check(identity(copy.persistentId) == 0xffffffffL);
    reject(() -> new EngineSdk.ManagedStore<>(EngineSdk.managedSchema(EngineSdk.EngineGraph.Item.class),
        copy, DOCUMENT, EngineSdk.ManagedOptions.defaults()));
  }

  /** Canonical codec clones normalize caller comparators before immutable pins and exact proposals persist. */
  private static void canonicalMapHistory(java.nio.file.Path folder) throws java.io.IOException {
    java.nio.file.Path path = java.nio.file.Files.createTempDirectory(folder, "canonical-map-").resolve("state.srjvmg");
    EngineSdk.ManagedSchema<EngineSdk.EngineGraph.Model> schema = EngineSdk.managedSchema(EngineSdk.EngineGraph.Model.class);
    byte[] expected;
    try (EngineSdk.ManagedStore<EngineSdk.EngineGraph.Model> store = new EngineSdk.ManagedStore<>(schema,
        initial(), DOCUMENT, EngineSdk.ManagedOptions.defaults())) {
      store.createJournal(path);
      store.transact(edit -> {
        java.util.NavigableMap<java.lang.Integer, EngineSdk.EngineGraph.Item> values =
            new java.util.TreeMap<>(java.util.Comparator.reverseOrder());
        values.put(7, new EngineSdk.EngineGraph.Item());
        values.put(9, new EngineSdk.EngineGraph.Item());
        edit.root().mapped = values;
      });
      EngineSdk.ManagedSnapshot<EngineSdk.EngineGraph.Model> pin = store.read();
      check(java.util.Arrays.equals(pin.bytes(), pin.copyValue().encode(EngineSdk.Protocol.@PROTOCOL@)));
      EngineSdk.ManagedClient<EngineSdk.EngineGraph.Model> client = new EngineSdk.ManagedClient<>(schema, pin, "canonical");
      EngineSdk.ManagedProposal proposal = client.propose("map", draft -> {
        java.util.NavigableMap<java.lang.Integer, EngineSdk.EngineGraph.Item> values =
            new java.util.TreeMap<>(java.util.Comparator.reverseOrder());
        values.putAll(draft.mapped); values.put(11, new EngineSdk.EngineGraph.Item());
        draft.mapped = values;
      });
      EngineSdk.ManagedReceipt<EngineSdk.EngineGraph.Model> receipt = new EngineSdk.ManagedAuthority<>(store).accept(proposal);
      client.acknowledge(receipt);
      check(client.read().copyValue().mapped.size() == 3);
      expected = client.read().bytes();
    }
    try (EngineSdk.ManagedStore<EngineSdk.EngineGraph.Model> restored = EngineSdk.ManagedStore.openJournal(schema, path)) {
      check(java.util.Arrays.equals(restored.read().bytes(), expected));
      restored.undo(); check(restored.read().copyValue().mapped.size() == 2);
      restored.redo(); check(restored.read().copyValue().mapped.size() == 3);
    }
  }

  /** Exercise one real generated profile without runtime injection or reflection into private support. */
  public static void main(java.lang.String[] args) throws java.lang.Exception {
    ownershipAndHistory(); journalAndCollaboration(java.nio.file.Path.of(args[0])); unsignedStorage();
    canonicalMapHistory(java.nio.file.Path.of(args[0]));
    java.lang.System.out.println("Generated native Java engine checks: " + checks);
  }
}
