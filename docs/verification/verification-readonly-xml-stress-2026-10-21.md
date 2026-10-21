# Large readonly XML exports — 21 October 2026

Compiler/runtime **2.24.0**, schema language **1.21.0**. This checkpoint broadens
`OPC-R10` qualification and leaves that parent open. It does not define a durable
address XML representation or qualify every possible readonly graph.

## Executed contract

All eleven actual Windows outputs traverse **25,000** source collection values
through scoped translator XML output. C/Rust include a borrowed digest descriptor;
the other fixtures include a nested record and a target-owned string default.
The collection stays associated with its original owner throughout the scope.
Native field IDs, model APIs and payload formats are unchanged.

Each consumer checks original storage identity, complete output at the exact byte
limit, rejection one byte below that limit, and a successful fresh traversal after
failure. Python, JavaScript, Java, C# and Go install existing generated-constructor
tripwires during output to reject materialization of an owning target. C++ checks
array/nested-record addresses and traverses with a zero tree-byte allowance.
Rust checks borrowed descriptor addresses; C checks descriptor pointers. Swift
compares the original array's buffer address captured before its exclusive scoped
borrow with the projected buffer address during traversal. Kotlin checks original
collection and nested object identity. These checks do not claim zero allocation:
output buffers, leaf formatting and independently prepared defaults can allocate.

All consumers preserve source values and verify native encodings or exact decoded
collection values. C++/Python/JavaScript/Java/C#/Go/Kotlin/Swift exercise nested
records/defaults; C/Rust compare all four native protocol outputs with the selected
owning collection/digest. The existing small-graph controls retain shared/weak
owner lifetime, unbound/expired references and export-only input rejection.

| Consumer | Result and independent boundary |
| --- | --- |
| C++ | 6/6 projection GTests, including the large array, zero retained XML tree, exact/one-tight output and mid-traversal callback failure; owning token decode checks every value |
| Python | 7/7 maintained tests; 25,000 values, constructor traps, all native protocols and independent lxml expanded elements/value inspection |
| JavaScript/TypeScript | 7/7 maintained tests, strict TypeScript consumer, actual Node traversal, constructor traps, native controls and independent lxml inspection; real libxml2-wasm provider completion control also passes |
| Java/C#/Go | All three maintained consumers pass; independent XSD validates each original large output and independently authored expected arrays/nested/default values match |
| Kotlin | Large source/object identity, exact/one-tight bytes, original cancellation object, four native protocols and independent XML value inspection pass |
| Swift | Array buffer identity during a legal scoped borrow, exact/one-tight bytes, cancellation case, fresh recovery, four native protocols and independent XML value inspection pass |
| C/Rust | Both actual SDK consumers pass at two and 25,000 collection items, borrowed collection/digest descriptors, exact/one-tight bytes, fresh recovery and four native protocols; independent XSD validates original large outputs |

C++, Java, C#, Go, Kotlin and Swift additionally interrupt the large traversal at
the hundredth operation callback. The previous caller result remains unchanged
or no owning output is returned, according to the public API. The original caller
exception/object/case is preserved. These are cooperative callbacks; they do not
establish process memory ceilings or hard provider preemption. C/Rust large output
uses the ordinary public byte bounds; this record does not add a large readonly
operation-descriptor cancellation qualification for those APIs.

## Source and tools

The compiler executable SHA-256 is
`7fa11fc8a0ea3b1c784481124a0cd129694f41335e634fe3b49c59f2c212fef7`.
The root Windows configuration builds through `make.ps1`/CMake with MSVC
**19.51**. SDKs include Python **3.14**, Node **22.23.1**, TypeScript **5.9.3**,
Go **1.26.4**, Oracle JDK **26** with generated Java release **17**, .NET SDK
**10.0.402** targeting .NET **8**, Kotlin **2.2.21**, and Swift **6.4.0**.
C links the configured **libxml2 2.15.3** provider. Rust uses the existing locked
example dependencies and its retained offline Cargo cache. The Node prepared
provider is **libxml2-wasm 0.7.1**. No dependency version or lockfile was changed.

Useful logs and generated verification consumers remain under ignored
`out/build/pending-active-2026-10-21/`: `projection-cpp-stress-final.log`,
`projection-python-stress.log`, `projection-javascript-stress.log`,
`projection-portable-stress.log`, `projection-mobile-stress.log`,
`projection-swift-stress.log` and `projection-native-stress.log`.
The combined mobile log records Kotlin success followed by an invalid Swift
test-probe borrow; the separate final Swift log records the corrected passing run.

Initial host/probe failures are not passes: C++ first expected the wrong standard
exception for a byte limit; the corrected test requires `std::length_error`.
Its interruption probe also initially compared `std::exception_ptr` storage
identity, which is not preserved by MSVC's exception copies. The final probe
compares a caller-owned identity token carried by the propagated exception.
Swift's original probe read the source during its exclusive projection scope;
the corrected probe captures an address before borrowing and never reads the
source inside that scope. Windows C discovery required the available Ninja
Multi-Config host to match the existing example runner's `Release` layout.
The Node fixture initially lacked its optional prepared provider; the replay uses
the already installed pinned package. No runtime check was weakened for these
corrections.

## Remaining acceptance work

The parent still requires broader readonly XML graph and resource qualification,
including namespace-sensitive scoped projections and relevant combinations of
containers, lifecycle and namespace companions across backends. Named durable
addresses continue to reject XML generation; native address codecs retain their
separate document/object/generation/path contract. XML value export emits the
selected payload and does not serialize those address records. See the
[value-reference guide](../schema/value_references.md),
[durable address contract](../schema/references.md), and
[pending requirements](../pending.md#opc-and-3mf-requirements).
