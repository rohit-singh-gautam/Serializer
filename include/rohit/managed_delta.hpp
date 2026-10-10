// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include "digest.hpp"
#include "managed_records.hpp"
#include "serializer.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rohit::managed {

// Construct a bounded prefix/suffix patch over canonical serialized bytes, not native object layout.
inline records::snapshot_patch make_snapshot_patch(std::span<const std::uint8_t> base,
                                                    std::span<const std::uint8_t> target,
                                                    std::size_t max_bytes) {
  if (base.size() > max_bytes || target.size() > max_bytes) {
    throw std::length_error{"Managed delta snapshot budget exceeded"};
  }
  const auto common = std::min(base.size(), target.size());
  std::size_t prefix = 0;
  while (prefix < common && base[prefix] == target[prefix]) {
    ++prefix;
  }
  std::size_t suffix = 0;
  while (suffix < common - prefix && base[base.size() - suffix - 1] ==
                                       target[target.size() - suffix - 1]) {
    ++suffix;
  }
  records::snapshot_patch result;
  result.format_version = 1;
  result.base_bytes = base.size();
  result.target_bytes = target.size();
  result.prefix_bytes = prefix;
  result.suffix_bytes = suffix;
  const auto base_digest = rohit::make_digest<rohit::digest_algorithm::sha256>(base);
  const auto target_digest = rohit::make_digest<rohit::digest_algorithm::sha256>(target);
  result.base_digest.assign(base_digest.begin(), base_digest.end());
  result.target_digest.assign(target_digest.begin(), target_digest.end());
  result.replacement.assign(target.begin() + static_cast<std::ptrdiff_t>(prefix),
                            target.end() - static_cast<std::ptrdiff_t>(suffix));
  return result;
}

// Reconstruct only from the exact named base; malformed extents and hashes fail before publication.
inline std::vector<std::uint8_t> apply_snapshot_patch(std::span<const std::uint8_t> base,
                                                     const records::snapshot_patch& patch,
                                                     std::size_t max_bytes) {
  constexpr auto digest_bytes = rohit::digest_size(rohit::digest_algorithm::sha256);
  if (patch.format_version != 1 || patch.base_bytes != base.size() ||
      base.size() > max_bytes || patch.target_bytes > max_bytes ||
      patch.base_digest.size() != digest_bytes || patch.target_digest.size() != digest_bytes ||
      patch.prefix_bytes > base.size() || patch.suffix_bytes > base.size() - patch.prefix_bytes ||
      patch.prefix_bytes > patch.target_bytes ||
      patch.suffix_bytes > patch.target_bytes - patch.prefix_bytes ||
      patch.replacement.size() != patch.target_bytes - patch.prefix_bytes - patch.suffix_bytes) {
    throw std::invalid_argument{"Invalid managed snapshot patch"};
  }
  const auto base_digest = rohit::make_digest<rohit::digest_algorithm::sha256>(base);
  if (!std::equal(base_digest.begin(), base_digest.end(), patch.base_digest.begin())) {
    throw std::invalid_argument{"Managed snapshot patch has a stale base"};
  }
  std::vector<std::uint8_t> result;
  result.reserve(static_cast<std::size_t>(patch.target_bytes));
  result.insert(result.end(), base.begin(),
                base.begin() + static_cast<std::ptrdiff_t>(patch.prefix_bytes));
  result.insert(result.end(), patch.replacement.begin(), patch.replacement.end());
  result.insert(result.end(), base.end() - static_cast<std::ptrdiff_t>(patch.suffix_bytes), base.end());
  const auto target_digest = rohit::make_digest<rohit::digest_algorithm::sha256>(std::span{result});
  if (!std::equal(target_digest.begin(), target_digest.end(), patch.target_digest.begin())) {
    throw std::invalid_argument{"Managed snapshot patch target checksum mismatch"};
  }
  return result;
}

// A bounded in-memory patch chain retains its checkpoint and never prunes required ancestors.
class snapshot_delta_chain {
  std::vector<std::uint8_t> checkpoint_{};
  std::vector<records::snapshot_patch> patches_{};
  std::size_t max_bytes_{};
  std::size_t max_deltas_{};

public:
  // Establish an independent checkpoint; zero depth disables delta chaining without losing snapshots.
  snapshot_delta_chain(std::vector<std::uint8_t> checkpoint, std::size_t max_bytes,
                       std::size_t max_deltas)
      : checkpoint_{std::move(checkpoint)}, max_bytes_{max_bytes}, max_deltas_{max_deltas} {
    if (checkpoint_.size() > max_bytes_) {
      throw std::length_error{"Managed delta checkpoint budget exceeded"};
    }
  }

  // Reconstruct the retained tip with bounded replay work; hash checks validate every dependency.
  std::vector<std::uint8_t> current() const {
    auto result = checkpoint_;
    for (const auto& patch : patches_) {
      result = apply_snapshot_patch(result, patch, max_bytes_);
    }
    return result;
  }

  // Append atomically or replace with a full checkpoint at the configured maximum replay depth.
  void append(std::span<const std::uint8_t> target) {
    auto patch = make_snapshot_patch(current(), target, max_bytes_);
    if (patches_.size() >= max_deltas_ || patch.replacement.size() >= target.size()) {
      std::vector<std::uint8_t> checkpoint{target.begin(), target.end()};
      checkpoint_.swap(checkpoint);
      patches_.clear();
    } else {
      patches_.push_back(std::move(patch));
    }
  }

  // Return replay depth for budgeting and observability, independently of managed revision IDs.
  std::size_t delta_count() const noexcept { return patches_.size(); }
};
} // namespace rohit::managed
