// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#include <rohit/package.hpp>

#include <rohit/xml.hpp>

#include <algorithm>
#include <cctype>
#include <exception>
#include <limits>
#include <map>
#include <set>
#include <tuple>
#include <utility>

namespace rohit::serializer::package {
namespace {
constexpr std::string_view content_types_namespace =
    "http://schemas.openxmlformats.org/package/2006/content-types";
constexpr std::string_view relationships_namespace =
    "http://schemas.openxmlformats.org/package/2006/relationships";
constexpr std::string_view relationships_content_type =
    "application/vnd.openxmlformats-package.relationships+xml";
constexpr std::string_view manifest_name = "[Content_Types].xml";
constexpr std::string_view relationships_suffix = ".rels";

// Apply OPC's ASCII case-insensitive comparison without locale or Unicode folding.
std::string identity(std::string_view text) {
  std::string result{text};
  for (auto& value : result) {
    if (value >= 'A' && value <= 'Z') {
      value = static_cast<char>(value - 'A' + 'a');
    }
  }
  return result;
}

// Check exact/derived OPC identities with ancestor lookups and one descendant range lookup.
std::string checked_identity(const std::set<std::string, std::less<>>& names,
                             std::string_view name) {
  auto key = identity(name);
  if (names.contains(key)) {
    throw error{error_code::duplicate_name, "Duplicate OPC part identity", std::string{name}};
  }
  // A lexical neighbor such as /a-1 can hide /a from /a/b; look up each full ancestor.
  for (auto separator = key.find('/', 1); separator != std::string::npos;
       separator = key.find('/', separator + 1)) {
    if (names.contains(std::string_view{key}.substr(0, separator))) {
      throw error{error_code::invalid_name, "OPC part name derives from another part", std::string{name}};
    }
  }
  const auto prefix = key + '/';
  const auto descendant = names.lower_bound(prefix);
  if (descendant != names.end() && descendant->starts_with(prefix)) {
    throw error{error_code::invalid_name, "OPC part name is an ancestor of another part", std::string{name}};
  }
  return key;
}

// Recognize ASCII letters independently of the active C locale.
bool letter(char value) {
  return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

// Recognize RFC 3986 unreserved bytes, which OPC names must not percent-encode.
bool unreserved(char value) {
  return letter(value) || (value >= '0' && value <= '9') ||
      value == '-' || value == '.' || value == '_' || value == '~';
}

// Decode one hexadecimal URI digit without accepting locale extensions.
int hex_digit(char value) {
  if (value >= '0' && value <= '9') { return value - '0'; }
  if (value >= 'A' && value <= 'F') { return value - 'A' + 10; }
  if (value >= 'a' && value <= 'f') { return value - 'a' + 10; }
  return -1;
}

// Check URI bytes once; external relationship URIs remain inert and need no resolver.
void validate_uri(std::string_view value, bool absolute) {
  if (value.empty()) {
    throw error{error_code::invalid_relationship, "Empty relationship URI"};
  }
  for (std::size_t index = 0; index < value.size(); ++index) {
    const auto byte = static_cast<unsigned char>(value[index]);
    if (byte <= 0x20 || byte >= 0x7f || value[index] == '\\' ||
        value[index] == '<' || value[index] == '>' || value[index] == '"') {
      throw error{error_code::invalid_relationship, "Invalid relationship URI"};
    }
    if (value[index] == '%') {
      if (value.size() - index < 3 || hex_digit(value[index + 1]) < 0 ||
          hex_digit(value[index + 2]) < 0) {
        throw error{error_code::invalid_relationship, "Malformed URI escape"};
      }
      index += 2;
    }
  }
  if (absolute) {
    const auto colon = value.find(':');
    if (colon == std::string_view::npos || colon == 0 || !letter(value.front()) ||
        colon + 1 == value.size()) {
      throw error{error_code::invalid_relationship, "Relationship type requires an absolute URI"};
    }
    for (const auto value_byte : value.substr(1, colon - 1)) {
      if (!letter(value_byte) && (value_byte < '0' || value_byte > '9') &&
          value_byte != '+' && value_byte != '-' && value_byte != '.') {
        throw error{error_code::invalid_relationship, "Invalid URI scheme"};
      }
    }
  }
}

// Validate canonical OPC part URI syntax before comparing or translating it to ZIP storage.
void validate_part_name(std::string_view name) {
  constexpr auto max_name_bytes = std::size_t{1} + std::numeric_limits<std::uint16_t>::max();
  if (name.size() < 2 || name.size() > max_name_bytes || name.front() != '/' || name.back() == '/' ||
      name.find_first_of("?#:\\") != std::string_view::npos) {
    throw error{error_code::invalid_name, "Invalid OPC part URI"};
  }
  for (std::size_t begin = 1; begin < name.size();) {
    const auto end = name.find('/', begin);
    const auto segment = name.substr(begin, end == std::string_view::npos ? end : end - begin);
    if (segment.empty() || segment.back() == '.') {
      throw error{error_code::invalid_name, "Invalid OPC part URI segment"};
    }
    for (std::size_t index = 0; index < segment.size(); ++index) {
      const auto value = segment[index];
      if (value == '%') {
        if (segment.size() - index < 3 || hex_digit(segment[index + 1]) < 0 ||
            hex_digit(segment[index + 2]) < 0) {
          throw error{error_code::invalid_name, "Malformed OPC part URI escape"};
        }
        const auto decoded = static_cast<char>((hex_digit(segment[index + 1]) << 4) |
                                               hex_digit(segment[index + 2]));
        // This ASCII subset also refuses escaped non-ASCII octets; I18N part names need
        // a separate Unicode/IRI policy and must not smuggle encoded iunreserved characters.
        if (unreserved(decoded) || decoded == '/' || decoded == '\\' ||
            static_cast<unsigned char>(decoded) < 0x20 ||
            static_cast<unsigned char>(decoded) >= 0x7f) {
          throw error{error_code::invalid_name, "Forbidden OPC part URI escape"};
        }
        index += 2;
      } else if (!unreserved(value) && std::string_view{"!$&'()*+,;=@"}.find(value) ==
                                           std::string_view::npos) {
        throw error{error_code::invalid_name, "Invalid OPC part URI character"};
      }
    }
    if (end == std::string_view::npos) { break; }
    begin = end + 1;
  }
}

// Return the source for a relationship part, or an empty string for a normal payload part.
std::string relationship_source(std::string_view name) {
  if (identity(name) == "/_rels/.rels") { return "/"; }
  const auto separator = identity(name).rfind("/_rels/");
  if (separator == std::string_view::npos || name.size() < relationships_suffix.size() ||
      identity(name.substr(name.size() - relationships_suffix.size())) != relationships_suffix) {
    return {};
  }
  const auto basename = name.substr(separator + 7,
      name.size() - separator - 7 - relationships_suffix.size());
  if (basename.empty() || basename.find('/') != std::string_view::npos) {
    throw error{error_code::invalid_metadata, "Invalid relationship part name"};
  }
  auto result = std::string{name.substr(0, separator + 1)};
  result += basename;
  validate_part_name(result);
  return result;
}

// Derive standard .rels ZIP names from a package or part relationship source.
std::string relationship_name(std::string_view source) {
  if (source == "/") { return "_rels/.rels"; }
  const auto separator = source.rfind('/');
  return std::string{source.substr(1, separator)} + "_rels/" +
      std::string{source.substr(separator + 1)} + std::string{relationships_suffix};
}

// Reserve all infrastructure identities for generated metadata, including root .rels.
void validate_payload_name(std::string_view name) {
  validate_part_name(name);
  if (identity(name) == "/[content_types].xml" || !relationship_source(name).empty()) {
    throw error{error_code::invalid_name, "OPC infrastructure names are reserved"};
  }
}

// MIME tokens exclude separators and controls; parameters are explicitly parsed below.
bool mime_token(char value) {
  return static_cast<unsigned char>(value) > 0x20 &&
      static_cast<unsigned char>(value) < 0x7f &&
      std::string_view{R"mime(()<>@,;:\"/[]?=)mime"}.find(value) == std::string_view::npos;
}

// Validate media type syntax without imposing application vocabulary semantics.
void validate_content_type(std::string_view value) {
  std::size_t position{};
  // Consume a nonempty RFC token.
  const auto token = [&] {
    const auto begin = position;
    while (position < value.size() && mime_token(value[position])) { ++position; }
    return position != begin;
  };
  if (!token() || position == value.size() || value[position++] != '/' || !token()) {
    throw error{error_code::invalid_metadata, "Invalid OPC content type"};
  }
  while (position < value.size()) {
    while (position < value.size() && value[position] == ' ') { ++position; }
    if (position == value.size() || value[position++] != ';') {
      throw error{error_code::invalid_metadata, "Invalid OPC media type parameter"};
    }
    while (position < value.size() && value[position] == ' ') { ++position; }
    if (!token() || position == value.size() || value[position++] != '=') {
      throw error{error_code::invalid_metadata, "Invalid OPC media type parameter"};
    }
    if (position < value.size() && value[position] == '"') {
      ++position;
      bool closed{};
      while (position < value.size()) {
        auto byte = static_cast<unsigned char>(value[position++]);
        if (byte == '"') { closed = true; break; }
        if (byte == '\\' && position < value.size()) {
          byte = static_cast<unsigned char>(value[position++]);
        }
        if (byte < 0x20 || byte >= 0x7f) {
          throw error{error_code::invalid_metadata, "Invalid quoted media type parameter"};
        }
      }
      if (!closed) { throw error{error_code::invalid_metadata, "Unclosed media type parameter"}; }
    } else if (!token()) {
      throw error{error_code::invalid_metadata, "Empty media type parameter"};
    }
  }
}

// Validate a source-local ASCII XML NCName, a declared subset of valid relationship IDs.
void validate_id(std::string_view value) {
  if (value.empty() || (!letter(value.front()) && value.front() != '_')) {
    throw error{error_code::invalid_relationship, "Relationship ID is not an ASCII NCName"};
  }
  for (const auto byte : value) {
    if (!letter(byte) && (byte < '0' || byte > '9') && byte != '_' && byte != '.' && byte != '-') {
      throw error{error_code::invalid_relationship, "Relationship ID is not an ASCII NCName"};
    }
  }
}

// Validate one link independently of target existence so forward references are supported.
void validate_relationship(const relationship& link) {
  if (link.source != "/") {
    validate_payload_name(link.source);
  }
  validate_id(link.id);
  validate_uri(link.type, true);
  if (link.mode == target_mode::internal) {
    static_cast<void>(resolve_target(link.source, link.target));
  } else {
    validate_uri(link.target, false);
  }
}

// Find OPC parts using their specified ASCII case-insensitive identity.
const part* find_part(const std::vector<part>& parts, std::string_view name) {
  const auto key = identity(name);
  const auto found = std::find_if(parts.begin(), parts.end(), [&key](const auto& item) {
    return identity(item.name) == key;
  });
  return found == parts.end() ? nullptr : &*found;
}

// Validate reference existence after all declarations, including forward links, are known.
void validate_links(const std::vector<part>& parts, const std::vector<relationship>& links,
                    budget& shared) {
  std::set<std::pair<std::string, std::string>> ids;
  for (const auto& link : links) {
    shared.check_cancelled();
    validate_relationship(link);
    if (!ids.emplace(identity(link.source), link.id).second) {
      throw error{error_code::invalid_relationship, "Duplicate source-local relationship ID",
                  link.source};
    }
    if (link.source != "/" && !find_part(parts, link.source)) {
      throw error{error_code::missing_part, "Relationship source part does not exist", link.source};
    }
    if (link.mode == target_mode::internal) {
      const auto target = resolve_target(link.source, link.target);
      if (!find_part(parts, target)) {
        throw error{error_code::missing_part, "Relationship target part does not exist", target};
      }
    }
  }
}

// Bound metadata XML through the same operation policy, with no package-supplied resolver.
xml::limits xml_bounds(const budget& shared) {
  const auto& policy = shared.configured();
  const auto maximum = static_cast<std::size_t>(std::min({policy.max_part_bytes,
      shared.remaining(resource::metadata_bytes), policy.max_buffer_bytes,
      static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())}));
  xml::limits result{};
  result.max_input_bytes = maximum;
  result.max_output_bytes = maximum;
  result.max_text_bytes = maximum;
  result.max_nodes = maximum / sizeof(xml::element);
  result.max_attributes = maximum / sizeof(xml::attribute);
  result.max_depth = 8;
  result.max_tree_bytes = maximum;
  return result;
}

// Require optional XML support before reading or emitting package infrastructure.
void require_xml() {
  if (!xml::available()) {
    throw error{error_code::unavailable, "OPC metadata requires XML support in this build"};
  }
}

// Construct namespace-aware metadata elements without maintaining a second XML implementation.
xml::element node(std::string_view local, std::string_view namespace_uri) {
  xml::element result{};
  result.qualified_name = {std::string{local}, std::string{namespace_uri}};
  return result;
}

// Add one unqualified infrastructure attribute; the shared XML writer performs escaping.
void attribute(xml::element& element, std::string_view name, std::string_view value) {
  element.attributes.push_back({{std::string{name}, {}}, std::string{value}});
}

// Require the exact expanded root/child name rather than a document prefix spelling.
void require_name(const xml::element& element, std::string_view local, std::string_view uri) {
  if (element.qualified_name.local != local || element.qualified_name.namespace_uri != uri) {
    throw error{error_code::invalid_metadata, "Unexpected OPC metadata expanded name"};
  }
}

// Reject undeclared attributes and nested/text content in flat OPC metadata records.
std::map<std::string, std::string> attributes(
    const xml::element& element, std::initializer_list<std::string_view> permitted) {
  if (!element.children.empty() ||
      element.text.find_first_not_of(" \t\r\n") != std::string::npos) {
    throw error{error_code::invalid_metadata, "Unexpected OPC metadata content"};
  }
  std::map<std::string, std::string> result;
  for (const auto& item : element.attributes) {
    if (!item.qualified_name.namespace_uri.empty() ||
        std::find(permitted.begin(), permitted.end(), item.qualified_name.local) == permitted.end() ||
        !result.emplace(item.qualified_name.local, item.value).second) {
      throw error{error_code::invalid_metadata, "Unexpected or duplicate OPC metadata attribute"};
    }
  }
  return result;
}

// Read one required nonempty metadata attribute with no map insertion side effect.
const std::string& required(const std::map<std::string, std::string>& values,
                            std::string_view key) {
  const auto found = values.find(std::string{key});
  if (found == values.end() || found->second.empty()) {
    throw error{error_code::invalid_metadata, "Required OPC metadata attribute missing"};
  }
  return found->second;
}

// Convert validated metadata output to the codec-independent ZIP byte interface.
void write_metadata(zip_writer& archive, std::string_view name, const xml::element& document) {
  auto& shared = *archive.shared_budget();
  shared.check_cancelled();
  auto context = shared.xml_context(name);
  const auto text = xml::write(document, xml_bounds(shared), &context);
  shared.consume(resource::metadata_bytes, text.size(), name);
  archive.add(name, {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
}

// Parse only a selected infrastructure entry and charge bounded tree/index storage conservatively.
xml::element read_metadata(zip_reader& archive, std::string_view name) {
  auto& shared = *archive.shared_budget();
  const auto* entry = archive.find(name);
  if (!entry) { throw error{error_code::missing_part, "OPC metadata part is absent", std::string{name}}; }
  shared.consume(resource::metadata_bytes, entry->expanded_bytes, name);
  const auto bytes = archive.read(name);
  try {
    auto context = shared.xml_context(name);
    auto document = xml::parse({reinterpret_cast<const char*>(bytes.data()), bytes.size()},
                               xml_bounds(shared), &context);
    shared.check_cancelled();
    // Each retained tree item is bounded independently of its compressed input length.
    std::uint64_t storage{};
    // Charge tree storage, including separately allocated strings and attributes.
    const auto measure = [&storage](const auto& self, const xml::element& element) -> void {
      storage += sizeof(xml::element) + element.qualified_name.local.size() +
          element.qualified_name.namespace_uri.size() + element.text.size();
      for (const auto& [prefix, uri] : element.namespace_bindings) {
        storage += sizeof(typename decltype(element.namespace_bindings)::value_type) + prefix.size() + uri.size();
      }
      for (const auto& item : element.attributes) {
        storage += sizeof(xml::attribute) + item.qualified_name.local.size() +
            item.qualified_name.namespace_uri.size() + item.value.size();
      }
      for (const auto& child : element.children) { self(self, child); }
    };
    measure(measure, document);
    shared.consume(resource::metadata_bytes, storage, name);
    return document;
  } catch (const error&) {
    throw;
  } catch (const std::exception&) {
    throw error{error_code::invalid_metadata, "Invalid OPC metadata XML", std::string{name}};
  }
}

// Validate an OPC extension default and use case-insensitive extension matching.
std::string extension_identity(std::string_view extension) {
  if (extension.empty() || std::any_of(extension.begin(), extension.end(), [](char byte) {
        return !unreserved(byte) || byte == '.';
      })) {
    throw error{error_code::invalid_metadata, "Invalid OPC content-type extension"};
  }
  return identity(extension);
}

// Return a part's final extension, excluding directory names.
std::string extension_of(std::string_view name) {
  const auto dot = name.rfind('.');
  const auto slash = name.rfind('/');
  return dot == std::string_view::npos || dot < slash ? std::string{} : identity(name.substr(dot + 1));
}
} // namespace

// Adapt only selected XML operations; native binary/JSON codecs retain their existing fast paths.
xml::operation_context budget::xml_context(std::string_view part) {
  xml::operation_context result{};
  result.is_cancelled = [this] { check_cancelled(); return false; };
  result.charge = [this, source = std::string{part}](xml::operation_resource selected,
      std::size_t amount) {
    switch (selected) {
    case xml::operation_resource::allocation_bytes:
      consume(resource::decoder_allocation_bytes, amount, source); break;
    case xml::operation_resource::work_units:
      consume(resource::work_units, amount, source); break;
    case xml::operation_resource::preserved_bytes:
      consume(resource::preserved_bytes, amount, source); break;
    }
  };
  return result;
}

// Remove literal dot segments once, then apply canonical OPC part-name validation.
std::string resolve_target(std::string_view source, std::string_view target) {
  if (source != "/") { validate_payload_name(source); }
  validate_uri(target, false);
  if (target.find_first_of("?#:") != std::string_view::npos || target.starts_with("//")) {
    throw error{error_code::invalid_relationship, "Unsupported internal OPC target URI"};
  }
  std::string path;
  if (target.front() == '/') {
    path = target;
  } else {
    path = std::string{source.substr(0, source.rfind('/') + 1)} + std::string{target};
  }
  std::vector<std::string_view> segments;
  for (std::size_t begin = 1; begin <= path.size();) {
    const auto end = path.find('/', begin);
    const auto segment = std::string_view{path}.substr(
        begin, end == std::string::npos ? end : end - begin);
    if (segment == "..") {
      if (segments.empty()) {
        throw error{error_code::invalid_relationship, "OPC target escapes package root"};
      }
      segments.pop_back();
    } else if (segment != ".") {
      if (segment.empty()) {
        throw error{error_code::invalid_relationship, "Empty OPC target segment"};
      }
      segments.push_back(segment);
    }
    if (end == std::string::npos) { break; }
    begin = end + 1;
  }
  std::string result;
  for (const auto segment : segments) {
    result += '/';
    result += segment;
  }
  validate_payload_name(result);
  return result;
}

// Check optional XML dependencies before the first package byte is written.
opc_writer::opc_writer(std::ostream& output, std::shared_ptr<budget> shared)
    : archive_{output, std::move(shared)} { require_xml(); }

// Defaults supplement explicit overrides; duplicate declarations never silently replace each other.
void opc_writer::content_type_default(std::string_view extension, std::string_view content_type) {
  if (finished_ || failed_) { throw error{error_code::invalid_state, "OPC writer is finished or failed"}; }
  const auto key = extension_identity(extension);
  validate_content_type(content_type);
  for (const auto& item : defaults_) {
    if (item.first == key) { throw error{error_code::invalid_metadata, "Duplicate content-type default"}; }
  }
  archive_.shared_budget()->consume(resource::metadata_bytes,
      sizeof(decltype(defaults_)::value_type) + key.size() + content_type.size());
  defaults_.emplace_back(key, content_type);
}

// Keep the public part model and physical ZIP entry in lockstep; failed writes cannot finalize.
void opc_writer::add_part(std::string_view name, std::string_view content_type,
                          std::span<const std::uint8_t> bytes, entry_options options) {
  if (finished_ || failed_) { throw error{error_code::invalid_state, "OPC writer is finished or failed"}; }
  validate_payload_name(name);
  validate_content_type(content_type);
  auto key = checked_identity(identities_, name);
  archive_.shared_budget()->consume(resource::metadata_bytes,
      sizeof(part) + sizeof(std::string) + name.size() + key.size() + content_type.size(), name);
  try {
    identities_.insert(std::move(key));
    parts_.push_back({std::string{name}, std::string{content_type}, bytes.size()});
    archive_.add(name.substr(1), bytes, options);
  } catch (...) { failed_ = true; throw; }
}

// Compose an existing whole-object serializer with one bounded OPC part stream.
void opc_writer::write_part(std::string_view name, std::string_view content_type,
                            const std::function<void(std::ostream&)>& producer,
                            entry_options options) {
  if (finished_ || failed_) { throw error{error_code::invalid_state, "OPC writer is finished or failed"}; }
  validate_payload_name(name);
  validate_content_type(content_type);
  auto key = checked_identity(identities_, name);
  archive_.shared_budget()->consume(resource::metadata_bytes,
      sizeof(part) + sizeof(std::string) + name.size() + key.size() + content_type.size(), name);
  try {
    identities_.insert(std::move(key));
    archive_.write(name.substr(1), producer, options);
    parts_.push_back({std::string{name}, std::string{content_type}, 0});
  } catch (...) { failed_ = true; throw; }
}

// Preserve authored IDs/targets while enforcing source-local uniqueness immediately.
void opc_writer::add_relationship(relationship link) {
  if (finished_ || failed_) { throw error{error_code::invalid_state, "OPC writer is finished or failed"}; }
  validate_relationship(link);
  for (const auto& existing : relationships_) {
    if (identity(existing.source) == identity(link.source) && existing.id == link.id) {
      throw error{error_code::invalid_relationship, "Duplicate source-local relationship ID", link.source};
    }
  }
  auto& shared = *archive_.shared_budget();
  shared.consume(resource::relationships, 1, link.source);
  shared.consume(resource::metadata_bytes, sizeof(relationship) + link.source.size() +
      link.id.size() + link.type.size() + link.target.size(), link.source);
  relationships_.push_back(std::move(link));
}

// Generate standard namespace-aware infrastructure without any private Serializer envelope.
void opc_writer::finish() {
  if (failed_) { throw error{error_code::invalid_state, "Cannot finalize failed OPC writer"}; }
  if (finished_) { return; }
  try {
    validate_links(parts_, relationships_, *archive_.shared_budget());
    auto manifest = node("Types", content_types_namespace);
    for (const auto& item : defaults_) {
      auto declaration = node("Default", content_types_namespace);
      attribute(declaration, "Extension", item.first);
      attribute(declaration, "ContentType", item.second);
      manifest.children.push_back(std::move(declaration));
    }
    for (const auto& item : parts_) {
      auto declaration = node("Override", content_types_namespace);
      attribute(declaration, "PartName", item.name);
      attribute(declaration, "ContentType", item.content_type);
      manifest.children.push_back(std::move(declaration));
    }
    std::map<std::string, std::vector<const relationship*>> sets;
    for (const auto& link : relationships_) { sets[identity(link.source)].push_back(&link); }
    for (const auto& [source, links] : sets) {
      auto document = node("Relationships", relationships_namespace);
      for (const auto* link : links) {
        auto declaration = node("Relationship", relationships_namespace);
        attribute(declaration, "Id", link->id);
        attribute(declaration, "Type", link->type);
        attribute(declaration, "Target", link->target);
        if (link->mode == target_mode::external) { attribute(declaration, "TargetMode", "External"); }
        document.children.push_back(std::move(declaration));
      }
      // Use an authored source spelling so relationship names track its actual part spelling.
      const auto rel_name = relationship_name(links.front()->source);
      auto key = checked_identity(identities_, "/" + rel_name);
      archive_.shared_budget()->consume(resource::metadata_bytes,
          sizeof(std::string) + key.size(), rel_name);
      identities_.insert(std::move(key));
      write_metadata(archive_, rel_name, document);
      auto declaration = node("Override", content_types_namespace);
      attribute(declaration, "PartName", "/" + rel_name);
      attribute(declaration, "ContentType", relationships_content_type);
      manifest.children.push_back(std::move(declaration));
      static_cast<void>(source);
    }
    write_metadata(archive_, manifest_name, manifest);
    archive_.finish();
    finished_ = true;
  } catch (...) { failed_ = true; throw; }
}

// Return the underlying archive's cumulative operation budget.
const std::shared_ptr<budget>& opc_writer::shared_budget() const noexcept {
  return archive_.shared_budget();
}

// Read OPC manifests/rels through the shared XML provider; defer only payload decompression.
opc_reader::opc_reader(std::istream& input, std::shared_ptr<budget> shared, input_options options)
    : archive_{input, std::move(shared), std::move(options)} {
  require_xml();
  const auto manifest = read_metadata(archive_, manifest_name);
  require_name(manifest, "Types", content_types_namespace);
  if (!manifest.attributes.empty() ||
      manifest.text.find_first_not_of(" \t\r\n") != std::string::npos) {
    throw error{error_code::invalid_metadata, "Invalid content-types root content"};
  }
  std::map<std::string, std::string> defaults;
  std::map<std::string, std::pair<std::string, std::string>> overrides;
  for (const auto& child : manifest.children) {
    archive_.shared_budget()->check_cancelled();
    if (child.qualified_name.local == "Default") {
      require_name(child, "Default", content_types_namespace);
      const auto values = attributes(child, {"Extension", "ContentType"});
      const auto key = extension_identity(required(values, "Extension"));
      const auto& type = required(values, "ContentType");
      validate_content_type(type);
      if (!defaults.emplace(key, type).second) {
        throw error{error_code::invalid_metadata, "Duplicate content-type default"};
      }
    } else {
      require_name(child, "Override", content_types_namespace);
      const auto values = attributes(child, {"PartName", "ContentType"});
      const auto& name = required(values, "PartName");
      const auto& type = required(values, "ContentType");
      validate_part_name(name);
      validate_content_type(type);
      if (!overrides.emplace(identity(name), std::pair{name, type}).second) {
        throw error{error_code::invalid_metadata, "Duplicate content-type override"};
      }
    }
  }
  std::set<std::string, std::less<>> identities;
  std::vector<std::pair<std::string, std::string>> relationship_parts;
  for (const auto& entry : archive_.entries()) {
    if (entry.name == manifest_name) { continue; }
    const auto name = "/" + entry.name;
    validate_part_name(name);
    auto key = checked_identity(identities, name);
    archive_.shared_budget()->consume(resource::metadata_bytes,
        sizeof(std::string) + key.size(), name);
    identities.insert(std::move(key));
    const auto override = overrides.find(identity(name));
    const auto fallback = defaults.find(extension_of(name));
    const auto type = override != overrides.end() ? override->second.second
        : fallback != defaults.end() ? fallback->second : std::string{};
    if (type.empty()) { throw error{error_code::invalid_metadata, "OPC part lacks a content type", name}; }
    const auto source = relationship_source(name);
    if (!source.empty()) {
      if (identity(type) != relationships_content_type) {
        throw error{error_code::invalid_metadata, "Relationship part has wrong content type", name};
      }
      relationship_parts.emplace_back(entry.name, source);
    } else {
      archive_.shared_budget()->consume(resource::metadata_bytes,
          sizeof(part) + name.size() + type.size(), name);
      parts_.push_back({name, type, entry.expanded_bytes});
    }
  }
  for (const auto& [key, declaration] : overrides) {
    if (!identities.contains(key)) {
      throw error{error_code::invalid_metadata, "Content-type override names an absent part",
                  declaration.first};
    }
  }
  for (const auto& [name, source] : relationship_parts) {
    if (source != "/" && !find_part(parts_, source)) {
      throw error{error_code::missing_part, "Relationship source part does not exist", source};
    }
    const auto document = read_metadata(archive_, name);
    require_name(document, "Relationships", relationships_namespace);
    if (!document.attributes.empty() ||
        document.text.find_first_not_of(" \t\r\n") != std::string::npos) {
      throw error{error_code::invalid_metadata, "Invalid relationships root content", name};
    }
    for (const auto& child : document.children) {
      require_name(child, "Relationship", relationships_namespace);
      const auto values = attributes(child, {"Id", "Type", "Target", "TargetMode"});
      relationship link{source, required(values, "Id"), required(values, "Type"),
                         required(values, "Target"), target_mode::internal};
      const auto mode = values.find("TargetMode");
      if (mode != values.end()) {
        if (mode->second == "External") { link.mode = target_mode::external; }
        else if (mode->second != "Internal") {
          throw error{error_code::invalid_relationship, "Invalid relationship TargetMode", name};
        }
      }
      validate_relationship(link);
      archive_.shared_budget()->consume(resource::relationships, 1, source);
      archive_.shared_budget()->consume(resource::metadata_bytes, sizeof(relationship) +
          link.source.size() + link.id.size() + link.type.size() + link.target.size(), source);
      relationships_.push_back(std::move(link));
    }
  }
  validate_links(parts_, relationships_, *archive_.shared_budget());
}

// Return metadata for user payloads only; infrastructure stays in the archive layer.
const std::vector<part>& opc_reader::parts() const noexcept { return parts_; }

// Return authored relationship metadata without fetching external targets.
const std::vector<relationship>& opc_reader::relationships() const noexcept { return relationships_; }

// Discover typed roles without hardcoded payload filenames or implicit cardinality policy.
std::vector<const relationship*> opc_reader::find_relationships(
    std::string_view source, std::string_view type) const {
  std::vector<const relationship*> result;
  for (const auto& link : relationships_) {
    archive_.shared_budget()->check_cancelled();
    if (identity(link.source) == identity(source) && link.type == type) { result.push_back(&link); }
  }
  return result;
}

// Resolve OPC identity consistently with duplicate detection and URI target resolution.
const part* opc_reader::find(std::string_view name) const {
  validate_payload_name(name);
  return find_part(parts_, name);
}

// Decode no unrelated typed payload when the caller opens one selected part.
std::vector<std::uint8_t> opc_reader::read(std::string_view name) {
  const auto* selected = find(name);
  if (!selected) { throw error{error_code::missing_part, "OPC part is absent", std::string{name}}; }
  return archive_.read(std::string_view{selected->name}.substr(1));
}

// Let callers explicitly opt into complete archive integrity validation before publication.
void opc_reader::validate_all() { archive_.validate_all(); }

// Preserve one cumulative operation budget when a profile invokes additional typed codecs.
const std::shared_ptr<budget>& opc_reader::shared_budget() const noexcept {
  return archive_.shared_budget();
}

// Keep aggregate selection and extension handling on the archive's original budget.
profile_loader::profile_loader(opc_reader& reader, extension_policy policy)
    : reader_{reader}, policy_{std::move(policy)} {}

// Resolve one expected role, retaining precise source diagnostics across caller codec failures.
void profile_loader::read_related(std::string_view source, std::string_view relationship_type,
    std::string_view expected_content_type,
    const std::function<void(std::span<const std::uint8_t>, const part&, budget&)>& decode,
    std::size_t minimum, std::size_t maximum) {
  read_related(source, relationship_type, expected_content_type, decode, minimum, maximum, false);
}

// A declared dynamic MIME role still requires existing internal targets and validated metadata.
void profile_loader::read_related(std::string_view source, std::string_view relationship_type,
    std::string_view expected_content_type,
    const std::function<void(std::span<const std::uint8_t>, const part&, budget&)>& decode,
    std::size_t minimum, std::size_t maximum, bool allow_any_content_type) {
  if (finished_ || failed_ || minimum > maximum || !decode) {
    throw error{error_code::invalid_state, "Invalid or finished package profile"};
  }
  struct failure_guard {
    bool& failed;
    bool complete{};
    // Any unsuccessful selected-part operation permanently fences this candidate loader.
    ~failure_guard() { if (!complete) { failed = true; } }
  } guard{failed_};
  auto& shared = shared_budget();
  const auto links = reader_.find_relationships(source, relationship_type);
  if (links.size() < minimum || links.size() > maximum) {
    throw error{error_code::invalid_relationship, "Profile relationship cardinality mismatch",
                std::string{source}, 0, links.size(), maximum, {}, {},
                std::string{relationship_type}};
  }
  for (const auto* link : links) {
    shared.consume(resource::work_units, 1, source);
    if (link->mode != target_mode::internal) {
      throw error{error_code::invalid_relationship, "Typed profile requires an internal target",
                  link->source, 0, 0, 0, {}, link->target, link->id};
    }
    const auto name = resolve_target(link->source, link->target);
    const auto* selected = reader_.find(name);
    if (!selected) {
      throw error{error_code::missing_part, "Profile target is absent", name,
                  0, 0, 0, {}, link->target, link->id};
    }
    if (!allow_any_content_type && selected->content_type != expected_content_type) {
      throw error{error_code::invalid_metadata, "Profile target has an unexpected content type",
                  selected->name, 0, 0, 0, {}, {}, link->id};
    }
    if (decoded_.contains(selected->name)) {
      throw error{error_code::invalid_state, "Profile selects a typed part more than once",
                  selected->name, 0, 0, 0, {}, {}, link->id};
    }
    shared.consume(resource::metadata_bytes,
        sizeof(std::string) + selected->name.size() + sizeof(link), selected->name);
    decoded_.insert(selected->name);
    selected_.insert(link);
    const auto bytes = reader_.read(selected->name);
    try {
      decode(bytes, *selected, shared);
    } catch (const error& failure) {
      throw error{failure.code, failure.what(), failure.part.empty() ? selected->name : failure.part,
                  failure.offset_bytes, failure.required, failure.permitted, failure.field_path,
                  failure.reference, failure.relationship_id.empty() ? link->id : failure.relationship_id};
    } catch (const std::exception&) {
      throw error{error_code::codec, "Profile part decoding failed", selected->name,
                  0, 0, 0, {}, {}, link->id};
    }
    shared.check_cancelled();
  }
  guard.complete = true;
}

// Finish full aggregate validation without hiding corrupt or required unknown capabilities.
preserved_extensions profile_loader::finish() {
  if (finished_ || failed_) {
    throw error{error_code::invalid_state, "Package profile is finished or failed"};
  }
  struct failure_guard {
    bool& failed;
    bool complete{};
    // A failed integrity/preservation pass cannot be retried on the same candidate loader.
    ~failure_guard() { if (!complete) { failed = true; } }
  } guard{failed_};
  preserved_extensions extensions;
  auto& shared = shared_budget();
  for (const auto& item : reader_.parts()) {
    shared.consume(resource::work_units, 1, item.name);
    if (decoded_.contains(item.name)) { continue; }
    if ((policy_.is_required_part && policy_.is_required_part(item)) ||
        policy_.parts == unknown_policy::reject) {
      throw error{error_code::unsupported_feature, "Profile does not support this part", item.name};
    }
    // Charge retention before expanding, so a small preservation bound cannot stage a huge part.
    if (policy_.parts == unknown_policy::preserve) {
      shared.consume(resource::preserved_bytes, item.expanded_bytes, item.name);
      shared.consume(resource::metadata_bytes,
          sizeof(opaque_part) + item.name.size() + item.content_type.size(), item.name);
    }
    auto bytes = reader_.read(item.name);
    if (policy_.parts == unknown_policy::preserve) {
      extensions.parts.push_back({item, std::move(bytes), {}});
    }
  }
  for (const auto& link : reader_.relationships()) {
    shared.consume(resource::work_units, 1, link.source);
    if (selected_.contains(&link)) { continue; }
    if ((policy_.is_required_relationship && policy_.is_required_relationship(link)) ||
        policy_.relationships == unknown_policy::reject) {
      throw error{error_code::unsupported_feature, "Profile does not support this relationship",
                  link.source, 0, 0, 0, {}, link.target, link.id};
    }
    if (policy_.relationships == unknown_policy::preserve) {
      const auto bytes = sizeof(relationship) + link.source.size() + link.id.size() +
          link.type.size() + link.target.size();
      shared.consume(resource::preserved_bytes, bytes, link.source);
      shared.consume(resource::metadata_bytes, bytes, link.source);
      extensions.relationships.push_back(link);
    }
  }
  shared.check_cancelled();
  finished_ = true;
  guard.complete = true;
  return extensions;
}

// Return the single budget used by archive, metadata, typed callbacks and references.
budget& profile_loader::shared_budget() const noexcept { return *reader_.shared_budget(); }

// Retain opaque bytes and authored links without introducing a second XML representation.
void write_preserved(opc_writer& writer, const preserved_extensions& extensions) {
  for (const auto& item : extensions.parts) {
    writer.add_part(item.metadata.name, item.metadata.content_type, item.bytes, item.storage);
  }
  for (const auto& link : extensions.relationships) { writer.add_relationship(link); }
}

// Sort borrowed part pointers, bounding auxiliary storage independently of caller-owned payloads.
void write_canonical_zip(std::ostream& output, std::span<const opaque_part> parts,
                          std::shared_ptr<budget> shared) {
  if (!shared) { shared = std::make_shared<budget>(); }
  if (parts.size() > shared->available_buffer_bytes() / sizeof(const opaque_part*)) {
    throw error{error_code::resource_limit, "Canonical ZIP index exceeds scratch limit"};
  }
  shared->consume(resource::metadata_bytes, parts.size() * sizeof(const opaque_part*));
  buffer_reservation scratch{*shared, parts.size() * sizeof(const opaque_part*)};
  std::vector<const opaque_part*> ordered;
  ordered.reserve(parts.size());
  for (const auto& item : parts) { shared->check_cancelled(); ordered.push_back(&item); }
  std::sort(ordered.begin(), ordered.end(), [&](const auto* left, const auto* right) {
    shared->check_cancelled();
    return left->metadata.name < right->metadata.name;
  });
  zip_writer writer{output, shared};
  for (const auto* item : ordered) { writer.add(item->metadata.name, item->bytes, item->storage); }
  writer.finish();
}

// Canonical OPC uses fixed ZIP metadata, sorted logical names and source-local relationship IDs.
void write_canonical_opc(std::ostream& output, std::span<const opaque_part> parts,
                          std::span<const relationship> relationships,
                          std::shared_ptr<budget> shared) {
  if (!shared) { shared = std::make_shared<budget>(); }
  constexpr auto part_pointer_bytes = sizeof(const opaque_part*);
  constexpr auto link_pointer_bytes = sizeof(const relationship*);
  const auto maximum = shared->available_buffer_bytes();
  if (parts.size() > maximum / part_pointer_bytes ||
      relationships.size() > (maximum - parts.size() * part_pointer_bytes) / link_pointer_bytes) {
    throw error{error_code::resource_limit, "Canonical OPC index exceeds scratch limit"};
  }
  shared->consume(resource::metadata_bytes,
      parts.size() * part_pointer_bytes + relationships.size() * link_pointer_bytes);
  buffer_reservation scratch{*shared,
      parts.size() * part_pointer_bytes + relationships.size() * link_pointer_bytes};
  std::vector<const opaque_part*> ordered_parts;
  std::vector<const relationship*> ordered_links;
  ordered_parts.reserve(parts.size());
  ordered_links.reserve(relationships.size());
  for (const auto& item : parts) { shared->check_cancelled(); ordered_parts.push_back(&item); }
  for (const auto& link : relationships) { shared->check_cancelled(); ordered_links.push_back(&link); }
  std::sort(ordered_parts.begin(), ordered_parts.end(), [&](const auto* left, const auto* right) {
    shared->check_cancelled();
    return left->metadata.name < right->metadata.name;
  });
  std::sort(ordered_links.begin(), ordered_links.end(), [&](const auto* left, const auto* right) {
    shared->check_cancelled();
    return std::tie(left->source, left->id) < std::tie(right->source, right->id);
  });
  opc_writer writer{output, shared};
  for (const auto* item : ordered_parts) {
    writer.add_part(item->metadata.name, item->metadata.content_type, item->bytes, item->storage);
  }
  for (const auto* link : ordered_links) { writer.add_relationship(*link); }
  writer.finish();
}
} // namespace rohit::serializer::package
