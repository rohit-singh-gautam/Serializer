# JVM wildcard declaration validation

Serializer 2.28.0 qualifies pinned local XSD `processContents="strict"` and
`processContents="lax"` independently from opaque preservation. The original
documents and every accepted emitted document are checked by an independent
libxml2 XSD processor through lxml. These checks use real generated Java and
Kotlin code, compiled and executed with their SDKs.

`test/xml_wildcard_validation_test.py` contains **142 decisions per configuration**.
Java and Kotlin each pass all four strict/lax × preserve/skip configurations:
**568 decisions per SDK**, with unchanged payloads checked through JSON and the
three native binary protocols. Both root-owning model classes are exercised.

The corpus covers global element and attribute declarations, scalar facets,
QName and QName-list scope, default/fixed values, required attributes, sequences,
all/choice groups, recursive strict wildcards, abstract elements and types,
admitted derived `xsi:type` selections, blocked derivations, nil markers and
attribute validation on nilled complex content. Unknown strict declarations
reject; unknown lax declarations follow the selected preservation policy.
Explicit empty attributes retain their own value rather than taking a schema
default. No-namespace attributes preserve the original owner's default namespace
when validating QName values.

One nested declared-content fixture in each configuration also checks both
decoding and encoding against the same caller-owned operation. It measures
cumulative work, temporary allocation and preserved-byte admission, accepts each
measured exact bound, and rejects a bound smaller by one while preserving the
caller's original exception object. Cancellation also returns the original
exception. Skip configurations consume no preserved bytes and accept a selected
preservation quota of zero. Failed validation leaves native payload bytes
unchanged, and a later independent operation succeeds.

Both SDKs also reject a deliberately edited preserved declared numeric child
before output and retain the original native payload. Kotlin's declaration names,
codecs and admitted derived-type arrays are immutable module metadata; lookup
does not allocate a fresh type table during an operation. A fresh Kotlin
four-mode replay after that change passes the same 568 decisions and admission
checks using checkpoint SHA-256
`494D07F0F1A91622D9B67709C4EC4F3CE83CC68E61AD0E0DC25E698A106F7DAC`.

Kotlin additionally passes all nine `all`/choice/explicit-choice × skip/lax/strict
compositor variants (**294 decisions**) and the foreign unknown-policy fallback
corpus with both five-field and 300-field owners under preserve and skip
(**192 decisions**). These real SDK runs also retain the four native protocol
checks. Logs are `out/opc-group-extensions-kotlin-*-final.log` and
`out/opc-unknown-policy-kotlin-*-final.log`.

`test/xml_wildcard_identity_policy_test.py` passes Kotlin's four direct/nested
declared-global identity documents, rejects duplicate keys, and honors a caller's
independent `maxTuples=0` on decoding and encoding. This probe used checkpoint
`5F4804F62D90870D7DDD7343AA7D4C4F795C89E6E35F7EC0436CDBA58FED4EFA`;
the retained log is `out/opc-wildcard-identity-kotlin-final.log`.

`test/java_xml_declaration_names_test.py` additionally compiles four authored
classes named `SrlXmlDeclaration`, `SrlXmlDeclarationType`, `SrlXmlDeclarations`
and `SrlXmlDeclarationPolicy` alongside the full strict/preserve corpus. Their
authored Java names, JSON names and default strings remain unchanged while
private runtime helpers select distinct names. This actual SDK probe passes.

The four-mode run used the coherent compiler checkpoint SHA-256
`EF8BEDC1E8BA2C520EA4856474D78CDE63E6DFC5446BF02A60FE8068A72C76DD`.
Retained local logs are `out/opc-wildcard-validation-java-*-admission.log`,
`out/opc-wildcard-validation-kotlin-*-final.log` and
`out/opc-wildcard-validation-java-helper-names.log`. Generated SDK directories and
diagnostics stay under ignored `out/`.

Private declaration DTOs reuse the ordinary generated XML validators and borrow
the original operation; they do not publish native payload fields or acquire a
second opaque preservation store. The inventory comes from the pinned schema
bundle, with no runtime schema retrieval. This record does not claim arbitrary
XSD grammar beyond the compiler's documented import/binding capabilities.
