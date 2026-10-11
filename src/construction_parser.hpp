// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later

// Reuse the class constraint grammar once its parser has been declared below this fragment.
generic_constraint parse_construction_constraint(const rohit::type_check::schema_input_buffer auto& input,
                                                namespace_node* space);

// Keep added construction contracts independent of the compiler/runtime release number.
void require_extended_construction(const rohit::type_check::schema_input_buffer auto& input) {
  if (active_language_version < language_version{1u, 11u, 0u}) {
    throw exception::bad_member_spec{input, "Extended construction requires serializer version 1.11.0"};
  }
}

// Distinguish declarations from existing user types named constructor or factory.
bool construction_head(const rohit::type_check::schema_input_buffer auto& input) {
  const auto probe = make_constant_full_stream(input.curr(), input.remaining_buffer());
  if (!next_keyword(probe, "constructor") && !next_keyword(probe, "factory")) { return false; }
  const auto keyword = parse_identifier_impl(probe);
  skip_whitespace_and_comment(probe);
  if (keyword == "factory" && !probe.full() && is_first_identifier(probe)) {
    parse_identifier_impl(probe);
    skip_whitespace_and_comment(probe);
  }
  return !probe.full() && (*probe == '(' || *probe == '<');
}

// Parse bounded portable owned construction; assignments have no wire identities.
construction_declaration parse_construction(const rohit::type_check::schema_input_buffer auto& input,
                                           access_type access, namespace_node* space) {
  if (active_language_version < language_version{1u, 8u, 0u}) {
    throw exception::bad_member_spec{input, "Portable construction requires serializer version 1.8.0"};
  }
  construction_declaration result;
  result.access = access;
  result.constructor = parse_identifier_impl(input) == "constructor";
  skip_whitespace_and_comment(input);
  if (!result.constructor) {
    result.name = parse_identifier_impl(input);
    skip_whitespace_and_comment(input);
  }
  if (!input.full() && *input == '<') {
    require_extended_construction(input);
    ++input;
    constexpr std::size_t maximum_input_types = 8;
    do {
      skip_whitespace_and_comment(input);
      generic_parameter parameter;
      parameter.name = parse_identifier_impl(input);
      if (result.generic_parameters.size() >= maximum_input_types ||
          !serializer::get_cpp_type_or_empty(parameter.name).empty() ||
          std::any_of(result.generic_parameters.begin(), result.generic_parameters.end(),
              [&](const auto& item) { return item.name == parameter.name; })) {
        throw exception::bad_member_spec{input, "Duplicate, reserved, or excessive construction input types"};
      }
      skip_whitespace_and_comment(input);
      check_and_increase(input, ':');
      parameter.constraint = parse_construction_constraint(input, space);
      result.generic_parameters.push_back(std::move(parameter));
      skip_whitespace_and_comment(input);
      if (input.full() || *input != ',') { break; }
      ++input;
    } while (true);
    check_and_increase(input, '>');
    skip_whitespace_and_comment(input);
  }
  check_and_increase(input, '(');
  skip_whitespace_and_comment(input);
  std::unordered_set<std::string> parameters;
  constexpr std::size_t maximum_parameters = 64;
  while (!input.full() && *input != ')') {
    function_parameter parameter;
    parameter.type = parse_type_expression(input, space);
    skip_whitespace_and_comment(input);
    if (parameter.type.name == "array" && !parameter.type.application && !input.full() && *input == '[') {
      require_extended_construction(input);
      parameter.modifier = member::modifier_type::array;
      ++input;
      skip_whitespace_and_comment(input);
      parameter.extent_expression.push_back(parse_dimension_expression(input));
      skip_whitespace_and_comment(input);
      check_and_increase(input, ']');
      skip_whitespace_and_comment(input);
      parameter.type = parse_type_expression(input, space);
      skip_whitespace_and_comment(input);
    }
    parameter.name = parse_identifier_impl(input);
    if (!parameters.insert(parameter.name).second || result.parameters.size() >= maximum_parameters) {
      throw exception::bad_member_spec{input, "Duplicate or excessive construction parameters"};
    }
    skip_whitespace_and_comment(input);
    if (!input.full() && *input == '=') {
      require_extended_construction(input);
      ++input;
      skip_whitespace_and_comment(input);
      if (next_keyword(input, "default")) {
        static_cast<void>(parse_identifier_impl(input));
        skip_whitespace_and_comment(input);
        check_and_increase(input, '(');
        skip_whitespace_and_comment(input);
        parameter.default_factory = parse_identifier_impl(input);
        skip_whitespace_and_comment(input);
        check_and_increase(input, ')');
      } else {
        const auto* begin = input.curr();
        char quote{};
        bool escaped{};
        constexpr std::size_t maximum_default_bytes = 4096;
        while (!input.full() && (quote || (*input != ',' && *input != ')'))) {
          const auto character = static_cast<char>(*input);
          if (input.curr() - begin >= static_cast<std::ptrdiff_t>(maximum_default_bytes)) {
            throw exception::bad_member_spec{input, "Construction parameter default exceeds 4096 bytes"};
          }
          if (quote) {
            if (escaped) { escaped = false; }
            else if (character == '\\') { escaped = true; }
            else if (character == quote) { quote = 0; }
          } else if (character == '\'' || character == '"') { quote = character; }
          ++input;
        }
        parameter.default_value = schema_version::trim(std::string_view{
            reinterpret_cast<const char*>(begin), static_cast<std::size_t>(input.curr() - begin)});
        if (parameter.default_value.empty()) {
          throw exception::bad_member_spec{input, "Construction parameter default cannot be empty"};
        }
      }
      skip_whitespace_and_comment(input);
    } else if (!result.parameters.empty() &&
        (!result.parameters.back().default_value.empty() || !result.parameters.back().default_factory.empty())) {
      throw exception::bad_member_spec{input, "All trailing construction parameters require defaults"};
    }
    result.parameters.push_back(std::move(parameter));
    if (*input != ',') { break; }
    ++input;
    skip_whitespace_and_comment(input);
    if (*input == ')') { throw exception::bad_member_spec{input, "Trailing construction parameter comma"}; }
  }
  check_and_increase(input, ')');
  skip_whitespace_and_comment(input);
  if (next_keyword(input, "explicit")) {
    require_extended_construction(input);
    if (!result.constructor) { throw exception::bad_member_spec{input, "Only a constructor can be explicit"}; }
    static_cast<void>(parse_identifier_impl(input));
    result.explicit_constructor = true;
    skip_whitespace_and_comment(input);
  }
  check_and_increase(input, '{');
  skip_whitespace_and_comment(input);
  std::unordered_set<std::string> fields;
  constexpr std::size_t maximum_assignments = 1024;
  while (!input.full() && *input != '}') {
    construction_assignment assignment;
    assignment.field = parse_identifier_impl(input);
    if (!fields.insert(assignment.field).second || result.assignments.size() >= maximum_assignments) {
      throw exception::bad_member_spec{input, "Duplicate or excessive construction field assignments"};
    }
    skip_whitespace_and_comment(input);
    check_and_increase(input, '=');
    skip_whitespace_and_comment(input);
    if (next_keyword(input, "default")) {
      parse_identifier_impl(input);
      skip_whitespace_and_comment(input);
      check_and_increase(input, '(');
      skip_whitespace_and_comment(input);
      assignment.value = parse_identifier_impl(input);
      skip_whitespace_and_comment(input);
      check_and_increase(input, ')');
      assignment.factory_default = true;
    } else {
      // Reuse the balanced literal scanner, preserving quoted whitespace and rejecting native code later.
      const auto* begin = input.curr();
      char quote{};
      bool escaped{};
      constexpr std::size_t maximum_literal_bytes = 4096;
      while (!input.full() && (quote || *input != ';')) {
        const auto character = static_cast<char>(*input);
        if (input.curr() - begin >= static_cast<std::ptrdiff_t>(maximum_literal_bytes)) {
          throw exception::bad_member_spec{input, "Construction literal exceeds 4096 bytes"};
        }
        if (quote) {
          if (escaped) { escaped = false; }
          else if (character == '\\') { escaped = true; }
          else if (character == quote) { quote = 0; }
        } else if (character == '\'' || character == '"') { quote = character; }
        else if (character == '{' || character == '}') {
          throw exception::bad_member_spec{input, "Construction accepts parameters, scalar literals, or default(factory)"};
        }
        ++input;
      }
      assignment.value = schema_version::trim(std::string_view{
          reinterpret_cast<const char*>(begin), static_cast<std::size_t>(input.curr() - begin)});
      assignment.parameter = parameters.contains(assignment.value);
      if (!assignment.parameter && active_language_version >= language_version{1u, 11u, 0u} &&
          !assignment.value.empty() && assignment.value.front() != '\'' && assignment.value.front() != '"' &&
          assignment.value.find("::") == std::string::npos) {
        const auto expression_input = make_constant_full_stream(begin,
            static_cast<std::size_t>(input.curr() - begin));
        assignment.expression = behavior_expression_parser{expression_input, 0, 0, space}.conditional();
        skip_whitespace_and_comment(expression_input);
        if (!expression_input.full()) {
          throw exception::bad_member_spec{input, "Unexpected token in construction binding"};
        }
      }
    }
    check_and_increase(input, ';');
    result.assignments.push_back(std::move(assignment));
    skip_whitespace_and_comment(input);
  }
  check_and_increase(input, '}');
  return result;
}
