// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later

// Bind readonly construction expressions against parameters, never partially initialized fields.
void bind_construction_expressions(const rohit::type_check::schema_input_buffer auto& input,
                                  class_node& owner,
                                  const std::unordered_map<std::string, syntax_node*>& symbols,
                                  const std::vector<const function_node*>& functions = {}) {
  validate_constructions(input, owner, symbols);
  for (auto& factory : owner.constructions) {
    class_node scope{object_type::class_type, std::string{}, owner.parent_namespace,
        class_attributes::none, {}};
    scope.type_parameters = owner.type_parameters;
    scope.generic_parameters = owner.generic_parameters;
    for (const auto& parameter : factory.generic_parameters) {
      scope.type_parameters.push_back(parameter.name);
      scope.generic_parameters.push_back(parameter);
    }
    function_declaration binding;
    binding.parameters = factory.parameters;
    binding.generic_parameters = factory.generic_parameters;
    if (factory.delegating) {
      if (!factory.constructor || !factory.assignments.empty() || !factory.validations.empty() ||
          !factory.native_validations.empty() || !factory.initializers.empty()) {
        throw exception::bad_member_spec{input, "Delegating constructors cannot add bindings or authoring hooks"};
      }
      if (factory.guard && factory.guard->available == false) { continue; }
      for (auto& argument : factory.delegation) {
        static_cast<void>(validate_behavior_expression(argument, scope, binding, symbols, {}, functions));
        if (argument.kind == behavior_expression::operation::symbol) {
          const auto parameter = std::find_if(factory.parameters.begin(), factory.parameters.end(),
              [&](const auto& value) { return value.name == argument.value; });
          if (parameter != factory.parameters.end()) {
            argument.collection_modifier = parameter->modifier;
            argument.fixed_extent = parameter->fixed_extent;
          }
        }
      }
      std::optional<std::size_t> selected;
      std::vector<behavior_expression> selected_arguments;
      for (const bool bind_literals : {false, true}) {
        for (std::size_t index = 0; index < owner.constructions.size(); ++index) {
          const auto& target = owner.constructions[index];
          if (!target.constructor || !target.generic_parameters.empty() ||
              (target.guard && target.guard->available == false) ||
              factory.delegation.size() > target.parameters.size() ||
              std::any_of(target.parameters.begin() + static_cast<std::ptrdiff_t>(factory.delegation.size()),
                  target.parameters.end(), [](const auto& parameter) {
                    return parameter.default_value.empty() && parameter.default_factory.empty();
                  })) { continue; }
          auto arguments = factory.delegation;
          bool matching = true;
          for (std::size_t argument_index = 0; argument_index < arguments.size(); ++argument_index) {
            auto& argument = arguments[argument_index];
            const auto& parameter = target.parameters[argument_index];
            bool exact = argument.resolved_type ?
                argument.collection_modifier == parameter.modifier &&
                    matching_behavior_type(*argument.resolved_type, parameter.type) :
                parameter.modifier == member::modifier_type::none && argument.result_type == parameter.type.name;
            if (parameter.modifier == member::modifier_type::array) {
              exact = exact && argument.fixed_extent == parameter.fixed_extent;
              if (exact && !parameter.fixed_extent) {
                const auto source = std::find_if(factory.parameters.begin(), factory.parameters.end(),
                    [&](const auto& value) {
                      return argument.kind == behavior_expression::operation::symbol && value.name == argument.value;
                    });
                exact = source != factory.parameters.end() && source->extent_expression == parameter.extent_expression;
              }
            }
            if (!exact) {
              try {
                if (!bind_literals || parameter.modifier != member::modifier_type::none ||
                    !bind_behavior_literal(argument, parameter.type.name, owner.parent_namespace)) {
                  matching = false;
                  break;
                }
              } catch (const std::invalid_argument&) {
                matching = false;
                break;
              }
            }
          }
          if (!matching) { continue; }
          if (selected) { throw exception::bad_member_spec{input, "Ambiguous constructor delegation target"}; }
          selected = index;
          selected_arguments = std::move(arguments);
        }
        if (selected) { break; }
      }
      if (!selected && !factory.delegation.empty()) {
        throw exception::bad_member_spec{input, "No exactly typed constructor delegation target; independent target deduction is unsupported"};
      }
      if (!selected && std::any_of(owner.constructions.begin(), owner.constructions.end(),
          [](const auto& target) { return target.constructor && target.parameters.empty(); })) {
        throw exception::bad_member_spec{input, "No available default constructor delegation target"};
      }
      factory.delegation_target = selected.value_or(std::numeric_limits<std::size_t>::max());
      if (selected) { factory.delegation = std::move(selected_arguments); }
      continue;
    }
    for (auto& validation : factory.validations) {
      function_declaration check;
      // Validation observes the bound candidate, not potentially moved input arguments.
      if (validate_behavior_expression(validation, owner, check, symbols, {}, functions) != "bool") {
        throw exception::bad_member_spec{input, "Construction validation must return bool"};
      }
    }
    for (auto& assignment : factory.assignments) {
      const auto found = std::find_if(owner.member_list.begin(), owner.member_list.end(),
          [&](const auto& field) { return field.name == assignment.field; });
      if (found == owner.member_list.end()) {
        throw exception::bad_member_spec{input, "Unknown construction expression field"};
      }
      std::vector<construction_assignment*> expressions;
      if (assignment.array_binding && assignment.generator_indices.empty()) {
        for (auto& element : assignment.elements) { expressions.push_back(&element); }
      } else { expressions.push_back(&assignment); }
      auto expression_binding = binding;
      for (const auto& index : assignment.generator_indices) {
        function_parameter parameter;
        parameter.name = index;
        parameter.type = type_name{std::string{"uint64"}, owner.parent_namespace};
        expression_binding.parameters.push_back(std::move(parameter));
      }
      for (auto* current : expressions) {
        if (!current->expression) { continue; }
        if (found->modifier != member::modifier_type::none && !assignment.array_binding) {
          throw exception::bad_member_spec{input, "Construction expressions require an ordinary scalar field"};
        }
        auto& expression = *current->expression;
        const auto bounded = [&](const auto& self, const behavior_expression& node) -> void {
          using operation = behavior_expression::operation;
          if (node.kind == operation::call && !factory.overload_contract_enabled) {
            throw exception::bad_member_spec{input, "Readonly construction calls require serializer version 1.12.0"};
          }
          if (node.kind == operation::all || node.kind == operation::any ||
              node.kind == operation::index_access || node.kind == operation::collection_size) {
            throw exception::bad_member_spec{input, "Construction bindings allow bounded scalar operations and readonly field projections"};
          }
          for (const auto& operand : node.operands) { self(self, operand); }
        };
        bounded(bounded, expression);
        static_cast<void>(validate_behavior_expression(expression, scope, expression_binding, symbols, {}, functions));
        const auto scalar_calls = [&](const auto& self, const behavior_expression& node) -> void {
          if (node.kind == behavior_expression::operation::call &&
              (node.result_type.starts_with("record:") || node.result_type == "collection" ||
               node.result_type == "string" || node.result_type == "digest")) {
            throw exception::bad_member_spec{input, "Construction calls require a scalar or enum result"};
          }
          for (const auto& operand : node.operands) { self(self, operand); }
        };
        scalar_calls(scalar_calls, expression);
        static_cast<void>(bind_behavior_literal(expression, found->type_name_list.front().name, owner.parent_namespace));
        const auto& target = found->type_name_list.front();
        const auto expected = target.type == object_type::enum_type ? "enum:" + target.get_full_name() : target.name;
        const bool matches = expression.resolved_type ? matching_behavior_type(*expression.resolved_type, target) :
            expression.result_type == expected;
        if (!matches || expression.collection_modifier != member::modifier_type::none ||
            expression.result_type.starts_with("record:") || expression.result_type == "collection") {
          throw exception::bad_member_spec{input, "Construction expression must match its exact scalar destination; use checked<T> explicitly"};
        }
        if (!assignment.generator_indices.empty()) { continue; }
        if (expression.kind == behavior_expression::operation::literal ||
            expression.kind == behavior_expression::operation::integer_literal ||
            expression.kind == behavior_expression::operation::boolean) {
          current->value = expression.value;
          current->expression.reset(); // Keep ordinary literal construction portable in the new language.
        } else if ((expression.kind == behavior_expression::operation::positive ||
                    expression.kind == behavior_expression::operation::negative) &&
            expression.operands.size() == 1 &&
            expression.operands.front().kind == behavior_expression::operation::literal) {
          current->value = (expression.kind == behavior_expression::operation::negative ? "-" : "") +
              expression.operands.front().value;
          current->expression.reset(); // Unary floating literals retain their established portable syntax.
        }
      }
    }
  }
  // Delegation invokes another constructor, so every active chain must terminate in ordinary bindings.
  std::vector<unsigned char> visited(owner.constructions.size());
  const auto check_chain = [&](const auto& self, std::size_t index) -> void {
    if (visited[index] == 2) { return; }
    if (visited[index] == 1) { throw exception::bad_member_spec{input, "Constructor delegation cycle"}; }
    visited[index] = 1;
    const auto& factory = owner.constructions[index];
    if (factory.delegating && (!factory.guard || factory.guard->available != false) &&
        factory.delegation_target != std::numeric_limits<std::size_t>::max()) {
      self(self, factory.delegation_target);
    }
    visited[index] = 2;
  };
  for (std::size_t index = 0; index < owner.constructions.size(); ++index) { check_chain(check_chain, index); }
  // Codec default construction must not replay hooks reached indirectly through delegation.
  for (const auto& factory : owner.constructions) {
    if (!factory.delegating || !factory.parameters.empty() ||
        (factory.guard && factory.guard->available == false)) { continue; }
    auto target = factory.delegation_target;
    while (target != std::numeric_limits<std::size_t>::max()) {
      const auto& constructor = owner.constructions[target];
      if (!constructor.validations.empty() || !constructor.native_validations.empty() ||
          !constructor.initializers.empty()) {
        throw exception::bad_member_spec{input, "Default constructor delegation cannot invoke authoring hooks"};
      }
      target = constructor.delegating ? constructor.delegation_target : std::numeric_limits<std::size_t>::max();
    }
  }
}
