// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/serializer_creator.hpp>
#include "document_profile_contract.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer::package_codegen {

// Detect package use at generation time; ordinary models need no package include or support.
inline bool contains(const std::vector<std::unique_ptr<syntax_node>>& statements) {
  return std::any_of(statements.begin(), statements.end(), [](const auto& node) {
    return node->type == object_type::package_type ||
        (node->type == object_type::namespace_type &&
            contains(static_cast<const namespace_node&>(*node).statements));
  });
}

// Escape one schema-authored URI/string without permitting generated source injection.
inline std::string quote(std::string_view value) {
  std::string result{"\""};
  for (const auto character : value) {
    const auto byte = static_cast<unsigned char>(character);
    if (byte == '"' || byte == '\\') {
      result += '\\';
      result += character;
    } else if (byte < 0x20U || byte >= 0x7fU) {
      result += '\\';
      result += static_cast<char>('0' + ((byte >> 6U) & 7U));
      result += static_cast<char>('0' + ((byte >> 3U) & 7U));
      result += static_cast<char>('0' + (byte & 7U));
    } else {
      result += character;
    }
  }
  return result + '"';
}

// Order declarative roles by their source roles and reject an undiscoverable dependency cycle.
inline std::vector<const package_part_declaration*> ordered(const package_node& package) {
  std::vector<const package_part_declaration*> result;
  std::set<std::string> selected;
  while (result.size() != package.parts.size()) {
    const auto previous = result.size();
    for (const auto& part : package.parts) {
      if (selected.contains(part.name)) { continue; }
      if (part.source != "/" && !selected.contains(part.source)) { continue; }
      result.push_back(&part);
      selected.insert(part.name);
    }
    if (result.size() == previous) {
      throw std::invalid_argument{"Package role sources form an undiscoverable cycle: " + package.name};
    }
  }
  return result;
}

// Identify a fixed singleton role that can accept a direct model pointer in the generated input.
inline bool singleton(const package_part_declaration& part) {
  return part.value_type && part.part_name && part.content_type &&
      part.max_occurs == std::optional<std::size_t>{1U};
}

// Generate a typed C++ facade; package assembly is compiler-owned and values remain borrowed.
inline std::string cpp(const package_node& package,
    const std::function<std::string(std::string_view)>& render_name,
    const std::function<std::string(const type_name&)>& render_type,
    const std::function<std::string(std::string_view)>& field_name,
    const xml_options& xml = {}) {
  const auto parts = ordered(package);
  const auto package_name = render_name(package.name);
  std::set<std::string> member_names{"extensions"};
  std::set<std::pair<std::string, std::string>> dispatch_roles;
  for (const auto* part : parts) {
    if (!member_names.insert(field_name(part->name)).second) {
      throw std::invalid_argument{"Generated package input member collision: " + part->name};
    }
    if (!dispatch_roles.emplace(part->source, part->relationship).second) {
      throw std::invalid_argument{"Package roles require distinct source/relationship dispatch: " + part->name};
    }
  }
  std::string output;
  const auto line = [&](std::string_view value = {}) { output += value; output += '\n'; };
  const auto maximum = [](const package_part_declaration& part) {
    return part.max_occurs ? std::to_string(*part.max_occurs) + "U" :
        "(::std::numeric_limits<::std::size_t>::max)()";
  };
  const auto member = [&](const package_part_declaration& part) { return field_name(part.name); };
  const auto local = [&](const package_part_declaration& part) { return "srl_" + member(part); };
  const auto storage = [](const package_part_declaration& part) {
    return "{.method = ::rohit::serializer::package::zip_method::" + part.compression + "}";
  };
  line("// Generated OPC profile: declarations select structure; callers supply values and providers.");
  line("struct " + package_name + " {");
  output += cpp_profile_metadata(package, xml, package_name);
  line("  // Borrow every resource for the synchronous Save; no model payload is copied here.");
  line("  struct input {");
  for (const auto* part : parts) {
    if (part->part_name && (!part->max_occurs || *part->max_occurs > 1U)) {
      throw std::invalid_argument{"A repeated package role requires name(dynamic): " + part->name};
    }
    if (part->relationship_id && (!part->max_occurs || *part->max_occurs > 1U)) {
      throw std::invalid_argument{"A repeated package role requires generated relationship IDs: " + part->name};
    }
  }
  for (const auto* part : parts) {
    if (singleton(*part)) {
      line("    const " + render_type(*part->value_type) + "* " + member(*part) + "{};");
    } else {
      const auto item = part->value_type ?
          "::rohit::serializer::package::model_part_view<" + render_type(*part->value_type) + ">" :
          "::rohit::serializer::package::binary_part_view";
      line("    ::std::span<const " + item + "> " + member(*part) + "{};");
    }
  }
  line("    const ::rohit::serializer::package::preserved_extensions* extensions{};");
  line("  };");
  line("  // Own a decoded aggregate; dynamic parts retain the names discovered through relationships.");
  line("  struct value {");
  for (const auto* part : parts) {
    if (singleton(*part)) {
      auto type = render_type(*part->value_type);
      if (part->min_occurs == 0U) { type = "::std::optional<" + type + ">"; }
      line("    " + type + " " + member(*part) + "{};");
    } else {
      const auto item = part->value_type ?
          "::rohit::serializer::package::model_part<" + render_type(*part->value_type) + ">" :
          "::rohit::serializer::package::opaque_part";
      line("    ::std::vector<" + item + "> " + member(*part) + "{};");
    }
  }
  line("  };");
  line("  // Check all declared role bounds before creating staging output.");
  line("  static void validate(const input& srl_input) {");
  for (const auto* part : parts) {
    const auto count = singleton(*part) ? "static_cast<::std::size_t>(srl_input." + member(*part) + " != nullptr)" :
        "srl_input." + member(*part) + ".size()";
    line("    ::rohit::serializer::package::require_occurrences(" + count + ", " +
        std::to_string(part->min_occurs) + "U, " + maximum(*part) + ", " + quote(part->name) + ");");
  }
  line("  }");
  line("  // Write the complete declared package to a borrowed stream; failure can retain a prefix.");
  line("  static void write(::std::ostream& srl_output, const input& srl_input,");
  line("      ::std::shared_ptr<::rohit::serializer::package::budget> srl_budget = {}) {");
  line("    validate(srl_input);");
  line("    ::rohit::serializer::package::opc_writer srl_writer{srl_output, ::std::move(srl_budget)};");
  line("    compose(srl_writer, srl_input);");
  line("    srl_writer.finish();");
  line("  }");
  line("  // Stage and validate a complete Save before native publication; input storage stays borrowed.");
  line("  static void save(const ::std::filesystem::path& srl_path, const input& srl_input,");
  line("      ::rohit::serializer::package::limits srl_limits = {}) {");
  line("    validate(srl_input);");
  line("    ::rohit::serializer::package::save_opc_file(srl_path, [&](auto& srl_writer) {");
  line("      compose(srl_writer, srl_input);");
  line("    }, ::std::move(srl_limits));");
  line("  }");
  line("  // Discover typed roles by relationships and publish only a fully checked candidate.");
  line("  static auto load(::std::istream& srl_source,");
  line("      ::rohit::serializer::package::limits srl_limits = {},");
  line("      ::rohit::serializer::package::extension_policy srl_policy = {},");
  line("      const ::std::function<void(const value&, ::rohit::serializer::package::budget&)>& srl_validate = {}) {");
  line("    auto srl_budget = ::std::make_shared<::rohit::serializer::package::budget>(::std::move(srl_limits));");
  line("    ::rohit::serializer::package::opc_reader srl_reader{srl_source, srl_budget};");
  line("    ::std::vector<::rohit::serializer::package::part_relocation> srl_relocations;");
  line("    auto srl_result = ::rohit::serializer::package::load_opc<value>(srl_reader,");
  line("        [&](value& srl_value, auto& srl_loader) {");
  for (const auto* part : parts) {
    line("      ::std::vector<::std::string> " + local(*part) + "_sources;");
    if (part->source == "/") {
      line("      for (const auto& srl_source_name : ::std::vector<::std::string>{\"/\"}) {");
    } else {
      const auto found = std::find_if(parts.begin(), parts.end(), [&](const auto* source) { return source->name == part->source; });
      line("      for (const auto& srl_source_name : " + local(**found) + "_sources) {");
    }
    line("        srl_loader.read_related(srl_source_name, " + quote(part->relationship) + ", " +
        quote(part->content_type.value_or("")) + ",");
    line("            [&](::std::span<const ::std::uint8_t> srl_bytes, const auto& srl_part, auto& srl_shared) {");
    line("          ::rohit::serializer::package::remember_source(" + local(*part) + "_sources, srl_part.name, srl_shared);");
    line("          ::rohit::serializer::package::require_occurrences(" + local(*part) + "_sources.size(), 0U, " +
        maximum(*part) + ", " + quote(part->name) + ");");
    if (part->value_type) {
      const auto type = render_type(*part->value_type);
      if (singleton(*part)) {
        line("          srl_shared.consume(::rohit::serializer::package::resource::metadata_bytes,");
        line("              sizeof(::rohit::serializer::package::part_relocation), srl_part.name);");
        line("          srl_shared.consume(::rohit::serializer::package::resource::metadata_bytes, srl_part.name.size(), srl_part.name);");
        line("          srl_shared.consume(::rohit::serializer::package::resource::metadata_bytes, " + std::to_string(part->part_name->size()) + "U, srl_part.name);");
        line("          srl_relocations.push_back({srl_part.name, " + quote(*part->part_name) + "});");
        line("          srl_value." + member(*part) + " = ::rohit::serializer::package::read_xml_part<" + type + ">(srl_bytes, srl_shared, srl_part.name);");
      } else {
        line("          srl_value." + member(*part) + ".push_back({srl_part.name, srl_part.content_type,");
        line("              ::rohit::serializer::package::read_xml_part<" + type + ">(srl_bytes, srl_shared, srl_part.name)});");
      }
    } else {
      line("          srl_shared.consume(::rohit::serializer::package::resource::decoder_allocation_bytes, srl_bytes.size(), srl_part.name);");
      line("          srl_value." + member(*part) + ".push_back({srl_part, {srl_bytes.begin(), srl_bytes.end()}, {}});");
    }
    line("        }, 0U, (::std::numeric_limits<::std::size_t>::max)(), " +
        std::string{part->content_type ? "false" : "true"} + ");");
    line("      }");
    line("      ::rohit::serializer::package::require_occurrences(" + local(*part) + "_sources.size(), " +
        std::to_string(part->min_occurs) + "U, " + maximum(*part) + ", " + quote(part->name) + ");");
  }
  line("      if (srl_validate) { srl_validate(srl_value, srl_loader.shared_budget()); }");
  line("    }, ::std::move(srl_policy));");
  line("    srl_result.extensions.relocations = ::std::move(srl_relocations);");
  line("    return srl_result;");
  line("  }");
  line("private:");
  line("  // Assemble declared roles; standard manifests and relationships remain runtime-owned.");
  line("  static void compose(::rohit::serializer::package::opc_writer& srl_writer, const input& srl_input) {");
  for (const auto* part : parts) {
    const auto field = "srl_input." + member(*part);
    const auto id = part->relationship_id ? quote(*part->relationship_id) :
        "::rohit::serializer::package::generated_relationship_id(" + quote(part->name) + ", " +
        (singleton(*part) ? "0U" : "srl_index") + ")";
    line("    ::std::vector<::std::string> " + local(*part) + "_sources;");
    if (singleton(*part)) {
      line("    if (" + field + ") {");
      line("      const auto srl_name = " + quote(*part->part_name) + ";");
      line("      const auto srl_relationship_id = " + id + ";");
      line("      ::rohit::serializer::package::write_xml_part(srl_writer, srl_name, " + quote(*part->content_type) + ", *" + field + ", " + storage(*part) + ", srl_relationship_id);");
    } else {
      line("    for (::std::size_t srl_index{}; srl_index < " + field + ".size(); ++srl_index) {");
      line("      const auto& srl_item = " + field + "[srl_index];");
      line("      const ::std::string_view srl_name = " + (part->part_name ? quote(*part->part_name) : "srl_item.name") + ";");
      line("      const auto srl_relationship_id = " + id + ";");
      const auto media = part->content_type ? quote(*part->content_type) : "srl_item.content_type";
      if (part->value_type) {
        line("      if (!srl_item.value) { throw ::std::invalid_argument{\"Package typed resource is unbound\"}; }");
        line("      ::rohit::serializer::package::write_xml_part(srl_writer, srl_name, " + media + ", *srl_item.value, " + storage(*part) + ", srl_relationship_id);");
      } else {
        line("      srl_writer.add_part(srl_name, " + media + ", srl_item.bytes, " + storage(*part) + ");");
      }
    }
    line("      ::rohit::serializer::package::remember_source(" + local(*part) + "_sources, srl_name, *srl_writer.shared_budget());");
    std::string source = "\"/\"";
    if (part->source != "/") {
      const auto found = std::find_if(parts.begin(), parts.end(), [&](const auto* value) { return value->name == part->source; });
      source = "::rohit::serializer::package::require_source(" + local(**found) + "_sources, " +
          (singleton(*part) ? "0U" : "srl_item.source_index") + ", " + quote(part->name) + ")";
    }
    line("      srl_writer.add_relationship({" + source + ", srl_relationship_id, " + quote(part->relationship) + ", ::std::string{srl_name}});");
    line("    }");
  }
  line("    if (srl_input.extensions) { ::rohit::serializer::package::write_preserved(srl_writer, *srl_input.extensions); }");
  line("  }");
  line("};");
  return output;
}

} // namespace rohit::serializer::writer::package_codegen
