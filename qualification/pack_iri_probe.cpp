// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/package.hpp>

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

// Hex transport keeps Unicode/invalid-byte corpus inputs independent of the Windows command-line codepage.
std::string decode_hex(std::string_view value) {
  if (value.size() % 2U != 0U) { throw std::invalid_argument{"Odd hex argument length"}; }
  const auto digit = [](char character) -> unsigned {
    if (character >= '0' && character <= '9') { return static_cast<unsigned>(character - '0'); }
    if (character >= 'a' && character <= 'f') { return static_cast<unsigned>(character - 'a') + 10U; }
    throw std::invalid_argument{"Invalid hex argument"};
  };
  std::string result{};
  result.reserve(value.size() / 2U);
  for (std::size_t index{}; index < value.size(); index += 2U) {
    result += static_cast<char>((digit(value[index]) << 4U) | digit(value[index + 1U]));
  }
  return result;
}

// Expose exact native results to an independent Python/RFC corpus without any archive/network side effects.
int main(int argc, const char* const* argv) {
  try {
    namespace package = rohit::serializer::package;
    if (argc < 3) { throw std::invalid_argument{"Expected compose, parse or resolve and its arguments"}; }
    std::string_view selected{argv[1]};
    const bool hexadecimal = selected.starts_with("hex-");
    if (hexadecimal) { selected.remove_prefix(4U); }
    const auto first = hexadecimal ? decode_hex(argv[2]) : std::string{argv[2]};
    const auto second = argc == 4 ? (hexadecimal ? decode_hex(argv[3]) : std::string{argv[3]}) : std::string{};
    if (selected == "compose" && argc == 4) { std::cout << package::compose_pack_iri(first, second); }
    else if (selected == "resolve" && argc == 4) { std::cout << package::resolve_pack_reference(first, second); }
    else if (selected == "parse" && argc == 3) {
      const auto value = package::parse_pack_iri(first);
      std::cout << value.package_iri << '\n' << value.part_name << '\n';
      std::cout << (value.query ? "present:" : "absent:") << value.query.value_or("") << '\n';
      std::cout << (value.fragment ? "present:" : "absent:") << value.fragment.value_or("") << '\n';
    } else { throw std::invalid_argument{"Invalid pack IRI probe arguments"}; }
    return std::cout ? 0 : 1;
  } catch (const std::exception& failure) {
    std::cerr << failure.what() << '\n';
    return 2;
  }
}
