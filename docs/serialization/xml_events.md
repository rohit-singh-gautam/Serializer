# Normalized XML events

Compiler/runtime **2.10.0** includes the provider-independent C++20 S01 contract
in `<rohit/xml_events.hpp>` (introduced in 2.9) and the S02 incremental UTF-8
producer in `<rohit/xml_event_reader.hpp>`. The contract defines borrowed event
payload, bounded structural validation and explicit completion; the LibXml2
reader establishes byte syntax and namespace scope without building an XML tree.
Event validation invokes no XML provider and remains available with
`SERIALIZER_WITH_XML=OFF`; constructing a reader requires XML enabled.
Generated token input/output remains in [the backlog](../pending.md).

Use this boundary for a byte provider or a synchronous consumer that needs a
normalized trace without retaining an XML tree. Existing
[whole-object APIs](xml_packages.md#whole-object-apis) remain supported with their
documented allocation and parsing behavior. Applications using a coding agent
should follow the [integration skill](../../.agents/skills/serializer-integration/SKILL.md).

## Ownership

An `event_view` borrows its expanded name, text, attribute views and local
namespace-declaration views. An expanded name consists of `local` and
`namespace_uri`; prefix spelling is not part of element or attribute identity.
Each local namespace declaration carries a prefix and namespace URI. Declarations
on ancestor elements are not repeated as local declarations.

The caller keeps every string and the storage underlying every span valid and
unchanged through `accept()` and any synchronous consumer using that event. The
event object does not extend those lifetimes. Copy any data that must survive
the call, asynchronous dispatch or reuse of a provider buffer. Attribute values
and text are decoded character data, rather than escaped XML source spelling.

`event_validator` copies only the expanded names of open elements so it can
match subsequent closes. It retains no event payload, attribute values,
namespace-declaration scope, text buffer or tree. It borrows an optional
`xml::operation_context`; that context and its callbacks must outlive the
validator. A validator and its context require external synchronization if used
across threads. Calls must not reenter the same validator, including from
operation-context callbacks.

## Normalized trace and completion

`event_kind` has five values:

| Kind | Payload | Structural rule |
| --- | --- | --- |
| `start_document` | Empty | Exactly once, as the first event. |
| `start_element` | Nonempty local name, namespace URI, attributes and local namespace declarations | Opens the single root or a child of the currently open element. No text payload. |
| `text` | Nonempty decoded text | Requires an open element. All name, attribute and namespace payload is empty. |
| `end_element` | Expanded name only | Matches the most recently opened element by URI plus local name. |
| `end_document` | Empty | Exactly once after the root has closed. No elements remain open. |

Empty payload means all unused fields are empty, including a name's namespace
URI. Attributes have nonempty local names; duplicate expanded attribute names
reject even if their values differ. Duplicate namespace prefixes among one
start event's local declarations reject. Namespace declarations are distinct
from ordinary attributes and are counted separately. Attribute and declaration
order does not change expanded-name identity.

An empty element always produces `start_element` followed by `end_element`;
there is no special empty-element event. Adjacent text chunks are permitted and
need not be coalesced. The event layer permits ordered mixed content, including
text produced from CDATA, but adds no generated mixed-content model or mapping.
Empty text chunks are omitted. Providers omit comments, processing instructions
and whitespace outside the root. Non-whitespace outside the root is invalid XML
and must reject at the provider boundary.

A golden trace for
`<root xmlns="urn:example" id="7">before<child/>after</root>` is:

```text
start_document
start_element {urn:example}root
  attributes: {}id="7"
  namespaces: ""="urn:example"
text "before"
start_element {urn:example}child
end_element {urn:example}child
text "after"
end_element {urn:example}root
end_document
```

The same trace can be validated synchronously with ordinary borrowed storage:

```cpp
#include <rohit/xml_events.hpp>
#include <array>

namespace xml = rohit::serializer::xml;

// Validate one normalized trace while all borrowed payload storage remains alive.
void validate_example() {
  const std::array attributes{xml::event_attribute{{"id", ""}, "7"}};
  const std::array declarations{xml::namespace_declaration{"", "urn:example"}};
  xml::event_validator validator;
  validator.accept({.kind = xml::event_kind::start_document});
  validator.accept({.kind = xml::event_kind::start_element,
      .qualified_name = {"root", "urn:example"},
      .attributes = attributes, .namespaces = declarations});
  validator.accept({.kind = xml::event_kind::text, .text = "before"});
  validator.accept({.kind = xml::event_kind::start_element,
      .qualified_name = {"child", "urn:example"}});
  validator.accept({.kind = xml::event_kind::end_element,
      .qualified_name = {"child", "urn:example"}});
  validator.accept({.kind = xml::event_kind::text, .text = "after"});
  validator.accept({.kind = xml::event_kind::end_element,
      .qualified_name = {"root", "urn:example"}});
  validator.accept({.kind = xml::event_kind::end_document});
  validator.finish();
}
```

An accepted `end_element` for the root is not parse success. A byte provider must
check the rest of the input through EOF before emitting `end_document`. It must
reject another root, malformed trailing data, incomplete tokens, DTDs and
user-declared/external entities. The S01 validator cannot perform those byte-level
checks. Predefined entities and character references must already be decoded
before text or attribute values reach this boundary.

Call `finish()` before publishing a decoded candidate. It requires the explicit
accepted `end_document`; an open document, a closed root without that event, or
no document rejects. `complete()` reports accepted explicit completion;
`failed()` reports terminal failure. Structural, resource, interruption and
context-callback failures make the validator terminal. Create a new validator
for a new document after failure. Events after completion reject.
`finish()` is idempotent after success while the operation context still permits
the call. A malformed shape, nesting or completion throws `std::invalid_argument`;
a resource limit throws `std::length_error`; reuse after failure throws
`std::logic_error`. Context callbacks preserve their original exception.

## Bounds and accounting

`event_limits::document` holds the document `xml::limits`; three additional
members bound the event layer:

| Limit | Default | Meaning |
| --- | --- | --- |
| `max_events` | 1,000,000 | All accepted events, including document markers, text and closes. |
| `max_event_bytes` | 16 MiB | Sum of string byte lengths in one event. |
| `max_state_bytes` | 1 MiB | Logical open-name storage plus temporary duplicate-check index storage. |

The document limits bound cumulative element count, attribute count, text bytes,
name bytes, local namespace declarations and current nesting depth. Zero is a
real bound; equality is permitted. Length/count arithmetic is checked before
growth. The limits are logical admissions, rather than a process heap or CPU
quota.

The per-event byte total includes its element local name and URI, each attribute
local name, URI and value, each namespace prefix and URI, and its text. Closing
names count toward that event's payload limit. Cumulative `name_bytes` counts
start-element names, attribute names and local namespace prefixes/URIs; it does
not charge closing names a second time. Cumulative `text_bytes` counts attribute
values and text chunks. `nodes` counts start elements. `namespace_bindings`
counts local declarations, not inherited bindings.

`event_usage` exposes `events`, `nodes`, `attributes`, `text_bytes`, `name_bytes`,
`namespace_bindings` and current `state_bytes` through `usage()`. Current logical
open-name storage is `sizeof(xml::name)` plus the local-name and namespace-URI
byte lengths for each open element. It falls as elements close and reaches zero
on successful completion. The duplicate checker temporarily sorts a `size_t`
index vector, reusing it for attributes and namespace declarations. For counts
of two or more it needs
`max(attribute_count, namespace_count) * sizeof(std::size_t)` bytes. Admission
includes that scratch and the newly opened frame simultaneously;
`usage().state_bytes` reports only the retained open-name stack. Allocator
bookkeeping, spare container/string capacity and provider buffers are excluded.
After a failure these counters may include partial admissions for the rejected
event; they describe diagnostics, not a reusable remaining budget.

`max_input_bytes` and `max_output_bytes` belong to byte-source/sink providers;
they cannot be inferred from normalized decoded events. `max_tree_bytes` does
not apply because the validator builds no tree. A provider still needs bounds
for input buffering, token assembly, namespace scope and output escaping. An
application retaining a tree or model must budget its own storage as well.

An optional shared operation context checks cancellation and deadlines and
charges one work unit per event, one per attribute and local namespace
declaration, and its payload byte total. These are separate charges to avoid
overflow; they do not count every sort comparison. Metadata span counts are
bounded before payload strings are scanned. Retained open-name
storage and duplicate-check scratch are admitted through the context before
allocation. Context counters are cumulative; charges already performed are not
rolled back after failure. Bound a complete composed operation with the same
context instead of resetting a budget for each event or nested document.

## Incremental byte reader

Require `find_package(Serializer 2.10 CONFIG REQUIRED)` and link
`Serializer::runtime`. No LibXml2 declarations appear in the public header.
`event_reader(callback, event_limits = {}, operation_context* = nullptr)` owns
one SAX2 push parse and its structural validator. The nonempty callback observes
each event synchronously after validation; copy any payload retained after it
returns. The reader owns its callable; storage that the callable borrows and the
optional context must outlive their use. The reader has a stable address, requires
external synchronization and forbids reentrant calls.

```cpp
#include <rohit/xml_event_reader.hpp>
#include <string>
#include <string_view>

namespace xml = rohit::serializer::xml;

// Collect an owning text candidate and publish it only after checked byte EOF.
std::string read_text(std::string_view first, std::string_view last) {
  std::string candidate;
  xml::event_reader reader{[&candidate](const xml::event_view& value) {
    if (value.kind == xml::event_kind::text) {
      candidate.append(value.text);
    }
  }};
  reader.feed(first);
  reader.feed(last);
  reader.finish();
  return candidate;
}
```

`feed()` accepts arbitrary slices, including empty slices, splits within UTF-8
code points and splits inside names, declarations, references or markup. Only
UTF-8 XML 1.0 is supported, with an optional UTF-8 BOM and compatible declaration.
Other declared encodings, UTF-16 BOMs, invalid/overlong UTF-8, surrogate code
points, forbidden XML characters and unfinished byte sequences reject. The
legacy whole-object context's UTF-16 opt-in does not change this reader's policy.
Provider-validated Unicode names and namespace IRIs are accepted. Prefix shadowing,
scope pop, default-namespace reset, reserved bindings and duplicate expanded
attributes are checked. Unprefixed ordinary attributes have an empty namespace;
namespace events contain local declarations only.

The provider decodes predefined entities/character references, normalizes line
endings and attribute whitespace, emits CDATA as ordinary text and pairs empty
element starts/closes. Adjacent text events may split differently with different
input slices. Comments, processing instructions and outside-root whitespace are
omitted. DTDs, including empty/internal/external subsets, and user/external
entities reject. External-resource requests cannot reach the provider's loader;
no document resource is fetched.

`finish()` checks all remaining byte syntax and EOF before delivering
`end_document`. A closed root, a successful `feed()` or an observed text event
cannot authorize model publication. `complete()` becomes true only after final
validation and the completion callback succeed. `finish()` is idempotent while
the context continues to permit it. Another `feed()` after completion rejects and
fences the reader. Syntax failures use `std::invalid_argument`, logical resource
limits use `std::length_error`, and reuse/reentry errors use `std::logic_error`.
The disabled-provider constructor throws `std::runtime_error`. Observer/context
exceptions retain their original type; none unwind across LibXml2 C callbacks.
Every failure is permanent, including a reentrant call caught by an observer.

`input_bytes()` counts cumulatively admitted source bytes; it does not count
decoded payload sizes. `usage()` exposes the S01 event validator's diagnostic
admissions. `max_input_bytes` is checked before feeding a slice, and internal
provider calls receive at most 4,096 bytes at a time. The same operation context
charges source work, event work and owned logical storage without resetting
between slices. Reader descriptor scratch, open expanded names, duplicate-index
scratch and temporary UTF-8 namespace-IRI escaping are admitted together against
`max_state_bytes` before owned growth. `usage().state_bytes` still reports the
validator's retained open names, rather than transient reader scratch.

The reader retains no complete source document or XML tree. LibXml2 nevertheless
owns token/input buffers, namespace structures and a name dictionary, and URI
validation may allocate provider storage. These allocations, allocator overhead
and spare capacities are outside exact logical state/context accounting. Total
admitted input bounds and the provider's normal non-HUGE token/depth limits
apply; a larger caller limit does not disable native provider limits. This is
not a process peak-memory bound or a hard interruption-latency guarantee inside
a provider call. Budget consumer-owned models/preserved trees separately and
publish a candidate only after complete validation.

## Provider responsibilities and remaining work

S01 checks normalized event shapes and nesting. It does not verify UTF-8,
XML 1.0 character/name grammar, prefix binding or namespace resolution. A byte
provider must establish those properties before producing events, normalize
line endings and attribute values, enforce its byte limits and disable external
resource resolution. The S02 reader above provides that byte boundary through
the maintained optional provider; custom producers retain the same obligations.

The event layer's ability to represent mixed text or qualified attributes does
not close the corresponding schema/compiler/backend tasks. Generated direct
token codecs now have selected 2.12 implementations described in the
[token guide](xml_tokens.md); remaining backend/provider qualification stays
pending. Shared
presence/mixed/map/variant/extension IR and reference checks are documented
[separately](xml_shared_contracts.md); selected C++/Python generated codecs are
implemented in 2.12 while other instances remain pending.
Existing generated XML
and ZIP/OPC behavior is unchanged. Schema language remains **1.20.0** and both
editor versions remain **2.0.5**; this API adds no schema syntax or navigation
destination.

The final 15-case event suite passes on Windows with XML enabled and with all
optional providers disabled, and in a fresh Linux XML/zlib build. It checks
golden traces, malformed boundaries, borrowed-buffer reuse, exact resource
limits, shared-context charges and terminal interruption/callback failures.
The documentation example compiles, links and runs under strict MSVC and GCC
C++20 checks. See the [verification record](../verification/verification-xml-events-2026-10-19.md)
for commands, platform/provider scope and remaining limits. That historical
record covers S01; [the 2.10 record](../verification/verification-xml-shared-contracts-2026-10-19.md)
records the incremental reader and new shared contracts separately.
