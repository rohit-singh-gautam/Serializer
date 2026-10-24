// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later

// Bind one typed recursive initializer; owned record fields use their declaration order.
value_initializer bind_value_initializer(const rohit::type_check::schema_input_buffer auto& input,
    const member& field, const class_node& owner, std::string text,
    std::size_t depth = 0, std::size_t* shared_nodes = nullptr,
    bool empty_record_default_enabled = false,
    bool owner_parameter_default_enabled = false) {
  constexpr std::size_t maximum_depth = 32, maximum_nodes = 65536;
  std::size_t local_nodes{};
  auto& nodes = shared_nodes ? *shared_nodes : local_nodes;
  if (depth >= maximum_depth || ++nodes > maximum_nodes) {
    throw exception::bad_member_spec{input, "Portable default resource limit exceeded"};
  }
  text = schema_version::trim(text);
  value_initializer result;
  result.type = field.type_name_list.front();
  if (owner_parameter_default_enabled && field.modifier == member::modifier_type::none &&
      empty_construction_default(text)) {
    // Open native representations defer T{} to omitted calls, never to a class-wide constraint.
    result.kind = value_initializer::kind_type::default_owner;
    if (result.type.resolved_node && (result.type.type == object_type::class_type ||
                                     result.type.type == object_type::generic_definition)) {
      const auto& record = static_cast<const class_node&>(*result.type.resolved_node);
      const bool portable_record = !record.supports_managed() && record.parents.empty() &&
          record.storage_modes == static_cast<std::uint8_t>(storage_mode::owning) &&
          (record.attributes & class_attributes::packed) == class_attributes::none &&
          std::none_of(record.native_blocks.begin(), record.native_blocks.end(), [](const auto& block) {
            return block.language == "cpp" && !block.no_instance_storage;
          }) && std::none_of(record.constructions.begin(), record.constructions.end(), [](const auto& construction) {
            return construction.constructor && construction.parameters.empty() &&
                construction.access != access_type::public_access;
          });
      // Concrete ordinary schema records use the already qualified portable default-construction path.
      if (portable_record) { result.kind = value_initializer::kind_type::default_record; }
    }
    return result;
  }
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

// Recognize the existing unsuffixed decimal/hex contract with constant stack use.
bool canonical_integer_default(std::string_view text) {
  if (text.starts_with('-')) { text.remove_prefix(1); }
  if (text.empty()) { return false; }
  if (text.starts_with("0x") || text.starts_with("0X")) {
    text.remove_prefix(2);
    return !text.empty() && std::all_of(text.begin(), text.end(), [](char value) {
      return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f') ||
          (value >= 'A' && value <= 'F');
    });
  }
  if (text.front() == '0') { return text.size() == 1; }
  return text.front() >= '1' && text.front() <= '9' &&
      std::all_of(text.begin(), text.end(), [](char value) { return value >= '0' && value <= '9'; });
}

// Reject known integer narrowing while leaving opaque native scalar expressions unchanged.
void validate_integer_default(const member& field) {
  const auto& type = field.type_name_list.front();
  if (field.modifier != member::modifier_type::none || type.type != object_type::primitive ||
      (!type.name.starts_with("int") && !type.name.starts_with("uint"))) { return; }
  const auto text = schema_version::trim(field.default_value);
  // Recursive regex engines can exhaust the process stack on long schema literals.
  // Classify the complete spelling before conversion so native suffixes remain opaque.
  if (!canonical_integer_default(text)) { return; }
  const bool negative = text.front() == '-';
  std::string_view magnitude{text};
  if (negative) { magnitude.remove_prefix(1); }
  int radix = 10;
  if (magnitude.starts_with("0x") || magnitude.starts_with("0X")) {
    magnitude.remove_prefix(2);
    radix = 16;
  }
  std::uint64_t value{};
  const auto parsed = std::from_chars(magnitude.data(), magnitude.data() + magnitude.size(), value, radix);
  const auto bits = static_cast<unsigned>(writer::native::width(type)) *
      std::numeric_limits<std::uint8_t>::digits;
  constexpr auto maximum_unsigned = std::numeric_limits<std::uint64_t>::max();
  constexpr auto maximum_bits = std::numeric_limits<std::uint64_t>::digits;
  const bool unsigned_type = type.name.starts_with("uint");
  const auto maximum = unsigned_type ? maximum_unsigned >> (maximum_bits - bits)
      : negative ? std::uint64_t{1} << (bits - 1u)
                 : maximum_unsigned >> (maximum_bits + 1u - bits);
  if (parsed.ec != std::errc{} || value > maximum || (negative && unsigned_type && value != 0)) {
    throw std::invalid_argument{"Numeric default exceeds the declared " + type.name + " range"};
  }
}

// Recognize canonical unsuffixed decimal literals in linear space, preserving native spellings.
bool canonical_floating_default(std::string_view text) {
  std::size_t offset = text.starts_with('-') ? 1U : 0U;
  const auto digit = [](char character) { return character >= '0' && character <= '9'; };
  if (offset == text.size() || !digit(text[offset])) { return false; }
  if (text[offset] == '0') { ++offset; }
  else { while (offset < text.size() && digit(text[offset])) { ++offset; } }
  if (offset < text.size() && text[offset] == '.') {
    ++offset;
    const auto start = offset;
    while (offset < text.size() && digit(text[offset])) { ++offset; }
    if (offset == start) { return false; }
  }
  if (offset < text.size() && (text[offset] == 'e' || text[offset] == 'E')) {
    ++offset;
    if (offset < text.size() && (text[offset] == '-' || text[offset] == '+')) { ++offset; }
    const auto start = offset;
    while (offset < text.size() && digit(text[offset])) { ++offset; }
    if (offset == start) { return false; }
  }
  return offset == text.size();
}

// Classify a canonical decimal literal's magnitude without overflowing its written exponent.
bool floating_default_at_least_one(std::string_view text) {
  if (text.starts_with('-')) { text.remove_prefix(1); }
  const auto exponent_position = text.find_first_of("eE");
  const auto mantissa = text.substr(0, exponent_position);
  const auto decimal_position = mantissa.find('.');
  const auto integer_digits = decimal_position == std::string_view::npos ?
      mantissa.size() : decimal_position;
  std::size_t first_nonzero = std::string_view::npos;
  std::size_t digits{};
  for (const auto character : mantissa) {
    if (character == '.') { continue; }
    if (first_nonzero == std::string_view::npos && character != '0') { first_nonzero = digits; }
    ++digits;
  }
  if (first_nonzero == std::string_view::npos) { return false; }
  std::size_t exponent{};
  bool negative_exponent{};
  if (exponent_position != std::string_view::npos) {
    auto lexical = text.substr(exponent_position + 1U);
    negative_exponent = lexical.starts_with('-');
    if (lexical.starts_with('-') || lexical.starts_with('+')) { lexical.remove_prefix(1); }
    const auto parsed = std::from_chars(lexical.data(), lexical.data() + lexical.size(), exponent);
    // An exponent exceeding size_t also exceeds every possible mantissa position.
    if (parsed.ec == std::errc::result_out_of_range) { return !negative_exponent; }
  }
  if (negative_exponent) {
    return first_nonzero < integer_digits && exponent <= integer_digits - first_nonzero - 1U;
  }
  return first_nonzero < integer_digits || exponent >= first_nonzero - integer_digits + 1U;
}

// Diagnose only known finite-width overflow; preserve underflow and opaque native expressions.
void validate_floating_default(const member& field) {
  const auto& type = field.type_name_list.front();
  if (field.modifier != member::modifier_type::none || type.type != object_type::primitive ||
      (type.name != "float" && type.name != "double")) { return; }
  const auto text = schema_version::trim(field.default_value);
  if (!canonical_floating_default(text)) { return; }
  float single{};
  double extended{};
  const auto parsed = type.name == "float" ?
      std::from_chars(text.data(), text.data() + text.size(), single) :
      std::from_chars(text.data(), text.data() + text.size(), extended);
  // from_chars reports both overflow and underflow as out_of_range. Values with
  // magnitude below one retain their established native underflow behavior.
  if (parsed.ec == std::errc::result_out_of_range && floating_default_at_least_one(text)) {
    throw std::invalid_argument{"Floating default exceeds the declared " + type.name + " range"};
  }
}

// Recognize simple qualified names without accepting native calls, casts or member expressions.
bool qualified_enum_default(std::string_view text) {
  if (text.starts_with("::")) { text.remove_prefix(2U); }
  const auto first_character = [](char character) {
    return character == '_' || (character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z');
  };
  std::size_t components{};
  while (!text.empty()) {
    if (!first_character(text.front())) { return false; }
    std::size_t end{1U};
    while (end < text.size() && (first_character(text[end]) ||
        (text[end] >= '0' && text[end] <= '9'))) { ++end; }
    ++components;
    text.remove_prefix(end);
    if (text.empty()) { return components > 1U; }
    if (!text.starts_with("::")) { return false; }
    text.remove_prefix(2U);
  }
  return false;
}

// Validate closed enum inventories only when a default explicitly names its declared enum type.
void validate_qualified_enum_default(const member& field) {
  const auto& type = field.type_name_list.front();
  if (field.modifier != member::modifier_type::none || type.type != object_type::enum_type ||
      !type.resolved_node) { return; }
  const auto text = schema_version::trim(field.default_value);
  if (!qualified_enum_default(text)) { return; }
  std::string_view qualified{text};
  if (qualified.starts_with("::")) { qualified.remove_prefix(2U); }
  const auto separator = qualified.rfind("::");
  const auto owner = qualified.substr(0, separator);
  const auto alternative = qualified.substr(separator + 2U);
  const auto& declaration = static_cast<const enum_node&>(*type.resolved_node);
  if (owner != type.name && owner != declaration.name && owner != declaration.get_full_name()) { return; }
  if (std::find(declaration.enum_name_list.begin(), declaration.enum_name_list.end(), alternative) ==
      declaration.enum_name_list.end()) {
    throw std::invalid_argument{"Unknown enumerator in default for " + type.name + ": " +
        std::string{alternative}};
  }
}

// Retain native scalar expressions while checking known scalar contracts and portable aggregates.
void validate_portable_default(const rohit::type_check::schema_input_buffer auto& input,
                              member& field, const class_node& owner,
                              bool empty_record_default_enabled = false,
                              bool owner_parameter_default_enabled = false) {
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
  const bool owner_default = owner_parameter_default_enabled &&
      field.modifier == member::modifier_type::none && empty_construction_default(field.default_value);
  const auto text = field.modifier == member::modifier_type::array ? "{" + field.default_value + "}" : field.default_value;
  try {
    if (!aggregate && !parameter && !owner_default) {
      validate_integer_default(field);
      validate_floating_default(field);
      validate_qualified_enum_default(field);
      if (field.strict_default_enabled) {
        static_cast<void>(writer::native::literal(field));
        if (field.bit_width != 0) {
          const auto literal = writer::native::literal(field);
          std::uint64_t number{};
          const auto parsed = std::from_chars(literal.data(), literal.data() + literal.size(), number);
          const auto maximum = std::numeric_limits<std::uint64_t>::max() >>
              (std::numeric_limits<std::uint64_t>::digits - field.bit_width);
          if (parsed.ec != std::errc{} || parsed.ptr != literal.data() + literal.size() || number > maximum) {
            throw std::invalid_argument{"Default exceeds the declared bit-field width"};
          }
        }
      }
      return;
    }
    field.initializer = bind_value_initializer(input, field, owner, text, 0, nullptr,
        empty_record_default_enabled, owner_default);
  } catch (const std::invalid_argument& error) {
    throw exception::bad_member_spec{input, member_diagnostic(field, error.what())};
  } catch (const exception::bad_member_spec& error) {
    throw exception::bad_member_spec{input, member_diagnostic(field, error.what())};
  }
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
