// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "xml_schema.hpp"
#include <functional>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_portable {
using field_name = std::function<std::string(const member&)>;
using type_name_fn = std::function<std::string(const type_name&)>;
// Return package-level Go imports before any declarations.
std::string imports(std::string_view language);
// Return the selected backend's bounded, namespace-aware data XML runtime.
std::string runtime(std::string_view language);
// Emit statically bound owning-record methods at the backend's class/package scope.
std::string methods(const class_node& object, const xml_binding::record& contract,
    const std::string& identifier, const field_name& field, const type_name_fn& type,
    const xml_options& options, std::string_view language);
} // namespace rohit::serializer::writer::xml_portable
