// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <cstdint>
#include <stdexcept>

namespace rohit::managed {

// Explicit version-one C++ configuration vocabulary; these values are not a saved/wire format.
enum class managed_capability : std::uint32_t {
  none = 0, history = 1, collaboration = 2, authorization = 4, journal = 8
};

// Combine independently selected mechanisms without changing identity/ownership requirements.
constexpr managed_capability operator|(managed_capability left, managed_capability right) noexcept {
  return static_cast<managed_capability>(static_cast<std::uint32_t>(left) |
                                          static_cast<std::uint32_t>(right));
}

struct managed_capability_profile {
  static constexpr std::uint32_t current_version = 1;
  static constexpr managed_capability version_one_all = managed_capability::history |
      managed_capability::collaboration | managed_capability::authorization | managed_capability::journal;
  std::uint32_t version{current_version};
  managed_capability selected{managed_capability::none};
  managed_capability excluded{managed_capability::none};

  // Resolve explicitly versioned selections; exclusions must name selected features and unknown bits reject.
  managed_capability resolve() const {
    const auto selected_bits = static_cast<std::uint32_t>(selected);
    const auto excluded_bits = static_cast<std::uint32_t>(excluded);
    const auto available_bits = static_cast<std::uint32_t>(version_one_all);
    if (version != current_version || (selected_bits & ~available_bits) != 0 ||
        (excluded_bits & ~selected_bits) != 0) {
      throw std::invalid_argument{"Unsupported managed capability profile or exclusions"};
    }
    return static_cast<managed_capability>(selected_bits & ~excluded_bits);
  }
};

// Validate a resolved profile against the actual C++ engine specialization and installed policy.
// This does not attach a journal/transport, convert history, add schema selectors or relax identity.
template <typename Store>
void validate_managed_capability_profile(const Store& store, managed_capability_profile profile,
                                         bool authorization_required = false) {
  const auto selected = static_cast<std::uint32_t>(profile.resolve());
  const auto has = [selected](managed_capability capability) {
    return (selected & static_cast<std::uint32_t>(capability)) != 0;
  };
  if (has(managed_capability::history) != Store::has_history ||
      (has(managed_capability::journal) && !Store::has_journal) ||
      (has(managed_capability::collaboration) && !Store::has_collaboration) ||
      (has(managed_capability::authorization) && !store.has_authorization_policy()) ||
      (authorization_required && !has(managed_capability::authorization))) {
    throw std::invalid_argument{"Managed capability profile does not match the configured engine/policy"};
  }
}
} // namespace rohit::managed
