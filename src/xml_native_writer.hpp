// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include <rohit/output_options.hpp>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_codegen {
// Append independent generated XML APIs for native C, Swift and Kotlin owning models.
std::string generate_native(const native::schema& model, const xml_options& options, std::string_view language);
} // namespace rohit::serializer::writer::xml_codegen
