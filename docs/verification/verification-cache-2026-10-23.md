# Rebuildable caches and graceful journal close — 23 October 2026

Compiler/runtime **2.30.0**, schema language **1.22.0**, and both editor packages
**2.0.12** implement the [cache contract](../design/cache_attributes.md).
This release also preserves the preceding 2.29.0 opt-in self-contained-document
retirement API while moving new journals into the shared companion layout.

## Configurations and evidence

Windows uses Visual Studio 18/MSVC 19.51, x64 Release, Ninja, C++20, with the
existing XML and compression providers enabled. Linux uses Ubuntu under WSL,
GNU 15, Ninja, C++20 and GoogleTest 1.18.0. The Linux configuration disables XML,
Zstandard, LZ4 and zlib; it does not qualify those optional providers. Its build
files reside on the mounted Windows workspace. Final Linux test data uses native
Linux temporary storage, separately from the mounted-filesystem run below.

The root wrappers configured these builds and packaged the extensions. Targeted
CMake builds and CTest selection then exercised the relevant native targets.
Useful build outputs and logs remain under ignored `out/build/`:

- Windows: `cache-final-windows-build.log`, `cache-final-windows-test.log`.
- Linux: `cache-linux-configure.log`, `cache-linux-build.log`, `cache-linux-test.log`.
- Native Linux execution: `cache-linux-native-test.log`; mounted-discard diagnosis:
  `cache-linux-mounted-discard.log` and `cache-linux-mounted-discard.trace`.
- Editors: `cache-editor-tests.log`, `cache-visual-studio-navigation.log`,
  `cache-visual-studio-grammar.log`, `cache-extension-build.log`.

The final Windows selection passes **13/13 CTests** in 80.76 seconds: the four
cache profiles, core serializer, managed collaboration/store/direct and both
document profiles, cache process-exit checks, existing journal crash checks and
CMake package-version compatibility. The compiler reports `2.30.0` and supported
schema language `1.22.0`.

All **eight selected Linux commands pass on native Linux storage**. The runner
reads their exact registered command arrays from CTest JSON, changes only the
working directory and crash helpers' `--directory` destinations to an isolated
`/tmp` directory, then removes that directory. All four profiles pass 17 tests
each; `managed_direct_test` passes 137 tests with one explicitly inapplicable
appended/sidecar check skipped. The two process-exit commands pass **27 cache/close
boundaries** and **480 existing journal recovery checks**. Package-version
compatibility also passes.

The separate mounted-Windows-filesystem CTest run passes **6/8 commands**.
Its four cache profiles, cache crash checks and package-version check pass, but
seven appended discard cases and the existing journal crash command fail.
An isolated `strace` captures `rename(staging, document) = -1 EACCES` on that
mount. The same discard case and complete eight-command selection pass with
native Linux test data. Consequently this record does **not** qualify journal
replacement on WSL's mounted Windows filesystem; these failures are retained,
not classified as successful tests. Use native Linux storage for Linux journals
or the native Windows runtime for Windows storage.

## Generated/runtime coverage

Each of the four generated C++ profiles combines direct/separate storage with
32/64-bit managed identities and runs the same 17 cache tests. Checks cover:

- Optional priority `0`, explicit `255`, overflow/negative/malformed options,
  language gating, an ordinary legacy type named `cache`, and unchanged implicit
  payload field IDs. Other output backends reject unsupported cache declarations.
- Private generated bindings, owning values, arrays, maps and variants;
  bounded cache codecs; defaults and ordinary payload exclusion.
- Schema priority propagation and application overrides without changing key
  identity, persistent reuse, recipe changes and corrupt-record rejection.
- Exclusive provider ownership, byte/count admission, optional write failures,
  immutable pins, invalidation, priority eviction and reclamation shortfall.
- Coalesced concurrent rebuilds, independent readiness, explicit retry,
  stale-source rejection and in-memory revalidation without a disk provider.
- Managed exact-source publication without durable/history changes; native and
  external close/reopen preserving identity/history, cache bytes and provenance.
- Released version-2/3 directory and flat migration, conflicting-layout refusal,
  old/new lock exclusion and close
  exceptions before/after durable publication.

The existing native core, managed stores, both document profiles, collaboration
and journal tests provide compatibility controls. The collaboration regression
also closes with an unsent request, reopens and publishes it exactly once.
Opt-in retirement keeps its dirty/prepared/identity/collaboration guards and now
explicitly checks that an independent cached entry survives retirement.

`cache_crash_test` terminates child processes at **27 boundaries**: five cache
publication/invalidation points and 22 native/external close points. An independent
Python reader validates cache magic, extent, priority, SHA-256 integrity and hashed
key filenames; reopening checks selected document state and retained caches.
The existing `managed_journal_crash_test` separately exercises journal publication
and preparation/recovery. Process termination is not a power-loss simulation.

## Editor checks and packages

VS Code's suite passes **160 tests**. Cache coverage includes bare/priority
syntax, snippets, compound owning map/variant types, qualified/transitive names,
selection boundaries, unsaved edits and legacy symbols named `cache`.
The Visual Studio native navigation adapter passes **128 cache checks**.
The installed Visual Studio grammar checks pass **546 checks** across all eleven
language tags; this qualifies grammar assets, not interactive editor UI behavior.

Both matching packages are rebuilt and validated under `out/extensions/`:

- `serializer-vscode-2.0.12.vsix`
- `serializer-visual-studio-2.0.12.vsix`

SHA-256 package identities:

```text
serializer-vscode-2.0.12.vsix
14402573148fea7285a16bcc9233d66dfa5420e669a753d17dc7bfe49c55c73e
serializer-visual-studio-2.0.12.vsix
ae4bdaa2b78df2129c04b25684099e3a2f842dc51bff55d256a33575066cefb7
```

Packaging does not install either extension or publish Marketplace listings.

## Boundaries

Cache declarations currently support inspectable owning C++ values. Open generic
owners, packed/raw-union/reference/native-instance graphs and other generated
languages diagnose explicitly. Applications may implement the provider interface
for their own inline/appended containers; Serializer supplies independent cache
files, not a universal adapter for arbitrary authoritative file formats.

Applications retain responsibility for rebuild recipes, external dependency
identity, authorization, scheduling and synchronization. Exact durable-source
hashes do not authenticate derived data. The supported directory provider uses
exclusive ownership; these tests do not qualify adversarial ancestor replacement,
live network filesystems, power loss or concurrent hostile filesystem mutation.

An earlier broad Windows all-SDK run was stopped after unrelated sandbox temporary
directory failures. It is not reported as a successful complete repository run.
Only the completed focused suites and editor checks above establish this record's
scope; historical verification records retain their original release metadata.
