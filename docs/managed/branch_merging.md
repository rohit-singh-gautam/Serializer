# Native C++ branch merging

Compiler/runtime 2.23.0 adds `merge_revision` to generated C++ tree stores.
Compiler/runtime 2.24.0 adds opt-in collaborative retained-branch merging, using
the existing accepted snapshot and field-delta protocol. The broader contract
is tracked in [the backlog](../pending.md#managed-state-and-storage).

```cpp
using tree_store = rohit::managed::model_store<document,
    rohit::managed::history_mode::tree>;

const auto source_revision = source_branch_revision;
const auto ancestor = store.merge_base(source_revision);
auto outcome = store.merge_revision(source_revision);
outcome.throw_if_failed();
const auto parents = store.revision_parents(outcome.revision);
```

The source must be a retained revision of this store. The current revision is
the target. The runtime computes a unique lowest common ancestor from both
parent edges. Missing ancestry and multiple merge bases reject. Merging a source
already contained in the target ancestry returns `no_change`. Otherwise a
successful merge creates a revision even when both branches reached equal
payload values: recording their ancestry is itself a durable decision.

Ordinary durable fields compare their exact native keyed codec values. Equal
changes coalesce; target-only and source-only changes survive. Generated private
fields and ordinary inherited base paths use the same typed traversal as public
fields. Identified maps merge independent keys and recurse into matching child
identities. Concurrent delete/edit and distinct identities at one key conflict.
Lists have atomic membership; matching membership permits independent edits to
identified child fields. A managed variant with the same active tag and identity
in all three states merges that child's fields. Switching tags or replacing its
identity remains atomic. Ordinary value containers and nested map wrappers retain
atomic field semantics. Ordering/move policies and text algorithms are separate work.
The [managed variant qualification](../verification/verification-managed-variants-2026-10-24.md)
covers child-field conflicts, same-tag identity replacement, journal recovery and
selective author Undo/Redo in all four Windows/Linux storage/ID profiles.

Unresolved conflicts throw `merge_conflict_error` through the outcome. Its owned
diagnostic contains the entity, complete ordinary-base scopes, field, conflict
kind, base/target/source bytes and presence flags. Map membership conflicts also
carry the encoded key. A synchronous resolver can select one complete side:

```cpp
rohit::managed::merge_options options;
options.resolve = [](const rohit::managed::merge_conflict& conflict) {
  return choose_source(conflict) ? rohit::managed::merge_resolution::source
                                 : rohit::managed::merge_resolution::target;
};
store.merge_revision(source_revision, options).throw_if_failed();
```

The callback cannot mutate or navigate this store. It runs only while preparing
private storage; recovery never replays it. Arbitrary combined resolutions can
be authored as later validated transactions. Resolver failures, identity/reference
failures, application invariants, authorization and resource rejection preserve
the published model and retained ancestry. The complete candidate is authorized
as an edit under the current host policy. Imported identities must already belong
to the retained source. Entity and revision allocation marks are never rewound.

Successful publication resets transient caches. General `exclude(history)`
overlays remain proposed syntax and gain no merge semantics from this API.
Attached store clients cannot call `merge_revision` directly. Tree authorities
use the explicitly enabled request contract below; custom command authorities
continue to reject branch-merge model requests.

The first parent is always the target and the second is the source. Undo and
explicit Redo follow the existing first-parent navigation contract. Both parents
participate in subsequent merge-base computation. `max_revisions` counts the new
node; `max_history_bytes` includes its snapshot/label and sixteen bytes for its
additional parent record. Resident admission includes the retained edge map.
`merge_options.max_work_bytes` bounds cumulative encoded comparisons and
diagnostics; this does not provide a hard process-memory ceiling. A merge stages
a copy of the revision directory; ordinary transaction hot paths retain their
existing append behavior.

Ordinary saves retain their exact existing envelope format until a merge is
recorded. Merge-aware saves use the versioned wrapper described in the
[wire contract](../serialization/wire_format.md#native-merge-ancestry).
Native full Save/load, envelope patches, explicit `load_migrated` conversions and
appended/sidecar journals retain ancestry. Merge journal records bind to the exact
first-parent envelope and optionally use a verified snapshot patch. An uncertain
write fences the store until recovery, as for other journal operations.
Standalone identity/history-policy conversion helpers do not yet negotiate this
wrapper; they reject it rather than dropping ancestry.

The generated direct/separate uint32/uint64 fixtures and both native label policies
are covered by [the native merge suite](../../test/managed_merge_test.cpp).
The current Windows/Linux runs pass all eighteen cases per storage/ID profile;
see [executed qualification and its limits](../verification/verification-collaboration-merging-2026-10-21.md).

## Collaboration

```cpp
rohit::managed::collaboration_options options;
options.allow_branch_merges = true;
using authority_type = rohit::managed::collaboration_authority<document,
    rohit::managed::history_mode::tree>;
authority_type authority{initial, epoch, document_id, options};
// Load a retained tree document before opening sessions, when needed.
authority.open_session(session);
replica.synchronize(authority.snapshot(), authority.context());
if (authority.branch_merges_enabled()) {
  auto request = replica.merge_request(session, operation,
      retained_source_revision, authority.current_revision());
  auto result = authority.submit_change(request, authenticated_session, now_ms);
  if (result->accepted) {
    replica.apply_accepted_change(*result->accepted, authenticated_authority);
  }
}
```

The host advertises the capability through its authenticated transport. It is
disabled by default, requires a tree authority using the built-in model protocol,
and selects only retained revisions of the authority's own document. Arbitrary
client snapshots, imported branch graphs and automatic stale-edit rebasing are
not part of this request. Both `base_sequence` and the target revision must match;
source ancestry is computed at the authority. An already incorporated source
returns `no_change`. Equal payloads on distinct branches still publish a merge
decision, increment the accepted cursor and retain both native parents.

The current session permissions, grants, affected-entity policy, model invariants,
identity validation, retention and merge-work budgets all precede durability.
Unresolved conflicts return `conflict`, retaining `merge_conflict_error` locally.
The host may install `set_branch_merge_resolver` to choose complete sides under
the native conflict contract. It cannot reenter the authority. Resolver failures
publish nothing. An exact retry returns its original outcome even if the host
later changes its resolver; a new decision needs a new operation ID.

Accepted changes use the existing snapshot and field/dependency deltas. Selective
session Undo/Redo reverses the merge's contributions while preserving unrelated
later edits. It creates ordinary subsequent revisions; it does not erase the
historical merge ancestry. Replicas retain accepted model state, while native
two-parent history remains authority-owned. Full Save and appended/sidecar journal
recovery retain that graph. Uncertain writes fence the authority until fresh-epoch
recovery, which never reruns the resolver or restores session rights/retry caches.
