// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/package.hpp>
#include <gtest/gtest.h>

#include <array>
#include <sstream>
#include <type_traits>

namespace {
namespace package = rohit::serializer::package;
constexpr package::entry_options default_options{};
static_assert(default_options.metadata == nullptr);
static_assert(std::is_trivially_copyable_v<package::entry_options>);

// Explicit replay owns a copy while keeping ordinary options a literal/trivially copyable type.
TEST(zip_metadata, physical_metadata_replays_after_source_and_option_lifetimes) {
  std::ostringstream output{std::ios::binary};
  package::zip_writer writer{output};
  package::zip_entry_metadata metadata{};
  metadata.version_made = 0x0314U;
  metadata.flags = 8U;
  metadata.central_time = metadata.local_time = 0x1234U;
  metadata.central_date = metadata.local_date = 0x5678U;
  metadata.external_attributes = 0x81a40000U;
  metadata.internal_attributes = 1U;
  metadata.comment = "entry comment";
  metadata.central_extra = {0xfeU, 0xcaU, 2U, 0U, 1U, 2U};
  metadata.local_extra = {0xfdU, 0xcaU, 1U, 0U, 3U};
  metadata.descriptor_signed = metadata.local_crc_zero = true;
  metadata.local_compressed_zero = metadata.local_expanded_zero = true;
  const auto expected = metadata;
  package::entry_options options{};
  options.metadata = &metadata;
  const std::array<std::uint8_t, 3U> payload{1U, 2U, 3U};
  writer.add("part.bin", payload, options);
  metadata.comment = "changed after copy";
  auto archive = std::make_shared<package::zip_archive_metadata>();
  archive->comment = "archive comment";
  archive->force_zip64 = true;
  archive->zip64_extensible_data = {7U, 8U, 9U};
  writer.set_archive_metadata(archive);
  writer.finish();
  std::istringstream input{output.str(), std::ios::binary};
  package::zip_reader reader{input};
  ASSERT_EQ(reader.entries().size(), 1U);
  ASSERT_TRUE(reader.entry_metadata("part.bin"));
  EXPECT_EQ(*reader.entry_metadata("part.bin"), expected);
  ASSERT_TRUE(reader.archive_metadata());
  EXPECT_EQ(*reader.archive_metadata(), *archive);
  EXPECT_EQ(reader.read("part.bin"), (std::vector<std::uint8_t>{1U, 2U, 3U}));
}

// Invalid supplied TLVs and descriptor state fail before publishing a local header.
TEST(zip_metadata, invalid_metadata_does_not_publish_an_entry) {
  std::ostringstream output{std::ios::binary};
  package::zip_writer writer{output};
  package::zip_entry_metadata metadata{};
  metadata.central_extra = {2U, 0U, 7U, 0U};
  package::entry_options options{};
  options.metadata = &metadata;
  EXPECT_THROW(writer.add("part.bin", {}, options), package::error);
  EXPECT_TRUE(output.str().empty());
  metadata.central_extra.clear();
  metadata.descriptor_signed = true;
  EXPECT_THROW(writer.add("part.bin", {}, options), package::error);
  EXPECT_TRUE(output.str().empty());
}

// Metadata allocation and cancellation use the caller's cumulative operation context.
TEST(zip_metadata, supplied_metadata_obeys_shared_admission_and_cancellation) {
  package::limits limits{};
  limits.max_metadata_bytes = sizeof(package::zip_entry) + 8U;
  auto shared = std::make_shared<package::budget>(limits);
  std::ostringstream output{std::ios::binary};
  package::zip_writer writer{output, shared};
  package::zip_entry_metadata metadata{};
  package::entry_options options{};
  options.metadata = &metadata;
  EXPECT_THROW(writer.add("part.bin", {}, options), package::error);
  EXPECT_TRUE(output.str().empty());
}
}
