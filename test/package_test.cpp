// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/package.hpp>
#include <rohit/package_profile.hpp>

#include <rohit/compression.hpp>
#include <rohit/xml.hpp>

#include <gtest/gtest.h>

#include <array>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
namespace package = rohit::serializer::package;
namespace compression = rohit::serializer::compression;
using bytes = std::vector<std::uint8_t>;

// Preserve the previously valid compile-time policies and their literal copied state.
constexpr compression::encode_limits legacy_compression_encode_limits{1024, 2048};
constexpr compression::decode_options legacy_compression_decode_options{
    compression::format::none, 1024, 2048, 4096, nullptr};
constexpr auto copied_compression_encode_limits = legacy_compression_encode_limits;
constexpr auto copied_compression_decode_options = legacy_compression_decode_options;
static_assert(copied_compression_encode_limits.max_output_bytes == 2048);
static_assert(copied_compression_decode_options.max_window_bytes == 4096);
static_assert(!copied_compression_encode_limits.check_cancelled);
static_assert(!copied_compression_decode_options.check_cancelled);
static_assert(std::is_trivially_copyable_v<compression::cancellation_callback>);
static_assert(std::is_trivially_copyable_v<compression::encode_limits>);
static_assert(std::is_trivially_copyable_v<compression::decode_options>);
using mutable_cancellation_probe = decltype([count = 0]() mutable { return ++count; });
using const_cancellation_probe = decltype([] {});
static_assert(std::is_nothrow_constructible_v<compression::cancellation_callback,
    mutable_cancellation_probe&>);
static_assert(std::is_constructible_v<compression::cancellation_callback,
    const const_cancellation_probe&>);
static_assert(!std::is_constructible_v<compression::cancellation_callback,
    const mutable_cancellation_probe&>);
static_assert(!std::is_constructible_v<compression::cancellation_callback,
    const_cancellation_probe&&>);
static_assert(!std::is_constructible_v<compression::cancellation_callback,
    const const_cancellation_probe&&>);

// Borrow string bytes only for the duration of a complete package call.
std::span<const std::uint8_t> as_bytes(std::string_view value) {
  return {reinterpret_cast<const std::uint8_t*>(value.data()), value.size()};
}

// Inspect decoded payloads without C-string truncation or encoding conversion.
std::string as_text(const bytes& value) { return {value.begin(), value.end()}; }

// Author independent XML UTF-16 bytes, including the mandatory BOM, without the XML writer.
std::string utf16_document(std::u16string_view value, bool little_endian) {
  std::string result = little_endian ? "\xff\xfe" : "\xfe\xff";
  for (const auto unit : value) {
    const auto low = static_cast<char>(unit & 0xffU);
    const auto high = static_cast<char>(unit >> 8U);
    result += little_endian ? low : high;
    result += little_endian ? high : low;
  }
  return result;
}

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

// Independently author standard ZIP metadata so MCE reader tests do not use its canonical writer.
std::string relationship_archive(std::string_view relationships) {
  constexpr std::string_view manifest =
      "<Types xmlns='http://schemas.openxmlformats.org/package/2006/content-types'>"
      "<Default Extension='bin' ContentType='application/octet-stream'/>"
      "<Default Extension='rels' ContentType='application/vnd.openxmlformats-package.relationships+xml'/>"
      "</Types>";
  std::ostringstream output;
  package::zip_writer archive{output};
  archive.add("[Content_Types].xml", as_bytes(manifest));
  archive.add("_rels/.rels", as_bytes(relationships));
  archive.add("model.bin", as_bytes("independent"));
  archive.finish();
  return output.str();
}

// Author complete OPC metadata as literal XML through plain ZIP, independent of the OPC writer.
std::string content_type_archive(std::string_view declarations, bool include_manifest = true) {
  std::ostringstream output;
  package::zip_writer archive{output};
  if (include_manifest) {
    const auto manifest = std::string{
        "<Types xmlns='http://schemas.openxmlformats.org/package/2006/content-types'>"} +
        std::string{declarations} + "</Types>";
    archive.add("[Content_Types].xml", as_bytes(manifest));
  }
  archive.add("Alternate/root.xml", as_bytes(
      "<model xmlns='urn:serializer:package-test' name='independent'/>"));
  archive.add("Textures/raw.bin", as_bytes("attachment"));
  archive.add("Extras/part%3Aname.bin", as_bytes("opaque"));
  archive.add("_rels/.rels", as_bytes(
      "<Relationships xmlns='http://schemas.openxmlformats.org/package/2006/relationships'>"
      "<Relationship Id='main' Type='urn:serializer:main-model' Target='Alternate/root.xml'/>"
      "</Relationships>"));
  archive.add("Alternate/_rels/root.xml.rels", as_bytes(
      "<Relationships xmlns='http://schemas.openxmlformats.org/package/2006/relationships'>"
      "<Relationship Id='texture' Type='urn:serializer:attachment' Target='../Textures/raw.bin'/>"
      "</Relationships>"));
  archive.finish();
  return output.str();
}

// Identify the central directory in small fixed test archives.
std::size_t central_offset(const std::string& archive) {
  return archive.find(std::string{"PK\x01\x02", 4});
}

// Find one independently framed central record without assuming local/central name ordering.
std::size_t central_entry_offset(const std::string& archive, std::string_view name) {
  auto position = central_offset(archive);
  while (position != std::string::npos && position <= archive.size() && archive.size() - position >= 46U) {
    const auto word = [&](std::size_t offset) {
      return static_cast<std::size_t>(static_cast<unsigned char>(archive[position + offset])) |
          (static_cast<std::size_t>(static_cast<unsigned char>(archive[position + offset + 1U])) << 8U);
    };
    const auto name_bytes = word(28U), extra_bytes = word(30U), comment_bytes = word(32U);
    const auto variable_bytes = name_bytes + extra_bytes + comment_bytes;
    if (variable_bytes > archive.size() - position - 46U) { break; }
    if (std::string_view{archive}.substr(position + 46U, name_bytes) == name) { return position; }
    position += 46U + variable_bytes;
    if (std::string_view{archive}.substr(position, 4U) != std::string_view{"PK\x01\x02", 4U}) { break; }
  }
  throw std::runtime_error{"Fixture central entry is absent"};
}

// Replace an intentionally corrupted little-endian field in a fixture.
void overwrite(bytes& data, std::size_t offset, std::uint32_t value, std::size_t count) {
  for (std::size_t index = 0; index < count; ++index) {
    data[offset + index] = static_cast<std::uint8_t>(value >> (index * 8));
  }
}

// Construct a byte string preserving embedded ZIP header zero bytes.
std::string to_string(const bytes& data) { return {data.begin(), data.end()}; }

// Set host attributes only in the central directory, independently of the package writer API.
std::string physical_attributes(std::string archive, std::string_view name,
    std::uint8_t creator_system, std::uint32_t attributes) {
  const auto position = central_entry_offset(archive, name);
  bytes changed{archive.begin(), archive.end()};
  changed[position + 5U] = creator_system;
  overwrite(changed, position + 38U, attributes, 4U);
  return to_string(changed);
}

// Load an independently produced Python zipfile fixture from the source tree.
std::ifstream independent_fixture(std::string_view name) {
  return std::ifstream{std::filesystem::path{__FILE__}.parent_path() / "resources" /
      std::string{name}, std::ios::binary};
}

// Load the shared independent mutation inventory, keeping index and payload checks separate.
std::vector<std::string> malformed_package_fixtures(std::string_view stage) {
  auto manifest = independent_fixture("package_malformed_fixtures/cases.txt");
  if (!manifest) { throw std::runtime_error{"Missing malformed package fixture inventory"}; }
  std::vector<std::string> result;
  std::string selected, filename;
  while (manifest >> selected >> filename) {
    if (selected != "index" && selected != "integrity") {
      throw std::runtime_error{"Unknown malformed package fixture stage"};
    }
    if (selected == stage) { result.push_back("package_malformed_fixtures/" + filename); }
  }
  if (!manifest.eof() || result.empty()) {
    throw std::runtime_error{"Invalid or empty malformed package fixture inventory"};
  }
  return result;
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

// Borrowed hooks share the original callable state while retaining safe empty behavior.
TEST(package_compression, cancellation_hooks_borrow_const_and_mutable_lvalues) {
  compression::cancellation_callback empty;
  EXPECT_FALSE(empty);
  EXPECT_NO_THROW(empty());
  auto mutable_check = [count = std::size_t{}]() mutable { return ++count; };
  compression::cancellation_callback first{mutable_check};
  const auto second = first;
  ASSERT_TRUE(first);
  first();
  second();
  EXPECT_EQ(mutable_check(), 3U);

  std::size_t polls{};
  const auto const_check = [&polls] { ++polls; };
  compression::encode_limits limits{1024, 2048, const_check};
  const auto copied = limits;
  copied.check_cancelled();
  compression::decode_options options{
      compression::format::none, 1024, 2048, 4096, nullptr, const_check};
  options.check_cancelled();
  EXPECT_EQ(polls, 2U);
  limits.check_cancelled = nullptr;
  EXPECT_FALSE(limits.check_cancelled);
  EXPECT_NO_THROW(limits.check_cancelled());

  const auto throw_cancelled = [] { throw std::runtime_error{"cancelled"}; };
  const compression::cancellation_callback throwing{throw_cancelled};
  EXPECT_THROW(throwing(), std::runtime_error);
}

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
TEST(package_zip, rejects_unsupported_flags_methods_missing_zip64_and_conflicting_headers) {
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
  expect_error(package::error_code::invalid_archive,
      [&] { package::zip_reader reader{zip64_input}; });
  bytes mismatched{archive.begin(), archive.end()};
  mismatched[30] = 'x';
  std::istringstream mismatched_input{to_string(mismatched)};
  expect_error(package::error_code::invalid_archive,
      [&] { package::zip_reader reader{mismatched_input}; });
}

// ZIP64 uses standard extra/end records even when a profile deliberately requests small entries.
TEST(package_zip, zip64_sizes_end_records_and_malformed_extra) {
  const auto encoded = make_archive({.force_zip64 = true});
  std::istringstream input{encoded};
  package::zip_reader reader{input};
  EXPECT_TRUE(reader.entries().front().zip64_sizes);
  EXPECT_EQ(as_text(reader.read("data/model.json")), "{\"count\":7}");
  EXPECT_NE(encoded.find(std::string{"PK\x06\x06", 4}), std::string::npos);
  EXPECT_NE(encoded.find(std::string{"PK\x06\x07", 4}), std::string::npos);
  bytes corrupt{encoded.begin(), encoded.end()};
  // The local ZIP64 field must include both expanded and compressed sizes.
  overwrite(corrupt, 30 + std::string_view{"data/model.json"}.size() + 2, 8, 2);
  std::istringstream malformed{to_string(corrupt)};
  expect_error(package::error_code::invalid_archive,
      [&] { package::zip_reader rejected{malformed}; });
}

// Independent Python output covers both expanded end records and signed 64-bit descriptors.
TEST(package_zip, independent_zip64_end_and_descriptor_fixtures) {
  for (const auto fixture : {"package_python_zip64.zip", "package_python_zip64_descriptor.zip"}) {
    auto input = independent_fixture(fixture);
    ASSERT_TRUE(input.is_open());
    package::zip_reader reader{input};
    ASSERT_EQ(reader.entries().size(), 1U);
    EXPECT_EQ(as_text(reader.read("data/model.json")), "{\"count\":7}");
  }
}

// Literal ZIP framing covers every optional descriptor signature and size width, including CRC ambiguity.
TEST(package_zip, independent_signed_unsigned_descriptor_forms_and_crc_signature) {
  const std::array samples{
      std::pair<std::string_view, std::string_view>{"empty", ""},
      std::pair<std::string_view, std::string_view>{"model", "{\"count\":7}"},
      std::pair<std::string_view, std::string_view>{"signature_crc", {"\xac\x0a\x7a\xd5", 4}}};
  for (const auto wide : {false, true}) {
    for (const auto signed_descriptor : {false, true}) {
      const auto prefix = std::string{"package_descriptor_fixtures/"} +
          (wide ? "zip64_" : "zip32_") + (signed_descriptor ? "signed_" : "unsigned_");
      for (const auto& [name, payload] : samples) {
        const auto filename = prefix + std::string{name} + ".zip";
        SCOPED_TRACE(filename);
        auto input = independent_fixture(filename);
        ASSERT_TRUE(input.is_open());
        package::zip_reader reader{input};
        ASSERT_EQ(reader.entries().size(), 1U);
        EXPECT_EQ(reader.entries().front().zip64_sizes, wide);
        if (compression::available(compression::format::deflate)) {
          EXPECT_EQ(as_text(reader.read("data/model.json")), payload);
        } else {
          expect_error(package::error_code::unavailable, [&] {
            static_cast<void>(reader.read("data/model.json"));
          });
        }
      }
      auto corrupted = independent_fixture(prefix + "bad_crc.zip");
      ASSERT_TRUE(corrupted.is_open());
      expect_error(package::error_code::integrity, [&] { package::zip_reader reader{corrupted}; });
    }
  }
}

// Nonseekable input never triggers implicit memory buffering or filesystem extraction.
TEST(package_zip, nonseekable_input_requires_bounded_explicit_disk_spooling) {
  struct nonseekable_buffer final : std::stringbuf {
    std::size_t requested_bytes{};
    bool fail_after_first{};
    // Expose ordinary input bytes while refusing random-access positioning.
    explicit nonseekable_buffer(const std::string& bytes, bool fail = false)
        : std::stringbuf{bytes}, fail_after_first{fail} {}
    // Deliberately model a network/pipe source with no positioning capability.
    pos_type seekoff(off_type, std::ios_base::seekdir, std::ios_base::openmode) override {
      return pos_type{off_type{-1}};
    }
    // Refuse absolute seeking just as the offset-based capability does.
    pos_type seekpos(pos_type, std::ios_base::openmode) override {
      return pos_type{off_type{-1}};
    }
    // Record physical read requests to verify staging never requests an unbounded excess suffix.
    std::streamsize xsgetn(char* output, std::streamsize count) override {
      if (fail_after_first && requested_bytes != 0) {
        throw std::ios_base::failure{"Injected nonseekable source failure"};
      }
      requested_bytes += static_cast<std::size_t>(count);
      return std::stringbuf::xsgetn(output, count);
    }
  };
  temporary_directory directory;
  const auto encoded = make_archive({.force_zip64 = true});
  nonseekable_buffer unsupported_buffer{encoded};
  std::istream unsupported{&unsupported_buffer};
  expect_error(package::error_code::unsupported_feature,
               [&] { package::zip_reader rejected{unsupported}; });
  {
    nonseekable_buffer buffer{encoded};
    std::istream source{&buffer};
    package::zip_reader reader{source, {}, {directory.path, encoded.size()}};
    EXPECT_EQ(as_text(reader.read("data/model.json")), "{\"count\":7}");
    EXPECT_FALSE(std::filesystem::is_empty(directory.path));
  }
  EXPECT_TRUE(std::filesystem::is_empty(directory.path));
  {
    nonseekable_buffer throwing_buffer{encoded};
    std::istream throwing_source{&throwing_buffer};
    const auto mask = std::ios::failbit | std::ios::badbit;
    throwing_source.exceptions(mask);
    package::zip_reader reader{throwing_source, {}, {directory.path, encoded.size() + 1}};
    EXPECT_EQ(as_text(reader.read("data/model.json")), "{\"count\":7}");
    EXPECT_EQ(throwing_source.exceptions(), mask);
  }
  EXPECT_TRUE(std::filesystem::is_empty(directory.path));
  nonseekable_buffer limited_buffer{encoded};
  std::istream limited{&limited_buffer};
  expect_error(package::error_code::resource_limit, [&] {
    package::zip_reader rejected{limited, {}, {directory.path, encoded.size() - 1}};
  });
  EXPECT_TRUE(std::filesystem::is_empty(directory.path));
  nonseekable_buffer overflow_buffer{encoded};
  std::istream overflow_source{&overflow_buffer};
  expect_error(package::error_code::resource_limit,
               [&] { package::zip_reader rejected{overflow_source, {}, {directory.path, 16}}; });
  EXPECT_EQ(overflow_buffer.requested_bytes, 17U);
  EXPECT_TRUE(std::filesystem::is_empty(directory.path));

  constexpr std::size_t spool_batch_bytes = 1024U;
  const std::string payload(spool_batch_bytes * 4U, 'x');
  std::ostringstream large;
  package::zip_writer large_writer{large};
  large_writer.add("payload", as_bytes(payload));
  large_writer.finish();
  nonseekable_buffer cancelled_buffer{large.str()};
  std::istream cancelled_source{&cancelled_buffer};
  bool saw_private_file{};
  package::limits cancelled_limits;
  cancelled_limits.max_buffer_bytes = spool_batch_bytes;
  cancelled_limits.is_cancelled = [&] {
    if (cancelled_buffer.requested_bytes == 0) {
      return false;
    }
    for (const auto& entry : std::filesystem::directory_iterator{directory.path}) {
      EXPECT_TRUE(entry.is_regular_file());
      saw_private_file = true;
#ifndef _WIN32
      const auto public_permissions =
          std::filesystem::perms::group_all | std::filesystem::perms::others_all;
      EXPECT_EQ(entry.status().permissions() & public_permissions, std::filesystem::perms::none);
#endif
    }
    return true;
  };
  expect_error(package::error_code::cancelled, [&] {
    package::zip_reader rejected{cancelled_source,
                                 std::make_shared<package::budget>(cancelled_limits),
                                 {directory.path, large.str().size()}};
  });
  EXPECT_TRUE(saw_private_file);
  EXPECT_EQ(cancelled_buffer.requested_bytes, spool_batch_bytes);
  EXPECT_TRUE(std::filesystem::is_empty(directory.path));
  nonseekable_buffer failed_buffer{large.str(), true};
  std::istream failed_source{&failed_buffer};
  package::limits failed_limits;
  failed_limits.max_buffer_bytes = spool_batch_bytes;
  expect_error(package::error_code::io, [&] {
    package::zip_reader rejected{failed_source,
                                 std::make_shared<package::budget>(failed_limits),
                                 {directory.path, large.str().size()}};
  });
  EXPECT_EQ(failed_buffer.requested_bytes, spool_batch_bytes);
  EXPECT_TRUE(std::filesystem::is_empty(directory.path));
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

// ZIP32 directory offsets, record ranges and entry counts reject independently authored mutations.
TEST(package_zip, independent_directory_offset_count_and_length_failures) {
  auto control = independent_fixture("package_malformed_fixtures/valid.zip");
  package::zip_reader reader{control};
  EXPECT_NE(reader.find("Alternate/root.xml"), nullptr);
  EXPECT_NE(reader.find("Textures/raw.bin"), nullptr);
  const auto fixtures = malformed_package_fixtures("index");
  ASSERT_EQ(fixtures.size(), 6U);
  for (const auto& fixture : fixtures) {
    SCOPED_TRACE(fixture);
    auto input = independent_fixture(fixture);
    ASSERT_TRUE(input.is_open());
    expect_error(package::error_code::invalid_archive, [&] { package::zip_reader rejected{input}; });
  }
}

// Coherent ZIP framing cannot hide bad CRC, declared expanded size or inexact DEFLATE EOF.
TEST(package_zip, independent_deflate_crc_size_and_framing_failures) {
  if (!compression::available(compression::format::deflate)) { GTEST_SKIP(); }
  auto control = independent_fixture("package_malformed_fixtures/valid.zip");
  package::zip_reader reader{control};
  EXPECT_NE(as_text(reader.read("Alternate/root.xml")).find("name=\"independent\""), std::string::npos);
  const auto fixtures = malformed_package_fixtures("integrity");
  ASSERT_EQ(fixtures.size(), 4U);
  for (const auto& fixture : fixtures) {
    SCOPED_TRACE(fixture);
    auto input = independent_fixture(fixture);
    ASSERT_TRUE(input.is_open());
    package::zip_reader indexed{input};
    expect_error(package::error_code::integrity, [&] { (void)indexed.read("Alternate/root.xml"); });
  }
}

// Malformed shared fixtures never publish a caller-authored aggregate or alter its prior destination.
TEST(package_profile, independent_malformed_package_preserves_aggregate_destination) {
  if (!rohit::serializer::xml::available() ||
      !compression::available(compression::format::deflate)) { GTEST_SKIP(); }
  bool decoded{};
  const auto decode = [&](std::string& candidate, package::profile_loader& loader) {
    loader.read_related("/", "urn:serializer:main-model", "application/vnd.serializer.test-model+xml",
        [&](std::span<const std::uint8_t> data, const package::part&, package::budget&) {
          decoded = true;
          candidate.assign(data.begin(), data.end());
        });
    loader.read_related("/Alternate/root.xml", "urn:serializer:attachment", "application/octet-stream",
        [](std::span<const std::uint8_t>, const package::part&, package::budget&) {});
  };
  auto control = independent_fixture("package_malformed_fixtures/valid.zip");
  package::opc_reader control_reader{control};
  const auto accepted = package::load_opc<std::string>(control_reader, decode);
  EXPECT_TRUE(decoded);
  EXPECT_NE(accepted.value.find("name=\"independent\""), std::string::npos);
  for (const auto stage : {"index", "integrity"}) {
    for (const auto& fixture : malformed_package_fixtures(stage)) {
      SCOPED_TRACE(fixture);
      decoded = false;
      package::aggregate_result<std::string> destination{"old complete aggregate", {}};
      auto input = independent_fixture(fixture);
      ASSERT_TRUE(input.is_open());
      const auto category = std::string_view{stage} == "index"
          ? package::error_code::invalid_archive : package::error_code::integrity;
      expect_error(category, [&] {
        package::opc_reader reader{input};
        package::load_opc_into(reader, destination, decode);
      });
      EXPECT_FALSE(decoded);
      EXPECT_EQ(destination.value, "old complete aggregate");
      EXPECT_TRUE(destination.extensions.parts.empty());
      EXPECT_TRUE(destination.extensions.relationships.empty());
    }
  }
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

// RFC 3986 resolution stays inside the package and identifies an entire part, not a fragment.
TEST(package_opc, scoped_uri_resolution) {
  EXPECT_EQ(package::resolve_target("/data/document.xml", "../media/image.png"), "/media/image.png");
  EXPECT_EQ(package::resolve_target("/data/document.xml", "./other.xml"), "/data/other.xml");
  EXPECT_EQ(package::resolve_target("/", "/media/a%20b.png"), "/media/a%20b.png");
  EXPECT_EQ(package::resolve_target("/data/document.xml", "../../outside"), "/outside");
  EXPECT_EQ(package::resolve_target("/data/document.xml", "#section"), "/data/document.xml");
  EXPECT_EQ(package::resolve_target("/data/document.xml", "?revision=2#section"), "/data/document.xml");
  EXPECT_EQ(package::resolve_target("/data/document.xml", ""), "/data/document.xml");
  EXPECT_EQ(package::resolve_target("/data/document.xml", "other.xml?revision=2#section"), "/data/other.xml");
  EXPECT_EQ(package::resolve_target("/data/document.xml", "./name:with-colon"), "/data/name:with-colon");
  for (const auto target : {"%2e%2e/outside", "a%2Fb", "a%5Cb", "//host/a", "http://host", "name:scheme",
                           "other/.", "other/..", ".", "..", "a{b}", "a[0]"}) {
    EXPECT_THROW(static_cast<void>(package::resolve_target("/data/document.xml", target)), package::error);
  }
}

// OPC URI names and schema-valid media defaults do not weaken ordinary ZIP name restrictions.
TEST(package_opc, standard_uri_colons_extension_tokens_and_reserved_media_types) {
  if (!rohit::serializer::xml::available()) { GTEST_SKIP(); }
  std::ostringstream output;
  package::opc_writer writer{output};
  writer.content_type_default("bin+v1", "application/octet-stream");
  writer.content_type_default("unused%20name", "text/plain");
  writer.add_part("/data:one.bin+v1", "application/octet-stream; name=\"\xc3\xa9\"", as_bytes("payload"));
  writer.add_part("/escaped%00.bin", "application/octet-stream", {});
  writer.add_part("/_rels/nested/plain.rels", "application/octet-stream", as_bytes("ordinary"));
  writer.add_relationship({"/", "\xc3\xa9" "clair", "urn:test:main", "./data:one.bin+v1"});
  writer.add_relationship({"/data:one.bin+v1", "self", "urn:test:self", "?view=2#section"});
  writer.add_relationship({"/", "external", "urn:test:external", "https://[2001:db8::1]:443/a?x#f",
                            package::target_mode::external});
  for (const auto type : {"application/{invalid}", "application/octet-stream; p=\"bad\rvalue\"",
                         "application/vnd.openxmlformats-package.relationships+xml",
                         "application/vnd.openxmlformats-package.core-properties+xml; charset=utf-8"}) {
    expect_error(package::error_code::invalid_metadata, [&] {
      writer.add_part("/invalid.bin", type, {});
    });
  }
  for (const auto target : {"https://[not-ip]/", "https://[::1", "https://host:port/", "https://host/a|b",
                           "https://host/a#one#two"}) {
    expect_error(package::error_code::invalid_relationship, [&] {
      writer.add_relationship({"/", "invalid", "urn:test:external", target, package::target_mode::external});
    });
  }
  expect_error(package::error_code::invalid_relationship, [&] {
    writer.add_relationship({"/", "invalid", "urn:test:type#fragment", "./data:one.bin+v1"});
  });
  writer.finish();
  std::istringstream input{output.str()};
  package::opc_reader reader{input};
  EXPECT_EQ(as_text(reader.read("/DATA:ONE.BIN+V1")), "payload");
  EXPECT_NE(reader.find("/escaped%00.bin"), nullptr);
  EXPECT_EQ(as_text(reader.read("/_rels/nested/plain.rels")), "ordinary");
  ASSERT_EQ(reader.find_relationships("/data:one.bin+v1", "urn:test:self").size(), 1U);
  EXPECT_EQ(reader.find_relationships("/", "urn:test:main").front()->id, "\xc3\xa9" "clair");
  std::istringstream ordinary{output.str()};
  expect_error(package::error_code::invalid_name, [&] { package::zip_reader archive{ordinary}; });
}

// Independently authored metadata follows XSD whitespace rules and permits relationship string content.
TEST(package_opc, independent_relationship_metadata_uses_schema_lexical_rules) {
  if (!rohit::serializer::xml::available()) { GTEST_SKIP(); }
  constexpr std::string_view manifest =
      "<Types xmlns='http://schemas.openxmlformats.org/package/2006/content-types'>"
      "<Default Extension='bin+v1' ContentType='APPLICATION/OCTET-STREAM'/>"
      "<Default Extension='rels' ContentType='application/vnd.openxmlformats-package.relationships+xml'/>"
      "</Types>";
  constexpr std::string_view relationships =
      "<Relationships xmlns='http://schemas.openxmlformats.org/package/2006/relationships'>"
      "<Relationship Id=' \xc3\xa9" "clair ' Type=' urn:test:main ' Target=' model.bin+v1?x#section '>"
      "Schema-valid string content</Relationship></Relationships>";
  std::ostringstream output;
  package::zip_writer archive{output};
  archive.add("[Content_Types].xml", as_bytes(manifest));
  archive.add("_rels/.rels", as_bytes(relationships));
  archive.add("model.bin+v1", as_bytes("independent"));
  archive.finish();
  std::istringstream input{output.str()};
  package::opc_reader reader{input};
  ASSERT_EQ(reader.relationships().size(), 1U);
  EXPECT_EQ(reader.relationships().front().id, "\xc3\xa9" "clair");
  EXPECT_EQ(reader.relationships().front().target, "model.bin+v1?x#section");
  EXPECT_EQ(as_text(reader.read("/model.bin+v1")), "independent");
  package::profile_loader profile{reader};
  bool decoded{};
  profile.read_related("/", "urn:test:main", "application/octet-stream",
      [&](std::span<const std::uint8_t> payload, const package::part&, package::budget&) {
        decoded = as_text(bytes{payload.begin(), payload.end()}) == "independent";
      });
  EXPECT_TRUE(decoded);
  EXPECT_TRUE(profile.finish().parts.empty());
}

// MIME role identity folds names, compares decoded values exactly, and ignores parameter ordering.
TEST(package_opc, typed_media_roles_preserve_parameter_value_case) {
  if (!rohit::serializer::xml::available()) { GTEST_SKIP(); }
  constexpr std::string_view manifest =
      "<Types xmlns='http://schemas.openxmlformats.org/package/2006/content-types'>"
      "<Default Extension='bin' ContentType='APPLICATION/Custom+XML; Profile=&quot;Foo;B\\ar&quot;; VERSION=One'/>"
      "<Default Extension='rels' ContentType='application/vnd.openxmlformats-package.relationships+xml'/>"
      "</Types>";
  constexpr std::string_view relationships =
      "<Relationships xmlns='http://schemas.openxmlformats.org/package/2006/relationships'>"
      "<Relationship Id='id' Type='urn:test:main' Target='model.bin'/></Relationships>";
  std::ostringstream output;
  package::zip_writer archive{output};
  archive.add("[Content_Types].xml", as_bytes(manifest));
  archive.add("_rels/.rels", as_bytes(relationships));
  archive.add("model.bin", as_bytes("independent"));
  archive.finish();
  for (const auto& [expected, matches] : {
      std::pair{"application/custom+xml; version=\"One\"; PROFILE=\"Foo;Bar\"", true},
      std::pair{"APPLICATION/CUSTOM+XML; profile=\"Foo;B\\ar\"; version=One", true},
      std::pair{"application/custom+xml; version=One; profile=\"foo;Bar\"", false},
      std::pair{"application/custom+xml; version=one; profile=\"Foo;Bar\"", false},
      std::pair{"application/custom+xml; version=One; profile=\"Foo;BAR\"", false},
      std::pair{"application/other+xml; version=One; profile=\"Foo;Bar\"", false},
      std::pair{"application/custom+xml; profile=\"Foo;Bar\"", false}}) {
    SCOPED_TRACE(expected);
    std::istringstream input{output.str()};
    package::opc_reader reader{input};
    package::profile_loader profile{reader};
    bool decoded{};
    const auto load = [&] {
      profile.read_related("/", "urn:test:main", expected,
          [&](std::span<const std::uint8_t>, const package::part&, package::budget&) { decoded = true; });
    };
    if (matches) { EXPECT_NO_THROW(load()); }
    else { expect_error(package::error_code::invalid_metadata, load); }
    EXPECT_EQ(decoded, matches);
  }
}

// Duplicate default/override identities reject before selection, including equivalent spellings.
TEST(package_opc, independent_content_type_identities_and_override_precedence) {
  if (!rohit::serializer::xml::available()) { GTEST_SKIP(); }
  constexpr std::string_view defaults =
      "<Default Extension='bin' ContentType='application/octet-stream'/>"
      "<Default Extension='rels' ContentType='application/vnd.openxmlformats-package.relationships+xml'/>";
  constexpr std::string_view main_override =
      "<Override PartName='/Alternate/root.xml' ContentType='application/vnd.serializer.test-model+xml'/>";
  const auto base = std::string{defaults} + std::string{main_override};
  for (const auto& [label, duplicate] : std::array{
      std::pair{"default identical", "<Default Extension='bin' ContentType='application/octet-stream'/>"},
      std::pair{"default conflicting", "<Default Extension='bin' ContentType='text/plain'/>"},
      std::pair{"default case", "<Default Extension='BIN' ContentType='text/plain'/>"},
      std::pair{"default percent case", "<Default Extension='bi%3An' ContentType='text/plain'/>"
          "<Default Extension='BI%3aN' ContentType='application/octet-stream'/>"},
      std::pair{"override identical", "<Override PartName='/Alternate/root.xml' ContentType='application/vnd.serializer.test-model+xml'/>"},
      std::pair{"override conflicting", "<Override PartName='/Alternate/root.xml' ContentType='text/plain'/>"},
      std::pair{"override case", "<Override PartName='/ALTERNATE/ROOT.XML' ContentType='text/plain'/>"},
      std::pair{"override whitespace", "<Override PartName=' /Alternate/root.xml &#x9;' ContentType='text/plain'/>"},
      std::pair{"override percent case", "<Override PartName='/Extras/part%3Aname.bin' ContentType='text/plain'/>"
          "<Override PartName='/EXTRAS/PART%3aname.BIN' ContentType='application/octet-stream'/>"}}) {
    SCOPED_TRACE(label);
    for (const bool prepend : {false, true}) {
      SCOPED_TRACE(prepend);
      std::istringstream input{content_type_archive(prepend ? duplicate + base : base + duplicate)};
      expect_error(package::error_code::invalid_metadata, [&] { package::opc_reader reader{input}; });
    }
  }
  // A distinct override legitimately takes precedence over a different extension default.
  for (const auto& declarations : {base,
      base + "<Override PartName='/Textures/raw.bin' ContentType='application/custom+binary'/>",
      base + "<Override PartName=' /EXTRAS/PART%3aname.BIN &#x9;' ContentType='text/plain'/>"}) {
    std::istringstream input{content_type_archive(declarations)};
    package::opc_reader reader{input};
    EXPECT_EQ(reader.parts().size(), 3U);
    EXPECT_EQ(as_text(reader.read("/Alternate/root.xml")),
        "<model xmlns='urn:serializer:package-test' name='independent'/>");
    if (declarations.find("application/custom+binary") != std::string::npos) {
      EXPECT_EQ(reader.find("/Textures/raw.bin")->content_type, "application/custom+binary");
    }
    if (declarations.find("text/plain") != std::string::npos) {
      EXPECT_EQ(reader.find("/Extras/part%3Aname.bin")->content_type, "text/plain");
    }
  }
}

// Missing declarations and malformed/reserved media types fail independently of generated output.
TEST(package_opc, independent_content_type_required_fields_and_media_types) {
  if (!rohit::serializer::xml::available()) { GTEST_SKIP(); }
  constexpr std::string_view binary_default =
      "<Default Extension='bin' ContentType='application/octet-stream'/>";
  constexpr std::string_view relationships_default =
      "<Default Extension='rels' ContentType='application/vnd.openxmlformats-package.relationships+xml'/>";
  constexpr std::string_view main_override =
      "<Override PartName='/Alternate/root.xml' ContentType='application/vnd.serializer.test-model+xml'/>";
  for (const auto& declarations : std::array{
      std::string{}, std::string{relationships_default} + std::string{main_override},
      std::string{binary_default} + std::string{relationships_default},
      std::string{relationships_default} + std::string{main_override} + "<Default Extension='bin'/>",
      std::string{relationships_default} + std::string{main_override} + "<Default ContentType='text/plain'/>",
      std::string{binary_default} + std::string{relationships_default} + "<Override PartName='/Alternate/root.xml'/>",
      std::string{binary_default} + std::string{relationships_default} + "<Override ContentType='text/plain'/>"}) {
    SCOPED_TRACE(declarations);
    std::istringstream input{content_type_archive(declarations)};
    expect_error(package::error_code::invalid_metadata, [&] { package::opc_reader reader{input}; });
  }
  for (const auto type : {"", "text", "application/{invalid}",
      "application/octet-stream; p=&quot;unterminated",
      "APPLICATION/VND.OPENXMLFORMATS-PACKAGE.RELATIONSHIPS+XML",
      "application/vnd.openxmlformats-package.core-properties+xml; charset=utf-8"}) {
    SCOPED_TRACE(type);
    for (const bool use_default : {false, true}) {
      SCOPED_TRACE(use_default);
      const auto declarations = use_default
          ? std::string{relationships_default} + std::string{main_override} +
              "<Default Extension='bin' ContentType='" + type + "'/>"
          : std::string{binary_default} + std::string{relationships_default} +
              "<Override PartName='/Alternate/root.xml' ContentType='" + type + "'/>";
      std::istringstream input{content_type_archive(declarations)};
      expect_error(package::error_code::invalid_metadata, [&] { package::opc_reader reader{input}; });
    }
  }
  std::istringstream absent{content_type_archive({}, false)};
  EXPECT_THROW(package::opc_reader reader{absent}, package::error);
}

// OPC infrastructure accepts exactly XML's UTF-8/UTF-16 encodings with original byte/tree bounds.
TEST(package_opc, independent_utf16_metadata_preserves_encoding_and_decoded_bounds) {
  namespace xml = rohit::serializer::xml;
  if (!xml::available()) { GTEST_SKIP(); }
  constexpr std::u16string_view manifest =
      u"<Types xmlns='http://schemas.openxmlformats.org/package/2006/content-types'>"
      u"<Default Extension='bin' ContentType='application/octet-stream'/>"
      u"<Default Extension='rels' ContentType='application/vnd.openxmlformats-package.relationships+xml'/>"
      u"</Types>";
  constexpr std::u16string_view relationships =
      u"<Relationships xmlns='http://schemas.openxmlformats.org/package/2006/relationships'>"
      u"<Relationship Id='\u00e9clair' Type='urn:test:main' Target='model.bin'/>"
      u"</Relationships>";
  const auto archive_bytes = [&](std::string_view types, std::string_view links) {
    std::ostringstream output;
    package::zip_writer archive{output};
    archive.add("[Content_Types].xml", as_bytes(types));
    archive.add("_rels/.rels", as_bytes(links));
    archive.add("model.bin", as_bytes("independent"));
    archive.finish();
    return output.str();
  };
  for (const bool little_endian : {false, true}) {
    for (const auto declaration : {u"", u"<?xml version='1.0' encoding='UTF-16'?>"}) {
      const auto types = utf16_document(std::u16string{declaration} + std::u16string{manifest}, little_endian);
      const auto links = utf16_document(std::u16string{declaration} + std::u16string{relationships}, !little_endian);
      std::istringstream input{archive_bytes(types, links)};
      package::opc_reader reader{input};
      EXPECT_EQ(as_text(reader.read("/model.bin")), "independent");
      ASSERT_EQ(reader.relationships().size(), 1U);
      EXPECT_EQ(reader.relationships().front().id, "\xc3\xa9" "clair");
      EXPECT_THROW((void)xml::parse(types), std::invalid_argument);
      xml::operation_context context{};
      context.input_encoding = xml::input_encoding_policy::utf8_or_utf16;
      xml::limits raw_bound{};
      raw_bound.max_input_bytes = types.size() - 1U;
      EXPECT_THROW((void)xml::parse(types, raw_bound, &context), std::length_error);
    }
    for (const auto declaration : {u"<?xml version='1.0' encoding='UTF-8'?>",
                                   u"<?xml version='1.0' encoding='ISO-8859-1'?>",
                                   u"<?xml version='1.0' encoding='UTF-16LE'?>",
                                   u"<?xml version='1.0' encoding='UTF-16BE'?>"}) {
      const auto types = utf16_document(std::u16string{declaration} + std::u16string{manifest}, little_endian);
      std::istringstream input{archive_bytes(types, utf16_document(relationships, little_endian))};
      expect_error(package::error_code::invalid_metadata, [&] { package::opc_reader reader{input}; });
    }
  }
  xml::operation_context context{};
  context.input_encoding = xml::input_encoding_policy::utf8_or_utf16;
  const auto text = utf16_document(u"<r>\u00e9\u00e9</r>", true);
  xml::limits decoded_bound{};
  decoded_bound.max_text_bytes = 3U;
  EXPECT_THROW((void)xml::parse(text, decoded_bound, &context), std::length_error);
  EXPECT_EQ(xml::parse(text, {}, &context).text, "\xc3\xa9\xc3\xa9");
  const xml::prepared_schema schema{
      "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'>"
      "<xs:element name='r' type='xs:string'/></xs:schema>"};
  EXPECT_NO_THROW(schema.validate(text, {}, &context));
  auto odd = text;
  odd.pop_back();
  const auto no_bom = text.substr(2U);
  const auto utf32 = std::string{"\xff\xfe\0\0<\0\0\0r\0\0\0/\0\0\0>\0\0\0", 20U};
  const auto unpaired = utf16_document(std::u16string{u"<r>"} + char16_t{0xd800U} + u"</r>", true);
  for (const auto& invalid : {odd, no_bom, utf32, unpaired,
         std::string{"<?xml version='1.0' encoding='UTF-16'?><r/>"},
         std::string{"<?xml version='1.0' encoding='ISO-8859-1'?><r/>"}}) {
    EXPECT_THROW((void)xml::parse(invalid, {}, &context), std::invalid_argument);
  }
}

// Inherited expanded-name rules and ordered selection process infrastructure before its schema.
TEST(package_opc, independent_relationship_mce_scopes_choices_and_mixed_content) {
  if (!rohit::serializer::xml::available()) { GTEST_SKIP(); }
  constexpr std::string_view relationships =
      "<Relationships xmlns='http://schemas.openxmlformats.org/package/2006/relationships' "
      "xmlns:r='http://schemas.openxmlformats.org/package/2006/relationships' "
      "xmlns:mc='http://schemas.openxmlformats.org/markup-compatibility/2006' xmlns:e='urn:extension' "
      "mc:Ignorable='e' mc:ProcessContent='e:box' mc:MustUnderstand='r' e:attribute='ignored'>"
      "<e:discard mc:MustUnderstand='e'>unknown <bad/> content</e:discard>"
      "<e:box><Relationship Id='wrapped' Type='urn:test:main' Target='model.bin'>"
      "before<e:discard/>after</Relationship></e:box>"
      "<mc:AlternateContent e:attribute='ignored'>"
      "<mc:Choice Requires='e' mc:MustUnderstand='e'><Relationship Id='unknown' Target='missing'/></mc:Choice>"
      "<mc:Choice Requires='r'><Relationship Id='choice' Type='urn:test:main' Target='model.bin'/></mc:Choice>"
      "<mc:Fallback mc:MustUnderstand='e'><bad/></mc:Fallback><e:future/>"
      "</mc:AlternateContent>"
      "<mc:AlternateContent><mc:Choice Requires='xml'><bad/></mc:Choice>"
      "<mc:Fallback><Relationship Id='fallback' Type='urn:test:main' Target='model.bin'/></mc:Fallback>"
      "</mc:AlternateContent>"
      "<mc:AlternateContent><mc:Choice Requires='e'><bad/></mc:Choice></mc:AlternateContent>"
      "<e:box xmlns:e='urn:rebound' mc:Ignorable='e' mc:ProcessContent='e:*'>"
      "<Relationship Id='rebound' Type='urn:test:main' Target='model.bin'/></e:box>"
      "</Relationships>";
  std::istringstream input{relationship_archive(relationships)};
  package::opc_reader reader{input};
  ASSERT_EQ(reader.relationships().size(), 4U);
  EXPECT_EQ(reader.relationships()[0U].id, "wrapped");
  EXPECT_EQ(reader.relationships()[1U].id, "choice");
  EXPECT_EQ(reader.relationships()[2U].id, "fallback");
  EXPECT_EQ(reader.relationships()[3U].id, "rebound");
  constexpr std::string_view alternate_root =
      "<mc:AlternateContent xmlns:mc='http://schemas.openxmlformats.org/markup-compatibility/2006' "
      "xmlns:r='http://schemas.openxmlformats.org/package/2006/relationships'>"
      "<mc:Choice Requires='r'><r:Relationships><r:Relationship Id='root' Type='urn:test:main' "
      "Target='model.bin'/></r:Relationships></mc:Choice></mc:AlternateContent>";
  std::istringstream root_input{relationship_archive(alternate_root)};
  package::opc_reader root_reader{root_input};
  ASSERT_EQ(root_reader.relationships().size(), 1U);
  EXPECT_EQ(root_reader.relationships().front().id, "root");
}

// Malformed or unsupported live MCE fails before it can weaken required OPC metadata checks.
TEST(package_opc, rejects_mce_mismatches_and_bounds_preprocessing_work) {
  if (!rohit::serializer::xml::available()) { GTEST_SKIP(); }
  constexpr std::string_view beginning =
      "<Relationships xmlns='http://schemas.openxmlformats.org/package/2006/relationships' "
      "xmlns:r='http://schemas.openxmlformats.org/package/2006/relationships' "
      "xmlns:mc='http://schemas.openxmlformats.org/markup-compatibility/2006' xmlns:e='urn:extension' ";
  for (const auto body : {
      "mc:Ignorable='missing'>",
      "mc:Ignorable='mc'>",
      "mc:ProcessContent='e:box'>",
      "mc:Ignorable='e' mc:ProcessContent='e'>",
      "mc:Ignorable='e' mc:ProcessContent='e:bad:name'>",
      "mc:MustUnderstand='e'>",
      "mc:MustUnderstand='xml'>",
      "mc:Unknown=''>",
      "><e:unknown/>",
      "><mc:Choice Requires='r'/>",
      "><mc:Fallback/>",
      "><mc:Unknown/>",
      "><mc:AlternateContent/>",
      "><mc:AlternateContent><mc:Fallback/></mc:AlternateContent>",
      "><mc:AlternateContent><mc:Choice/></mc:AlternateContent>",
      "><mc:AlternateContent><mc:Choice Requires=''/></mc:AlternateContent>",
      "><mc:AlternateContent><mc:Choice Requires='missing'/></mc:AlternateContent>",
      "><mc:AlternateContent><mc:Choice Requires='mc'/></mc:AlternateContent>",
      "><mc:AlternateContent><mc:Choice Requires='r'/><mc:Fallback/><mc:Fallback/></mc:AlternateContent>",
      "><mc:AlternateContent><mc:Choice Requires='r'/><mc:Fallback/><mc:Choice Requires='r'/></mc:AlternateContent>",
      "><mc:AlternateContent bad='x'><mc:Choice Requires='r'/></mc:AlternateContent>",
      "><mc:AlternateContent xml:lang='en'><mc:Choice Requires='r'/></mc:AlternateContent>",
      "><mc:AlternateContent><mc:Choice Requires='r' e:attribute='x'/></mc:AlternateContent>",
      "><mc:AlternateContent><mc:Choice Requires='r' mc:MustUnderstand='e'/></mc:AlternateContent>",
      "mc:Ignorable='e' mc:ProcessContent='e:*'><e:box xml:base='x'/>",
      "mc:Ignorable='e' mc:ProcessContent='e:*'><e:box xml:lang='en'/>",
      "mc:Ignorable='e' mc:ProcessContent='e:*'><e:box xml:space='preserve'/>",
      "mc:Ignorable='e' mc:ProcessContent='e:*'><mc:AlternateContent><e:box/>"
        "<mc:Choice Requires='r'/></mc:AlternateContent>"}) {
    SCOPED_TRACE(body);
    std::istringstream input{relationship_archive(std::string{beginning} + body + "</Relationships>")};
    expect_error(package::error_code::invalid_metadata, [&] { package::opc_reader reader{input}; });
  }
  const auto bounded = relationship_archive(std::string{beginning} +
      "mc:Ignorable='e' mc:ProcessContent='e:*'><e:box><Relationship Id='id' Type='urn:test:main' "
      "Target='model.bin'/></e:box></Relationships>");
  auto measured = std::make_shared<package::budget>();
  std::istringstream measure_input{bounded};
  package::opc_reader measured_reader{measure_input, measured};
  const auto work = measured->configured().max_work_units - measured->remaining(package::resource::work_units);
  const auto allocation = measured->configured().max_decoder_allocation_bytes -
      measured->remaining(package::resource::decoder_allocation_bytes);
  ASSERT_GT(work, 0U);
  ASSERT_GT(allocation, 0U);
  for (const auto resource : {package::resource::work_units, package::resource::decoder_allocation_bytes}) {
    package::limits policy{};
    if (resource == package::resource::work_units) { policy.max_work_units = work - 1U; }
    else { policy.max_decoder_allocation_bytes = allocation - 1U; }
    std::istringstream input{bounded};
    expect_error(package::error_code::resource_limit, [&] {
      package::opc_reader reader{input, std::make_shared<package::budget>(policy)};
    });
    const auto ordinary = relationship_archive(std::string{beginning} +
        "><Relationship Id='id' Type='urn:test:main' Target='model.bin'/></Relationships>");
    std::istringstream ordinary_input{ordinary};
    EXPECT_NO_THROW((package::opc_reader{ordinary_input, std::make_shared<package::budget>(policy)}));
  }
}

// Physical directories/non-parts do not require MIME roles or become preserved logical payloads.
TEST(package_opc, maps_only_regular_supported_parts_and_keeps_zip_entry_layout) {
  if (!rohit::serializer::xml::available()) { GTEST_SKIP(); }
  // Measure the previous public metadata prefix against the additive fields on the actual build ABI.
  struct prior_entry_layout {
    std::string name{};
    package::zip_method method{};
    std::uint64_t compressed_bytes{}, expanded_bytes{};
    std::uint32_t crc32{};
    std::uint16_t flags{};
    std::uint64_t header_offset_bytes{}, data_offset_bytes{};
    bool zip64_sizes{};
  };
  EXPECT_EQ(sizeof(package::zip_entry), sizeof(prior_entry_layout));
  RecordProperty("zip_entry_bytes", static_cast<int>(sizeof(package::zip_entry)));
  RecordProperty("prior_entry_bytes", static_cast<int>(sizeof(prior_entry_layout)));
  constexpr std::string_view manifest =
      "<Types xmlns='http://schemas.openxmlformats.org/package/2006/content-types'>"
      "<Default Extension='bin' ContentType='application/octet-stream'/>"
      "<Default Extension='rels' ContentType='application/vnd.openxmlformats-package.relationships+xml'/></Types>";
  constexpr std::string_view relationships =
      "<Relationships xmlns='http://schemas.openxmlformats.org/package/2006/relationships'>"
      "<Relationship Id='main' Type='urn:test:main' Target='data/model.bin'/></Relationships>";
  std::ostringstream output;
  package::zip_writer archive{output};
  archive.add("[Content_Types].xml", as_bytes(manifest));
  archive.add("_rels/.rels", as_bytes(relationships));
  archive.add("data/", {});
  archive.add("data/model.bin", as_bytes("model"));
  archive.add("folder.bin", {});
  archive.add("unix_directory.bin", {});
  archive.add("unix_link.bin", as_bytes("data/model.bin"));
  archive.add("mode0.bin", as_bytes("regular"));
  archive.add("[trash]/0000.dat", as_bytes("ignored"));
  archive.finish();
  auto encoded = physical_attributes(output.str(), "folder.bin", 0U, 0x10U);
  encoded = physical_attributes(std::move(encoded), "unix_directory.bin", 3U, 0040755U << 16U);
  encoded = physical_attributes(std::move(encoded), "unix_link.bin", 3U, 0120777U << 16U);
  encoded = physical_attributes(std::move(encoded), "mode0.bin", 3U, 0000600U << 16U);
  std::istringstream plain{encoded};
  package::zip_reader physical{plain};
  EXPECT_EQ(physical.find("unix_link.bin")->creator_system, 3U);
  EXPECT_EQ(as_text(physical.read("unix_link.bin")), "data/model.bin");
  std::istringstream input{encoded};
  package::opc_reader reader{input};
  ASSERT_EQ(reader.parts().size(), 2U);
  EXPECT_NE(reader.find("/data/model.bin"), nullptr);
  EXPECT_NE(reader.find("/mode0.bin"), nullptr);
  for (const auto name : {"/folder.bin", "/unix_directory.bin", "/unix_link.bin"}) {
    EXPECT_EQ(reader.find(name), nullptr);
  }
  for (const auto name : {"/data/", "/[trash]/0000.dat"}) {
    expect_error(package::error_code::invalid_name, [&] { (void)reader.find(name); });
  }
  package::extension_policy policy{};
  policy.parts = package::unknown_policy::preserve;
  policy.relationships = package::unknown_policy::preserve;
  package::profile_loader profile{reader, policy};
  const auto preserved = profile.finish();
  ASSERT_EQ(preserved.parts.size(), 2U);
  std::ostringstream rewritten;
  package::opc_writer writer{rewritten};
  package::write_preserved(writer, preserved);
  writer.finish();
  std::istringstream rewritten_input{rewritten.str()};
  package::zip_reader saved{rewritten_input};
  EXPECT_EQ(saved.find("[trash]/0000.dat"), nullptr);
  EXPECT_EQ(saved.find("data/"), nullptr);
  EXPECT_EQ(saved.find("unix_link.bin"), nullptr);
}

// Exclusion cannot bypass CRC/expansion limits, dangling references, metadata identity or Unicode policy.
TEST(package_opc, excluded_physical_items_are_integrity_checked_and_not_reference_targets) {
  if (!rohit::serializer::xml::available()) { GTEST_SKIP(); }
  const auto make = [](std::string_view name, std::string_view payload, std::string_view target = "model.bin",
      bool orphan_relationships = false) {
    constexpr std::string_view manifest =
        "<Types xmlns='http://schemas.openxmlformats.org/package/2006/content-types'>"
        "<Default Extension='bin' ContentType='application/octet-stream'/>"
        "<Default Extension='rels' ContentType='application/vnd.openxmlformats-package.relationships+xml'/></Types>";
    const auto relationships =
        std::string{"<Relationships xmlns='http://schemas.openxmlformats.org/package/2006/relationships'>"
                    "<Relationship Id='main' Type='urn:test:main' Target='"} +
        std::string{target} + "'/></Relationships>";
    std::ostringstream output;
    package::zip_writer archive{output};
    archive.add("[Content_Types].xml", as_bytes(manifest));
    archive.add("_rels/.rels", as_bytes(relationships));
    archive.add("model.bin", as_bytes("model"));
    archive.add(name, as_bytes(payload));
    if (orphan_relationships) {
      archive.add("_rels/folder.bin.rels", as_bytes(
          "<Relationships xmlns='http://schemas.openxmlformats.org/package/2006/relationships'/>"));
    }
    archive.finish();
    return output.str();
  };
  const auto valid = make("[trash]/discard.dat", "physical-payload");
  std::istringstream physical_input{valid};
  package::zip_reader physical{physical_input};
  const auto* junk = physical.find("[trash]/discard.dat");
  ASSERT_NE(junk, nullptr);
  auto corrupt = valid;
  ASSERT_LT(junk->data_offset_bytes, corrupt.size());
  corrupt[static_cast<std::size_t>(junk->data_offset_bytes)] ^= 1;
  std::istringstream corrupt_input{corrupt};
  expect_error(package::error_code::integrity, [&] { package::opc_reader reader{corrupt_input}; });
  auto measured = std::make_shared<package::budget>();
  std::istringstream measured_input{valid};
  package::opc_reader measured_reader{measured_input, measured};
  const auto used = measured->configured().max_total_expanded_bytes -
      measured->remaining(package::resource::expanded_bytes);
  ASSERT_GT(used, 0U);
  package::limits small{};
  small.max_total_expanded_bytes = used - 1U;
  std::istringstream bounded_input{valid};
  expect_error(package::error_code::resource_limit, [&] {
    package::opc_reader reader{bounded_input, std::make_shared<package::budget>(small)};
  });
  const auto directory = physical_attributes(make("folder.bin", {}, "folder.bin"), "folder.bin", 0U, 0x10U);
  std::istringstream dangling_input{directory};
  expect_error(package::error_code::missing_part, [&] { package::opc_reader reader{dangling_input}; });
  const auto orphan = physical_attributes(make("folder.bin", {}, "model.bin", true), "folder.bin", 0U, 0x10U);
  std::istringstream orphan_input{orphan};
  expect_error(package::error_code::missing_part, [&] { package::opc_reader reader{orphan_input}; });
  const auto bad_manifest = physical_attributes(valid, "[Content_Types].xml", 0U, 0x10U);
  std::istringstream manifest_input{bad_manifest};
  expect_error(package::error_code::missing_part, [&] { package::opc_reader reader{manifest_input}; });
  std::ostringstream override_output;
  package::zip_writer override_archive{override_output};
  override_archive.add("[Content_Types].xml", as_bytes(
      "<Types xmlns='http://schemas.openxmlformats.org/package/2006/content-types'>"
      "<Override PartName='/folder.bin' ContentType='application/octet-stream'/></Types>"));
  override_archive.add("folder.bin", {});
  override_archive.finish();
  const auto overridden_directory = physical_attributes(override_output.str(), "folder.bin", 0U, 0x10U);
  std::istringstream override_input{overridden_directory};
  expect_error(package::error_code::invalid_metadata, [&] { package::opc_reader reader{override_input}; });
  for (const auto name : {"%C3%A9.bin", "\xc3\xa9.bin"}) {
    std::istringstream unicode_input{make(name, "unsupported")};
    expect_error(package::error_code::unsupported_feature, [&] { package::opc_reader reader{unicode_input}; });
  }
}

// Reserved relationship media types cannot disguise payload parts; empty writers emit a valid stream.
TEST(package_opc, rejects_reserved_payload_media_types_and_empty_media_type_stream) {
  if (!rohit::serializer::xml::available()) { GTEST_SKIP(); }
  for (const auto type : {"application/vnd.openxmlformats-package.relationships+xml",
                         "application/vnd.openxmlformats-package.relationships+xml; charset=utf-8"}) {
    const auto manifest = std::string{
        "<Types xmlns='http://schemas.openxmlformats.org/package/2006/content-types'>"
        "<Default Extension='bin' ContentType='"} + type + "'/></Types>";
    std::ostringstream output;
    package::zip_writer archive{output};
    archive.add("[Content_Types].xml", as_bytes(manifest));
    archive.add("payload.bin", {});
    archive.finish();
    std::istringstream input{output.str()};
    expect_error(package::error_code::invalid_metadata, [&] { package::opc_reader reader{input}; });
  }
  std::ostringstream empty;
  package::zip_writer invalid{empty};
  invalid.add("[Content_Types].xml", as_bytes(
      "<Types xmlns='http://schemas.openxmlformats.org/package/2006/content-types'/>"));
  invalid.finish();
  std::istringstream invalid_input{empty.str()};
  expect_error(package::error_code::invalid_metadata, [&] { package::opc_reader reader{invalid_input}; });
  std::ostringstream valid;
  package::opc_writer writer{valid};
  writer.finish();
  std::istringstream valid_input{valid.str()};
  EXPECT_TRUE(package::opc_reader{valid_input}.parts().empty());
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
    if (name == std::string_view{"/dat%61.bin"}) {
      package::opc_reader reader{input};
      EXPECT_TRUE(reader.parts().empty());
    } else {
      expect_error(package::error_code::unsupported_feature, [&] { package::opc_reader reader{input}; });
    }
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

// Aggregate publication waits for typed callbacks, semantic checks and optional opaque integrity.
TEST(package_profile, aggregate_rollback_preservation_and_shared_callback_budget) {
  if (!rohit::serializer::xml::available()) { GTEST_SKIP(); }
  std::ostringstream encoded;
  package::opc_writer writer{encoded};
  writer.add_part("/renamed/model.json", "application/json", as_bytes("7"));
  writer.add_part("/extensions/other.xml", "application/xml",
                  as_bytes("<e:extra xmlns:e='urn:extension' e:flag='yes'/>"));
  writer.add_relationship({"/", "main", "urn:test:main", "renamed/model.json"});
  writer.add_relationship({"/renamed/model.json", "extension", "urn:test:optional",
                           "../extensions/other.xml"});
  writer.finish();
  package::aggregate_result<int> destination{42, {}};
  const auto decode = [](int& model, package::profile_loader& loader) {
    loader.read_related("/", "urn:test:main", "application/json",
        [&](std::span<const std::uint8_t> bytes, const package::part&, package::budget& shared) {
          shared.consume(package::resource::decoder_allocation_bytes, sizeof(int));
          shared.consume(package::resource::work_units, bytes.size());
          model = std::stoi(std::string{bytes.begin(), bytes.end()});
        });
  };
  std::istringstream rejected_source{encoded.str()};
  package::opc_reader rejected{rejected_source};
  expect_error(package::error_code::unsupported_feature, [&] {
    package::load_opc_into(rejected, destination, decode);
  });
  EXPECT_EQ(destination.value, 42);
  EXPECT_TRUE(destination.extensions.parts.empty());
  std::istringstream source{encoded.str()};
  package::opc_reader reader{source};
  const auto infrastructure_decoder_bytes = reader.shared_budget()->configured().max_decoder_allocation_bytes -
      reader.shared_budget()->remaining(package::resource::decoder_allocation_bytes);
  package::load_opc_into(reader, destination, decode,
      {.parts = package::unknown_policy::preserve,
       .relationships = package::unknown_policy::preserve});
  EXPECT_EQ(destination.value, 7);
  ASSERT_EQ(destination.extensions.parts.size(), 1U);
  ASSERT_EQ(destination.extensions.relationships.size(), 1U);
  const auto opaque = destination.extensions.parts.front().bytes;
  std::ostringstream edited;
  package::opc_writer saved{edited};
  saved.add_part("/renamed/model.json", "application/json", as_bytes("8"));
  saved.add_relationship({"/", "main", "urn:test:main", "renamed/model.json"});
  package::write_preserved(saved, destination.extensions);
  saved.finish();
  std::istringstream reopened_source{edited.str()};
  package::opc_reader reopened{reopened_source};
  EXPECT_EQ(reopened.read("/extensions/other.xml"), opaque);
  EXPECT_EQ(as_text(reopened.read("/renamed/model.json")), "8");
  EXPECT_EQ(reopened.relationships().size(), 2U);

  std::istringstream limited_source{encoded.str()};
  // Admit exactly the measured infrastructure parsing cost, then reject the callback allocation.
  package::opc_reader limited{limited_source, std::make_shared<package::budget>(
      package::limits{.max_decoder_allocation_bytes = infrastructure_decoder_bytes})};
  EXPECT_EQ(limited.shared_budget()->remaining(package::resource::decoder_allocation_bytes), 0U);
  expect_error(package::error_code::resource_limit, [&] {
    package::load_opc_into(limited, destination, decode,
        {.parts = package::unknown_policy::skip, .relationships = package::unknown_policy::skip});
  });
  EXPECT_EQ(destination.value, 7);
  EXPECT_EQ(destination.extensions.parts.front().bytes, opaque);
}

// Fixed roles may exchange names; every preserved link resolves once in its original source scope.
TEST(package_profile, simultaneous_part_relocations_preserve_scopes_selectors_and_external_targets) {
  if (!rohit::serializer::xml::available()) { GTEST_SKIP(); }
  package::preserved_extensions extensions;
  extensions.relocations = {{"/left/A.xml", "/right/B.xml"}, {"/right/B.xml", "/left/A.xml"}};
  constexpr std::string_view external_target = "https://example.invalid/left/A.xml?q=1#external";
  extensions.relationships = {
      {"/", "first", "urn:test:first", "left/A.xml?layout=1#first"},
      {"/", "second", "urn:test:second", "/RIGHT/b.XML#second"},
      {"/LEFT/a.XML", "asset", "urn:test:asset", "../assets/blob.bin?keep=%2f#asset"},
      {"/right/B.xml", "peer", "urn:test:peer", "../left/A.xml?swap=1#peer"},
      {"/left/A.xml", "self", "urn:test:self", "#self"},
      {"/left/A.xml", "external", "urn:test:external", std::string{external_target},
          package::target_mode::external},
      {"/assets/blob.bin", "owner", "urn:test:owner", "../left/A.xml#owner"}};
  std::ostringstream output;
  package::opc_writer writer{output};
  writer.add_part("/left/A.xml", "application/xml", as_bytes("<from_b/>"));
  writer.add_part("/right/B.xml", "application/xml", as_bytes("<from_a/>"));
  writer.add_part("/assets/blob.bin", "application/octet-stream", as_bytes("opaque"));
  package::write_preserved(writer, extensions);
  writer.finish();
  std::istringstream input{output.str()};
  package::opc_reader reader{input};
  ASSERT_EQ(reader.relationships().size(), extensions.relationships.size());
  // Inspect authored link spelling as well as the whole-part identity returned by resolution.
  const auto check = [&](std::string_view source, std::string_view type, std::string_view target,
      std::string_view resolved, package::target_mode mode = package::target_mode::internal) {
    const auto links = reader.find_relationships(source, type);
    ASSERT_EQ(links.size(), 1U);
    EXPECT_EQ(links.front()->source, source);
    EXPECT_EQ(links.front()->target, target);
    EXPECT_EQ(links.front()->mode, mode);
    if (mode == package::target_mode::internal) {
      EXPECT_EQ(package::resolve_target(links.front()->source, links.front()->target), resolved);
    }
  };
  check("/", "urn:test:first", "/right/B.xml?layout=1#first", "/right/B.xml");
  check("/", "urn:test:second", "/left/A.xml#second", "/left/A.xml");
  check("/right/B.xml", "urn:test:asset", "/assets/blob.bin?keep=%2f#asset", "/assets/blob.bin");
  check("/left/A.xml", "urn:test:peer", "/right/B.xml?swap=1#peer", "/right/B.xml");
  check("/right/B.xml", "urn:test:self", "/right/B.xml#self", "/right/B.xml");
  check("/right/B.xml", "urn:test:external", external_target, {}, package::target_mode::external);
  check("/assets/blob.bin", "urn:test:owner", "/right/B.xml#owner", "/right/B.xml");
  EXPECT_EQ(as_text(reader.read("/left/A.xml")), "<from_b/>");
  EXPECT_EQ(as_text(reader.read("/right/B.xml")), "<from_a/>");
  EXPECT_EQ(as_text(reader.read("/assets/blob.bin")), "opaque");
  // Writing the rebound links must leave the loaded preservation record reusable and unchanged.
  EXPECT_EQ(extensions.relationships[2].source, "/LEFT/a.XML");
  EXPECT_EQ(extensions.relationships[3].target, "../left/A.xml?swap=1#peer");
}

// Every unknown part/relationship policy combination retains only explicitly selected optional data.
TEST(package_profile, optional_and_required_extension_policy_matrix) {
  if (!rohit::serializer::xml::available()) {
    GTEST_SKIP();
  }
  std::ostringstream encoded;
  package::opc_writer writer{encoded};
  writer.add_part("/model", "application/json", as_bytes("7"));
  writer.add_part("/optional", "application/octet-stream", as_bytes("opaque"));
  writer.add_relationship({"/", "main", "urn:test:main", "model"});
  writer.add_relationship({"/model", "optional", "urn:test:optional", "optional"});
  writer.finish();
  // Decode the known part through the ordinary unpublished aggregate callback.
  const auto decode = [](int& value, package::profile_loader& loader) {
    loader.read_related("/", "urn:test:main", "application/json",
                        [&](auto bytes, const auto&, auto&) {
                          value = std::stoi(as_text({bytes.begin(), bytes.end()}));
                        });
  };
  constexpr std::array policies{package::unknown_policy::reject, package::unknown_policy::skip,
                                package::unknown_policy::preserve};
  for (const auto parts : policies) {
    for (const auto relationships : policies) {
      std::istringstream source{encoded.str()};
      package::opc_reader reader{source};
      package::aggregate_result<int> destination{42, {}};
      const package::extension_policy policy{.parts = parts, .relationships = relationships};
      if (parts == package::unknown_policy::reject ||
          relationships == package::unknown_policy::reject) {
        expect_error(package::error_code::unsupported_feature,
                     [&] { package::load_opc_into(reader, destination, decode, policy); });
        EXPECT_EQ(destination.value, 42);
        continue;
      }
      package::load_opc_into(reader, destination, decode, policy);
      EXPECT_EQ(destination.value, 7);
      EXPECT_EQ(destination.extensions.parts.size(),
                parts == package::unknown_policy::preserve ? 1U : 0U);
      EXPECT_EQ(destination.extensions.relationships.size(),
                relationships == package::unknown_policy::preserve ? 1U : 0U);
      for (const auto required_parts : {false, true}) {
        for (const auto required_links : {false, true}) {
          if (!required_parts && !required_links) {
            continue;
          }
          std::istringstream required_source{encoded.str()};
          package::opc_reader required{required_source};
          auto required_policy = policy;
          required_policy.is_required_part = [=](const auto&) { return required_parts; };
          required_policy.is_required_relationship = [=](const auto&) { return required_links; };
          expect_error(package::error_code::unsupported_feature, [&] {
            package::load_opc_into(required, destination, decode, required_policy);
          });
          EXPECT_EQ(destination.value, 7);
        }
      }
    }
  }
}

// Preserved links are revalidated after edits; missing sources/targets cannot replace a saved file.
TEST(package_profile, edited_preserved_links_reject_before_file_publication) {
  if (!rohit::serializer::xml::available()) {
    GTEST_SKIP();
  }
  temporary_directory directory;
  const auto destination = directory.path / "preserved.opc";
  constexpr std::string_view old_generation = "previous complete generation";
  const std::array invalid_links{
      package::relationship{"/", "vendor", "urn:vendor:metadata", "Missing/target.xml"},
      package::relationship{"/Missing/source.xml", "vendor", "urn:vendor:metadata", "../model"}};
  for (const auto& link : invalid_links) {
    {
      std::ofstream original{destination};
      original << old_generation;
    }
    package::preserved_extensions extensions;
    extensions.relationships.push_back(link);
    expect_error(package::error_code::missing_part, [&] {
      package::save_opc_file(destination, [&](auto& output) {
        output.add_part("/model", "application/json", as_bytes("8"));
        output.add_relationship({"/", "main", "urn:test:main", "model"});
        package::write_preserved(output, extensions);
      });
    });
    std::ifstream original{destination};
    const std::string retained{std::istreambuf_iterator<char>{original}, {}};
    EXPECT_EQ(retained, old_generation);
    EXPECT_EQ(std::distance(std::filesystem::directory_iterator{directory.path},
                            std::filesystem::directory_iterator{}),
              1);
  }
}

// A required unknown capability cannot be silently skipped/preserved, and failures retain paths.
TEST(package_profile, required_extensions_and_codec_diagnostics) {
  if (!rohit::serializer::xml::available()) {
    GTEST_SKIP();
  }
  std::ostringstream encoded;
  package::opc_writer writer{encoded};
  writer.add_part("/model", "application/json", as_bytes("7"));
  writer.add_relationship({"/", "main", "urn:test:main", "model"});
  writer.finish();
  for (const auto policy : {package::unknown_policy::skip, package::unknown_policy::preserve}) {
    std::istringstream source{encoded.str()};
    package::opc_reader reader{source};
    expect_error(package::error_code::unsupported_feature, [&] {
      static_cast<void>(package::load_opc<int>(reader, [](int&, package::profile_loader&) {},
          {.parts = policy, .relationships = policy,
           .is_required_part = [](const package::part&) { return true; }}));
    });
  }
  std::istringstream source{encoded.str()};
  package::opc_reader reader{source};
  package::profile_loader loader{reader};
  try {
    loader.read_related("/", "urn:test:main", "application/json",
        [](auto, const auto&, auto&) {
          throw package::error{package::error_code::invalid_reference, "Missing referenced item",
                               {}, 0, 0, 0, "items[2].target", "missing-id"};
        });
    FAIL() << "Expected reference failure";
  } catch (const package::error& failure) {
    EXPECT_EQ(failure.code, package::error_code::invalid_reference);
    EXPECT_EQ(failure.part, "/model");
    EXPECT_EQ(failure.field_path, "items[2].target");
    EXPECT_EQ(failure.reference, "missing-id");
    EXPECT_EQ(failure.relationship_id, "main");
  }
  expect_error(package::error_code::invalid_state, [&] { static_cast<void>(loader.finish()); });
  std::istringstream finish_source{encoded.str()};
  package::opc_reader finish_reader{finish_source};
  package::profile_loader finish_loader{finish_reader};
  expect_error(package::error_code::unsupported_feature,
      [&] { static_cast<void>(finish_loader.finish()); });
  expect_error(package::error_code::invalid_state,
      [&] { static_cast<void>(finish_loader.finish()); });
}

// Canonical output is equal for independently reordered parts and authored relationship sets.
TEST(package_profile, canonical_ordering_is_independent_of_input_order) {
  std::vector<package::opaque_part> parts{
      {{"z.bin", "application/octet-stream", 1}, {'z'}, {.force_zip64 = true}},
      {{"a.bin", "application/octet-stream", 1}, {'a'}, {}}};
  std::ostringstream zip_first;
  package::write_canonical_zip(zip_first, parts);
  std::reverse(parts.begin(), parts.end());
  std::ostringstream zip_second;
  package::write_canonical_zip(zip_second, parts);
  EXPECT_EQ(zip_first.str(), zip_second.str());
  std::istringstream zip_input{zip_first.str()};
  package::zip_reader archive{zip_input};
  EXPECT_EQ(archive.entries().front().name, "a.bin");
  archive.validate_all();
  if (!rohit::serializer::xml::available()) { return; }
  for (auto& item : parts) { item.metadata.name.insert(0, "/"); }
  std::vector<package::relationship> links{
      {"/", "z", "urn:test:attachment", "z.bin"},
      {"/", "a", "urn:test:attachment", "a.bin"}};
  std::ostringstream opc_first;
  package::write_canonical_opc(opc_first, parts, links);
  std::reverse(parts.begin(), parts.end());
  std::reverse(links.begin(), links.end());
  std::ostringstream opc_second;
  package::write_canonical_opc(opc_second, parts, links);
  EXPECT_EQ(opc_first.str(), opc_second.str());
}

// Typed key scopes permit forward references and distinguish payload types without model pointers.
TEST(package_profile, typed_reference_indexes_validate_forward_scoped_and_wrong_type_keys) {
  package::budget shared;
  package::reference_index<std::uint32_t> index{shared};
  index.require_target<int>("document", 7, {"/source.xml", "items[0].target"});
  index.add_target<int>("document", 7, {"/target.xml", "id"});
  index.add_target<double>("other-scope", 7, {"/other.xml", "id"});
  index.validate();
  expect_error(package::error_code::invalid_reference, [&] {
    index.add_target<double>("document", 7, {"/duplicate.xml", "id"});
  });
  package::reference_index<std::string> missing{shared};
  missing.require_target<int>("document", "absent", {"/source.xml", "items[1].target"});
  try {
    missing.validate();
    FAIL() << "Expected missing target";
  } catch (const package::error& failure) {
    EXPECT_EQ(failure.part, "/source.xml");
    EXPECT_EQ(failure.field_path, "items[1].target");
    EXPECT_EQ(failure.reference, "absent");
  }
  package::reference_index<std::string> wrong_type{shared};
  wrong_type.require_target<int>("document", "key", {"/source.xml", "target"});
  wrong_type.add_target<double>("document", "key");
  expect_error(package::error_code::invalid_reference, [&] { wrong_type.validate(); });
  package::budget bounded{package::limits{.max_references = 1}};
  package::reference_index<std::uint64_t> limited{bounded};
  limited.add_target<int>("", 1);
  expect_error(package::error_code::resource_limit, [&] { limited.require_target<int>("", 1); });
}

// Live canonical indexes reserve scratch that remains unavailable to nested compression calls.
TEST(package_profile, live_scratch_reservations_share_capacity_and_release_on_failure) {
  package::budget shared{package::limits{.max_buffer_bytes = 10}};
  EXPECT_EQ(shared.available_buffer_bytes(), 10U);
  {
    package::buffer_reservation retained{shared, 6};
    EXPECT_EQ(shared.available_buffer_bytes(), 4U);
    shared.check_buffer(4);
    expect_error(package::error_code::resource_limit, [&] { shared.check_buffer(5); });
  }
  EXPECT_EQ(shared.available_buffer_bytes(), 10U);
}

// New resource fields append after the legacy aggregate callback to preserve positional callers.
TEST(package_profile, legacy_limit_initializers_and_resource_ordinals_remain_compatible) {
  const package::limits configured{1, 2, 3, 4, 5, 6, 7, 8, [] { return true; }};
  EXPECT_EQ(configured.max_relationships, 8U);
  EXPECT_TRUE(configured.is_cancelled());
  EXPECT_EQ(configured.max_references, package::limits{}.max_references);
  static_assert(static_cast<int>(package::resource::relationships) == 4);
  static_assert(static_cast<int>(package::error_code::publication_uncertain) == 14);
  static_assert(!std::is_copy_constructible_v<package::zip_reader>);
  static_assert(!std::is_move_constructible_v<package::zip_reader>);
}

// A DEFLATE cancellation signal is observed inside provider work, before candidate publication.
TEST(package_zip, deflate_cancellation_checks_provider_batches) {
  if (!compression::available(compression::format::deflate)) { GTEST_SKIP(); }
  std::size_t polls{};
  const std::vector<std::uint8_t> payload(4 * package::mebibyte, 'x');
  auto shared = std::make_shared<package::budget>(package::limits{
      .is_cancelled = [&] { return ++polls > 70; }});
  std::ostringstream encoded;
  package::zip_writer writer{encoded, shared};
  expect_error(package::error_code::cancelled, [&] {
    writer.add("large", payload, {.method = package::zip_method::deflate});
  });
  EXPECT_TRUE(encoded.str().empty());
}
