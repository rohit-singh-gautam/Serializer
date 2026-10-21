# OPC core properties

Compiler/runtime **2.21.0** supplies the reusable C++
`<rohit/core_properties.hpp>` adapter. Applications supply property values;
the runtime does not invent timestamps, authors, revisions or identifiers.
This is an optional standard OPC part, separate from application payload models.

```cpp
#include <rohit/core_properties.hpp>

namespace package = rohit::serializer::package;
package::core_properties metadata{};
metadata.title = "Model title";
metadata.creator = "Application user";
metadata.created = "2026-10-21T12:00:00Z";

// Add payload parts and their relationships through the same writer.
package::opc_writer writer{output};
package::write_core_properties(writer, metadata);
writer.finish();

package::opc_reader reader{input};
package::profile_loader loader{reader};
auto loaded_metadata = package::read_core_properties(loader);
// Read application roles before finishing the shared loader.
auto preserved = loader.finish();
```

The adapter exposes all fifteen fifth-edition standard properties: `category`,
`content_status`, `created`, `creator`, `description`,
`identifier`, `keywords`, `language`, `last_modified_by`, `last_printed`,
`modified`, `revision`, `subject`, `title` and `version`. Properties other than
`keywords` are optional owning strings. An absent property differs from a present empty string.
String content retains its value through normal XML escaping and XML newline
normalization; no lexical XML preservation is promised.

`keywords` is an optional `core_property_keywords` value with an optional
`language` and ordered `content` fragments. Each fragment is either an owning
string or a `core_property_keyword` containing `value` and optional `language`.
This represents fifth-edition mixed text and localized `<value xml:lang>`
elements without inventing delimiters. Adjacent text fragments may coalesce on
XML decoding; surrounding/interleaved text and language attributes retain their
values. The legacy `contentType` property was removed in the fifth edition and
is rejected by this adapter. Applications requiring an older metadata vocabulary
can retain its whole part as opaque bytes.

The default output part is `/docProps/core.xml`; callers may choose another
valid OPC part name and relationship ID. `read_core_properties` discovers the
part through its standard root relationship, independently of its physical
name. A missing relationship returns `std::nullopt`; multiple relationships,
an external target or an incorrect content type reject. The part is marked
consumed in the supplied `profile_loader`, so ordinary application-role and
unknown-part policies continue to apply when that loader finishes. Call the
writing adapter once per writer. Generic low-level OPC builders can still
represent arbitrary relationships; they do not supply vocabulary-specific
singleton enforcement on their own.

`created` and `modified` implement the normative imported Dublin Core schema's
`W3CDTF` union of XML Schema `gYear`, `gYearMonth`, `date` and `dateTime`.
The general [W3C date/time note](https://www.w3.org/TR/NOTE-datetime) includes
minute-only timestamps; those are rejected because they are not members of
that imported schema union. Calendar boundaries and Gregorian leap years are
checked; year zero is rejected. `last_printed` uses XML Schema 1.0 `dateTime`.
Seconds are required for timestamps; timezone presence is optional and offsets
stop at fourteen hours. Extended nonzero years and midnight `24:00:00` are
accepted. Surrounding XML whitespace is admitted by the schema's collapse
facet and retained in the model's lexical string. Fractional precision is
bounded by the operation's text and allocation limits. These are lexical values; the API does not
compare instants, infer timezones or convert reduced date precision.

The XML adapter accepts UTF-8 and BOM-marked UTF-16 input and writes
deterministic UTF-8. It checks property expanded names, singleton occurrences,
flat content and the namespace-resolved `xsi:type="dcterms:W3CDTF"` on
`created` and `modified`. Equivalent locally declared prefixes work. Except for
the localized keyword shape above, other attributes, unknown elements, nested content, alternate type identities and
markup-compatibility extensions reject in this closed typed adapter. To pass
through a part outside that contract, retain it through the existing opaque
whole-part preservation APIs without decoding or rewriting it. This adapter
does not claim signature preservation or complete OPC corpus qualification.

`encode_core_properties` and `decode_core_properties` operate on standalone XML
using an explicit `package::budget&`. Archive helpers reuse their writer/loader's
cumulative budget. Admission, cancellation and deadlines cover tree building,
XML processing and returned value storage; the ordinary XML provider's stated
internal allocation/preemption boundaries still apply. Construction and
decoding produce fresh values. Invalid date values are checked before the
writing helper adds an entry; a fallible stream writer can otherwise leave a
partial archive. Use the existing staged full-Save API for final-file
transactional publication.

The implemented vocabulary follows clauses 8.2-8.3 and Annex C.3 of
[ECMA-376 Part 2, fifth edition](https://ecma-international.org/publications-and-standards/standards/ecma-376/),
including its published OPC schema and imported Dublin Core schemas. Standard identities and discovery also follow the public
[Microsoft OPC description](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/opc/finding-the-core-properties-part)
and [package relationship inventory](https://learn.microsoft.com/en-us/dotnet/api/system.io.packaging.package.createrelationship).
See [the general package API](xml_packages.md) and
[scope and verification](../verification/verification-core-properties-2026-10-21.md).
