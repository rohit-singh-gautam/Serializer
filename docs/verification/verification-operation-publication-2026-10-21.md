# Operation completion and publication verification

This record qualifies concrete operation-boundary fixes on Windows. It is
additional evidence for `OPC-R11`, `XML-B05` and `XML-B21`; it does not claim
completion of all combined graphs, providers or deployment limits in those
requirements.

## Generated Python prepared-provider completion

Generated owning XML encode/decode and readonly translator encode now poll the
borrowed operation immediately before and after a caller-prepared schema
validator. The standard provider already polls internally, but a supported
provider subclass can finish actual validation and then make cancellation or a
deadline observable before returning. The generated completion fence rejects
that operation before returning output or constructing an input candidate.
An exception raised by the provider retains its original identity.

`test/python_xml_projection_test.py` executes a prepared `lxml` validator
subclass that signals cancellation after successful validation. Owning output,
readonly output and input all reject with the original caller exception. Input
candidate construction is forbidden by a mock, all four native protocol bytes
remain unchanged, and a simultaneous provider failure preserves its own error.
All **6 projection tests pass** against the final compiler below.

## Generated C ZIP candidate publication

The generated C directory-open and part-read helpers previously replaced the
caller's destination before their final cancellation poll. A failure at that
poll could therefore return cancellation after changing the previous result.
Both helpers now poll while their fresh candidate remains private; failure frees
only that candidate, and successful completion publishes it afterwards.

The C consumer first measures a successful operation's poll count and then
signals cancellation at that final poll. Directory-open preserves the previous
archive allocation, and part-read preserves the previous output pointer, size
and bytes. The new test fails against the previous frozen compiler
`34e67ca28294018920cc54cd26b4ae6f67ed1f42ef67c4ed7c9aac8ef846cf7b`
with `late directory cancellation retains old archive`; the corrected compiler
passes. This comparison distinguishes the new regression from an assertion that
merely mirrors the implementation.

The actual MSVC 19.51 C11 consumer compiles with `/W4 /WX` and passes **165
independent package fixtures and 16 application fixtures**, including the
separate UTF-16, Annex B and markup-compatibility controls. Its existing 68 encode
and 85 load allocation-failure rollback cases also pass.

## Readonly JavaScript/TypeScript, Kotlin and Swift completion

Readonly translator XML exports now apply the same borrowed-operation fences
immediately before and after their validator returns. Inspection of freshly
generated JS, Kotlin and Swift confirms both fences in the translator entry
points; owning-codec fences alone do not cover those distinct entry points.

The actual JS/TS projection suite passes **6 tests**, including a real prepared
`libxml2-wasm` 0.7.1 XSD validator wrapped to signal cancellation after successful
validation. The readonly export must throw the exact caller error instead of
returning its XML. A strict TypeScript consumer also verifies the typed callback
and cancellation path. Existing constructor tripwires, weak-owner expiration,
QName scopes, nil/empty distinctions, export-only input rejection and all native
protocol controls continue to pass. The suite uses Node 22.23.1, TypeScript
5.9.3, `saxes` 6.0.0 and an independent `lxml` output oracle.

`test/mobile_xml_projection_test.py` also passes with actual native Windows
Kotlin 2.2.21 and Swift 6.4 SDKs. Each prepared validator-interface callback decodes
the actual projected XML, checks its selected fields, then makes cancellation
observable on the shared operation. Both readonly exports reject that completed
operation. These mobile callbacks exercise the public validator interface and
real generated XML decoder; they do not represent an additional external XSD
engine replay. The same consumers verify live references, borrowed array/nested
getters, defaults, export-only input rejection and native protocol identities;
Swift additionally checks weak-owner expiration. Independent `lxml` inspection
checks both exported documents. Kotlin uses `-Werror`; Swift uses
`-O -warnings-as-errors` with its native Windows SDK.

The JS and Kotlin new cancellation controls fail against the earlier frozen
`fdd015a516f50d11f6cbd88140053fbc718ce5a337b7a612d40b2a813f0c5d04`
compiler: JS reports a missing expected exception and Kotlin rejects the
unexpectedly successful completed operation. Those counterfactual logs remain
under `out/build/pending-runtime-providers-completion-2026-10-21/` as
`js-projection-before.log` and `mobile-projection-before.log`.

The positive readonly replays use compiler SHA-256
`93906e87453be863a47fc29915192cf5c1fc06f7cfec07aa6a2cfdba13131e35`,
frozen at
`out/build/pending-xml-completion-2026-10-21/compiler-opc-identity/serializer.exe`.
Their final logs are `js-projection-final.log`, `kotlin-projection-final.log` and
`swift-projection-final.log` beneath that ignored build directory. Generated
fence-inspection snippets and useful SDK outputs remain there for replay.

## Public cancellation from separate libraries

The additive cancellation policies are callable from separately compiled
applications, without access to generated internal operation hooks:

| Output | Public policy API | External consumer boundary |
| --- | --- | --- |
| C# | `Schema.Limits.WithCancellation(Action)` | Generated library assembly and separate executable assembly |
| Kotlin | `XmlLimits.withCancellation(() -> Unit)` | Generated library JAR and separately compiled consumer JAR/module |
| Swift | `XmlLimits.withCancellation(() throws -> Void)` | Generated DLL/Swift module and separately compiled importing executable |

`test/xml_public_cancellation_test.py` passes all three actual Windows SDK
profiles against the immutable 2.22.0 compiler SHA-256
`d8d2b683d4490dca4609f9aa70fe39f347dd6c7150df08d3e9f68a07ad00a610`,
retained at
`out/build/pending-large-xml-2026-10-21/compiler-wrappers/serializer.exe`.
The C# consumer also checks that generated models and its entry point belong to
different assemblies; Kotlin checks the distinct JAR code origins. The Swift
consumer imports only the compiled public module and links its DLL.

Each consumer checks copied bounds, successful stacked public callbacks,
owning encode/decode and readonly projection completion, no successful result
after cancellation, the exact original caller error, sticky first failure,
unchanged native protocol bytes and recovery with a fresh policy. Real prepared
System.Xml, JAXP and libxml2 validators run in C#, Kotlin and Swift respectively.
The sealed C# provider's final interruption poll is selected by a successful
baseline count; Kotlin/Swift wrap the real provider and signal cancellation only
after it accepts the document. Provider rejection retains precedence over a
subsequent cancellation poll. An independent Python XML parser checks the final
emitted expanded names and values.

The returned policy shares its first failure through nested captures, and is
terminal after failure. Applications create a fresh wrapper to recover and must
not invoke one wrapped policy concurrently. The callback can check cancellation
or a caller-selected deadline; the library does not choose the application's
clock or deadline policy.

Actual toolchains are .NET SDK 10.0.402 targeting .NET 8, Kotlin 2.2.21 with JDK
26, and native Windows Swift 6.4. Kotlin compiles library and consumer separately
with `-Werror`; Swift uses `-O -warnings-as-errors`. Exact commands, generated
hashes, separate library/consumer logs and acceptance reports remain under
`out/build/pending-xml-completion-2026-10-21/public-{csharp,kotlin,swift}/`.
These are public-interface and completion checks; the separate
[large interruption record](verification-large-xml-2026-10-21.md) owns later
large-message findings and their compiler checkpoints.

## Parent acceptance checkpoint

At the earlier `fdd015a5`/`93906e87` checkpoints, `XML-B05`, `XML-B21` and
`OPC-R11` remained open. The prior
[eleven-output public composition replay](verification-native-xml-operations-2026-10-21.md)
already establishes actual same-ledger selected-codec composition, exact and
one-tight reference/work/allocation quotas, middle-role interruption and failure
publication controls. The current provider and readonly checks add concrete
completion-fence coverage; they do not substitute for large-workload interruption
execution.

At that checkpoint, large mid-operation cancellation/deadline/recovery adapters
had not been executed for C, Rust, Go, Java, C#, JavaScript, TypeScript, Kotlin or
Swift. Later actual executions and their remaining limits belong to the linked
large interruption record; this historical list is not its current status.
Successful large throughput measurements alone do not demonstrate
that interruption behavior. The separate large C++/Python interruption controls
cover owning XML rather than a large combined OPC/DEFLATE, preserved/reference
and final-publication operation. Existing Java/C#/Go readonly projection tests
exercise limits and provider rejection, but do not contain the new late-success
provider cancellation control used above. These are concrete qualification gaps,
not evidence that those backends necessarily reset their ledgers or publish a
failed candidate.

C/Rust caller callbacks can carry a host-clock expiry; absence of a built-in
deadline field is not itself a missing engine mechanism. Host selection of a
clock, forced provider preemption and process-wide memory ceilings retain their
documented application/deployment responsibilities. The recommendation to keep
these three broader parents open at that checkpoint rested on unexecuted supported-operation checks,
not on demanding those host policies from Serializer.

## Replay and limits

The initial C and Python completion replays use compiler SHA-256
`fdd015a516f50d11f6cbd88140053fbc718ce5a337b7a612d40b2a813f0c5d04`,
frozen at
`out/build/pending-xml-completion-2026-10-21/compiler-publication/serializer.exe`.
Native processes inherit Windows error-dialog suppression. C uses the existing
release libxml2/zlib providers; Python uses the real installed `lxml` provider.

Useful replay output remains under that ignored build directory:

- `c-publication/verification.json`, `commands.json`, `compile.log` and
  `acceptance.log` retain the successful actual SDK run.
- `c-publication-before.log` retains the expected old-compiler failure; its
  unused build directory was removed.
- `python-provider-fences-final.log` retains the six passing Python tests.

Focused source review found these publication/completion gaps rather than a
demonstrated fresh-ledger reset. Existing composition tests and their broader
qualification inventories remain necessary. No Linux replay, forced interruption
inside an external provider, peak provider-memory bound, or complete eleven-output
operation-graph claim is made by this record.

## Combined-operation qualification

The [2.18 public composition inventory](verification-native-xml-operations-2026-10-21.md)
already executes selected-codec same-ledger composition in all eleven outputs,
including deliberate repeated decoding, exact/one-tight cumulative limits,
middle-operation cancellation and atomic Save. This batch identifies completion
and publication holes rather than a demonstrated new ledger reset.

At the intermediate checkpoints, `XML-B05`, `XML-B21` and `OPC-R11` still required
a large combined OPC/DEFLATE/preserved-reference/publication chain. Large owning XML
timing and interruption checks are separate evidence; they do not execute that
combined graph. The external C# controls and later Java/Go readonly provider
controls below cover the previously missing late-success projection boundary.
The subsequent [large owning package qualification](verification-large-package-operations-2026-10-21.md)
now completes that selected chain in all eleven actual SDKs, including cumulative
quotas, scoped identities, preservation, cancellation/real deadlines, atomic Save
and fresh recovery. C/Rust caller-supplied cancellation/error callbacks carry
host-clock expiry without requiring a new native deadline field.
Host-enforced provider preemption and process-wide memory ceilings remain
deployment responsibilities; enforcing the documented logical ledger and
transactional publication remains Serializer's responsibility.

The large-adapter follow-up identified a public API gap in C#/Kotlin/Swift.
The later public `WithCancellation`/`withCancellation` policy wrappers and external
library tests above close that interface gap. Package hooks still compose the
existing ledger; the wrappers add a public standalone policy without exposing
internal budget types. Their separate full-chain qualification is linked above.

## Final public-library checkpoints

The separately compiled Swift DLL/module and importing consumer pass again
against the compiler containing the deferred FoundationXML parser-failure fix:
`6d3537f66e40460956b70d45c1cbe47ad11a22af7e8a055485fef30124c7d82b`,
retained at
`out/build/pending-large-xml-2026-10-21/compiler-swift-final/serializer.exe`.
The generated Swift source SHA-256 is
`12c7685c4e64ae7ac1035ad3a176c84025defc9f9a09cbca7718b9f6d301d6cf`.
The real libxml2 provider, owning encode/decode, readonly late completion,
original/sticky errors, native bytes and fresh-wrapper recovery checks all pass.
This is the actual external-module replay after that Swift fix, separate from
the earlier `d8d2b683` checkpoint and from the large SAX interruption replay.

The Kotlin external-library/consumer JAR pair also passes again after the
`kotlin.Unit`/`kotlin.Throwable` qualification repair, using final public compiler
SHA-256
`69c2f1a263be717de5338653c8026ddc6ea69c2e35e941092e418dce62be4421`,
retained in `out/build/pending-large-xml-2026-10-21/compiler-public-final/`.
Its generated Kotlin source SHA-256 is
`6d3be81a8ea2539e5951a8cde12ed53fbe7b61b6b99b9719e617b2f0c4577bef`.
Real JAXP validation and all external public-policy checks pass.

Final compiler `69c2f1a2` separately regenerates byte-identical C# source to the
executed `d8d2b683` C# library and byte-identical Swift source to the executed
`6d3537f6` Swift library. The C# source SHA-256 remains
`571826b88265649fcf070af267a9c8a61f8a3d837248c916c76e1666ba3d8fb4`.
This generation comparison is not reported as an additional SDK execution.

Final reports, exact commands, library/consumer compile logs and acceptance logs
are retained under
`out/build/pending-xml-completion-2026-10-21/public-swift-final/` and
`public-kotlin-final/`. The C#/Swift generation bridge and its comparison manifest
are retained in `public-final-source-bridge/`. Earlier checkpoint reports remain
distinct in the original three `public-*` directories.

## Java and Go readonly provider completion

The maintained `test/portable_xml_projection_test.py` now executes readonly
late-success cancellation with real prepared JAXP and libxml2 providers. Java's
sealed provider is exercised through its public operation callback. A standalone
validation measures that provider's callback count; a stack-qualified callback
then marks cancellation at its final callback, after actual JAXP validation, and
returns normally. The next caller-operation poll must throw the original marker.
The readonly export must leave its caller's output holder untouched. This avoids
replacing the prepared validator with a successful stub.

This control fails on compiler SHA-256
`69c2f1a263be717de5338653c8026ddc6ea69c2e35e941092e418dce62be4421`:
the readonly export returns bytes without observing cancellation after the
prepared provider returns. The counterfactual is retained in
`out/build/pending-xml-completion-2026-10-21/java-go-projection-before.log`.
`src/java_writer.cpp` now emits operation polls immediately before and after
prepared validation in its shared owning/readonly encode path and owning decode
path. A provider exception propagates directly before the completion poll.

Go wraps a genuine prepared libxml2 bridge validator, marks caller cancellation
only after that validation succeeds, and requires nil bytes plus the exact
caller error. A separate control returns an original provider error at that
same boundary and verifies cancellation does not replace it. Go already passes
these controls on the old compiler; no Go production change was needed.
Both consumers also verify fresh-operation recovery while constructor tripwires
for owning projection targets remain active, and retain their existing native
wire and independent XSD checks.

Both actual SDK consumers pass with compiler SHA-256
`69da7b7057f9d60d2ce635886208a51c183eab9734ed23e34d26fb7da4c9846f`,
frozen under
`out/build/pending-xml-completion-2026-10-21/compiler-java-completion/`.
Replay with the installed Python `lxml` dependency on `PYTHONPATH`:

```powershell
python -B test/portable_xml_projection_test.py --compiler out/build/pending-xml-completion-2026-10-21/compiler-java-completion/serializer.exe --build out/build/pending-xml-completion-2026-10-21/java-go-projection-final --language java,go --bridge out/build/ReleaseWindows/test/xml_provider_bridge.dll
python -B test/portable_xml_provider_test.py --compiler out/build/pending-xml-completion-2026-10-21/compiler-java-completion/serializer.exe --build out/build/pending-xml-completion-2026-10-21/java-completion-provider-final --language java
```

The separate Java prepared-provider suite also passes bounds, concurrency and
no-fetch checks. The final readonly run retains compiler/generated-source hashes
in `java-go-projection-final/verification.json`, with actual acceptance output in
`java-go-projection-final.log`. The provider regression output is retained in
`java-completion-provider-final.log`. These completion controls do not constitute
the large combined OPC/DEFLATE/preserved-reference/publication qualification.

## Python Save publication callbacks

The large-package follow-up exposed a separate final-boundary defect in
`_pkg_save`: a caller's `before_publish` hook could arm the budget's cancellation
predicate and return normally, after the last policy check. Replacement then
succeeded. An `after_publish` hook could likewise arm cancellation without the
operation reporting its existing post-publication uncertainty contract.

The maintained regular Python package test now covers both cases, using actual
generated `ProfileArchive.save` and public `PackageBudget`. On immutable compiler
`dea245fd49925f789d3d4444750d21501bdd726656fcf7372c046dfde4af6499`,
the **44-test** suite fails exactly those two subcases because no error is
raised. The counterfactual log is retained under
`pending-runtime-providers-completion-2026-10-21/python-publication-before.log`.

`src/python_package_runtime.inc` now checks the budget immediately after each
hook returns. A throwing hook retains precedence. Before publication, cancellation
leaves the old destination unchanged; after publication, it raises
`publication_uncertain` with the cancellation cause while retaining the new
complete archive. Both paths remove private staging output. Fresh-operation Save
and reopen succeed after each failure.

The integrated post-fix compiler SHA-256 is
`82019ab1cc5088111bc7c562a5b29ba5899f660a5143b0120abcf9736dc3a061`.
All **44 Python package tests** pass as part of six affected CTest targets;
all six pass in **169.78 seconds**. This fast default-suite regression is separate
from the larger repeated identity/quota qualification; the separate passing
large record closes the selected combined-operation gates.
