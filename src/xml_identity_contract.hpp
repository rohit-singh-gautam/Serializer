// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/xml_identity.hpp>

#include <string_view>

namespace rohit::serializer::writer::xml_binding {
struct schema_bundle;

// Compile one global element's supported key/unique/keyref declarations and scalar field types.
// All names and paths are copied; the returned contract does not borrow the bundle's lifetime.
[[nodiscard]] xml::identity_scope bind_identity_constraints(const schema_bundle& bundle,
    const xml::element& declaration, std::string_view target_namespace,
    const xml::identity_limits& limits = {});
} // namespace rohit::serializer::writer::xml_binding
