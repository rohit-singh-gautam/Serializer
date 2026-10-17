// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <model.hpp>
#include <rohit/xml.hpp>

#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace xml = rohit::serializer::xml;

// Stop the SDK probe when a generated model or validation contract differs.
void require(bool success, std::string_view message) {
  if (!success) { throw std::runtime_error{std::string{message}}; }
}

// Require a malformed document or selected runtime validator to reject the operation.
template <typename Operation> void require_failure(Operation operation) {
  try { operation(); }
  catch (const std::invalid_argument&) { return; }
  throw std::runtime_error{"Expected XML failure"};
}
} // namespace

// Qualify nested, optional, numeric and fixed-array XML with the real generated C++ model.
int main() {
  constexpr std::string_view catalog_xml{
      "<catalog xmlns='urn:serializer:vocabulary'><prices><price currency='USD'>12.5</price>"
      "<price currency='EUR'>0</price></prices><labels><label>A&amp;B</label><label/></labels></catalog>"};
  const auto catalog = xml::deserialize<shapes::catalog>(catalog_xml);
  require(catalog.prices.size() == 2U && catalog.prices.front().value == 12.5 &&
      catalog.prices.front().currency == "USD" && catalog.labels == std::vector<std::string>{"A&B", ""},
      "XML simple content and wrappers differ");
  const auto catalog_copy = xml::deserialize<shapes::catalog>(xml::serialize(catalog));
  require(catalog_copy.prices.size() == 2U && catalog_copy.prices.front().value == 12.5 &&
      catalog_copy.labels == catalog.labels, "XML vocabulary round trip differs");
  const auto empty_catalog = xml::deserialize<shapes::catalog>(xml::serialize(shapes::catalog{}));
  require(empty_catalog.prices.empty() && empty_catalog.labels.empty(), "Empty wrappers lost");
  require_failure([] { (void)xml::deserialize<shapes::catalog>(
      "<catalog xmlns='urn:serializer:vocabulary'><labels/></catalog>"); });
  require_failure([] { (void)xml::deserialize<shapes::price>(
      "<price xmlns='urn:serializer:vocabulary' currency='USD'><value>1</value></price>"); });
  require_failure([] { (void)xml::deserialize<shapes::catalog>(
      "<catalog xmlns='urn:serializer:vocabulary'><prices extra='1'/><labels/></catalog>"); });
  shapes::record value{};
  value.big = std::numeric_limits<decltype(value.big)>::max();
  value.label = "\tX\r\n\xf0\x9f\x98\x80";
  value.flag = true;
  value.nested.value = std::numeric_limits<decltype(value.nested.value)>::min();
  shapes::child child{};
  child.value = std::numeric_limits<decltype(child.value)>::max();
  value.children.push_back(child);
  value.states = {shapes::state::zero, shapes::state::one};
  value.amount = std::numeric_limits<double>::infinity();
  value.small = 1.25F;
  const auto encoded = xml::serialize(value);
  const auto copy = xml::deserialize<shapes::record>(encoded);
  require(copy.big == value.big && copy.label == value.label && copy.flag == value.flag &&
      copy.nested.value == value.nested.value && copy.children.size() == 1U &&
      copy.children.front().value == child.value && copy.states == value.states &&
      copy.amount == value.amount && copy.small == value.small, "Typed XML round trip differs");
  value.flag = false;
  const auto false_copy = xml::deserialize<shapes::record>(xml::serialize(value));
  require(false_copy.flag.has_value() && !*false_copy.flag, "Present false differs from absence");
  value.flag.reset();
  require(!xml::deserialize<shapes::record>(xml::serialize(value)).flag, "Optional absence differs");

  auto zero = encoded;
  const auto start = zero.find("big=\"");
  require(start != std::string::npos, "Configured scalar attribute is absent");
  const auto number = start + std::string_view{"big=\""}.size();
  const auto end = zero.find('"', number);
  require(end != std::string::npos, "Configured scalar attribute is unterminated");
  zero.replace(number, end - number, "-000");
  require(xml::deserialize<shapes::record>(zero).big == 0U, "Unsigned negative zero differs");

  const xml::prepared_schema schema{
      "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'>"
      "<xs:element name='record' type='xs:string'/></xs:schema>"};
  require_failure([&] { (void)xml::deserialize<shapes::record>(encoded, {}, &schema); });
  require_failure([&] { (void)xml::serialize(value, {}, &schema); });
  std::cout << "C++ generated typed XML and prepared XSD checks passed\n";
}
