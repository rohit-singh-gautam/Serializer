// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later

// Recognize a C++ declaration addition without consuming an ordinary native class block.
bool callable_prefix_head(const rohit::type_check::schema_input_buffer auto& input) {
  const auto probe = make_constant_full_stream(input.curr(), input.remaining_buffer());
  if (!next_keyword(probe, "cpp")) { return false; }
  static_cast<void>(parse_identifier_impl(probe));
  skip_whitespace_and_comment(probe);
  if (probe.full() || *probe != '{') { return false; }
  static_cast<void>(parse_native_block(probe, "cpp", access_type::public_access));
  skip_whitespace_and_comment(probe);
  return construction_head(probe) || behavior_modifier_head(probe, "function");
}

// Keep additions a closed qualifier contract instead of injecting arbitrary declaration tokens.
std::string parse_callable_prefix(const rohit::type_check::schema_input_buffer auto& input,
                                 access_type access) {
  if (active_language_version < language_version{1u, 14u, 0u}) {
    throw exception::bad_member_spec{input, "Callable additions require serializer version 1.14.0"};
  }
  static_cast<void>(parse_identifier_impl(input));
  auto block = parse_native_block(input, "cpp", access);
  const auto qualifiers = make_constant_full_stream(block.text.data(), block.text.size());
  std::string result;
  std::unordered_set<std::string> seen;
  skip_whitespace_and_comment(qualifiers);
  while (!qualifiers.full()) {
    const auto qualifier = parse_identifier_impl(qualifiers);
    if ((qualifier != "constexpr" && qualifier != "inline") || !seen.insert(qualifier).second) {
      throw exception::bad_member_spec{input, "C++ callable additions support constexpr and inline once each"};
    }
    result += qualifier + " ";
    skip_whitespace_and_comment(qualifiers);
  }
  if (result.empty()) { throw exception::bad_member_spec{input, "A callable addition cannot be empty"}; }
  skip_whitespace_and_comment(input);
  return result;
}
