# Fixed XSD bundles and contract tools

Serializer compiles a supported fixed XSD subset into the XML mappings used by
every generated language. The compiler's XML provider must be enabled. Bundle
processing happens during generation; generated codecs do not load these files.

```ini
[xml]
enabled = true
xsd = schemas/order.xsd
# Optional: require the exact identity printed by --verbose.
# bundle_sha256 = <64 lowercase hexadecimal digits>
```

The root schema may include another schema in its own namespace or import a
different namespace. Every `schemaLocation` must name an explicit relative local
file. Canonical paths, including symlink targets, must remain inside the root
schema's directory. Network locations, absolute paths, URI escapes, missing
locations, conflicting namespaces, chameleon includes and reference cycles
reject. Limits cover 64 documents, 32 reference levels and 16 MiB of aggregate
source bytes. No document-supplied schema hint changes this allowlist.

The bundle identity is SHA-256 over the byte string `Serializer XSD bundle 1\n`
followed by every source in lexicographic relative-path order. Each source adds
its UTF-8 generic relative path and exact file contents, each preceded by its
decimal byte length and `:`. Moving an unchanged bundle to a different checkout
does not change its identity. Changing a path or source byte does. This is a build
input identity, not XML canonicalization or a signature. `bundle_sha256` pins it;
a mismatch fails generation. `--verbose` prints the actual identity. Generated
depfiles include every transitive XSD source.

Supported declarations include global elements, attributes, named complex types,
inline complex root types, named simple aliases restricted to a supported
primitive without facets, global declaration references, and nested singleton
sequence groups. A nested Serializer record must have a global root declaration
sharing the named complex type used by its containing element. This keeps its
standalone root valid as well as its nested representation. Configure a class's
root and namespace when the association would otherwise be ambiguous. Each
defining schema retains its own element and attribute qualification defaults.

Facets, choices, `all`, identity constraints, wildcard content, mixed content,
simple-content extensions and anonymous nested record associations remain
unsupported in fixed XSD compilation and fail explicitly. Runtime XSD providers
are a separate optional API; the fixed compiler bundle resolver does not enable
runtime imports or remote loading.

Export the supported actual model contract with:

```powershell
serializer --input model.serializer --config xml.ini --xml.export-xsd order.xsd
```

Native code output is optional for this command. Export supports scalar fields,
attributes, optional/repeated occurrence intervals and nested ordinary records in
one namespace. The exported XSD is compiled before publication. Unsupported enum,
text, wrapper or foreign-field-namespace exports reject. Export does not infer
types from sample XML or generate `.serializer` declarations from arbitrary XSD.
The existing XSD binding workflow imports the declared subset into a supplied
Serializer model.

Compare the previous and current fixed contracts against the same model with:

```powershell
serializer --input model.serializer --config current.ini --xml.compare-xsd previous.xsd
```

`current.ini` must enable XML and select the current `xsd`. The check requires the
current generated reader to accept the previous bound contract. It diagnoses
expanded root/field name changes, attribute/element placement, element order,
representation changes and narrowed occurrence intervals. Both schemas must bind
the supplied model completely; unsupported or conflicting bindings fail instead
of yielding a compatibility claim. This checks the supported bound XML subset,
not arbitrary XSD language inclusion or application-level invariants. Use separate
checks in the opposite direction when both reader directions are required.

Both tools require exactly one generator configuration and validate all artifacts
before replacing outputs. They may accompany ordinary language output flags.
