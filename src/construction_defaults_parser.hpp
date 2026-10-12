// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later

// Bind one typed recursive initializer; owned record fields use their declaration order.
value_initializer bind_value_initializer(const rohit::type_check::schema_input_buffer auto& input,
    const member& field, const class_node& owner, std::string text,
    std::size_t depth = 0, std::size_t* shared_nodes = nullptr,
    bool empty_record_default_enabled = false) {
  constexpr std::size_t maximum_depth = 32, maximum_nodes = 65536;
  std::size_t local_nodes{};
  auto& nodes = shared_nodes ? *shared_nodes : local_nodes;
  if (depth >= maximum_depth || ++nodes > maximum_nodes) {
    throw exception::bad_member_spec{input, "Portable default resource limit exceeded"};
  }
  text = schema_version::trim(text);
  value_initializer result;
  result.type = field.type_name_list.front();
  if (field.modifier == member::modifier_type::array ||
      (result.type.resolved_node && (result.type.type == object_type::class_type ||
                                    result.type.type == object_type::generic_definition))) {
    if (field.managed || field.version || field.wire_only || field.magic) {
      throw exception::bad_member_spec{input, "Portable aggregate defaults require ordinary owning fields"};
    }
    if (text.empty() || text.front() != '{' || text.back() != '}') {
      throw exception::bad_member_spec{input, "Portable aggregate default requires a brace-enclosed value list"};
    }
    const auto values_input = make_constant_full_stream(text.data(), text.size());
    auto values = parse_array_initializer(values_input);
    skip_whitespace_and_comment(values_input);
    if (!values_input.full()) { throw exception::bad_member_spec{input, "Trailing portable default tokens"}; }
    if (field.modifier == member::modifier_type::array) {
      if (!field.fixed_extent || values.size() != field.fixed_extent) {
        throw exception::bad_member_spec{input, "Portable fixed-array defaults require exactly the declared cardinality"};
      }
      result.kind = value_initializer::kind_type::fixed_array;
      auto element = field;
      element.modifier = member::modifier_type::none;
      element.initializer.reset();
      for (auto& value : values) {
        result.elements.push_back(bind_value_initializer(input, element, owner, std::move(value),
            depth + 1, &nodes, empty_record_default_enabled));
      }
    } else {
      result.kind = value_initializer::kind_type::record;
      const auto& record = static_cast<const class_node&>(*result.type.resolved_node);
      if (record.supports_managed() || !record.parents.empty() ||
          record.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
          (record.attributes & class_attributes::packed) != class_attributes::none ||
          std::any_of(record.native_blocks.begin(), record.native_blocks.end(),
              [](const auto& block) { return block.language == "cpp" && !block.no_instance_storage; })) {
        throw exception::bad_member_spec{input, "Portable record defaults require nonmanaged owning records without bases/native storage"};
      }
      const bool has_fields = std::any_of(record.member_list.begin(), record.member_list.end(),
          [](const auto& member) { return !member.wire_only; });
      if (values.empty() && has_fields) {
        if (!empty_record_default_enabled) {
          throw exception::bad_member_spec{input,
              "Empty record parameter defaults require serializer version 1.15.0"};
        }
        if (std::any_of(record.constructions.begin(), record.constructions.end(), [](const auto& construction) {
            return construction.constructor && construction.parameters.empty() &&
                construction.access != access_type::public_access;
          })) {
          throw exception::bad_member_spec{input, "Empty record parameter default requires a public default constructor"};
        }
        // Default construction keeps schema field defaults and never runs a parameterized authoring hook.
        result.kind = value_initializer::kind_type::default_record;
        return result;
      }
      std::size_t position{};
      for (const auto& record_field : record.member_list) {
        if (record_field.wire_only) { continue; }
        if (position == values.size() || record_field.version || record_field.managed ||
            (record_field.modifier != member::modifier_type::none && record_field.modifier != member::modifier_type::array)) {
          throw exception::bad_member_spec{input, "Portable record defaults must supply every ordinary field in declaration order"};
        }
        auto element = record_field;
        // Open record applications preserve the containing owner's template parameter spelling.
        const auto substitute = [&](const auto& self, type_name& type) -> void {
          for (std::size_t index{}; index < record.generic_parameters.size() && index < result.type.arguments.size(); ++index) {
            if (!type.application && type.name == record.generic_parameters[index].name) {
              type = result.type.arguments[index];
              return;
            }
          }
          for (auto& argument : type.arguments) { self(self, argument); }
        };
        for (auto& type : element.type_name_list) { substitute(substitute, type); }
        result.elements.push_back(bind_value_initializer(input, element, owner,
            std::move(values[position++]), depth + 1, &nodes, empty_record_default_enabled));
      }
      if (position != values.size()) {
        throw exception::bad_member_spec{input, "Portable record default has excessive field values"};
      }
    }
  } else {
    if (field.modifier != member::modifier_type::none || text.empty() || result.type.application) {
      throw exception::bad_member_spec{input, "Portable default requires a typed scalar literal"};
    }
    auto scalar = field;
    scalar.default_value = text;
    scalar.initializer.reset();
    scalar.default_factory.clear();
    const bool parameter = !result.type.resolved_node &&
        std::find(owner.type_parameters.begin(), owner.type_parameters.end(), result.type.name) != owner.type_parameters.end();
    if (parameter) {
      // The explicit scalar-default contract defers T{literal} construction to native specialization.
      scalar.type_name_list.front() = type_name{
          text == "true" || text == "false" ? "bool" : text.front() == '"' ? "string" :
              text.front() == '\'' ? "char" : text.find_first_of(".eE") != std::string::npos ? "double" :
                  text.front() == '-' ? "int64" : "uint64", nullptr};
      scalar.type_name_list.front().type = object_type::primitive;
    } else if (result.type.name == "char" && text.front() != '\'') {
      // Inferred char arrays already contain checked byte numbers instead of source quotes.
      scalar.type_name_list.front() = type_name{"uint8", nullptr};
      scalar.type_name_list.front().type = object_type::primitive;
    }
    try { result.value = writer::native::literal(scalar); }
    catch (const std::invalid_argument& error) { throw exception::bad_member_spec{input, error.what()}; }
    // Enum and char lowerers need their validated source spelling, not a target-specific number/name.
    if (result.type.type == object_type::enum_type || result.type.name == "char") { result.value = text; }
  }
  return result;
}

// Retain ordinary scalar defaults and opt into typed fixed-array/nested/open-generic defaults only in 1.14.
void validate_portable_default(const rohit::type_check::schema_input_buffer auto& input,
                              member& field, const class_node& owner,
                              bool empty_record_default_enabled = false) {
  if (field.portable_default_enabled && !field.fixed_extent && field.extent_expression.size() == 1 &&
      field.extent_expression.front().kind == dimension_expression::operation::literal) {
    field.fixed_extent = field.extent_expression.front().value;
  }
  if (!field.portable_default_enabled || field.default_value.empty() || !field.default_factory.empty()) { return; }
  const auto& type = field.type_name_list.front();
  const bool aggregate = field.modifier == member::modifier_type::array ||
      (type.resolved_node && (type.type == object_type::class_type || type.type == object_type::generic_definition));
  const bool parameter = !type.resolved_node &&
      std::find(owner.type_parameters.begin(), owner.type_parameters.end(), type.name) != owner.type_parameters.end();
  if (!aggregate && !parameter) { return; }
  const auto text = field.modifier == member::modifier_type::array ? "{" + field.default_value + "}" : field.default_value;
  field.initializer = bind_value_initializer(input, field, owner, text, 0, nullptr,
      empty_record_default_enabled);
}

// Supply a compiler-owned positional factory only for record types used by typed aggregate defaults.
void install_default_constructions(const std::vector<class_node*>& owners) {
  std::set<const syntax_node*> targets;
  const auto collect = [&](const auto& self, const value_initializer& value) -> void {
    if (value.kind == value_initializer::kind_type::record) { targets.insert(value.type.resolved_node); }
    for (const auto& element : value.elements) { self(self, element); }
  };
  for (const auto* owner : owners) {
    for (const auto& field : owner->member_list) { if (field.initializer) { collect(collect, *field.initializer); } }
    for (const auto& factory : owner->constructions) {
      for (const auto& parameter : factory.parameters) { if (parameter.initializer) { collect(collect, *parameter.initializer); } }
    }
  }
  // C++ emits concrete generic instances as aliases to the original open template.
  for (const auto* owner : owners) {
    if (targets.contains(owner) && !owner->generic_name.empty()) {
      for (const auto* definition : owners) {
        if (definition->get_full_name() == owner->generic_name) { targets.insert(definition); }
      }
    }
  }
  for (auto* owner : owners) {
    if (!targets.contains(owner)) { continue; }
    constexpr std::string_view helper_name = "serializer_make_default";
    if (std::any_of(owner->constructions.begin(), owner->constructions.end(),
        [&](const auto& factory) { return factory.name == helper_name; })) {
      throw std::invalid_argument{"Portable default helper name collides with a declared construction"};
    }
    construction_declaration helper;
    helper.name = helper_name;
    helper.member_initialization_enabled = true;
    helper.overload_contract_enabled = true;
    for (const auto& field : owner->member_list) {
      if (field.wire_only) { continue; }
      function_parameter parameter;
      parameter.type = field.type_name_list.front();
      parameter.name = field.name;
      parameter.modifier = field.modifier;
      parameter.fixed_extent = field.fixed_extent;
      parameter.extent_expression = field.extent_expression;
      helper.parameters.push_back(std::move(parameter));
      helper.assignments.push_back({field.name, field.name, true, false, {}});
    }
    owner->constructions.push_back(std::move(helper));
  }
}
