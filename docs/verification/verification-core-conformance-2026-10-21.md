# Official Core 1.4 corpus conformance

Date: 2026-10-21. **OPC-R05** qualifies the selected published Core 1.4/XML XSD
bundle and the currently admitted C++/Python owning codecs against independent
official Core-only inputs. This is distinct from complete application or package
conformance and from admitting rich Core codecs in the other nine languages.

## Pinned independent inputs and oracles

The [official 3MF conformance suite](https://github.com/3MFConsortium/test_suites)
is pinned at revision `f483d3beee069e7adbac125d1f7baa36b5b3e5bf`, Suite3 Core-only:
**175 archives, 133 positive and 42 negative**, containing **175 model parts** and
712,069,884 original XML bytes. The complete
[manifest](../../test/resources/3mf_core_1_4/official_suite3_manifest.json) retains
every archive/model SHA-256 and independent verdict. Original archives remain in
ignored build storage; the upstream BSD license is retained beside the manifest.
Discovery uses declared model MIME types, including arbitrary/no extensions and
case aliases; identifiable `.model` payloads in package-invalid negatives are also
inspected. A filename extension alone does not select the corpus.

The [published Core 1.4/XML bundle](../../test/resources/3mf_core_1_4/README.md)
is unchanged from the earlier pin except for its documented local XML import
location. Independent **.NET 10.0.13 `XmlSchemaSet`** validates every original model
stream, including the largest 380,469,865-byte model. It uses the original finite
`maxOccurs="2147483647"`, with no grammar widening or network/DTD resolution.
Independent **xmlschema 4.2.0 / elementpath 5.1.4** agrees on all 111 original parts
at most 8192 bytes, using the same unchanged bundle. The .NET oracle also validates
every actually emitted C++ and Python model. Neither oracle uses Serializer's XSD
importer, generated schema or libxml2 grammar projection.
Maintained oracle controls reject a foreign document root and an external DTD,
and accept the known valid local model. A missing root schema cannot silently
turn into an unvalidated successful verdict.

## Results

| Original model disposition | C++ | Python |
| --- | --- | --- |
| Decode, metadata edit where present, encode, decode again | **156 passed** | **156 passed** |
| Schema-invalid original rejected | **6 passed** | **6 passed** |
| Valid original exceeds cumulative XML text/structural policy | **4 resource rejections** | **4 resource rejections** |
| Original exceeds the explicitly selected 16 MiB input policy | **9 outside selected profile** | **9 outside selected profile** |
| Total original parts accounted for | **175** | **175** |
| Independent XSD validation of actually emitted models | **156 passed** | **156 passed** |

The common owning workload explicitly selects 16 MiB input/output/text limits,
one million structural entries/attributes and depth 64 through each backend's
public limits. Backend accounting categories remain distinct. These are selected
bounds, not a claim that Python's default byte limit is 16 MiB. The four cumulative
resource cases are `P_XXX_0903_04`, `P_XXX_0906_01`, `P_XXX_0906_04` and
`P_XXX_0909_05`. Nine oversized originals are validated by the streaming independent
oracle and explicitly excluded from this owning-consumer byte profile; their
geometry is not silently reduced or split to obtain a pass.

C++ additionally compares every persistent native field through binary payload
equality before/after the edited XML round trip, including geometry and resource
identities. Both consumers process 55,256,749 original XML bytes in their 156
successful cases. Emitted XML totals are 40,198,350 bytes for C++ and 95,966,738
bytes for Python. Namespace presentation differs while both emitted graphs pass
the independent original XSD. Corpus discovery was corrected during qualification
to include four arbitrary-extension cases; the original 171 cases and four added
Python cases use the same immutable compiler/generated module, and the final C++
replay covers all 175 together.

Fresh import/generation attempts exercise all eleven backend selectors. **C++ and
Python bind** the published Core graph. C, C#, Java, Kotlin, Go, Rust, Swift,
JavaScript and TypeScript reject precisely with the unsupported XML
choices/wildcards/extension-policy diagnostic. These nine rejections are admission
checks, not executed rich-Core SDK support. Ordinary owning XML support in those
languages is qualified separately. Existing XSD import/binding tests retain precise
rejection of unsupported external grammar shapes.

## Upstream labels and ownership

The independent original XSD rejects three official **positive** fixtures:

- `P_XXX_0335_03` and `_04`: `thumbnail` is not admitted on that element by the
  pinned published XSD.
- `P_XXX_0339_01`: same-Core `mockelelement` is not admitted at that position.

Both native consumers reject these same originals. They remain explicit upstream
corpus/published-schema disagreements; qualification does not weaken the XSD or
relabel the upstream fixtures. Three official negative originals are also
XSD-invalid: `N_XXX_0410_01` has an undeclared QName prefix, `N_XXX_0422_01` has
invalid numeric lexical data and `N_XXX_0428_01` has invalid Core element content.

The other **39 negative originals are XSD-valid**. Their package, object-reference,
geometry or business rules are outside an XSD validity verdict. OPC package rules
remain Serializer obligations under their separate package contracts; only
application geometry/business validation is
[host-owned](../project/application_responsibilities.md). Acceptance here does not
certify these negative packages as conforming 3MF applications.

## Tooling and reproducibility

The [maintained replay](../../qualification/core_conformance/README.md) contains
the actual .NET oracle, native C++ probe and Python corpus runner. Current checks
use Windows x64, MSVC 19.51.36260.0 C++20 `/O2 /W4 /WX` and Python 3.14.3.
The immutable compiler reports 2.21.0 / schema 1.21.0, SHA-256
`320bb8f59b79f7e0781cc6f1152818a359aa46916c39db9657d625961b55465d`.
The imported authored profile hash is
`05b441502b8eacf3e4c60f837644dc472de149ac6e1c58be9aac88cbf40064d6`.
Generated C++/Python hashes, backend diagnostics, all raw per-case results and
encoded artifacts are retained under ignored
`out/build/pending-core-conformance-2026-10-21/replay/`. Native launch parents
suppress interactive Windows error dialogs.

As observational context, the original 171-case Python process ran about 778 s
from observed process start to final result write; its four additional small cases
ran separately. A sampled working set reached 254,197,760 bytes, which is a lower
bound, not an absolute peak. The original 171-case C++ workload took 25.09 s with
OS-reported peak working-set samples reaching 194,793,472 bytes (20 ms sampling;
the last unsampled interval is excluded). These are complete graph workload
observations under concurrent machine work, not per-operation allocation figures
or comparative performance thresholds. Large-model benchmarking is separate.
