// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "package_pipeline.hpp"

#include <rohit/package.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace package = rohit::serializer::package;
namespace document = rohit::serializer::document;
using archive = pipeline::archive;
#ifdef SERIALIZER_PIPELINE_BINARY_ONLY
constexpr bool binary_only = true;
#else
constexpr bool binary_only = false;
#endif

// Keep consumer checks enabled in optimized SDK builds.
void check(bool accepted, std::string_view message, int& count) {
  if (!accepted) { throw std::runtime_error{std::string{message}}; }
  ++count;
}

// Encode through the generated borrowed stream API under one cumulative ledger.
std::string encode(const archive::input& value, std::shared_ptr<package::budget> shared = {}) {
  std::ostringstream output;
  archive::write(output, value, std::move(shared));
  return output.str();
}

// Write test-owned output only after the generated operation completed successfully.
void write(const std::filesystem::path& path, const std::string& data) {
  std::ofstream output{path, std::ios::binary};
  output.write(data.data(), static_cast<std::streamsize>(data.size()));
  if (!output) { throw std::runtime_error{"Could not write SDK output"}; }
}
} // namespace

// Consume one independent OPC fixture and retain complete output for the Python ZIP/XML oracle.
int main(int argc, char** argv) {
  try {
    if (argc != 5) { throw std::runtime_error{"Expected directory, case, codec and decision"}; }
    const std::filesystem::path directory{argv[1]};
    const std::string name{argv[2]}, codec{argv[3]}, decision{argv[4]};
    package::profile_overrides profiles{{"main", {.format = codec == "xml" ? document::payload_format::xml :
        codec == "json" ? document::payload_format::json : document::payload_format::binary_none}}};
    std::ifstream source{directory / (name + ".opc"), std::ios::binary};
    int checks{};
    auto loaded = [&] {
      try { return archive::load(source, {}, {}, {}, profiles); }
      catch (const std::exception&) {
        check(decision == "LOAD_ERROR", "Unexpected fixture load failure", checks);
        std::cout << "{\"decision\":\"LOAD_ERROR\",\"checks\":" << checks << "}\n";
        throw decision;
      }
    }();
    check(decision == "OK", "Invalid input published", checks);
    auto& value = loaded.value;
    check(value.main.id == 17U && value.main.label == "pipeline" && value.main.values.size() == 3U,
        "Native model changed", checks);
    check(value.links.size() == 1U && value.links.front().metadata.content_type.empty() && value.links.front().bytes.empty(),
        "External role did not retain an inert URI", checks);
    std::vector<package::binary_part_view> links;
    for (const auto& part : value.links) { links.push_back({part.bytes, part.metadata.name, part.metadata.content_type, part.source_index}); }
    archive::input input{};
    input.main = &value.main; input.fixed = value.fixed ? &*value.fixed : nullptr;
    input.links = links; input.extensions = &loaded.extensions; input.profiles = &profiles;
    const auto observed = std::make_shared<package::budget>();
    write(directory / (name + "-encoded.opc"), encode(input, observed));
    archive::save(directory / (name + "-saved.opc"), input);
    for (const auto resource : {package::resource::work_units, package::resource::decoder_allocation_bytes}) {
      const auto maximum = resource == package::resource::work_units ? observed->configured().max_work_units : observed->configured().max_decoder_allocation_bytes;
      const auto used = maximum - observed->remaining(resource);
      check(used > 0U, "Missing native resource admission", checks);
      for (const bool tight : {false, true}) {
        package::limits limits{};
        if (resource == package::resource::work_units) { limits.max_work_units = used - static_cast<std::uint64_t>(tight); }
        else { limits.max_decoder_allocation_bytes = used - static_cast<std::uint64_t>(tight); }
        try { static_cast<void>(encode(input, std::make_shared<package::budget>(limits))); check(!tight, "One-tight native quota accepted", checks); }
        catch (const package::error& failure) { check(tight && failure.code == package::error_code::resource_limit, "Exact quota or classification failed", checks); }
      }
    }
    for (const auto format : {binary_only ? document::payload_format::json : document::payload_format::xml,
        document::payload_format::binary_none}) {
      try { static_cast<void>(archive::select_profiles({{"fixed", {.format = format}}})); throw std::runtime_error{"Fixed native override accepted"}; }
      catch (const std::invalid_argument&) { ++checks; }
    }
    if constexpr (binary_only) {
      try { static_cast<void>(archive::select_profiles({{"main", {.format = document::payload_format::json}}}));
        throw std::runtime_error{"Disabled JSON override accepted"}; }
      catch (const package::error& failure) {
        check(failure.code == package::error_code::unsupported_feature && failure.field_path == "main",
            "Disabled JSON profile lost its role/category", checks);
      }
    }
    std::cout << "{\"decision\":\"OK\",\"checks\":" << checks << "}\n";
    return 0;
  } catch (const std::string& decision) { return decision == "LOAD_ERROR" ? 0 : 1; }
  catch (const std::exception& failure) { std::cerr << failure.what() << '\n'; return 1; }
}
