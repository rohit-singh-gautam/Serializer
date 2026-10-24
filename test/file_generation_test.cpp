// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/file_stream.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {
// Isolate native publication fixtures without depending on generated models.
class generation_directory {
  std::filesystem::path path_;
public:
  // Refuse an existing directory instead of acquiring ownership of user files.
  generation_directory() : path_{std::filesystem::temp_directory_path() /
      ("serializer-generation-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))} {
    if (!std::filesystem::create_directory(path_)) { throw std::runtime_error{"Cannot create generation fixture"}; }
  }
  // Remove only files inside this newly created private fixture.
  ~generation_directory() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
  // Select one ordinary child of the private fixture directory.
  auto file(const char* name) const { return path_ / name; }
};

// Read one fixture generation from its already retained native handle.
std::string read(rohit::file_stream& stream) {
  stream.seek(0);
  std::array<char, 3> bytes{};
  stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  EXPECT_EQ(stream.gcount(), static_cast<std::streamsize>(bytes.size()));
  return {bytes.begin(), bytes.end()};
}
} // namespace

// Read pins keep their old file object while the name selects the complete new generation.
TEST(file_generation, replacement_preserves_old_readers_and_open_staging_writer) {
  generation_directory directory;
  const auto target = directory.file("current");
  const auto staged = directory.file("staged");
  {
    rohit::file_stream original{target, rohit::file_open_mode::create};
    original.write("old", 3);
    original.sync();
  }
  rohit::file_stream old_reader{target, rohit::file_open_mode::read};
  rohit::file_stream replacement{staged, rohit::file_open_mode::create};
  replacement.write("new", 3);
  replacement.sync();
  rohit::replace_file_generation(staged, target);
  rohit::sync_parent_directory(target);
  rohit::file_stream current{target, rohit::file_open_mode::read_shared};
  EXPECT_EQ(read(current), "new");
  EXPECT_EQ(read(old_reader), "old");
  EXPECT_EQ(read(replacement), "new");
  EXPECT_FALSE(std::filesystem::exists(staged));
}

// An active writer remains on its old inode; migration may transfer a preopened replacement handle.
TEST(file_generation, replacement_preserves_predecessor_writer_until_transfer) {
  generation_directory directory;
  const auto target = directory.file("current");
  const auto staged = directory.file("staged");
  rohit::file_stream predecessor{target, rohit::file_open_mode::create};
  predecessor.write("old", 3);
  predecessor.sync();
  rohit::file_stream replacement{staged, rohit::file_open_mode::create};
  replacement.write("new", 3);
  replacement.sync();
  rohit::replace_file_generation(staged, target);
  rohit::sync_parent_directory(target);
  EXPECT_EQ(read(predecessor), "old");
  EXPECT_EQ(read(replacement), "new");
  rohit::file_stream current{target, rohit::file_open_mode::read_shared};
  EXPECT_EQ(read(current), "new");
}

// Vary UTF-16 name lengths so native parsing cannot depend on allocator padding after the name.
TEST(file_generation, repeated_unicode_names_select_exact_replacement_paths) {
  generation_directory directory;
  constexpr std::size_t name_variants = 32;
  for (std::size_t count = 1; count <= name_variants; ++count) {
    auto target = directory.file("current-");
    target += std::wstring(count, L'\u03a9');
    auto staged = target;
    staged += ".staged";
    {
      rohit::file_stream original{target, rohit::file_open_mode::create};
      original.write("old", 3);
      original.sync();
    }
    rohit::file_stream old_reader{target, rohit::file_open_mode::read};
    rohit::file_stream replacement{staged, rohit::file_open_mode::create};
    replacement.write("new", 3);
    replacement.sync();
    rohit::replace_file_generation(staged, target);
    rohit::file_stream current{target, rohit::file_open_mode::read_shared};
    EXPECT_EQ(read(current), "new") << count;
    EXPECT_EQ(read(old_reader), "old") << count;
    EXPECT_FALSE(std::filesystem::exists(staged)) << count;
  }
}
