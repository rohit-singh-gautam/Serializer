#include <rohit/schema_compatibility.hpp>

#include "native_schema.hpp"
#include "protobuf_schema.hpp"
#include "schema_version.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rohit::serializer {
namespace {
using declarations = std::map<std::string, const syntax_node*>;

// Reject misspelled and repeated JSON policy keys rather than silently dropping constraints.
void policy_key(std::set<std::string>& seen, std::string_view key) {
  if (!seen.emplace(key).second) {
    throw std::invalid_argument{"Repeated compatibility policy key: " + std::string{key}};
  }
}

struct reservation_input {
  reserved_fields value{};
  std::set<std::string> seen{};

  // Decode a reservation with explicit type identity; empty ID/name lists are permitted.
  template <typename Protocol>
  void serialize_in(Protocol& decoder) {
    decoder.struct_serialize_in(this);
    if (!seen.contains("type")) {
      throw std::invalid_argument{"A reservation requires type"};
    }
  }
  // Accept only the three version-1 reservation properties.
  template <typename Protocol>
  void serialize_in_member_by_name(Protocol& decoder, std::string_view key) {
    policy_key(seen, key);
    if (key == "type") {
      decoder.serialize_in(value.type);
    } else if (key == "ids") {
      decoder.serialize_in(value.ids);
    } else if (key == "names") {
      decoder.serialize_in(value.names);
    } else {
      throw std::invalid_argument{"Unknown reservation key: " + std::string{key}};
    }
  }
};

struct policy_input {
  std::uint32_t version{};
  std::vector<reservation_input> reservations{};
  std::set<std::string> seen{};

  // Require an explicit policy version independently of the schema-language version.
  template <typename Protocol>
  void serialize_in(Protocol& decoder) {
    decoder.struct_serialize_in(this);
    if (version != 1) {
      throw std::invalid_argument{"Compatibility policy version must be 1"};
    }
  }
  // Decode bounded JSON without accepting extension keys accidentally.
  template <typename Protocol>
  void serialize_in_member_by_name(Protocol& decoder, std::string_view key) {
    policy_key(seen, key);
    if (key == "version") {
      decoder.serialize_in(version);
    } else if (key == "reserved_fields") {
      decoder.serialize_in(reservations);
    } else {
      throw std::invalid_argument{"Unknown compatibility policy key: " + std::string{key}};
    }
  }
};

// Validate library-supplied policies as strictly as policies read by the CLI.
void validate_policy(const compatibility_policy& policy) {
  std::set<std::string> scopes{};
  for (const auto& reservation : policy.reservations) {
    if (reservation.type.empty() || !scopes.insert(reservation.type).second) {
      throw std::invalid_argument{"Empty or repeated reservation type"};
    }
    // Resolve names exactly as the schema parser exposes them; a leading :: must not go unnoticed.
    const auto is_initial = [](char ch) {
      return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
    };
    std::size_t offset{};
    while (offset < reservation.type.size()) {
      if (!is_initial(reservation.type[offset++])) {
        throw std::invalid_argument{
            "Reservation type must be a qualified schema name without leading ::"};
      }
      while (offset < reservation.type.size() &&
             (is_initial(reservation.type[offset]) ||
              (reservation.type[offset] >= '0' && reservation.type[offset] <= '9'))) {
        ++offset;
      }
      if (offset == reservation.type.size()) {
        break;
      }
      if (reservation.type.substr(offset, 2) != "::" || offset + 2 == reservation.type.size()) {
        throw std::invalid_argument{"Malformed reservation type"};
      }
      offset += 2;
    }
    std::set<std::uint32_t> ids{};
    for (const auto id : reservation.ids) {
      if (id == 0 || id > constants::variable_four_byte_max || !ids.insert(id).second) {
        throw std::invalid_argument{"Invalid or repeated reserved field ID"};
      }
    }
    std::set<std::string> names{};
    for (const auto& name : reservation.names) {
      if (name.empty() || !names.insert(name).second) {
        throw std::invalid_argument{"Empty or repeated reserved wire name"};
      }
    }
  }
}

// Flatten namespace blocks using resolved identities, including reopened and included scopes.
void collect(const std::vector<std::unique_ptr<syntax_node>>& statements, declarations& result) {
  for (const auto& statement : statements) {
    if (statement->type == object_type::generic_definition) { continue; }
    if (statement->type == object_type::namespace_type) {
      collect(static_cast<const namespace_node&>(*statement).statements, result);
    } else {
      result.emplace(statement->get_full_name(), statement.get());
    }
  }
}

// Compare resolved type identities, avoiding false differences from relative type spellings.
std::string type_identity(const type_name& type) {
  if (type.is_digest()) {
    return "digest:" + std::to_string(static_cast<unsigned>(type.digest)) + ":" +
           std::to_string(type.digest_extent);
  }
  return type.resolved_node ? type.resolved_node->get_full_name() : type.name;
}

// Split a flat literal list without rewriting quoted text or interpreting arbitrary C++ expressions.
std::optional<std::vector<std::string>> fixed_array_literals(std::string_view input) {
  std::vector<std::string> result{};
  std::size_t begin{};
  char quote{};
  bool escaped{};
  for (std::size_t index = 0; index < input.size(); ++index) {
    const auto character = input[index];
    if (quote != 0) {
      if (escaped) {
        escaped = false;
      } else if (character == '\\') {
        escaped = true;
      } else if (character == quote) {
        quote = 0;
      }
      continue;
    }
    if (character == '"' ||
        (character == '\'' && schema_version::trim(input.substr(begin, index - begin)).empty())) {
      quote = character;
    } else if (character == '{' || character == '}' || character == '(' || character == ')' ||
               character == '[' || character == ']') {
      return std::nullopt;
    } else if (character == ',') {
      auto literal = schema_version::trim(input.substr(begin, index - begin));
      if (literal.empty()) {
        return std::nullopt;
      }
      result.push_back(std::move(literal));
      begin = index + 1;
    }
  }
  if (quote != 0) {
    return std::nullopt;
  }
  auto literal = schema_version::trim(input.substr(begin));
  if (!literal.empty()) {
    result.push_back(std::move(literal));
  } else if (result.empty()) {
    return std::nullopt;
  }
  return result;
}

// Remove only valid numeric digit separators, preserving whitespace and every nonnumeric token.
std::string fixed_array_numeric_token(std::string text) {
  const auto digits = text.starts_with("0x") || text.starts_with("0X") ||
                      text.starts_with("-0x") || text.starts_with("-0X")
      ? std::string_view{"0123456789abcdefABCDEF"} : std::string_view{"0123456789"};
  std::string result{};
  for (std::size_t index = 0; index < text.size(); ++index) {
    if (text[index] == '\'') {
      if (index == 0 || index + 1 == text.size() ||
          digits.find(text[index - 1]) == std::string_view::npos ||
          digits.find(text[index + 1]) == std::string_view::npos) {
        throw std::invalid_argument{"Not a numeric digit separator"};
      }
    } else {
      result += text[index];
    }
  }
  const bool negative = result.starts_with('-');
  auto magnitude = std::string_view{result};
  if (negative) {
    magnitude.remove_prefix(1);
  }
  if (magnitude.starts_with("0x") || magnitude.starts_with("0X")) {
    if (negative) {
      // Unary minus may wrap an unsigned C++ hex literal before conversion to the field type.
      throw std::invalid_argument{"Keep negative hexadecimal expressions conservative"};
    }
    magnitude.remove_prefix(2);
    std::uint64_t value{};
    const auto parsed = std::from_chars(magnitude.data(), magnitude.data() + magnitude.size(), value, 16);
    if (magnitude.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != magnitude.data() + magnitude.size()) {
      throw std::invalid_argument{"Not an integer hexadecimal literal"};
    }
    return (negative ? "-" : "") + std::to_string(value);
  }
  return result;
}

// Compare safe scalar defaults by value while leaving arbitrary aggregate/expression text conservative.
std::string fixed_array_default(const member& field) {
  if (field.modifier != member::modifier_type::array || field.fixed_extent == 0 ||
      field.type_name_list.size() != 1 || field.default_value.empty()) {
    return field.default_value;
  }
  const auto& type = field.type_name_list.front();
  if ((type.type != object_type::primitive && type.type != object_type::enum_type) ||
      (type.type == object_type::enum_type && !type.resolved_node)) {
    return field.default_value;
  }
  const auto literals = fixed_array_literals(field.default_value);
  if (!literals || literals->size() != field.fixed_extent) {
    return field.default_value;
  }
  std::string result{};
  try {
    for (auto text : *literals) {
      auto scalar = field;
      scalar.modifier = member::modifier_type::none;
      scalar.magic = false;
      if (type.name == "char" && type.type == object_type::primitive) {
        if (text.size() == 6 && text.starts_with("'\\x") && text.back() == '\'') {
          unsigned byte{};
          const auto parsed = std::from_chars(text.data() + 3, text.data() + 5, byte, 16);
          if (parsed.ec != std::errc{} || parsed.ptr != text.data() + 5) {
            return field.default_value;
          }
          text = std::to_string(byte);
        }
        if (!text.starts_with('\'')) {
          scalar.type_name_list.front().name = "uint8";
          text = fixed_array_numeric_token(std::move(text));
        }
      } else if (type.type == object_type::primitive &&
                 (type.name.starts_with("int") || type.name.starts_with("uint") ||
                  type.name == "float" || type.name == "double")) {
        text = fixed_array_numeric_token(std::move(text));
      }
      scalar.default_value = std::move(text);
      auto normalized = writer::native::literal(scalar);
      if (type.type == object_type::primitive && (type.name == "float" || type.name == "double")) {
        // An integer -0 initializer converts to positive floating zero in generated C++.
        if (normalized == "-0") {
          normalized = "0";
        }
        float single{};
        double value{};
        const auto parsed = type.name == "float"
            ? std::from_chars(normalized.data(), normalized.data() + normalized.size(), single)
            : std::from_chars(normalized.data(), normalized.data() + normalized.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != normalized.data() + normalized.size()) {
          return field.default_value;
        }
        constexpr auto capacity = std::numeric_limits<double>::max_digits10 +
            std::numeric_limits<int>::digits10 + sizeof("-0.e-");
        std::array<char, capacity> canonical{};
        const auto converted = type.name == "float"
            ? std::to_chars(canonical.data(), canonical.data() + canonical.size(), single)
            : std::to_chars(canonical.data(), canonical.data() + canonical.size(), value);
        if (converted.ec != std::errc{}) {
          return field.default_value;
        }
        normalized.assign(canonical.data(), converted.ptr);
      } else if (type.type == object_type::primitive && type.name.starts_with("int")) {
        std::int64_t value{};
        const auto parsed = std::from_chars(normalized.data(), normalized.data() + normalized.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != normalized.data() + normalized.size()) {
          return field.default_value;
        }
        normalized = std::to_string(value);
      }
      if (!result.empty()) {
        result += ',';
      }
      result += normalized;
    }
  } catch (const std::invalid_argument&) {
    return field.default_value;
  }
  return result;
}

struct field_contract {
  std::uint32_t id{};
  std::string name{};
  std::string shape{};
  std::string default_value{};
  std::vector<std::pair<std::string, std::string>> alternatives{};
  std::uint64_t fixed_extent{};
  compact_encoding compact{compact_encoding::none};
};

// Preserve parent-before-member order, field identity, map keys, and ordered union alternatives.
std::vector<field_contract> fields(const class_node& type, std::string_view format = {}) {
  std::vector<field_contract> result{};
  for (const auto& base : type.parents) {
    result.push_back(
        {base.id, base.display_name, "parent:" + base.parent_class->get_full_name(), {}, {}});
  }
  for (const auto& field : type.member_list) {
    if (!format.empty() && field.omits(format)) { continue; }
    field_contract contract{field.id, field.display_name, {}, fixed_array_default(field), {}};
    contract.fixed_extent = field.fixed_extent;
    contract.compact = field.compact;
    switch (field.modifier) {
    case member::modifier_type::none:
      contract.shape = "value:" + type_identity(field.type_name_list.front());
      break;
    case member::modifier_type::array:
      contract.shape = "array:" + type_identity(field.type_name_list.front());
      break;
    case member::modifier_type::map:
      contract.shape = "map:" + (field.key_node ? field.key_node->get_full_name() : field.key) +
                       ":" + type_identity(field.type_name_list.front());
      break;
    case member::modifier_type::variant:
      contract.shape = "union";
      for (const auto& alternative : field.type_name_list) {
        contract.alternatives.emplace_back(alternative.enum_name, type_identity(alternative));
      }
      break;
    }
    result.push_back(std::move(contract));
  }
  return result;
}

// A reserved union base name also protects its colon-qualified native wire keys.
bool reserved_name(const reserved_fields& reserved, std::string_view name) {
  return std::any_of(reserved.names.begin(), reserved.names.end(), [&](const auto& retired) {
    return name == retired || (name.starts_with(retired) && name.size() > retired.size() &&
                               name[retired.size()] == ':');
  });
}

// Find reservations by stable qualified class identity; removed scopes can remain in policy files.
const reserved_fields* reservation_for(const compatibility_policy& policy, std::string_view type) {
  const auto found = std::find_if(policy.reservations.begin(), policy.reservations.end(),
                                  [&](const auto& value) { return value.type == type; });
  return found == policy.reservations.end() ? nullptr : &*found;
}

// Require retirement history to preserve both numeric identity and named-codec identity.
bool retired(const reserved_fields* reservation, const field_contract& field) {
  return reservation &&
         std::find(reservation->ids.begin(), reservation->ids.end(), field.id) !=
             reservation->ids.end() &&
         reserved_name(*reservation, field.name);
}

// Merge class-local permanent reservations with the optional external retirement policy.
reserved_fields class_reservations(const class_node& object, const compatibility_policy& policy) {
  reserved_fields result{object.get_full_name(), object.reserved_ids, object.reserved_names};
  if (const auto* external = reservation_for(policy, result.type)) {
    result.ids.insert(result.ids.end(), external->ids.begin(), external->ids.end());
    result.names.insert(result.names.end(), external->names.begin(), external->names.end());
  }
  return result;
}

// Materialize a historical contract for comparison without changing either resolved schema.
class_node historical_contract(const class_node& object, std::string_view revision) {
  class_node result{object_type::class_type, std::string{object.name}, object.parent_namespace,
                    object.attributes, std::vector<parent>{object.parents}};
  const auto& version = *object.version_member();
  auto discriminator = version;
  discriminator.version = false;
  discriminator.default_value
      .clear(); // The prefix value is checked separately from field defaults.
  result.member_list.push_back(std::move(discriminator));
  for (const auto& field : object.member_list) {
    if (!field.version && schema_version::active(field, version, revision)) {
      result.member_list.push_back(field);
    }
  }
  result.reserved_ids = object.reserved_ids;
  result.reserved_names = object.reserved_names;
  return result;
}

// Append a deterministic diagnostic, marking the reader directions requiring review.
void issue(std::vector<compatibility_issue>& result, const std::string& path, std::string message,
           bool old_reader = true, bool new_reader = true) {
  result.push_back({path, std::move(message), old_reader, new_reader});
}

// Reject reuse in the new schema, even when the retired field disappeared many revisions ago.
void check_reservations(std::vector<compatibility_issue>& result, const std::string& path,
                        const std::vector<field_contract>& current,
                        const reserved_fields* reservation) {
  if (!reservation) {
    return;
  }
  for (const auto& field : current) {
    bool conflicts = reserved_name(*reservation, field.name) ||
                     std::find(reservation->ids.begin(), reservation->ids.end(), field.id) !=
                         reservation->ids.end();
    for (const auto& [name, type] : field.alternatives) {
      static_cast<void>(type);
      conflicts = conflicts || reserved_name(*reservation, field.name + ":" + name);
    }
    if (conflicts) {
      issue(result, path + "." + field.name, "Field or parent reuses a reserved ID or wire name");
    }
  }
}

// Declaration-order ordinals cannot be reused safely; append-only changes affect older readers.
template <typename Value>
void check_ordinals(std::vector<compatibility_issue>& result, const std::string& path,
                    const std::vector<Value>& previous, const std::vector<Value>& current,
                    std::string_view kind) {
  const auto common = std::min(previous.size(), current.size());
  if (!std::equal(previous.begin(), previous.begin() + static_cast<std::ptrdiff_t>(common),
                  current.begin())) {
    issue(result, path, std::string{kind} + " ordinal changed, reordered, or reused");
  }
  if (current.size() > previous.size()) {
    issue(result, path,
          std::string{kind} + " alternatives added; older readers cannot preserve them", true,
          false);
  } else if (current.size() < previous.size()) {
    issue(result, path, std::string{kind} + " alternatives removed; retain retired ordinal slots",
          true, true);
  }
}

// Compare class contracts conservatively while accounting for each codec's unknown-field rules.
void check_class(std::vector<compatibility_issue>& result, const class_node& previous,
                 const class_node& current, compatibility_protocol protocol,
                 const compatibility_policy& policy) {
  const auto path = previous.get_full_name();
  const std::string_view format = protocol == compatibility_protocol::json ? "json" :
      protocol == compatibility_protocol::binary_none ? "binary_none" :
      protocol == compatibility_protocol::binary_integer ? "binary_integer" :
      protocol == compatibility_protocol::binary_string ? "binary_string" : "protobuf";
  const bool old_magic = previous.has_magic() && !previous.magic_omits(format);
  const bool new_magic = current.has_magic() && !current.magic_omits(format);
  const bool typed_magic_changed = previous.magic_field.has_value() != current.magic_field.has_value() ||
      (previous.magic_field && current.magic_field &&
       (type_identity(previous.magic_field->type_name_list.front()) !=
            type_identity(current.magic_field->type_name_list.front()) ||
        previous.magic_field->default_value != current.magic_field->default_value));
  if (old_magic != new_magic || (old_magic && (typed_magic_changed ||
      previous.magic_bytes != current.magic_bytes ||
      (protocol == compatibility_protocol::protobuf_binary && previous.magic_id != current.magic_id)))) {
    issue(result, path, "Magic header added, removed, or changed; select a separate contract");
  }
  const auto* before_version = previous.version_member();
  const auto* after_version = current.version_member();
  if (before_version || after_version) {
    if (!before_version || !after_version || before_version->id != after_version->id ||
        before_version->display_name != after_version->display_name ||
        before_version->type_name_list.front().name != after_version->type_name_list.front().name) {
      issue(result, path,
            "Version discriminator added, removed, or changed; select a separate contract");
      return;
    }
    const auto& type = before_version->type_name_list.front().name;
    const auto old_revision = schema_version::parse(type, before_version->default_value);
    const auto new_revision = schema_version::parse(type, after_version->default_value);
    const auto old_minimum = schema_version::parse(type, schema_version::minimum(*before_version));
    const auto new_minimum = schema_version::parse(type, schema_version::minimum(*after_version));
    const auto reservations = class_reservations(current, policy);
    const auto all_new = fields(current);
    for (const auto& before : fields(previous)) {
      if (std::none_of(all_new.begin(), all_new.end(),
                       [&](const auto& after) { return before.id == after.id; }) &&
          !retired(&reservations, before)) {
        issue(result, path + "." + before.name,
              "Removed historical field requires its ID and wire name to be reserved");
      }
    }
    // Compare every historical transition the previous reader supported, not only its latest layout.
    auto revisions = schema_version::transitions(previous);
    for (const auto& boundary : schema_version::transitions(current)) {
      const auto value = schema_version::parse(type, boundary);
      if (!(value < old_minimum) && !(old_revision < value)) {
        revisions.push_back(boundary);
      }
    }
    revisions.push_back(before_version->default_value);
    for (const auto& revision : revisions) {
      const auto value = schema_version::parse(type, revision);
      if (value < new_minimum || new_revision < value) {
        issue(result, path, "Historical writer revision is outside the new supported range", false,
              true);
        continue;
      }
      auto old_layout = historical_contract(previous, revision);
      auto new_layout = historical_contract(current, revision);
      std::vector<compatibility_issue> layout_issues{};
      check_class(layout_issues, old_layout, new_layout, protocol, policy);
      for (auto& problem : layout_issues) {
        problem.breaks_old_reader = false;
        problem.breaks_new_reader = true;
        problem.message = "Historical revision " + revision + ": " + problem.message;
        result.push_back(std::move(problem));
      }
    }
    if (new_revision < old_minimum || old_revision < new_revision) {
      issue(result, path, "New writer revision is outside the previous supported range", true,
            false);
    } else {
      auto old_layout = historical_contract(previous, after_version->default_value);
      auto new_layout = historical_contract(current, after_version->default_value);
      std::vector<compatibility_issue> layout_issues{};
      check_class(layout_issues, old_layout, new_layout, protocol, policy);
      for (auto& problem : layout_issues) {
        problem.breaks_old_reader = true;
        problem.breaks_new_reader = false;
        result.push_back(std::move(problem));
      }
    }
    return;
  }
  const auto old_fields = fields(previous, format);
  const auto new_fields = fields(current, format);
  const auto reservations = class_reservations(current, policy);
  const auto* reservation = &reservations;
  const bool positional = protocol == compatibility_protocol::binary_none;
  const bool protobuf = protocol == compatibility_protocol::protobuf_binary;
  if (positional) {
    const bool same_order = old_fields.size() == new_fields.size() &&
                            std::equal(old_fields.begin(), old_fields.end(), new_fields.begin(),
                                       [](const auto& before, const auto& after) {
                                         return before.id == after.id && before.name == after.name;
                                       });
    if (!same_order) {
      issue(result, path, "Positional fields added, removed, or reordered");
    }
  }
  for (const auto& before : old_fields) {
    const auto found = std::find_if(new_fields.begin(), new_fields.end(), [&](const auto& after) {
      // IDs are unique even where a union base name is also used by an ordinary named field.
      return before.id == after.id;
    });
    const auto field_path = path + "." + before.name;
    if (found == new_fields.end()) {
      if (!retired(reservation, before)) {
        issue(result, field_path,
              "Removed field or parent requires its ID and wire name to be reserved");
      }
      if (!protobuf && !positional) {
        issue(result, field_path, "New native reader rejects the removed field in older messages",
              false, true);
      }
      continue;
    }
    if (before.id != found->id || before.name != found->name) {
      issue(result, field_path, "Field identity changed or was reused (ID/wire-name pair)");
    }
    if (before.shape != found->shape) {
      issue(result, field_path, "Field type, container shape, map key, or parent identity changed");
    }
    if ((protocol == compatibility_protocol::binary_none ||
         protocol == compatibility_protocol::binary_integer ||
         protocol == compatibility_protocol::binary_string) && before.compact != found->compact) {
      issue(result, field_path, "Compact integer encoding changed");
    }
    if (before.fixed_extent != found->fixed_extent) {
      issue(result, field_path, "Fixed array cardinality changed", before.fixed_extent != 0,
            found->fixed_extent != 0);
    }
    if (!protobuf && before.default_value != found->default_value) {
      issue(result, field_path, "Schema default changed; absent native fields can change meaning");
    }
    check_ordinals(result, field_path, before.alternatives, found->alternatives, "Union");
  }
  if (!protobuf && !positional) {
    for (const auto& after : new_fields) {
      const bool exists = std::any_of(old_fields.begin(), old_fields.end(),
                                      [&](const auto& before) { return before.id == after.id; });
      if (!exists) {
        issue(result, path + "." + after.name, "Old native reader rejects the added field", true,
              false);
      }
    }
  }
}
} // namespace

// Keep CLI protocol selection explicit; text Protobuf needs a separately defined comparison policy.
compatibility_protocol parse_compatibility_protocol(std::string_view name) {
  if (name == "binary_none" || name == "binary_positional") {
    return compatibility_protocol::binary_none;
  }
  if (name == "binary_integer") {
    return compatibility_protocol::binary_integer;
  }
  if (name == "binary_string") {
    return compatibility_protocol::binary_string;
  }
  if (name == "json") {
    return compatibility_protocol::json;
  }
  if (name == "protobuf_binary" || name == "protobuf") {
    return compatibility_protocol::protobuf_binary;
  }
  throw std::invalid_argument{"Unsupported compatibility protocol: " + std::string{name}};
}

// Decode and validate the complete bounded policy before exposing any reservations to the caller.
compatibility_policy read_compatibility_policy(const std::filesystem::path& path) {
  std::ifstream input{path, std::ios::binary};
  if (!input) {
    throw std::runtime_error{"Cannot open compatibility policy: " + path.string()};
  }
  decode_limits limits{};
  constexpr std::size_t maximum_policy_bytes = 1024 * 1024;
  limits.max_input_bytes = maximum_policy_bytes;
  auto parsed = deserialize_exact<policy_input, json>(input, limits);
  compatibility_policy result{};
  for (auto& reservation : parsed.reservations) {
    result.reservations.push_back(std::move(reservation.value));
  }
  validate_policy(result);
  return result;
}

// Visit both independent resolved trees in deterministic name order without mutating either one.
std::vector<compatibility_issue> check_schema_compatibility(const parser::parsed_schema& previous,
                                                            const parser::parsed_schema& current,
                                                            compatibility_protocol protocol,
                                                            const compatibility_policy& policy) {
  validate_policy(policy);
  switch (protocol) {
  case compatibility_protocol::binary_none:
  case compatibility_protocol::binary_integer:
  case compatibility_protocol::binary_string:
  case compatibility_protocol::json:
    break;
  case compatibility_protocol::protobuf_binary:
    writer::validate_protobuf_schema(previous.statements);
    writer::validate_protobuf_schema(current.statements);
    break;
  default:
    throw std::invalid_argument{"Invalid compatibility protocol"};
  }
  declarations before{};
  declarations after{};
  collect(previous.statements, before);
  collect(current.statements, after);
  for (const auto& reservation : policy.reservations) {
    for (const auto* types : {&before, &after}) {
      const auto found = types->find(reservation.type);
      if (found != types->end() && found->second->type != object_type::class_type) {
        throw std::invalid_argument{"Field reservations require a class scope: " +
                                    reservation.type};
      }
    }
  }
  std::vector<compatibility_issue> result{};
  for (const auto& [name, node] : after) {
    if (node->type == object_type::class_type) {
      const auto reservations = class_reservations(static_cast<const class_node&>(*node), policy);
      check_reservations(result, name, fields(static_cast<const class_node&>(*node)),
                         &reservations);
    }
  }
  for (const auto& [name, node] : before) {
    const auto found = after.find(name);
    if (found == after.end()) {
      issue(result, name, "Type removed or renamed; review message identity and retired fields");
    } else if (node->type != found->second->type) {
      issue(result, name, "Declaration changed between enum and class");
    } else if (node->type == object_type::enum_type) {
      check_ordinals(result, name, static_cast<const enum_node&>(*node).enum_name_list,
                     static_cast<const enum_node&>(*found->second).enum_name_list, "Enum");
    } else {
      check_class(result, static_cast<const class_node&>(*node),
                  static_cast<const class_node&>(*found->second), protocol, policy);
    }
  }
  return result;
}
} // namespace rohit::serializer
