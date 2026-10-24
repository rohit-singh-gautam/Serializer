# Application responsibilities and will-not-fix decisions

[Pending requirements](../pending.md) · [Integration guide](../guides/usage.md)

Serializer supplies schema compilation, generated codecs and the documented
native runtime mechanisms. The consuming application supplies its domain data,
security decisions and external services. An application responsibility is not
an unimplemented Serializer feature.

Closed **will not fix — application or maintainer responsibility** decisions are
recorded here and excluded from the pending list and implemented feature count.
Historical `[-]` markers identify a disposition, not a passing test or feature.
For mixed requirements, only the application-owned responsibility is excluded;
missing compiler, codec, runtime or editor behavior stays pending.

## Disposed requirements

| ID | Responsibility and decision | Serializer behavior retained |
| --- | --- | --- |
| `MIG02` | **Will not fix — application responsibility.** The application selects and interprets its historical layouts, custom containers, outbox meanings, lease transitions and migration business rules. Serializer cannot infer an unspecified historical application format or safely choose an epoch transition. Supply an explicit application migration using the supported conversion/import APIs and test its publication/recovery boundary. | Existing explicit fixed/compact identity and historical managed-save/raw-model conversions remain supported. This decision does not excuse regressions reading supported Serializer-owned formats or remove their compatibility guarantees. |
| `MG06` | **Will not fix — application responsibility.** Cross-service participants, file/network side effects, business compensation, authentication and exactly-once delivery belong to an application coordinator. Persist an application-defined intent in the managed transaction, perform the effect through an idempotent service, and reconcile acknowledged or uncertain outcomes. Serializer will not invent or execute those business effects. | Atomic local managed publication, write-before-publication journal ordering, recovery and indeterminate-outcome fencing remain runtime responsibilities. A local transaction does not make an external service part of its atomic commit. |
| `REL02` | **Will not fix — maintainer responsibility.** Publishing releases, Marketplace listings, credentials, signing authority and upstream package-port submissions are separately authorized repository operations, not runtime features. Maintainers select the release, review artifacts and authorize publication. | Build/package generation, package identities, version consistency and reproducible verification remain repository responsibilities. This disposition does not prevent a separately requested publication task. |

## Security and authorization

The host authenticates the user, binds the trusted principal to each request,
constructs and persists its policies, identifies permission-bearing application
fields, and controls policy administration. These responsibilities are
**will not fix in Serializer**. An API parameter containing a principal or
session identifier is not proof of authentication. Policy values and signing or
encryption keys must not be inferred from decoded, untrusted payloads.

Serializer enforces its installed complete-candidate authorization boundary
before native-store publication and journal I/O. Its inherited rules distinguish
content, structure and explicit policy permissions; source and destination
ownership checks prevent a move or deletion from bypassing a protected subtree.
Use the same `authorize_managed_candidate` helper in the authority's trusted
acceptance callback. The host must request `authorization_permission::policy`
before its policy-administration operation. Install the policy on a validated
store before exposing edit/import entry points. See the
[authorization contract](../managed/authorization.md) and its
[executed tests](../verification/verification-managed-authorization-2026-10-21.md).

Consequently, `MG03`'s Serializer-owned boundary is implemented; host principal
binding, policy storage and administration are excluded responsibilities, not a
request for a second authentication or policy database inside the runtime.

## Supported OPC profile and optional exclusions

The selected ECMA-376 Part 2 fifth-edition profile uses ASCII logical part names
and contiguous physical ZIP items. Non-ASCII logical part naming (§6.2.2.2),
piecewise interleaving (§7.2.4) and production of growth hints (§6.2.4) are
explicit optional profile exclusions. They are not represented as implemented
features or missing mandatory conformance. Outer package locators may still
contain UTF-8 IRIs through the [Pack-IRI API](../serialization/pack_iri.md).

These exclusions are library support decisions, separate from application-owned
domain semantics. Unsupported forms must reject precisely. Opaque growth-hint
data may be retained by explicit physical metadata replay without being applied
as an allocation instruction. Save uses the documented
[complete-rewrite ZIP contract](../serialization/zip_metadata.md), which does not
promise in-place editing or byte-identical physical records. Mandatory input
consumption and supported logical part preservation remain Serializer obligations.

## Mixed requirements that remain pending

| Area | Application responsibility: will not fix | Serializer responsibility: remains required |
| --- | --- | --- |
| XML/XSD and typed packages | Choose the domain vocabulary, native/XML associations, business key/reference meaning, profiles and extension semantics. Supply domain-specific conformance corpora. | Preserve declared identities, enforce supported mappings, reject ambiguity precisely, validate generated references and qualify supported standard vocabularies and runtime limits. |
| Durable field addresses | Define the application's persistent address identity, namespace, lifetime, versioning and resolution policy. | Broader durable-address XML requires an explicit supported contract; unsupported combinations reject. The completed [OPC-R10 borrowed-output qualification](completed_requirements.md#opc-r10) preserves that rejection boundary; [XML-B15 is closed](xml_package_backlog_resolution.md#xml-b15) with durable-address XML deliberately disabled; no address codec is claimed. |
| Collaboration | Supply authenticated network transports, service discovery, access policy and presence UI. | Preserve protocol/session ownership, leases, retries, journal ordering and deterministic reconnect/fault behavior. `MG05`'s native session/cache qualification is [executed on Windows and Linux](../verification/verification-managed-capabilities-2026-10-21.md). |
| Signatures | Supply keys, trust anchors, signing authority and acceptance policy. | Canonicalization/signature provider interfaces and signed-package editing qualification remain missing work in `OPC-OPT02`. |
| Views and native code | Keep borrowed source storage alive and unmodified for the stated lifetime; compile application native blocks and native-only expressions with the selected target toolchain. | Enforce representable bounds and documented ownership/error contracts; preserve diagnostics and qualify supported mappings. `BE03` and compiler/backend feature gaps remain pending. |
| Providers and performance | Select and deploy optional provider dependencies, supported application workloads and the host's process-wide resource limits. | Validate provider contracts, enforce documented cumulative admissions, test cancellation/failure behavior and measure claimed library profiles. Logical byte admission is not a process-memory ceiling. |
| Restricted HTML | Parse or render arbitrary HTML/unrestricted XHTML through an application-selected HTML library when required. | Maintain the explicitly supported restricted adapter and reject unsupported structures. The [30-tag profile closes XML-B24](xml_package_backlog_resolution.md#xml-b24), including sub/sup phrasing containers; arbitrary HTML remains excluded. |

Unimplemented native managed engines, paging/lazy storage, allocator propagation
and compiler output layouts are Serializer features. Unsupported XML graph/backend
combinations remain explicit capability limits, as recorded in the resolution
ledger. They are not reclassified as application responsibilities to reduce the count.
