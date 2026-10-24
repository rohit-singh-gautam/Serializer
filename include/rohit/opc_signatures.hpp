// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/package.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::package::signatures {

// Explicit signature coverage; relationship sources use "/" for the package root.
struct coverage {
  std::vector<std::string> parts{};
  std::vector<std::string> relationship_sources{};
};

// A successful record verifies only this coverage with the caller's pinned signing certificate.
struct verified_signature {
  std::string signature_part{};
  coverage signed_content{};
};

// Signing returns an independently owning package and the selected public certificate, never its key.
struct signed_package {
  std::vector<std::uint8_t> bytes{};
  std::vector<std::uint8_t> certificate_der{};
};

// Report whether the Windows Packaging API provider was compiled into this runtime.
[[nodiscard]] bool available() noexcept;

// Verify every signature with an explicitly pinned DER certificate and require coverage per signature.
// Checks certificate validity now; does not use embedded certificates as trust anchors, consult system
// trust stores, fetch external resources, validate timestamps, or retain inputs. Invalid, unsigned,
// untrusted, unsupported or incomplete packages throw package::error. Provider allocations are not
// routed through Serializer's allocation hooks; limits bound input/output and preflight ZIP expansion.
[[nodiscard]] std::vector<verified_signature> verify(std::span<const std::uint8_t> package_bytes,
    std::span<const std::uint8_t> trusted_certificate_der, const coverage& required,
    limits configured = {});

// Sign selected parts and complete relationship parts with RSA-SHA256, SHA256 and inclusive C14N.
// Imports one private-key certificate from an in-memory PFX without persisting keys. Rejects already
// signed inputs and empty part selection. Borrows all inputs synchronously; returns no partial output.
[[nodiscard]] signed_package sign(std::span<const std::uint8_t> package_bytes,
    std::span<const std::uint8_t> certificate_pfx, std::wstring_view password,
    const coverage& selected, limits configured = {});

// Explicitly remove signatures before editing; never represents retained signature bytes as valid.
// Does not authenticate the input. The returned owning package must be verified anew after signing.
[[nodiscard]] std::vector<std::uint8_t> remove(std::span<const std::uint8_t> package_bytes,
    limits configured = {});
} // namespace rohit::serializer::package::signatures
