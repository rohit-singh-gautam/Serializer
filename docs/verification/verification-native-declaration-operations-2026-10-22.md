# Native and C#/Go wildcard declaration operations: 2026-10-22

This focused record qualifies C, Rust, C# and Go strict/lax declaration dispatch,
private validation resource accounting and transactional replay. The
[eleven-output wildcard record](verification-xml-wildcards-2026-10-22.md)
tracks the umbrella `OPC-R02` acceptance and later integrated regression.

## Implemented behavior

Immutable pinned global element/attribute identities select compiler-generated
validation DTOs. C resolves declared presence before capture: strict absence
rejects early, and undeclared lax payload under discard policy traverses without
an owning subtree copy. Rust strict/lax dispatch instead uses bounded temporary
XML framing under the original operation, without retained-preservation charges;
its explicit processContents=skip path streams. A matching declaration must
validate even when the caller discards its foreign payload. Declared invalid
content never falls back to lax unknown handling. Explicit processContents=skip
bypasses declaration validation.

Private codecs enforce complete particles, atomic/list/QName restrictions,
original occurrence namespaces, attribute required/default/fixed rules,
abstract-element/type flags, nil lexical/content rules and legally derived
expanded `xsi:type` selection. Derived inventories respect block/final constraints.
Only validated root xsi:type/nil transport markers are removed before private
replay; nested markers remain subject to their own declarations. Attribute DTOs
use the implicit `xml:attribute` wrapper, avoiding new namespace bindings which
could change unprefixed QName or QName|string fallback identities.

Validation contexts retain the original operation and independent policies while
masking package root type/capability authorization. Bounded temporary capture and
nested private DTO presentation consume work/allocation, with no retained-byte
charge. Actual retained state consumes preservation before copying. C private
filter validation mode suppresses foreign/default/context-owner sidecars; the
ordinary filter API delegates to its unchanged owning behavior. C reader cleanup
now distinguishes an explicit stack callback context from heap-owned operation
reader contexts. Cleanup cannot free that stack context or replace the first error.

C#/Go private helper families select collision-free names. Aliasing applies only
to compiler-owned runtime identifiers and explicit generated references; authored
model names and quoted XML/schema identities remain intact. The C# helper-name
SDK exercises seven authored names overlapping both old and new helper families.

## Executed SDK matrix

`test/xml_wildcard_validation_test.py` authors 142 independent decisions per mode:
strict/lax times preserve/skip, 568 decisions per actual SDK. Each mode validates
original and emitted documents with lxml XMLSchema and compares all four native
protocols. Fixtures include global kinds, scalar facets/enumeration, QName/list
scope aliases and unbound prefixes, sequence/all/choice content, nested strict
wildcards, attributes, empty attribute defaults/fixed values, unqualified attributes
under a foreign default namespace, legal complex/simple derivations, abstract and
blocked types, nil attributes/fixed/content and unknown vocabularies.

C# and Go final semantic/resource runs use frozen compiler SHA-256
`ef8bedc1e8ba2c520ea4856474d78cde63e6dfc5446bf02a60fe8068a72c76dd`.
Logs: `out/opc-wildcard-validation-{csharp,go}-final-<mode>.log`.
C# compiles with warnings as errors; Go builds its real generated module.
The seven authored-name SDK passes XML identities, exact lexical strings and all
four native protocols in `out/opc-wildcard-helper-names-csharp-final.log` using
`test/xml_wildcard_helper_names_test.py`.

C ordinary semantic modes and nine compositor variants use SHA-256
`2e236d9ed1cb9d4cf066bbc81e5e51d003c8b7e00989e65781f7313cc6e6b6ae`.
Logs `out/opc-wildcard-validation-c-<mode>-ninja.log` record 568 decisions.
`test/xml_group_extensions_test.py` additionally passes all/anyAttribute,
choice/XSD suffix and choice/authored suffix times skip/lax/strict: 294 decisions,
with original/emitted provider checks and four native protocols. Logs:
`out/opc-wildcard-groups-c-<group>-<process>.log`.
Native C compiles through Ninja CMake with MSVC C11 `/W4 /WX` and libxml2 2.15.3.
The installed Visual Studio default CMake generator could not discover C, so the
executed configuration explicitly selects `CMAKE_GENERATOR=Ninja`.

The later 300-field fallback preserve/skip matrix passes 48 decisions per mode
in each actual C, Rust and Go SDK (96/backend), using the final `0d86b6...`
snapshot. `test/xml_unknown_policy_test.py --field-count 300` checks the
any-position sentinel through large field inventories, independently observes
opaque preservation/discard, and compares four native protocols. Logs:
`out/opc-fallback300-{c,rust,go}-{preserve,skip}.log`.

## Empty foreign declaration inventories

`test/xml_wildcard_empty_inventory_test.py` pins an import-free XSD with two
known string children, a middle `xs:any namespace="##other"` particle (zero to
two occurrences), and matching anyAttribute. There are no foreign global
declarations. Sixteen independent cases per mode cover known content,
undeclared foreign elements/attributes, core unknowns, misplaced/excess
occurrences and missing known children. Strict rejects undeclared foreign names;
lax admits them subject to the same namespace/position/count rules. Preserve
retains their opaque content, while explicit skip discards it. Original and
emitted documents are independently checked with lxml XMLSchema; actual SDKs
also compare all four native protocols.

C and Rust each pass all four strict/lax preserve/skip modes (64 decisions per
SDK) using compiler SHA-256
`744e32a98729a0ef345ea3d48fab12bfa3eb07805a5a0760310f12e23bff739a`.
C builds as C11 with `/W4 /WX`; Rust builds with `-D warnings`.
The preceding `fbb7245b...` compiler exposed unused uri/local/value parameters in
empty native dispatchers. Explicit no-op parameter uses fix those warning errors
without changing dispatch semantics or public contracts. The maintained fixture
covers this zero-declaration emission path separately from the populated
142-case inventory. Final logs:
`out/opc-wildcard-empty-{c,rust}-<process>-<retention>.log`.
The initial compiler failures remain in
`out/opc-wildcard-empty-{c,rust}-strict-preserve-before.log`.

## Shared-operation and failure proof

`test/xml_wildcard_admission_consumers.py` injects C#/Go/Rust operation probes into
the same independent 142-case corpus. The nested valid declaration case measures
actual encode/decode work, allocation and preserved bytes, then tests exact
admission and one-tight failure for each nonzero resource. The original C#/Go
exception/error object survives; Rust checks exact Arc identity and explicit poll
cancellation. Skip mode reports zero shared preservation and accepts local
maxPreservedBytes=0, including nested private wildcard preservation. C#/Go tests
also edit retained declared count text from valid 3 to invalid 0, require output
rejection, restore it, and continue the ordinary native-wire checks.

Rust final four admission modes use SHA-256
`0d86b6b21bf0586ee20b6929ef9c49816b801fe6c3144fea34c84a9cc1806a84`;
all 568 decisions pass in `out/opc-wildcard-admission-rust-final-<mode>.log`.
The tests call the public `DocumentCodec` and `Operation` contracts. Native bytes
and opaque companion preserved-byte counts remain unchanged after rejected
operations. Rust opaque companion fields have no public editing API; the safe
edited-value probe rejects an XML-invalid native string and restores that value.
It does not claim public opaque-payload mutation support.

`test/native_wildcard_admission_test.py` adds a real generated package declaration
to the shared corpus and calls its native document descriptors. All four modes
pass another 568 provider decisions using the same `0d86b6...` snapshot, with
`test/resources/native_wildcard_admission.inc` checking exact/one-tight shared
work/allocation/preservation in both directions, original caller status 777,
package contract restoration and unchanged initialized native/state/output
ownership after failure. Skip passes local/shared quota0. Preserve tests edit the
retained declared count, reject output and restore the original payload.
Logs: `out/opc-wildcard-admission-c-final-<mode>.log`.
These are unmodified final generated SDKs; the earlier isolated diagnostic reader
cleanup experiment was superseded by the fresh compiler run.

## Independent native policy propagation

A final review found that C/Rust private declaration DTOs inherited shared
operation quotas but reset independently selected native identity limits to their
defaults. Native private decode now forwards the selected map, mixed, digest and
identity policies. C borrows the original operation policy pointers; Rust stores
a private copy of those immutable limits in its synchronous shared operation
state, without retaining the caller's options or changing public/native layouts.
Nested declaration replay uses that same selection. Ordinary companion methods
which expose no independent policy selector retain their existing defaults.

`test/xml_wildcard_identity_policy_test.py` adds a declared global with an XSD key
and a second global which recursively validates it through a strict wildcard.
Four independent documents cover direct/nested valid and duplicate keys. Actual
C/Rust SDKs pass all four decisions and four native protocols using compiler
SHA-256 `953fa6df089addceb9c23ed5f1ddddd664e9e7ad7ef82073784d8c72037360be`.
For each valid shape, public operation decode, encode and prepare-only admission
accept defaults, reject maxTuples=0, and accept defaults again. C checks typed
resource-limit classification, original policy/package-contract pointer identity
and unpublished model/state/output on failure. Rust checks the concrete failure
returned by the caller's Operation::limit callback. The former `0d86b6...`
compiler compiles this regression but fails its selected-policy assertions;
`out/opc-identity-policy-{c,rust}-before.log` records that reproduction.
Final logs: `out/opc-identity-policy-{c,rust}-final.log`.

After this policy fix, the full native shared-operation admission matrix was
regenerated and replayed with the same `953fa6df...` compiler. C and Rust each
pass all four strict/lax preserve/skip modes, 568 independent decisions/backend,
including the existing exact/one-tight work, allocation and preservation probes,
first-failure identity, skip quota0 and transactional replay checks described
above. This remeasures actual Rust state allocation after its private policy
snapshot was added. Logs:
`out/opc-wildcard-admission-{c,rust}-policy-final-<mode>.log`.

The same direct/nested selected-policy regression passes actual Go SDK output
with its public identity-limit selectors on decode and encode, using the earlier
`0d86b6...` snapshot (`out/opc-identity-policy-go.log`). The independent key probe
qualifies identity policy propagation; the forwarding implementation also carries
the other existing independent native policy values into private token decoding.

Representative commands (initialize the native MSVC environment for C):

```powershell
$env:PYTHONPATH='out/python-dependencies'
$env:CMAKE_GENERATOR='Ninja'
python -B test/xml_wildcard_validation_test.py --compiler out/build/opc-declaration-sdk-checkpoint/serializer.exe --build out/build/opc-completion/wildcard-validation-csharp-final-strict-preserve --language csharp --process strict --retention preserve
python -B test/native_wildcard_admission_test.py --compiler out/build/opc-final-declaration-checkpoint/serializer.exe --build out/build/opc-completion/wildcard-admission-c-final-strict-skip --language c --process strict --retention skip --libxml2-prefix out/build/ReleaseWindows/vcpkg_installed/x64-windows
python -B test/xml_wildcard_validation_test.py --compiler out/build/opc-final-declaration-checkpoint/serializer.exe --build out/build/opc-completion/wildcard-admission-rust-final-lax-preserve --language rust --process lax --retention preserve
python -B test/xml_wildcard_identity_policy_test.py --compiler out/build/opc-native-policy-checkpoint/serializer.exe --build out/build/opc-completion/identity-policy-native --language c,rust --libxml2-prefix out/build/ReleaseWindows/vcpkg_installed/x64-windows
python -B test/xml_wildcard_empty_inventory_test.py --compiler out/build/opc-empty-native-checkpoint/serializer.exe --build out/build/opc-completion/empty-inventory-native --language c,rust --process lax --retention preserve --libxml2-prefix out/build/ReleaseWindows/vcpkg_installed/x64-windows
```

## Compatibility and limits

The same final `0d86b6...` snapshot replays C namespace/foreign package companions
(seven inputs/eight independently inspected archives), and C#/Go common profile
pipelines (16 inputs/eight archives each). Logs:
`out/opc-pipeline-c-xml-graphs-declaration-final.log`,
`out/opc-pipeline-{csharp,go}-declaration-final.log`.
The [package pipeline record](verification-package-pipelines-2026-10-22.md)
retains broader eleven-output envelope/relocation evidence.

These counters admit logical work and owned storage at generated boundaries.
Opaque provider allocations/CPU and authored factory internals retain their
published boundary; no process-peak memory or immediate provider-preemption
claim follows. Final combined repository tests and later source edits must be
reported independently from these captured SDK runs. Useful logs/final outputs
remain under ignored `out/`; temporary diagnostic wrappers and the patched SDK
were removed.
