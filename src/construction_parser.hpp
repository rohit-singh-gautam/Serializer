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

// Recognize fresh value-initialization without treating comments as aggregate elements.
bool empty_construction_default(std::string_view text) {
  const auto input = make_constant_full_stream(text.data(), text.size());
  skip_whitespace_and_comment(input);
  if (input.full() || *input != '{') { return false; }
  ++input;
  skip_whitespace_and_comment(input);
  if (input.full() || *input != '}') { return false; }
  ++input;
  skip_whitespace_and_comment(input);
  return input.full();
}

// Parse bounded portable owned construction; assignments have no wire identities.
construction_declaration parse_construction(const rohit::type_check::schema_input_buffer auto& input,
                                           access_type access, namespace_node* space,
                                           const class_node& owner) {
  if (active_language_version < language_version{1u, 8u, 0u}) {
    throw exception::bad_member_spec{input, "Portable construction requires serializer version 1.8.0"};
  }
  construction_declaration result;
  result.access = access;
  result.overload_contract_enabled = active_language_version >= language_version{1u, 12u, 0u};
  result.forward_signature_enabled = active_language_version >= language_version{1u, 14u, 0u};
  result.member_initialization_enabled = active_language_version >= language_version{1u, 14u, 0u};
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
      if (!input.full() && *input == ':') {
        ++input;
        parameter.constraint = parse_construction_constraint(input, space);
      } else if (active_language_version < language_version{1u, 17u, 0u}) {
        throw exception::bad_member_spec{input, "Unconstrained delegating constructor inputs require serializer version 1.17.0"};
      }
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
    parameter.portable_default_enabled = result.member_initialization_enabled;
    parameter.empty_record_default_enabled = active_language_version >= language_version{1u, 15u, 0u};
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
    const bool owner_type_parameter = parameter.modifier == member::modifier_type::none &&
        !parameter.type.application &&
        std::any_of(owner.generic_parameters.begin(), owner.generic_parameters.end(),
            [&](const auto& item) {
              return item.kind == generic_argument_kind::type && item.name == parameter.type.name;
            });
    parameter.owner_parameter_default_enabled = owner_type_parameter &&
        active_language_version >= language_version{1u, 16u, 0u};
    if (parameter.modifier == member::modifier_type::none && !parameter.type.application &&
        active_language_version >= language_version{1u, 24u, 0u} &&
        std::any_of(result.generic_parameters.begin(), result.generic_parameters.end(),
            [&](const auto& item) { return item.name == parameter.type.name; })) {
      parameter.owner_parameter_default_enabled = true;
    }
    parameter.name = parse_identifier_impl(input);
    if (!parameters.insert(parameter.name).second || result.parameters.size() >= maximum_parameters) {
      throw exception::bad_member_spec{input, "Duplicate or excessive construction parameters"};
    }
    skip_whitespace_and_comment(input);
    if (next_keyword(input, "const_ref")) {
      if (active_language_version < language_version{1u, 17u, 0u}) {
        throw exception::bad_member_spec{input, "Borrowed constructor const_ref inputs require serializer version 1.17.0"};
      }
      parameter.const_ref = true;
      static_cast<void>(parse_identifier_impl(input));
      skip_whitespace_and_comment(input);
    }
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
        char comment{};
        bool escaped{};
        std::size_t aggregate_depth{};
        constexpr std::size_t maximum_default_bytes = 4096;
        while (!input.full() && (quote || comment || aggregate_depth || (*input != ',' && *input != ')'))) {
          const auto character = static_cast<char>(*input);
          if (input.curr() - begin >= static_cast<std::ptrdiff_t>(maximum_default_bytes)) {
            throw exception::bad_member_spec{input, "Construction parameter default exceeds 4096 bytes"};
          }
          if (comment) {
            if (comment == '/' && character == '\n') { comment = 0; }
            else if (comment == '*' && character == '*' && input.remaining_buffer() >= 2 &&
                input.curr()[1] == '/') {
              if (input.curr() - begin >= static_cast<std::ptrdiff_t>(maximum_default_bytes - 1)) {
                throw exception::bad_member_spec{input, "Construction parameter default exceeds 4096 bytes"};
              }
              input.advance_unchecked(2);
              comment = 0;
              continue;
            }
          } else if (quote) {
            if (escaped) { escaped = false; }
            else if (character == '\\') { escaped = true; }
            else if (character == quote) { quote = 0; }
          } else if (character == '/' && input.remaining_buffer() >= 2 &&
              (input.curr()[1] == '/' || input.curr()[1] == '*')) {
            // Comment contents consume the same byte budget but cannot alter delimiter or quote state.
            if (input.curr() - begin >= static_cast<std::ptrdiff_t>(maximum_default_bytes - 1)) {
              throw exception::bad_member_spec{input, "Construction parameter default exceeds 4096 bytes"};
            }
            comment = static_cast<char>(input.curr()[1]);
            input.advance_unchecked(2);
            continue;
          } else if (character == '\'' || character == '"') { quote = character; }
          else if (character == '{') {
            if (!result.member_initialization_enabled || ++aggregate_depth > 32) {
              throw exception::bad_member_spec{input, "Aggregate defaults require serializer version 1.14.0 and at most 32 levels"};
            }
          } else if (character == '}') {
            if (!aggregate_depth) { throw exception::bad_member_spec{input, "Unbalanced aggregate parameter default"}; }
            --aggregate_depth;
          }
          ++input;
        }
        if (comment == '*') { throw exception::bad_input_data{input, "Unterminated schema comment"}; }
        parameter.default_value = schema_version::trim(std::string_view{
            reinterpret_cast<const char*>(begin), static_cast<std::size_t>(input.curr() - begin)});
        if (parameter.default_value.empty()) {
          throw exception::bad_member_spec{input, "Construction parameter default cannot be empty"};
        }
        if (owner_type_parameter && !parameter.owner_parameter_default_enabled &&
            empty_construction_default(parameter.default_value)) {
          throw exception::bad_member_spec{input,
              "Empty owner-parameter defaults require serializer version 1.16.0"};
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
  if (next_keyword(input, "when")) { result.guard = parse_callable_guard(input, space); }
  if (next_keyword(input, "delegates")) {
    if (!result.constructor || active_language_version < language_version{1u, 17u, 0u}) {
      throw exception::bad_member_spec{input, "Constructor delegation requires serializer version 1.17.0"};
    }
    result.delegating = true;
    static_cast<void>(parse_identifier_impl(input));
    skip_whitespace_and_comment(input);
    check_and_increase(input, '(');
    skip_whitespace_and_comment(input);
    constexpr std::size_t maximum_delegation_arguments = 64;
    while (!input.full() && *input != ')') {
      if (result.delegation.size() >= maximum_delegation_arguments) {
        throw exception::bad_member_spec{input, "Constructor delegation exceeds 64 arguments"};
      }
      result.delegation.push_back(behavior_expression_parser{input, 0, 0, space}.conditional());
      skip_whitespace_and_comment(input);
      if (input.full() || *input != ',') { break; }
      ++input;
      skip_whitespace_and_comment(input);
      if (!input.full() && *input == ')') {
        throw exception::bad_member_spec{input, "Trailing constructor delegation argument comma"};
      }
    }
    check_and_increase(input, ')');
    skip_whitespace_and_comment(input);
    check_and_increase(input, ';');
    return result;
  }
  if (std::any_of(result.generic_parameters.begin(), result.generic_parameters.end(),
      [](const auto& parameter) { return !parameter.constraint; })) {
    throw exception::bad_member_spec{input, "Unconstrained independent inputs require a delegating constructor"};
  }
  if (!input.full() && (*input == ';' || *input == '=')) {
    if (!result.forward_signature_enabled || !result.constructor) {
      throw exception::bad_member_spec{input,
          "Constructor declarations require serializer version 1.14.0"};
    }
    result.declaration_only = true;
    if (*input == '=') {
      ++input;
      skip_whitespace_and_comment(input);
      if (parse_identifier_impl(input) != "default" || !result.parameters.empty() ||
          !result.generic_parameters.empty() || result.explicit_constructor) {
        throw exception::bad_member_spec{input, "Only a zero-argument constructor may be defaulted"};
      }
      result.defaulted = true;
      skip_whitespace_and_comment(input);
    }
    check_and_increase(input, ';');
    return result;
  }
  if (result.constructor && result.parameters.empty() &&
      active_language_version < language_version{1u, 17u, 0u}) {
    throw exception::bad_member_spec{input, "Zero-argument constructor bindings require serializer version 1.17.0"};
  }
  check_and_increase(input, '{');
  skip_whitespace_and_comment(input);
  std::unordered_set<std::string> fields;
  constexpr std::size_t maximum_assignments = 1024;
  while (!input.full() && *input != '}') {
    const auto directive = make_constant_full_stream(input.curr(), input.remaining_buffer());
    const auto name = parse_identifier_impl(directive);
    skip_whitespace_and_comment(directive);
    if ((name == "validate" || name == "initialize") && !directive.full() && *directive != '=') {
      if (!result.overload_contract_enabled) {
        throw exception::bad_member_spec{input, "Authoring validation and initialization require serializer version 1.12.0"};
      }
      static_cast<void>(parse_identifier_impl(input));
      skip_whitespace_and_comment(input);
      if (name == "validate" && next_keyword(input, "expression")) {
        if (!result.constructor || !result.native_validations.empty() || !result.initializers.empty()) {
          throw exception::bad_member_spec{input, "Validation must precede host initialization"};
        }
        constexpr std::size_t maximum_validation_checks = 64;
        if (result.validations.size() >= maximum_validation_checks) {
          throw exception::bad_member_spec{input, "Construction exceeds 64 validation checks"};
        }
        static_cast<void>(parse_identifier_impl(input));
        skip_whitespace_and_comment(input);
        check_and_increase(input, '(');
        result.validations.push_back(behavior_expression_parser{input, 0, 0, space}.conditional());
        skip_whitespace_and_comment(input);
        check_and_increase(input, ')');
      } else {
        const auto language = parse_identifier_impl(input);
        if (language != "cpp" || !result.constructor) {
          throw exception::bad_member_spec{input, "Authoring host hooks require a C++ constructor"};
        }
        auto& blocks = name == "initialize" ? result.initializers : result.native_validations;
        if (!blocks.empty() || (name == "validate" && !result.initializers.empty())) {
          throw exception::bad_member_spec{input, "Repeated or misordered authoring host hook"};
        }
        blocks.push_back(parse_native_block(input, language, access));
      }
      skip_whitespace_and_comment(input);
      check_and_increase(input, ';');
      skip_whitespace_and_comment(input);
      continue;
    }
    if (!result.validations.empty() || !result.native_validations.empty() || !result.initializers.empty()) {
      throw exception::bad_member_spec{input, "Construction bindings must precede validation and initialization"};
    }
    construction_assignment assignment;
    assignment.field = parse_identifier_impl(input);
    if (!fields.insert(assignment.field).second || result.assignments.size() >= maximum_assignments) {
      throw exception::bad_member_spec{input, "Duplicate or excessive construction field assignments"};
    }
    skip_whitespace_and_comment(input);
    check_and_increase(input, '=');
    skip_whitespace_and_comment(input);
    if (!input.full() && *input == '{') {
      if (active_language_version < language_version{1u, 17u, 0u}) {
        throw exception::bad_member_spec{input, "Array construction bindings require serializer version 1.17.0"};
      }
      assignment.array_binding = true;
      ++input;
      skip_whitespace_and_comment(input);
      constexpr std::size_t maximum_array_bindings = 65536;
      while (!input.full() && *input != '}') {
        construction_assignment element;
        element.expression = behavior_expression_parser{input, 0, 0, space}.conditional();
        if (element.expression->kind == behavior_expression::operation::symbol &&
            parameters.contains(element.expression->value)) {
          element.parameter = true;
          element.value = element.expression->value;
          element.expression.reset();
        }
        assignment.elements.push_back(std::move(element));
        if (assignment.elements.size() > maximum_array_bindings) {
          throw exception::bad_member_spec{input, "Array construction exceeds 65536 elements"};
        }
        skip_whitespace_and_comment(input);
        if (input.full() || *input != ',') { break; }
        ++input;
        skip_whitespace_and_comment(input);
        if (!input.full() && *input == '}') {
          throw exception::bad_member_spec{input, "Trailing array construction comma"};
        }
      }
      check_and_increase(input, '}');
      skip_whitespace_and_comment(input);
    } else if (next_keyword(input, "generate")) {
      if (active_language_version < language_version{1u, 17u, 0u}) {
        throw exception::bad_member_spec{input, "Array generation requires serializer version 1.17.0"};
      }
      assignment.array_binding = true;
      static_cast<void>(parse_identifier_impl(input));
      skip_whitespace_and_comment(input);
      check_and_increase(input, '(');
      skip_whitespace_and_comment(input);
      do {
        const auto index = parse_identifier_impl(input);
        if (parameters.contains(index) || std::find(assignment.generator_indices.begin(),
            assignment.generator_indices.end(), index) != assignment.generator_indices.end() ||
            assignment.generator_indices.size() >= 8) {
          throw exception::bad_member_spec{input, "Array generation requires at most eight independent index names"};
        }
        assignment.generator_indices.push_back(index);
        skip_whitespace_and_comment(input);
        if (input.full() || *input != ',') { break; }
        ++input;
        skip_whitespace_and_comment(input);
      } while (true);
      check_and_increase(input, '=');
      check_and_increase(input, '>');
      assignment.expression = behavior_expression_parser{input, 0, 0, space}.conditional();
      skip_whitespace_and_comment(input);
      check_and_increase(input, ')');
      skip_whitespace_and_comment(input);
    } else if (next_keyword(input, "default")) {
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
          (assignment.value.find("::") == std::string::npos ||
           (result.overload_contract_enabled && assignment.value.find('(') != std::string::npos))) {
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
