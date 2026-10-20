// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/xml_event_reader.hpp>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifndef SERIALIZER_HAS_XML
#define SERIALIZER_HAS_XML 0
#endif
#if SERIALIZER_HAS_XML
#include <libxml/entities.h>
#include <libxml/parser.h>
#include <libxml/uri.h>
#endif

namespace rohit::serializer::xml {
namespace {

// Reject overflow before adding an admission to its configured bound.
void admit(std::size_t& used, std::size_t amount, std::size_t maximum, operation_context* context = nullptr) {
  if (used > maximum || amount > maximum - used) {
    reject_limit(context, "XML event reader resource limit exceeded", amount, maximum - std::min(used, maximum));
  }
  used += amount;
}

#if SERIALIZER_HAS_XML
// Borrow a NUL-terminated provider string; null represents an empty namespace or prefix.
std::string_view borrow(const xmlChar* value) noexcept {
  return value ? std::string_view{reinterpret_cast<const char*>(value)} : std::string_view{};
}
#endif

} // namespace

struct event_reader::implementation {
  // Provider callbacks retain this owner at a stable address throughout one parse.
  implementation(const implementation&) = delete;
  // A parser context and its failure ledger cannot be shared by assignment.
  implementation& operator=(const implementation&) = delete;
  // Moving would invalidate the provider's callback owner pointer.
  implementation(implementation&&) = delete;
  // Keep provider ownership and callback identity bound to their original object.
  implementation& operator=(implementation&&) = delete;
  event_callback callback;
  event_limits bounds;
  operation_context* context;
  event_validator validator;
  std::size_t admitted_bytes{};
  std::size_t depth{};
  std::uint32_t codepoint{};
  std::uint32_t minimum_codepoint{};
  unsigned continuation_bytes{};
  bool terminal_failure{};
  bool completed{};
  bool active{};
#if SERIALIZER_HAS_XML
  xmlSAXHandler sax{};
  xmlParserCtxtPtr parser{};
  std::exception_ptr pending_exception{};
  bool provider_ended{};

  // Validate IRI namespace spelling through the provider's URI parser after UTF-8 escaping.
  void validate_namespace_uri(std::string_view value, std::size_t live_state_bytes) {
    if (value.empty()) {
      return;
    }
    const bool needs_escaping =
        std::any_of(value.begin(), value.end(), [](unsigned char byte) { return byte >= 0x80U; });
    std::string escaped;
    const char* uri_bytes = value.data();
    if (needs_escaping) {
      auto state_bytes = live_state_bytes;
      std::size_t escaped_bytes{};
      for (const unsigned char byte : value) {
        constexpr std::size_t escaped_byte_width = 3U;
        admit(escaped_bytes, byte >= 0x80U ? escaped_byte_width : 1U, bounds.max_state_bytes, context);
      }
      admit(state_bytes, escaped_bytes, bounds.max_state_bytes, context);
      if (context) {
        context->consume(operation_resource::allocation_bytes, escaped_bytes);
        context->consume(operation_resource::work_units, value.size());
      }
      escaped.reserve(escaped_bytes);
      constexpr std::string_view hex_digits = "0123456789ABCDEF";
      for (const unsigned char byte : value) {
        if (byte >= 0x80U) {
          escaped += '%';
          escaped += hex_digits[byte >> 4U];
          escaped += hex_digits[byte & 0x0fU];
        } else {
          escaped += static_cast<char>(byte);
        }
      }
      uri_bytes = escaped.c_str();
    }
    const std::unique_ptr<xmlURI, decltype(&xmlFreeURI)> parsed{xmlParseURI(uri_bytes),
                                                                &xmlFreeURI};
    if (!parsed) {
      throw std::invalid_argument{"Invalid XML namespace IRI"};
    }
  }

  // Trap all C++ exceptions at the C ABI and stop the parser before any later event.
  template <typename Function>
  void guarded(Function&& function) noexcept {
    if (terminal_failure) {
      return;
    }
    try {
      function();
    } catch (...) {
      pending_exception = std::current_exception();
      fence();
    }
  }

  // Verify normalized events before synchronously borrowing them to the observer.
  void emit(const event_view& value) {
    validator.accept(value);
    callback(value);
    // A callback may catch a rejected reentrant call; it must still abort this parse.
    if (terminal_failure) {
      throw std::logic_error{"XML event reader callback reentered the reader"};
    }
  }

  // Validate the declaration after the provider has parsed it, without enabling other encodings.
  static void start_document(void* owner) noexcept {
    auto& state = *static_cast<implementation*>(owner);
    state.guarded([&state] {
      if ((state.parser->version && borrow(state.parser->version) != "1.0") ||
          (state.parser->encoding &&
           xmlStrcasecmp(state.parser->encoding, BAD_CAST "UTF-8") != 0)) {
        throw std::invalid_argument{"XML event reader requires XML 1.0 and UTF-8"};
      }
      state.emit({.kind = event_kind::start_document});
    });
  }

  // Record provider EOF privately; only finish publishes the end_document boundary.
  static void end_document(void* owner) noexcept {
    auto& state = *static_cast<implementation*>(owner);
    state.guarded([&state] { state.provider_ended = true; });
  }

  // Borrow expanded names and local declarations; provider scope handles prefix shadowing.
  static void start_element(void* owner, const xmlChar* local, const xmlChar*, const xmlChar* uri,
                            int namespace_count, const xmlChar** namespaces, int attribute_count,
                            int, const xmlChar** attributes) noexcept {
    auto& state = *static_cast<implementation*>(owner);
    state.guarded([&] {
      if (namespace_count < 0 || attribute_count < 0) {
        throw std::invalid_argument{"Invalid XML provider descriptor count"};
      }
      const auto declarations_count = static_cast<std::size_t>(namespace_count);
      const auto attributes_count = static_cast<std::size_t>(attribute_count);
      std::size_t descriptor_bytes{};
      if (declarations_count > state.bounds.document.max_namespace_bindings ||
          attributes_count > state.bounds.document.max_attributes ||
          declarations_count > state.bounds.max_state_bytes / sizeof(namespace_declaration) ||
          attributes_count > state.bounds.max_state_bytes / sizeof(event_attribute)) {
        reject_limit(state.context, "XML event reader descriptor limit exceeded");
      }
      admit(descriptor_bytes, declarations_count * sizeof(namespace_declaration),
            state.bounds.max_state_bytes, state.context);
      admit(descriptor_bytes, attributes_count * sizeof(event_attribute),
            state.bounds.max_state_bytes, state.context);
      // Descriptor scratch is live together with the validator's owned open-name stack.
      admit(descriptor_bytes, state.validator.usage().state_bytes, state.bounds.max_state_bytes, state.context);
      admit(descriptor_bytes, sizeof(name), state.bounds.max_state_bytes, state.context);
      admit(descriptor_bytes, borrow(local).size(), state.bounds.max_state_bytes, state.context);
      admit(descriptor_bytes, borrow(uri).size(), state.bounds.max_state_bytes, state.context);
      const auto duplicate_count = std::max(declarations_count, attributes_count);
      if (duplicate_count >= 2U) {
        if (duplicate_count > state.bounds.max_state_bytes / sizeof(std::size_t)) {
          reject_limit(state.context, "XML event reader duplicate index limit exceeded");
        }
        admit(descriptor_bytes, duplicate_count * sizeof(std::size_t),
              state.bounds.max_state_bytes, state.context);
      }
      if (state.context) {
        state.context->consume(operation_resource::allocation_bytes,
                               declarations_count * sizeof(namespace_declaration));
        state.context->consume(operation_resource::allocation_bytes,
                               attributes_count * sizeof(event_attribute));
      }
      std::vector<namespace_declaration> declarations;
      declarations.reserve(declarations_count);
      constexpr std::size_t namespace_stride = 2U;
      for (std::size_t index{}; index < declarations_count; ++index) {
        const auto offset = index * namespace_stride;
        state.validate_namespace_uri(borrow(namespaces[offset + 1U]), descriptor_bytes);
        declarations.push_back({borrow(namespaces[offset]), borrow(namespaces[offset + 1U])});
      }
      std::vector<event_attribute> ordinary_attributes;
      ordinary_attributes.reserve(attributes_count);
      constexpr std::size_t attribute_stride = 5U;
      for (std::size_t index{}; index < attributes_count; ++index) {
        const auto offset = index * attribute_stride;
        const auto* begin = attributes[offset + 3U];
        const auto* end = attributes[offset + 4U];
        if (!begin || !end || end < begin) {
          throw std::invalid_argument{"Invalid XML provider attribute range"};
        }
        ordinary_attributes.push_back(
            {{borrow(attributes[offset]), borrow(attributes[offset + 2U])},
             {reinterpret_cast<const char*>(begin), static_cast<std::size_t>(end - begin)}});
      }
      state.emit({.kind = event_kind::start_element,
                  .qualified_name = {borrow(local), borrow(uri)},
                  .attributes = ordinary_attributes,
                  .namespaces = declarations});
      ++state.depth;
    });
  }

  // Close by expanded identity after the provider restores the matching lexical scope.
  static void end_element(void* owner, const xmlChar* local, const xmlChar*,
                          const xmlChar* uri) noexcept {
    auto& state = *static_cast<implementation*>(owner);
    state.guarded([&] {
      state.emit({.kind = event_kind::end_element, .qualified_name = {borrow(local), borrow(uri)}});
      --state.depth;
    });
  }

  // Deliver normalized decoded text; comments, processing instructions and outside space vanish.
  static void characters(void* owner, const xmlChar* bytes, int length) noexcept {
    auto& state = *static_cast<implementation*>(owner);
    state.guarded([&] {
      if (length > 0 && state.depth != 0U) {
        state.emit(
            {.kind = event_kind::text,
             .text = {reinterpret_cast<const char*>(bytes), static_cast<std::size_t>(length)}});
      }
    });
  }

  // Reject even an empty DOCTYPE before user entity declarations or external resolution.
  static void subset(void* owner, const xmlChar*, const xmlChar*, const xmlChar*) noexcept {
    auto& state = *static_cast<implementation*>(owner);
    state.guarded([] { throw std::invalid_argument{"XML event reader rejects DTDs"}; });
  }

  // Expose only the five built-in XML entities and never consult a document/entity loader.
  static xmlEntityPtr get_entity(void* owner, const xmlChar* value) noexcept {
    auto& state = *static_cast<implementation*>(owner);
    xmlEntityPtr result{};
    state.guarded([&] {
      result = xmlGetPredefinedEntity(value);
      if (!result) {
        throw std::invalid_argument{"XML event reader rejects user entities"};
      }
    });
    return result;
  }

  // No external entity request may reach the process-global provider resource loader.
  static xmlParserInputPtr resolve_entity(void* owner, const xmlChar*, const xmlChar*) noexcept {
    auto& state = *static_cast<implementation*>(owner);
    state.guarded(
        [] { throw std::invalid_argument{"XML event reader rejects external entities"}; });
    return nullptr;
  }

  // Convert provider diagnostics into a fenced exception without printing or escaping C callbacks.
#if LIBXML_VERSION >= 21200
  static void provider_error(void* owner, const xmlError* error) noexcept {
#else
  static void provider_error(void* owner, xmlError* error) noexcept {
#endif
      auto& state = *static_cast<implementation*>(owner);
  // LibXml2 applies an ASCII URI check to namespace IRIs. Validate those declarations
  // separately after percent-encoding UTF-8, while rejecting every namespace grammar error.
  if (error && error->level >= XML_ERR_ERROR && error->code != XML_WAR_NS_URI) {
    state.guarded([&] {
      throw std::invalid_argument{error->message ? error->message : "Invalid XML input"};
    });
  }
}

// Rethrow observer/context failures unchanged after the provider returns to C++.
void parse_bytes(std::string_view bytes, bool final) {
  const auto result =
      xmlParseChunk(parser, bytes.data(), static_cast<int>(bytes.size()), final ? 1 : 0);
  if (pending_exception) {
    std::rethrow_exception(pending_exception);
  }
  if (result != 0 || !parser->wellFormed || terminal_failure) {
    throw std::invalid_argument{"Invalid or incomplete XML event input"};
  }
}
#endif

// Initialize a SAX-only parser; no default tree-building callbacks are installed.
implementation(event_callback selected_callback, event_limits selected_bounds,
               operation_context* selected_context)
    : callback{std::move(selected_callback)}, bounds{selected_bounds}, context{selected_context},
      validator{bounds, context} {
  if (!callback) {
    throw std::invalid_argument{"XML event reader requires a callback"};
  }
#if SERIALIZER_HAS_XML
  sax.initialized = XML_SAX2_MAGIC;
  sax.startDocument = &start_document;
  sax.endDocument = &end_document;
  sax.startElementNs = &start_element;
  sax.endElementNs = &end_element;
  sax.characters = &characters;
  sax.cdataBlock = &characters;
  sax.ignorableWhitespace = &characters;
  sax.internalSubset = &subset;
  sax.externalSubset = &subset;
  sax.getEntity = &get_entity;
  sax.resolveEntity = &resolve_entity;
  sax.serror = &provider_error;
  parser = xmlCreatePushParserCtxt(&sax, this, nullptr, 0, nullptr);
  if (!parser) {
    throw std::bad_alloc{};
  }
  // Decode references through our predefined-only getEntity callback. DTDs reject before
  // declarations, the resolver rejects every request, and no external fetch is permitted.
  // Use provider options rather than the deprecated writable parser-context property.
  constexpr int parser_options = XML_PARSE_NONET | XML_PARSE_NOENT;
#if LIBXML_VERSION >= 21300
  xmlCtxtUseOptions(parser, parser_options | XML_PARSE_NO_XXE);
#else
  xmlCtxtUseOptions(parser, parser_options);
#endif
#else
    throw std::runtime_error{"XML event reader requires SERIALIZER_WITH_XML=ON"};
#endif
}

// Free provider buffers and its bounded-by-input name dictionary, without owning an XML tree.
~implementation() {
#if SERIALIZER_HAS_XML
  xmlFreeParserCtxt(parser);
#endif
}

// Make every subsequent call fail and prevent callbacks after the first failed admission.
void fence() noexcept {
  terminal_failure = true;
  completed = false;
#if SERIALIZER_HAS_XML
  if (parser) {
    xmlStopParser(parser);
  }
#endif
}

// Reject invalid raw UTF-8/XML characters before provider encoding detection can reinterpret it.
void inspect_utf8(std::string_view bytes) {
  for (const unsigned char byte : bytes) {
    if (continuation_bytes == 0U) {
      if (byte < 0x80U) {
        if (byte < 0x20U && byte != '\t' && byte != '\n' && byte != '\r') {
          throw std::invalid_argument{"Invalid XML 1.0 input character"};
        }
      } else if (byte >= 0xc2U && byte <= 0xdfU) {
        codepoint = byte & 0x1fU;
        minimum_codepoint = 0x80U;
        continuation_bytes = 1U;
      } else if (byte >= 0xe0U && byte <= 0xefU) {
        codepoint = byte & 0x0fU;
        minimum_codepoint = 0x800U;
        continuation_bytes = 2U;
      } else if (byte >= 0xf0U && byte <= 0xf4U) {
        codepoint = byte & 0x07U;
        minimum_codepoint = 0x10000U;
        continuation_bytes = 3U;
      } else {
        throw std::invalid_argument{"Invalid XML UTF-8 leading byte"};
      }
    } else {
      if ((byte & 0xc0U) != 0x80U) {
        throw std::invalid_argument{"Invalid XML UTF-8 continuation byte"};
      }
      codepoint = (codepoint << 6U) | (byte & 0x3fU);
      --continuation_bytes;
      if (continuation_bytes == 0U && (codepoint < minimum_codepoint || codepoint > 0x10ffffU ||
                                       (codepoint >= 0xd800U && codepoint <= 0xdfffU) ||
                                       codepoint == 0xfffeU || codepoint == 0xffffU)) {
        throw std::invalid_argument{"Invalid XML UTF-8 code point"};
      }
    }
  }
}

// Enter one public operation; reentrancy fences even if the observer catches the exception.
void enter() {
  if (terminal_failure) {
    throw std::logic_error{"XML event reader has failed"};
  }
  if (active) {
    fence();
    throw std::logic_error{"XML event reader is not reentrant"};
  }
  active = true;
}

// Bound provider input chunks and interruption latency without accumulating document bytes.
void feed(std::string_view bytes) {
  enter();
  try {
    if (completed) {
      throw std::logic_error{"XML input after event reader completion"};
    }
    if (context) {
      context->check();
    }
    admit(admitted_bytes, bytes.size(), bounds.document.max_input_bytes, context);
    constexpr std::size_t provider_chunk_bytes = 4096U;
    while (!bytes.empty()) {
      const auto chunk = bytes.substr(0U, std::min(bytes.size(), provider_chunk_bytes));
      if (context) {
        context->consume(operation_resource::work_units, chunk.size());
      }
      inspect_utf8(chunk);
#if SERIALIZER_HAS_XML
      parse_bytes(chunk, false);
#endif
      bytes.remove_prefix(chunk.size());
    }
    active = false;
  } catch (...) {
    active = false;
    fence();
    throw;
  }
}

// Finalize the provider before certifying byte EOF and delivering the completion event.
void finish() {
  enter();
  try {
    if (context) {
      context->check();
    }
    if (!completed) {
      if (continuation_bytes != 0U) {
        throw std::invalid_argument{"Truncated XML UTF-8 at EOF"};
      }
#if SERIALIZER_HAS_XML
      parse_bytes({}, true);
      if (!provider_ended) {
        throw std::invalid_argument{"Missing XML provider end_document"};
      }
      emit({.kind = event_kind::end_document});
#endif
      validator.finish();
      completed = true;
    }
    active = false;
  } catch (...) {
    active = false;
    fence();
    throw;
  }
}
}; // namespace rohit::serializer::xml

// Keep provider declarations private to the compiled runtime.
event_reader::event_reader(event_callback callback, event_limits bounds, operation_context* context)
    : state_{std::make_unique<implementation>(std::move(callback), bounds, context)} {}

// Destroy the complete private implementation in this translation unit.
event_reader::~event_reader() = default;

// Forward one borrowed input slice to the stable parser owner.
void event_reader::feed(std::string_view bytes) {
  state_->feed(bytes);
}

// Publish completion only after the provider has accepted final byte EOF.
void event_reader::finish() {
  state_->finish();
}

// Expose only final successful observer completion.
bool event_reader::complete() const noexcept {
  return state_->completed;
}

// Expose terminal failures even when the underlying validator accepted the last event.
bool event_reader::failed() const noexcept {
  return state_->terminal_failure;
}

// Borrow diagnostic admissions without retaining event payloads.
const event_usage& event_reader::usage() const noexcept {
  return state_->validator.usage();
}

// Report admitted source bytes without recounting decoded text.
std::size_t event_reader::input_bytes() const noexcept {
  return state_->admitted_bytes;
}

} // namespace rohit::serializer::xml
