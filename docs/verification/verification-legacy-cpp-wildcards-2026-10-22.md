# Legacy C++ repeated-choice wildcards

Serializer 2.28.0 preserves the existing C++ tree codec for repeated XSD choices
with one known collection branch and wildcard branches. A repeated-choice
descendant selects that path for its containing graph. This avoids requiring a
direct token mapping from the existing 3MF Core resources representation.
Extension-only nested records retain tree readers/writers, while contracts that
require other token-only features keep their explicit capability diagnostics.
`serialize_tokens`/`deserialize_tokens` remain unavailable for this legacy
repeated-choice representation; explicit ordered owning storage is a separate
supported mapping.

Tree wildcard traversal records bounded borrowed payload views at the original
known-child and native-field boundaries. The shared extension inspector applies
the same pinned declaration, namespace, required-capability, occurrence and
retention contracts as direct codecs. It validates before publishing retained
subtrees. Output revalidates edited retained payloads. Existing repeated-choice
grammar checks retain per-branch bounds and aggregate repetition semantics.

The public XML facade borrows complete-document bounds through nested tree
declaration replay and restores the enclosing pointer on success or failure.
An explicit `extension_bounds.document` remains an independent selected policy.
Ordinary null-context XML calls do not acquire an operation context merely to
track document bounds; legacy tree graphs use a stack context when needed.

`test/xml_cpp_legacy_wildcard_test.py` passes **28 independent XSD decisions** in
each strict/lax × preserve/skip configuration: **112 decisions** through real
generated C++, compiled with MSVC `/std:c++20 /W4 /WX`. Original and accepted
emitted documents pass the independent lxml/libxml2 XSD oracle. JSON and all three
native binary payloads remain unchanged. The corpus includes interleaved known
and foreign occurrences, two wildcard repetitions despite a per-branch maximum
of one, excess group repetitions, declaration facets, wrong-kind/unknown names,
required tails, and global attribute validation.

Additional probes reject missing required vocabulary understanding on decoding
and encoding, including skip output with no retained foreign state. A declared
two-node packet rejects an independent foreign-document `max_nodes=1` and accepts
`max_nodes=2`; failed callbacks leave the caller's enclosing document policy
unchanged. The packet also exercises shared-operation exact and one-tight work,
allocation and preserved-byte admission, original failure identity, skip with
zero preservation quota, and edited invalid declared output rejection.

The SDK matrix uses compiler checkpoint SHA-256
`65A61BBF803F68C8C34DA3193724B2BDB7ED40EE6DC472FD7D5ECCCC4BC2E3B3`.
Logs and useful diagnostics remain under ignored `out/`, including
`out/opc-legacy-wildcard-cpp-*-resources.log`. The maintained full 3MF Core
generation and native runtime tests are separate repository regression checks.
