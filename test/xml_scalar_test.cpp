// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <xml_scalar_fixture.hpp>
#include <rohit/xml_tokens.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace xml = rohit::serializer::xml;

// Read an oracle input without changing its UTF-8 bytes or XML whitespace.
std::string read_file(const std::filesystem::path& path) {
  std::ifstream file{path, std::ios::binary};
  if (!file) { throw std::runtime_error{"Cannot open scalar oracle input"}; }
  return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

// Publish only complete valid output and require a second direct pass to be stable.
template <typename T> void publish(const std::filesystem::path& path, const T& value,
    const xml::limits& bounds) {
  const auto output = xml::serialize_tokens(value, bounds);
  const auto copy = xml::deserialize_tokens<T>(output, bounds, nullptr, 1U);
  if (xml::serialize_tokens(copy, bounds) != output) {
    throw std::runtime_error{"Scalar output changed during a second direct pass"};
  }
  auto destination = path; destination += ".cpp.xml";
  std::ofstream file{destination, std::ios::binary}; file << output;
  if (!file) { throw std::runtime_error{"Cannot publish scalar oracle output"}; }
}

// Reject typed output at each numeric, Unicode, enumeration and Boolean boundary.
void reject_mutations(const facets::sample& value, const xml::limits& bounds) {
  for (std::size_t index{}; index < 7U; ++index) {
    auto invalid = value;
    switch (index) {
    case 0U: invalid.integer = std::numeric_limits<std::uint64_t>::max(); break;
    case 1U: invalid.amount = -1.0; break;
    case 2U: invalid.rounded = 0.11F; break;
    case 3U: invalid.text = "B\xf0\x9f\x98\x80"; break;
    case 4U: invalid.choice = std::numeric_limits<std::uint64_t>::max() - 1U; break;
    case 5U: invalid.enabled = false; break;
    default: invalid.code = "\xc3\xa9"; break;
    }
    bool rejected{};
    try { (void)xml::serialize_tokens(invalid, bounds); }
    catch (const std::invalid_argument&) { rejected = true; }
    if (!rejected) { throw std::runtime_error{"Invalid typed scalar output was published"}; }
  }
}

// Match independent numeric admissions and require exact native width and normalized strings.
void check_numeric(const std::filesystem::path& path, bool valid, const xml::limits& bounds) {
  std::optional<facets::sample> value;
  try { value.emplace(xml::deserialize_tokens<facets::sample>(read_file(path), bounds, nullptr, 1U)); }
  catch (const std::invalid_argument&) { if (valid) { throw; } return; }
  if (!valid) { throw std::runtime_error{"Invalid numeric oracle input was accepted"}; }
  if (value->integer < std::numeric_limits<std::uint64_t>::max() - 2U ||
      value->integer == std::numeric_limits<std::uint64_t>::max() ||
      value->choice != std::numeric_limits<std::uint64_t>::max() || value->rounded != 0.1F ||
      value->text != "A\xf0\x9f\x98\x80" || value->code != "\xc3\xa9\xf0\x9f\x98\x80" || !value->enabled) {
    throw std::runtime_error{"Scalar normalization or native numeric width changed"};
  }
  publish(path, *value, bounds);
  reject_mutations(*value, bounds);
}

// Validate text pattern inputs and native output against the same independent Unicode corpus.
template <typename T> void check_pattern(const std::filesystem::path& path, bool valid,
    const xml::limits& bounds) {
  const auto scalar = read_file(std::filesystem::path{path.string() + ".txt"});
  std::optional<T> decoded;
  try { decoded.emplace(xml::deserialize_tokens<T>(read_file(path), bounds, nullptr, 1U)); }
  catch (const std::invalid_argument&) { if (valid) { throw; } }
  catch (const std::length_error&) { if (valid) { throw; } }
  if (decoded.has_value() != valid || (decoded && decoded->value != scalar)) {
    throw std::runtime_error{"Pattern input differs from the independent oracle"};
  }
  T value{}; value.value = scalar;
  bool written{};
  try { publish(path, value, bounds); written = true; }
  catch (const std::invalid_argument&) { if (valid) { throw; } }
  catch (const std::length_error&) { if (valid) { throw; } }
  if (written != valid) { throw std::runtime_error{"Pattern output differs from the independent oracle"}; }
}

// Compare namespace-bearing scalars and lists without converting their lexical prefixes into identities.
template <typename T> void check_qname(const std::filesystem::path& path, bool valid,
    const xml::limits& bounds) {
  std::optional<T> value;
  try { value.emplace(xml::deserialize_tokens<T>(read_file(path), bounds, nullptr, 1U)); }
  catch (const std::invalid_argument&) { if (valid) { throw; } return; }
  catch (const std::length_error&) { if (valid) { throw; } return; }
  if (!valid) { throw std::runtime_error{"Invalid QName oracle input was accepted"}; }
  publish(path, *value, bounds);
}

// Empty collapsed values have a whitespace spelling, while other defaulted strings cannot encode empty.
void check_empty_defaults(const xml::limits& bounds) {
  defaults::collapsed collapsed{};
  const auto bytes = xml::serialize_tokens(collapsed, bounds);
  if (!xml::deserialize_tokens<defaults::collapsed>(bytes, bounds).value.empty()) {
    throw std::runtime_error{"Empty collapsed text reactivated its element default"};
  }
  for (const auto text : {"<collapsed/>", "<preserved/>", "<replaced/>"}) {
    const auto value = std::string_view{text}.starts_with("<collapsed") ?
        xml::deserialize_tokens<defaults::collapsed>(text, bounds).value :
        std::string_view{text}.starts_with("<preserved") ? xml::deserialize_tokens<defaults::preserved>(text, bounds).value :
        xml::deserialize_tokens<defaults::replaced>(text, bounds).value;
    if (value != "fallback") { throw std::runtime_error{"Root simpleContent default was omitted"}; }
  }
  const auto reject = [&](const auto& value) {
    bool rejected{};
    try { (void)xml::serialize_tokens(value, bounds); }
    catch (const std::invalid_argument&) { rejected = true; }
    if (!rejected) { throw std::runtime_error{"Unrepresentable empty defaulted text was published"}; }
  };
  reject(defaults::preserved{}); reject(defaults::replaced{});
  scalars::items many{}; many.value.assign(32U, "a");
  auto limited = bounds; limited.max_nodes = 16U;
  bool rejected{};
  try { (void)xml::serialize_tokens(many, limited); }
  catch (const std::length_error&) { rejected = true; }
  if (!rejected) { throw std::runtime_error{"Native list item bound was bypassed"}; }
}

// Compare both generated XML paths with the independent XSD provider and stable direct output.
template <typename T> void accept_qname_union(std::string_view document,
    const xml::prepared_schema* oracle, const xml::limits& bounds) {
  if (oracle) { oracle->validate(document); }
  const auto value = xml::deserialize_tokens<T>(document, bounds, nullptr, 1U);
  const auto output = xml::serialize_tokens(value, bounds);
  if (oracle) { oracle->validate(output); }
  const auto copy = xml::deserialize_tokens<T>(output, bounds, nullptr, 1U);
  if (xml::serialize_tokens(copy, bounds) != output) {
    throw std::runtime_error{"QName union selected member changed during direct round trip"};
  }
  const auto tree_value = xml::deserialize<T>(document);
  const auto tree_output = xml::serialize(tree_value);
  if (oracle) { oracle->validate(tree_output); }
  if (xml::serialize_tokens(tree_value, bounds) != output) {
    throw std::runtime_error{"QName union tree and direct namespace contracts differ"};
  }
}

// Reject scope loss and malformed union values in both generated entry points.
template <typename T> void reject_qname_union(std::string_view document,
    const xml::limits& bounds) {
  bool direct_rejected{}, tree_rejected{};
  try { (void)xml::deserialize_tokens<T>(document, bounds, nullptr, 1U); }
  catch (const std::invalid_argument&) { direct_rejected = true; }
  try { (void)xml::deserialize<T>(document); }
  catch (const std::invalid_argument&) { tree_rejected = true; }
  if (!direct_rejected || !tree_rejected) {
    throw std::runtime_error{"QName union child lost its original namespace identity"};
  }
}

// Namespace-sensitive child unions retain their first successful branch and expanded identities.
void check_qname_unions(const xml::limits& bounds) {
  static_assert(qname_union::sample::xml_tokens_supported && qname_union::lists::xml_tokens_supported &&
      qname_union::nested::xml_tokens_supported && qname_union::fallback::xml_tokens_supported);
  std::optional<xml::prepared_schema> oracle;
  if (xml::available()) {
    oracle.emplace(read_file(std::filesystem::path{__FILE__}.parent_path() /
        "resources/xml_scalar/qname_union.xsd"));
  }
  const auto* reference = oracle ? &*oracle : nullptr;
  for (const auto text : {
      "<sample xmlns:p='urn:types'><value>p:item</value><optional>12</optional><entry>p:one</entry><entry xmlns:p='urn:types'>p:two</entry></sample>",
      "<sample><value>65535</value></sample>",
      "<sample xmlns=''><value>item</value></sample>",
      "<sample xmlns:p='urn:types' xmlns:alias='urn:types'><value>alias:item</value><entry>p:other</entry></sample>"}) {
    accept_qname_union<qname_union::sample>(text, reference, bounds);
  }
  for (const auto text : {
      "<lists xmlns:p='urn:types'><value>p:one p:two</value></lists>",
      "<lists xmlns:p='urn:types'><value xmlns:p='urn:types'>p:one\tp:two</value></lists>",
      "<lists><value>42</value></lists>"}) {
    accept_qname_union<qname_union::lists>(text, reference, bounds);
  }
  for (const auto text : {
      "<nested xmlns:p='urn:types'><value>p:item</value></nested>",
      "<nested><value>true</value></nested>", "<nested><value>42</value></nested>"}) {
    accept_qname_union<qname_union::nested>(text, reference, bounds);
  }
  for (const auto text : {
      "<fallback xmlns:p='urn:allowed'><value>p:item</value></fallback>",
      "<fallback><value>free form text</value></fallback>",
      "<fallback xmlns:p='urn:other'><value>p:item</value></fallback>"}) {
    accept_qname_union<qname_union::fallback>(text, reference, bounds);
  }
  // libxml2 leaks an unbound QName failure past the successful string fallback; check generated paths.
  accept_qname_union<qname_union::fallback>(
      "<fallback><value>unbound:item</value></fallback>", nullptr, bounds);
  // An earlier string member gives the text no QName identity, even with a child-local declaration.
  accept_qname_union<qname_union::string_first>(
      "<string_first><value xmlns:p='urn:child'>p:item</value></string_first>", reference, bounds);
  accept_qname_union<qname_union::whitespace>(
      "<whitespace xmlns:p='urn:parent'><value> p:item </value></whitespace>", reference, bounds);
  accept_qname_union<qname_union::whitespace>(
      "<whitespace xmlns:p='urn:parent'><value xmlns:p='urn:child'>p:item</value></whitespace>", reference, bounds);
  const auto whitespace = xml::deserialize_tokens<qname_union::whitespace>(
      "<whitespace xmlns:p='urn:parent'><value> p:item </value></whitespace>", bounds);
  if (whitespace.value != " p:item ") {
    throw std::runtime_error{"QName union lexical whitespace changed its selected member"};
  }
  for (const auto text : {
      "<sample><value>unbound:item</value></sample>", "<sample><value>65536</value></sample>",
      "<sample xmlns:p='urn:parent'><value xmlns:p='urn:child'>p:item</value></sample>",
      "<sample><value xmlns:p='urn:child'>p:item</value></sample>",
      "<sample xmlns:p='urn:parent'><value>1</value><optional xmlns:p='urn:child'>p:item</optional></sample>",
      "<sample xmlns:p='urn:parent'><value>1</value><entry xmlns:p='urn:child'>p:item</entry></sample>"}) {
    reject_qname_union<qname_union::sample>(text, bounds);
  }
  reject_qname_union<qname_union::lists>(
      "<lists xmlns:p='urn:parent'><value xmlns:p='urn:child'>p:one p:two</value></lists>", bounds);
  reject_qname_union<qname_union::nested>(
      "<nested xmlns:p='urn:parent'><value xmlns:p='urn:child'>p:item</value></nested>", bounds);
  reject_qname_union<qname_union::whitespace>(
      "<whitespace xmlns:p='urn:parent'><value xmlns:p='urn:child'> p:item </value></whitespace>", bounds);
  // Moving a string fallback into an owner scope must never reactivate an earlier QName branch.
  reject_qname_union<qname_union::fallback>(
      "<fallback xmlns:p='urn:allowed'><value xmlns:p='urn:other'>p:item</value></fallback>", bounds);
  reject_qname_union<qname_union::fallback>(
      "<fallback xmlns:p='urn:other'><value xmlns:p='urn:allowed'>p:item</value></fallback>", bounds);
  auto invalid = xml::deserialize_tokens<qname_union::sample>(
      "<sample xmlns:p='urn:types'><value>p:item</value></sample>", bounds);
  invalid.value = "unbound:item";
  bool rejected{};
  try { (void)xml::serialize_tokens(invalid, bounds); }
  catch (const std::invalid_argument&) { rejected = true; }
  if (!rejected) { throw std::runtime_error{"Invalid native QName union output was published"}; }
}
} // namespace

// Exercise the real generated SDK with zero tree budget and provider-split UTF-8 input.
int main(int argc, char** argv) {
  try {
    static_assert(facets::sample::xml_tokens_supported && pattern::sample::xml_tokens_supported &&
        large::sample::xml_tokens_supported);
    xml::limits bounds{}; bounds.max_tree_bytes = 0U;
    check_empty_defaults(bounds);
    check_qname_unions(bounds);
    for (int index{1}; index < argc; ++index) {
      const std::filesystem::path path{argv[index]};
      const auto name = path.filename().string();
      const bool valid = name.ends_with(".valid.xml");
      auto selected_bounds = bounds;
      if (name.starts_with("scalar-items-limit")) { selected_bounds.max_nodes = 16U; }
      if (name.starts_with("scalar-items-text")) { selected_bounds.max_text_bytes = 18U; }
      try {
        if (name.starts_with("scalar-sample")) { check_qname<scalars::sample>(path, valid, bounds); }
        else if (name.starts_with("scalar-amount")) { check_qname<scalars::amount>(path, valid, bounds); }
        else if (name.starts_with("scalar-words")) { check_qname<scalars::words>(path, valid, bounds); }
        else if (name.starts_with("scalar-numbers")) { check_qname<scalars::numbers>(path, valid, bounds); }
        else if (name.starts_with("scalar-items")) { check_qname<scalars::items>(path, valid, selected_bounds); }
        else if (name.starts_with("scalar-fixed_list")) { check_qname<scalars::fixed_list>(path, valid, bounds); }
        else if (name.starts_with("scalar-child_list")) { check_qname<scalars::child_list>(path, valid, bounds); }
        else if (name.starts_with("scalar-optional_child")) { check_qname<scalars::optional_child>(path, valid, bounds); }
        else if (name.starts_with("scalar-list_all")) { check_qname<scalars::list_all>(path, valid, bounds); }
        else if (name.starts_with("scalar-plain_default")) { check_qname<scalars::plain_default>(path, valid, bounds); }
        else if (name.starts_with("scalar-guard")) { check_qname<scalars::guard>(path, valid, bounds); }
        else if (name.starts_with("default-collapsed")) { check_qname<defaults::collapsed>(path, valid, bounds); }
        else if (name.starts_with("default-preserved")) { check_qname<defaults::preserved>(path, valid, bounds); }
        else if (name.starts_with("default-replaced")) { check_qname<defaults::replaced>(path, valid, bounds); }
        else if (name.starts_with("default-fixed")) { check_qname<defaults::required_fixed>(path, valid, bounds); }
        else if (name.starts_with("qname-child")) { check_qname<qnames::qname_children>(path, valid, bounds); }
        else if (name.starts_with("qname")) { check_qname<qnames::qualified_names>(path, valid, bounds); }
        else if (name.starts_with("numeric")) { check_numeric(path, valid, bounds); }
        else if (name.starts_with("large") || name.starts_with("work") || name.starts_with("bytes")) {
          check_pattern<large::sample>(path, valid, bounds);
        } else { check_pattern<pattern::sample>(path, valid, bounds); }
      } catch (const std::exception& error) { throw std::runtime_error{name + ": " + error.what()}; }
    }
    std::cout << "C++ exact numeric, Unicode and prepared pattern scalar probes pass\n";
    return 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
