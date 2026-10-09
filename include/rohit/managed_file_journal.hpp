// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/managed_journal.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace rohit::managed::detail {

// Bind the journal sink to an exclusive durable file stream and publish a new baseline.
// Existing documents are rejected; indeterminate is set even if allocating an error fails.
std::unique_ptr<journal_sink> create_file_journal(const std::filesystem::path& path,
                                                  journal_storage_mode mode,
                                                  const std::vector<std::uint8_t>& envelope,
                                                  journal_options options, bool& indeterminate);

// Replay validated borrowed slices under a stable writer lock; delay tail repair until validation.
std::unique_ptr<journal_sink>
recover_file_journal(const std::filesystem::path& path, journal_options options,
                     const std::function<void(bool, std::span<const std::uint8_t>)>& consume);

} // namespace rohit::managed::detail
