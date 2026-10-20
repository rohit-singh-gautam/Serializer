# Eleven-language XML exchange qualification — 2026-10-20

Compiler/runtime **2.14.0**, schema language **1.20.0**, passed actual generated
XML exchange across **C++, Python, Java, Kotlin, C#, Go, JavaScript, TypeScript,
Rust, Swift and C** on the selected Windows amd64 SDK profile. No backend was
substituted or silently skipped.

## Executed decisions

The maintained [qualification driver](../../qualification/xml_exchange_qualification.py)
reuses [the XML example runner](../../example/xml/run.py) and
[the eleven-language manifest](../../test/resources/xml_exchange_manifest.json).
It builds or interprets each actual generated backend, runs every producer into
every consumer, and independently checks the resulting XML with Python's XML
parser. The oracle compares expanded element names, attributes, escaped text,
child order and repeated values against the manifest.

| Selected mapping | Producers × consumers | Independent XML documents |
| --- | ---: | ---: |
| Configured XML mapping | 11 × 11 = **121** | **132** |
| Fixed XSD-derived mapping | 11 × 11 = **121** | **132** |
| Total | **242 exchanges** | **264 documents** |

The C++ configured and XSD consumers are copied executables built through the
actual native CMake targets. Freshly generated headers matched the headers used
by those helpers byte-for-byte. Their hashes are respectively
`72401b26e6edb58c2478c257e42129932305cb349e42eb64721b7aeb0d35f6da`
and `4fdaf388e64a237d688c07be0dad3f5de1194e1182b13995606ff171d863d27b`.

The same driver then generated all eleven ordinary native models with XML
disabled. Each actual application passed **JSON, BINARY_NONE, BINARY_INTEGER
and BINARY_STRING**, giving **44 protocol roundtrips**. All eleven applications
also rejected an unsupported future version without publishing an output file.
Generated native-only sources contained no XML parser imports; the C and C++
executables had no XML-provider imports in `dumpbin /dependents`. Java compilation
and Java/Kotlin native execution succeeded with only the `java.base` module.
Rust native compilation used its standard-library-only path. These are selected
source/build/link independence checks, not a claim that every installed SDK
contains no XML implementation.

## Actual tools and dependencies

| Backend/tool | Executed profile |
| --- | --- |
| C/C++ | MSVC 19.51 family; CMake 4.4.2; LibXml2 2.15.3 for XML-enabled consumers |
| Python | CPython 3.14.3 |
| Java/Kotlin | Microsoft OpenJDK 17.0.18; Kotlin JVM 2.2.21 |
| C# | .NET SDK 10.0.402, targeting `net8.0` |
| Go | Go 1.26.4 |
| JavaScript/TypeScript | Node 22.23.1; npm 11.18.0; TypeScript 5.9.3 |
| Rust | Rust 1.99.0; Cargo 1.99.0 |
| Swift | Swift 6.4, Windows x86-64 |

JavaScript and TypeScript use the maintained lockfiles for
`@xmldom/xmldom` 0.9.12, `saxes` 6.0.0 and `xmlchars` 2.2.0. Rust uses the
maintained locked `roxmltree` 0.21.1, `xmlparser` 0.13.6 and `memchr` 2.8.3.
The replay used private offline npm/Cargo caches. Missing locked npm tarballs
were obtained from their exact official registry URLs and checked against the
maintained SHA-512 lock integrity before the final offline run; no install
scripts or global package changes were used.

The actual compiler SHA-256 is
`9938073386c2a00641d69a3410690322f5b5fd396f9fc9371f185e1f10b65206`,
from the 2,349-file source capture with aggregate
`303fc92b9f2932d0b4c5d4572ab27b5be81b304a4c8735d48a0979b994e0c223`.
The copied configured/XSD helper hashes are
`7e2302bc593ee5d7746f97a9a564f133425ec8cc6e193bb1b680092f7dae9651`
and `b2e668b5455b8467ca7bf3611c7173d31452a226753fb397daeff83937ec5837`.
The native-only C++ runtime library hash is
`6c3f405b53f20346623940dfb8a0323c86bb2c6d545f17974e009efe415874ba`.
The driver verifies these selected tools remain unchanged throughout execution.

## Retained evidence and reproduction

The final evidence is under
`out/build/backlog-2026-10-20/xml-exchange-current-final/`: generated sources,
XML outputs, `verification.json`, each profile's `commands.json`, and per-command
logs. There are **158 configured**, **158 XSD** and **34 native-only** recorded
commands, including the eleven expected negative exits. The wrapper completed
in 1,806.368 seconds while other qualification jobs ran; this elapsed time is
not a performance gate.

The final report SHA-256 is
`a1611d6cc11a13ea117dfab78703cab34323f1322bc35d93ecdb35cfd8ec6e5b`.
The driver hash is
`e662f9f0e3207c982cc5c6fce22dc9687841b586e8504cea40d2c6e18c8d6143`;
the manifest hash is
`0758be21ac2414c97821c1282dfbd76c972f6c919ba8fa3af6c5ef345dfbe632`.
`xml-exchange-final-audit.json` separately verifies source/tool/output/log hashes,
both full matrices, native negatives, and the exact qualification IDs.

An explicit missing-Kotlin control using the same final driver aborted before
generation or exchanges. Its failed report is retained in `xml-sdk-absence/`.
This control used the earlier actual compiler `309eb574…`; it verifies SDK
absence is an error rather than a skipped producer or consumer. The final green
run used the compiler and tools pinned above.

Run the maintained driver from a configured native SDK environment with a fresh
output directory, explicit copied C++ helpers/runtime library, and the required
SDK and locked-cache paths:

    python -B qualification/xml_exchange_qualification.py --help

## Qualified scope

This evidence qualifies the selected **Q03** producer/consumer matrix for all
eleven language prefixes `CP/PY/JV/KT/CS/GO/JS/TS/RS/SW/C`, and **Q01/Q04** for
the actual SDK/native-format/XML-independent profiles described above. The
fixture is a small declared XML document under two mapping profiles. **Q02**
uniform malformed-input/resource/interruption qualification, broader XML feature
matrices, other platforms, process-memory ceilings and performance gates remain
outside this proof.
