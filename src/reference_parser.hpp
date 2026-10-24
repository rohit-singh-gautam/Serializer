// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later

// Lower a named reference into its ordinary portable address record before type resolution.
std::unique_ptr<class_node> parse_reference(
    const rohit::type_check::schema_input_buffer auto& input, namespace_node* space,
    std::unordered_map<std::string, syntax_node*>& declarations) {
  if (active_language_version < language_version{1u, 18u, 0u}) {
    throw exception::bad_object_type{input, "Field references require serializer version 1.18.0"};
  }
  parse_identifier_impl(input);
  skip_whitespace_and_comment(input);
  auto name = parse_identifier_impl(input);
  auto result = std::make_unique<class_node>(object_type::class_type, std::move(name),
      space, class_attributes::stable_ids, std::vector<parent>{});
  result->reference.emplace();
  auto& reference = *result->reference;
  skip_whitespace_and_comment(input);
  check_and_increase(input, '=');
  skip_whitespace_and_comment(input);
  const auto mode = parse_identifier_impl(input);
  if (mode == "shared") {
    reference.mode = reference_declaration::mode_type::shared;
  } else if (mode == "weak") {
    reference.mode = reference_declaration::mode_type::weak;
  } else if (mode != "borrowed") {
    throw exception::bad_member_spec{input, "Reference mode must be borrowed, shared or weak"};
  }
  skip_whitespace_and_comment(input);
  check_and_increase(input, '(');
  skip_whitespace_and_comment(input);
  reference.owner = parse_type_expression(input, space);
  constexpr std::size_t maximum_reference_depth = 32;
  skip_whitespace_and_comment(input);
  while (!input.full() && *input == '.') {
    ++input;
    skip_whitespace_and_comment(input);
    if (reference.source_path.size() >= maximum_reference_depth) {
      throw exception::bad_member_spec{input, "Reference field path exceeds 32 components"};
    }
    reference.source_path.push_back(parse_identifier_impl(input));
    skip_whitespace_and_comment(input);
  }
  if (reference.source_path.empty()) {
    throw exception::bad_member_spec{input, "A field reference requires an owner and field path"};
  }
  check_and_increase(input, ')');
  skip_whitespace_and_comment(input);
  check_and_increase(input, ';');
  // These explicit IDs are the portable address contract, independent of native pointer layout.
  const auto add_field = [&](std::string field_name, std::string type, std::uint32_t id,
      member::modifier_type modifier = member::modifier_type::none) {
    member field{};
    field.access = access_type::public_access;
    field.modifier = modifier;
    field.name = std::move(field_name);
    field.display_name = field.name;
    field.key = field.name;
    field.id = id;
    field.explicit_id = true;
    field.fixed_name = true;
    field.type_name_list.emplace_back(std::move(type), space);
    result->member_list.push_back(std::move(field));
  };
  add_field("document_id", "string", 1);
  add_field("owner_id", "uint64", 2);
  add_field("generation", "uint64", 3);
  add_field("field_path", "uint32", 4, member::modifier_type::array);
  register_declaration(input, *result, declarations);
  return result;
}

// Resolve direct payloads or public field projections; durable addresses also retain stable IDs.
void validate_reference(const rohit::type_check::schema_input_buffer auto& input,
    class_node& record, const std::unordered_map<std::string, syntax_node*>& declarations) {
  if (!record.reference) { return; }
  auto& reference = *record.reference;
  resolve_type(input, reference.owner, declarations);
  const auto& owner = conversion::record(reference.owner);
  if (owner.reference || owner.type == object_type::generic_definition) {
    throw exception::bad_member_type{input, "Reference owners require concrete ordinary owning records"};
  }
  auto current = reference.owner;
  reference.field_path.clear();
  reference.source_base_paths.clear();
  for (std::size_t component{}; component < reference.source_path.size(); ++component) {
    const auto& name = reference.source_path[component];
    std::vector<std::size_t> bases;
    const auto& field = conversion::find_field(conversion::record(current), name, bases);
    for (const auto index : bases) {
      const auto& base = conversion::record(current).parents[index];
      if (!reference.value_projection && !base.explicit_id) {
        throw exception::bad_member_type{input, "Reference paths require explicit stable base IDs"};
      }
      if (!reference.value_projection) { reference.field_path.push_back(base.id); }
      current = conversion::base_type(base, current);
    }
    if (field.bit_width != 0 || (!reference.value_projection && (!field.explicit_id || field.transient)) || field.wire_only || field.managed ||
        field.modifier != member::modifier_type::none || field.type_name_list.size() != 1) {
      throw exception::bad_member_type{input,
          reference.value_projection ?
          "Value references require ordinary scalar or record fields; wrap collection fields in a record" :
          "References require durable ordinary fields with explicit stable IDs; collection elements are unsupported"};
    }
    if (!reference.value_projection) { reference.field_path.push_back(field.id); }
    current = conversion::bound_type(field.type_name_list.front(), current);
    reference.source_base_paths.push_back(std::move(bases));
  }
  std::set<const class_node*> active;
  if (reference.value_projection) { conversion::validate_borrow(current, active); }
  else { conversion::validate_copy(current, active); }
  reference.value_type = current;
  if (reference.value_projection) {
    // This internal slot describes the wire type only; export handles own no value or defaults.
    member payload{};
    payload.access = access_type::private_access;
    payload.modifier = member::modifier_type::none;
    payload.name = "srl_value";
    payload.display_name = payload.name;
    payload.key = payload.name;
    payload.id = 1;
    payload.fixed_name = true;
    payload.type_name_list.push_back(std::move(current));
    record.member_list = {std::move(payload)};
  }
}

// Insert transparent wrapper declarations before their consumer while preserving its authored fields.
void lower_value_references(const rohit::type_check::schema_input_buffer auto& input,
    class_node& consumer, std::vector<std::unique_ptr<syntax_node>>& statements,
    std::unordered_map<std::string, syntax_node*>& declarations) {
  for (auto& field : consumer.member_list) {
    if (!field.value_reference) { continue; }
    if (consumer.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
        consumer.supports_managed() ||
        (consumer.attributes & class_attributes::packed) == class_attributes::packed) {
      throw exception::bad_member_type{input, "Value references require an ordinary owning consumer"};
    }
    auto name = consumer.name + "_" + field.name + "_value_reference";
    auto wrapper = std::make_unique<class_node>(object_type::class_type, std::move(name),
        consumer.parent_namespace, class_attributes::none, std::vector<parent>{});
    wrapper->source_path = consumer.source_path;
    wrapper->output_source_path = consumer.output_source_path;
    wrapper->source_notices = consumer.source_notices;
    wrapper->reference = std::move(field.value_reference);
    field.value_reference.reset();
    field.type_name_list = {type_name{std::string{wrapper->name}, consumer.parent_namespace}};
    register_declaration(input, *wrapper, declarations);
    statements.push_back(std::move(wrapper));
  }
}
