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
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>
#include <rohit/version.hpp>

#include "command_line.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
// Shared descriptors implement the Chaturanga-style short/long command-line interface.
constexpr rohit::serializer::cli::commandline_option command_options[] = {
    {'h', "help", "", "Display this help."},
    {'v', "version", "", "Display compiler and supported schema language versions."},
    {'i', "input", "schema.serializer", "Input with a serializer version 1; header."},
    {'o', "output", "file", "Output path when selecting exactly one language."},
    {'c', "config", "file.ini", "Generator configuration; CLI options override its values."},
    {'l', "language", "cpp|java", "Select output languages; repeat or comma-separate values.",
     true},
    {'\0', "cpp.output", "file.hpp", "C++ output path; required for multi-language generation."},
    {'\0', "java.output", "ClassName.java",
     "Java output path; required for multi-language generation."},
    {'\0', "cpp.coding_standard", "profile",
     "serializer|core|google|llvm|gnu|cert|misra|autosar|qt"},
    {'\0', "cpp.naming", "profile|preserve", "C++ identifier naming policy."},
    {'\0', "cpp.format", "true|false", "Run clang-format (default true)."},
    {'\0', "cpp.clang_format", "executable", "clang-format 19+ executable (default clang-format)."},
    {'\0', "cpp.format_file", "file", "Custom layout; --cpp.format_file= clears it.", false, true},
    {'\0', "java.coding_standard", "profile", "serializer|google|oracle"},
    {'\0', "java.naming", "profile|preserve", "Java identifier naming policy."},
    {'\0', "java.package", "package.name", "Java package; --java.package= clears it.", false, true},
};

// Detect aliases and hard links as well as identical normalized paths before replacing output.
bool same_file(const std::filesystem::path& first, const std::filesystem::path& second) {
  if (std::filesystem::exists(first) && std::filesystem::exists(second) &&
      std::filesystem::equivalent(first, second)) {
    return true;
  }
  return std::filesystem::weakly_canonical(first) == std::filesystem::weakly_canonical(second);
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
                << "\nSupported schema language version: " << schema_language_version << '\n';
      return 0;
    }
    std::map<std::string, std::string> arguments{};
    for (const auto& [key, values] : parsed) {
      arguments.emplace(key, values.front());
    }
    if (!arguments.contains("input")) {
      throw std::invalid_argument{"--input is required; use --help for options"};
    }
    auto options = arguments.contains("config")
                       ? writer::read_output_options(arguments.at("config"))
                       : writer::output_options{};
    if (parsed.contains("language")) {
      options.language.clear();
      for (const auto& value : parsed.at("language")) {
        if (!options.language.empty()) {
          options.language += ',';
        }
        options.language += value;
      }
    }
    const auto languages = writer::parse_output_languages(options.language);
    if (arguments.contains("java.coding_standard")) {
      options.java.standard =
          writer::parse_java_coding_standard(arguments.at("java.coding_standard"));
    }
    if (arguments.contains("java.package")) {
      options.java.package_name = arguments.at("java.package");
    }
    if (arguments.contains("java.naming")) {
      const auto& value = arguments.at("java.naming");
      if (value != "profile" && value != "preserve") {
        throw std::invalid_argument{"java.naming must be profile or preserve"};
      }
      options.java.rename_identifiers = value == "profile";
    }
    if (arguments.contains("cpp.coding_standard")) {
      options.cpp.standard = writer::parse_coding_standard(arguments.at("cpp.coding_standard"));
    }
    if (arguments.contains("cpp.naming")) {
      const auto& value = arguments.at("cpp.naming");
      if (value != "profile" && value != "preserve") {
        throw std::invalid_argument{"cpp.naming must be profile or preserve"};
      }
      options.cpp.rename_identifiers = value == "profile";
    }
    if (arguments.contains("cpp.format")) {
      const auto& value = arguments.at("cpp.format");
      if (value != "true" && value != "false") {
        throw std::invalid_argument{"cpp.format must be true or false"};
      }
      options.cpp.format = value == "true";
    }
    if (arguments.contains("cpp.clang_format")) {
      options.cpp.clang_format = arguments.at("cpp.clang_format");
    }
    if (arguments.contains("cpp.format_file")) {
      options.cpp.format_file = arguments.at("cpp.format_file");
    }
    if (!options.cpp.format && !options.cpp.format_file.empty()) {
      throw std::invalid_argument{"cpp.format_file requires cpp.format true"};
    }
    const std::filesystem::path input_file{arguments.at("input")};
    if (input_file.extension() != ".serializer") {
      throw std::invalid_argument{
          "Input must use .serializer; rename the schema and add serializer version 1;"};
    }
    if (arguments.contains("output") && languages.size() != 1) {
      throw std::invalid_argument{"Use --cpp.output and --java.output for multiple languages"};
    }
    for (const auto language : {"cpp", "java"}) {
      if (arguments.contains(std::string{language} + ".output") &&
          std::find(languages.begin(), languages.end(), language) == languages.end()) {
        throw std::invalid_argument{"Output path supplied for unselected language: " +
                                    std::string{language}};
      }
    }
    std::vector<std::filesystem::path> destinations{};
    for (const auto& language : languages) {
      const auto key = language + ".output";
      if (arguments.contains("output") && arguments.contains(key)) {
        throw std::invalid_argument{"Specify only one of --output and --" + key};
      }
      if (!arguments.contains("output") && !arguments.contains(key)) {
        throw std::invalid_argument{"Missing output path: --" + key};
      }
      const std::filesystem::path output_file{
          arguments.at(arguments.contains("output") ? "output" : key)};
      if (same_file(input_file, output_file) ||
          (arguments.contains("config") && same_file(arguments.at("config"), output_file)) ||
          (!options.cpp.format_file.empty() && same_file(options.cpp.format_file, output_file))) {
        throw std::invalid_argument{
            "Output must not overwrite the schema or an output configuration"};
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
      const auto parent = output_file.parent_path();
      if ((!parent.empty() && !std::filesystem::is_directory(parent)) ||
          std::filesystem::is_directory(output_file)) {
        throw std::invalid_argument{
            "Output requires an existing parent directory and a file path: " +
            output_file.string()};
      }
      destinations.push_back(output_file);
    }
    const auto input = rohit::make_stream_from_file(input_file);
    const auto statements = parser::parse(input, true);
    std::vector<std::unique_ptr<rohit::full_stream_auto_alloc>> outputs{};
    for (std::size_t index = 0; index < languages.size(); ++index) {
      auto output = std::make_unique<rohit::full_stream_auto_alloc>();
      if (languages[index] == "java") {
        writer::java::write(*output, statements, destinations[index].stem().string(), options.java);
      } else {
        writer::cpp::write(*output, statements, options.cpp);
      }
      outputs.push_back(std::move(output));
    }
    // Backend/formatter failures leave all destinations intact. Filesystem writes can still fail.
    for (std::size_t index = 0; index < languages.size(); ++index) {
      outputs[index]->write_to_file_till_offset(destinations[index]);
      std::cout << "Generated " << destinations[index] << " (" << languages[index] << ", "
                << (languages[index] == "java"
                        ? writer::java_coding_standard_name(options.java.standard)
                        : writer::coding_standard_name(options.cpp.standard))
                << ")\n";
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Serializer: " << error.what() << '\n';
    return 1;
  }
}
