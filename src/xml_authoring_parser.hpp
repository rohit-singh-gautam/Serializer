// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later

// Keep optional XML/package syntax unavailable to older declaring files, including includes.
void require_xml_authoring_language(const rohit::type_check::schema_input_buffer auto& input) {
  constexpr language_version introduced{1U, 20U, 0U};
  if (active_language_version < introduced) {
    throw exception::bad_member_spec{input, "XML authoring and packages require serializer version 1.20.0"};
  }
}

// Read a bounded metadata string without interpreting target-language source or fetching resources.
std::string parse_xml_authoring_string(const rohit::type_check::schema_input_buffer auto& input) {
  skip_whitespace_and_comment(input);
  check_and_increase(input, '"');
  constexpr std::size_t maximum_authoring_string_bytes = 65536U;
  std::string result;
  while (!input.full() && *input != '"') {
    auto value = *input;
    ++input;
    if (value == '\\') {
      if (input.full()) { throw exception::bad_member_spec{input, "Unterminated XML metadata escape"}; }
      value = *input;
      ++input;
      if (value != '\\' && value != '"' && value != '/') {
        throw exception::bad_member_spec{input, "XML metadata supports only quote, slash and backslash escapes"};
      }
    }
    if (value < 0x20U || result.size() == maximum_authoring_string_bytes) {
      throw exception::bad_member_spec{input, "XML metadata contains control bytes or exceeds its string limit"};
    }
    result.push_back(static_cast<char>(value));
  }
  check_and_increase(input, '"');
  skip_whitespace_and_comment(input);
  return result;
}

// Parse one checked nonnegative occurrence bound, rejecting signs and arithmetic overflow.
std::size_t parse_package_occurrence(const rohit::type_check::schema_input_buffer auto& input) {
  skip_whitespace_and_comment(input);
  std::size_t result{};
  bool present{};
  constexpr std::size_t decimal_base = 10U;
  while (!input.full() && *input >= '0' && *input <= '9') {
    const auto digit = static_cast<std::size_t>(*input - '0');
    if (result > (std::numeric_limits<std::size_t>::max() - digit) / decimal_base) {
      throw exception::bad_member_spec{input, "Package occurrence bound overflows size_t"};
    }
    result = result * decimal_base + digit;
    present = true;
    ++input;
  }
  if (!present) { throw exception::bad_member_spec{input, "Package occurrence requires a nonnegative integer"}; }
  skip_whitespace_and_comment(input);
  return result;
}

// Resolve a closed placement keyword without inferring XML shape from primitive types.
xml_field_placement parse_xml_placement(const rohit::type_check::schema_input_buffer auto& input) {
  const auto keyword = parse_identifier_impl(input);
  if (keyword == "element") { return xml_field_placement::element; }
  if (keyword == "attribute") { return xml_field_placement::attribute; }
  if (keyword == "text") { return xml_field_placement::text; }
  throw exception::bad_member_spec{input, "XML placement must be element, attribute or text"};
}

// Read namespace/class defaults as sparse compiler metadata; duplicate keys fail transactionally.
void parse_xml_authoring_defaults(const rohit::type_check::schema_input_buffer auto& input,
                                  xml_authoring_options& destination, bool class_scope) {
  require_xml_authoring_language(input);
  auto candidate = destination;
  parse_identifier_impl(input);
  skip_whitespace_and_comment(input);
  check_and_increase(input, '(');
  skip_whitespace_and_comment(input);
  if (input.full() || *input == ')') { throw exception::bad_member_spec{input, "XML defaults cannot be empty"}; }
  do {
    const auto key = parse_identifier_impl(input);
    skip_whitespace_and_comment(input);
    check_and_increase(input, '(');
    skip_whitespace_and_comment(input);
    if (key == "namespace" || (key == "root" && class_scope)) {
      auto& slot = key == "namespace" ? candidate.namespace_uri : candidate.root;
      if (slot) { throw exception::bad_member_spec{input, "Duplicate XML default: " + key}; }
      slot = parse_xml_authoring_string(input);
      if (key == "root" && slot->empty()) { throw exception::bad_member_spec{input, "XML root name cannot be empty"}; }
    } else if (key == "scalars") {
      if (candidate.scalar_placement) { throw exception::bad_member_spec{input, "Duplicate XML scalar placement"}; }
      candidate.scalar_placement = parse_xml_placement(input);
      if (*candidate.scalar_placement == xml_field_placement::text) {
        throw exception::bad_member_spec{input, "XML text placement must select one field explicitly"};
      }
    } else if (key == "unknown") {
      if (candidate.unknown) { throw exception::bad_member_spec{input, "Duplicate XML unknown-content policy"}; }
      const auto policy = parse_identifier_impl(input);
      if (policy == "reject") { candidate.unknown = xml_unknown_policy::reject; }
      else if (policy == "skip") { candidate.unknown = xml_unknown_policy::skip; }
      else if (policy == "preserve") { candidate.unknown = xml_unknown_policy::preserve; }
      else { throw exception::bad_member_spec{input, "XML unknown policy must be reject, skip or preserve"}; }
    } else {
      throw exception::bad_member_spec{input, "Unknown XML default or invalid scope: " + key};
    }
    skip_whitespace_and_comment(input);
    check_and_increase(input, ')');
    skip_whitespace_and_comment(input);
    if (input.full() || *input != ',') { break; }
    ++input;
    skip_whitespace_and_comment(input);
  } while (true);
  check_and_increase(input, ')');
  skip_whitespace_and_comment(input);
  check_and_increase(input, ';');
  destination = std::move(candidate);
}

// Parse an explicit field override without duplicating ordinary member names in configuration.
xml_field_options parse_xml_field_options(const rohit::type_check::schema_input_buffer auto& input) {
  require_xml_authoring_language(input);
  parse_identifier_impl(input);
  skip_whitespace_and_comment(input);
  check_and_increase(input, '(');
  skip_whitespace_and_comment(input);
  xml_field_options result;
  if (input.full() || *input == ')') { throw exception::bad_member_spec{input, "XML field mapping cannot be empty"}; }
  do {
    if (next_keyword(input, "element") || next_keyword(input, "attribute") || next_keyword(input, "text")) {
      if (result.placement) { throw exception::bad_member_spec{input, "Duplicate XML field placement"}; }
      result.placement = parse_xml_placement(input);
    } else if (next_keyword(input, "list") || next_keyword(input, "qname") || next_keyword(input, "prefix_list")) {
      const auto key = parse_identifier_impl(input);
      auto& slot = key == "list" ? result.list : key == "qname" ? result.qname : result.prefix_list;
      if (slot) { throw exception::bad_member_spec{input, "Duplicate XML lexical mapping: " + key}; }
      slot = true;
    } else {
      const auto key = parse_identifier_impl(input);
      skip_whitespace_and_comment(input);
      check_and_increase(input, '(');
      std::optional<std::string>* slot{};
      if (key == "name") { slot = &result.name; }
      else if (key == "namespace") { slot = &result.namespace_uri; }
      else if (key == "wrapper") { slot = &result.wrapper; }
      else { throw exception::bad_member_spec{input, "Unknown XML field mapping: " + key}; }
      if (*slot) { throw exception::bad_member_spec{input, "Duplicate XML field mapping: " + key}; }
      *slot = parse_xml_authoring_string(input);
      if (key != "namespace" && (*slot)->empty()) { throw exception::bad_member_spec{input, "XML mapped name cannot be empty"}; }
      check_and_increase(input, ')');
    }
    skip_whitespace_and_comment(input);
    if (input.full() || *input != ',') { break; }
    ++input;
    skip_whitespace_and_comment(input);
  } while (true);
  check_and_increase(input, ')');
  if (result.placement == xml_field_placement::text && (result.name || result.wrapper || result.namespace_uri)) {
    throw exception::bad_member_spec{input, "XML text has no element name, namespace or wrapper"};
  }
  if (result.wrapper && result.placement == xml_field_placement::attribute) {
    throw exception::bad_member_spec{input, "XML attributes cannot have collection wrappers"};
  }
  if ((result.qname && result.prefix_list) || (result.list && (result.qname || result.prefix_list))) {
    throw exception::bad_member_spec{input, "XML lexical mappings list, qname and prefix_list are mutually exclusive"};
  }
  return result;
}

// Associate one logical namespace with a fixed local schema, retaining its declaring source.
void parse_xml_schema_binding(const rohit::type_check::schema_input_buffer auto& input,
                               namespace_node* space) {
  require_xml_authoring_language(input);
  if (!space) { throw exception::bad_namespace{input, "using schema requires a namespace scope"}; }
  if (space->xml.schema) { throw exception::bad_namespace{input, "Only one fixed schema binding is allowed per namespace"}; }
  parse_identifier_impl(input);
  skip_whitespace_and_comment(input);
  if (parse_identifier_impl(input) != "schema") { throw exception::bad_namespace{input, "Expected using schema"}; }
  auto path = parse_xml_authoring_string(input);
  if (path.empty()) { throw exception::bad_namespace{input, "Fixed schema path cannot be empty"}; }
  check_and_increase(input, ';');
  space->xml.schema = xml_schema_binding{std::filesystem::path{path},
      active_behavior_source ? std::string{active_behavior_source->path} : std::string{}};
}

// Read a fixed package setting or its explicit caller-supplied dynamic value.
std::optional<std::string> parse_package_dynamic_string(
    const rohit::type_check::schema_input_buffer auto& input) {
  skip_whitespace_and_comment(input);
  if (!input.full() && *input == '"') { return parse_xml_authoring_string(input); }
  if (parse_identifier_impl(input) != "dynamic") {
    throw exception::bad_member_spec{input, "Package name/content type requires a quoted value or dynamic"};
  }
  return {};
}

// Parse one typed/opaque role; code generation validates codec support and the complete graph.
package_part_declaration parse_package_part(const rohit::type_check::schema_input_buffer auto& input,
                                             namespace_node* space) {
  if (parse_identifier_impl(input) != "part") { throw exception::bad_member_spec{input, "Expected a package part"}; }
  skip_whitespace_and_comment(input);
  package_part_declaration result;
  result.name = parse_identifier_impl(input);
  skip_whitespace_and_comment(input);
  check_and_increase(input, ':');
  skip_whitespace_and_comment(input);
  if (next_keyword(input, "bytes")) {
    parse_identifier_impl(input);
    result.codec = "bytes";
  } else { result.value_type = parse_type_expression(input, space); }
  skip_whitespace_and_comment(input);
  check_and_increase(input, '{');
  skip_whitespace_and_comment(input);
  std::set<std::string> settings;
  while (!input.full() && *input != '}') {
    const auto key = parse_identifier_impl(input);
    if (key != "key" && key != "reference" && !settings.insert(key).second) {
      throw exception::bad_member_spec{input, "Duplicate package part setting: " + key};
    }
    skip_whitespace_and_comment(input);
    check_and_increase(input, '(');
    skip_whitespace_and_comment(input);
    if (key == "policy" || key == "key" || key == "reference") {
      constexpr language_version introduced{1U, 21U, 0U};
      if (active_language_version < introduced) {
        throw exception::bad_member_spec{input, "Package policies and references require serializer version 1.21.0"};
      }
      if (key == "policy") {
        result.profile_policy = parse_identifier_impl(input);
        if (result.profile_policy != "fixed" && result.profile_policy != "defaults") {
          throw exception::bad_member_spec{input, "Package policy must be fixed or defaults"};
        }
      } else {
        constexpr std::size_t maximum_package_selectors = 1024U;
        if (result.keys.size() + result.references.size() >= maximum_package_selectors) {
          throw exception::bad_member_spec{input, "Package selector count exceeds its limit"};
        }
        auto scope = parse_xml_authoring_string(input);
        if (scope.empty()) { throw exception::bad_member_spec{input, "Package key scope cannot be empty"}; }
        check_and_increase(input, ',');
        skip_whitespace_and_comment(input);
        auto field = parse_identifier_impl(input);
        if (key == "key") { result.keys.push_back({std::move(scope), std::move(field)}); }
        else {
          skip_whitespace_and_comment(input);
          check_and_increase(input, ',');
          skip_whitespace_and_comment(input);
          auto target = parse_identifier_impl(input);
          result.references.push_back({std::move(scope), std::move(field), std::move(target)});
        }
      }
    } else if (key == "name" || key == "content_type") {
      auto& slot = key == "name" ? result.part_name : result.content_type;
      slot = parse_package_dynamic_string(input);
      if (slot && slot->empty()) { throw exception::bad_member_spec{input, "Fixed package names and content types cannot be empty"}; }
    } else if (key == "relationship" || key == "id") {
      auto value = parse_xml_authoring_string(input);
      if (value.empty()) { throw exception::bad_member_spec{input, "Package relationship identity cannot be empty"}; }
      if (key == "relationship") { result.relationship = std::move(value); }
      else { result.relationship_id = std::move(value); }
    } else if (key == "source") {
      result.source = !input.full() && *input == '"' ? parse_xml_authoring_string(input) : parse_identifier_impl(input);
    } else if (key == "compression") {
      result.compression = parse_identifier_impl(input);
      if (result.compression != "stored" && result.compression != "deflate") {
        throw exception::bad_member_spec{input, "Package compression must be stored or deflate"};
      }
    } else if (key == "codec") {
      result.codec = parse_identifier_impl(input);
      if (result.codec != "xml" && result.codec != "bytes" && result.codec != "json" && result.codec != "binary_none") {
        throw exception::bad_member_spec{input, "Unsupported package codec"};
      }
    } else if (key == "occurs") {
      result.min_occurs = parse_package_occurrence(input);
      check_and_increase(input, ',');
      skip_whitespace_and_comment(input);
      if (next_keyword(input, "unbounded")) { parse_identifier_impl(input); result.max_occurs.reset(); }
      else { result.max_occurs = parse_package_occurrence(input); }
      if (result.max_occurs && (*result.max_occurs == 0U || result.min_occurs > *result.max_occurs)) {
        throw exception::bad_member_spec{input, "Package occurrence interval must be nonempty"};
      }
    } else { throw exception::bad_member_spec{input, "Unknown package part setting: " + key}; }
    skip_whitespace_and_comment(input);
    check_and_increase(input, ')');
    skip_whitespace_and_comment(input);
    check_and_increase(input, ';');
    skip_whitespace_and_comment(input);
  }
  check_and_increase(input, '}');
  if (!settings.contains("name") || !settings.contains("content_type") || result.relationship.empty()) {
    throw exception::bad_member_spec{input, "Package parts require name, content_type and relationship declarations"};
  }
  if (result.value_type.has_value() == (result.codec == "bytes")) {
    throw exception::bad_member_spec{input, "Opaque bytes parts and typed model codecs cannot be interchanged"};
  }
  return result;
}

// Retain a declarative multipart contract without inserting any per-instance model storage.
std::unique_ptr<package_node> parse_package(const rohit::type_check::schema_input_buffer auto& input,
                                           namespace_node* space) {
  require_xml_authoring_language(input);
  parse_identifier_impl(input);
  skip_whitespace_and_comment(input);
  auto result = std::make_unique<package_node>(parse_identifier_impl(input), space);
  skip_whitespace_and_comment(input);
  check_and_increase(input, '{');
  skip_whitespace_and_comment(input);
  std::set<std::string> roles;
  constexpr std::size_t maximum_package_roles = 1024U;
  while (!input.full() && *input != '}') {
    if (result->parts.size() == maximum_package_roles) { throw exception::bad_member_spec{input, "Package role count exceeds its limit"}; }
    auto part = parse_package_part(input, space);
    if (!roles.insert(part.name).second) { throw exception::bad_member_spec{input, "Duplicate package role: " + part.name}; }
    result->parts.push_back(std::move(part));
    skip_whitespace_and_comment(input);
  }
  check_and_increase(input, '}');
  if (result->parts.empty()) { throw exception::bad_member_spec{input, "A package requires at least one part role"}; }
  for (const auto& part : result->parts) {
    if (part.source != "/" && (!roles.contains(part.source) || part.source == part.name)) {
      throw exception::bad_member_spec{input, "Package relationship source must name the root or another declared role"};
    }
  }
  return result;
}
