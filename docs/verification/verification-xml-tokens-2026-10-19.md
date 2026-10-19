# Generated XML token verification - 19 October 2026

Compiler/runtime **2.12.0**, schema language **1.20.0**, editor packages **2.0.5**.
Source identity is the uncommitted worktree based on
`1c6a155329a9364f67ced27cbe7fa142aaf11b9d`. These checks ran from that worktree
and did not publish artifacts.
This record tracks individual X01-X22 instances from [the backlog](../pending.md),
and the [commented OPC reference example](../../example/opc/README.md).

## Executed checks

The native build uses the existing Windows full-provider Ninja configuration at
`out/build/opc-implementation`, through `./make.ps1` and CMake target refreshes.
It enables LibXml2, zlib, Zstandard, LZ4 and managed state. The generated-code
tests compile actual emitted headers with MSVC warnings treated as errors.

The later XML compiler refresh is
`out/build/x-tasks/final-xml-compiler/serializer.exe`, reporting compiler 2.12.0
and schema language 1.20.0. Its SHA-256 is
`683c3d5f3e183ee63e84c2db38cf47359eebe455c52d805ce971dcdb86067fe9`.
The corresponding 318-file inventory of `src/`, `include/`, `cmake/`, root CMake
and test CMake inputs has aggregate SHA-256
`f6587122d8824172591a3a9673f075e65087ccddae282d717ab3be12e776ca43`;
per-file hashes and the aggregation procedure are retained in
`out/build/x-tasks/portable/source-fingerprint.json` and `source_fingerprint.py`.
The earlier 82632 fingerprint is preserved separately as
`source-fingerprint.82632.json`; later paragraphs retain their executed
checkpoint identities. Fresh C/Rust extension SDK qualification passes on this
compiler, as recorded below.

All **eleven non-XML generated outputs are byte-identical** to the independently
SHA-256-checked captured ordinary baseline after fresh generation with this
compiler. Results and the exact command are retained under
`out/build/x-tasks/portable/non-xml-regression/`. This establishes the selected
fixture's source equality, without a universal performance claim. The
changed-document audit first recorded 1,125 local links at 82632. After the final
compiler and documentation refresh it passes **1,127 local links and 269 heading
anchors**, with zero introduced broken destinations; two unrelated preexisting
invalid references were excluded.
Its report is `out/build/x-tasks/portable/changed-doc-heading-audit.json`. The
added-content audit separately passes 41 local links and two heading anchors.
The XML support inventory checks 203 include references to 130 distinct support
files, with no missing files or CMake entries; `source-inventory.json` records
those checks. The runtime-template literal-size/license test also passes.

The Python identity suite passes **5/5 cases** through the real compiler and
Python 3.14. It checks split-byte decoding, exact numeric aliases, incomplete and
duplicate tuples, forward/missing references, output rejection, every exposed
index admission bound, shared callback failure, backend-specific generation,
and preservation of both existing outputs on a failed multi-target request.
The independent schema oracle uses lxml **6.1.3** from
`out/build/python-package/python-deps-readable`. That provider rejects the
`+0007` unsigned lexical alias accepted by the existing Serializer contract;
the independent input uses `0007`, the generated decoder separately checks the
leading-plus alias, and canonical generated output is independently validated.

The OPC runner passes all **four C++/Python producer-consumer pairs** and two
independent ZIP/XML checks. It regenerates Python codecs, saves/loads both
packages, inspects actual DEFLATE parts, checks the complete metadata inventory,
and compares expanded XML names, model attributes and exact attachment bytes.

The Go facet suite passes **12 independent XSD oracle documents**, including
uint64 boundaries above binary64 precision, exclusive floating boundaries,
selected float32 rounding, Unicode lengths, whitespace, enumeration and invalid
output rejection. Missing Go or lxml fails this opt-in qualification.

Reproduce these checks with:

```powershell
$env:PYTHONPATH = (Resolve-Path out/build/python-package/python-deps-readable).Path
python -B test/python_xml_identity_test.py --compiler out/build/opc-implementation/serializer.exe --build out/build/x-tasks/python-identity
python -B test/go_xml_facets_test.py --compiler out/build/opc-implementation/serializer.exe --build out/build/x-tasks/portable/go-facets
python -B example/opc/run.py --compiler out/build/opc-implementation/serializer.exe --cpp out/build/opc-implementation/example/opc/serializer_opc_example.exe --build out/build/x-tasks/opc
```

The Python token/provider/HTML suite passes **12/12 cases**; scalar and portable
pattern qualification passes **4/4**, rich XML **11/11**, identity **5/5**, choice
**4/4** (1,275 independent oracle traces), and package **11/11**. Python shapes
and concrete generic instances pass their actual SDK consumer. Logs are retained
at `out/build/x-tasks/python/*-final.log`. The token tests disable legacy tree
entry points and include typed-memoryview byte accounting, one-byte mixed-text
chunks, nil/maps/both variant forms/all/inheritance and failed-output preservation.
Python model qualification adds **5/5** cases covering revision/magic metadata,
historical/current field intervals, malformed layouts and owning raw/fixed/MD5
digest hex values. Extension qualification adds **4/4** reject/skip/preserve,
slot/count, namespace/capability, edited-content and cumulative-bound cases.
Preserved foreign content retains only its required local subtree; skipped
content is discarded. Mixed/`all` plus new extension slots remain unsupported.

Kotlin direct input/output passes its actual SDK with the retained-node constructor
forced to throw, including a negative control proving the instrumentation is
active. The test checks exact EOF, Unicode/lexical boundaries, optional empty
content, wrappers and resource failures; log `out/build/x-tasks/kotlin-tokens-final.log`.
Java/C#/Kotlin prepared XSD providers pass real generated encode/decode, wrong-root,
no-fetch, instance-hint, bound and eight-worker repeated-validation checks. Logs
are `out/build/x-tasks/provider-portable-final.log`; all three SDKs pass in that
final run. Kotlin array comparisons use native `IntArray.contentEquals`.
The C# tests exposed and fixed undeclared-root acceptance as an optional provider
warning; prepared validation now explicitly checks the compiled global declarations.

The first full Windows native run passes **186/194**. All eight failures pass on
targeted rerun: the runtime-literal test accepts the legitimate 2026-only copyright
prologue, the XSD bundle test expects newly supported C++ all generation, five
managed/history tests use workspace-local `TEMP`/`TMP`, and the C optional consumer
uses Ninja to avoid MSBuild's denied FileTracker path. Logs are
`out/build/x-tasks/final-native.log`, `native-recheck.log` and `optional-c-ninja.log`.
Newer generator groups are qualified separately below; these results do not
claim a full native run against later unverified source edits.

An earlier minimal all-provider-OFF build passes both selected CTests (16 active GTests;
eight provider-dependent cases skip). An installed CMake consumer compiles with
warnings as errors without a vcpkg toolchain, generates a native model through
installed `serializer_generate`, and checks provider-free output/HTML plus clear
reader/XSD-unavailability errors. Package discovery accepts 2.0/2.7/2.12 and
rejects 2.13/3.0; the installed token header matches source. Logs remain under
`out/build/s01-minimal/x-tasks-*.log`.

The final provider-OFF MSVC run through `./make.ps1 test` in
`out/build/s01-minimal` passes **106/106 CTests in 181.75 seconds**. The log is
`out/build/x-tasks/final-minimal-test.log`. The full-provider native build also
completes all 955 steps (`out/build/x-tasks/final-native-build.log`), followed by
**196/196 passing CTests in 223.85 seconds**
(`out/build/x-tasks/final-native-ctest.log`). The earlier native checks above are
historical checkpoints.

The final provider-OFF installation also passes its refreshed installed-consumer
check. After a targeted compiler rebuild and `cmake --install`, a consumer using
`find_package(serializer 2.0 REQUIRED CONFIG)` compiles with MSVC `/W4 /WX` and
exits successfully. It checks the token writer, portable pattern program,
metadata constant and clear provider-OFF diagnostic; the installed compiler
reports 2.12.0 with schema language 1.20.0. Logs are
`out/build/x-tasks/final-minimal-compiler-refresh.log`, `final-minimal-install.log`
and `final-minimal-consumer.log`. The generated package-version matrix accepts
2.0.0, 2.7.0, 2.11.9 and 2.12.0, and rejects 2.12.1, 2.13.0, 1.99.0 and 3.0.0
(`out/build/x-tasks/final-package-version.log`).

## Scope boundaries

The latest C++ scalar consumer passes **71 independent oracle cases** through an
installed CMake package: exact uint64 endpoints, numeric facets, signed floating
underflow/subnormals, Unicode lengths, whitespace/enumerations, approved patterns,
QName/list/default contexts and output revalidation. The provider-backed pattern
path admits the complete input before validation; it does not promise a hard
bound on LibXml2's internal work. The direct namespace group additionally passes
32 native token tests with retained-tree storage disabled. Logs include
`out/build/x-tasks/cpp-scalar-underflow-oracle.log`.

JavaScript/TypeScript scalar qualification passes **6/6 actual SDK cases**,
including exact integer/decimal boundaries, Unicode facets, whitespace,
enumerations, fixed/default values, simpleContent, prepared portable pattern
programs and cumulative state/work limits. The suite tests long integer aliases
before BigInt conversion and independent XSD output validation. It uses canonical
integer fixed values in the provider fixture because the independently reproduced
LibXml2 fixed-value comparison for noncanonical integer literals is lexical;
native alias acceptance remains checked separately. Python scalar **4/4**,
token **12/12** and rich **11/11** reruns also pass the required-fixed-attribute
repair. Logs remain under `javascript/*-final.log` and `python/*-final.log`.

Java/C# owning inheritance each passes **30 independent XSD cases**, with private
and protected multiple bases, nested optional/map/variant fields, canonical base
order, unchanged native binary values, null-base rejection and atomic naming
failures. C# compiles with nullable warnings treated as errors;
`out/build/x-tasks/portable/final-inherited.log`. Rust and C each pass the shared
**56 independent all-group probes** with their actual SDKs and strict warnings;
logs are `out/build/x-tasks/rust/all-final.log` and `c/all-final.log`.

Java/C# namespace-independent scalar qualification additionally passes **95
probes and six atomic generation gates per SDK** (`portable/final-scalars.log`).
The suite checks arbitrary-precision decimal/integer families using independent
Python Decimal/int expectations where LibXml2's own decimal precision is limited,
and validates other cases and emitted XML through the independent XSD provider.
It covers typed/lexical lists, union branch selection, Unicode name types,
defaults/fixed values and authoritative simpleContent. Namespace-dependent QName
rules remain explicitly gated; later bounded-list source changes require their
own recorded rerun.

Rust and C also each pass **110 independent presence decisions** on compiler
snapshot 41099: authoritative XSD scalars/records, absent/nil/empty nested states,
optional record/enum collections, required fixed arrays and exact native protocol
identity. C checks unchanged destination bytes on every rejected candidate and
releases owned allocations; Rust compiles with `-D warnings`. Both baseline direct
token suites pass again with retained-tree entry points disabled or counted.
Logs are `rust/presence-final.log`, `c/presence-final.log`, `rust/tokens-final.log`
and `c/tokens-final.log` under `out/build/x-tasks/`.

C/Rust/Kotlin/Swift each pass **23 independent authoritative-XSD inheritance
probes**, including private/protected multiple bases, fixed arrays and optional
nil states; all four native protocols retain the same complete field values.
Ambiguous/repeated bases reject atomically in a separate four-language gate run.
Logs are `out/build/x-tasks/native-model-{rust,c,kotlin,swift}.log`. Swift's direct
token suite additionally reproduces and repairs Foundation dictionary/callback
merging of canonically equivalent Unicode attribute prefixes: the provider still
validates XML, while a bounded active lexical cursor and exact prefix scopes
preserve both expanded attributes without retaining completed XML nodes.
The regression, ordinary lexical checks and tree-disabled control pass in
`out/build/x-tasks/swift-unicode-tokens.log`.

C/Rust/Kotlin/Swift each pass the shared **40 independent content probes** on
snapshot 18028: required and optional singleton choices with scalar/string/record
alternatives, invalid edited native zero/multiple alternatives, authoritative
uint64 simpleContent with required attributes, and repeated record wrappers.
Every successful output is independently XSD-validated, compared with typed XML
values and round-tripped through all four native formats. C additionally verifies
unchanged destinations on input failure. Logs are
`out/build/x-tasks/content-{rust,c,kotlin,swift}.log`. Grouped/repeated
heterogeneous choices retain their explicit storage restriction.

Java/C#/Go each pass the same **40 content probes** through their SDKs, with
edited missing/multiple choice and null-record failures
(`portable/final-content.log`). Java/C#/Go namespace-independent scalar
qualification now passes **122 probes and six atomic binding gates per SDK**,
including child/all/simpleContent lists, exact list/text admissions and defaults;
`portable/final-scalars.log` and `portable/final-scalars-go.log`. Their older
native numeric/pattern suites also pass on snapshot 18028.

JavaScript/TypeScript now pass mixed **4/4**, identity **6/6**, exact floating
**2/2**, lexical **5/5** and scalar **6/6** suites, alongside all earlier SDK
regressions on snapshot 15606. Mixed callbacks preserve private ordered native
fragment storage, arbitrary UTF8 chunking and exact logical text/fragment bounds.
Identity observers enforce exact decimal/QName tuples, forward references, nil
key/unique rules and cumulative work/index policies without a known-element
tree. The nil-unique independent oracle uses JDK XSD for the one isolated LibXml2
nil-unique diagnostic defect. Logs include `javascript/mixed-final.log`,
`identity-final.log`, `identity-nil-oracle.log`, `numeric-final.log` and
`lexical-final.log`.

Kotlin/Swift native map suites each pass **33 independent XSD/key probes** on
snapshot 15606, all four native formats, canonical aliases, optional present-empty
wrappers and exact/default/expanded entry/key policies propagated through nested
records. Logs are `out/build/x-tasks/maps-kotlin.log` and `maps-swift.log`.
`XmlMapLimits` is a companion policy; existing XML-limit constructor/storage
contracts remain unchanged. The subsequent Swift output-constructor cleanup
validates the policy once per operation. This paragraph records the 15606
checkpoint; later checks retain their separately stated scope.

The final matrix below records the selected X01-X22 reference contracts after
the later qualification runs in this record. Each range expands under its
backend prefix. Completion of these finite reference contracts does not imply
support for every combination, full OPC/3MF conformance or qualification on
platforms not executed here. Broader requirements retain their OPC-R IDs in
[the backlog](../pending.md). Provider-backed rich QName/wildcard/repeated-choice
paths are not claimed tree-free by the ordinary token checks.

| Backend prefix | Verified task instances | Explicit selected-contract limits |
| --- | --- | --- |
| CP | X01-X22 | Mixed/all foreign-slot combinations, unrepresentable input/managed/view projections and child-local QName storage retain explicit fences; concrete owning generics are checked |
| PY | X01-X22 | Broader grouped heterogeneous choices and namespace-dependent union/list value identities retain their documented restrictions |
| JV | X01-X22 | QName/prefix-list scalar mappings and broader grouped/repeated heterogeneous choices reject; mixed/all plus extension slots reject |
| KT | X01-X22 | QName/prefix-list scalar mappings and broader grouped/repeated heterogeneous choices reject; mixed/all plus extension slots reject |
| CS | X01-X22 | QName/prefix-list scalar mappings and broader grouped/repeated heterogeneous choices reject; mixed/all plus extension slots reject |
| GO | X01-X22 | QName/prefix-list scalar mappings and broader grouped/repeated heterogeneous choices reject; mixed/all plus extension slots reject |
| JS | X01-X22 | Broader grouped/repeated heterogeneous choices and unresolved namespace-dependent union/list value identities retain explicit fences |
| TS | X01-X22 | Broader grouped/repeated heterogeneous choices and unresolved namespace-dependent union/list value identities retain explicit fences |
| RS | X01-X22 | Namespace-dependent scalar restrictions remain; opaque state uses explicit document companions with the graph fences recorded below |
| SW | X01-X22 | QName/prefix-list scalar mappings and broader grouped/repeated heterogeneous choices reject; mixed/all plus extension slots reject |
| C | X01-X22 | Namespace-dependent scalar restrictions remain; opaque state uses initialized document companions with the graph fences recorded below |

All eleven outputs' baseline direct owning input/output, namespace traces and
concrete integer/text generic instances pass actual SDK consumers. Swift uses
typed callbacks; Rust/C input uses pull readers. Java/C#/Go all traversal passes
**56 independent XSD oracle decisions per SDK**, including permutations, nested
records, optional/empty groups, required attributes, exact namespace identity,
duplicate/missing members and output revalidation; `portable/final-all.log`.
Java/C# prepared scalar/pattern suites each pass **61 decisions** (59 independent
XSD oracles and two machine-policy checks), including generated programs larger
than Java's constant-string limit. Programs are emitted in bounded fragments;
`portable/final-portable-facets.log` records actual javac/.NET compilation.
Go maps pass **33 independent XSD/key cases**, plus zero/exact/tight/default and
expanded entry/key bounds, nested policies and existing positional-constructor
compatibility. Java and C# each pass the same **33** map oracle cases and policy
checks (`portable/final-go-maps.log`, `portable/final-managed-maps.log`).
Go/Java/C# element-selected variants each pass **51 independent XSD decisions**,
covering scalar/enum/record alternatives, alias overrides, matching default and
prefixed QName hints, tag/type disagreement and cardinality. Invalid native
selectors/null payloads and node limits also reject (`portable/final-variants.log`).
The Go module language baseline remains 1.22. The Go portable pattern
suite passes **31 independent XML/output probes**, including Unicode ranges,
nullable loops, intersected groups, input/state/work limits and transactional
rejection of provider-only syntax. Go inherited tests cover multiple/nested base
composition, base-first ordering, native binary equality, nil-base output failure,
late input failure and static collision diagnostics. Configured text/wrappers
in the nine portable shape consumers do not close their broader XSD-derived X11.

Kotlin passes the same **56 independent all-group decisions** through its native
SDK (`out/build/x-tasks/kotlin-all-final.log`). Kotlin also passes **20 independent
nil/absence XSD decisions**, retaining all optional layers and native protocol
values for absent, nil and empty scalars/records/collections
(`out/build/x-tasks/kotlin-presence-final.log`).
C++ token tests now pass **25/25**, plus **18 identity tests**,
through optimized CMake/MSVC compilation with warnings as errors. Added cases
check concrete templates, distinct named instances, dimensions, nesting, private
fields and private/multiple owning inheritance; ordinary tree exchanges remain
compatible. Unknown application-only template specializations compile native
code and reject XML explicitly. Identity observers check scoped key/unique indexes,
forward/missing/wrong-scope keyrefs and bounds directly during token traversal.
Variant tests include QName type hints, default/prefix-shadowed namespaces,
tag/type disagreement, nil and exact payload cardinality. See
`out/build/x-tasks/cpp-generic-inheritance-*.log` and the xsi/identity checkpoint logs.
C++ mixed tests retain no document tree (`max_tree_bytes=0`), traverse private
variant fragments, and check one-byte Unicode, interleaving/escaping,
adjacent/empty text normalization, fixed-array counts and mixed text/fragment
bounds (`out/build/x-tasks/cpp-mixed-tests.log`).

JavaScript/TypeScript pass **6/6 actual SDK cases** with DOM parsing and old
model-tree readers disabled. They cover fatal arbitrary UTF-8 chunks and exact
EOF, namespaces/private fields, independent semantic resource boundaries,
validator callbacks, strict TypeScript compilation, native-format controls and
transactional naming diagnostics (`javascript/token-final.log`). Both generated
shape/generic consumers also pass (`javascript/shapes-final.log`).

Later generated SDK checks pass JavaScript/TypeScript content **5/5** (including
authoritative XSD nil bindings), maps **4/4**, variants **4/4**, and private owning
inheritance **4/4**. The variant cases cover both selection modes, private native
selectors, shadowed Unicode prefixes and empty type namespaces. Inheritance
rebases private declaring-owner callbacks over the existing composed bases;
native fields and all protocol bytes remain unchanged. Logs are
`javascript/content-final.log`, `map-final.log`, `variant-final.log` and
`model-final.log`.

Go/Java/C# each pass **148 independent presence decisions** and **13 atomic
binding rejections**, covering native optional layers, record/enum collections,
fixed extents, authoritative XSD nillability, inferred wrappers and nil with
unordered/scalar/pattern constraints. Both variant modes additionally pass
**110 strict independent XSD decisions per SDK**, including empty type namespaces
and ancestor-prefix collisions (`portable/final-presence.log`,
`portable/final-typed-variants.log`).

Swift's direct input test passes with the legacy parser disabled, including its
negative control, UTF-8/lexical/resource failures, exact trailing-root rejection
and byte-exact Unicode namespace identity (`swift-tokens-final.log`). Its
presence suite passes **110 independent XSD decisions**: 79 authoritative base
cases and 31 manual record/enum collection/fixed-array cases
(`swift-presence-final.log`). Its unordered suite passes **56 decisions**
(`swift-all-final.log`). Foundation's nonfatal Unicode namespace URI warning is
distinguished from parser errors; complete-document callbacks remain mandatory.

Rust's generated direct-input suite passes with the legacy tree parser disabled,
including a valid-document instrumentation control, split text/CDATA/entities,
long leading-zero character references, exact EOF, UTF-8 byte admission and all
native protocols. Maintained namespace/generic/wrapper/provider-hook consumers
also pass (`rust-tokens-final.log`, `rust-shapes-final.log`). Pinned xmlparser
0.13.6 is added alongside unchanged roxmltree 0.21.1; all four fixture lockfiles
preserve their existing dependency versions.

C's generated pull-input suite passes with explicitly tracked DOM calls held at
zero, all four native protocols, split numeric CDATA/entity content, 15 malformed
input probes and unchanged destination bytes after failure. Its maintained shape,
generic, namespace, non-C-locale and prepared-XSD consumer also passes with MSVC
warnings as errors and LibXml2 2.15.3 (`c-tokens-final.log`, `c-shapes-final.log`).

C++ positioned extension qualification at its earlier recorded checkpoint passes **29 generated token tests** and
**9 shared extension tests**, with direct known fields and only explicit opaque
foreign payloads retained. Exact positions/counts, namespace collisions, repeated
roundtrip stability, skip/reject/capability and preservation bounds are checked
(`cpp-extensions-tests.log`, `cpp-extensions-shared-tests.log`). Mixed/all/generic
per-owner slot combinations remain outside that checked contract. C++ also passes the shared **148
presence decisions plus 13 binding rejections**, including one-byte input,
authoritative nil/wrappers, facets and unordered dispatch
(`cpp-presence-oracle.log`, `cpp-presence-native-tests.log`).

Logs are retained under `out/build/x-tasks/portable/`: `final-sdk.log`,
`final-go-facets.log`, `final-go-model.log`, `final-go-patterns.log`, and
`sdk-versions.log`. SDKs are Go 1.26.4, JDK 26+35-2893, .NET SDK 10.0.402,
Node 22.23.1, npm 11.18.0, TypeScript 5.9.3, Rust/Cargo 1.99.0, Kotlin 2.2.21
using JRE 17.0.18+8, and Swift 6.4. Native C uses MSVC 19.51.36260 with
LibXml2 2.15.3. Independent lxml 6.1.3 reports LibXml2 2.11.9; that is distinct
from the native runtime provider.

Direct token paths retain the owning destination, open element state, bounded
scalar text and permitted validation indexes. C++ pulls at most 4096 input bytes
per queue refill; Python consumes arbitrary caller chunks through Expat. These
are logical admission bounds, without a hard provider/process peak-memory or
preemption guarantee. New direct namespace presentation can differ from the
older tree writer. No throughput, memory-peak or encoded-size improvement is
claimed.

C++ concrete owning template XML is qualified for the checked mappings; unresolved
or unsupported generic contracts reject. Earlier checkpoints in this record
preceded the later cross-language mixed, extension, inheritance, lifecycle/digest,
projection, facet, identity, provider and HTML checks below. Their selected
reference contracts are now qualified; broader combinations remain governed by
the explicit matrix limits and OPC-R backlog.
Existing C++/Python rich tree mappings remain available under their documented
contracts; explicit token-required mappings reject unsupported rich graphs.

Schema grammar and generated file destinations did not change. Both extensions'
READMEs, the separate Visual Studio Marketplace overview, and generated-output
navigation assumptions were reviewed; no extension source/package changes or
installation/publication were needed. Native wire IDs, protocol values, payload
revisions and managed journal bytes are unchanged. The generic Python runtime
template is unchanged; XML-only policies are emitted only with XML support.

## Artifacts

`.gitignore` already covers `out/`, generated SDK outputs and caches. Task scratch
and qualification outputs stay under ignored build directories. Useful generated
sources, executable outputs, provider dependencies, OPC files and verification
logs are retained there; they are not source deliverables. Failed isolated build
configuration scratch is removed after checking its absolute workspace boundary.

## Additional cross-language reference qualification

The 21499/79211/99286 compiler checkpoints qualify C/Rust maps (33 each), variants
(161 each: 51 element and 110 xsi:type probes) and mixed content (21 each plus
three atomic helper-use gates). C compiles with /W4 /WX; Rust uses -D warnings.
Kotlin/Swift likewise pass the shared 161 variant probes and 21 mixed probes,
including empty type namespaces, shadowed prefixes, private native fragment
storage, significant text, nested independent policies and edited invalid output.
These suites compare native wire values and independently validate XML output.

Java/C#/Go pass 35 identity probes and five atomic binding gates each on 99286.
The separately authored raw JDK XSD oracle validates every document; lxml agrees
apart from its documented unsigned-plus lexical defect. Checks include exact
arbitrary decimal tuples, Boolean aliases, forward references, owner isolation,
seven independent index/admission bounds, native edits and staged publication.

JavaScript/TypeScript extension4 and HTML4 pass on 55059 alongside all 46 prior
SDK cases. Their lifecycle5 suite passes on 99286, including exact historical
uint64, dotted/float32 revisions, inherited private discriminators, immutable
magic, owning digest hex/extent boundaries and independent hexBinary validation.
Kotlin and Swift each pass 30 ordinary HTML decisions and exact resource policies
on 79211, independently compared with HTMLParser and libxml2 HTML trees.
Kotlin and Swift each pass 183 independently classified scalar/list/union/default,
exact numeric and Unicode pattern probes plus six atomic binding gates on 18323.
The large-program corpus raises its document text limit to 16 MiB to exercise the
separate 8 MiB regex input limit. Both SDKs pass that limit and the independent
regex work limit. Both SDKs additionally pass twelve Fraction-oracle binary32
midpoint/subnormal/signed-zero probes across XML and every native protocol.

Checkpoint 18323 qualifies C/Rust owning lifecycle and digest mappings with 53
probes each: exact uint64, dotted and binary32 revisions, private inherited
discriminators, raw/typed magic, nested layouts, exact hex bytes, one-MiB default
and custom recursive digest bounds, four native protocols and transactional C
failures. Swift passes the same 53 probes and owning digest policies.

Python readonly projections pass five real generated-code tests on 18323, with
constructor tripwires, live owner edits, weak expiry, nested/array reference
identity, target mappings, native-wire controls and export-only input rejection.
Its six identity cases and JavaScript/TypeScript's six identity cases also pass
the shared selected-nillable-key declaration rule.

Java/C#/Go approved ordinary HTML qualification passes 33 decisions per SDK on
8316 against two independent HTML oracles. Go and Swift prepared-provider tests
pass on 18323 using LibXml2 2.15.3 through the test-only C ABI bridge: complete
generated input/output callbacks, declared roots, five bounds, malformed UTF-8,
XML 1.0, provider mismatch publication failures, local/remote external schema
rejection and eight concurrent workers reusing one immutable schema. The native
loader fixture is Windows-specific; it is not an installed runtime dependency.
JavaScript/TypeScript's five real prepared-provider cases pass on 96850 using
pinned libxml2-wasm 0.7.1, including isolated worker reuse and unavailable/closed
provider behavior.

Java/C#/Go identity qualification expands to 45 probes and five atomic binding
fences per SDK on 18323. Selected `xs:key` fields whose declarations are nillable
reject even when their actual content is non-nil or empty; no-selected-node
controls and distinct unique/keyref rules remain accepted as appropriate.

Checkpoint 24319 qualifies the C/Rust scalar reference contract: 183 independent
scalar/facet/list/union/default/pattern decisions and six atomic binding fences
per SDK. C passes on 11010 with MSVC /W4 /WX; Rust passes on 24319 with release
compilation and -D warnings. Native binary32 input converts directly at its width,
including underflow to signed zero. Portable pattern programs include Unicode
and large prepared programs; no host regex dialect substitutes for their VM.
The C pre-normalization text admission follow-up passes in the later 82632 rerun
recorded below.

Checkpoint 72539 qualifies Kotlin/Swift identity with 45 decisions and five
atomic binding fences each. Java/C#/Go lifecycle passes 57 decisions per SDK,
including exact uint64 edges; C/Rust/Kotlin/Swift pass their 53-case native
lifecycle corpus. Readonly source references and scoped translator XML pass in
all eleven outputs' selected native contracts. Java/C#/Go constructor tripwires
and native array/nested identities pass on 73700; Kotlin/Swift owning references,
Swift weak expiry, scoped arrays and native wire controls pass on 72539.
JavaScript/TypeScript regression tests also pass on 73700 after correcting
synthetic map-entry lifecycle metadata and typed translator namespace binding.

C/Rust each pass the 30-case HTML corpus, two independent HTML oracles, six exact
resource policies and malformed UTF-8/semantic-event probes on 62032. Their real
prepared LibXml2 provider fixtures pass on the same checkpoint: generated
input/output validation, native-wire controls, atomic mismatch failures, five
resource policies, closed/unavailable providers, local/remote acquisition
rejection and eight concurrent workers sharing one immutable compiled schema.
The bridge is test-only and Windows-specific; it adds no installed dependency.

C/Rust identity qualification passes 45 independent decisions and five atomic
binding fences per SDK on 2748. Raw JDK and lxml XSD oracles check the documents
and outputs. Tests cover exact tuple/index admissions, nested owner isolation,
declared-nillable keys, forward/missing/wrong-scope references, edited output,
four native protocols and C publication failures. Existing limits and entrypoint
arities remain; independent policies use `with_identity_limits` companions.
These implementations add a separate bounded token validation scan over the
original input or completed private output, retaining no completed node tree.
The log is `out/build/x-tasks/native-identity-sdk.log`.

Kotlin/Swift extension qualification passes on 82632 in both strict actual SDKs.
Maintained `mobile_xml_extension_test.py` checks the independent lxml XSD and
expanded-name/mixed-content oracle, ten malformed/permission rejections,
required URI/prefix capabilities, explicit reserved model-namespace denial,
exact discovered preserved-byte admission and one-short failures, four stable
roundtrips at that limit, zero-byte skipping, native collection edits, nested
owner-local item budgets and four native protocol identities. Attribute-only
slots preserve scalar-text records. Three atomic binding fences per SDK reject
overflowing native counts and scalar-text child slots while preserving existing
outputs. Matching-prefix reuse avoids adding aliases to retained root scopes;
the first 3892 probe exposed that issue before its 7260 repair. Logs are
`out/build/x-tasks/mobile-extensions-kotlin.log` and
`out/build/x-tasks/mobile-extensions-swift.log`.

Java/C#/Go extension qualification passes 30 independent decisions per SDK on
82632, plus exact byte-bound/one-short checks, four stable roundtrips, repeated
collection edits, two independently bounded nested owners, live borrowed-record
sidecars, explicit option forwarding and four native protocols. A readonly root
translator rejects inventing required opaque payload. Two atomic native-counter
metadata fences per SDK pass, and retained-document readers/writers are tripwired
forbidden in the actual generated fixtures. Attribute-only scalar-text records
preserve QName attribute context and reject missing/unknown/nested content.
C# compiles without warnings. The log is
`out/build/x-tasks/portable/final-extensions.log`. These checks do not close
broader OPC-R02 preservation requirements.

Snapshot 82632 also passes the final C/Rust scalar, identity and readonly
projection reruns. Scalar qualification covers **183 independent probes and six
atomic binding gates per SDK**, including the C pre-normalization text bound;
identity covers **45 independent raw-JDK/lxml XSD decisions and five atomic
binding fences per SDK**. Projection checks live/scoped native references,
readonly getter export, independent XSD validation and exact values through all
four native protocols. C compiles with `/W4 /WX`; Rust uses release compilation
with `-D warnings`. Logs are `out/build/x-tasks/final-native-scalar.log`,
`final-native-identity.log` and `final-native-projection.log`. The later C/Rust
extension runtime repairs pass their separate fresh-compiler SDK run below.
These 82632 reruns preceded the final companion and shape checks below.

C/Rust positioned-extension companions pass **29 independent probes per SDK**
with the final `683c3d5f...` compiler above and unchanged `f6587122...` source
inventory. C compiles with `/W4 /WX`; Rust uses release compilation with
`-D warnings`. Rust owns native fields and private opaque state in
`XmlDocument<T>`, decoded by `decode_xml_document` and replayed by
`encode_xml_with_extensions`. C exposes initialized `<record>_xml_document`
companions with explicit init/free/decode/encode functions. Ordinary native
records and their four native wire formats exclude the sidecar. The suite
separately checks original/final provider callbacks and identity composition
against the complete document; failed C operations preserve the caller's
initialized destination and published output.

The maintained `native_xml_extension_test.py` corpus checks exact positions,
reserved core/model namespaces, capabilities, skipping and cumulative companion
item/storage limits. It checks four stable cycles at the exact preserved-byte
bound, empty/default namespace contexts, a 32-qualified-attribute document whose
exact final byte limit must admit private alias expansion, and 32 attributes
sharing a namespace URI longer than 512 bytes. A separate C probe rejects
fragmented modeled string content under a 31-byte native string limit. Private
known XML framing has its own bounded transport allowance; original native
value checks and complete original/final XML admission retain the caller's
limits. The corpus uses finite nested owner graphs; it does not add self-recursive
schema grammar. C/Rust extension companions with mixed/all, revision/magic,
readonly references, or nested extension owners in wrapped/nil/map/variant
payloads retain explicit generation fences. Logs are
`out/build/x-tasks/python/native-extension-c-final.log` and
`out/build/x-tasks/python/native-extension-rust-final.log`.

The final `683c3d5f...` compiler also passes refreshed C/Rust namespace, concrete
generic and wrapper shape consumers, four XML-name/XML-disabled compatibility
controls, and the live/scoped readonly projection consumers. Logs are
`out/build/x-tasks/final-native-shapes.log` and
`out/build/x-tasks/final-native-projection.log`. The C shape fixture accepts
either signed spelling of numeric zero, matching its value-space comparison.
