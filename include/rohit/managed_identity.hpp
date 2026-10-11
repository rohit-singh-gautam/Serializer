// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once
#include <cstdint>

namespace rohit::managed {
// Stable document namespace, separate from dense document-local object IDs.
struct document_id {
  std::uint64_t high{};
  std::uint64_t low{};
};
} // namespace rohit::managed
