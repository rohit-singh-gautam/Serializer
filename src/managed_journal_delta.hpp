// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <span>
#include <vector>

namespace rohit::managed::detail {

// Own the immutable saved bytes used by independently recoverable sidecar patches.
// Concurrent encode/decode calls only read this dictionary; callers own their returned buffers.
class journal_delta_base {
  struct implementation;
  std::unique_ptr<implementation> state_;

public:
  // Index an owning baseline once; allocation failures leave its caller's live journal untouched.
  explicit journal_delta_base(std::vector<std::uint8_t> baseline);
  // Release the dictionary and its bounded-probe matching index.
  ~journal_delta_base();
  // Keep dictionary ownership stable while journal generations exchange owning pointers.
  journal_delta_base(const journal_delta_base&) = delete;
  // Never accidentally copy a potentially large matching index.
  journal_delta_base& operator=(const journal_delta_base&) = delete;
  // Expose the saved bytes for the lifetime of this immutable dictionary.
  const std::vector<std::uint8_t>& bytes() const noexcept;
  // Encode copy/literal instructions with both expanded and encoded lengths bounded by the limit.
  std::vector<std::uint8_t>
  encode(std::initializer_list<std::span<const std::uint8_t>> parts,
         std::size_t max_record_bytes) const;
  // Reject foreign dictionaries, malformed references, trailing bytes, and expansion over the limit.
  std::vector<std::uint8_t> decode(std::span<const std::uint8_t> patch,
                                   std::size_t max_record_bytes,
                                   std::size_t max_expanded_bytes =
                                       std::numeric_limits<std::size_t>::max()) const;
};

} // namespace rohit::managed::detail
