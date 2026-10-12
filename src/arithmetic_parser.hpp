// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later

// Parse an explicit operator mapping; inline expressions state their owning result and binary operand.
void parse_arithmetic(const rohit::type_check::schema_input_buffer auto& input,
                      class_node& owner, bool keyword_consumed = false) {
  if (active_language_version < language_version{1u, 14u, 0u}) {
    throw exception::bad_member_spec{input, "Arithmetic mappings require serializer version 1.14.0"};
  }
  if (!keyword_consumed) { static_cast<void>(parse_identifier_impl(input)); }
  skip_whitespace_and_comment(input);
  check_and_increase(input, '(');
  skip_whitespace_and_comment(input);
  arithmetic_declaration mapping;
  const auto form = parse_identifier_impl(input);
  if (form != "unary" && form != "binary") {
    throw exception::bad_member_spec{input, "Arithmetic mapping requires unary or binary"};
  }
  mapping.unary = form == "unary";
  skip_whitespace_and_comment(input);
  check_and_increase(input, '(');
  skip_whitespace_and_comment(input);
  if (input.full()) { throw exception::bad_member_spec{input, "Missing arithmetic operator"}; }
  mapping.symbol.assign(1, static_cast<char>(*input));
  ++input;
  if (mapping.unary ? mapping.symbol != "+" && mapping.symbol != "-" :
      mapping.symbol != "+" && mapping.symbol != "-" && mapping.symbol != "*" &&
      mapping.symbol != "/" && mapping.symbol != "%") {
    throw exception::bad_member_spec{input, "Unsupported arithmetic operator"};
  }
  function_declaration helper;
  helper.access = access_type::private_access;
  helper.source_offset = behavior_source_offset(input);
  skip_whitespace_and_comment(input);
  if (!input.full() && *input == ',') {
    if (mapping.unary) {
      throw exception::bad_member_spec{input, "Unary arithmetic does not take a right operand"};
    }
    ++input;
    skip_whitespace_and_comment(input);
    function_parameter parameter;
    parameter.type = parse_type_expression(input, owner.parent_namespace);
    skip_whitespace_and_comment(input);
    parameter.name = parse_identifier_impl(input);
    helper.parameters.push_back(std::move(parameter));
    skip_whitespace_and_comment(input);
  }
  check_and_increase(input, ')');
  skip_whitespace_and_comment(input);
  check_and_increase(input, '-');
  check_and_increase(input, '>');
  skip_whitespace_and_comment(input);
  if (next_keyword(input, "expression")) {
    mapping.inline_expression = true;
    static_cast<void>(parse_identifier_impl(input));
    skip_whitespace_and_comment(input);
    check_and_increase(input, '(');
    skip_whitespace_and_comment(input);
    // The outer construction spells the result explicitly. No field-based unit inference occurs.
    const auto probe = make_constant_full_stream(input.curr(), input.remaining_buffer());
    helper.return_type = parse_type_expression(probe, owner.parent_namespace);
    skip_whitespace_and_comment(probe);
    if (probe.full() || *probe != '(' || (!mapping.unary && helper.parameters.empty())) {
      throw exception::bad_member_spec{input,
          "Inline arithmetic requires an owning constructor result and an explicit binary right operand"};
    }
    helper.expression = behavior_expression_parser{input, 0, 0, owner.parent_namespace}.conditional();
    skip_whitespace_and_comment(input);
    check_and_increase(input, ')');
    helper.name = "serializer_arithmetic_" + std::to_string(owner.arithmetic.size());
    mapping.method_name = helper.name;
    mapping.method_index = owner.functions.size();
    owner.body_order.push_back({class_body_item::kind_type::function, owner.functions.size()});
    owner.functions.push_back(std::move(helper));
  } else {
    if (!helper.parameters.empty()) {
      throw exception::bad_member_spec{input, "Named arithmetic mappings take operands from the method signature"};
    }
    mapping.method_name = parse_identifier_impl(input);
  }
  skip_whitespace_and_comment(input);
  check_and_increase(input, ')');
  constexpr std::size_t maximum_arithmetic_mappings = 64;
  if (owner.arithmetic.size() == maximum_arithmetic_mappings) {
    throw exception::bad_member_spec{input, "Arithmetic mapping count exceeds 64"};
  }
  owner.arithmetic.push_back(std::move(mapping));
}

// Choose private helper names after all authored fields/callables are known, before signature binding.
void prepare_arithmetic_names(class_node& owner) {
  for (auto& mapping : owner.arithmetic) {
    if (mapping.inline_expression) {
      auto& helper = owner.functions.at(mapping.method_index);
      // Compiler-private names must remain distinct under snake/camel/Pascal presentation profiles.
      const auto symbol_key = [](std::string_view name) {
        std::string result;
        result.reserve(name.size());
        for (const auto character : name) {
          if (character == '_') { continue; }
          result += character >= 'A' && character <= 'Z' ?
              static_cast<char>(character + ('a' - 'A')) : character;
        }
        return result;
      };
      const auto collides = [&](std::string_view name) {
        const auto key = symbol_key(name);
        return std::any_of(owner.member_list.begin(), owner.member_list.end(), [&](const auto& field) {
          return symbol_key(field.name) == key ||
              (!field.readonly_getter.empty() && symbol_key(field.readonly_getter) == key);
        }) || std::any_of(owner.functions.begin(), owner.functions.end(), [&](const auto& method) {
          return &method != &helper && symbol_key(method.name) == key;
        }) || std::any_of(owner.constructions.begin(), owner.constructions.end(), [&](const auto& factory) {
          return symbol_key(factory.constructor ? std::string_view{"create"} : factory.name) == key;
        }) || std::any_of(owner.conversions.begin(), owner.conversions.end(), [&](const auto& conversion) {
          return symbol_key(conversion.name) == key;
        });
      };
      const auto base = helper.name;
      std::size_t suffix{};
      while (collides(helper.name)) { helper.name = base + "_" + std::to_string(++suffix); }
      mapping.method_name = helper.name;
    }
  }
}

// Bind each operator to one exact ordinary readonly method and reject duplicate native signatures.
void validate_arithmetic(const rohit::type_check::schema_input_buffer auto& input, class_node& owner) {
  if (owner.arithmetic.empty()) { return; }
  if (owner.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
      owner.supports_managed() || (owner.attributes & class_attributes::packed) != class_attributes::none) {
    throw exception::bad_member_spec{input, "Arithmetic mappings require an ordinary unpacked owning record"};
  }
  std::set<std::string> signatures;
  for (auto& mapping : owner.arithmetic) {
    if (!mapping.inline_expression) {
      std::optional<std::size_t> selected;
      for (std::size_t index{}; index < owner.functions.size(); ++index) {
        const auto& method = owner.functions[index];
        if (method.name != mapping.method_name || method.parameters.size() != (mapping.unary ? 0u : 1u)) {
          continue;
        }
        if (selected) { throw exception::bad_member_spec{input, "Ambiguous arithmetic method overload"}; }
        selected = index;
      }
      if (!selected) { throw exception::bad_member_spec{input, "Arithmetic method is missing or has invalid arity"}; }
      mapping.method_index = *selected;
    }
    const auto& method = owner.functions.at(mapping.method_index);
    if (method.effect != function_declaration::effect_type::readonly ||
        method.dispatch != function_declaration::dispatch_type::ordinary ||
        !method.generic_parameters.empty() || method.return_type.name == "void") {
      throw exception::bad_member_spec{input, "Arithmetic requires an ordinary nonvoid readonly method"};
    }
    if (mapping.inline_expression && (!method.expression || !method.expression->call_constructor ||
        !method.expression->result_type.starts_with("record:"))) {
      throw exception::bad_member_spec{input, "Inline arithmetic requires an explicit owning constructor result"};
    }
    const auto signature = (mapping.unary ? "unary" : "binary") + mapping.symbol +
        (mapping.unary ? std::string{} : method.parameters.front().type.get_full_name());
    if (!signatures.insert(signature).second) {
      throw exception::bad_member_spec{input, "Conflicting arithmetic operator signature"};
    }
  }
}
