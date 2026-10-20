# Go ZIP and OPC packages

The Go generator provides dependency-free physical ZIP helpers alongside normal
generated models and emits typed OPC facades for schema `package` declarations.
It uses Go 1.22 language/library APIs. Physical ZIP indexing and OPC validation
are separate entry points. Package declarations enable XML; explicitly disabling
XML rejects before an existing generated output is replaced.

The helper family normally starts with `SerializerPackage`. The generator chooses
one available suffix, such as `SerializerPackage_3`, when an existing generated
declaration already uses a family name. The generated comment records the selected
prefix. Existing model names and independently selected compression names stay
unchanged. Private helper names use the compiler's existing reserved `srl` family.
OPC support is emitted only when the schema declares a package. The original ZIP
rollout's recorded before/after comparison preserved ordinary Go and the other
ten ordinary language outputs. Later additive XML operation APIs have their own
compatibility qualification.

## Typed OPC facades

A schema package such as `profile::archive` generates `ProfileArchive`, its
`Input`/`Value` aliases, `LoadOptions`/`SaveOptions`, and `Encode`, `Load`, `Write`
and `Save` functions. `Load` constructs an independent fresh aggregate and returns
`nil` on any error, including failure in a middle repeated part. Ordinary model
binary and XML APIs remain available.

Release 2.18.0 adds selected-codec functions under a separate collision-selected
prefix, recorded by `Generated Go selected-codec prefix`. Its usual form is
`SerializerPackageCodecsProfileArchive`. `Encode(value, profiles, budget, ...)`
and `Load(source, loadOptions, ...)` take one typed callback per typed role in
schema dependency order; nil selects the generated native direction. Callbacks
receive the native model or XML bytes, existing public `XmlOptions`, actual part
name and relationship ID. Pass those options into `EncodeXML` / `Decode...XML`
to retain the same cumulative budget and immutable root/type selection when
composing several native codecs. The callback may capture that public budget;
its returned error retains the exact original identity.

The same prefix exposes `Write` and `Save`; Save takes all encoder callbacks
followed by all decoder callbacks and validates the full staged round trip before
publication. Root/type dispatch rejects mismatches before selected decoders.
Existing facade names, `XmlOptions`, positional LoadOptions/SaveOptions literals
and old signatures remain unchanged. Callbacks are synchronous and must not
retain the borrowed context or use it concurrently.

```go
input := &model.ProfileArchiveInput{
  Main: &model.ProfileModel{Name: "example"},
  Attachments: []model.SerializerPackageBinaryPart{{
    Data: payload, Name: "/Attachments/data.bin",
    ContentType: "application/octet-stream", SourceIndex: 0,
  }},
}
archive, err := model.ProfileArchiveEncode(input)
if err != nil {
  return err
}
value, err := model.ProfileArchiveLoad(archive, model.ProfileArchiveLoadOptions{
  ExtensionPolicy: model.SerializerPackageExtensionPolicy{
    Parts: model.SerializerPackagePreserve,
    Relationships: model.SerializerPackagePreserve,
  },
})
```

Use the helper prefix recorded in the generated file when a collision selects a
suffix. Repeated/dynamic typed roles use
`SerializerPackageModelPart[*Model]` with `Value`, `Name`, `ContentType` and
`SourceIndex`. A source index identifies an occurrence of the role named by the
schema's `source(...)`, rather than a global entry index. Fixed typed singleton
roles accept a direct model pointer. Fixed names are writer defaults; loading
discovers roles by source, relationship type and MIME identity and accepts valid
alternate physical names. MIME parameter values retain case and significant
quoted whitespace.

`LoadOptions.Budget` shares the complete package operation's ledger.
`Validate func(*ProfileArchiveValue, *SerializerPackageBudget) error` observes all
declared values and retained extensions before publication. Unknown parts and
links reject by default; select `Skip` or `Preserve` separately through
`ExtensionPolicy`. Its `RequiredPart` and `RequiredRelationship` predicates reject
required unknown capabilities even when optional preservation is enabled. Their
returned errors and domain-validation errors retain exact identity. Skipped
payloads still undergo ZIP integrity validation.

Preservation retains whole opaque bytes, logical names/media types and authored
relationship strings. Edited packages are checked again for duplicate identities
and complete source/target closure. Ordinary `Load` and the existing encoders
retain their previous behavior: fixed roles use declared writer paths, and stale
preserved scopes require explicit application repair. Release 2.19.0 provides
an opt-in canonical load before editing:

```go
value, err := model.SerializerPackageCodecsProfileArchiveLoadCanonical(
  archive,
  model.ProfileArchiveLoadOptions{
    ExtensionPolicy: model.SerializerPackageExtensionPolicy{
      Parts: model.SerializerPackagePreserve,
      Relationships: model.SerializerPackagePreserve,
    },
  },
)
```

Use the prefix recorded by `Generated Go selected-codec prefix` when a collision
selects another spelling. `LoadCanonicalWithCodecs` takes the same options and
per-role decoder callbacks as the existing selected `Load`. Decoders still see
actual discovered input paths and IDs with the original shared XML context.
The explicit mode rebases retained links only after all declared roles have been
decoded, before domain validation and publication of the normal owning Value.
Choose that mode before editing; an already edited legacy Value has no additional
hidden provenance fields. Existing aggregate layouts, unkeyed struct literals,
options and default APIs remain unchanged.

Canonical load resolves each internal target in its original source scope and
maps moved fixed singleton endpoints simultaneously. It preserves raw query and
fragment suffixes, unchanged relative spelling and inert external URI text.
Unknown payload bytes remain exact. Occupied destinations, source-local ID
collisions and broken prospective closure reject without publishing a Value.
After success, ordinary `Encode`, `Write` and `Save` use the already canonical
records; repeated output does not reapply an old move. Removing an optional role
then leaves references to its distinct canonical path dangling, so subsequent
encode/Save rejects. Routing indexes, copies and work share the budget and
cancellation checks; no moved fixed paths uses the existing finish path.
Arbitrary dynamic-role renames remain an application responsibility.
Reordering unordered
explicit extension part/link sets preserves canonical output. Declared repeated
role order remains semantic because generated IDs encode occurrence order.

`ProfileArchiveWrite(sink, value[, budget])` stages the complete package before
borrowing the sink and returns accepted bytes plus any error. `ProfileArchiveSave`
accepts optional `SaveOptions{Budget, Validate, Fault}`. It validates a fresh typed
round-trip before invoking filesystem publication. The same platform-specific
Save guarantees and uncertainty reporting described below apply.

For a generic OPC catalog, use `SerializerPackageOpenOPC` and its detached
`Parts`/`Relationships` observations and owning `Read` method. It validates standard
content types, ASCII part identity, source-scoped relationships, inert RFC3987
IRI references, strict XML1.0 metadata and the selected Relationships MCE profile.
Metadata admits strict UTF8 and BOM-marked UTF16; DTDs, entities, undeclared
prefixes and unsupported encoding declarations reject. External targets are
never opened. `SerializerPackageEncodeOPC(parts, links[, budget])` writes standard
canonical infrastructure from checked whole parts and links.

Typed payloads require ordinary owning XML records under their existing mappings.
Native token/scalar work, retained namespaces, identity tuples and generated
application references share the package ledger and retain part/field/link
diagnostics. Existing positional `XmlOptions{limits, validator}` literals remain
valid. `XmlOperationOptions` and additive `EncodeXMLWithOperation`/
`DecodeModelXMLWithOperation` entry points carry optional operation context;
helper names acquire a suffix when a schema already declares that name.

Language 1.21 adds fixed/default storage policy and direct scalar key/reference
selectors. `ProfileArchiveEncodeWithProfiles(input, map[string]string{"resources":
"stored"}, budget)` selects defaults by schema role name. Fixed methods and XML
root/type identities cannot be changed. Automatic forward and typed target checks
run before aggregate publication; domain validators handle additional invariants.
See [application contracts](../serialization/package_application_contracts.md).
These limits do not imply complete Office/OpenXML or 3MF conformance.

## Owned entries and borrowed transports

```go
writer, err := model.SerializerPackageNewWriter()
if err != nil {
  return err
}
defer writer.Close()
if err := writer.Add("data.bin", data, "deflate"); err != nil {
  return err
}
archive, err := writer.Finish()
if err != nil {
  return err
}
reader, err := model.SerializerPackageOpen(archive)
if err != nil {
  return err
}
defer reader.Close()
candidate, err := reader.Read("data.bin")
```

`Add` snapshots payload bytes. `Finish` returns independent owned archive bytes;
`Read` returns independent expanded bytes after size, exact raw-DEFLATE EOF and
CRC validation. `Entries` returns a detached catalog slice. Names retain their
exact ZIP identity, including case; names are metadata and never extraction paths.
UTF-8 and the ZIP legacy CP437 filename mapping are supported. Absolute names,
backslashes, NULs and dot/dot-dot traversal segments reject.

`Open` accepts an owned snapshot of `[]byte`, a path string whose file it owns,
an explicit `SerializerPackageSource`, or an explicitly enabled sequential spool.
For random access, pass `SerializerPackageSource{Reader: source, SizeBytes: n}`.
Its `io.ReaderAt` must expose a stable archive extent and must not retain or mutate
the supplied read buffer after returning. Legal short reads accumulate in calls
of at most 32 KiB. Use `io.NewSectionReader` for an archive beginning inside another
source. The source is borrowed and is never closed. Passing an `*os.File` directly
uses its sequential `io.Reader` path; use the explicit source wrapper for borrowed
random access.

`ZipWriter.WriteTo` implements `io.WriterTo` after complete private archive staging.
`SerializerPackageWrite` writes already prepared bytes. Neither closes nor flushes
a borrowed `io.Writer`. A sink error can leave an accepted prefix, and short writes
fail according to the `io.Writer` contract. The byte count reports accepted bytes.
`Close` abandons private work; it never implicitly finishes an archive.

## Limits and lifecycle

Use `SerializerPackageDefaultLimits`, change the required fields and construct a
`SerializerPackageNewBudget(limits)`. A zero `Limits` value deliberately grants
zero capacity. The budget snapshots its policy; changing the original or a
`Configured` copy cannot relax a running operation. Pass the same budget to all
parts of one operation to share cumulative I/O, metadata, entry, expanded-byte,
allocation and work counters. A successful `Read` charges expanded work again if
the caller requests the same entry again.

Archive, part and logical offset extents are checked before narrowing or source
access. ZIP64 offsets use nonnegative signed 64-bit values; owning arrays also
require the native Go `int` width. This permits bounded virtual-offset readers
without allocating a multi-gigabyte archive. The in-memory writer still requires
complete output storage and does not promise enormous-output streaming.

`MaxBufferBytes` bounds simultaneous logical private byte lengths.
`MaxDecoderAllocationBytes` counts byte results and conservative per-entry
metadata estimates. OPC MIME parsing admits temporary decoded strings and
parameter-record estimates before parsing or invoking a native model codec.
Relationships MCE processing also admits copied text and child-slot estimates
before flattening selected wrappers; these conservative reservations remain
live until the metadata tree is discarded. Go map overhead, spare slice capacity,
garbage-collection
timing, compression workspace and process RSS are not hard physical-memory
ceilings. Use independent process limits when those guarantees are required.
The metadata budget admits the complete central directory before indexing it;
the returned catalog copy is caller-owned output storage.

`CheckCancelled` must return promptly. `Deadline` is an absolute `time.Time`.
Checks surround application callbacks and publication. Reader, writer and shared
budget calls require sequential caller access; callers synchronize goroutines.
Synchronous callback reentry or closure fences the operation before another
borrowed call. Application cancellation and transport errors retain their exact
error values. Subsequent failed/finished/closed operations return `invalid_state`
with the original failure available through `errors.Is`/`errors.As` and `Unwrap`.
Application panics propagate unchanged; recovering outside the operation does
not make its abandoned wrapper resumable. A policy-callback panic also fences
the ledger that was checking it.

Sequential `io.Reader` input requires `AllowSpool`, an explicit existing
`SpoolDirectory`, and `MaxSpoolBytes`. Spool I/O and byte-buffer capacity are
admitted before private file creation or source reads. Returned short bytes are
charged once; probing EOF requires remaining request capacity even though a
zero-byte EOF charges zero bytes. Private spool files are removed on failure or
reader closure. The application source stays open.

## ZIP output and publication

Stored and raw-DEFLATE entries support ZIP32 and ZIP64. `ForceZIP64` enables
small wide fixtures. `Descriptor` selects absent, signed or unsigned 32/64-bit
descriptors; the default is signed. Canonical output sorts physical names and
uses fixed timestamp and regular-file metadata. Entry addition order therefore
does not affect bytes. Encryption, split archives and other compression methods
reject explicitly. Readers validate local/directory agreement, descriptor
extents, duplicate names, overlapping local records and unsupported extraction
versions before publishing the indexed reader. Reading a selected entry performs
its payload integrity checks; indexing alone does not inflate every payload.

`SerializerPackageSaveBytes(path, archive, budget, fault)` snapshots and validates
every payload before creating a sibling temporary file. It writes bounded chunks,
syncs and closes that file, replaces through same-directory `os.Rename`, and syncs
the directory on POSIX. Atomicity and durability depend on the selected filesystem
and operating system; Go's portable `os.Rename` API does not establish a universal
Windows atomic-replacement guarantee. Windows has no portable Go directory-sync
step. An error after replacement is `publication_uncertain` and retains its cause;
it does not mean that the previous file remains present.

The optional fault callback identifies publication boundaries without exposing
payloads. A callback panic still propagates unchanged. If application code recovers
after replacement, it must treat the abandoned acknowledgement as uncertain.
Private handles and staging paths are cleaned up. Crash-safe cross-process
coordination and alternate filesystem qualification remain separate work.

## Verification

The maintained runner is `test/go_package_test.py`. It requires an actual Go SDK,
runs offline with `go 1.22`, and uses independently authored ZIP, descriptor,
malformed-directory and sparse ZIP64 fixtures. Optional native producer/probe
arguments qualify actual C++ interchange. Its `--staged-runtime` switch is only
for explicitly labeled development assembly; that mode is not proof that the
compiler emits the helper. Actual compiler qualification and platform scopes are
recorded separately from development runs.

The [20 October 2026 qualification](../verification/verification-go-packages-2026-10-20.md)
passes the physical ZIP foundation, GO-P01 through GO-P12, using actual compiler
output, Go 1.26.4 on Windows, Go 1.26.0 on native Linux and the `go 1.22` language
baseline. It includes
independent ZIP oracles, actual C++ interchange and fault-injected publication
on the selected filesystem. A separate execution with the Go 1.22 SDK is not
claimed. The final Linux replay builds the actual compiler through the root
wrapper and runs the same independent/native controls, including POSIX Save
directory synchronization. An earlier WSL startup timeout was resolved without
a restart before this successful replay.
The separate [Go OPC qualification](../verification/verification-go-opc-packages-2026-10-20.md)
records actual compiler/SDK evidence for metadata, typed aggregate loading,
preserved context and canonical output on Windows Go 1.26.4 and native Linux
Go 1.26.0. The final native compiler is built with strict GCC checks; both
platforms pass the MIME/MCE scratch regressions, native C++ interchange and Save
fault boundaries. The additional application-contract and shared-codec-budget
runners qualify language 1.21 selectors and native cumulative accounting separately
from that historical platform record.
