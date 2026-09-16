#include "protobuf_schema.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer::writer {
namespace {
// Require identifiers shared by Protobuf's text representations.
void identifier(std::string_view value) {
  const auto letter = [](char character) {
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
           character == '_';
  };
  if (value.empty() || !letter(value.front()) ||
      !std::all_of(value.begin(), value.end(), [&](char character) {
        return letter(character) || (character >= '0' && character <= '9');
      })) {
    throw std::invalid_argument{"Protobuf requires an identifier for wire name: " +
                                std::string{value}};
  }
}

// Validate the narrower Protobuf field-number space without changing existing IDs.
void field_number(std::uint32_t id) {
  constexpr std::uint32_t maximum_field_number = 536870911;
  constexpr std::uint32_t reserved_field_begin = 19000;
  constexpr std::uint32_t reserved_field_end = 19999;
  if (id == 0 || id > maximum_field_number ||
      (id >= reserved_field_begin && id <= reserved_field_end)) {
    throw std::invalid_argument{"Invalid Protobuf field number: " + std::to_string(id)};
  }
}

// Apply Protobuf's default JSON field-name conversion for collision detection.
std::string json_name(std::string_view value) {
  std::string result;
  bool uppercase = false;
  for (const auto character : value) {
    if (character == '_') {
      uppercase = true;
    } else {
      result += uppercase && character >= 'a' && character <= 'z'
                    ? static_cast<char>(character - 'a' + 'A')
                    : character;
      uppercase = false;
    }
  }
  return result;
}

// Validate one owning class, including synthetic union fields and map key restrictions.
void object(const class_node& value) {
  if (!value.has_mode(storage_mode::owning)) {
    throw std::invalid_argument{"Protobuf requires owning storage: " + value.get_full_name()};
  }
  std::set<std::string> names;
  const auto register_name = [&](std::string_view name) {
    identifier(name);
    if (!names.insert(json_name(name)).second) {
      throw std::invalid_argument{"Protobuf JSON field-name collision: " + std::string{name}};
    }
  };
  for (const auto& base : value.parents) {
    register_name(base.display_name);
    field_number(base.id);
  }
  for (const auto& item : value.member_list) {
    register_name(item.display_name);
    field_number(item.id);
    if (item.modifier == member::modifier_type::map &&
        (item.key_node || item.key == "float" || item.key == "double")) {
      throw std::invalid_argument{"Protobuf map keys must be integers, bool, or string: " +
                                  item.name};
    }
    if (item.modifier == member::modifier_type::variant) {
      std::set<std::string> alternatives;
      for (std::size_t index = 0; index < item.type_name_list.size(); ++index) {
        const auto& name = item.type_name_list[index].enum_name;
        identifier(name);
        field_number(static_cast<std::uint32_t>(index + 1));
        if (!alternatives.insert(json_name(name)).second) {
          throw std::invalid_argument{"Protobuf union JSON name collision: " + name};
        }
      }
    }
  }
}
} // namespace

// Validate only requested Protobuf output; existing formats retain their wider schema contract.
void validate_protobuf_schema(const std::vector<std::unique_ptr<syntax_node>>& statements) {
  for (const auto& node : statements) {
    if (node->type == object_type::namespace_type) {
      validate_protobuf_schema(static_cast<const namespace_node&>(*node).statements);
    } else if (node->type == object_type::class_type) {
      object(static_cast<const class_node&>(*node));
    } else if (node->type == object_type::enum_type) {
      const auto& names = static_cast<const enum_node&>(*node).enum_name_list;
      if (names.empty()) {
        throw std::invalid_argument{"Protobuf enums cannot be empty"};
      }
      for (const auto& name : names) {
        identifier(name);
      }
    }
  }
}
} // namespace rohit::serializer::writer
