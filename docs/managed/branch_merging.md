# Native C++ branch merging

Compiler/runtime 2.23.0 adds `merge_revision` to generated C++ tree stores.
This is the native retained-history portion of **MG04**. Collaboration merge
negotiation and accepted-delta integration remain pending. The broader contract
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
Attached collaboration sessions and authority acceptance hooks reject branch
merging until the collaboration protocol has its own negotiated merge contract.

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

The generated direct/separate uint32/uint64 fixtures and both label policies are
covered by [the native merge suite](../../test/managed_merge_test.cpp). Initial
Windows checks passed before the additional malformed/fault cases were added.
Final qualification is deferred until the requested implementation/compilation
commit; no current all-platform qualification is claimed.
