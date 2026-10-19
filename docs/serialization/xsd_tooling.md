# Fixed XSD bundles and contract tools

Compiler/runtime **2.7.0**, schema language **1.20.0**, adds inline schema binding
and declaration import. Serializer compiles a supported fixed XSD subset into
generated XML mappings. The primitive/ordinary-record baseline is available in
all eleven outputs; richer contracts are currently C++ only and other backends
reject them explicitly. The compiler's XML provider must be enabled. Bundle
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

The baseline supports global elements/attributes, named complex types, inline
complex roots, supported primitive aliases, global declaration references and
nested singleton sequences. A nested class can bind its named complex type
without inventing a global element; a uniquely named anonymous local complex
type can also bind. Ambiguous associations reject. Each defining schema retains
its own element/attribute qualification defaults.

The C++ path additionally retains numeric inclusive/exclusive restrictions,
enumerations, lengths, whitespace rules, default/fixed values, singleton element
choices, simple-content text extensions, builtin scalar lists and wildcard
contracts. QName identity and list adapters use explicit XML mappings. These
contracts must be enforceable by the selected emitter: retaining compiler
metadata alone does not imply that every content model or XSD pattern can be
generated. Unsupported patterns, repeated/grouped choices, `all`, identity
constraints, complex-content inheritance and ambiguous anonymous associations
still fail. Custom restricted list item types reject until per-item and
container constraints have separate complete support. The complete official
3MF XSD and arbitrary XSD 1.0 are not supported.

Runtime XSD providers are a separate optional API; the fixed compiler bundle
resolver does not enable runtime imports or remote loading. Qualification of
the new inputs and selected SDKs is pending until the current verification run
completes; historical baseline checks do not qualify these additions.

## Bind an existing model in its schema

An application can retain its chosen API names and native field IDs while
selecting one XSD bundle relative to the declaring `.serializer` file:

```text
serializer version 1.20.0;

namespace orders {
  using schema "order.xsd";

  class order stable_ids {
    public uint32 id (1);
    public string customer (2);
  }
}
```

The supplied XSD must declare the compatible `order` contract. `using schema`
binds declarations already present; it does not create missing classes. XML
selection is automatic for schema-authored XML metadata unless explicitly
disabled. A package declaration cannot be combined with explicit XML disable.
All declaring files using this syntax need a `1.20.0` header. Paths resolve from
the declaring file, and generation dependencies include transitive XSD inputs.

For a contract authored without XSD, use namespace/class defaults such as
`xml(namespace("urn:orders"), scalars(attribute));` and field exceptions such as
`xml(element)`, `xml(text)`, `xml(name("item"))` or
`xml(attribute, namespace("http://www.w3.org/XML/1998/namespace"), name("lang"))`.
Explicit `list`, `qname` and `prefix_list` adapters are currently C++ only.
Class INI mappings remain compatible and merge nonempty overrides over inherited
defaults; an explicit empty reset through a legacy class INI entry is not
represented. A field's inline namespace override can explicitly be empty.

## Import declarations from XSD

```powershell
serializer --import-xsd schemas/order.xsd --namespace orders --root order --output build/imported/order.serializer --depfile build/imported/order.serializer.d
serializer --input build/imported/order.serializer --language cpp --output build/generated/order.hpp
```

Create the output directories first. `--namespace` defaults to `imported` and
accepts a lowercase snake_case namespace; `--root` selects a global complex root
by local name or `{namespace_uri}local`. Without it, all eligible global complex
roots are selected. Multiple global roots sharing one complex type require an
explicit selection. Recursive owning records and unsupported scalar/root shapes
reject rather than yielding a weaker model.

Import generates reviewable ordinary models, inline XML bindings and one
`using schema` dependency, with deterministic `xsd_` host names. Explicit native
member IDs derive from expanded XML identity and attribute/element placement,
not declaration position; collisions reject. Adding an unrelated field does
not renumber an existing identity. Namespace identities disambiguate generated
type names. Arbitrary-precision numeric XML types remain lexical strings rather
than being silently narrowed to machine integers. A fixed builtin scalar list
can become a typed fixed array while retaining one XML attribute/text value.

The CLI requires a `.serializer` destination with an existing parent. Identical
existing output is accepted; differing existing source is not overwritten.
Import changes into a fresh file and review native compatibility, declaration
order, renamed XML identities and authored behavior before migration. Stable IDs
alone do not preserve a positional layout after field reordering. `--depfile`
records the complete pinned XSD graph. Import is separate from ordinary
`--input` generation and cannot be mixed with unrelated generator options.

The [automatic and optional hand-authored examples](../../example/xsd/README.md)
generate and compile the same primitive contract in all eleven languages. Rich
C++ feature checks and other-backend rejection are separate from that baseline;
an imported model is not an OPC package or a 3MF conformance claim.

## Export and compare bound contracts

Export the supported actual model contract with:

```powershell
serializer --input model.serializer --config xml.ini --xml.export-xsd order.xsd
```

Native code output is optional for this command. Export supports scalar fields,
attributes, optional/repeated occurrence intervals and nested ordinary records in
one namespace, including supported scalar facets, builtin lists and default/fixed
values. The exported XSD is compiled before publication. Unsupported enum, text,
wrapper, choice/wildcard or foreign-field-namespace exports reject. Export does
not infer types from sample XML; declaration import is the separate command
above. See the [authoring contract](../design/opc_3mf_support.md#schema-first-authoring-and-declaration-import).

Compare the previous and current fixed contracts against the same model with:

```powershell
serializer --input model.serializer --config current.ini --xml.compare-xsd previous.xsd
```

`current.ini` must enable XML and select the current `xsd`. The check requires the
current generated reader to accept the previous bound contract. It diagnoses
expanded root/field name changes, attribute/element placement, element order,
representation changes, narrowed occurrence intervals and changed scalar/default
semantics. Both schemas must bind
the supplied model completely; unsupported or conflicting bindings fail instead
of yielding a compatibility claim. This checks the supported bound XML subset,
not arbitrary XSD language inclusion or application-level invariants. Use separate
checks in the opposite direction when both reader directions are required.

Both tools require exactly one generator configuration and validate all artifacts
before replacing outputs. They may accompany ordinary language output flags.
