// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace journal_test_paths {
// Derive the documented new layout independently of the file adapter's implementation.
inline std::filesystem::path directory(const std::filesystem::path& document) {
  std::filesystem::path name{"."};
  name += document.filename().native();
  name += ".serializer";
  return document.parent_path() / name;
}

// Locate exact selected generation bytes in either supported filesystem layout.
inline std::filesystem::path companion(const std::filesystem::path& document,
    std::uint64_t high, std::uint64_t low) {
  const auto folder = directory(document);
  const auto generation = std::to_string(high) + "-" + std::to_string(low);
  if (std::filesystem::is_directory(folder)) { return folder / "journal" / ("changes-" + generation); }
  auto old = document.parent_path() / ("." + document.filename().string() + ".journal");
  if (std::filesystem::is_directory(old)) { return old / ("changes-" + generation); }
  auto result = document;
  result += ".journal-" + generation;
  return result;
}

// Select the stable or pending external-document manifest without changing any file.
inline std::filesystem::path manifest(const std::filesystem::path& document, bool pending = false) {
  const auto folder = directory(document);
  if (std::filesystem::is_directory(folder)) { return folder / "state" / (pending ? "manifest.pending" : "manifest"); }
  auto result = document;
  result += pending ? ".journal.pending" : ".journal";
  return result;
}

// Fixtures calling this helper have one live generation after successful cleanup.
inline std::filesystem::path first_companion(const std::filesystem::path& document) {
  const auto folder = directory(document);
  const bool grouped = std::filesystem::is_directory(folder);
  const auto prefix = grouped ? std::string{"changes-"} : document.filename().string() + ".journal-";
  for (const auto& item : std::filesystem::directory_iterator{grouped ? folder / "journal" : document.parent_path()}) {
    if (item.path().filename().string().starts_with(prefix)) { return item.path(); }
  }
  throw std::runtime_error{"Missing test journal companion"};
}
} // namespace journal_test_paths
