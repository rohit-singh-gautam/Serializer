// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/serializer_creator.hpp>
#include "generated_notices.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rohit::serializer::writer::portable::detail {

// Keep generated module identifiers portable and distinct even for punctuation in output names.
inline std::string module_name(std::string_view filename) {
  constexpr std::uint64_t hash_origin = 14695981039346656037ULL;
  constexpr std::uint64_t hash_prime = 1099511628211ULL;
  auto hash = hash_origin;
  std::string name{"_serializer_"};
  for (const auto character : filename) {
    const auto byte = static_cast<unsigned char>(character);
    hash = (hash ^ byte) * hash_prime;
    name += (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
        (byte >= '0' && byte <= '9') || byte == '_' ? character : '_';
  }
  return name + "_" + std::to_string(hash);
}

// Broaden only generated runtime helpers to module visibility when multiple files share them.
inline std::string module_runtime(std::string text, bool top_level = true) {
  bool quoted{};
  bool escaped{};
  bool line_comment{};
  bool block_comment{};
  for (std::size_t position{}; position < text.size(); ++position) {
    const auto character = text[position];
    const auto pair = std::string_view{text}.substr(position, 2);
    if (line_comment) { if (character == '\n') { line_comment = false; } continue; }
    if (block_comment) { if (pair == "*/") { block_comment = false; ++position; } continue; }
    if (quoted) {
      if (escaped) { escaped = false; }
      else if (character == '\\') { escaped = true; }
      else if (character == '"') { quoted = false; }
      continue;
    }
    if (pair == "//") { line_comment = true; ++position; continue; }
    if (pair == "/*") { block_comment = true; ++position; continue; }
    if (character == '"') { quoted = true; continue; }
    const bool line_start = position == 0 || text[position - 1] == '\n';
    if (text.compare(position, std::string_view{"fileprivate "}.size(), "fileprivate ") == 0 &&
        (line_start || text[position - 1] == ' ')) {
      text.replace(position, std::string_view{"fileprivate"}.size(), "internal");
      position += std::string_view{"internal"}.size() - 1;
    } else if (top_level && line_start && text.compare(position, 8, "private ") == 0) {
      text.replace(position, std::string_view{"private"}.size(), "internal");
      position += std::string_view{"internal"}.size() - 1;
    }
  }
  return text;
}

// Preserve each source's required SDK imports, package and file-level opt-in annotations.
inline std::string module_preamble(std::string_view runtime) {
  std::string result;
  for (std::size_t position{}; position < runtime.size();) {
    const auto end = runtime.find('\n', position);
    const auto line = runtime.substr(position, end == std::string_view::npos ? end : end - position);
    if (line.starts_with("import ") || line.starts_with("package ") || line.starts_with("@file:")) {
      result += std::string{line} + '\n';
    }
    if (end == std::string_view::npos) { break; }
    position = end + 1;
  }
  return result;
}

// Partition compiler-owned markers before any artifact is published; no schema text becomes a path.
template <typename Generate>
std::vector<cpp::generated_file> native_files(const parser::parsed_schema& schema,
    std::string_view language, const std::filesystem::path& destination,
    const portable_options& options, Generate&& generate) {
  if (schema.source_files.empty()) { throw std::invalid_argument{"Native bundles require a root source path"}; }
  std::map<std::string, std::size_t> sources;
  for (std::size_t index{}; index < schema.source_files.size(); ++index) {
    if (schema.source_files[index].generic_string().find_first_of("\r\n") != std::string::npos) {
      throw std::invalid_argument{"Schema paths must not contain line breaks"};
    }
    if (!sources.emplace(schema.source_files[index].generic_string(), index).second) {
      throw std::invalid_argument{"Native bundle source paths must be distinct"};
    }
  }
  const auto root_index = schema.source_files.size() - 1;
  const auto text = generate(sources);
  const auto first = text.find('\x1e');
  if (first == std::string::npos) { return {{destination, text}}; }
  const bool python = language == "python";
  const bool fragments = language == "c" || language == "rust";
  const auto notice = generated_license_notice(schema.statements, python ? "#" : "//",
      false, options.xml.enabled);
  const auto source_notice = [&](std::size_t index) {
    const std::string comment = python ? "#" : "//";
    return notice + comment + " Serializer source: " + schema.source_files[index].generic_string() +
        "\n" + comment + " Serializer language: " + std::string{language} + "\n";
  };
  auto runtime = text.substr(0, first);
  std::string imports;
  if (!fragments && !python) {
    runtime = module_runtime(std::move(runtime));
    imports = module_preamble(runtime);
  }
  const auto package = module_name(destination.filename().string());
  const auto directory = destination.parent_path() /
      (python ? package : destination.filename().string() + ".includes");
  std::vector<cpp::generated_file> files;
  std::string root = fragments ? (language == "c" ? "#pragma once\n" : "") + runtime : runtime;
  std::string python_links;
  if (python) {
    files.push_back({directory / "__init__.py", source_notice(root_index)});
    files.push_back({directory / "runtime.py", source_notice(root_index) + runtime +
        "\n# Publish preceding declarations before importing a dependent class definition.\n"
        "def _srl_publish(bindings):\n"
        "    globals().update({name: value for name, value in bindings.items() if not name.startswith('__')})\n"
        "    globals()['__all__'] = [name for name in globals() if not name.startswith('__')]\n"
        "__all__ = [name for name in globals() if not name.startswith('__')]\n"});
    root = source_notice(root_index) + "from __future__ import annotations\n"
        "from importlib import import_module as _srl_import_module\n"
        "_srl_prefix = (__package__ + '.' if __package__ else '') + '" + package + "'\n"
        "_srl_runtime = _srl_import_module(_srl_prefix + '.runtime')\n"
        "globals().update({name: getattr(_srl_runtime, name) for name in _srl_runtime.__all__})\n";
  }
  std::map<std::size_t, std::string> modules;
  std::size_t previous_fragment = sources.size();
  bool append_fragment{};
  std::size_t part_index{};
  for (auto position = first; position < text.size();) {
    const auto marker_end = text.find('\x1f', position + 1);
    if (marker_end == std::string::npos) { throw std::logic_error{"Incomplete native source marker"}; }
    std::size_t source_index{};
    const auto parsed = std::from_chars(text.data() + position + 1, text.data() + marker_end, source_index);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + marker_end || source_index > root_index) {
      throw std::invalid_argument{"Native source contains a reserved compiler marker"};
    }
    const auto next = text.find('\x1e', marker_end + 1);
    const auto body = text.substr(marker_end + 1,
        next == std::string::npos ? next : next - marker_end - 1);
    if (body.find_first_not_of(" \r\n\t") != std::string::npos) {
      if (source_index == root_index) { root += body; append_fragment = false; }
      else if (fragments) {
        if (append_fragment && previous_fragment == source_index) {
          files.back().contents += body;
          if (next == std::string::npos) { break; }
          position = next;
          continue;
        }
        const auto name = "source_" + std::to_string(source_index) + "_part_" +
            std::to_string(part_index++) + destination.extension().string();
        const auto path = directory / name;
        const auto relative = path.lexically_relative(destination.parent_path()).generic_string();
        if (relative.find_first_of("\"\r\n\\") != std::string::npos) {
          throw std::invalid_argument{"Native include paths must not contain quotes or line breaks"};
        }
        files.push_back({path, source_notice(source_index) + body});
        previous_fragment = source_index;
        append_fragment = true;
        root += language == "c" ? "\n#include \"" + relative + "\"\n" :
            "\ninclude!(\"" + relative + "\");\n";
      } else if (python) {
        if (append_fragment && previous_fragment == source_index) {
          const auto trailer = files.back().contents.rfind("\ndef _srl_bind(");
          if (trailer == std::string::npos) { throw std::logic_error{"Missing Python declaration boundary"}; }
          files.back().contents.insert(trailer, body);
          if (next == std::string::npos) { break; }
          position = next;
          continue;
        }
        const auto module = "source_" + std::to_string(source_index) + "_part_" + std::to_string(part_index++);
        files.push_back({directory / (module + ".py"), source_notice(source_index) +
            "from __future__ import annotations\nfrom .runtime import *\n" + body +
            "\ndef _srl_bind(bindings):\n    globals().update({name: value for name, value in bindings.items() if not name.startswith('__')})\n"
            "__all__ = [name for name in globals() if not name.startswith('__')]\n"});
        const auto alias = "_srl_module_" + std::to_string(part_index);
        root += "\n_srl_runtime._srl_publish(globals())\n" + alias +
            " = _srl_import_module(_srl_prefix + '." + module + "')\n"
            "globals().update({name: getattr(" + alias + ", name) for name in " + alias + ".__all__})\n";
        python_links += alias + "._srl_bind(globals())\n";
        previous_fragment = source_index;
        append_fragment = true;
      } else { modules[source_index] += body; }
    }
    if (next == std::string::npos) { break; }
    position = next;
  }
  if (python) {
    root += "\n_srl_runtime._srl_publish(globals())\n" + python_links;
  } else if (!fragments) {
    for (const auto& [index, body] : modules) {
      files.push_back({directory / ("source_" + std::to_string(index) + destination.extension().string()),
          source_notice(index) + imports + module_runtime(body, false)});
    }
    root = module_runtime(std::move(root));
  }
  std::string links;
  for (const auto& file : files) {
    const auto relative = file.path.lexically_relative(destination.parent_path()).generic_string();
    if (relative.find_first_of("\r\n") != std::string::npos) {
      throw std::invalid_argument{"Native output paths must not contain line breaks"};
    }
    links += std::string{python ? "#" : "//"} + " Serializer output: " + relative + '\n';
  }
  files.push_back({destination, source_notice(root_index) + links + root});
  return files;
}
} // namespace rohit::serializer::writer::portable::detail
