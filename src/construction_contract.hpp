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
    for (auto& validation : factory.validations) {
      function_declaration check;
      // Validation observes the bound candidate, not potentially moved input arguments.
      if (validate_behavior_expression(validation, owner, check, symbols, {}, functions) != "bool") {
        throw exception::bad_member_spec{input, "Construction validation must return bool"};
      }
    }
    for (auto& assignment : factory.assignments) {
      if (!assignment.expression) { continue; }
      const auto found = std::find_if(owner.member_list.begin(), owner.member_list.end(),
          [&](const auto& field) { return field.name == assignment.field; });
      if (found == owner.member_list.end() || found->modifier != member::modifier_type::none) {
        throw exception::bad_member_spec{input, "Construction expressions require an ordinary scalar field"};
      }
      auto& expression = *assignment.expression;
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
      static_cast<void>(validate_behavior_expression(expression, scope, binding, symbols, {}, functions));
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
      if (expression.kind == behavior_expression::operation::literal ||
          expression.kind == behavior_expression::operation::integer_literal ||
          expression.kind == behavior_expression::operation::boolean) {
        assignment.value = expression.value;
        assignment.expression.reset(); // Keep ordinary literal construction portable in the new language.
      } else if ((expression.kind == behavior_expression::operation::positive ||
                  expression.kind == behavior_expression::operation::negative) &&
          expression.operands.size() == 1 &&
          expression.operands.front().kind == behavior_expression::operation::literal) {
        assignment.value = (expression.kind == behavior_expression::operation::negative ? "-" : "") +
            expression.operands.front().value;
        assignment.expression.reset(); // Unary floating literals retain their established portable syntax.
      }
    }
  }
}
