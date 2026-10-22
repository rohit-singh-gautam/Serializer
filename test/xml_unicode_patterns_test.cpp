// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <xml_unicode_patterns.hpp>
#include <rohit/xml.hpp>

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace xml = rohit::serializer::xml;

// Read untouched fixture bytes produced independently of every generated model codec.
static std::string read_file(const std::string& path) {
  std::ifstream input{path, std::ios::binary};
  if (!input) { throw std::runtime_error{"Cannot open pattern fixture"}; }
  return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

// Exercise generated C++ input/output against independently classified original documents.
int main(int argc, char** argv) {
  for (int index{1}; index < argc; ++index) {
    const std::string path{argv[index]};
    const bool valid = path.ends_with(".valid.xml");
    const auto document = read_file(path), text = read_file(path + ".txt");
    bool accepted{};
    try {
      const auto value = xml::deserialize<patterns::sample>(document);
      accepted = true;
      if (value.value != text) { throw std::logic_error{"Pattern scalar differs"}; }
    } catch (const std::invalid_argument&) {
      if (valid) { throw; }
    }
    if (accepted != valid) { throw std::runtime_error{"C++ pattern input oracle differs: " + path}; }
    patterns::sample candidate{};
    candidate.value = text;
    accepted = false;
    std::string output{};
    try { output = xml::serialize(candidate); accepted = true; }
    catch (const std::invalid_argument&) { if (valid) { throw; } }
    if (accepted != valid || (!valid && !output.empty())) {
      throw std::runtime_error{"C++ pattern output oracle differs: " + path};
    }
    if (valid) {
      std::ofstream stream{path + ".cpp.xml", std::ios::binary | std::ios::trunc};
      stream.exceptions(std::ios::badbit | std::ios::failbit);
      stream.write(output.data(), static_cast<std::streamsize>(output.size()));
    }
  }
  std::cout << "C++ generated Unicode pattern input/output contracts pass\n";
}
