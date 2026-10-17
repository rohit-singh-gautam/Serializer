// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/package.hpp>

#include <rohit/compression.hpp>
#include <rohit/xml.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace package = rohit::serializer::package;
namespace compression = rohit::serializer::compression;
using bytes = std::vector<std::uint8_t>;

// Borrow string bytes only for the duration of a complete package call.
std::span<const std::uint8_t> as_bytes(std::string_view value) {
  return {reinterpret_cast<const std::uint8_t*>(value.data()), value.size()};
}

// Inspect decoded payloads without C-string truncation or encoding conversion.
std::string as_text(const bytes& value) { return {value.begin(), value.end()}; }

// Build the smallest useful archive, returning only a finalized complete directory.
std::string make_archive(package::entry_options options = {}) {
  std::ostringstream output;
  package::zip_writer writer{output};
  writer.add("data/model.json", as_bytes("{\"count\":7}"), options);
  writer.add("media/picture.bin", as_bytes(std::string_view{"a\0b", 3}));
  writer.finish();
  writer.finish();
  return output.str();
}

// Identify the central directory in small fixed test archives.
std::size_t central_offset(const std::string& archive) {
  return archive.find(std::string{"PK\x01\x02", 4});
}

// Replace an intentionally corrupted little-endian field in a fixture.
void overwrite(bytes& data, std::size_t offset, std::uint32_t value, std::size_t count) {
  for (std::size_t index = 0; index < count; ++index) {
    data[offset + index] = static_cast<std::uint8_t>(value >> (index * 8));
  }
}

// Construct a byte string preserving embedded ZIP header zero bytes.
std::string to_string(const bytes& data) { return {data.begin(), data.end()}; }

// Load an independently produced Python zipfile fixture from the source tree.
std::ifstream independent_fixture(std::string_view name) {
  return std::ifstream{std::filesystem::path{__FILE__}.parent_path() / "resources" /
      std::string{name}, std::ios::binary};
}

// Verify stable package categories instead of backend-specific exception messages.
template <typename Callback>
void expect_error(package::error_code category, Callback callback) {
  try {
    callback();
    FAIL() << "Expected package failure";
  } catch (const package::error& failure) {
    EXPECT_EQ(failure.code, category) << failure.what();
  }
}

// Own only a test-created directory beneath the system temporary root.
struct temporary_directory {
  std::filesystem::path path;
  // Create a private test directory without touching preexisting user paths.
  temporary_directory()
      : path{std::filesystem::temp_directory_path() /
          ("serializer-package-test-" + std::to_string(
              std::chrono::steady_clock::now().time_since_epoch().count()))} {
    if (!std::filesystem::create_directory(path)) {
      throw std::runtime_error{"Cannot create package test directory"};
    }
  }
  // Remove known flat test outputs only; no recursive or computed-root deletion is used.
  ~temporary_directory() {
    std::error_code ignored;
    for (const auto& entry : std::filesystem::directory_iterator{path}) {
      if (entry.is_regular_file()) { std::filesystem::remove(entry.path(), ignored); }
    }
    std::filesystem::remove(path, ignored);
  }
};
} // namespace

// Plain ZIP carries arbitrary codec bytes without requiring XML or OPC metadata.
TEST(package_zip, stored_parts_complete_directory_and_exact_names) {
  std::istringstream input{make_archive()};
  package::zip_reader reader{input};
  ASSERT_EQ(reader.entries().size(), 2U);
  EXPECT_EQ(as_text(reader.read("data/model.json")), "{\"count\":7}");
  EXPECT_EQ(as_text(reader.read("media/picture.bin")), std::string("a\0b", 3));
  EXPECT_EQ(reader.find("DATA/model.json"), nullptr);
  EXPECT_EQ(reader.find("[Content_Types].xml"), nullptr);
  reader.validate_all();
}

// A standard whole-object stream callback composes with the same bounded ZIP path.
TEST(package_zip, complete_stream_producer_and_failed_callback) {
  std::ostringstream output;
  package::zip_writer writer{output};
  EXPECT_THROW(writer.write("bad", [](std::ostream& stream) {
    stream << "partial";
    throw std::runtime_error{"injected producer failure"};
  }), std::runtime_error);
  EXPECT_TRUE(output.str().empty());
  writer.write("model.json", [](std::ostream& stream) { stream << "{\"count\":7}"; });
  writer.finish();
  EXPECT_THROW(writer.add("late", {}), package::error);
  std::istringstream input{output.str()};
  package::zip_reader reader{input};
  EXPECT_EQ(as_text(reader.read("model.json")), "{\"count\":7}");
}

// Unsupported archive features are errors even when their payload bytes resemble stored data.
TEST(package_zip, rejects_unsupported_flags_methods_zip64_and_conflicting_headers) {
  const auto archive = make_archive();
  const auto central = central_offset(archive);
  ASSERT_NE(central, std::string::npos);
  for (const auto mutation : std::array{
           std::pair<std::size_t, std::uint32_t>{central + 8, 1},
           std::pair<std::size_t, std::uint32_t>{central + 10, 99},
           std::pair<std::size_t, std::uint32_t>{central + 34, 1}}) {
    bytes changed{archive.begin(), archive.end()};
    overwrite(changed, mutation.first, mutation.second, 2);
    std::istringstream input{to_string(changed)};
    expect_error(package::error_code::unsupported_feature, [&] { package::zip_reader reader{input}; });
  }
  bytes zip64{archive.begin(), archive.end()};
  overwrite(zip64, central + 24, 0xffffffff, 4);
  std::istringstream zip64_input{to_string(zip64)};
  expect_error(package::error_code::unsupported_feature,
      [&] { package::zip_reader reader{zip64_input}; });
  bytes mismatched{archive.begin(), archive.end()};
  mismatched[30] = 'x';
  std::istringstream mismatched_input{to_string(mismatched)};
  expect_error(package::error_code::invalid_archive,
      [&] { package::zip_reader reader{mismatched_input}; });
}

// CRC checks occur on access; a corrupt unrelated attachment does not force decompression.
TEST(package_zip, selective_integrity_and_truncation) {
  auto archive = make_archive();
  const auto payload = archive.find(std::string{"a\0b", 3});
  ASSERT_NE(payload, std::string::npos);
  archive[payload] = 'x';
  std::istringstream input{archive};
  package::zip_reader reader{input};
  EXPECT_EQ(as_text(reader.read("data/model.json")), "{\"count\":7}");
  expect_error(package::error_code::integrity, [&] { static_cast<void>(reader.read("media/picture.bin")); });
  for (const auto removed : {1U, 10U, 30U}) {
    std::istringstream truncated{archive.substr(0, archive.size() - removed)};
    expect_error(package::error_code::invalid_archive, [&] { package::zip_reader invalid{truncated}; });
  }
}

// Names are logical identities and must never authorize paths outside an application staging area.
TEST(package_zip, rejects_duplicate_names_and_unsafe_segments) {
  std::ostringstream output;
  package::zip_writer writer{output};
  writer.add("valid", {});
  expect_error(package::error_code::duplicate_name, [&] { writer.add("valid", {}); });
  for (const auto name : {"", "/absolute", "../outside", "a/../outside", "a//b", "C:drive", "a\\b"}) {
    expect_error(package::error_code::invalid_name, [&] { writer.add(name, {}); });
  }
  writer.finish();
}

// A nested reader cannot reset aggregate limits, including repeated reads and cancellation.
TEST(package_zip, shared_limits_cancel_and_complete_part_staging) {
  const auto archive = make_archive();
  auto shared = std::make_shared<package::budget>(package::limits{.max_total_expanded_bytes = 12});
  std::istringstream input{archive};
  package::zip_reader reader{input, shared};
  EXPECT_EQ(reader.read("data/model.json").size(), 11U);
  expect_error(package::error_code::resource_limit, [&] { static_cast<void>(reader.read("media/picture.bin")); });
  std::istringstream metadata_input{archive};
  expect_error(package::error_code::resource_limit, [&] {
    package::zip_reader rejected{metadata_input,
      std::make_shared<package::budget>(package::limits{.max_metadata_bytes = 10})};
  });
  std::ostringstream bounded;
  package::zip_writer bounded_writer{bounded,
      std::make_shared<package::budget>(package::limits{.max_part_bytes = 3})};
  expect_error(package::error_code::resource_limit, [&] {
    bounded_writer.write("oversized", [](std::ostream& output) { output << "1234"; });
  });
  EXPECT_TRUE(bounded.str().empty());
  std::istringstream cancelled_input{archive};
  expect_error(package::error_code::cancelled, [&] {
    package::zip_reader cancelled{cancelled_input,
        std::make_shared<package::budget>(package::limits{.is_cancelled = [] { return true; }})};
  });
}

// Python zipfile fixtures independently cover ordinary and streamed data-descriptor framing.
TEST(package_zip, independent_stored_deflated_and_descriptor_fixtures) {
  for (const auto fixture : {"package_python.zip", "package_python_descriptor.zip"}) {
    auto input = independent_fixture(fixture);
    ASSERT_TRUE(input.is_open()) << fixture;
    package::zip_reader reader{input};
    ASSERT_EQ(reader.entries().size(), 2U);
    EXPECT_EQ(as_text(reader.read("media/picture.bin")), std::string("a\0b", 3));
    if (compression::available(compression::format::deflate)) {
      EXPECT_EQ(as_text(reader.read("data/model.json")), "{\"count\":7}");
      reader.validate_all();
    } else {
      expect_error(package::error_code::unavailable, [&] {
        static_cast<void>(reader.read("data/model.json"));
      });
    }
  }
  if (!compression::available(compression::format::deflate)) { return; }
  std::istringstream own{make_archive({.method = package::zip_method::deflate})};
  package::zip_reader reader{own};
  EXPECT_EQ(as_text(reader.read("data/model.json")), "{\"count\":7}");
}

// Availability is explicit at the package boundary rather than a silent stored fallback.
TEST(package_zip, deflate_unavailable_or_invalid_level_is_structured) {
  std::ostringstream output;
  package::zip_writer writer{output};
  const auto expected = compression::available(compression::format::deflate)
      ? package::error_code::invalid_state : package::error_code::unavailable;
  expect_error(expected, [&] {
    writer.add("data", as_bytes("payload"), {.method = package::zip_method::deflate,
                                            .compression_level = 10000});
  });
  EXPECT_TRUE(output.str().empty());
}

// Full Save preserves the old file when preparation fails and publishes only complete archives.
TEST(package_zip, full_save_is_transactional_before_publication_and_cleans_staging) {
  temporary_directory directory;
  const auto destination = directory.path / "document.zip";
  { std::ofstream original{destination}; original << "old contents"; }
  EXPECT_THROW(package::save_zip_file(destination, [](package::zip_writer& writer) {
    writer.add("new", as_bytes("candidate"));
    throw std::runtime_error{"late failure"};
  }), std::runtime_error);
  { std::ifstream old{destination}; std::string contents; std::getline(old, contents);
    EXPECT_EQ(contents, "old contents"); }
  package::save_zip_file(destination, [](package::zip_writer& writer) {
    writer.add("new", as_bytes("candidate"));
  });
  std::ifstream saved{destination, std::ios::binary};
  package::zip_reader reader{saved};
  EXPECT_EQ(as_text(reader.read("new")), "candidate");
  EXPECT_EQ(std::distance(std::filesystem::directory_iterator{directory.path},
                         std::filesystem::directory_iterator{}), 1);
}

// URI resolution allows legitimate parent segments but never repeated decoding or root escapes.
TEST(package_opc, scoped_uri_resolution) {
  EXPECT_EQ(package::resolve_target("/data/document.xml", "../media/image.png"), "/media/image.png");
  EXPECT_EQ(package::resolve_target("/data/document.xml", "./other.xml"), "/data/other.xml");
  EXPECT_EQ(package::resolve_target("/", "/media/a%20b.png"), "/media/a%20b.png");
  for (const auto target : {"../../outside", "%2e%2e/outside", "a%2Fb", "a%5Cb", "//host/a", "a#id", "a?x", "http://host"}) {
    EXPECT_THROW(static_cast<void>(package::resolve_target("/data/document.xml", target)), package::error);
  }
}

// Standard OPC infrastructure is generated, and relationships discover renamed payloads.
TEST(package_opc, builder_metadata_discovery_relative_targets_and_external_data) {
  if (!rohit::serializer::xml::available()) { GTEST_SKIP(); }
  std::ostringstream output;
  package::opc_writer writer{output};
  writer.content_type_default("json", "application/octet-stream");
  writer.add_part("/renamed/main.json", "application/json", as_bytes("{\"count\":7}"));
  writer.write_part("/media/picture.bin", "application/octet-stream",
      [](std::ostream& stream) { stream.write("a\0b", 3); });
  writer.add_relationship({"/", "rId1", "urn:example:main", "renamed/main.json"});
  writer.add_relationship({"/renamed/main.json", "rId1", "urn:example:image", "../media/picture.bin"});
  writer.add_relationship({"/renamed/main.json", "rId2", "urn:example:external",
                           "https://invalid.example/no-fetch", package::target_mode::external});
  writer.finish();
  std::istringstream input{output.str()};
  package::opc_reader reader{input};
  ASSERT_EQ(reader.parts().size(), 2U);
  ASSERT_EQ(reader.relationships().size(), 3U);
  ASSERT_EQ(reader.find_relationships("/", "urn:example:main").size(), 1U);
  EXPECT_EQ(reader.find("/RENAMED/MAIN.JSON")->content_type, "application/json");
  EXPECT_EQ(as_text(reader.read("/renamed/main.json")), "{\"count\":7}");
  EXPECT_EQ(as_text(reader.read("/media/picture.bin")), std::string("a\0b", 3));
  reader.validate_all();
  std::istringstream plain{output.str()};
  package::zip_reader archive{plain};
  EXPECT_NE(archive.find("[Content_Types].xml"), nullptr);
  EXPECT_NE(archive.find("_rels/.rels"), nullptr);
  EXPECT_NE(archive.find("renamed/_rels/main.json.rels"), nullptr);
}

// Forward links work, source-local duplicate IDs fail, and missing targets prevent finalization.
TEST(package_opc, duplicate_identity_and_invalid_references_fail) {
  if (!rohit::serializer::xml::available()) { GTEST_SKIP(); }
  std::ostringstream output;
  package::opc_writer writer{output};
  writer.add_part("/data/model.json", "application/json", as_bytes("{}"));
  expect_error(package::error_code::duplicate_name, [&] {
    writer.add_part("/DATA/MODEL.JSON", "application/json", {});
  });
  writer.add_relationship({"/", "rId1", "urn:example:main", "data/model.json"});
  expect_error(package::error_code::invalid_relationship, [&] {
    writer.add_relationship({"/", "rId1", "urn:example:other", "data/model.json"});
  });
  writer.add_relationship({"/data/model.json", "rId1", "urn:example:missing", "absent.bin"});
  expect_error(package::error_code::missing_part, [&] { writer.finish(); });
  expect_error(package::error_code::invalid_state, [&] { writer.finish(); });
}

// OPC identities fold ASCII and percent-hex case; a part cannot also act as a directory.
TEST(package_opc, rejects_equivalent_and_derived_part_names_in_both_orders) {
  if (!rohit::serializer::xml::available()) { GTEST_SKIP(); }
  struct name_case {
    std::string_view first;
    std::string_view second;
    package::error_code category;
  };
  constexpr name_case cases[]{
      {"/data.bin", "/DATA.BIN", package::error_code::duplicate_name},
      {"/data%3A.bin", "/DATA%3a.BIN", package::error_code::duplicate_name},
      {"/data.bin", "/DATA.BIN/child.bin", package::error_code::invalid_name},
      {"/data%3A.bin", "/DATA%3a.BIN/child.bin", package::error_code::invalid_name}};
  constexpr std::string_view manifest =
      "<Types xmlns='http://schemas.openxmlformats.org/package/2006/content-types'>"
      "<Default Extension='bin' ContentType='application/octet-stream'/></Types>";
  for (const auto& value : cases) {
    for (const bool reverse : {false, true}) {
      const auto first = reverse ? value.second : value.first;
      const auto second = reverse ? value.first : value.second;
      std::ostringstream output;
      package::opc_writer writer{output};
      writer.add_part("/data.bin-other.bin", "application/octet-stream", {});
      writer.add_part(first, "application/octet-stream", {});
      expect_error(value.category, [&] { writer.add_part(second, "application/octet-stream", {}); });
      bool invoked = false;
      expect_error(value.category, [&] {
        writer.write_part(second, "application/octet-stream", [&](std::ostream&) { invoked = true; });
      });
      EXPECT_FALSE(invoked);

      std::ostringstream encoded;
      package::zip_writer archive{encoded};
      archive.add("[Content_Types].xml", as_bytes(manifest));
      archive.add("data.bin-other.bin", {});
      archive.add(first.substr(1), {});
      archive.add(second.substr(1), {});
      archive.finish();
      std::istringstream plain{encoded.str()};
      EXPECT_EQ(package::zip_reader{plain}.entries().size(), 4U);
      std::istringstream input{encoded.str()};
      expect_error(value.category, [&] { package::opc_reader reader{input}; });
    }
  }
  std::ostringstream output;
  package::opc_writer writer{output};
  for (const auto name : {"/dat%61.bin", "/%C3%A9.bin"}) {
    expect_error(package::error_code::invalid_name, [&] {
      writer.add_part(name, "application/octet-stream", {});
    });
    std::ostringstream encoded;
    package::zip_writer archive{encoded};
    archive.add("[Content_Types].xml", as_bytes(manifest));
    archive.add(std::string_view{name}.substr(1), {});
    archive.finish();
    std::istringstream input{encoded.str()};
    expect_error(package::error_code::invalid_name, [&] { package::opc_reader reader{input}; });
  }
}

// Intervening lexical neighbors cannot hide an ancestor; generated .rels parts obey the same rule.
TEST(package_opc, rejects_nonadjacent_ancestors_and_infrastructure_derivation) {
  if (!rohit::serializer::xml::available()) { GTEST_SKIP(); }
  std::ostringstream output;
  package::opc_writer writer{output};
  writer.add_part("/data.bin", "application/octet-stream", {});
  writer.add_part("/data.bin-other.bin", "application/octet-stream", {});
  expect_error(package::error_code::invalid_name, [&] {
    writer.add_part("/DATA.BIN/child.bin", "application/octet-stream", {});
  });
  std::ostringstream infrastructure;
  package::opc_writer with_relationships{infrastructure};
  with_relationships.add_part("/_RELS", "application/octet-stream", {});
  with_relationships.add_relationship({"/", "r1", "urn:example:main", "_RELS"});
  expect_error(package::error_code::invalid_name, [&] { with_relationships.finish(); });
  expect_error(package::error_code::invalid_state, [&] { with_relationships.finish(); });
}

// Malformed namespace/DTD declarations and uncovered content never become typed package parts.
TEST(package_opc, rejects_hostile_infrastructure_and_missing_content_types) {
  if (!rohit::serializer::xml::available()) { GTEST_SKIP(); }
  for (const auto manifest : {
      "<Types xmlns='urn:wrong'/>",
      "<!DOCTYPE Types [<!ENTITY x SYSTEM 'file:///no-access'>]><Types/>",
      "<Types xmlns='http://schemas.openxmlformats.org/package/2006/content-types'/>"}) {
    std::ostringstream output;
    package::zip_writer writer{output};
    writer.add("[Content_Types].xml", as_bytes(manifest));
    writer.add("data.xml", as_bytes("<data/>"));
    writer.finish();
    std::istringstream input{output.str()};
    expect_error(package::error_code::invalid_metadata, [&] { package::opc_reader reader{input}; });
  }
}

// An empty relationship set still belongs to an existing source part.
TEST(package_opc, rejects_empty_relationship_set_for_absent_source) {
  if (!rohit::serializer::xml::available()) { GTEST_SKIP(); }
  constexpr std::string_view manifest =
      "<Types xmlns='http://schemas.openxmlformats.org/package/2006/content-types'>"
      "<Default Extension='rels' "
      "ContentType='application/vnd.openxmlformats-package.relationships+xml'/>"
      "</Types>";
  constexpr std::string_view relationships =
      "<Relationships xmlns='http://schemas.openxmlformats.org/package/2006/relationships'/>";
  std::ostringstream output;
  package::zip_writer writer{output};
  writer.add("[Content_Types].xml", as_bytes(manifest));
  writer.add("missing/_rels/document.xml.rels", as_bytes(relationships));
  writer.finish();
  std::istringstream input{output.str()};
  expect_error(package::error_code::missing_part, [&] { package::opc_reader reader{input}; });
}
