#include <rohit/schema_compatibility.hpp>

#include "protobuf_schema.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

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
    if (statement->type == object_type::namespace_type) {
      collect(static_cast<const namespace_node&>(*statement).statements, result);
    } else {
      result.emplace(statement->get_full_name(), statement.get());
    }
  }
}

// Compare resolved type identities, avoiding false differences from relative type spellings.
std::string type_identity(const type_name& type) {
  return type.resolved_node ? type.resolved_node->get_full_name() : type.name;
}

struct field_contract {
  std::uint32_t id{};
  std::string name{};
  std::string shape{};
  std::string default_value{};
  std::vector<std::pair<std::string, std::string>> alternatives{};
};

// Preserve parent-before-member order, field identity, map keys, and ordered union alternatives.
std::vector<field_contract> fields(const class_node& type) {
  std::vector<field_contract> result{};
  for (const auto& base : type.parents) {
    result.push_back(
        {base.id, base.display_name, "parent:" + base.parent_class->get_full_name(), {}, {}});
  }
  for (const auto& field : type.member_list) {
    field_contract contract{field.id, field.display_name, {}, field.default_value, {}};
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
  const auto old_fields = fields(previous);
  const auto new_fields = fields(current);
  const auto* reservation = reservation_for(policy, path);
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
  if (name == "binary_none") {
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
  if (name == "protobuf_binary") {
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
      check_reservations(result, name, fields(static_cast<const class_node&>(*node)),
                         reservation_for(policy, name));
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
