// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later

// Preserve ordinary fields whose user type happens to be named conversion.
bool conversion_head(const rohit::type_check::schema_input_buffer auto& input) {
  const auto probe = make_constant_full_stream(input.curr(), input.remaining_buffer());
  if (!next_keyword(probe, "conversion")) { return false; }
  parse_identifier_impl(probe);
  skip_whitespace_and_comment(probe);
  if (probe.full() || !is_first_identifier(probe)) { return false; }
  parse_type_expression(probe, nullptr);
  skip_whitespace_and_comment(probe);
  return next_keyword(probe, "as");
}

// Parse a bounded explicit conversion with side-effect-free dotted source projections.
conversion_declaration parse_conversion(const rohit::type_check::schema_input_buffer auto& input,
    access_type access, namespace_node* space) {
  if (active_language_version < language_version{1u, 9u, 0u}) {
    throw exception::bad_member_spec{input, "Portable conversions require serializer version 1.9.0"};
  }
  conversion_declaration result;
  result.access = access;
  parse_identifier_impl(input);
  skip_whitespace_and_comment(input);
  result.target = parse_type_expression(input, space);
  skip_whitespace_and_comment(input);
  if (parse_identifier_impl(input) != "as") { throw exception::bad_member_spec{input, "Expected conversion target as name"}; }
  skip_whitespace_and_comment(input);
  result.name = parse_identifier_impl(input);
  skip_whitespace_and_comment(input);
  check_and_increase(input, '{');
  skip_whitespace_and_comment(input);
  std::unordered_set<std::string> fields;
  constexpr std::size_t maximum_assignments = 1024;
  constexpr std::size_t maximum_projection_depth = 32;
  while (!input.full() && *input != '}') {
    conversion_assignment assignment;
    assignment.field = parse_identifier_impl(input);
    if (!fields.insert(assignment.field).second || result.assignments.size() >= maximum_assignments) {
      throw exception::bad_member_spec{input, "Duplicate or excessive conversion assignments"};
    }
    skip_whitespace_and_comment(input);
    check_and_increase(input, '=');
    skip_whitespace_and_comment(input);
    for (;;) {
      if (assignment.source_path.size() >= maximum_projection_depth) {
        throw exception::bad_member_spec{input, "Conversion projection depth exceeds 32"};
      }
      assignment.source_path.push_back(parse_identifier_impl(input));
      skip_whitespace_and_comment(input);
      if (input.full() || *input != '.') { break; }
      ++input;
      skip_whitespace_and_comment(input);
    }
    check_and_increase(input, ';');
    result.assignments.push_back(std::move(assignment));
    skip_whitespace_and_comment(input);
  }
  check_and_increase(input, '}');
  skip_whitespace_and_comment(input);
  check_and_increase(input, ';');
  return result;
}
