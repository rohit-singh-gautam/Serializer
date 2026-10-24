# Compact retained C++ history

`store_options::compact_history = true` selects independent immutable byte-block
retention for linear and tree history. Four generated Windows profiles pass 24
history/allocation cases; see the [executed matrix and measurements](../verification/verification-compact-history-2026-10-24.md).
The standalone primitive also passes ten Linux GCC cases; generated Linux store
and installed-consumer qualification remain pending. MG10's broader
normalized entity-version history remains separate.
Default stores retain ordinary full snapshot vectors.

```cpp
rohit::managed::store_options options;
options.compact_history = true;
rohit::managed::model_store<generated_root> state{initial, document_id, options};
```

Each revision has its own ordered table of immutable 4 KiB blocks. Preparation
shares a same-position block only when its exact length and bytes match the
chosen preceding revision in the same resident/resource domain. It never uses a
hash match as proof of equality or retains a predecessor revision table. Every
revision independently reconstructs its complete encoded model, including nested
IDs, ownership links, collection order and deleted/restored payloads.

Undo, Redo and tree navigation materialize the selected bytes before normal native
decode, identity/reference/schema validation and publication. Save and journals
retain their existing complete snapshot formats and schema bindings. The option
is runtime storage policy, not schema syntax or a persistent compatibility flag.
Reopen/load selects it again through the destination store options.

Each physical block and table owns one shared resident admission charge. Reusing
a block never charges it again; pruning releases obsolete tables and blocks as
their final revision owner disappears. Allocations use the retained
`allocation_resource`, including pointer tables and shared control blocks. This
resource must support every thread that may release its last owner. Conservative
resident admission and actual requested-byte PMR limits remain different metrics.
The existing `max_history_bytes` limit continues to count logical complete encoded
snapshots; compaction does not silently increase the permitted undo horizon.

All tables, blocks, map/deque insertion and notification data are prepared before
journal acknowledgement. Publication retains its no-allocation swaps/removals;
a failed preparation or append drops only provisional ownership. An old immutable
root pin keeps its model and resource alive independently of history pruning.

This profile reduces repeated retained bytes; it still decodes, validates and
encodes complete transaction candidates. Fixed positional blocks can lose reuse
after an early variable-length insertion/deletion shifts subsequent bytes. It
does not normalize entity versions, compress exported history files, provide
changed-entity-proportional edit CPU, or make models/page loading lazy. These
limitations are separate from the lossless identity/Undo contract.

`managed_snapshot_test` qualifies the standalone block/table lifetime primitive.
Four `managed_compact_history_*_test` targets exercise generated direct/separate
32/64-bit models, exact default-format exports, nested identities/deletions,
branch merging, runtime fields, pruning/pins, allocator failures and journal
publication. All 24 cases pass on Windows; the standalone primitive passes ten
lifetime and allocation cases. These results retain their recorded platform scope.
The maintained `managed_history_benchmark` compares default and compact modes on
the same 64/512/4,096-entity, 32-edit workloads; see the
[measurement contract](history.md#storage-and-performance-priorities).
