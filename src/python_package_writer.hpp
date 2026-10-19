// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "package_writer.hpp"

#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::package_codegen {
// Emit a Python facade over the shared bounded runtime without reflecting over model fields.
inline std::string python(const package_node& package, const std::string& name,
    const std::function<std::string(const type_name&)>& render_type,
    const std::function<std::string(std::string_view)>& render_field,
    const std::function<std::string(std::string_view)>& literal) {
  const auto parts = ordered(package);
  std::string output;
  const auto line = [&](const std::string& text = {}) { output += text + '\n'; };
  std::set<std::string> fields{"extensions"};
  std::set<std::pair<std::string, std::string>> dispatch;
  std::string parameters;
  for (const auto* part : parts) {
    const auto field = render_field(part->name);
    if (!fields.insert(field).second || !dispatch.emplace(part->source, part->relationship).second) {
      throw std::invalid_argument{"Python package field or role dispatch collision: " + part->name};
    }
    if ((part->part_name || part->relationship_id) && (!part->max_occurs || *part->max_occurs > 1U)) {
      throw std::invalid_argument{"Repeated package roles require dynamic names and generated relationship IDs"};
    }
    parameters += ", " + field + (singleton(*part) ? "=None" : "=()");
  }
  line("class " + name + ":");
  line("    \"\"\"Generated complete OPC profile; callers supply typed values and binary resources.\"\"\"");
  line("    class Input:");
  line("        \"\"\"Borrow role values for synchronous encoding; extensions retain whole unknown parts.\"\"\"");
  line("        def __init__(self" + parameters + ", extensions=None):");
  line("            \"\"\"Bind resources without copying model or binary payloads.\"\"\"");
  for (const auto* part : parts) {
    const auto field = render_field(part->name);
    line("            self." + field + " = " + field);
  }
  line("            self.extensions = extensions");
  line("    class Value(Input):");
  line("        \"\"\"Own the fresh decoded role aggregate.\"\"\"");
  line("        pass");
  line("    _roles = (");
  for (const auto* part : parts) {
    const auto optional = [&](const auto& value) { return value ? literal(*value) : "None"; };
    line("        {'field': " + literal(render_field(part->name)) + ", 'role': " + literal(part->name) +
        ", 'model': " + (part->value_type ? render_type(*part->value_type) : "None") +
        ", 'singleton': " + (singleton(*part) ? "True" : "False") +
        ", 'name': " + optional(part->part_name) + ", 'mime': " + optional(part->content_type) +
        ", 'source': " + literal(part->source == "/" ? "/" : render_field(part->source)) +
        ", 'relationship': " + literal(part->relationship) + ", 'id': " + optional(part->relationship_id) +
        ", 'compression': " + literal(part->compression) + ", 'minimum': " + std::to_string(part->min_occurs) +
        ", 'maximum': " + (part->max_occurs ? std::to_string(*part->max_occurs) : "None") + "},");
  }
  line("    )");
  line("    @classmethod");
  line("    def encode(cls, value, limits=None, budget=None):");
  line("        \"\"\"Return a complete archive after role, URI and relationship validation.\"\"\"");
  line("        return _pkg_profile_encode(value, cls._roles, _pkg_budget(limits, budget))");
  line("    @classmethod");
  line("    def write(cls, destination, value, limits=None, budget=None):");
  line("        \"\"\"Prepare a complete archive before borrowing a sink; sink failure may retain a prefix.\"\"\"");
  line("        shared = _pkg_budget(limits, budget)");
  line("        _pkg_write_stream(destination, _pkg_profile_encode(value, cls._roles, shared), shared)");
  line("    @classmethod");
  line("    def load(cls, source, limits=None, policy=None, validate=None, budget=None):");
  line("        \"\"\"Return only a fully checked fresh aggregate; external targets are never opened.\"\"\"");
  line("        shared = _pkg_budget(limits, budget)");
  line("        if isinstance(source, (str, _pkg_os.PathLike)):");
  line("            with open(source, 'rb') as stream:");
  line("                return _pkg_profile_load(stream, cls._roles, cls.Value, shared, policy, validate)");
  line("        return _pkg_profile_load(source, cls._roles, cls.Value, shared, policy, validate)");
  line("    @classmethod");
  line("    def save(cls, path, value, limits=None, validate=None, budget=None, fault=None):");
  line("        \"\"\"Validate staged bytes and atomically replace the final file after file synchronization.\"\"\"");
  line("        shared = _pkg_budget(limits, budget)");
  line("        data = _pkg_profile_encode(value, cls._roles, shared)");
  line("        _pkg_profile_load(data, cls._roles, cls.Value, shared, ExtensionPolicy('preserve', 'preserve'), validate)");
  line("        _pkg_save(path, data, shared, fault)");
  return output;
}
} // namespace rohit::serializer::writer::package_codegen
