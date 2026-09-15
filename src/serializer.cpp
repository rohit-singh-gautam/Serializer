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

#include <rohit/serializer.hpp>
#include <rohit/serializer_creator.hpp>

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

// Print command usage and an optional diagnostic.
void display_help(const std::string& err) {
  std::cout << "Usage: Serializer input <input filename> output <output filename>" << std::endl;
  if (!err.empty()) {
    std::cout << "Error: " << err << std::endl;
  }
}

// Write the argument list in the command diagnostic format.
std::ostream& operator<<(std::ostream& os, const std::vector<std::string>& strings) {
  os << "{ ";
  for (auto& str : strings) {
    os << str << ' ';
  }
  os << '}';
  return os;
}

int main(const int argc, const char* argv[]) {
  const std::vector<std::string> args{argv, argv + argc};
  std::filesystem::path input_file{};
  std::filesystem::path output_file{};
  for (std::size_t argument_index{0}; argument_index < args.size(); ++argument_index) {
    if (args[argument_index] == "input") {
      ++argument_index;
      if (argument_index >= args.size()) {
        display_help("Insufficient arguments");
        return 0;
      }
      input_file = std::filesystem::path{args[argument_index]};
      if (!std::filesystem::exists(input_file)) {
        display_help("input file does not exists");
        return 0;
      }
      std::cout << "Input File: " << input_file << std::endl;
    } else if (args[argument_index] == "output") {
      ++argument_index;
      if (argument_index >= args.size()) {
        display_help("Insufficient arguments");
        return 0;
      }
      output_file = std::filesystem::path{args[argument_index]};
      std::cout << "Output File: " << output_file << std::endl;
    }
  }

  if (input_file.empty() || output_file.empty()) {
    display_help("Input and output both parameters are required.");
    std::cout << "Param: " << args << std::endl;
    return 0;
  }

  auto in_stream = rohit::make_stream_from_file(input_file);
  constexpr std::size_t initial_output_capacity_bytes = 256;
  rohit::full_stream_auto_alloc out_stream{initial_output_capacity_bytes};

  const bool output_is_header = output_file.extension() == ".h" ||
                                output_file.extension() == ".hpp" ||
                                output_file.extension() == ".hxx";
  if (!output_is_header) {
    std::cout << "WARNING: Output file is designed for C++ header, output extension must be one of "
                 ".h, .hpp or .hxx"
              << std::endl;
  }

  try {
    auto statements = rohit::serializer::parser::parse(in_stream);
    rohit::serializer::writer::cpp::write(out_stream, statements);
    out_stream.write_to_file_till_offset(output_file);
  } catch (const std::exception& e) {
    std::cout << "Failed to parse with error:\n" << e.what() << std::endl;
  }

  return 0;
}
