// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once
#include <cstdint>

namespace rohit::managed {
// Generated persistent IDs and reference object IDs share this native binary policy.
enum class identity_encoding { fixed, compact_varint };

// Stable document namespace, separate from dense document-local object IDs.
struct document_id {
  std::uint64_t high{};
  std::uint64_t low{};
  // Compare the complete document namespace without resolving any entity or pointer.
  bool operator==(const document_id&) const = default;
};
} // namespace rohit::managed
