// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later

// Resolve an explicitly selected array shape after concrete generic arguments have been bound.
void validate_array_exposure(const rohit::type_check::schema_input_buffer auto& input,
                            class_node& owner) {
  const member* selected{};
  for (auto& field : owner.member_list) {
    if (!field.array_exposure) { continue; }
    if (selected) { throw exception::bad_member_spec{input, "Only one array field may be exposed per owning class"}; }
    selected = &field;
    if (owner.supports_managed() ||
        owner.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
        (owner.attributes & class_attributes::packed) != class_attributes::none ||
        field.wire_only || field.magic || field.version || field.managed ||
        field.type_name_list.size() != 1 ||
        (field.modifier != member::modifier_type::array &&
         (field.modifier != member::modifier_type::none || !field.type_name_list.front().is_sequence()))) {
      throw exception::bad_member_spec{input, "Array exposure requires stored ordinary arrays in an unpacked owning class"};
    }
    auto& options = *field.array_exposure;
    if (!options.dimensions.empty() && field.extent_expression.empty()) {
      throw exception::bad_member_spec{input, "Explicit array dimensions require fixed owning storage"};
    }
    constexpr std::uint64_t maximum_fixed_elements = 65536;
    // Unknown dimensions remain dependent only for an open C++ owner, with a generated shape assertion.
    const auto resolve_dimension = [&](const auto& self, dimension_expression& dimension)
        -> std::optional<std::uint64_t> {
      using operation = dimension_expression::operation;
      if (dimension.kind == operation::literal) { return dimension.value; }
      if (dimension.kind == operation::parameter) {
        const auto parameter = std::find_if(owner.generic_parameters.begin(), owner.generic_parameters.end(),
            [&](const auto& value) { return value.name == dimension.name; });
        if (parameter == owner.generic_parameters.end() || parameter->kind != generic_argument_kind::dimension) {
          throw exception::bad_member_spec{input, "Array dimensions require declared owner dimensions"};
        }
        const auto position = static_cast<std::size_t>(parameter - owner.generic_parameters.begin());
        if (position >= owner.generic_arguments.size() || !owner.generic_arguments[position].dimension) {
          return std::nullopt;
        }
        dimension.kind = operation::literal;
        dimension.value = owner.generic_arguments[position].dimension;
        dimension.name.clear();
        return dimension.value;
      }
      const auto left = self(self, dimension.operands.at(0));
      const auto right = self(self, dimension.operands.at(1));
      if (!left || !right) { return std::nullopt; }
      if (*left > maximum_fixed_elements || *right > maximum_fixed_elements ||
          (dimension.kind == operation::add ? *right > maximum_fixed_elements - *left :
           *left && *right > maximum_fixed_elements / *left)) {
        throw exception::bad_member_spec{input, "Array dimensions exceed the fixed storage limit"};
      }
      dimension.value = dimension.kind == operation::add ? *left + *right : *left * *right;
      dimension.kind = operation::literal;
      dimension.operands.clear();
      return dimension.value;
    };
    std::optional<std::uint64_t> cardinality{1};
    if (!field.fixed_extent && !field.extent_expression.empty()) {
      const auto extent = resolve_dimension(resolve_dimension, field.extent_expression.front());
      if (extent) { field.fixed_extent = *extent; }
    }
    for (auto& dimension : options.dimensions) {
      const auto extent = resolve_dimension(resolve_dimension, dimension);
      if (!extent) { cardinality.reset(); continue; }
      if (!*extent || *extent > maximum_fixed_elements ||
          (cardinality && *extent > maximum_fixed_elements / *cardinality)) {
        throw exception::bad_member_spec{input, "Array dimensions require positive bounded extents"};
      }
      if (cardinality) { *cardinality *= *extent; }
    }
    if (!cardinality && owner.type != object_type::generic_definition) {
      throw exception::bad_member_spec{input, "Concrete exposed array dimensions must resolve before generation"};
    }
    if (!options.dimensions.empty() && cardinality && field.fixed_extent && field.fixed_extent != *cardinality) {
      throw exception::bad_member_spec{input, "Array dimensions product must equal its fixed backing extent"};
    }
    // Portable logical helpers share the source namespace; native extra names are checked by C++ generation.
    for (const std::string_view name : {"get", "set", "size", "values"}) {
      if (name == "set" && options.readonly) { continue; }
      if (owner.name == name ||
          std::find(owner.type_parameters.begin(), owner.type_parameters.end(), name) != owner.type_parameters.end() ||
          std::any_of(owner.member_list.begin(), owner.member_list.end(), [&](const auto& value) {
            return value.name == name || value.readonly_getter == name;
          }) || std::any_of(owner.functions.begin(), owner.functions.end(), [&](const auto& value) {
            return value.name == name;
          }) || std::any_of(owner.constructions.begin(), owner.constructions.end(), [&](const auto& value) {
            return !value.constructor && value.name == name;
          }) || std::any_of(owner.conversions.begin(), owner.conversions.end(), [&](const auto& value) {
            return value.name == name;
          })) {
        throw exception::bad_member_spec{input, "Array exposure API collides with a source declaration: " + std::string{name}};
      }
    }
  }
}
