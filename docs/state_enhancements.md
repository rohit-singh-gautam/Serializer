# State framework enhancements and runnable examples

Compiler/runtime 1.11 and schema language 1.6 add schema-declared behavior and
memory-only members. Managed generation remains C++; ordinary codecs target all
eleven supported languages. This page distinguishes implemented APIs from work
that still needs qualification. The integration skill requires using every
implemented feature applicable to the requested contract.

All 97 CTest checks, both 11-language behavior matrices and the document collection
examples passed. See [qualification evidence](verification-state-enhancements-2026-10-10.md)
for the tested source identity and scope.

## Reproduce the examples

```powershell
.\make.ps1 configure -Configuration Release -BuildDirectory out/build/make-Release -VcpkgRoot C:/src/vcpkg
cmake --build out/build/make-Release --config Release --parallel 3
ctest --test-dir out/build/make-Release -C Release --output-on-failure
python test/behavior_languages.py --compiler out/build/make-Release/Release/serializer.exe --language all
python test/native_behavior_languages.py --compiler out/build/make-Release/Release/serializer.exe --language all
python example/run.py --compiler out/build/make-Release/Release/serializer.exe --language all --example document_collections
```

Run CTest in a Visual C++ developer shell on Windows; set
`SERIALIZER_TEST_CLANG_FORMAT` to the installed clang-format executable.
Select only installed SDKs with `--language`; use `--wsl-languages` for SDKs in WSL.
A missing SDK is a qualification failure, not evidence of support. Examples use
exceptions and RTTI in C++; no disabling flags are required.

| Implemented feature | Runnable source | Checks |
|---|---|---|
| Method signatures, effects, native scanning/order, portable typing, wire exclusion | [behavior tests](../test/behavior_test.cpp), [schema](../test/resources/document_behavior.serializer), [SDK runner](../test/behavior_languages.py) | Rejections, resolved symbols, every generated backend, finite arithmetic |
| External/native/abstract behavior contracts | [native SDK runner](../test/native_behavior_languages.py), [schema](../test/resources/document_native_behavior.serializer) | Implementations, dispatch, missing definitions and C/C++ link checks |
| Source mapping and complete-unit formatting | [behavior tests](../test/behavior_test.cpp), [behavior guide](behavior.md) | Schema spans, postformat anchors, literal preservation and idempotence |
| Multiple managed bases; private managed fields | [document example](../example/managed/document_features/main.cpp), [schema](../example/managed/document_features/document.serializer) | Independent base IDs, generated accessors, atomic undo/redo and Save/load |
| Ordinary inherited bases | Same document example | One enclosing entity with base-prefixed field paths; conditional undo |
| Separate-value storage | [regressions](../test/managed_document_separate_test.cpp), [configuration](../test/resources/managed_document_separate.ini) | Distinct values/IDs, conversions, runtime fields and inherited identities |
| Nested collaboration field paths | [document regressions](../test/managed_document_features_test.cpp) | Scoped inherited paths, nested numeric/typed protocols and unchanged legacy flat bytes |
| Owning managed variants | Same document example and [regressions](../test/managed_document_features_test.cpp) | Active identities, replacement, undo, stale alternative handles |
| Concrete managed generics and payload lifecycle metadata | Same document example and [regressions](../test/managed_document_features_test.cpp) | Bound traits/editors, lifecycle and dotted versions; explicit legacy binding upgrades |
| Managed native/external edit methods | Same document example, [definitions](../example/managed/document_features/behavior.hpp), regressions | Guarded mutation, required definitions, rollback, undo/redo and stale/runtime rejection |
| Transient members, guarded runtime updates, ordinary copying and reset clones | [runtime example](../example/managed/runtime_state/main.cpp), [tests](../test/runtime_features_test.cpp) | Immutable pins, unchanged durable bytes/IDs/history, reset after durable edits |
| Precise changed identities, publication notifications and navigation | Same runtime example | Durable changed IDs, separate cache invalidation, immutable stamp, observer error isolation, linear/tree APIs |
| Whole-history and journal migration | Same runtime example | Explicit converter, every retained state, preserved IDs/cursor, atomic failure |
| Opt-in delta journals, snapshot patches and bounded checkpoint chains | Same runtime example | Reconstruction, baseline verification, bounded chains |
| Resident admission and scoped PMR allocation | Same runtime example and tests | Pinned roots, callback growth, rejection before journal writes, resource lifetime |
| Exact fixed arrays and transient codecs | [shared document schema](../example/schemas/document_collections/model.serializer), [runner](../example/run.py) | Nested records, integers and strings; four protocols; short/long rejection |
| Editor grammar, snippets and type navigation | [shared grammar](../editors/serializer.tmLanguage.json), editor tests | Method types, transient operands and opaque native blocks |

## Behavior belongs in the schema

```text
serializer version 1.6.0;
cpp preamble { #include <string_view> }
class document_metrics stable_ids {
  public double words_per_page (1);
  public uint32 page_count (2);
  public function double estimated_words() readonly
    expression(words_per_page * to_double(page_count));
  public function double supplied_metric(double scale) readonly
    cpp { return words_per_page * scale; }
    csharp { return words_per_page * scale; };
  private cpp { friend class document_index; }
  public transient uint64 lookup_cache {0};
}
```

The portable expression is type checked before generation. It accepts binary64
arithmetic, `math.pi` and checked `to_double` for int32/uint32. Method bodies are
not invoked by codecs or recovery. Backend bodies use that backend's actual
member names. External declarations require application definitions; abstract
methods cannot contain a body. See [behavior](behavior.md) for dispatch, target
hooks, finite-result errors and formatting boundaries. Raw native blocks do not
create persistent schema fields.

## Managed occurrences and inherited data

```text
class author_data stable_ids managed {
  private string name (1);
}
class review_data stable_ids managed {
  public string status (1) {"draft"};
}
class text_data stable_ids managed { public string text (1); }
class attachment_data stable_ids managed { public string filename (1); }
class document_data stable_ids
    : public managed author_data ("author", 1),
      public managed review_data ("review", 2) {
  public string title (3);
  public managed variant(text_data = text, attachment_data = attachment) content (4);
}
```

```cpp
rohit::managed::model_store<document_data> store{document_data{}};
store.execute_transaction([](auto& tx) {
  tx.root().author().set_name("Ada");
  tx.root().review().set_status("approved");
  tx.root().template edit_content<0>().set_text("Reviewed paragraph");
}).throw_if_failed();
store.undo();
store.redo();
auto saved = store.save();
```

Each `managed` base occurrence receives its own identity; ordinary bases instead
share the derived entity and qualify local field IDs by the base wire ID.
The compiler rejects virtual/repeated base layouts. A `managed` member activates
identity only for that occurrence: an unmarked sibling remains ordinary owned
data, even if its type can be managed.

Named managed specializations bind their own traits:

```text
class annotation<T = string> stable_ids managed { public T note (1); }
instantiate string_annotation = annotation<string>;
```

## Payload versions and explicit upgrades

```text
class versioned_document stable_ids managed {
  public version {2} compatibility {1};
  obsolete(2) public string old_title (2);
  created(2) replaced(old_title) public string title (3) {"untitled"};
}
```

`replaced` records schema evolution; it does not infer a semantic conversion.
Migrate all retained values explicitly before accepting the new fingerprint:

```cpp
target.load_migrated<old_document, old_traits>(saved, [](const auto& previous) {
  auto converted = convert_document(previous); // Caller preserves durable entity IDs.
  return converted;
});
// Recover the previous journal, convert its retained states, then create a distinct artifact.
target.recover_migrated_journal<old_document, old_traits>(
    "previous.document", "current.document", convert_document);
```

The runtime validates the converter and publishes only a complete valid result.
The runnable example models a previous fingerprint/text convention and proves
redo survives migration. Stronger managed bindings now include omission and
lifecycle metadata: old roots with omitted durable fields or reachable ordinary
versioned children may have a different fingerprint despite unchanged current
wire bytes. Use the old binding/traits and an explicit converter; strict load
does not silently alias those fingerprints. Client checkpoint/outbox and authority epoch upgrades
need their own qualified migration path; this API alone does not supply them.

## Runtime-only state and normal copying

```text
class document_data stable_ids managed {
  public string text (1);
  public transient uint64 lookup_cache {0};
}
```

```cpp
const auto before = store.save();
const auto pin = store.read();
store.update_runtime([](auto& edit) {
  edit.root().set_lookup_cache(77);
}).throw_if_failed();
// pin still sees its old cache; before == store.save(); no history step is added.
auto normal = store.clone_value(); // Cache is copied normally.
auto copied = normal;              // Ordinary C++ copy constructor is unchanged.
auto reset = store.clone_value({rohit::managed::runtime_copy_policy::reset_to_defaults});
```

Runtime updates may change only transient members. Durable edits, navigation and
recovery conservatively reset runtime fields to schema defaults. Failed or no-op
durable edits preserve published caches. A numeric cache key is non-owning;
resetting it does not destroy an application resource. `omit` remains the existing
per-format annotation and does not acquire these runtime/history semantics.

## Notifications, navigation and snapshot transfer

```cpp
store.set_publication_callback([](const auto& event) {
  refresh_document(event.after, event.stamp, event.changed_ids);
  invalidate_runtime_caches(event.invalidated_ids);
});
auto observer_failure = store.take_notification_error();
auto cursor = store.history_cursor(); // Linear store only.
auto revision = tree.current_revision(); // Tree store only.
auto patch = store.save_delta(previous_save);
receiver.load_delta(previous_save, patch);
rohit::managed::snapshot_delta_chain chain{previous_save, byte_budget, 8};
chain.append(store.save());
```

Observer failures are separate from durable transaction outcomes. Reentrant
publication is rejected. Generated traits supply precise `changed_ids` for durable
fields, membership, births/deletions and type changes. Child-only edits identify
the child; ownership edits also identify the owner. `invalidated_ids` separately
contains the conservative old/new entity union for whole-store cache reset.
Runtime updates have no durable changed IDs; handwritten legacy traits lacking
field traversal disclose `changed_ids_precise = false`. Delta APIs currently
transfer a complete Save envelope through a verified patch; integrating deltas
into retained history and collaboration is separate work. Opt-in journals use
the same verified patch mechanism, as shown below.

## Bounded delta journals

```cpp
rohit::managed::store_options options;
options.delta_journal = true;
options.max_delta_chain = 8;
store_type store{draft, document_id, options};
store.create_journal("document.srj");
// Normal transactions append a verified delta only when it is smaller than a full frame.
store.save_journal(); // Full checkpoint retains history and resets replay depth.
```

Default journal records remain byte compatible. Opt-in delta frames use new tags;
older runtimes reject those records. A full frame appears at the configured chain
bound. Recovery validates base/target hashes and length/work limits before
publication. Attached collaboration journals retain their existing full frames.
This optimizes journal bytes; retained history still stores snapshots.

## Resident admission and scoped allocation

```cpp
rohit::managed::store_options options;
options.max_resident_bytes = 32 * 1024 * 1024;
options.allocation_resource = std::make_shared<std::pmr::synchronized_pool_resource>();
store_type store{draft, document_id, options};
auto older_view = store.read();
auto charged = store.resident_bytes();
// Old model charges remain until the last immutable pin is released.
older_view.reset();
```

Generated traits estimate owning container capacities, models and runtime fields;
the store also admits history/snapshot/identity metadata and outstanding model
pins. Growth is checked again before acceptance. Finite budgets require complete
model memory traits; failures precede journal append/publication. The optional
shared PMR hook controls root/control-block allocations and outlives weak pins.

This is conservative **admission accounting**, not a hard process peak limit.
Native blocks with extra heap/resource ownership need a custom `Traits` estimate;
opaque native code cannot supply automatic allocation metadata. Generated
containers, decoder temporaries, user callback allocations, authority
retention and client outbox metadata keep their existing allocators/limits. A hard
allocator-wide ceiling needs allocator-aware containers and decoding.

## Fixed arrays across ordinary backends

```text
class page_data stable_ids { public string title (1); }
class document_data stable_ids {
  public array[2] page_data pages (1);
  public array[3] uint32 columns (2);
  public array[2] string headings (3);
}
```

The arrays retain the normal sequence count on the wire. Constructors create
independent owning defaults; encoders require exact length; binary readers check
the count before payload allocation; JSON readers reject an excess element
before reading its payload. Direct Protobuf fixed-array output still rejects.
Nonempty array initializers remain C++ only. JS/TS expose normal arrays with
runtime extent checks rather than fixed-length tuple types.

## Remaining work

Native managed engines outside C++, complete retained
history/collaboration delta integration, hard peak allocation hooks,
collaboration checkpoint/outbox migrations, conditional packed/view/recursive/
Protobuf schemas, schema feature selectors/history-policy conversions and database
journal adapters remain
separate implementation/qualification items. This page must be updated as those
items land; generated codecs or standalone patch APIs do not prove them.

Managed variant collaboration currently merges/replaces the alternative as an
atomic value; field-level merging inside an alternative remains future work.
Alternative handles are conservatively invalidated by a whole-value replacement.
Managed edit methods support ordinary dispatch; virtual/abstract/override edit
contracts are rejected. Ordinary behavior interfaces remain available. Method parameters/results use
existing backend ownership and error conventions; new portable owned/borrowed
annotations or typed-error contracts are not implemented.
