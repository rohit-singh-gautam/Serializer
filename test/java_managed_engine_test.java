// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// The standalone runner inserts the runtime template at the marked boundary before javac.
final class JavaManagedEngineTest {
  /*SERIALIZER_MANAGED_ENGINE_RUNTIME*/

  private static int checks;
  private static final ManagedDocument DOCUMENT = new ManagedDocument(17, 18);

  /** Assert contracts regardless of whether Java assertions are enabled. */
  private static void check(boolean value) {
    ++checks;
    if (!value) throw new java.lang.AssertionError("Native Java managed check " + checks);
  }

  /** Require an explicit native contract rejection rather than accidental silent success. */
  private static void reject(java.lang.Runnable callback) {
    try { callback.run(); }
    catch (ManagedError expected) { ++checks; return; }
    throw new java.lang.AssertionError("Expected a native Java managed rejection");
  }

  /** Finite independent ordinary models mirror graph/branch/leaf generated storage, without runtime business names. */
  private static final class WireLeaf {
    long id;
    java.lang.String text = "";
    long number;
  }
  private static final class WireBranch {
    long id;
    final java.util.ArrayList<WireLeaf> leaves = new java.util.ArrayList<>();
  }
  private static final class WireGraph {
    long id;
    java.lang.String text = "";
    final java.util.ArrayList<WireBranch> branches = new java.util.ArrayList<>();
    WireBranch ordinary = new WireBranch();
    ManagedToken selected;
  }

  /** Emit ordinary fixture strings independently of engine metadata encoding. */
  private static void fixtureText(java.io.DataOutputStream output, java.lang.String text) throws java.io.IOException {
    byte[] bytes = text.getBytes(java.nio.charset.StandardCharsets.UTF_8);
    output.writeInt(bytes.length); output.write(bytes);
  }
  /** Decode bounded ordinary fixture strings; empty values are ordinary valid payloads. */
  private static java.lang.String fixtureText(java.io.DataInputStream input) throws java.io.IOException {
    int size = input.readInt();
    if (size < 0 || size > 1024 * 1024 || size > input.available()) throw new ManagedError("Fixture string exceeds bound");
    byte[] data = new byte[size]; input.readFully(data);
    return java.nio.charset.StandardCharsets.UTF_8.newDecoder()
        .onMalformedInput(java.nio.charset.CodingErrorAction.REPORT).decode(java.nio.ByteBuffer.wrap(data)).toString();
  }
  /** Encode one concrete branch and its finite owned leaf values. */
  private static void fixtureBranch(java.io.DataOutputStream output, WireBranch branch) throws java.io.IOException {
    output.writeLong(branch.id); output.writeInt(branch.leaves.size());
    for (WireLeaf leaf : branch.leaves) {
      output.writeLong(leaf.id); fixtureText(output, leaf.text); output.writeLong(leaf.number);
    }
  }
  /** Decode fresh branch/leaf objects, with no static cache or accepted graph alias. */
  private static WireBranch fixtureBranch(java.io.DataInputStream input) throws java.io.IOException {
    WireBranch branch = new WireBranch();
    branch.id = input.readLong();
    int count = input.readInt();
    if (count < 0 || count > 10000) throw new ManagedError("Fixture leaf count exceeds bound");
    for (int index = 0; index < count; ++index) {
      WireLeaf leaf = new WireLeaf();
      leaf.id = input.readLong(); leaf.text = fixtureText(input); leaf.number = input.readLong();
      branch.leaves.add(leaf);
    }
    return branch;
  }

  /** Generic codec/metadata provider uses the same methods that a generated concrete descriptor would implement. */
  private static ManagedSchema<WireGraph> schema(int idBytes) {
    return schema(idBytes, "test.native.java.v1", 1024 * 1024, 10000);
  }
  /** Allow independent malformed binding/resource tests without changing runtime implementation logic. */
  private static ManagedSchema<WireGraph> schema(int idBytes, java.lang.String binding, int bytes, int entities) {
    return new ManagedSchema<>(WireGraph.class, binding, idBytes, bytes, entities, new ManagedCodec<>() {
      /** Encode fixture records with an independent ordinary exact binary codec. */
      @Override public byte[] encode(WireGraph value) {
        try {
          java.io.ByteArrayOutputStream bytes = new java.io.ByteArrayOutputStream();
          java.io.DataOutputStream output = new java.io.DataOutputStream(bytes);
          output.writeLong(value.id); fixtureText(output, value.text); output.writeInt(value.branches.size());
          for (WireBranch branch : value.branches) fixtureBranch(output, branch);
          fixtureBranch(output, value.ordinary);
          output.writeByte(value.selected == null ? 0 : 1);
          if (value.selected != null) {
            output.writeLong(value.selected.document().high()); output.writeLong(value.selected.document().low());
            output.writeLong(value.selected.classId()); output.writeLong(value.selected.objectId());
          }
          output.flush(); return bytes.toByteArray();
        } catch (java.io.IOException error) { throw new ManagedError("Fixture encode failed", error); }
      }
      /** Decode into caller-owned fresh objects and require exact-message consumption. */
      @Override public WireGraph decode(byte[] data) {
        try {
          java.io.DataInputStream input = new java.io.DataInputStream(new java.io.ByteArrayInputStream(data));
          WireGraph value = new WireGraph(); value.id = input.readLong(); value.text = fixtureText(input);
          int count = input.readInt();
          if (count < 0 || count > 10000) throw new ManagedError("Fixture branch count exceeds bound");
          for (int index = 0; index < count; ++index) value.branches.add(fixtureBranch(input));
          value.ordinary = fixtureBranch(input);
          int optional = input.readUnsignedByte();
          if (optional == 1) value.selected = new ManagedToken(new ManagedDocument(input.readLong(), input.readLong()),
              input.readLong(), input.readLong());
          else if (optional != 0) throw new ManagedError("Fixture optional marker is not canonical");
          if (input.available() != 0) throw new ManagedError("Fixture trailing bytes");
          return value;
        } catch (java.io.IOException error) { throw new ManagedError("Fixture decode failed", error); }
      }
      /** Ordinary ancestor boundaries suppress independently managed grandchildren without relying on object addresses. */
      @Override public void visit(WireGraph value, ManagedVisitor visitor) {
        visitor.entity(value, 703, () -> value.id, id -> value.id = id, true);
        for (WireBranch branch : value.branches) visitBranch(branch, visitor, true);
        visitBranch(value.ordinary, visitor, false);
        if (value.selected != null) visitor.reference(value.selected.document(), value.selected.classId(), value.selected.objectId(), 701);
      }
      /** Field descriptors preserve actual managed occurrence participation instead of class eligibility alone. */
      private void visitBranch(WireBranch branch, ManagedVisitor visitor, boolean managed) {
        visitor.entity(branch, 702, () -> branch.id, id -> branch.id = id, managed);
        for (WireLeaf leaf : branch.leaves) {
          visitor.entity(leaf, 701, () -> leaf.id, id -> leaf.id = id, managed);
        }
      }
    });
  }

  /** Construct only ordinary model values; the store owns and identifies an independent codec clone. */
  private static WireGraph initial() {
    WireGraph value = new WireGraph(); value.text = "baseline";
    WireBranch branch = new WireBranch();
    for (int index = 0; index < 2; ++index) {
      WireLeaf leaf = new WireLeaf(); leaf.text = "leaf-" + index; leaf.number = index;
      branch.leaves.add(leaf);
    }
    value.branches.add(branch);
    value.ordinary.leaves.add(new WireLeaf());
    return value;
  }

  /** Scope each native fixture beneath a unique task-created directory. */
  private static java.nio.file.Path journalPath(java.nio.file.Path folder, java.lang.String label) throws java.io.IOException {
    return java.nio.file.Files.createTempDirectory(folder, label + "-").resolve("state.srjvmg");
  }

  /** Source aliases, copied arrays, escaped drafts and cross-thread read copies never mutate accepted state. */
  private static void ownership(int width) throws java.lang.InterruptedException {
    ManagedSchema<WireGraph> contract = schema(width);
    WireGraph input = initial();
    try (ManagedStore<WireGraph> store = new ManagedStore<>(contract, input, DOCUMENT, ManagedOptions.defaults(),
        edit -> edit.root().selected = edit.token(edit.root().branches.get(0).leaves.get(0)))) {
      input.text = "foreign"; input.branches.get(0).leaves.get(0).text = "foreign";
      ManagedSnapshot<WireGraph> before = store.read();
      WireGraph detached = before.copyValue(); detached.text = "detached";
      byte[] bytes = before.bytes(); bytes[0] ^= 1;
      check(before.copyValue().text.equals("baseline"));
      check(before.copyValue().branches.get(0).leaves.get(0).text.equals("leaf-0"));
      check(before.copyValue().id == 1 && before.copyValue().branches.get(0).id == 2);
      check(before.copyValue().branches.get(0).leaves.get(0).id == 3);
      check(before.copyValue().ordinary.id == 0 && before.copyValue().ordinary.leaves.get(0).id == 0);
      check(before.resolve(before.copyValue().selected, WireLeaf.class).id == 3);
      reject(() -> before.resolve(new ManagedToken(new ManagedDocument(19, 20), 701, 3), WireLeaf.class));
      reject(() -> before.resolve(new ManagedToken(DOCUMENT, 702, 3), WireLeaf.class));
      java.util.ArrayList<ManagedEdit<WireGraph>> escaped = new java.util.ArrayList<>();
      java.util.ArrayList<WireGraph> drafts = new java.util.ArrayList<>();
      store.transact(edit -> { escaped.add(edit); drafts.add(edit.root()); edit.root().text = "accepted"; });
      drafts.get(0).text = "late";
      reject(() -> escaped.get(0).root());
      check(store.read().copyValue().text.equals("accepted") && before.copyValue().text.equals("baseline"));
      java.util.ArrayList<java.lang.String> results = new java.util.ArrayList<>();
      java.lang.Thread other = new java.lang.Thread(() -> {
        results.add(store.read().copyValue().text);
        try { store.transact(edit -> edit.root().text = "wrong thread"); }
        catch (ManagedError expected) { results.add("confined"); }
      });
      other.start(); other.join();
      check(results.equals(java.util.List.of("accepted", "confined")));
    }
  }

  /** Callback failures, caught nesting and identity forgery leave the exact accepted pin unchanged. */
  private static void transactions(int width) {
    try (ManagedStore<WireGraph> store = new ManagedStore<>(schema(width), initial(), DOCUMENT, ManagedOptions.defaults())) {
      ManagedSnapshot<WireGraph> before = store.read();
      reject(() -> store.transact(edit -> { edit.root().text = "partial"; throw new ManagedError("abort"); }));
      check(store.read() == before);
      reject(() -> store.transact(edit -> { edit.root().text = "partial"; reject(() -> store.transact(child -> {})); }));
      check(store.read() == before);
      reject(() -> store.transact(edit -> { reject(store::close); edit.root().text = "partial"; }));
      check(store.read() == before);
      check(store.transact(edit -> {}) == before);
      reject(() -> store.transact(edit -> edit.root().id++));
      reject(() -> store.transact(edit -> edit.root().ordinary.leaves.get(0).id = 7));
      reject(() -> store.transact(edit -> edit.root().branches.get(0).leaves.add(edit.root().branches.get(0).leaves.get(0))));
      check(store.read() == before);
      reject(() -> store.transact(edit -> edit.root().selected = new ManagedToken(DOCUMENT, 702, 3)));
      check(store.read() == before);
      WireGraph changedRoot = before.copyValue(); changedRoot.id = 0;
      ManagedProposal replaced = new ManagedProposal("root", "replace", DOCUMENT, store.schema.schemaId,
          before.epoch(), store.schema.encode(changedRoot));
      reject(() -> new ManagedAuthority<>(store).accept(replaced));
      check(store.read() == before);
    }
  }

  /** Undo preserves lifetimes, exposed failed reservations burn IDs and unsigned maximum IDs never wrap. */
  private static void identities(int width) {
    try (ManagedStore<WireGraph> store = new ManagedStore<>(schema(width), initial(), DOCUMENT, ManagedOptions.defaults())) {
      java.util.ArrayList<java.lang.Long> observed = new java.util.ArrayList<>();
      reject(() -> store.transact(edit -> edit.root().branches.get(0).leaves.add(new WireLeaf()), edit -> {
        long id = edit.token(edit.root().branches.get(0).leaves.get(2)).objectId(); observed.add(id);
        throw new ManagedError("abort after identity exposure");
      }));
      check(store.allocatedId() == observed.get(0));
      store.transact(edit -> edit.root().branches.get(0).leaves.add(new WireLeaf()), edit ->
          edit.root().selected = edit.token(edit.root().branches.get(0).leaves.get(2)));
      long first = store.read().copyValue().selected.objectId();
      check(java.lang.Long.compareUnsigned(first, observed.get(0)) > 0);
      ManagedSnapshot<WireGraph> pin = store.read();
      store.undo(); check(store.read().copyValue().branches.get(0).leaves.size() == 2);
      store.redo(); check(store.read().copyValue().selected.objectId() == first);
      store.undo();
      store.transact(edit -> edit.root().branches.get(0).leaves.add(new WireLeaf()));
      check(java.lang.Long.compareUnsigned(store.read().copyValue().branches.get(0).leaves.get(2).id, first) > 0);
      reject(store::redo);
      check(pin.resolve(pin.copyValue().selected, WireLeaf.class).id == first);
    }
    try (ManagedStore<WireGraph> store = new ManagedStore<>(schema(width), initial(), DOCUMENT, ManagedOptions.defaults())) {
      long maximum = width == 4 ? 0xffffffffL : -1L;
      ManagedState<WireGraph> old = store.state;
      store.state = new ManagedState<>(maximum - 1, old.epoch, old.revision, old.history, old.cursor, old.current, old.receipts);
      store.transact(edit -> edit.root().branches.get(0).leaves.add(new WireLeaf()), edit ->
          edit.root().selected = edit.token(edit.root().branches.get(0).leaves.get(2)));
      check(store.allocatedId() == maximum && store.read().copyValue().selected.objectId() == maximum);
      ManagedSnapshot<WireGraph> before = store.read();
      reject(() -> store.transact(edit -> edit.root().branches.get(0).leaves.add(new WireLeaf())));
      check(store.read() == before && store.allocatedId() == maximum);
    }
  }

  /** History eviction and codec/descriptor admission are independent of an unrestricted JVM allocator. */
  private static void bounds(int width) {
    ManagedOptions defaults = ManagedOptions.defaults();
    ManagedOptions limited = new ManagedOptions(2, defaults.maxHistoryBytes(), defaults.maxReceipts(),
        defaults.maxReceiptBytes(), defaults.maxFrameBytes(), defaults.maxJournalBytes(), defaults.maxJournalRecords());
    try (ManagedStore<WireGraph> store = new ManagedStore<>(schema(width), initial(), DOCUMENT, limited)) {
      for (java.lang.String title : java.util.List.of("one", "two", "three")) store.transact(edit -> edit.root().text = title);
      store.undo(); check(store.read().copyValue().text.equals("two")); reject(store::undo);
    }
    reject(() -> new ManagedStore<>(schema(width, "small.bytes", 1, 10000), initial(), DOCUMENT, defaults));
    reject(() -> new ManagedStore<>(schema(width, "small.entities", 1024 * 1024, 2), initial(), DOCUMENT, defaults));
    reject(() -> new ManagedDocument(0, 0));
    reject(() -> checkedText("\ud800", 256, "request"));
  }

  /** Native frames recover history and exact accepted/conflict outcomes across a fresh equivalent descriptor. */
  private static void journalsAndAuthority(java.nio.file.Path folder, int width) throws java.io.IOException {
    java.nio.file.Path path = journalPath(folder, "authority-" + width);
    ManagedSchema<WireGraph> contract = schema(width);
    ManagedProposal acceptedRequest;
    ManagedProposal conflictRequest;
    ManagedReceipt<WireGraph> accepted;
    ManagedReceipt<WireGraph> conflict;
    byte[] expected;
    try (ManagedStore<WireGraph> store = new ManagedStore<>(contract, initial(), DOCUMENT, ManagedOptions.defaults())) {
      store.createJournal(path);
      reject(() -> ManagedStore.openJournal(contract, path));
      ManagedClient<WireGraph> first = new ManagedClient<>(schema(width), store.read(), "one");
      ManagedClient<WireGraph> stale = new ManagedClient<>(contract, store.read(), "two");
      java.util.ArrayList<java.lang.String> calls = new java.util.ArrayList<>();
      acceptedRequest = first.propose("request", draft -> { calls.add("once"); draft.text = "accepted"; });
      check(first.retry() == acceptedRequest && first.read().copyValue().text.equals("baseline"));
      ManagedAuthority<WireGraph> authority = new ManagedAuthority<>(store);
      accepted = authority.accept(acceptedRequest);
      check(authority.accept(first.retry()) == accepted);
      first.acknowledge(accepted);
      check(first.read().copyValue().text.equals("accepted") && calls.equals(java.util.List.of("once")));
      conflictRequest = stale.propose("request", draft -> draft.text = "stale");
      conflict = authority.accept(conflictRequest);
      check(conflict.status() == ManagedStatus.CONFLICT);
      stale.acknowledge(conflict); check(stale.read().copyValue().text.equals("accepted"));
      ManagedProposal changed = new ManagedProposal(acceptedRequest.clientId, acceptedRequest.requestId,
          acceptedRequest.document, acceptedRequest.schemaId, acceptedRequest.baseEpoch, new byte[] {1});
      reject(() -> authority.accept(changed));
      expected = store.read().bytes();
    }
    try (ManagedStore<WireGraph> recovered = ManagedStore.openJournal(schema(width), path)) {
      check(java.util.Arrays.equals(recovered.read().bytes(), expected));
      ManagedAuthority<WireGraph> authority = new ManagedAuthority<>(recovered);
      check(sameReceipt(authority.accept(acceptedRequest), accepted));
      check(sameReceipt(authority.accept(conflictRequest), conflict));
      recovered.undo(); check(recovered.read().copyValue().text.equals("baseline"));
      recovered.redo(); check(recovered.read().copyValue().text.equals("accepted"));
    }
  }

  /** Header/payload corruption rejects unchanged; repair waits for complete schema/identity semantic validation. */
  private static void recovery(java.nio.file.Path folder, int width) throws java.io.IOException {
    java.nio.file.Path path = journalPath(folder, "repair-" + width);
    ManagedSchema<WireGraph> contract = schema(width);
    try (ManagedStore<WireGraph> store = new ManagedStore<>(contract, initial(), DOCUMENT, ManagedOptions.defaults())) {
      store.createJournal(path); store.transact(edit -> edit.root().text = "durable");
    }
    byte[] complete = java.nio.file.Files.readAllBytes(path);
    byte[] tail = java.util.Arrays.copyOf(complete, complete.length + 3);
    tail[complete.length] = 1;
    java.nio.file.Files.write(path, tail);
    reject(() -> ManagedStore.openJournal(schema(width, "wrong.schema", 1024 * 1024, 10000), path));
    check(java.util.Arrays.equals(java.nio.file.Files.readAllBytes(path), tail));
    try (ManagedStore<WireGraph> restored = ManagedStore.openJournal(contract, path)) {
      check(restored.read().copyValue().text.equals("durable"));
    }
    check(java.util.Arrays.equals(java.nio.file.Files.readAllBytes(path), complete));
    for (int location : new int[] {ManagedJournal.MAGIC.length, complete.length - 1}) {
      byte[] corrupt = complete.clone(); corrupt[location] ^= 64;
      java.nio.file.Files.write(path, corrupt);
      reject(() -> ManagedStore.openJournal(contract, path));
      check(java.util.Arrays.equals(java.nio.file.Files.readAllBytes(path), corrupt));
    }
    java.nio.file.Files.write(path, complete);
    java.util.ArrayList<ManagedJournalEvent> events = new java.util.ArrayList<>();
    try (ManagedStore<WireGraph> restored = recoverManagedStore(contract, path, ManagedOptions.defaults(), events::add)) {
      check(restored.read().copyValue().text.equals("durable"));
    }
    check(events.equals(java.util.List.of(ManagedJournalEvent.RECOVERY_BEFORE_FORCE, ManagedJournalEvent.RECOVERY_AFTER_FORCE)));
    reject(() -> recoverManagedStore(contract, path, ManagedOptions.defaults(), event -> {
      if (event == ManagedJournalEvent.RECOVERY_BEFORE_FORCE) throw new ManagedError("Injected recovery force failure");
    }));
    try (ManagedStore<WireGraph> restored = ManagedStore.openJournal(contract, path)) {
      check(restored.read().copyValue().text.equals("durable"));
    }
  }

  /** Post-force interruption fences mutation while reopening adopts the complete durable state. */
  private static void uncertain(java.nio.file.Path folder, int width) throws java.io.IOException {
    java.nio.file.Path path = journalPath(folder, "uncertain-" + width);
    ManagedSchema<WireGraph> contract = schema(width);
    try (ManagedStore<WireGraph> store = new ManagedStore<>(contract, initial(), DOCUMENT, ManagedOptions.defaults())) {
      store.createJournal(path);
      ManagedSnapshot<WireGraph> before = store.read();
      store.journal.observer = event -> {
        if (event == ManagedJournalEvent.AFTER_FORCE) throw new ManagedError("Injected durable publication interruption");
      };
      reject(() -> store.transact(edit -> edit.root().text = "durable but unacknowledged"));
      check(store.read() == before);
      reject(() -> store.transact(edit -> {}));
    }
    try (ManagedStore<WireGraph> restored = ManagedStore.openJournal(contract, path)) {
      check(restored.read().copyValue().text.equals("durable but unacknowledged"));
    }
  }

  /** A complete valid-checksum acceptance may not replace the root, even after a matching ID reservation. */
  private static void rootRecovery(java.nio.file.Path folder, int width) throws java.io.IOException {
    java.nio.file.Path path = journalPath(folder, "root-recovery-" + width);
    ManagedSchema<WireGraph> contract = schema(width);
    try (ManagedStore<WireGraph> store = new ManagedStore<>(contract, initial(), DOCUMENT, ManagedOptions.defaults())) {
      store.createJournal(path);
      ManagedState<WireGraph> old = store.state;
      long nextId = old.highwater + 1;
      ManagedState<WireGraph> reserved = new ManagedState<>(nextId, old.epoch, old.revision,
          old.history, old.cursor, old.current, old.receipts);
      store.journal.append(ManagedOperation.RESERVE, encodeManagedState(store, reserved));
      WireGraph changed = old.current.copyValue(); changed.id = nextId;
      ManagedSnapshot<WireGraph> replaced = new ManagedSnapshot<>(contract, DOCUMENT, 1, 1, contract.encode(changed));
      ManagedState<WireGraph> malicious = new ManagedState<>(nextId, 1, 1, java.util.List.of(old.current, replaced),
          1, replaced, old.receipts);
      store.journal.append(ManagedOperation.ACCEPT, encodeManagedState(store, malicious));
      check(store.read() == old.current);
    }
    java.nio.file.Files.write(path, new byte[] {1, 2, 3}, java.nio.file.StandardOpenOption.APPEND);
    byte[] original = java.nio.file.Files.readAllBytes(path);
    reject(() -> ManagedStore.openJournal(contract, path));
    check(java.util.Arrays.equals(java.nio.file.Files.readAllBytes(path), original));
  }

  /** Complete frame/record admission failure precedes writes and does not fence a proven local state. */
  private static void journalCapacity(java.nio.file.Path folder, int width) throws java.io.IOException {
    ManagedOptions defaults = ManagedOptions.defaults();
    ManagedOptions limits = new ManagedOptions(defaults.maxHistoryEntries(), defaults.maxHistoryBytes(),
        defaults.maxReceipts(), defaults.maxReceiptBytes(), defaults.maxFrameBytes(), defaults.maxJournalBytes(), 1);
    java.nio.file.Path path = journalPath(folder, "capacity-" + width);
    try (ManagedStore<WireGraph> store = new ManagedStore<>(schema(width), initial(), DOCUMENT, limits)) {
      store.createJournal(path);
      long originalSize = store.journal.file.size();
      ManagedSnapshot<WireGraph> before = store.read();
      reject(() -> store.transact(edit -> edit.root().text = "too many records"));
      check(store.read() == before && store.transact(edit -> {}) == before);
      check(store.journal.file.size() == originalSize);
    }
  }

  /** Actual native-object locks exclude distinct hard-link names and release every rejected-open handle. */
  private static void writerAliases(java.nio.file.Path folder, int width) throws java.io.IOException {
    java.nio.file.Path path = journalPath(folder, "writer-alias-" + width);
    java.nio.file.Path alias = path.resolveSibling("alias.srjvmg");
    ManagedSchema<WireGraph> contract = schema(width);
    try (ManagedStore<WireGraph> store = new ManagedStore<>(contract, initial(), DOCUMENT, ManagedOptions.defaults())) {
      store.createJournal(path);
      java.nio.file.Files.createLink(alias, path);
      reject(() -> ManagedStore.openJournal(contract, alias));
      store.transact(edit -> edit.root().text = "single native writer");
    }
    try (ManagedStore<WireGraph> restored = ManagedStore.openJournal(contract, alias)) {
      check(restored.read().copyValue().text.equals("single native writer"));
      reject(() -> ManagedStore.openJournal(contract, path));
    }
    try (ManagedStore<WireGraph> restored = ManagedStore.openJournal(contract, path)) {
      check(restored.read().copyValue().text.equals("single native writer"));
    }
  }

  /** Malformed/backwards acknowledgments retain the exact original pending request for retry. */
  private static void clientErrors(int width) {
    ManagedSchema<WireGraph> contract = schema(width);
    try (ManagedStore<WireGraph> store = new ManagedStore<>(contract, initial(), DOCUMENT, ManagedOptions.defaults())) {
      ManagedClient<WireGraph> client = new ManagedClient<>(contract, store.read(), "client");
      reject(() -> client.propose("nested", draft -> reject(() -> client.propose("inner", child -> {}))));
      reject(client::retry);
      reject(() -> client.propose("ordinary", draft -> draft.ordinary.id = 7));
      reject(() -> client.propose("wrong-document", draft -> draft.selected =
          new ManagedToken(new ManagedDocument(3, 4), 701, draft.branches.get(0).leaves.get(0).id)));
      reject(client::retry);
      ManagedProposal proposal = client.propose("request", draft -> draft.text = "accepted");
      ManagedReceipt<WireGraph> receipt = new ManagedAuthority<>(store).accept(proposal);
      ManagedSnapshot<WireGraph> malformed = new ManagedSnapshot<>(contract, DOCUMENT, receipt.snapshot.epoch + 1,
          receipt.snapshot.revision, receipt.snapshot.data);
      reject(() -> client.acknowledge(new ManagedReceipt<>(proposal, ManagedStatus.ACCEPTED, malformed)));
      check(client.retry() == proposal);
      WireGraph replaced = receipt.snapshot.copyValue(); replaced.id = 999;
      ManagedSnapshot<WireGraph> changedRoot = new ManagedSnapshot<>(contract, DOCUMENT, receipt.snapshot.epoch,
          receipt.snapshot.revision, contract.encode(replaced));
      reject(() -> client.acknowledge(new ManagedReceipt<>(proposal, ManagedStatus.ACCEPTED, changedRoot)));
      check(client.retry() == proposal);
      client.acknowledge(receipt);
      check(client.read().copyValue().text.equals("accepted"));
    }
  }

  /** Seed/process-crash/recovery modes run in independent JVMs so OS writer-lock release is exercised. */
  public static void main(java.lang.String[] args) throws java.lang.Exception {
    if (args.length > 0 && args[0].startsWith("--")) {
      java.nio.file.Path path = java.nio.file.Path.of(args[1]);
      ManagedSchema<WireGraph> contract = schema(4);
      if (args[0].equals("--seed")) {
        try (ManagedStore<WireGraph> store = new ManagedStore<>(contract, initial(), DOCUMENT, ManagedOptions.defaults())) {
          store.createJournal(path); check(store.read().copyValue().text.equals("baseline"));
        }
      } else if (args[0].equals("--hold-writer")) {
        try (ManagedStore<WireGraph> store = ManagedStore.openJournal(contract, path)) {
          check(store.read().copyValue().text.equals("baseline"));
          java.nio.file.Files.writeString(java.nio.file.Path.of(args[2]), "ready");
          java.lang.System.in.read();
        }
      } else if (args[0].equals("--writer-locked")) {
        reject(() -> ManagedStore.openJournal(contract, path));
      } else if (args[0].equals("--crash")) {
        ManagedJournalEvent boundary = ManagedJournalEvent.valueOf(args[2]);
        try (ManagedStore<WireGraph> store = ManagedStore.openJournal(contract, path)) {
          store.journal.observer = event -> { if (event == boundary) java.lang.Runtime.getRuntime().halt(75); };
          store.transact(edit -> edit.root().text = "crash accepted");
        }
        throw new java.lang.AssertionError("Crash boundary was not observed");
      } else if (args[0].equals("--verify-crash")) {
        ManagedJournalEvent boundary = ManagedJournalEvent.valueOf(args[2]);
        boolean accepted = boundary == ManagedJournalEvent.AFTER_CHECKSUM || boundary == ManagedJournalEvent.AFTER_FORCE;
        try (ManagedStore<WireGraph> store = ManagedStore.openJournal(contract, path)) {
          check(store.read().copyValue().text.equals(accepted ? "crash accepted" : "baseline"));
          store.transact(edit -> edit.root().text = "reopened writer");
          check(store.read().copyValue().text.equals("reopened writer"));
        }
      } else throw new java.lang.AssertionError("Unknown fixture process mode");
      return;
    }
    java.nio.file.Path folder = java.nio.file.Path.of(args[0]);
    for (int width : new int[] {4, 8}) {
      ownership(width); transactions(width); identities(width); bounds(width);
      journalsAndAuthority(folder, width); recovery(folder, width); uncertain(folder, width);
      rootRecovery(folder, width); journalCapacity(folder, width); writerAliases(folder, width); clientErrors(width);
    }
    java.lang.System.out.println("Standalone native Java engine checks: " + checks + "; two identity widths");
  }
}
