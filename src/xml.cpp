// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/xml.hpp>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <map>
#include <set>

#ifndef SERIALIZER_HAS_XML
#define SERIALIZER_HAS_XML 0
#endif
#if SERIALIZER_HAS_XML
#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlschemas.h>
#include <libxml/tree.h>
#endif

namespace rohit::serializer::xml {
namespace {
// Fail bounded counters before overflow or allocation beyond the document budget.
void consume(std::size_t& used, std::size_t amount, std::size_t bound) {
  if (used > bound || amount > bound - used) { throw std::length_error{"XML resource limit exceeded"}; }
  used += amount;
}
// Reject names outside the supported portable NCName subset rather than emitting invalid XML.
void check_name(std::string_view value) {
  const auto initial = [](unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
  };
  if (value.empty() || !initial(static_cast<unsigned char>(value.front()))) {
    throw std::invalid_argument{"XML mapping requires a nonempty ASCII NCName"};
  }
  for (const auto character : value) {
    const auto c = static_cast<unsigned char>(character);
    if (!initial(c) && !(c >= '0' && c <= '9') && c != '-' && c != '.') {
      throw std::invalid_argument{"Invalid XML mapping NCName"};
    }
  }
}
// Validate canonical UTF-8 and XML 1.0 character ranges before any bytes are emitted.
void check_text(std::string_view value) {
  for (std::size_t index{}; index < value.size();) {
    const auto first = static_cast<unsigned char>(value[index++]);
    std::uint32_t codepoint{first};
    std::size_t count{};
    if (first >= 0xc2U && first <= 0xdfU) { count = 1U; codepoint &= 0x1fU; }
    else if (first >= 0xe0U && first <= 0xefU) { count = 2U; codepoint &= 0x0fU; }
    else if (first >= 0xf0U && first <= 0xf4U) { count = 3U; codepoint &= 0x07U; }
    else if (first >= 0x80U) { throw std::invalid_argument{"Invalid XML UTF-8"}; }
    if (count > value.size() - index) { throw std::invalid_argument{"Truncated XML UTF-8"}; }
    for (std::size_t next{}; next < count; ++next) {
      const auto byte = static_cast<unsigned char>(value[index++]);
      if ((byte & 0xc0U) != 0x80U) { throw std::invalid_argument{"Invalid XML UTF-8 continuation"}; }
      codepoint = (codepoint << 6U) | (byte & 0x3fU);
    }
    if ((count == 1U && codepoint < 0x80U) || (count == 2U && codepoint < 0x800U) ||
        (count == 3U && codepoint < 0x10000U) ||
        !(codepoint == 0x09U || codepoint == 0x0aU || codepoint == 0x0dU ||
          (codepoint >= 0x20U && codepoint <= 0xd7ffU) ||
          (codepoint >= 0xe000U && codepoint <= 0xfffdU) ||
          (codepoint >= 0x10000U && codepoint <= 0x10ffffU))) {
      throw std::invalid_argument{"Invalid XML 1.0 character"};
    }
  }
}
// Build deterministic output without permitting intermediate escaped output beyond its limit.
class document_writer {
  limits bounds_;
  std::string bytes_{};
  std::size_t nodes_{};
  std::size_t attributes_{};
  std::size_t text_bytes_{};
  std::size_t name_bytes_{};
  std::map<std::string, std::string> prefixes_{};
  std::set<std::string> retained_prefixes_{};
  std::size_t namespace_bindings_{};
  // Append within the complete output budget.
  void append(std::string_view value) {
    if (bytes_.size() > bounds_.max_output_bytes || value.size() > bounds_.max_output_bytes - bytes_.size()) {
      throw std::length_error{"XML output limit exceeded"};
    }
    bytes_ += value;
  }
  // Preserve text/attribute whitespace using character references where XML normalizes literals.
  void escaped(std::string_view value, bool is_attribute) {
    check_text(value);
    for (const auto c : value) {
      switch (c) {
      case '&': append("&amp;"); break;
      case '<': append("&lt;"); break;
      case '>': append("&gt;"); break;
      case '"': if (is_attribute) { append("&quot;"); } else { append({&c, 1U}); } break;
      case '\r': append("&#13;"); break;
      case '\n': if (is_attribute) { append("&#10;"); } else { append({&c, 1U}); } break;
      case '\t': if (is_attribute) { append("&#9;"); } else { append({&c, 1U}); } break;
      default: append({&c, 1U}); break;
      }
    }
  }
  // Register stable namespace prefixes in first-use traversal order.
  void register_name(const name& value) {
    consume(name_bytes_, value.local.size(), bounds_.max_name_bytes);
    consume(name_bytes_, value.namespace_uri.size(), bounds_.max_name_bytes);
    check_name(value.local);
    check_text(value.namespace_uri);
    if (value.namespace_uri == "http://www.w3.org/2000/xmlns/") {
      throw std::invalid_argument{"Namespace declaration names are reserved"};
    }
    if (!value.namespace_uri.empty() && !prefixes_.contains(value.namespace_uri)) {
      prefixes_.emplace(value.namespace_uri, value.namespace_uri == "http://www.w3.org/XML/1998/namespace" ? "xml" : "");
    }
  }
  // Verify shape, budgets and duplicate expanded attributes before serializing.
  void inspect(const element& node, std::size_t depth) {
    if (depth > bounds_.max_depth) { throw std::length_error{"XML depth limit exceeded"}; }
    consume(nodes_, 1U, bounds_.max_nodes);
    consume(text_bytes_, node.text.size(), bounds_.max_text_bytes);
    register_name(node.qualified_name);
    consume(namespace_bindings_, node.namespace_bindings.size(), bounds_.max_namespace_bindings);
    for (const auto& [prefix, uri] : node.namespace_bindings) {
      if (!prefix.empty()) { check_name(prefix); }
      check_text(uri);
      if (prefix == "xmlns" || uri == "http://www.w3.org/2000/xmlns/" ||
          (prefix == "xml") != (uri == "http://www.w3.org/XML/1998/namespace")) {
        throw std::invalid_argument{"Invalid retained XML namespace binding"};
      }
      consume(name_bytes_, prefix.size(), bounds_.max_name_bytes);
      consume(name_bytes_, uri.size(), bounds_.max_name_bytes);
      retained_prefixes_.insert(prefix);
    }
    if (!node.children.empty() && !detail::trim(node.text).empty()) {
      throw std::invalid_argument{"XML mixed content is unsupported"};
    }
    std::set<std::pair<std::string, std::string>> names{};
    for (const auto& attribute : node.attributes) {
      consume(attributes_, 1U, bounds_.max_attributes);
      consume(text_bytes_, attribute.value.size(), bounds_.max_text_bytes);
      register_name(attribute.qualified_name);
      if (attribute.qualified_name.namespace_uri.empty() && attribute.qualified_name.local == "xmlns") {
        throw std::invalid_argument{"Namespace declarations are not ordinary attributes"};
      }
      if (!names.emplace(attribute.qualified_name.namespace_uri, attribute.qualified_name.local).second) {
        throw std::invalid_argument{"Duplicate XML attribute"};
      }
    }
    for (const auto& child : node.children) { inspect(child, depth + 1U); }
  }
  // Emit an expanded name using its registered namespace prefix.
  void emit_name(const name& value) {
    if (!value.namespace_uri.empty()) { append(prefixes_.at(value.namespace_uri)); append(":"); }
    append(value.local);
  }
  // Emit one already bounded record, with declarations centralized on the root.
  void emit(const element& node, bool root, std::string_view inherited_default = {}) {
    const auto default_binding = node.namespace_bindings.find("");
    auto effective_default = default_binding == node.namespace_bindings.end() ?
        inherited_default : std::string_view{default_binding->second};
    const bool reset_default = node.qualified_name.namespace_uri.empty() && !effective_default.empty();
    append("<"); emit_name(node.qualified_name);
    if (root) {
      for (const auto& [uri, prefix] : prefixes_) {
        if (prefix == "xml") { continue; }
        append(" xmlns:"); append(prefix); append("=\""); escaped(uri, true); append("\"");
      }
    }
    // Retain QName-valued attribute/text bindings, using distinct prefixes for expanded element names.
    for (const auto& [prefix, uri] : node.namespace_bindings) {
      if (prefix == "xml") { continue; }
      if (prefix.empty() && reset_default) { continue; }
      append(" xmlns");
      if (!prefix.empty()) { append(":"); append(prefix); }
      append("=\""); escaped(uri, true); append("\"");
    }
    if (reset_default) {
      append(" xmlns=\"\"");
      effective_default = {};
    }
    for (const auto& attribute : node.attributes) {
      append(" "); emit_name(attribute.qualified_name); append("=\"");
      escaped(attribute.value, true); append("\"");
    }
    if (node.children.empty() && node.text.empty()) { append("/>"); return; }
    append(">"); escaped(node.text, false);
    for (const auto& child : node.children) { emit(child, false, effective_default); }
    append("</"); emit_name(node.qualified_name); append(">");
  }
public:
  // Bind immutable complete-document limits.
  explicit document_writer(limits bounds) : bounds_{bounds} {}
  // Return one complete document after all semantic/resource checks succeed.
  std::string encode(const element& document) {
    inspect(document, 1U);
    std::size_t next_prefix{};
    for (auto& [uri, prefix] : prefixes_) {
      (void)uri;
      if (prefix == "xml") { continue; }
      do { prefix = "srl" + std::to_string(next_prefix++); } while (retained_prefixes_.contains(prefix));
    }
    emit(document, true); return std::move(bytes_);
  }
};
#if SERIALIZER_HAS_XML
// Copy an optional provider UTF-8 string before advancing its cursor.
std::string copy(const xmlChar* value) { return value ? reinterpret_cast<const char*>(value) : ""; }
// Enforce the UTF-8-only input contract rather than silently overriding another declared encoding.
void check_encoding(std::string_view bytes) {
  if (bytes.starts_with("\xef\xbb\xbf")) { bytes.remove_prefix(3U); }
  if (!bytes.starts_with("<?xml") || bytes.size() <= 5U ||
      std::string_view{" \t\r\n"}.find(bytes[5U]) == std::string_view::npos) { return; }
  const auto end = bytes.find("?>");
  const auto offset = bytes.find("encoding");
  if (end == std::string_view::npos || offset == std::string_view::npos || offset >= end) { return; }
  if (end-offset < std::string_view{"encoding"}.size()) { throw std::invalid_argument{"Malformed XML encoding declaration"}; }
  auto attribute = bytes.substr(offset + std::string_view{"encoding"}.size(), end-offset-std::string_view{"encoding"}.size());
  attribute = detail::trim(attribute);
  if (attribute.empty() || attribute.front() != '=') { throw std::invalid_argument{"Malformed XML encoding declaration"}; }
  attribute.remove_prefix(1U); attribute = detail::trim(attribute);
  if (attribute.empty() || (attribute.front() != '\'' && attribute.front() != '"')) { throw std::invalid_argument{"Malformed XML encoding declaration"}; }
  const auto quote = attribute.front(); attribute.remove_prefix(1U);
  const auto closing = attribute.find(quote);
  const auto value = attribute.substr(0U, closing);
  if (closing == std::string_view::npos || value.size() != 5U ||
      (value[0] != 'u' && value[0] != 'U') || (value[1] != 't' && value[1] != 'T') ||
      (value[2] != 'f' && value[2] != 'F') || value[3] != '-' || value[4] != '8') {
    throw std::invalid_argument{"XML input must declare UTF-8 encoding"};
  }
}
// Keep libxml diagnostics local and return errors through typed exceptions.
void discard_error(void*, const char*, ...) {}
// Reject XSD reference constructs recursively before libxml's schema loader can fetch anything.
void check_schema_references(const element& node) {
  constexpr std::string_view xsd_namespace{"http://www.w3.org/2001/XMLSchema"};
  if (node.qualified_name.namespace_uri == xsd_namespace &&
      (node.qualified_name.local == "include" || node.qualified_name.local == "import" ||
       node.qualified_name.local == "redefine" || node.qualified_name.local == "override")) {
    throw std::invalid_argument{"XSD external references require a bundled resolver (not supported)"};
  }
  for (const auto& child : node.children) { check_schema_references(child); }
}
#endif
} // namespace

bool available() noexcept { return SERIALIZER_HAS_XML != 0; }

element parse(std::string_view document, limits bounds) {
  if (document.size() > bounds.max_input_bytes || document.size() > static_cast<std::size_t>(INT_MAX)) {
    throw std::length_error{"XML input limit exceeded"};
  }
#if SERIALIZER_HAS_XML
  check_encoding(document);
  const auto reader = std::unique_ptr<xmlTextReader, decltype(&xmlFreeTextReader)>{
      xmlReaderForMemory(document.data(), static_cast<int>(document.size()), nullptr, "UTF-8",
                         XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING), &xmlFreeTextReader};
  if (!reader) { throw std::invalid_argument{"Cannot create XML reader"}; }
  element result{};
  std::vector<element*> stack{};
  std::size_t nodes{}, attributes{}, text_bytes{}, name_bytes{}, namespace_bindings{}, tree_bytes{};
  bool root_seen{false};
  int status{};
  while ((status = xmlTextReaderRead(reader.get())) == 1) {
    if (copy(xmlTextReaderConstXmlVersion(reader.get())) != "1.0") {
      throw std::invalid_argument{"Only XML 1.0 documents are supported"};
    }
    const auto type = xmlTextReaderNodeType(reader.get());
    if (type == XML_READER_TYPE_DOCUMENT_TYPE || type == XML_READER_TYPE_ENTITY ||
        type == XML_READER_TYPE_ENTITY_REFERENCE) {
      throw std::invalid_argument{"XML DTDs and entities are prohibited"};
    }
    if (type == XML_READER_TYPE_ELEMENT) {
      consume(nodes, 1U, bounds.max_nodes);
      consume(tree_bytes, sizeof(element), bounds.max_tree_bytes);
      if (stack.size() >= bounds.max_depth) { throw std::length_error{"XML depth limit exceeded"}; }
      element* node{};
      if (stack.empty()) {
        if (root_seen) { throw std::invalid_argument{"Multiple XML roots"}; }
        root_seen = true; node = &result;
      } else {
        stack.back()->children.emplace_back(); node = &stack.back()->children.back();
      }
      node->qualified_name = {copy(xmlTextReaderConstLocalName(reader.get())),
                              copy(xmlTextReaderConstNamespaceUri(reader.get()))};
      consume(name_bytes, node->qualified_name.local.size(), bounds.max_name_bytes);
      consume(name_bytes, node->qualified_name.namespace_uri.size(), bounds.max_name_bytes);
      consume(tree_bytes, node->qualified_name.local.size(), bounds.max_tree_bytes);
      consume(tree_bytes, node->qualified_name.namespace_uri.size(), bounds.max_tree_bytes);
      if (!stack.empty()) {
        // Charge inherited maps before copying; compact nested documents cannot multiply metadata unboundedly.
        consume(namespace_bindings, stack.back()->namespace_bindings.size(), bounds.max_namespace_bindings);
        for (const auto& [prefix, uri] : stack.back()->namespace_bindings) {
          consume(name_bytes, prefix.size(), bounds.max_name_bytes);
          consume(name_bytes, uri.size(), bounds.max_name_bytes);
          consume(tree_bytes, sizeof(std::pair<const std::string, std::string>), bounds.max_tree_bytes);
          consume(tree_bytes, prefix.size(), bounds.max_tree_bytes);
          consume(tree_bytes, uri.size(), bounds.max_tree_bytes);
        }
        node->namespace_bindings = stack.back()->namespace_bindings;
      }
      check_name(node->qualified_name.local);
      if (xmlTextReaderMoveToFirstAttribute(reader.get()) == 1) {
        do {
          if (xmlTextReaderIsNamespaceDecl(reader.get()) == 1) {
            const auto prefix = copy(xmlTextReaderConstPrefix(reader.get())) == "xmlns" ?
                copy(xmlTextReaderConstLocalName(reader.get())) : std::string{};
            const auto uri = copy(xmlTextReaderConstValue(reader.get()));
            consume(namespace_bindings, 1U, bounds.max_namespace_bindings);
            consume(name_bytes, prefix.size(), bounds.max_name_bytes);
            consume(name_bytes, uri.size(), bounds.max_name_bytes);
            consume(tree_bytes, sizeof(std::pair<const std::string, std::string>), bounds.max_tree_bytes);
            consume(tree_bytes, prefix.size(), bounds.max_tree_bytes);
            consume(tree_bytes, uri.size(), bounds.max_tree_bytes);
            node->namespace_bindings[prefix] = uri;
            continue;
          }
          consume(attributes, 1U, bounds.max_attributes);
          consume(tree_bytes, sizeof(attribute), bounds.max_tree_bytes);
          attribute item{{copy(xmlTextReaderConstLocalName(reader.get())),
                           copy(xmlTextReaderConstNamespaceUri(reader.get()))},
                          copy(xmlTextReaderConstValue(reader.get()))};
          check_name(item.qualified_name.local);
          consume(text_bytes, item.value.size(), bounds.max_text_bytes);
          consume(name_bytes, item.qualified_name.local.size(), bounds.max_name_bytes);
          consume(name_bytes, item.qualified_name.namespace_uri.size(), bounds.max_name_bytes);
          consume(tree_bytes, item.value.size(), bounds.max_tree_bytes);
          consume(tree_bytes, item.qualified_name.local.size(), bounds.max_tree_bytes);
          consume(tree_bytes, item.qualified_name.namespace_uri.size(), bounds.max_tree_bytes);
          node->attributes.push_back(std::move(item));
        } while (xmlTextReaderMoveToNextAttribute(reader.get()) == 1);
        xmlTextReaderMoveToElement(reader.get());
      }
      if (xmlTextReaderIsEmptyElement(reader.get()) != 1) { stack.push_back(node); }
    } else if (type == XML_READER_TYPE_END_ELEMENT) {
      if (stack.empty()) { throw std::invalid_argument{"Unexpected XML closing element"}; }
      const auto* node = stack.back();
      if (!node->children.empty() && !detail::trim(node->text).empty()) {
        throw std::invalid_argument{"XML mixed content is unsupported"};
      }
      stack.pop_back();
    } else if (type == XML_READER_TYPE_TEXT || type == XML_READER_TYPE_CDATA ||
               type == XML_READER_TYPE_WHITESPACE || type == XML_READER_TYPE_SIGNIFICANT_WHITESPACE) {
      const auto text = copy(xmlTextReaderConstValue(reader.get()));
      consume(text_bytes, text.size(), bounds.max_text_bytes);
      consume(tree_bytes, text.size(), bounds.max_tree_bytes);
      if (!stack.empty()) { stack.back()->text += text; }
      else if (!detail::trim(text).empty()) { throw std::invalid_argument{"Text outside XML root"}; }
    }
  }
  if (status != 0 || !stack.empty() || !root_seen) { throw std::invalid_argument{"Malformed XML document"}; }
  return result;
#else
  (void)document; (void)bounds;
  throw std::runtime_error{"XML support was not enabled in this Serializer runtime"};
#endif
}

std::string write(const element& document, limits bounds) {
  return document_writer{bounds}.encode(document);
}

struct prepared_schema::implementation {
#if SERIALIZER_HAS_XML
  std::unique_ptr<xmlSchema, decltype(&xmlSchemaFree)> schema{nullptr, &xmlSchemaFree};
#endif
};

prepared_schema::prepared_schema(std::string_view xsd, limits bounds) {
#if SERIALIZER_HAS_XML
  const auto tree = parse(xsd, bounds);
  check_schema_references(tree);
  const auto context = std::unique_ptr<xmlSchemaParserCtxt, decltype(&xmlSchemaFreeParserCtxt)>{
      xmlSchemaNewMemParserCtxt(xsd.data(), static_cast<int>(xsd.size())), &xmlSchemaFreeParserCtxt};
  if (!context) { throw std::invalid_argument{"Cannot prepare XSD parser"}; }
  xmlSchemaSetParserErrors(context.get(), &discard_error, &discard_error, nullptr);
  implementation_ = std::make_unique<implementation>();
  implementation_->schema.reset(xmlSchemaParse(context.get()));
  if (!implementation_->schema) { throw std::invalid_argument{"Invalid or unsupported XSD 1.0 schema"}; }
#else
  (void)xsd; (void)bounds;
  throw std::runtime_error{"Runtime XSD support was not enabled in this Serializer runtime"};
#endif
}
prepared_schema::~prepared_schema() = default;
prepared_schema::prepared_schema(prepared_schema&&) noexcept = default;
prepared_schema& prepared_schema::operator=(prepared_schema&&) noexcept = default;

void prepared_schema::validate(std::string_view document, limits bounds) const {
#if SERIALIZER_HAS_XML
  (void)parse(document, bounds);
  if (!implementation_) { throw std::logic_error{"Moved-from prepared XML schema"}; }
  const auto tree = std::unique_ptr<xmlDoc, decltype(&xmlFreeDoc)>{
      xmlReadMemory(document.data(), static_cast<int>(document.size()), nullptr, "UTF-8",
                    XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING), &xmlFreeDoc};
  const auto context = std::unique_ptr<xmlSchemaValidCtxt, decltype(&xmlSchemaFreeValidCtxt)>{
      xmlSchemaNewValidCtxt(implementation_->schema.get()), &xmlSchemaFreeValidCtxt};
  if (!tree || !context) { throw std::runtime_error{"Cannot prepare XML validation context"}; }
  xmlSchemaSetValidErrors(context.get(), &discard_error, &discard_error, nullptr);
  if (xmlSchemaValidateDoc(context.get(), tree.get()) != 0) {
    throw std::invalid_argument{"XML document does not satisfy the selected XSD"};
  }
#else
  (void)document; (void)bounds;
  throw std::runtime_error{"Runtime XSD support was not enabled in this Serializer runtime"};
#endif
}
} // namespace rohit::serializer::xml
