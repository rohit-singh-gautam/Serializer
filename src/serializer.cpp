//////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024  Rohit Jairaj Singh (rohit@singh.org.in)          //
//                                                                      //
// This program is free software: you can redistribute it and/or modify //
// it under the terms of the GNU General Public License as published by //
// the Free Software Foundation, either version 3 of the License, or    //
// (at your option) any later version.                                  //
//                                                                      //
// This program is distributed in the hope that it will be useful,      //
// but WITHOUT ANY WARRANTY; without even the implied warranty of       //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        //
// GNU General Public License for more details.                         //
//                                                                      //
// You should have received a copy of the GNU General Public License    //
// along with this program.  If not, see <https://www.gnu.org/licenses/ //
//////////////////////////////////////////////////////////////////////////

#include <rohit/output_options.hpp>
#include <rohit/schema_compatibility.hpp>
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>
#include <rohit/version.hpp>
#include "xml_schema.hpp"
#include "xsd_import.hpp"
#include "xsd_bundle_output.hpp"

#include "command_line.hpp"
#include "schema_policy.hpp"
#include "compiler_artifact_output.hpp"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
// Shared descriptors implement the Chaturanga-style short/long command-line interface.
constexpr rohit::serializer::cli::commandline_option command_options[] = {
    {'h', "help", "", "Display this help."},
    {'v', "version", "", "Display compiler and supported schema language versions."},
    {'\0', "verbose", "", "Report parse reuse, release-policy dates, and minimum versions."},
    {'\0', "version-policy-as-of", "YYYY-MM-DD",
     "Evaluate release policies on this date (default today's UTC date)."},
    {'\0', "version-policy-warnings-as-errors", "",
     "Fail generation when the current version exceeds a release time policy."},
    {'i', "input", "schema.serializer", "Input with a serializer version 1.0.0; header (1 is a legacy alias)."},
    {'\0', "import-xsd", "file.xsd", "Import a checked local XSD into an owning .serializer source (-o required)."},
    {'\0', "namespace", "name", "Namespace for --import-xsd (default imported)."},
    {'\0', "root", "element", "Select one global XSD root for --import-xsd."},
    {'\0', "check-against", "previous.serializer",
     "Check schema compatibility without generating output."},
    {'\0', "compatibility-protocol", "protocol",
     "Required with --check-against: "
     "binary_none|binary_integer|binary_string|json|protobuf_binary."},
    {'\0', "compatibility-direction", "backward|forward|both",
     "Reader direction to enforce (default both)."},
    {'\0', "compatibility-policy", "policy.json",
     "Versioned reservations for retired field IDs and wire names."},
    {'o', "output", "file", "One output per configuration selecting exactly one language.", true},
    {'\0', "depfile", "file.d", "Write Make-style dependencies for all generated outputs."},
    {'c', "config", "file.ini", "Independent generator configuration; repeat to parse once.", true},
    {'\0', "include-mode", "separate|inline",
     "C++ included schemas use separate headers by default; inline combines them."},
    {'l', "language", "cpp|java|js|typescript|go|csharp|rust|python|swift|kotlin|c",
     "Select output languages; repeat or comma-separate values.", true},
    {'\0', "cpp.output", "file.hpp", "C++ output path; repeat in configuration order.", true},
    {'\0', "java.output", "ClassName.java",
     "Java output path; repeat in configuration order.", true},
    {'\0', "js.output", "schema.mjs", "JavaScript ES module output; repeat per configuration.", true},
    {'\0', "typescript.output", "schema.d.mts",
     "TypeScript declarations matching JS output; repeat per configuration.", true},
    {'\0', "go.output", "schema.go", "Go source output; repeat per configuration.", true},
    {'\0', "csharp.output", "Schema.cs",
     "C# source output; filename supplies outer class; repeat per configuration.", true},
    {'\0', "rust.output", "schema.rs", "Rust source output; repeat per configuration.", true},
    {'\0', "python.output", "schema.py", "Python source output; repeat per configuration.", true},
    {'\0', "swift.output", "Schema.swift", "Swift source output; repeat per configuration.", true},
    {'\0', "kotlin.output", "Schema.kt", "Kotlin source output; repeat per configuration.", true},
    {'\0', "c.output", "schema.h", "C11 source output; repeat per configuration.", true},
    {'\0', "js.naming", "profile|preserve", "JS and TypeScript identifier naming."},
    {'\0', "go.naming", "profile|preserve", "Go identifier naming."},
    {'\0', "csharp.naming", "profile|preserve", "C# identifier naming."},
    {'\0', "go.package", "name", "Go package (default generated)."},
    {'\0', "kotlin.package", "name", "Kotlin package; empty clears it.", false, true},
    {'\0', "csharp.namespace", "name", "C# namespace; empty clears it.", false, true},
    {'\0', "cpp.coding_standard", "profile",
     "serializer|core|google|llvm|gnu|cert|misra|autosar|qt"},
    {'\0', "cpp.naming", "profile|preserve", "C++ identifier naming policy."},
    {'\0', "cpp.format", "true|false", "Run clang-format (default true)."},
    {'\0', "cpp.constant_evaluation", "true|false",
     "Enable generated positional constant evaluation (default false)."},
    {'\0', "cpp.protocols", "all|binary_none", "Select generated C++ protocols (default all)."},
    {'\0', "protocols", "all|binary_none", "Select native protocols for every output; XML is independent."},
    {'\0', "cpp.emission_only", "true|false",
     "Generate borrowed emission models (default false; requires constant evaluation and binary_none)."},
    {'\0', "cpp.protobuf", "true|false",
     "Generate direct Protobuf binary, ProtoJSON, and TextProto codecs."},
    {'\0', "xml.enabled", "true|false", "Generate statically mapped owning C++ XML codecs."},
    {'\0', "xml.namespace", "uri", "Default XML element namespace; class/field bindings may override it."},
    {'\0', "xml.scalars", "attribute|element", "Default scalar placement without a fixed XSD (default element)."},
    {'\0', "xml.runtime_xsd", "true|false", "Allow prepared runtime XSD validation for XML models."},
    {'\0', "xml.validation_scope", "input|output|both", "Choose prepared XSD validation directions (default input)."},
    {'\0', "xml.xsd", "file.xsd", "Bind a fixed supported XSD subset during XML generation."},
    {'\0', "xml.bundle_sha256", "sha256", "Require the exact fixed XSD bundle identity."},
    {'\0', "xml.export-xsd", "file.xsd", "Export one configuration's supported bound XML contract."},
    {'\0', "xml.export-xsd-bundle", "directory", "Export a pinned multi-namespace XSD bundle to a new directory."},
    {'\0', "xml.compare-xsd", "previous.xsd", "Require the current bound XML reader to accept the previous XSD subset."},
    {'\0', "managed.id_type", "uint32|uint64", "Persistent managed ID type (default uint32)."},
    {'\0', "managed.id_encoding", "fixed|compact_varint", "Persistent managed ID encoding (default compact_varint)."},
    {'\0', "managed.separate_values", "true|false",
     "Generate separate ID-free values and managed storage (default false)."},
    {'\0', "cpp.clang_format", "executable", "clang-format 19+ executable (default clang-format)."},
    {'\0', "cpp.format_file", "file", "Custom layout; --cpp.format_file= clears it.", false, true},
    {'\0', "java.coding_standard", "profile", "serializer|google|oracle"},
    {'\0', "java.naming", "profile|preserve", "Java identifier naming policy."},
    {'\0', "java.package", "package.name", "Java package; --java.package= clears it.", false, true},
    {'\0', "java.output_buffer_reuse", "true|false", "Add caller-owned Java output windows (default false)."},
};

// Apply global command-line values independently to each configuration.
void apply_output_overrides(rohit::serializer::writer::output_options& options,
                            const rohit::serializer::cli::arguments& arguments) {
  using namespace rohit::serializer;
  if (arguments.contains("protocols")) {
    writer::select_output_protocols(options, writer::parse_output_protocols(cli::first(arguments, "protocols")));
  }
  for (const auto* key : {"xml.enabled", "xml.runtime_xsd"}) {
    if (!arguments.contains(key)) { continue; }
    const auto& value = cli::first(arguments, key);
    if (value != "true" && value != "false") {
      throw std::invalid_argument{std::string{key} + " must be true or false"};
    }
    if (std::string_view{key} == "xml.enabled") {
      options.cpp.xml.enabled = value == "true";
      options.cpp.xml.enabled_selection = options.cpp.xml.enabled;
    }
    else { options.cpp.xml.runtime_xsd = value == "true"; }
  }
  if (arguments.contains("xml.xsd")) { options.cpp.xml.xsd = cli::first(arguments, "xml.xsd"); }
  if (arguments.contains("xml.namespace")) { options.cpp.xml.namespace_uri = cli::first(arguments, "xml.namespace"); }
  if (arguments.contains("xml.scalars")) {
    const auto& value = cli::first(arguments, "xml.scalars");
    if (value != "attribute" && value != "element") {
      throw std::invalid_argument{"xml.scalars must be attribute or element"};
    }
    options.cpp.xml.scalar_attributes = value == "attribute";
  }
  if (arguments.contains("xml.bundle_sha256")) {
    options.cpp.xml.bundle_sha256 = cli::first(arguments, "xml.bundle_sha256");
  }
  if (arguments.contains("xml.validation_scope")) {
    const auto& value = cli::first(arguments, "xml.validation_scope");
    if (value != "input" && value != "output" && value != "both") {
      throw std::invalid_argument{"XML validation_scope must be input, output or both"};
    }
    options.cpp.xml.validation_scope = value;
  }
  if (arguments.contains("include-mode")) {
    options.includes = writer::parse_include_mode(cli::first(arguments, "include-mode"));
  }
  if (arguments.contains("language")) {
    options.language.clear();
    for (const auto& value : arguments.at("language")) {
      if (!options.language.empty()) {
        options.language += ',';
      }
      options.language += value;
    }
  }
  if (arguments.contains("java.coding_standard")) {
    options.java.standard =
        writer::parse_java_coding_standard(cli::first(arguments, "java.coding_standard"));
  }
  if (arguments.contains("java.package")) {
    options.java.package_name = cli::first(arguments, "java.package");
  }
  if (arguments.contains("java.output_buffer_reuse")) {
    const auto& value = cli::first(arguments, "java.output_buffer_reuse");
    if (value != "true" && value != "false") {
      throw std::invalid_argument{"java.output_buffer_reuse must be true or false"};
    }
    options.java.output_buffer_reuse = value == "true";
  }
  if (arguments.contains("java.naming")) {
    const auto& value = cli::first(arguments, "java.naming");
    if (value != "profile" && value != "preserve") {
      throw std::invalid_argument{"java.naming must be profile or preserve"};
    }
    options.java.rename_identifiers = value == "profile";
  }
  if (arguments.contains("cpp.coding_standard")) {
    options.cpp.standard =
        writer::parse_coding_standard(cli::first(arguments, "cpp.coding_standard"));
  }
  if (arguments.contains("cpp.naming")) {
    const auto& value = cli::first(arguments, "cpp.naming");
    if (value != "profile" && value != "preserve") {
      throw std::invalid_argument{"cpp.naming must be profile or preserve"};
    }
    options.cpp.rename_identifiers = value == "profile";
  }
  if (arguments.contains("managed.id_type")) {
    options.cpp.managed_id_type = cli::first(arguments, "managed.id_type");
    if (options.cpp.managed_id_type != "uint32" && options.cpp.managed_id_type != "uint64") {
      throw std::invalid_argument{"managed.id_type must be uint32 or uint64"};
    }
  }
  if (arguments.contains("managed.id_encoding")) {
    options.cpp.managed_id_encoding = writer::parse_managed_id_encoding(cli::first(arguments, "managed.id_encoding"));
  }
  if (arguments.contains("managed.separate_values")) {
    const auto& value = cli::first(arguments, "managed.separate_values");
    if (value != "true" && value != "false") {
      throw std::invalid_argument{"managed.separate_values must be true or false"};
    }
    options.cpp.managed_separate_values = value == "true";
  }
  if (arguments.contains("cpp.constant_evaluation")) {
    const auto& value = cli::first(arguments, "cpp.constant_evaluation");
    if (value != "true" && value != "false") {
      throw std::invalid_argument{"cpp.constant_evaluation must be true or false"};
    }
    options.cpp.constant_evaluation = value == "true";
  }
  if (arguments.contains("cpp.emission_only")) {
    const auto& value = cli::first(arguments, "cpp.emission_only");
    if (value != "true" && value != "false") {
      throw std::invalid_argument{"cpp.emission_only must be true or false"};
    }
    options.cpp.emission_only = value == "true";
  }
  if (arguments.contains("cpp.protocols")) {
    options.cpp.protocols = writer::parse_cpp_protocols(cli::first(arguments, "cpp.protocols"));
  }
  if (arguments.contains("cpp.protobuf")) {
    const auto& value = cli::first(arguments, "cpp.protobuf");
    if (value != "true" && value != "false") {
      throw std::invalid_argument{"cpp.protobuf must be true or false"};
    }
    options.cpp.protobuf = value == "true";
  }
  if (arguments.contains("cpp.format")) {
    const auto& value = cli::first(arguments, "cpp.format");
    if (value != "true" && value != "false") {
      throw std::invalid_argument{"cpp.format must be true or false"};
    }
    options.cpp.format = value == "true";
  }
  if (arguments.contains("cpp.clang_format")) {
    options.cpp.clang_format = cli::first(arguments, "cpp.clang_format");
  }
  if (arguments.contains("cpp.format_file")) {
    options.cpp.format_file = cli::first(arguments, "cpp.format_file");
  }
  if (!options.cpp.format && !options.cpp.format_file.empty()) {
    throw std::invalid_argument{"cpp.format_file requires cpp.format true"};
  }
  for (const auto language : {"js", "go", "csharp"}) {
    auto& settings = std::string_view{language} == "js"   ? options.js
                     : std::string_view{language} == "go" ? options.go
                                                          : options.csharp;
    const auto key = std::string{language} + ".naming";
    if (arguments.contains(key)) {
      const auto& value = cli::first(arguments, key);
      if (value != "profile" && value != "preserve") {
        throw std::invalid_argument{key + " must be profile or preserve"};
      }
      settings.rename_identifiers = value == "profile";
    }
  }
  if (arguments.contains("go.package")) {
    options.go.package_name = cli::first(arguments, "go.package");
  }
  if (arguments.contains("csharp.namespace")) {
    options.csharp.namespace_name = cli::first(arguments, "csharp.namespace");
  }
  if (arguments.contains("kotlin.package")) {
    options.kotlin.package_name = cli::first(arguments, "kotlin.package");
  }
}

struct generation_configuration {
  rohit::serializer::writer::output_options options{};
  std::vector<std::string> languages{};
};

struct generation_request {
  std::size_t configuration_index{};
  std::string language{};
  std::filesystem::path destination{};
};

// Keep each generated artifact associated with the configuration that owns its presentation.
struct generation_artifact {
  std::size_t configuration_index{};
  std::string language{};
  rohit::serializer::writer::cpp::generated_file file{};
};

// Load independent INIs, or one default configuration, then apply shared CLI overrides.
std::vector<generation_configuration>
read_configurations(const rohit::serializer::cli::arguments& arguments) {
  std::vector<generation_configuration> configurations{};
  if (arguments.contains("config")) {
    for (const auto& file : arguments.at("config")) {
      configurations.push_back({rohit::serializer::writer::read_output_options(file), {}});
    }
  } else {
    configurations.emplace_back();
  }
  for (auto& configuration : configurations) {
    apply_output_overrides(configuration.options, arguments);
    configuration.languages =
        rohit::serializer::writer::parse_output_languages(configuration.options.language);
    // XML mapping is shared across output backends; presentation remains language-specific.
    configuration.options.java.xml = configuration.options.cpp.xml;
    configuration.options.js.xml = configuration.options.cpp.xml;
    configuration.options.go.xml = configuration.options.cpp.xml;
    configuration.options.csharp.xml = configuration.options.cpp.xml;
    configuration.options.kotlin.xml = configuration.options.cpp.xml;
  }
  return configurations;
}

// Match each output list to configurations selecting its language, preserving configuration order.
std::vector<generation_request>
resolve_generation_requests(const rohit::serializer::cli::arguments& arguments,
                            const std::vector<generation_configuration>& configurations) {
  if (arguments.contains("xml.export-xsd") || arguments.contains("xml.export-xsd-bundle") ||
      arguments.contains("xml.compare-xsd")) {
    bool has_output = arguments.contains("output");
    for (const auto* language : {"cpp", "java", "js", "typescript", "go", "csharp", "rust", "python", "swift", "kotlin", "c"}) {
      has_output = has_output || arguments.contains(std::string{language} + ".output");
    }
    if (!has_output) { return {}; }
  }
  std::map<std::string, std::size_t> language_counts{};
  for (const auto& configuration : configurations) {
    if (arguments.contains("output") && configuration.languages.size() != 1) {
      throw std::invalid_argument{
          "Use language-specific --<language>.output paths for multiple languages"};
    }
    for (const auto& language : configuration.languages) {
      ++language_counts[language];
    }
  }
  if (arguments.contains("output") &&
      arguments.at("output").size() != configurations.size()) {
    throw std::invalid_argument{
        "--output requires one output path per configuration (expected " +
        std::to_string(configurations.size()) + ", got " +
        std::to_string(arguments.at("output").size()) + ")"};
  }
  for (const auto language : {"cpp", "java", "js", "typescript", "go", "csharp", "rust", "python",
                              "swift", "kotlin", "c"}) {
    const auto key = std::string{language} + ".output";
    if (!arguments.contains(key)) {
      continue;
    }
    if (!language_counts.contains(language)) {
      throw std::invalid_argument{"Output path supplied for unselected language: " +
                                  std::string{language}};
    }
    if (arguments.contains("output")) {
      throw std::invalid_argument{"Specify only one of --output and --" + key};
    }
    if (arguments.at(key).size() != language_counts.at(language)) {
      throw std::invalid_argument{
          "--" + key + " requires one output path per selected configuration (expected " +
          std::to_string(language_counts.at(language)) + ", got " +
          std::to_string(arguments.at(key).size()) + ")"};
    }
  }
  std::vector<generation_request> requests{};
  std::map<std::string, std::size_t> output_indices{};
  for (std::size_t index = 0; index < configurations.size(); ++index) {
    for (const auto& language : configurations[index].languages) {
      const auto key = arguments.contains("output") ? "output" : language + ".output";
      if (!arguments.contains(key)) {
        throw std::invalid_argument{"Missing output path: --" + key};
      }
      const auto output_index = arguments.contains("output") ? index : output_indices[language]++;
      requests.push_back({index, language, arguments.at(key).at(output_index)});
    }
  }
  return requests;
}

// Detect aliases and hard links as well as identical normalized paths before replacing output.
bool same_file(const std::filesystem::path& first, const std::filesystem::path& second) {
  if (std::filesystem::exists(first) && std::filesystem::exists(second) &&
      std::filesystem::equivalent(first, second)) {
    return true;
  }
  return std::filesystem::weakly_canonical(first) == std::filesystem::weakly_canonical(second);
}

// Quote an absolute path for CMake's Make-style depfile reader, including Windows drive letters.
std::string dependency_path(const std::filesystem::path& path) {
  std::string result{};
  for (const auto ch : std::filesystem::absolute(path).lexically_normal().generic_string()) {
    if (ch == '\n' || ch == '\r') {
      throw std::invalid_argument{"Dependency paths must not contain line breaks"};
    }
    if (ch == '$') {
      result += "$$";
    } else {
      if (ch == ' ' || ch == '\t' || ch == '#' || ch == ':' || ch == '\\') {
        result.push_back('\\');
      }
      result.push_back(ch);
    }
  }
  return result;
}

// Import into a new reviewed source file; never overwrite manually edited declarations.
int import_schema(const rohit::serializer::cli::arguments& arguments) {
  using namespace rohit::serializer;
  const std::set<std::string> allowed{"import-xsd", "namespace", "root", "output", "depfile", "verbose"};
  for (const auto& [name, values] : arguments) {
    static_cast<void>(values);
    if (!allowed.contains(name)) {
      throw std::invalid_argument{"--import-xsd cannot be combined with --" + name};
    }
  }
  if (!arguments.contains("output") || arguments.at("output").size() != 1U) {
    throw std::invalid_argument{"--import-xsd requires exactly one --output file.serializer"};
  }
  const std::filesystem::path input{cli::first(arguments, "import-xsd")};
  const std::filesystem::path output{cli::first(arguments, "output")};
  if (output.extension() != ".serializer") {
    throw std::invalid_argument{"Imported output must use .serializer"};
  }
  const auto parent = std::filesystem::absolute(output).parent_path();
  if (!std::filesystem::is_directory(parent) || std::filesystem::is_directory(output)) {
    throw std::invalid_argument{"Import output requires an existing parent directory and a file path"};
  }
  writer::xml_binding::import_options options{};
  if (arguments.contains("namespace")) { options.namespace_name = cli::first(arguments, "namespace"); }
  if (arguments.contains("root")) { options.root_name = cli::first(arguments, "root"); }
  const auto source = std::filesystem::weakly_canonical(input);
  const auto relative = source.lexically_relative(std::filesystem::weakly_canonical(parent));
  options.schema_location = (relative.empty() ? source : relative).generic_string();
  const auto result = writer::xml_binding::import_xsd(input, options);
  for (const auto& dependency : result.dependencies) {
    if (same_file(output, dependency)) {
      throw std::invalid_argument{"Import must not overwrite an XSD dependency"};
    }
  }
  std::string deptext{};
  std::filesystem::path depfile{};
  if (arguments.contains("depfile")) {
    depfile = cli::first(arguments, "depfile");
    if (same_file(depfile, output) || std::filesystem::is_directory(depfile) ||
        !std::filesystem::is_directory(std::filesystem::absolute(depfile).parent_path())) {
      throw std::invalid_argument{"Import depfile needs a distinct file with an existing directory parent"};
    }
    for (const auto& dependency : result.dependencies) {
      if (same_file(depfile, dependency)) {
        throw std::invalid_argument{"Import depfile must not overwrite an XSD dependency"};
      }
    }
    deptext = dependency_path(output) + ':';
    for (const auto& dependency : result.dependencies) { deptext += ' ' + dependency_path(dependency); }
    deptext += '\n';
  }
  if (std::filesystem::exists(output)) {
    std::ifstream existing{output, std::ios::binary};
    const std::string contents{std::istreambuf_iterator<char>{existing}, {}};
    if (!existing.eof() && existing.fail()) { throw std::runtime_error{"Could not read existing import output"}; }
    if (contents != result.schema_text) {
      throw std::invalid_argument{"Import output differs; generate to a new path and review stable field IDs and application edits"};
    }
  } else {
    rohit::full_stream_auto_alloc destination{};
    destination.write(result.schema_text);
    destination.write_to_file_till_offset(output);
  }
  if (!depfile.empty()) {
    rohit::serializer::compiler_output::write_if_different(depfile, deptext);
  }
  std::cout << "Imported " << output << " (XSD bundle SHA-256 " << result.bundle_identity << ")\n";
  return 0;
}

// Run read-only evolution checks separately from code generation and select the reader direction.
int check_compatibility(const rohit::serializer::cli::arguments& arguments,
                        const rohit::serializer::parser::parse_options& parse_options) {
  using namespace rohit::serializer;
  const std::set<std::string> allowed{"input",
                                      "check-against",
                                      "compatibility-protocol",
                                      "compatibility-direction",
                                      "compatibility-policy",
                                      "verbose",
                                      "version-policy-as-of",
                                      "version-policy-warnings-as-errors"};
  for (const auto& [name, values] : arguments) {
    static_cast<void>(values);
    if (!allowed.contains(name)) {
      throw std::invalid_argument{"--check-against cannot be combined with --" + name};
    }
  }
  if (!arguments.contains("compatibility-protocol")) {
    throw std::invalid_argument{"--check-against requires --compatibility-protocol"};
  }
  const auto protocol =
      parse_compatibility_protocol(cli::first(arguments, "compatibility-protocol"));
  const auto direction = arguments.contains("compatibility-direction")
                             ? cli::first(arguments, "compatibility-direction")
                             : "both";
  if (direction != "backward" && direction != "forward" && direction != "both") {
    throw std::invalid_argument{"compatibility-direction must be backward, forward, or both"};
  }
  const auto previous = parser::parse_file(cli::first(arguments, "check-against"), parse_options);
  const auto current = parser::parse_file(cli::first(arguments, "input"), parse_options);
  const auto policy = arguments.contains("compatibility-policy")
                          ? read_compatibility_policy(cli::first(arguments, "compatibility-policy"))
                          : compatibility_policy{};
  const auto issues = check_schema_compatibility(previous, current, protocol, policy);
  bool incompatible{};
  for (const auto& item : issues) {
    const bool selected = (direction != "forward" && item.breaks_new_reader) ||
                          (direction != "backward" && item.breaks_old_reader);
    incompatible = incompatible || selected;
    std::cout << (selected ? "incompatible: " : "other direction: ") << item.path << ": "
              << item.message << " [" << (item.breaks_new_reader ? "new-reader" : "")
              << (item.breaks_new_reader && item.breaks_old_reader ? ", " : "")
              << (item.breaks_old_reader ? "old-reader" : "") << "]\n";
  }
  if (!incompatible) {
    std::cout << "No incompatibilities detected for " << direction
              << "; byte order and application semantics require separate agreement.\n";
  }
  return incompatible ? 2 : 0;
}
} // namespace

// Validate all destinations and generate every backend before replacing any output files.
int main(const int argc, const char* argv[]) {
  using namespace rohit::serializer;
  try {
    const auto parsed = cli::parse(argc, argv, command_options);
    if (parsed.contains("help")) {
      std::cout << cli::usage(command_options)
                << "Defaults: C++20, Serializer style, profile naming, clang-format 19+.\n"
                   "Precedence: defaults < config file < command line.\n";
      return 0;
    }
    if (parsed.contains("version")) {
      std::cout << "Serializer compiler " << compiler_version
                << "\nSupported schema language version: " << schema_language_version_text << '\n';
      return 0;
    }
    if (parsed.contains("import-xsd")) { return import_schema(parsed); }
    if (parsed.contains("namespace") || parsed.contains("root")) {
      throw std::invalid_argument{"--namespace and --root require --import-xsd"};
    }
    if (!parsed.contains("input")) {
      throw std::invalid_argument{"--input is required; use --help for options"};
    }
    parser::parse_options parse_options{};
    if (parsed.contains("version-policy-as-of")) {
      parse_options.version_policy_as_of = cli::first(parsed, "version-policy-as-of");
    }
    parse_options.version_policy_warnings_as_errors =
        parsed.contains("version-policy-warnings-as-errors");
    parse_options.warning = [](std::string_view message) {
      std::cerr << "Serializer warning: " << message << '\n';
    };
    if (parsed.contains("verbose")) {
      parse_options.information = [](std::string_view message) {
        std::cout << "Serializer: " << message << '\n';
      };
    }
    parse_options = schema_policy::capture_options(parse_options);
    if (parsed.contains("check-against")) {
      return check_compatibility(parsed, parse_options);
    }
    if (parsed.contains("compatibility-protocol") || parsed.contains("compatibility-direction") ||
        parsed.contains("compatibility-policy")) {
      throw std::invalid_argument{"Compatibility options require --check-against"};
    }
    auto configurations = read_configurations(parsed);
    const std::filesystem::path input_file{cli::first(parsed, "input")};
    if (input_file.extension() != ".serializer") {
      throw std::invalid_argument{
          "Input must use .serializer; rename the schema and add serializer version 1.0.0;"};
    }
    const auto schema = parser::parse_file(input_file, parse_options);
    for (auto& configuration : configurations) {
      auto& options = configuration.options;
      options.cpp.xml = writer::xml_binding::effective_options(schema.statements, options.cpp.xml);
      options.java.xml = options.cpp.xml;
      options.js.xml = options.cpp.xml;
      options.go.xml = options.cpp.xml;
      options.csharp.xml = options.cpp.xml;
      options.kotlin.xml = options.cpp.xml;
    }
    const bool export_bundle = parsed.contains("xml.export-xsd-bundle");
    if ((parsed.contains("xml.export-xsd") || export_bundle || parsed.contains("xml.compare-xsd")) && configurations.size() != 1U) {
      throw std::invalid_argument{"XML export/compatibility tools require exactly one configuration"};
    }
    auto requests = resolve_generation_requests(parsed, configurations);
    if (export_bundle && (!requests.empty() || parsed.contains("xml.export-xsd") ||
        parsed.contains("xml.compare-xsd") || parsed.contains("depfile"))) {
      throw std::invalid_argument{"--xml.export-xsd-bundle is a standalone export; do not combine outputs, comparison or depfiles"};
    }
    if (parsed.contains("xml.export-xsd")) {
      requests.push_back({0U, "xsd", cli::first(parsed, "xml.export-xsd")});
    }
    std::vector<std::filesystem::path> configuration_dependencies{input_file};
    std::vector<writer::xml_binding::schema> xml_contracts{};
    if (parsed.contains("config")) {
      for (const auto& file : parsed.at("config")) {
        configuration_dependencies.emplace_back(file);
      }
    }
    for (const auto& configuration : configurations) {
      if (!configuration.options.cpp.format_file.empty()) {
        configuration_dependencies.push_back(configuration.options.cpp.format_file);
      }
      xml_contracts.emplace_back(configuration.options.cpp.xml);
      if (parsed.contains("verbose") && !xml_contracts.back().bundle_identity().empty()) {
        std::cout << "Serializer: XSD bundle SHA-256 " << xml_contracts.back().bundle_identity() << '\n';
      }
    }
    std::vector<std::filesystem::path> destinations{};
    for (const auto& request : requests) {
      const auto& language = request.language;
      const auto& output_file = request.destination;
      for (const auto& dependency : configuration_dependencies) {
        if (same_file(dependency, output_file)) {
          throw std::invalid_argument{
              "Output must not overwrite the schema or an output configuration"};
        }
      }
      for (const auto& previous : destinations) {
        if (same_file(previous, output_file)) {
          throw std::invalid_argument{"Output paths must identify distinct files"};
        }
      }
      const auto extension = output_file.extension();
      if (language == "cpp" && extension != ".h" && extension != ".hpp" && extension != ".hxx") {
        throw std::invalid_argument{"C++ output must have a .h, .hpp, or .hxx extension"};
      }
      if (language == "java" && extension != ".java") {
        throw std::invalid_argument{"Java output must have a .java extension"};
      }
      if ((language == "js" && extension != ".js" && extension != ".mjs") ||
          (language == "go" && extension != ".go") || (language == "rust" && extension != ".rs") ||
          (language == "python" && extension != ".py") ||
          (language == "swift" && extension != ".swift") ||
          (language == "kotlin" && extension != ".kt") || (language == "c" && extension != ".h") ||
          (language == "csharp" && extension != ".cs") ||
          (language == "typescript" && !output_file.string().ends_with(".d.ts") &&
           !output_file.string().ends_with(".d.mts"))) {
        throw std::invalid_argument{"Invalid output extension for " + language};
      }
      const auto parent = output_file.parent_path();
      if ((!parent.empty() && !std::filesystem::is_directory(parent)) ||
          std::filesystem::is_directory(output_file)) {
        throw std::invalid_argument{
            "Output requires an existing parent directory and a file path: " +
            output_file.string()};
      }
      destinations.push_back(output_file);
    }
    std::vector<const class_node*> xml_records{};
    const auto collect_xml_records = [&](auto&& self, const auto& statements) -> void {
      for (const auto& node : statements) {
        if (node->type == object_type::namespace_type) { self(self, static_cast<const namespace_node&>(*node).statements); }
        else if (node->type == object_type::class_type) { xml_records.push_back(&static_cast<const class_node&>(*node)); }
      }
    };
    collect_xml_records(collect_xml_records, schema.statements);
    std::set<const class_node*> xml_storage_fragments;
    for (std::size_t index{}; index < configurations.size(); ++index) {
      if (!configurations[index].options.cpp.xml.enabled) { continue; }
      // Bind inline XSDs before collecting dependencies or publishing any generated files.
      // Mixed fragment records are native storage in the backends that implement ordered content.
      const bool mixed_storage_only = std::all_of(requests.begin(), requests.end(), [&](const auto& request) {
        return request.configuration_index != index || request.language == "python" || request.language == "cpp" ||
            request.language == "js" || request.language == "typescript" || request.language == "java" ||
            request.language == "csharp" || request.language == "go" || request.language == "kotlin" || request.language == "swift" ||
            request.language == "c" || request.language == "rust" || request.language == "xsd";
      });
      std::set<const class_node*> mixed_fragments{};
      const auto& mappings = configurations[index].options.cpp.xml.classes;
      if (mixed_storage_only) {
        for (const auto* record : xml_records) {
          const auto configured = mappings.find(record->get_full_name());
          if (configured != mappings.end() && (configured->second.contracts.contains("mixed") ||
              configured->second.contracts.contains("ordered"))) {
            const auto contract = xml_contracts[index].bind(*record);
            if (contract.mixed && !mappings.contains(contract.mixed->fragment->get_full_name())) {
              mixed_fragments.insert(contract.mixed->fragment);
            }
          }
        }
      }
      for (const auto* record : xml_records) {
        if (!mixed_fragments.contains(record)) { static_cast<void>(xml_contracts[index].bind(*record)); }
      }
      if (index == 0U) { xml_storage_fragments = mixed_fragments; }
      const auto bundle_dependencies = xml_contracts[index].dependencies();
      configuration_dependencies.insert(configuration_dependencies.end(), bundle_dependencies.begin(), bundle_dependencies.end());
    }
    if (parsed.contains("xml.export-xsd") || export_bundle || parsed.contains("xml.compare-xsd")) {
      if (!configurations.front().options.cpp.xml.enabled) {
        throw std::invalid_argument{"XML export/compatibility tools require xml.enabled = true"};
      }
      if (xml_records.empty()) { throw std::invalid_argument{"XML tooling requires at least one ordinary record"}; }
    }
    if (export_bundle) {
      writer::xml_binding::validate_options(schema.statements, configurations.front().options.cpp.xml, "xsd");
      const auto bundle = xml_contracts.front().export_xsd_bundle(xml_records);
      const std::filesystem::path destination{cli::first(parsed, "xml.export-xsd-bundle")};
      writer::xml_binding::publish_xsd_bundle(bundle, destination);
      std::cout << "Exported XSD bundle " << destination << " (SHA-256 " << bundle.identity << ")\n";
      return 0;
    }
    if (parsed.contains("xml.compare-xsd")) {
      auto previous_options = configurations.front().options.cpp.xml;
      if (previous_options.xsd.empty()) { throw std::invalid_argument{"--xml.compare-xsd requires the current --xml.xsd contract"}; }
      previous_options.xsd = cli::first(parsed, "xml.compare-xsd");
      previous_options.bundle_sha256.clear();
      const writer::xml_binding::schema previous{previous_options};
      bool compatible{true};
      for (const auto* record : xml_records) {
        if (xml_storage_fragments.contains(record)) { continue; }
        for (const auto& diagnostic : writer::xml_binding::compare_contracts(
            previous.bind(*record), xml_contracts.front().bind(*record))) {
          std::cerr << "XML incompatibility: " << record->get_full_name();
          if (!diagnostic.member.empty()) { std::cerr << '.' << diagnostic.member; }
          std::cerr << ": " << diagnostic.message << '\n';
          compatible = false;
        }
      }
      if (!compatible) { return 1; }
      const auto previous_dependencies = previous.dependencies();
      configuration_dependencies.insert(configuration_dependencies.end(), previous_dependencies.begin(), previous_dependencies.end());
      std::cout << "XML compatibility: current reader accepts the previous bound contract\n";
    }
    // Validate the actual requested backend: a Python-only contract need not be emitted in C++.
    // All artifacts are still prepared before publication, so one unsupported target aborts the batch.
    for (const auto& request : requests) {
      const auto& configuration = configurations[request.configuration_index];
      writer::xml_binding::validate_options(schema.statements, configuration.options.cpp.xml,
          request.language);
    }
    if (parsed.contains("verbose")) {
      std::cout << "Serializer: Parsed schema once for " << configurations.size()
                << " configurations\n";
    }
    auto dependencies = schema.dependencies;
    // The parser supplies the root and transitive schemas; add every INI and custom format file.
    dependencies.insert(dependencies.end(), configuration_dependencies.begin() + 1,
                        configuration_dependencies.end());
    std::vector<generation_artifact> artifacts{};
    for (const auto& request : requests) {
      const auto& options = configurations[request.configuration_index].options;
      if (request.language == "xsd") {
        artifacts.push_back({request.configuration_index, request.language,
            {request.destination, xml_contracts.at(request.configuration_index).export_xsd(xml_records)}});
      } else if (request.language == "cpp") {
        for (auto& file : writer::cpp::generate_files(schema, request.destination, options.cpp,
                                                     options.includes)) {
          artifacts.push_back({request.configuration_index, request.language, std::move(file)});
        }
      } else {
        std::string contents{};
        if (request.language == "java") {
          contents = writer::java::generate_schema(schema, request.destination.stem().string(),
                                                   options.java);
        } else {
          const auto& language = request.language;
          auto settings = language == "go" ? options.go : language == "csharp" ? options.csharp :
                          language == "kotlin" ? options.kotlin : options.js;
          if (const auto found = options.class_comparisons.find(language);
              found != options.class_comparisons.end()) {
            settings.class_comparisons = found->second;
          }
          if (const auto found = options.class_auto_sizes.find(language);
              found != options.class_auto_sizes.end()) {
            settings.class_auto_sizes = found->second;
          }
          contents = writer::portable::generate_schema(schema, language,
              request.destination.stem().string(), settings);
        }
        artifacts.push_back({request.configuration_index, request.language,
                             {request.destination, std::move(contents)}});
      }
    }
    destinations.clear();
    for (const auto& artifact : artifacts) {
      const auto& destination = artifact.file.path;
      for (const auto& previous : destinations) {
        if (same_file(previous, destination)) {
          throw std::invalid_argument{"Output paths must identify distinct files"};
        }
      }
      auto parent = destination.parent_path();
      while (!parent.empty() && !std::filesystem::exists(parent)) { parent = parent.parent_path(); }
      if ((!parent.empty() && !std::filesystem::is_directory(parent)) ||
          std::filesystem::is_directory(destination)) {
        throw std::invalid_argument{"Generated output requires a directory parent and a file path"};
      }
      // A pre-existing symlink must not redirect an implicit child outside its output directory.
      const auto owner = std::find_if(requests.begin(), requests.end(), [&](const auto& request) {
        return request.configuration_index == artifact.configuration_index &&
               request.language == artifact.language;
      });
      if (destination != owner->destination) {
        const auto root = std::filesystem::weakly_canonical(
            std::filesystem::absolute(owner->destination).parent_path());
        const auto relative = std::filesystem::weakly_canonical(destination).lexically_relative(root);
        if (relative.empty() || *relative.begin() == "..") {
          throw std::invalid_argument{"Generated output must remain inside its output directory"};
        }
      }
      destinations.push_back(destination);
    }
    for (const auto& destination : destinations) {
      for (const auto& dependency : dependencies) {
        if (same_file(destination, dependency)) {
          throw std::invalid_argument{"Output must not overwrite a schema or configuration"};
        }
      }
    }
    std::string dependency_text{};
    std::filesystem::path depfile{};
    if (parsed.contains("depfile")) {
      depfile = cli::first(parsed, "depfile");
      const auto parent = depfile.parent_path();
      if ((!parent.empty() && !std::filesystem::is_directory(parent)) ||
          std::filesystem::is_directory(depfile)) {
        throw std::invalid_argument{
            "Depfile requires an existing parent directory and a file path"};
      }
      for (const auto& dependency : dependencies) {
        if (same_file(depfile, dependency)) {
          throw std::invalid_argument{"Depfile must not overwrite a schema or configuration"};
        }
      }
      for (const auto& destination : destinations) {
        if (same_file(depfile, destination)) {
          throw std::invalid_argument{"Depfile must not overwrite generated output"};
        }
        if (!dependency_text.empty()) {
          dependency_text += ' ';
        }
        dependency_text += dependency_path(destination);
      }
      dependency_text += ':';
      for (const auto& dependency : dependencies) {
        dependency_text += ' ';
        dependency_text += dependency_path(dependency);
      }
      dependency_text += '\n';
    }
    // Backend/formatter failures leave all destinations intact. Filesystem writes can still fail.
    for (const auto& artifact : artifacts) {
      const auto& options = configurations[artifact.configuration_index].options;
      const auto parent = artifact.file.path.parent_path();
      if (!parent.empty()) { std::filesystem::create_directories(parent); }
      rohit::serializer::compiler_output::write_if_different(artifact.file.path, artifact.file.contents);
      std::cout << "Generated " << artifact.file.path << " (" << artifact.language << ", "
                << (artifact.language == "java"
                        ? writer::java_coding_standard_name(options.java.standard)
                    : artifact.language == "cpp" ? writer::coding_standard_name(options.cpp.standard)
                                                : "native")
                << ")\n";
    }
    if (!depfile.empty()) {
      rohit::serializer::compiler_output::write_if_different(depfile, dependency_text);
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Serializer: " << error.what() << '\n';
    return 1;
  }
}
