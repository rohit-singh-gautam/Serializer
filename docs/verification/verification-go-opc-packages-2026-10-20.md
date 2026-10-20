# Go OPC qualification, 20 October 2026

[Go package API](../languages/go_packages.md) ·
[Physical ZIP qualification](verification-go-packages-2026-10-20.md) ·
[Package contract](../serialization/xml_packages.md)

This record qualifies **GO-P13 through GO-P15 and GO-P17 through GO-P19** for
the selected dependency-free Go, ASCII OPC part-name, ordinary owning XML-model
and selected Windows/Linux filesystem profiles in compiler/runtime **2.14.0**, schema language
**1.20.0**. **GO-P16, GO-P20 and GO-P21 remain open.** Native nested allocation
and reference counters retain independent bounded XML policies; complete runtime
profile selection and cross-part application key checks are outside this scope.

## Actual compiler and Windows SDK

The maintained [runner](../../test/go_package_opc_test.py) executes the actual
rebuilt compiler, generates the full package fixture and separately declared
fixed/dynamic MIME profiles, and tests the emitted facades using **Go 1.26.4
windows/amd64**, offline with cgo disabled and the **`go 1.22` language baseline**.
It uses no appended runtime or handwritten facade binding. The final run exits
zero in **20.429 seconds**; `staged_runtime` is **false**.

| Executed check | Result |
| --- | --- |
| Independent corpus | **143 fixtures**, each executed at generic metadata and schema-bound typed/policy layers: **286 matching decisions** |
| Native metadata input | **2/2** actual C++ canonical OPC archives pass the generic owning catalog |
| Additional SDK groups | **12/12**, including **29 named boundary subcases** plus direct MIME/IRI, exact quota, canonical-byte and public-facade controls |
| Independent output oracle | **8/8**: six ZIP32/forced-ZIP64 absent/signed/unsigned-descriptor OPC archives, one actual typed output and one preserved-extension output |
| Actual typed interchange | **2/2**: C++ example output is loaded and re-encoded through generated Go models/facades, then consumed by the actual C++ example |
| Additional generated profiles | Collision-safe `SerializerPackage_3` independently of `SerializerCompression_3`, and binary-only native protocol selection, compile/run successfully |
| Prepublication validation | Companion-name collisions and explicit XML-disable/package conflicts preserve preexisting output bytes |

The actual compiler SHA-256 is
`309eb574e5cc10ff6a0d2b65e6f069da5b2add267fb831a657ce60139bc4e14f`,
retained with deployed DLLs and source manifests in ignored
`out/build/backlog-2026-10-20/compiler-go-opc-quota-309eb574/`.
The compiler was strictly rebuilt through the Windows root wrapper; its
2,342-file capture aggregate is
`050246d1238e51820c74f99a3443de59cd29cca60721748b555a7c1a37c6d9f9`.
The final runner/resource and CTest registration are included in that capture.

Final `verification.json`, actual generated modules, exact SDK/compiler logs,
native producer/consumer logs and oracle outputs are retained in ignored
`go-opc-quota-windows-final/`. Copied native tools and their deployed DLL hashes
remain in `go-opc-native-tools/`. The runner SHA-256 is
`617dc8afb9150592996792363ba00b491267a4dd8694ad4eecb2d6fcf1e12c14`;
the [Go driver](../../test/resources/package_go_opc_test.go) SHA-256 is
`54fe68fac87012b20156714ea9d7125ca94996bb741f200840d224491b096a2a`.
The ordinary actual package module SHA-256 is
`512151c9a0bc2881fcce5c8166694702d31e966a861a848edc7f9cc097dd4086`.
Every selected executable hash is checked before and after the SDK execution.

## Metadata, graph and publication coverage

Independent Python ZIP/XML and explicit wire producers cover duplicate/conflicting
defaults and overrides, MIME parameter identity, part case/escape aliases and
ancestry conflicts, physical nonparts and their integrity, selected Unicode-name
rejection, UTF8/BOM-UTF16, invalid declarations/DTDs/entities and Relationships
MCE capability/scoping/order/selection rules. Literal IRI delimiters preserve
Unicode lookalikes while malformed bracket/IP/port/escape/scheme/private-use
cases reject; external resources remain unopened.

Graph controls check source-local ID uniqueness, legal reuse at another source,
Unicode NCNames, missing sources/targets, declared external links, duplicate
role claims, alternate main paths and discovered source occurrence routing.
Actual native model decoding stops at the second occurrence of a three-part
collection when its middle scalar is malformed, returning no partial aggregate.
Retained extensions are assigned before complete-aggregate domain validation.
Required unknown part/link predicates override optional preservation; skipped
payloads still undergo integrity checks. Edited preserved graphs are revalidated.

Lifecycle controls swallow callback close/reentry attempts deliberately and prove
no later borrowed read or successful publication occurs. Native and domain
callback panics/errors retain their defined identity/first-cause behavior,
including an earlier ledger error followed by a different callback return.
Exact/one-tight metadata admission leaves no held logical scratch after cleanup.

The same final harness first ran against immutable compiler `8b97919a` and
reproduced **two new quota failures**. A caller-owned 4,096-character quoted MIME
parameter with a 1 KiB buffer grant reached the invalid native model codec;
the repaired emitter reports `resource_limit` before that codec. A valid MCE
ProcessContent wrapper containing 16 KiB of whitespace previously succeeded
under a 64 KiB grant despite additional live text copies. It now rejects that
grant, accepts a 256 KiB control, and passes an independently searched exact and
one-tight capacity boundary. MIME parsed strings/parameter estimates and MCE
text/child-slot/synthetic-root estimates are admitted before copying. Nested MIME
comparison retains both temporary reservations until comparison completes.
Before-control logs, hashes and the two expected failures remain in ignored
`go-opc-quota-before/`; both regressions pass in the final actual SDK replay.
These logical reservations do not promise bounds on Go heap overhead, retained
capacity, garbage-collection timing or process RSS.

Canonical controls reverse two explicit extension parts and two stable-ID links
and require identical bytes. Public generated Write proves bounded borrowed-sink
ownership and exact short/error handling. Save exercises all seven named fault
points: pre-replacement faults retain original bytes and remove staging;
post-replacement faults leave a complete readable new generation and report
`publication_uncertain` with the original cause. A domain-validator rejection
reaches no filesystem fault callback. This is selected-filesystem qualification;
portable Go `os.Rename` does not provide universal Windows atomic-replacement
guarantees, and the Windows path has no portable directory-sync step.

## Native Linux compiler and SDK

The current source snapshot is strictly built through root `make test` with
**GCC 15.2.0**, **CMake 4.2.3** and all four native providers enabled. Its
2,342-file native inventory aggregate is
`d8bae44c3ee2e1c099b890897e436adf512bda59956bc63580283fc2f732fdaf`.
The complete registered suite passes **210/210** in **391.92 seconds**, including
`serializer_package_go_opc`. This full-source gate is recorded separately in the
[current regression record](verification-current-regression-2026-10-20.md).

A separate private replay then uses the copied immutable native compiler
`df7b20c804669d9f57ae80a0eb1b3ff023f5c5150a4d0eed32703745e8ba499b`,
native `package_interop` and native typed OPC example with **Go 1.26.0
linux/amd64**. It executes the same maintained runner without staging support
and exits zero in **31.970 seconds**. All **286** independent decisions,
**12** additional groups/**29** named boundaries, **8** independent outputs,
**2** native metadata inputs and **2** actual typed C++/Go exchanges pass.
Collision and binary-only profiles pass too. The three actual generated module
hashes and all eight archive output hashes match the Windows replay exactly.
Both quota regressions and all seven Save fault boundaries execute on native
Linux; its successful Save path includes directory synchronization.

Native replay reports, generated modules, SDK/native logs and copied tool capture
are retained in ignored `go-opc-quota-linux-final/`. An earlier isolated native
Go replay of the exact Windows-generated module also passes; it remains labeled
as SDK-only evidence in `go-opc-quota-linux-sdk/`, separate from the fresh native
compiler replay above. The active CMake/CTest cache was not changed by either
private SDK replay.

## Existing output preservation

Before integrating OPC hooks, a Rust-inclusive immutable compiler with SHA-256
`cdaa775d0b98f5b2b0b640f9add6ff81224723cc6a82f021a46be16a65d0b337`
generated the representative ordinary schema. The initial Go-only OPC compiler
`8b97919a6797891e020d1fb0383b0f491bfa2f2796bac9d144ac4bd67af4b0c2`
then reproduces **11/11 byte-identical ordinary outputs**, including ordinary Go
and all ten other languages. Source/output hashes and exact before/after files
remain in ignored `go-opc-output-preservation/`. This comparison completed before
the separate Swift optional-provider integration.

The final quota-repaired compiler also reproduces the same ordinary Go output,
145,172 bytes with SHA-256
`e86a1744871889be2fe6b2ebcb7fdef410da2a18db4f223561e25c63e74799ee`.
Its focused report remains in `go-opc-quota-output-preservation/`. The separate
Swift provider feature intentionally changes Swift support output and is not
part of the historical eleven-output equality claim.
