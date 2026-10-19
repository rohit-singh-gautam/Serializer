// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <xml_presence_fixture.hpp>
#include <rohit/xml_tokens.hpp>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace xml = rohit::serializer::xml;
namespace codec = rohit::serializer;

// Retain complete native wire bytes as an independent control for XML-only mapping changes.
template <typename T> std::vector<std::uint8_t> native_bytes(const T& value) {
  rohit::full_stream_auto_alloc storage{};
  codec::binary_none<codec::serialize_type::out> protocol{storage};
  protocol.serialize_out(value);
  return {storage.begin(), storage.begin() + storage.current_offset()};
}

// Keep rejected documents unpublished and require both XML directions to preserve native payloads.
template <typename T> void check_document(const std::filesystem::path& path, const std::string& bytes, bool valid) {
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  std::optional<T> value;
  try { value.emplace(xml::deserialize_tokens<T>(bytes, bounds, nullptr, 1U)); }
  catch (const std::exception&) { if (valid) { throw; } return; }
  if (!valid) { throw std::runtime_error{"Invalid presence document was accepted"}; }
  const auto output = xml::serialize(*value, bounds);
  const auto copy = xml::deserialize_tokens<T>(output, bounds, nullptr, 1U);
  if (native_bytes(*value) != native_bytes(copy)) { throw std::runtime_error{"XML changed native presence payload bytes"}; }
  if (xml::serialize_tokens(copy, bounds) != output) { throw std::runtime_error{"XML presence output is unstable"}; }
  auto destination = path; destination += ".cpp.xml";
  std::ofstream file{destination, std::ios::binary}; file << output;
  if (!file) { throw std::runtime_error{"Cannot publish XML oracle output"}; }
}

// Distinguish every optional layer before running the independent document corpus.
void check_states() {
  const std::string prefix{"<sample xmlns='urn:serializer:nil' xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'>"};
  const std::string ending{"<nested xsi:nil='true'/></sample>"};
  const auto absent = xml::deserialize_tokens<nil_fixture::sample>(prefix + "<nil_value xsi:nil='true'/>" + ending);
  const auto nil = xml::deserialize_tokens<nil_fixture::sample>(prefix + "<nil_value/><states xsi:nil='true'/><entries xsi:nil='true'/>" + ending);
  const auto empty = xml::deserialize_tokens<nil_fixture::sample>(prefix + "<nil_value/><states/><entries/>" + ending);
  if (absent.states || absent.entries || absent.nil_value || !nil.states || *nil.states || !nil.entries || *nil.entries ||
      !empty.states || !*empty.states || !empty.states->value().empty() || !empty.entries || !*empty.entries || !empty.entries->value().empty()) {
    throw std::runtime_error{"Absent, nil and present-empty native states were merged"};
  }
}
} // namespace

// Execute real generated owning codecs against oracle-labelled document files.
int main(int argc, char** argv) {
  try {
    check_states();
    for (int index{1}; index < argc; ++index) {
      const std::filesystem::path path{argv[index]};
      const auto name = path.filename().string();
      std::ifstream file{path, std::ios::binary};
      if (!file) { throw std::runtime_error{"Cannot open XML presence fixture"}; }
      const std::string bytes{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
      const bool valid = name.ends_with(".valid.xml");
      if (name.starts_with("holder")) { check_document<nil_fixture::holder>(path, bytes, valid); }
      else if (name.starts_with("rich")) { check_document<nil_shapes::sample>(path, bytes, valid); }
      else if (name.starts_with("all")) { check_document<nil_all::sample>(path, bytes, valid); }
      else { check_document<nil_fixture::sample>(path, bytes, valid); }
    }
    std::cout << "C++ direct native presence states and exact payload controls pass\n";
    return 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
