//////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024  Rohit Jairaj Singh (rohit@singh.org.in)          //
//                                                                      //
// This program is free software: you can redistribute it and/or modify //
// it under the terms of the GNU General Public License as published by //
// the Free Software Foundation, either version 3 of the License, or    //
// (at your option) any later version.                                  //
//                                                                      //
// This program is distributed in the hope that it will be useful,      //
// but WITHOUT ANY WARRANTY; without even the implied warranty of       //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        //
// GNU General Public License for more details.                         //
//                                                                      //
// You should have received a copy of the GNU General Public License    //
// along with this program.  If not, see <https://www.gnu.org/licenses/ //
//////////////////////////////////////////////////////////////////////////

#include "cpp_managed_writer.hpp"
#include "cpp_naming.hpp"
#include "generated_notices.hpp"
#include "protobuf_schema.hpp"
#include "schema_version.hpp"
#include "behavior_writer.hpp"
#include "predicate_helpers.hpp"
#include "statement_behavior_writer.hpp"
#include "construction_writer.hpp"
#include "conversion_writer.hpp"
#include "translator_writer.hpp"
#include "comparison_writer.hpp"
#include "arithmetic_writer.hpp"
#include "auto_size_writer.hpp"
#include "array_exposure_writer.hpp"
#include "reference_writer.hpp"
#include "xml_schema.hpp"

#include <rohit/serializer_creator.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rohit::serializer::writer::cpp {
namespace {
// Escape one decoded magic byte for both C++ character literals and a single-line comment.
std::string escape_magic_byte(unsigned char byte) {
  switch (byte) {
  case '\0': return "\\000";
  case '\a': return "\\a";
  case '\b': return "\\b";
  case '\t': return "\\t";
  case '\n': return "\\n";
  case '\v': return "\\v";
  case '\f': return "\\f";
  case '\r': return "\\r";
  case '\'': return "\\'";
  case '"': return "\\\"";
  case '\\': return "\\\\";
  case '?': return "\\?";
  default: break;
  }
  constexpr unsigned char printable_ascii_begin = 0x20U;
  constexpr unsigned char printable_ascii_end = 0x7eU;
  if (byte >= printable_ascii_begin && byte <= printable_ascii_end) {
    return std::string(1U, static_cast<char>(byte));
  }
  // Fixed-width octal escapes preserve byte values without source-encoding or digit ambiguity.
  constexpr unsigned octal_digit_bits = 3U;
  constexpr unsigned octal_digit_mask = (1U << octal_digit_bits) - 1U;
  std::string result{"\\000"};
  for (std::size_t index = 1U; index < result.size(); ++index) {
    const auto shift = (result.size() - 1U - index) * octal_digit_bits;
    result[index] = static_cast<char>('0' + ((byte >> shift) & octal_digit_mask));
  }
  return result;
}

// Emit one schema using an isolated naming policy; the parsed schema is never rewritten.
class emitter : private naming {
  bool protobuf_enabled{false};
  bool constant_evaluation_enabled{false};
  bool positional_only{false};
  bool emission_only{false};
  bool managed_separate_values{false};
  bool rename_behavior_identifiers{true};
  std::map<std::string, class_comparison_options> class_comparisons;
  auto_size::configuration class_auto_sizes;
  xml_options xml_options_{};
  xml_binding::schema xml_schema_;
  std::string source_filter{};
  std::vector<std::string> included_headers{};
  friend class managed_writer<emitter>;
  managed_writer<emitter> managed_;
  std::map<std::string, std::string> projection_fields_{};
  std::string projected_target_type_{};
  // Keep compiler metadata names independent of presentation profiles.
  std::string member_name(const member& field) const {
    if (const auto found = projection_fields_.find(field.name); found != projection_fields_.end()) { return found->second; }
    return field.fixed_name ? field.name : field_name(field.name);
  }
public:
  // Restrict definitions to one source file while validating against the complete schema.
  bool owns_source(const syntax_node* node) const {
    return source_filter.empty() ||
        (node->output_source_path.empty() ? node->source_path : node->output_source_path) == source_filter;
  }

  // Bind a file's output context; relative includes refer only to generated bundle artifacts.
  void select_source(std::string source, std::vector<std::string> headers) {
    source_filter = std::move(source);
    included_headers = std::move(headers);
  }

  // Identify the owning schema before any authored preamble text reaches the header.
  void write_source_marker(rohit::type_check::output_buffer auto& output) const {
    if (!source_filter.empty()) {
      output.write("// Serializer source: ", source_filter, "\n\n");
    }
  }

  // Emit dependency headers before this source's declarations, including empty sources.
  void write_included_headers(rohit::type_check::output_buffer auto& output) const {
    for (const auto& header : included_headers) {
      output.write("#include \"", header, "\"\n");
    }
    if (!source_filter.empty()) { output.write("\n"); }
  }

  // Bind the per-output C++ naming policy.
  explicit emitter(const cpp_options& options)
      : naming{options}, protobuf_enabled{options.protobuf},
        constant_evaluation_enabled{options.constant_evaluation},
        positional_only{options.protocols == cpp_protocols::binary_none},
        emission_only{options.emission_only},
        managed_separate_values{options.managed_separate_values},
        rename_behavior_identifiers{options.rename_identifiers},
        class_comparisons{options.class_comparisons},
        class_auto_sizes{options.class_auto_sizes},
        xml_options_{options.xml}, xml_schema_{options.xml},
        managed_{*this, options.managed_id_type, options.managed_separate_values} {
    if (emission_only && (!constant_evaluation_enabled || !positional_only)) {
      throw std::invalid_argument{
          "cpp.emission_only true requires cpp.constant_evaluation true and cpp.protocols binary_none"};
    }
    if (positional_only && protobuf_enabled) {
      throw std::invalid_argument{"cpp.protocols binary_none cannot be combined with cpp.protobuf true"};
    }
    if (options.xml.enabled && emission_only) {
      throw std::invalid_argument{"XML generation requires owning models"};
    }
  }

  // Return the public enumerator spelling used in generated mode selections.
  std::string storage_mode_name(storage_mode mode) {
    switch (mode) {
    case storage_mode::owning:
      return "::rohit::serializer::storage_mode::owning";
    case storage_mode::read_only_view:
      return "::rohit::serializer::storage_mode::read_only_view";
    case storage_mode::mutable_view:
      return "::rohit::serializer::storage_mode::mutable_view";
    }
    throw std::invalid_argument{"Unknown generated storage mode"};
  }

  // Select a nested class's concrete declaration or enabled template specialization.
  std::string storage_type_name(const std::string& name, const syntax_node* node,
                                storage_mode mode = storage_mode::owning) {
    if (!node) {
      if (emission_only && name == "string") { return "::std::string_view"; }
      const auto& primitive = serializer::get_cpp_type(name);
      return primitive.starts_with("std::") ? "::" + primitive : primitive;
    }
    auto result = full_type_name(node);
    if (node->type == object_type::class_type &&
        static_cast<const class_node*>(node)->multiple_modes()) {
      result += "<" + storage_mode_name(mode) + ">";
    }
    return result;
  }

  // Preserve digest byte cardinality wherever a primitive type appears in generated storage.
  std::string storage_type_name(const ::rohit::serializer::type_name& value,
                                storage_mode mode = storage_mode::owning) {
    if (value.is_optional()) {
      return "::std::optional<" + storage_type_name(value.arguments.front(), mode) + ">";
    }
    if (value.is_sequence()) {
      return "::std::vector<" + storage_type_name(value.arguments.front(), mode) + ">";
    }
    if (value.is_associative()) {
      return "::std::map<" + storage_type_name(value.arguments[0], mode) + ", " +
             storage_type_name(value.arguments[1], mode) + ">";
    }
    if (value.type == object_type::unresolved && value.name == "digest") {
      return value.name; // Existing generic parameters retain their spelling and meaning.
    }
    if (value.is_digest()) {
      if (value.digest_extent != 0) {
        return "::std::array<::std::uint8_t, " + std::to_string(value.digest_extent) + ">";
      }
      return emission_only ? "::std::span<const ::std::uint8_t>" : "::std::vector<::std::uint8_t>";
    }
    return storage_type_name(value.name, value.resolved_node, mode);
  }

  // Retain dependent generated base applications in native template inheritance and traversal.
  std::string parent_type_name(const parent& base) {
    return !base.type_expression.empty() && base.type_expression.front().application ?
        generic_default(base.type_expression.front()) : storage_type_name(base.name, base.parent_class);
  }

  // Lower bound readonly calls identically in method bodies and construction input bindings.
  std::string readonly_call(const class_node* owner, const behavior_expression& node,
                            const std::vector<std::string>& arguments) {
    if (node.call_constructor) {
      auto owned_arguments = arguments;
      for (auto& argument : owned_arguments) { argument = "::std::move(" + argument + ")"; }
      return behavior::constructor_expression(node, owned_arguments, "cpp",
          [&](const auto& value) { return generic_default(value); },
          [&](auto name) { return function_name(name); });
    }
    const auto& target = behavior::called_function(node);
    if (node.call_target->type == object_type::function_type) {
      return full_type_name(node.call_target) + "(" + behavior::call_arguments(arguments) + ")";
    }
    std::string receiver = node.call_receiver ? "&(" + arguments.at(0) + ")" : "this";
    const auto* record = node.call_receiver ?
        static_cast<const class_node*>(node.operands.front().resolved_type->resolved_node) : owner;
    auto application = node.call_receiver ? *node.operands.front().resolved_type :
        rohit::serializer::type_name{std::string{}, nullptr};
    for (const auto index : node.call_base_path) {
      const auto& base = record->parents.at(index);
      auto parent_name = parent_type_name(base);
      if (node.call_receiver && !base.type_expression.empty()) {
        // Bind inherited receiver types to the caller's arguments rather than lexical parameters.
        const auto substitute = [&](const auto& self, rohit::serializer::type_name type) -> rohit::serializer::type_name {
          if (!type.application && type.arguments.empty()) {
            for (std::size_t parameter{}; parameter < record->generic_parameters.size() &&
                 parameter < application.arguments.size(); ++parameter) {
              if (type.name == record->generic_parameters[parameter].name) { return application.arguments[parameter]; }
            }
          }
          for (auto& argument : type.arguments) { argument = self(self, std::move(argument)); }
          return type;
        };
        application = substitute(substitute, base.type_expression.front());
        parent_name = generic_default(application);
      }
      receiver = "static_cast<const " + parent_name + "*>(" + receiver + ")";
      record = base.parent_class;
    }
    return "(" + receiver + ")->" + function_name(target.name) + "(" +
        behavior::call_arguments(arguments, node.call_receiver ? 1 : 0) + ")";
  }

  // Sequence reached construction calls/operands with the established checked statement lowerer.
  std::string construction_expression(const class_node* owner, const behavior_expression& expression,
      const std::function<std::string(std::string_view)>& symbol) {
    std::string output{"([&]() { "};
    behavior::statement_context context;
    context.cpp = true;
    context.prefix = "srl_construction_value_";
    context.valid = "srl_construction_ok";
    context.budget = "srl_construction_budget";
    context.symbol = symbol;
    context.native_type = [&](const auto& type) { return generic_default(type); };
    context.enum_value = [&](const auto& type, auto name) { return full_type_name(type.resolved_node) + "::" + enum_name(name); };
    context.member = [&](const behavior_expression& node, std::string base) {
      const auto* record = static_cast<const class_node*>(node.operands.front().resolved_type->resolved_node);
      for (const auto index : node.member_base_path) { record = record->parents.at(index).parent_class; }
      const auto field = std::find_if(record->member_list.begin(), record->member_list.end(),
          [&](const auto& item) { return item.name == node.value; });
      return "(" + base + ")." + member_name(*field);
    };
    context.call = [&](const auto& node, const auto& arguments) { return readonly_call(owner, node, arguments); };
    context.line = [&](const auto& text) { output += text + " "; };
    context.open = [&](const auto& text) { context.line(text + " {"); };
    context.close = [&]() { context.line("}"); };
    context.fail = [&]() { context.line("throw ::std::domain_error{\"Invalid portable expression\"};"); };
    context.line("bool " + context.valid + " = true;");
    context.line("(void)" + context.valid + ";");
    if (behavior::has_reduction(expression)) {
      context.line("::std::size_t " + context.budget + " = " +
          std::to_string(behavior_expression::maximum_reduction_visits) + ";");
    }
    behavior::statement_expression lower{context};
    const auto result = lower.emit(expression);
    context.line("return " + result + ";");
    return output + "}())";
  }

  // Preserve the parser's portable Boolean type contract on open native templates.
  std::string constraint_expression(const generic_constraint& constraint, std::string_view parameter) {
    using operation = generic_constraint::operation;
    const auto type = std::string{parameter};
    switch (constraint.kind) {
    case operation::integral: return "::std::integral<" + type + ">";
    case operation::floating_point: return "::std::floating_point<" + type + ">";
    case operation::stateless: return "::std::is_empty_v<" + type + ">";
    case operation::factory: return "requires { { " + type + "::" + function_name(constraint.factory_name) +
        "() } -> ::std::same_as<" + type + ">; }";
    case operation::exact_type: return "::std::same_as<" + type + ", " + generic_default(*constraint.type) + ">";
    case operation::logical_not: return "!(" + constraint_expression(constraint.operands.at(0), parameter) + ")";
    case operation::logical_and:
    case operation::logical_or:
      return "(" + constraint_expression(constraint.operands.at(0), parameter) +
          (constraint.kind == operation::logical_and ? " && " : " || ") +
          constraint_expression(constraint.operands.at(1), parameter) + ")";
    }
    throw std::invalid_argument{"Unknown generic constraint operation"};
  }

  // Emit one identical requires-clause for each declaration of a constrained template.
  std::string template_constraints(const class_node& object) {
    std::string result;
    for (const auto& parameter : object.generic_parameters) {
      if (!parameter.constraint) { continue; }
      if (!result.empty()) { result += " && "; }
      result += "(" + constraint_expression(*parameter.constraint, parameter.name) + ")";
    }
    return result.empty() ? result : "requires (" + result + ")\n";
  }

  // Compare selected schema values without including wire headers or hidden runtime metadata.
  void write_comparisons(rohit::type_check::output_buffer auto& output,
                         const class_node* object, access_type& current_access) {
    const auto* comparison_settings = comparison::find(*object, class_comparisons);
    if (!comparison_settings || (!comparison_settings->equality && !comparison_settings->ordering)) {
      return;
    }
    const auto selection = comparison::parts(*object, *comparison_settings);
    const auto values = [&](std::string_view receiver) {
      std::vector<std::string> result;
      for (const auto* base : selection.bases) {
        result.push_back("static_cast<const " + parent_type_name(*base) + "&>(" +
                         std::string{receiver} + ")");
      }
      for (const auto* field : selection.fields) {
        result.push_back(std::string{receiver} + "." + member_name(*field));
      }
      return result;
    };
    const auto tuple = [](const std::vector<std::string>& items) {
      std::string result{"::std::tie("};
      for (std::size_t index{}; index < items.size(); ++index) {
        if (index) { result += ", "; }
        result += items[index];
      }
      return result + ")";
    };
    const auto self = type_name(object->name);
    std::set<std::string> occupied;
    std::set<const class_node*> visited;
    // Inherited members and template parameters can also trigger native shadow diagnostics.
    const auto collect_names = [&](auto&& recurse, const class_node& owner) -> void {
      if (!visited.insert(&owner).second) { return; }
      occupied.insert(type_name(owner.name));
      for (const auto& parameter : owner.generic_parameters) { occupied.insert(parameter.name); }
      for (const auto& field : owner.member_list) { occupied.insert(member_name(field)); }
      for (const auto& method : owner.functions) { occupied.insert(function_name(method.name)); }
      for (const auto& base : owner.parents) {
        if (base.parent_class) { recurse(recurse, *base.parent_class); }
      }
    };
    collect_names(collect_names, *object);
    // Select a deterministic spelling without restricting previously valid schema names.
    const auto parameter_name = [&](std::string_view source) {
      const auto base = local_name(source);
      auto result = base;
      for (std::size_t suffix{}; occupied.contains(result);) {
        result = base + "_" + std::to_string(++suffix);
      }
      occupied.insert(result);
      return result;
    };
    const auto other = parameter_name("other");
    const auto left = parameter_name("left");
    const auto right = parameter_name("right");
    const auto lhs = tuple(values("(*this)"));
    const auto rhs = tuple(values(other));
    auto comparison_type = std::string{"ComparisonValue"};
    for (std::size_t suffix{}; occupied.contains(comparison_type);) {
      comparison_type = "ComparisonValue" + std::to_string(++suffix);
    }
    // Tuple operators omit noexcept on some libraries; prove their individual value operations.
    const auto nothrow_components = [&](bool ordering) {
      std::string condition;
      for (const auto& value : values("(*this)")) {
        if (!condition.empty()) { condition += " && "; }
        const auto first = "::std::declval<const " + comparison_type + "&>()";
        const auto second = first;
        condition += "[]<class " + comparison_type + ">() constexpr { ";
        if (ordering) {
          // Match tuple's synthesized ordering: native comparison categories, otherwise two less-tests.
          condition += "if constexpr (::std::three_way_comparable_with<" + comparison_type +
              ", " + comparison_type + ">) { return noexcept(" + first + " <=> " + second +
              "); } else if constexpr (requires { static_cast<bool>(" + first + " < " + second +
              "); }) { return noexcept(static_cast<bool>(" + first + " < " + second + ")); } ";
        } else {
          condition += "if constexpr (requires { static_cast<bool>(" + first + " == " + second +
              "); }) { return noexcept(static_cast<bool>(" + first + " == " + second + ")); } ";
        }
        condition += "else { return false; } }.template operator()<::std::remove_cvref_t<decltype(" +
            value + ")>>()";
      }
      return condition.empty() ? std::string{"true"} : condition;
    };
    const bool generic = object->type == object_type::generic_definition;
    output.write('\n', access_section(access_type::public_access, current_access));
    if (comparison_settings->equality) {
      output.write("  // Test memberwise equality using the configured schema field selection.\n"
                   "  [[nodiscard]] constexpr bool operator==(const ", self, "& ", other, ") const"
                   " noexcept(", nothrow_components(false), ")");
      const auto left_values = values(left);
      const auto right_values = values(right);
      if (generic && !left_values.empty()) {
        output.write("\n    requires requires(const ", self, "& ", left,
                     ", const ", self, "& ", right, ") {\n");
        for (std::size_t index{}; index < left_values.size(); ++index) {
          output.write("      { ", left_values[index], " == ", right_values[index],
                       " } -> ::std::convertible_to<bool>;\n");
        }
        output.write("    }");
      }
      output.write(" {\n");
      if (selection.bases.empty() && selection.fields.empty()) {
        output.write("    static_cast<void>(", other, ");\n");
      }
      output.write("    return ", lhs, " == ", rhs, ";\n  }\n");
    }
    if (comparison_settings->ordering) {
      output.write("  // Preserve the members' comparison category in lexicographic order.\n"
                   "  [[nodiscard]] constexpr auto operator<=>(const ", self, "& ", other, ") const"
                   " noexcept(", nothrow_components(true), ")");
      if (generic) {
        output.write("\n    requires requires(const ", self, "& ", left,
                     ", const ", self, "& ", right, ") {\n      ",
                     tuple(values(left)), " <=> ", tuple(values(right)), ";\n    }");
      }
      output.write(" {\n");
      if (selection.bases.empty() && selection.fields.empty()) {
        output.write("    static_cast<void>(", other, ");\n");
      }
      output.write("    return ", lhs, " <=> ", rhs, ";\n  }\n");
    }
  }

  // Native generic aliases share operators with their source template and cannot diverge.
  void bind_comparison_aliases(const std::vector<std::unique_ptr<syntax_node>>& statements) {
    for (const auto& statement : statements) {
      if (statement->type == object_type::namespace_type) {
        bind_comparison_aliases(static_cast<const namespace_node&>(*statement).statements);
      } else if (statement->type == object_type::class_type) {
        const auto& object = static_cast<const class_node&>(*statement);
        const auto configured = class_comparisons.find(object.get_full_name());
        if (object.generic_name.empty() || object.supports_managed() ||
            configured == class_comparisons.end()) { continue; }
        const auto [source, inserted] = class_comparisons.emplace(object.generic_name,
                                                                 configured->second);
        if (!inserted && (source->second.equality != configured->second.equality ||
                          source->second.ordering != configured->second.ordering ||
                          source->second.fields != configured->second.fields)) {
          throw std::invalid_argument{
              "C++ generic aliases require identical comparison configuration: " +
              object.get_full_name() + " and " + object.generic_name};
        }
      }
    }
  }

  // Share identical generated declarations while diagnosing incompatible imports of one type.
  void write_reusable_unit(auto& output, std::string_view identity, std::string_view source) {
    if (source.empty()) { return; }
    constexpr std::string_view hex_digits = "0123456789ABCDEF";
    std::string guard = "SERIALIZER_GENERATED_";
    for (const unsigned char byte : identity) {
      guard += hex_digits[byte >> 4];
      guard += hex_digits[byte & 15];
    }
    const auto signature = std::to_string(detail::field_name_hash(source)) + "ULL";
    output.write("#if defined(", guard, ")\n#if ", guard, " != ", signature,
        "\n#error \"Incompatible Serializer generated definitions for ", identity,
        "\"\n#endif\n#else\n#define ", guard, " ", signature, "\n", source,
        "#endif // ", guard, "\n\n");
  }

  // Detect a schema-native optional anywhere in a generated model's owned value graph.
  bool contains_optional(const class_node* object, std::set<const class_node*>& visited) const {
    if (!visited.insert(object).second) { return false; }
    const auto inspect = [&](auto&& recurse, const serializer::type_name& type) -> bool {
      if (type.is_optional()) { return true; }
      for (const auto& argument : type.arguments) {
        if (recurse(recurse, argument)) { return true; }
      }
      return type.resolved_node && type.resolved_node->type == object_type::class_type &&
             contains_optional(static_cast<const class_node*>(type.resolved_node), visited);
    };
    for (const auto& base : object->parents) {
      if (contains_optional(base.parent_class, visited)) { return true; }
    }
    for (const auto& parameter : object->generic_parameters) {
      for (const auto& argument : parameter.default_argument) {
        if (inspect(inspect, argument)) { return true; }
      }
    }
    for (const auto& field : object->member_list) {
      for (const auto& type : field.type_name_list) {
        if (inspect(inspect, type)) { return true; }
      }
    }
    return false;
  }

  // Scope canonical JSON optional projection only for schemas using the built-in optional type.
  void write_schema_value_scope(rohit::type_check::output_buffer auto& output,
                                const class_node* object) {
    std::set<const class_node*> visited;
    const bool optional = contains_optional(object, visited);
    if (optional || object->type == object_type::generic_definition) {
      output.write("\n    [[maybe_unused]] auto ",
                   variant_helper_name(*object, local_name("serializer_schema_values")),
                   " = ::rohit::serializer::detail::enter_schema_values");
      if (!optional) {
        output.write("<::rohit::serializer::detail::schema_optional_values<",
                     projected_target_type_.empty() ? type_name(object->name) : projected_target_type_, ">>");
      }
      output.write("(",
                   local_name("serializer_protocol"), ");");
    }
  }

  // Describe schema-selected generic applications without changing legacy host specializations.
  void write_optional_applications(rohit::type_check::output_buffer auto& output,
                                    const std::vector<std::unique_ptr<syntax_node>>& statements,
                                    std::set<std::string>& emitted) {
    for (const auto& statement : statements) {
      if (!owns_source(statement.get())) { continue; }
      if (statement->type == object_type::namespace_type) {
        write_optional_applications(output,
            static_cast<const namespace_node&>(*statement).statements, emitted);
      } else if (statement->type == object_type::class_type) {
        const auto& object = static_cast<const class_node&>(*statement);
        std::set<const class_node*> visited;
        if (object.generic_name.empty() || object.supports_managed() ||
            !contains_optional(&object, visited)) { continue; }
        const auto* definition = generic_definitions.at(object.generic_name);
        std::string application = full_type_name(definition) + "<";
        for (std::size_t index = 0; index < object.generic_arguments.size(); ++index) {
          if (index != 0) { application += ", "; }
          const auto& argument = object.generic_arguments[index];
          application += argument.kind == generic_argument_kind::dimension ?
              std::to_string(argument.dimension) + "ULL" : storage_type_name(argument);
        }
        application += ">";
        if (emitted.insert(application).second) {
          full_stream_auto_alloc unit_output{};
          unit_output.write("namespace rohit::serializer::detail {\n"
                       "// Retain canonical optional values for this concrete schema application.\n"
                       "template<> inline constexpr bool schema_optional_values<", application,
                       "> = true;\n}\n");
          write_reusable_unit(output, "optional:" + application,
              {reinterpret_cast<const char*>(unit_output.begin()), unit_output.current_offset()});
        }
      }
    }
  }

  // Select the symbolic discriminator type independently of its storage field.
  std::string union_enum_name(const member& field) {
    return type_name("e_" + field.name);
  }

  // Select the owning union's internal type using the type naming convention.
  std::string union_storage_name(const member& field) {
    return type_name("u_" + field.name);
  }

  // Select the discriminator data member using the field naming convention.
  std::string union_tag_name(const member& field) {
    return field_name(field.name + "_type");
  }

  // Derive an owning variant's discriminator from its sole active value.
  std::string union_tag_expression(const member& field) {
    return "this->" + (field.owning_variant ?
        function_name("get_" + field.name + "_type") + "()" : union_tag_name(field));
  }

  // Select by index so repeated alternative C++ types remain unambiguous.
  std::string union_payload_expression(const member& field, std::size_t index) {
    return field.owning_variant ? "::std::get<" + std::to_string(index) + ">(this->" +
        member_name(field) + ")" : "this->" + member_name(field) + "." +
        field_name(field.type_name_list[index].enum_name);
  }

  // Keep generated helper identifiers from shadowing user-selected class template parameters.
  std::string variant_helper_name(const class_node& object, const std::string& base,
                                  bool template_parameter = false) {
    auto name = base;
    std::size_t suffix{};
    while (name == type_name(object.name) ||
           std::any_of(object.generic_parameters.begin(), object.generic_parameters.end(),
                       [&](const auto& parameter) { return parameter.name == name; })) {
      name = base + (template_parameter ? "" : "_") + std::to_string(++suffix);
    }
    return name;
  }

  // Emit owning storage and typed operations without a second mutable discriminator.
  std::string get_cpp_type_support_variant(const member& field, const class_node& object) {
    const auto tag = union_enum_name(field);
    const auto storage = union_storage_name(field);
    const auto payload = member_name(field);
    const auto alternative_parameter = variant_helper_name(object, "Alternative", true);
    const auto arguments_parameter = variant_helper_name(object, "Args", true);
    const auto visitor_parameter = variant_helper_name(object, "Visitor", true);
    const auto arguments = variant_helper_name(object, local_name("args"));
    const auto visitor = variant_helper_name(object, local_name("visitor"));
    const auto index_name = variant_helper_name(object, local_name("index"));
    const auto value_name = variant_helper_name(object, local_name("value"));
    const auto wire_name = variant_helper_name(object, local_name("name"));
    std::string result{"  enum class " + tag + " {\n"};
    for (const auto& alternative : field.type_name_list) {
      result += "    " + enum_name(alternative.enum_name) + ",\n";
    }
    result += "  };\n  using " + storage + " = ::std::variant<";
    for (std::size_t index{}; index < field.type_name_list.size(); ++index) {
      if (index != 0U) { result += ", "; }
      result += generic_default(field.type_name_list[index]);
    }
    result += ">;\n"
        "  // Derive the wire alternative from the owned active value.\n"
        "  [[nodiscard]] constexpr " + tag + " " + function_name("get_" + field.name + "_type") +
        "() const {\n    if (this->" + payload + ".valueless_by_exception()) {\n"
        "      throw ::std::invalid_argument{\"Valueless variant payload\"};\n    }\n"
        "    return static_cast<" + tag + ">(this->" + payload + ".index());\n  }\n"
        "  // Construct the selected schema alternative with normal RAII ownership.\n"
        "  template <" + tag + " " + alternative_parameter + ", class... " + arguments_parameter + ">\n"
        "  constexpr decltype(auto) " + function_name("emplace_" + field.name) +
        "(" + arguments_parameter + "&&... " + arguments + ") {\n    constexpr auto " + index_name +
        " = static_cast<::std::size_t>(" + alternative_parameter + ");\n"
        "    static_assert(" + index_name + " < ::std::variant_size_v<" + storage +
        ">, \"Invalid variant alternative\");\n"
        "    return this->" + payload + ".template emplace<" + index_name + ">(::std::forward<" +
        arguments_parameter + ">(" + arguments + ")...);\n  }\n";
    for (const auto qualifier : {"", " const"}) {
      result += "  // Visit only the selected typed payload.\n"
          "  template <class " + visitor_parameter + ">\n  constexpr decltype(auto) " +
          function_name("visit_" + field.name) + "(" + visitor_parameter + "&& " + visitor + ")" + qualifier +
          " {\n    return ::std::visit(::std::forward<" + visitor_parameter + ">(" + visitor + "), this->" + payload +
          ");\n  }\n";
    }
    result += "  // Return the unchanged wire name of a generated alternative.\n"
        "  [[nodiscard]] static constexpr ::std::string to_string(" + tag + " " + value_name +
        ") {\n    switch (" + value_name + ") {\n";
    for (const auto& alternative : field.type_name_list) {
      result += "    case " + tag + "::" + enum_name(alternative.enum_name) +
          ": return \"" + alternative.enum_name + "\";\n";
    }
    result += "    default: throw ::std::invalid_argument{\"Invalid variant discriminator\"};\n"
        "    }\n  }\n"
        "  // Match complete schema names without relying on a hash alone.\n"
        "  [[nodiscard]] static constexpr " + tag + " " + function_name("to_e_" + field.name) +
        "(const auto& " + value_name + ") {\n    const ::std::string_view " +
        wire_name + "{" + value_name + "};\n";
    for (const auto& alternative : field.type_name_list) {
      result += "    if (" + wire_name + " == \"" + alternative.enum_name +
          "\") { return " + tag + "::" + enum_name(alternative.enum_name) + "; }\n";
    }
    result += "    throw ::std::invalid_argument{\"Unknown variant alternative\"};\n  }";
    return result;
  }

  // Emit union storage and conversion functions while keeping wire alternative names unchanged.
  std::string get_cpp_type_support_union(const member& field, const class_node& object) {
    if (field.owning_variant) { return get_cpp_type_support_variant(field, object); }
    const auto tag = union_enum_name(field);
    const auto storage = union_storage_name(field);
    std::string result{"  enum class " + tag + " {\n"};
    for (const auto& type : field.type_name_list) {
      result += "    " + enum_name(type.enum_name) + ",\n";
    }
    result += "  };\n  union " + storage + " {\n";
    for (const auto& type : field.type_name_list) {
      result += "    " + storage_type_name(type) + " " +
                field_name(type.enum_name) + ";\n";
    }
    if (emission_only) {
      return result + "  };\n  static_assert(::std::is_trivially_destructible_v<" + storage +
             ">, \"Raw union payloads must be trivially destructible\");";
    }
    result += "  };\n  static_assert(::std::is_trivially_destructible_v<" + storage +
              ">, \"Raw union payloads must be trivially destructible\");\n"
              "  // Return the wire name for a valid union alternative.\n"
              "  [[nodiscard]] static constexpr ::std::string to_string(" +
              tag +
              (std::string{" "} + local_name("value") + ") {\n    switch (" + local_name("value") +
               ") {\n");
    for (const auto& type : field.type_name_list) {
      result += "    case " + tag + "::" + enum_name(type.enum_name) + ":\n      return \"" +
                type.enum_name + "\";\n";
    }
    result +=
        "    default:\n      throw ::std::invalid_argument{\"Invalid union discriminator\"};\n"
        "    }\n  }\n"
        "  // Match the full wire name, including when names share a hash.\n"
        "  [[nodiscard]] static constexpr " +
        tag + " " + function_name("to_e_" + field.name) +
        (std::string{"(const auto& "} + local_name("value") + ") {\n    const ::std::string_view " +
         local_name("name") + "{" + local_name("value") + "};\n    const auto " +
         local_name("hash") + " = ::rohit::serializer::detail::field_name_hash(" +
         local_name("name") + ");\n");
    for (const auto& type : field.type_name_list) {
      result += (std::string{"    if ("} + local_name("hash") +
                 " == ::rohit::serializer::detail::field_name_hash(\"") +
                type.enum_name + "\") && " + local_name("name") + " == \"" + type.enum_name +
                "\") {\n      return " + tag + "::" + enum_name(type.enum_name) + ";\n    }\n";
    }
    result += "    throw ::std::invalid_argument{\"Unknown union alternative\"};\n  }";
    return result;
  }

  // Emit C++ support for the parsed schema.
  std::string get_cpp_type_support(const member& member, const class_node& object) {
    switch (member.modifier) {
    default:
    case member::modifier_type::none:
      return {};
    case member::modifier_type::array:
      return {};
    case member::modifier_type::map:
      return {};
    case member::modifier_type::variant:
      return get_cpp_type_support_union(member, object);
    }
  }

  // Return the mapped C++ type, preserving user-defined type names.
  std::string get_cpp_type(const member& member) {
    switch (member.modifier) {
    default:
    case member::modifier_type::none:
      // TODO: Range check
      return generic_default(member.type_name_list[0]);
    case member::modifier_type::array:
      if (emission_only) {
        return "::std::span<const " + generic_default(member.type_name_list[0]) + ">";
      }
      return std::string(!member.extent_expression.empty() ? "::std::array<" : "::std::vector<") +
             generic_default(member.type_name_list[0]) +
             (!member.extent_expression.empty() ? ", ::rohit::serializer::detail::fixed_extent(" +
                 (member.fixed_extent ? std::to_string(member.fixed_extent) :
                     dimension_default(member.extent_expression.front())) + ")" : "") +
             ">";
    case member::modifier_type::map:
      return std::string("::std::map<") +
             (member.key_type ? generic_default(*member.key_type) : storage_type_name(member.key, member.key_node)) + "," +
             generic_default(member.type_name_list[0]) +
             ">";
    case member::modifier_type::variant:
      if (member.owning_variant) { return union_storage_name(member); }
      return union_enum_name(member) + " " + union_tag_name(member) + "{};\n  " +
             union_storage_name(member);
    }
  }

  // Emit C++ access type for the parsed schema.
  void write_access_type(rohit::type_check::output_buffer auto& out_stream,
                         const access_type access) {
    switch (access) {
    default:
    case access_type::public_access:
      out_stream.write("public");
      break;

    case access_type::protected_access:
      out_stream.write("protected");
      break;

    case access_type::private_access:
      out_stream.write("private");
      break;
    }
  }

  // Share each class's effective visibility without moving any declarations between sections.
  std::string access_section(access_type requested, access_type& current_access) {
    if (requested == current_access) { return {}; }
    current_access = requested;
    return requested == access_type::private_access ? "private:\n" :
        requested == access_type::protected_access ? "protected:\n" : "public:\n";
  }

  // Emit C++ parent list for the parsed schema.
  void write_parent_list(rohit::type_check::output_buffer auto& out_stream,
                         const std::vector<parent>& parents) {
    bool first{true};
    for (const auto& parent : parents) {
      if (first) {
        first = false;
      } else {
        out_stream.write(", ");
      }
      write_access_type(out_stream, parent.access);
      out_stream.write(' ', parent_type_name(parent));
    }
  }

  // Preserve the exact value/borrowed representation of one authored callable input.
  std::string behavior_parameter_type(const function_parameter& parameter) {
    return construction::parameter_type(parameter, "cpp",
        [&](const auto& type) { return generic_default(type); });
  }

  // Render a fixed-array result without adding a serialized member or changing its value ownership.
  std::string behavior_return_type(const function_declaration& method) {
    if (method.return_type.name == "void") { return "void"; }
    function_parameter result;
    result.type = method.return_type;
    result.modifier = method.return_modifier;
    result.extent_expression = method.return_extent_expression;
    result.fixed_extent = method.return_fixed_extent;
    return behavior_parameter_type(result);
  }

  // Emit a readonly free function or one method on an owning/editor receiver.
  void write_behavior_method(rohit::type_check::output_buffer auto& output, const class_node* owner,
                             const function_declaration& method, bool editor_receiver = false,
                             bool declaration_only = false, access_type* current_access = nullptr) {
    if (method.dispatch == function_declaration::dispatch_type::abstract_method) { return; }
    if (method.guard && method.guard->available == false) { return; }
    const auto* body = behavior::native_body(method, "cpp");
    if (body && rename_behavior_identifiers) {
      throw std::invalid_argument{"cpp: opaque native bodies require naming = preserve"};
    }
    if (owner && !editor_receiver && owner->supports_managed() && method.effect == function_declaration::effect_type::edit) {
      return; // The corresponding generated editor owns this mutation contract.
    }
    if (!declaration_only) { output.write(behavior::source_marker(method, "cpp")); }
    if (owner) {
      if (current_access) { output.write(access_section(method.access, *current_access)); }
      else { write_access_type(output, method.access); output.write(":\n"); }
      output.write("  ");
    }
    if (!method.generic_parameters.empty()) {
      construction_declaration inputs;
      inputs.generic_parameters = method.generic_parameters;
      output.write(construction::input_template(inputs,
          [&](const auto& type) { return generic_default(type); },
          [&](auto name) { return function_name(name); }), "  ");
    }
    using dispatch = function_declaration::dispatch_type;
    const bool typed_body = method.expression && !body && !editor_receiver &&
        method.effect == function_declaration::effect_type::readonly;
    static const class_node free_owner{object_type::class_type, std::string{}, nullptr,
        class_attributes::none, {}};
    const bool inferred_constexpr = typed_body && method.dispatch == dispatch::ordinary &&
        behavior::constexpr_expression(*method.expression) &&
        behavior::constexpr_parameters(owner ? *owner : free_owner, method);
    const bool automatic_native_constexpr = body && method.automatic_constexpr_enabled &&
        !editor_receiver && method.dispatch == dispatch::ordinary &&
        behavior::constexpr_parameters(owner ? *owner : free_owner, method) &&
        (!owner || construction::constexpr_construction(*owner));
    const bool scalar_return = method.return_type.type == object_type::enum_type ||
        (method.return_type.type == object_type::primitive && method.return_type.name != "string" &&
         !method.return_type.is_digest() && !method.return_type.is_expected() &&
         method.return_modifier == member::modifier_type::none);
    const bool inferred_noexcept = typed_body && scalar_return &&
        ((method.predicate_body && !behavior::has_call(*method.expression) &&
          behavior::native_scalar_expression(*method.expression)) ||
         behavior::noexcept_expression(*method.expression));
    if (method.effect == function_declaration::effect_type::readonly && method.return_type.name != "void") {
      output.write("[[nodiscard]] ");
    }
    if (method.dispatch == dispatch::virtual_method || method.dispatch == dispatch::override_method) {
      output.write("virtual ");
    }
    if (method.is_static) { output.write("static "); }
    if (method.automatic_constexpr_enabled && (body || method.expression) &&
        !inferred_constexpr && !automatic_native_constexpr && !method.cpp_constexpr &&
        method.cpp_prefix.find("constexpr") == std::string::npos) {
      output.write("// Constant evaluation unavailable: ",
          behavior::constexpr_method_restriction(owner, method, editor_receiver), ".\n  ");
    }
    if (inferred_constexpr || automatic_native_constexpr ||
        (body && method.cpp_constexpr && !editor_receiver)) { output.write("constexpr "); }
    else if (!method.cpp_prefix.empty()) { output.write(method.cpp_prefix); }
    else if (!owner && (body || method.expression)) { output.write("inline "); }
    output.write(behavior_return_type(method), " ",
                 function_name(method.name), "(");
    for (std::size_t index = 0; index < method.parameters.size(); ++index) {
      const auto& argument = method.parameters[index];
      if (index) { output.write(", "); }
      if (method.expression) { output.write("[[maybe_unused]] "); }
      output.write(behavior_parameter_type(argument), " ", local_name(argument.name));
    }
    output.write(")");
    if (owner && !editor_receiver && !method.is_static && method.effect == function_declaration::effect_type::readonly) { output.write(" const"); }
    if (inferred_noexcept || method.cpp_noexcept) { output.write(" noexcept"); }
    if (method.dispatch == dispatch::override_method && behavior::concrete_virtual_base(*owner, method)) { output.write(" override"); }
    if (editor_receiver) { output.write(" requires (!Access::is_runtime_access)"); }
    if (method.guard && !method.guard->available.has_value()) {
      output.write(" requires (", callable_availability::cpp(*method.guard,
          [&](const auto& type) { return generic_default(type); }), ")");
    }
    if (declaration_only) { output.write(";\n"); return; }
    if (method.dispatch == dispatch::abstract_method) { output.write(" = 0;\n"); return; }
    if (body) {
      if (editor_receiver) {
        output.write(" { return access_.guard([&]() -> ",
            method.return_type.name == "void" ? "void" : generic_default(method.return_type),
            " {\n// clang-format off\n", behavior::line_directive(*body, "cpp"), body->text,
            behavior::reset_line("cpp"), "// clang-format on\n}); }\n");
      } else {
        output.write(" {\n// clang-format off\n", behavior::line_directive(*body, "cpp"), body->text,
            behavior::reset_line("cpp"), "// clang-format on\n}\n");
      }
      return;
    }
    if (!method.expression) { output.write(";\n"); return; }
    const auto symbol = [&](std::string_view name) {
      const auto argument = std::find_if(method.parameters.begin(), method.parameters.end(),
          [&](const auto& value) { return value.name == name; });
      if (argument != method.parameters.end()) { return local_name(argument->name); }
      if (!owner) { throw std::invalid_argument{"Unbound free function behavior symbol"}; }
      const auto field = std::find_if(owner->member_list.begin(), owner->member_list.end(),
          [&](const auto& value) { return value.name == name; });
      if (field == owner->member_list.end()) { throw std::invalid_argument{"Unbound native behavior symbol"}; }
      return std::string{"this->"} + member_name(*field);
    };
    output.write(" {\n");
    if (!method.predicate_body) {
      std::set<std::string> symbols;
      behavior::floating_symbols(*method.expression, symbols);
      for (const auto& name : symbols) {
        const auto value = "static_cast<double>(" + symbol(name) + ")";
        output.write("    if (!(::std::is_constant_evaluated() ? "
            "::rohit::serializer::detail::srl_behavior_checked_predicate_finite(", value,
            ") : ::std::isfinite(", value,
            "))) { throw ::std::domain_error{\"Portable expression requires finite inputs\"}; }\n");
      }
    }
    behavior::statement_context context;
    context.cpp = true; context.predicate = method.predicate_body; context.symbol = symbol;
    const auto collides = [&](std::string_view prefix) {
      return std::any_of(method.parameters.begin(), method.parameters.end(), [&](const auto& argument) {
        return local_name(argument.name).starts_with(prefix);
      });
    };
    while (collides(context.prefix) || collides(context.valid) || collides(context.budget)) {
      context.prefix += '_'; context.valid += '_'; context.budget += '_';
    }
    context.native_type = [&](const auto& type) { return generic_default(type); };
    context.enum_value = [&](const auto& type, auto name) { return full_type_name(type.resolved_node) + "::" + enum_name(name); };
    context.member = [&](const behavior_expression& node, std::string base) {
      const auto* record = static_cast<const class_node*>(node.operands.front().resolved_type->resolved_node);
      for (const auto index : node.member_base_path) { record = record->parents.at(index).parent_class; }
      const auto field = std::find_if(record->member_list.begin(), record->member_list.end(),
          [&](const auto& item) { return item.name == node.value; });
      return "(" + base + ")." + member_name(*field);
    };
    context.call = [&](const behavior_expression& node, const std::vector<std::string>& arguments) {
      return readonly_call(owner, node, arguments);
    };
    std::size_t indentation = 4;
    context.line = [&](const auto& text) { output.write(std::string(indentation, ' '), text, "\n"); };
    context.open = [&](const auto& text) { context.line(text + " {"); indentation += 2; };
    context.close = [&]() { indentation -= 2; context.line("}"); };
    context.fail = [&]() { context.line(method.predicate_body ? "return false;" :
        "throw ::std::domain_error{\"Invalid portable expression\"};"); };
    context.line("bool " + context.valid + " = true;");
    context.line("(void)" + context.valid + ";");
    if (behavior::has_reduction(*method.expression)) {
      context.line("::std::size_t " + context.budget + " = " +
          std::to_string(behavior_expression::maximum_reduction_visits) + ";");
    }
    behavior::statement_expression lower{context};
    const auto result = lower.emit(*method.expression);
    context.line("return " + result + ";");
    output.write("  }\n");
  }

  // Explicit qualification retains references to every required external definition, even when unused.
  void write_behavior_contract(rohit::type_check::output_buffer auto& output, const class_node* owner,
                               bool editor_receiver = false, access_type* current_access = nullptr) {
    std::vector<const function_declaration*> required;
    for (const auto& method : owner->functions) {
      if (method.guard && method.guard->available == false) { continue; }
      if (method.expression || behavior::native_body(method, "cpp") ||
          method.dispatch == function_declaration::dispatch_type::abstract_method ||
          !method.generic_parameters.empty()) { continue; }
      const bool edit = method.effect == function_declaration::effect_type::edit;
      if (editor_receiver != (owner->supports_managed() && edit)) { continue; }
      required.push_back(&method);
    }
    if (required.empty()) { return; }
    output.write(current_access ? access_section(access_type::public_access, *current_access) : "public:\n",
        "  ", ::std::string_view{editor_receiver ? "" : "static "},
        "void serializer_require_behavior_definitions()",
        ::std::string_view{editor_receiver ? " const requires (!Access::is_runtime_access)" : ""}, " {\n");
    if (editor_receiver) {
      output.write("    using SerializerReceiver = ::std::remove_cvref_t<decltype(*this)>;\n");
    } else {
      output.write("    using SerializerReceiver = ", type_name(owner->name), ";\n");
    }
    for (std::size_t index{}; index < required.size(); ++index) {
      const auto& method = *required[index];
      const bool guarded = method.guard && !method.guard->available.has_value();
      if (guarded) {
        output.write("    if constexpr (", callable_availability::cpp(*method.guard,
            [&](const auto& type) { return generic_default(type); }), ") {\n");
      }
      output.write("    using SerializerMethod", std::to_string(index), " = ",
          behavior_return_type(method),
          ::std::string_view{method.is_static ? " (*)(" : " (SerializerReceiver::*)("});
      for (std::size_t argument{}; argument < method.parameters.size(); ++argument) {
        if (argument) { output.write(", "); }
        output.write(behavior_parameter_type(method.parameters[argument]));
      }
      output.write(")", ::std::string_view{!editor_receiver && !method.is_static && method.effect == function_declaration::effect_type::readonly ? " const" : ""},
          ::std::string_view{method.cpp_noexcept ? " noexcept" : ""},
          ";\n    SerializerMethod", std::to_string(index), " volatile required", std::to_string(index),
          " = static_cast<SerializerMethod", std::to_string(index), ">(&SerializerReceiver::", function_name(method.name),
          ");\n    (void)required", std::to_string(index), ";\n");
      if (guarded) { output.write("    }\n"); }
    }
    output.write("  }\n");
  }

  // Preserve authored field, method and native declaration order without changing wire order.
  void write_ordered_body(rohit::type_check::output_buffer auto& output, const class_node* object,
                          access_type& current_access) {
    behavior::require_native_names(*object, "cpp", rename_behavior_identifiers);
    if (object->body_order.empty()) {
      for (const auto& field : object->member_list) { write_member(output, field, *object, current_access); }
      for (const auto& method : object->functions) {
        write_behavior_method(output, object, method, false, false, &current_access);
      }
      return;
    }
    for (const auto& item : object->body_order) {
      switch (item.kind) {
      case class_body_item::kind_type::field: {
        const auto& value = object->member_list.at(item.index);
        if (!value.wire_only) { output.write(std::string(item.blank_lines_before, '\n')); }
        write_member(output, value, *object, current_access);
        break;
      }
      case class_body_item::kind_type::function:
        output.write(std::string(item.blank_lines_before, '\n'));
        write_behavior_method(output, object, object->functions.at(item.index), false, false, &current_access);
        break;
      case class_body_item::kind_type::native_code: {
        const auto& code = object->native_blocks.at(item.index);
        if (code.language == "cpp") {
          output.write(std::string(item.blank_lines_before, '\n'), access_section(code.access, current_access),
              behavior::source_marker(code, "cpp"), "// clang-format off\n",
              behavior::line_directive(code, "cpp"), code.text, behavior::reset_line("cpp"), "// clang-format on\n");
          // Opaque hooks can change class access through labels or macros; restore it explicitly.
          current_access = access_type::error;
        }
        break;
      }
      case class_body_item::kind_type::magic: break; // Immutable metadata is emitted once above.
      }
    }
  }

  // Reuse native target output while getters borrow the source and only leaf defaults own storage.
  std::string translator_codecs(const conversion_declaration& method, std::string_view) {
    rohit::full_stream_auto_alloc output;
    const auto& target = ::rohit::serializer::conversion::record(method.target);
    auto projected = translator::output_fields(method, "cpp", [&](auto value, auto) { return field_name(value); });
    if (target.has_magic()) { output.write("  private:\n"); }
    if (target.magic_field) {
      output.write("    inline static constexpr ", get_cpp_type(*target.magic_field), " magic = ", magic_literal(*target.magic_field), ";\n");
    } else if (target.has_magic()) {
      output.write("    inline static constexpr char magic[] = {");
      for (std::size_t index{}; index < target.magic_bytes.size(); ++index) {
        if (index != 0) { output.write(", "); }
        output.write('\'', escape_magic_byte(static_cast<unsigned char>(target.magic_bytes[index])), '\'');
      }
      output.write("};\n");
    }
    if (target.has_magic()) { output.write("  public:\n"); }
    const auto saved_protobuf = protobuf_enabled;
    protobuf_enabled = false;
    projected_target_type_ = generic_default(method.target);
    auto bound_target = native_layout(target, std::string_view{});
    for (auto& field : bound_target.member_list) {
      for (auto& value : field.type_name_list) { value = ::rohit::serializer::conversion::bound_type(std::move(value), method.target); }
    }
    for (auto& base : bound_target.parents) {
      for (auto& value : base.type_expression) { value = ::rohit::serializer::conversion::bound_type(std::move(value), method.target); }
    }
    projection_fields_ = std::move(projected);
    write_serializer_out_body(output, &bound_target);
    projection_fields_.clear();
    projected_target_type_.clear();
    protobuf_enabled = saved_protobuf;
    return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
  }

  // Emit C++ member list for the parsed schema.
  // Translate typed owning defaults after selecting the target's open/concrete generic names.
  std::string default_value(const member& field) {
    if (field.initializer) {
      return construction::initializer(*field.initializer, "cpp",
          [&](const auto& value) { return generic_default(value); },
          [&](const auto& value) { return naming::default_value(value); },
          [&](auto value) { return function_name(value); });
    }
    return naming::default_value(field);
  }

  // Emit one field and its companion APIs while retaining the containing class's access state.
  void write_member(rohit::type_check::output_buffer auto& out_stream,
                    const member& member, const class_node& object, access_type& current_access) {
    if (member.wire_only) { return; }
    out_stream.write(access_section(member.access, current_access));
    auto support = get_cpp_type_support(member, object);
    if (!support.empty()) {
      out_stream.write(support, '\n');
    }
    if (member.empty_layout) {
      out_stream.write("  static_assert(std::is_empty_v<", get_cpp_type(member), ">, \"layout(empty) requires a stateless type\");\n"
                       "#if defined(_MSC_VER)\n  [[msvc::no_unique_address]]\n#else\n  [[no_unique_address]]\n#endif\n");
    }
    out_stream.write("  ", get_cpp_type(member), ' ', member_name(member), "{");
    if (!member.default_value.empty() || !member.default_factory.empty()) {
      // Initialize the backing array explicitly so generic elements may also be aggregates.
      const bool aggregate_elements = member.inferred_extent && !member.initializer;
      if (aggregate_elements) { out_stream.write("{"); }
      out_stream.write(default_value(member));
      if (aggregate_elements) { out_stream.write("}"); }
    }
    out_stream.write("};\n");
    if (!member.readonly_getter.empty()) {
      out_stream.write('\n');
      out_stream.write(access_section(member.readonly_getter_access, current_access),
          "  // Borrow this field from a live lvalue owner; its lifetime bounds the reference.\n",
          "  constexpr inline const ", get_cpp_type(member), "& ",
          function_name(member.readonly_getter), "() const & noexcept { return this->",
          member_name(member), "; }\n",
          "  // Reject temporary owners before a borrowed field reference can escape.\n",
          "  const ", get_cpp_type(member), "& ", function_name(member.readonly_getter),
          "() const && = delete;\n");
    }
    if (member.array_exposure) {
      const auto& element = member.modifier == member::modifier_type::array ?
          member.type_name_list.front() : member.type_name_list.front().arguments.front();
      out_stream.write(array_exposure::methods(member, get_cpp_type(member), generic_default(element),
          member_name(member), behavior::constexpr_type(object, element),
          [&](const auto& expression) { return dimension_default(expression); },
          [&](auto name) { return function_name(name); },
          [&](auto access) { return access_section(access, current_access); }));
    }
  }

  // Select pre-encoded wire-name storage when the protocol supports it, preserving custom codecs.
  std::string constant_output_name(std::string_view name) {
    return "::rohit::serializer::detail::constant_field_name<SerializeOutProtocol, \"" +
           std::string{name} + "\">()";
  }

  // Emit C++ serializer out body for parent for the parsed schema.
  void write_serializer_out_body_for_parent(rohit::type_check::output_buffer auto& out_stream,
                                            const class_node* obj,
                                            const rohit::serializer::serialize_key_type key_type,
                                            bool& first) {
    for (const auto& parent : obj->parents) {
      if (first) {
        first = false;
        out_stream.write((std::string{"\n      "} + local_name("serializer_protocol") +
                          ".struct_serialize_out_start("));
      } else {
        out_stream.write((std::string{"\n      "} + local_name("serializer_protocol") +
                          ".struct_serialize_out("));
      }
      if (key_type == rohit::serializer::serialize_key_type::string) {
        out_stream.write("::std::make_pair(", constant_output_name(parent.display_name),
                         ", static_cast<const ",
                         parent_type_name(parent),
                         "*>(this))"
                         ");");
      } else if (key_type == rohit::serializer::serialize_key_type::integer) {
        out_stream.write("::std::make_pair(static_cast<::std::uint32_t>(", parent.id,
                         "), static_cast<const ",
                         parent_type_name(parent),
                         "*>(this))"
                         ");");
      } else {
        out_stream.write("static_cast<const ", parent_type_name(parent),
                         "*>(this)"
                         ");");
      }
    }
  }

  // Emit C++ serializer out body non union for the parsed schema.
  void write_serializer_out_body_non_union(rohit::type_check::output_buffer auto& out_stream,
                                           const member& member,
                                           const rohit::serializer::serialize_key_type key_type,
                                           bool& first) {
    if (emission_only && member.modifier == member::modifier_type::array &&
        !member.extent_expression.empty()) {
      out_stream.write("\n      if (this->", member_name(member),
                       ".size() != ::rohit::serializer::detail::fixed_extent(",
                       member.fixed_extent ? std::to_string(member.fixed_extent) :
                           dimension_default(member.extent_expression.front()),
                       ")) { throw ::std::invalid_argument{\"Emission array extent mismatch\"}; }");
    }
    if (first) {
      first = false;
      out_stream.write((std::string{"\n      "} + local_name("serializer_protocol") +
                        ".struct_serialize_out_start("));
    } else {
      out_stream.write(
          (std::string{"\n      "} + local_name("serializer_protocol") + ".struct_serialize_out("));
    }
    if (!member.auto_size_formats.empty()) {
      if (key_type == serialize_key_type::string) {
        out_stream.write("::std::make_pair(", constant_output_name(member.display_name), ", ");
      } else if (key_type == serialize_key_type::integer) {
        out_stream.write("::std::make_pair(static_cast<::std::uint32_t>(", member.id, "), ");
      }
      out_stream.write(local_name("auto_size_value_" + std::to_string(member.id)),
                       std::string_view{key_type == serialize_key_type::none ? ");" : "));"});
      return;
    }
    if (member.compact != compact_encoding::none) {
      if (key_type == serialize_key_type::string) {
        out_stream.write("::std::make_pair(", constant_output_name(member.display_name), ", ");
      } else if (key_type == serialize_key_type::integer) {
        out_stream.write("::std::make_pair(static_cast<::std::uint32_t>(", member.id, "), ");
      }
      out_stream.write("::rohit::serializer::detail::compact_output<",
                       std::string_view{member.compact == compact_encoding::prefix ? "true" : "false"}, ", ",
                       std::string_view{member.compact_strict ? "true" : "false"}, ", ", get_cpp_type(member),
                       ">{this->", member_name(member), "}");
      out_stream.write(std::string_view{key_type == serialize_key_type::none ? ");" : "));"});
      return;
    }
    if (key_type == rohit::serializer::serialize_key_type::string) {
      if (member.modifier != member::modifier_type::none ||
          member.type_name_list[0].type != object_type::enum_type) {
        out_stream.write("::std::make_pair(", constant_output_name(member.display_name), ", ",
                         "::rohit::serializer::detail::schema_named_value{this->", member_name(member),
                         "})"
                         ");");
      } else {
        out_stream.write("::std::make_pair(", constant_output_name(member.display_name), ", ",
                         "::rohit::serializer::detail::enum_name(this->", member_name(member),
                         "))"
                         ");");
      }
    } else if (key_type == rohit::serializer::serialize_key_type::integer) {
      out_stream.write("::std::make_pair(static_cast<::std::uint32_t>(", member.id,
                       "), ::std::cref(this->", member_name(member),
                       "))"
                       ");");
    } else {
      out_stream.write("this->", member_name(member), ");");
    }
  }

  // Emit only the active union payload, preserving original wire names and numeric alternatives.
  void write_serializer_out_body_union(rohit::type_check::output_buffer auto& output,
                                       const member& field, serialize_key_type key_type,
                                       bool& first) {
    output.write("\n      switch (", union_tag_expression(field), ") {\n");
    for (std::size_t index = 0; index < field.type_name_list.size(); ++index) {
      const auto& alternative = field.type_name_list[index];
      const auto payload = union_payload_expression(field, index);
      output.write("      case ", union_enum_name(field), "::", enum_name(alternative.enum_name),
                   ":\n",
                   first ? (std::string{"        "} + local_name("serializer_protocol") +
                            ".struct_serialize_out_start(")
                         : (std::string{"        "} + local_name("serializer_protocol") +
                            ".struct_serialize_out("));
      if (key_type == serialize_key_type::string) {
        output.write("::std::make_pair(",
                     constant_output_name(field.display_name + ":" + alternative.enum_name),
                     ", ::std::cref(", payload, "))");
      } else if (key_type == serialize_key_type::integer) {
        output.write("::std::make_tuple(static_cast<::std::uint32_t>(", field.id,
                     "), static_cast<::std::uint32_t>(", index, "), ::std::cref(", payload, "))");
      } else {
        output.write("::std::make_pair(static_cast<::std::uint32_t>(", index, "), ::std::cref(",
                     payload, "))");
      }
      output.write(");\n        break;\n");
    }
    first = false;
    output.write(
        "      default:\n        throw ::std::invalid_argument{\"Invalid union discriminator\"};\n"
        "      }");
  }

  // Identify fixed-width schema scalars; collections, enums, unions, and objects end a run.
  bool is_fixed_field(const member& field) {
    return field.modifier == member::modifier_type::none &&
           field.auto_size_formats.empty() &&
           field.compact == compact_encoding::none &&
           field.type_name_list.front().type == object_type::primitive &&
           !field.type_name_list.front().is_digest() &&
           !field.type_name_list.front().is_optional() &&
           !field.type_name_list.front().is_sequence() &&
           !field.type_name_list.front().is_associative() &&
           field.type_name_list.front().name != "string" &&
           !field.type_name_list.front().name.starts_with("version");
  }

  // Bound each consecutive run to keep generated template packs and snapshots small.
  std::span<const member> fixed_field_run(std::span<const member> fields, std::size_t begin) {
    auto end = begin;
    while (end < fields.size() && end - begin < constants::maximum_fixed_batch_fields &&
           is_fixed_field(fields[end])) {
      ++end;
    }
    return fields.subspan(begin, end - begin);
  }

  // Capture scalar values and literal wire keys for one output batch, respecting the naming profile.
  std::string fixed_output_arguments(std::span<const member> fields, serialize_key_type keys) {
    std::string arguments;
    for (const auto& field : fields) {
      if (!arguments.empty()) {
        arguments += ", ";
      }
      const auto value = "this->" + member_name(field);
      if (keys == serialize_key_type::integer) {
        arguments += "::std::make_pair(static_cast<::std::uint32_t>(" + std::to_string(field.id) +
                     "), " + value + ")";
      } else if (keys == serialize_key_type::string) {
        arguments +=
            "::std::make_pair(" + constant_output_name(field.display_name) + ", " + value + ")";
      } else {
        arguments += value;
      }
    }
    return arguments;
  }

  // Use a protocol batch hook when available; preserve JSON and custom protocol framing otherwise.
  void write_fixed_output(rohit::type_check::output_buffer auto& output,
                          std::span<const member> fields, serialize_key_type keys, bool& first) {
    const auto call = local_name("serializer_protocol") + ".struct_serialize_out_fixed(" +
                      fixed_output_arguments(fields, keys) + ");";
    output.write("\n      if constexpr (requires { ", call, " }) {\n        ", call,
                 "\n      } else {");
    for (const auto& field : fields) {
      write_serializer_out_body_non_union(output, field, keys, first);
    }
    output.write("\n      }");
  }

  // Emit C++ serializer out body for the parsed schema.
  // Build a format-specific field list at generation time; no runtime omit flags are emitted.
  class_node native_layout(const class_node& object, std::string_view format) {
    class_node result{object.type, std::string{object.name}, object.parent_namespace,
                      object.attributes, std::vector<parent>{object.parents}};
    result.generic_parameters = object.generic_parameters;
    result.reference = object.reference;
    result.magic_bytes = object.magic_bytes;
    result.magic_field = object.magic_field;
    result.magic_access = object.magic_access;
    result.magic_id = object.magic_id;
    result.magic_omitted_formats = object.magic_omitted_formats;
    for (const auto& field : object.member_list) {
      if (!field.omits(format)) { result.member_list.push_back(field); }
    }
    return result;
  }

  // Keep legacy byte literals and typed scalar constants distinct at generation time.
  std::string magic_value(const class_node& object) {
    return object.magic_field ? "magic" : "::std::string_view{magic, sizeof(magic)}";
  }

  // Resolve the typed constant independently of output naming profiles.
  std::string magic_literal(const member& field) {
    const auto& type = field.type_name_list.front();
    if (type.type == object_type::enum_type) {
      return full_type_name(type.resolved_node) + "::" + enum_name(field.default_value);
    }
    return "static_cast<" + get_cpp_type(field) + ">(" + default_value(field) + ")";
  }

  // Emit only compile-time protocol comparisons for explicitly excluded formats.
  std::string omission_condition(const std::vector<std::string>& formats, std::string_view protocol) {
    std::string condition{};
    for (const auto& format : formats) {
      if (!condition.empty()) { condition += " || "; }
      condition += "::rohit::serializer::detail::protocol_wire_format<" + std::string{protocol} +
                   ">() == ::rohit::serializer::wire_format::" + format;
    }
    return condition.empty() ? "false" : condition;
  }

  // Reserve computed locals separately from authored storage; a counting sink writes fixed-width zeroes.
  void write_auto_size_output(auto& output, const class_node& owner) {
    for (const auto* field : auto_size::fields(owner)) {
      const auto local = local_name("auto_size_value_" + std::to_string(field->id));
      const auto selected = omission_condition(field->auto_size_formats, "SerializeOutProtocol");
      std::string excluded{"0"};
      if (field->size_scope == auto_size_scope::payload) {
        excluded.clear();
        for (const auto& format : field->auto_size_formats) {
          if (!excluded.empty()) { excluded += " + "; }
          excluded += "(::rohit::serializer::detail::protocol_wire_format<SerializeOutProtocol>() == "
                      "::rohit::serializer::wire_format::" + format + " ? " +
                      std::to_string(auto_size::field_bytes(*field, format)) + " : 0)";
        }
      }
      output.write("\n    [[maybe_unused]] const auto ", local_name("auto_size_count_guard_" + std::to_string(field->id)),
                   " = ::rohit::serializer::detail::make_auto_size_count_guard<", get_cpp_type(*field), ">(",
                   local_name("serializer_protocol"), ", ", selected, ", ", excluded, ");\n");
      output.write("\n    [[maybe_unused]] ", get_cpp_type(*field), " ", local,
                   " = ", field->wire_only ? "0" : "this->" + member_name(*field), ";\n    if constexpr (",
                   selected, ") {\n      ",
                   local, " = ", local_name("serializer_protocol"), ".template measure_record_size<",
                   get_cpp_type(*field), ">(*this");
      if (field->size_scope == auto_size_scope::payload) {
        output.write(", ", excluded);
      }
      output.write(");\n    }\n");
    }
  }

  // Validate fixed derived header widths after one complete count traversal, including nested records.
  void write_auto_size_output_end(auto& output, const class_node& owner) {
    for (const auto* field : auto_size::fields(owner)) {
      output.write("\n    ", local_name("auto_size_count_guard_" + std::to_string(field->id)), ".finish();\n");
    }
  }

  // Snapshot the bounded cursor before magic and track one required header without trusting its value.
  void write_auto_size_input_start(auto& output, const class_node& owner) {
    if (auto_size::fields(owner).empty()) { return; }
    output.write("\n    [[maybe_unused]] const auto ", local_name("auto_size_remaining"),
                 " = ::rohit::serializer::detail::auto_size_remaining(", local_name("serializer_protocol"),
                 ");\n    bool ", local_name("auto_size_seen"), " = false;\n"
                 "    [[maybe_unused]] ::std::uint32_t ", local_name("auto_size_id"), " = 0;\n"
                 "    [[maybe_unused]] ::std::string_view ", local_name("auto_size_name"), ";\n");
    output.write("    [[maybe_unused]] ::std::uint64_t ", local_name("auto_size_wire_value"), " = 0;\n"
                 "    [[maybe_unused]] ::std::uint64_t* ", local_name("auto_size_wire_destination"), " = nullptr;\n"
                 "    [[maybe_unused]] ::std::size_t ", local_name("auto_size_wire_width"), " = 0;\n"
                 "    [[maybe_unused]] ::std::size_t ", local_name("auto_size_field_bytes"), " = 0;\n");
    for (const auto* field : auto_size::fields(owner)) {
      output.write("    if constexpr (", omission_condition(field->auto_size_formats, "SerializeInProtocol"),
                   ") { ", local_name("auto_size_id"), " = ", field->id, "; ",
                   local_name("auto_size_name"), " = \"", field->display_name, "\";");
      if (field->wire_only) {
        output.write(" ", local_name("auto_size_wire_destination"), " = &", local_name("auto_size_wire_value"),
                     "; ", local_name("auto_size_wire_width"), " = sizeof(", get_cpp_type(*field), ");");
      }
      output.write(" }\n");
    }
  }

  // Verify native record spans after complete structural decoding; retain only skips the value comparison.
  void write_auto_size_input_end(auto& output, const class_node& owner) {
    for (const auto* field : auto_size::fields(owner)) {
      output.write("\n    if constexpr (", omission_condition(field->auto_size_formats, "SerializeInProtocol"),
                   ") {\n      if (!", local_name("auto_size_seen"),
                   ") { throw ::std::invalid_argument{\"Missing auto_size field\"}; }\n");
      if (auto_size::verifies(owner, class_auto_sizes)) {
        output.write("      const auto ", local_name("auto_size_actual"), " = ",
                     local_name("auto_size_remaining"), " - ::rohit::serializer::detail::auto_size_remaining(",
                     local_name("serializer_protocol"), ");\n      if (",
                     field->wire_only ? local_name("auto_size_wire_value") : "this->" + member_name(*field), " != ",
                     local_name("auto_size_actual"));
        if (field->size_scope == auto_size_scope::payload) {
          output.write(" - ", local_name("auto_size_field_bytes"));
        }
        output.write(
                     ") { throw ::std::invalid_argument{\"auto_size field does not match record bytes\"}; }\n");
      }
      output.write("    }\n");
    }
  }

  void write_serializer_out_layout(rohit::type_check::output_buffer auto& out_stream,
                                   const class_node* obj,
                                   const rohit::serializer::serialize_key_type key_type,
                                   const member* version = nullptr, bool json_magic = false) {
    bool first = true;
    if (json_magic) {
      out_stream.write("\n      ", local_name("serializer_protocol"),
                       ".struct_serialize_out_start(::std::make_pair(", constant_output_name("magic"),
                       ", ", magic_value(*obj), "));");
      first = false;
    }
    if (version) {
      write_serializer_out_body_non_union(out_stream, *version, key_type, first);
    }
    write_serializer_out_body_for_parent(out_stream, obj, key_type, first);
    for (std::size_t index = 0; index < obj->member_list.size();) {
      const auto run = fixed_field_run(obj->member_list, index);
      if (run.size() > 1) {
        write_fixed_output(out_stream, run, key_type, first);
        index += run.size();
        continue;
      }
      const auto& member = obj->member_list[index++];
      if (member.modifier != member::modifier_type::variant) {
        write_serializer_out_body_non_union(out_stream, member, key_type, first);
      } else {
        write_serializer_out_body_union(out_stream, member, key_type, first);
      }
    }
    out_stream.write(first ? (std::string{"\n      "} + local_name("serializer_protocol") +
                              ".struct_serialize_out_empty();")
                           : (std::string{"\n      "} + local_name("serializer_protocol") +
                              ".struct_serialize_out_end();"));
  }

  // Select one historical field list at generator time, retaining its declaration order.
  class_node version_layout(const class_node& object, std::string_view revision) {
    class_node result{object_type::class_type, std::string{object.name}, object.parent_namespace,
                      object.attributes, std::vector<parent>{object.parents}};
    result.magic_bytes = object.magic_bytes;
    result.magic_field = object.magic_field;
    const auto& version = *object.version_member();
    for (const auto& field : object.member_list) {
      if (!field.version && schema_version::active(field, version, revision)) {
        result.member_list.push_back(field);
      }
    }
    return result;
  }

  // Validate a discriminator once; policy checks add no code to unversioned schemas.
  void write_version_check(rohit::type_check::output_buffer auto& output, const member& version,
                           std::string_view value, bool input) {
    output.write("      if (!::rohit::serializer::detail::valid_version(", value, ") || ", value,
                 " < ", schema_version::cpp_literal(version, schema_version::minimum(version)),
                 " || ", schema_version::cpp_literal(version, version.default_value), " < ", value,
                 ") { throw ::std::invalid_argument{\"Unsupported schema version\"}; }\n");
    if (input) {
      output.write("      if constexpr "
                   "(::rohit::serializer::detail::protocol_read_policy<SerializeInProtocol>() == "
                   "::rohit::serializer::read_policy::strict) {\n        if (",
                   value, " != ", schema_version::cpp_literal(version, version.default_value),
                   ") { throw ::std::invalid_argument{\"Strict input requires current schema "
                   "version\"}; }\n      }\n");
    }
  }

  // Emit one direct output path per distinct supported layout, with the version before parents.
  void write_serializer_out_body(rohit::type_check::output_buffer auto& output,
                                 const class_node* original, serialize_key_type keys, bool json = false) {
    const auto filtered = native_layout(*original, json ? "json" : keys == serialize_key_type::none ?
                                        "binary_none" : keys == serialize_key_type::integer ?
                                        "binary_integer" : "binary_string");
    const auto* object = &filtered;
    const bool json_magic = json && original->has_magic() && !original->magic_omits("json");
    const auto* version = object->version_member();
    if (!version) {
      write_serializer_out_layout(output, object, keys, nullptr, json_magic);
      return;
    }
    const auto value = "this->" + member_name(*version);
    write_version_check(output, *version, value, false);
    const auto revisions = schema_version::transitions(*object);
    for (std::size_t index = 0; index < revisions.size(); ++index) {
      if (index + 1 < revisions.size()) {
        output.write(std::string_view{index == 0 ? "      if (" : "      else if ("}, value, " < ",
                     schema_version::cpp_literal(*version, revisions[index + 1]), ") {\n");
      } else {
        output.write(std::string_view{index == 0 ? "      {\n" : "      else {\n"});
      }
      const auto layout = version_layout(*object, revisions[index]);
      write_serializer_out_layout(output, &layout, keys, version, json_magic);
      output.write("\n      }\n");
    }
  }

  // Emit only positional output, reusing the same field traversal for each selected sink.
  void write_positional_serializer_out_body(rohit::type_check::output_buffer auto& output,
                                           const class_node* object, bool constant_output) {
    if (emission_only) {
      output.write("  // Encode borrowed fields through the binary-only counting or memory writer.\n"
                   "  constexpr void serialize_out(::rohit::serializer::binary_none_output& ",
                   local_name("serializer_protocol"), ") const {\n"
                   "    using SerializeOutProtocol [[maybe_unused]] = ::rohit::serializer::binary_none_output;\n");
    } else {
      output.write("  // Encode positional fields using the selected output sink.\n"
                 "  template <typename SerializeOutProtocol>\n"
                 "    requires(SerializeOutProtocol::key_type == ::rohit::serializer::serialize_key_type::none &&\n"
                 "             ::rohit::serializer::detail::protocol_wire_format<SerializeOutProtocol>() ==\n"
                 "                 ::rohit::serializer::wire_format::binary_none)\n  ",
                 "constexpr void ",
                 std::string_view{constant_output ? "serialize_constant_out" : "serialize_out"},
                 "(SerializeOutProtocol& ", local_name("serializer_protocol"), ") const {\n");
    }
    if (object->reference) { output.write("    validate_address();\n"); }
    write_auto_size_output(output, *object);
    if (object->has_magic()) {
      output.write("    if constexpr (!(",
                   omission_condition(object->magic_omitted_formats, "SerializeOutProtocol"),
                   ")) {\n      ",
                   std::string_view{object->magic_field ?
                       "::rohit::serializer::detail::write_typed_magic(" : ""},
                   local_name("serializer_protocol"),
                   std::string_view{object->magic_field ? ", magic);\n    }\n" :
                       ".serialize_magic(::std::string_view{magic, sizeof(magic)});\n    }\n"});
    }
    write_serializer_out_body(output, object, serialize_key_type::none);
    write_auto_size_output_end(output, *object);
    output.write("\n  }\n\n");
    if (!constant_output) {
      output.write("  // Encode to a buffer or byte sink satisfying the output stream concept.\n"
                       "  template <template <::rohit::serializer::serialize_type> class Protocol,\n"
                       "            ::rohit::type_check::output_stream SerializerStream>\n"
                       "  constexpr void serialize_out(SerializerStream& ",
                       local_name("stream"),
                       ") const {\n"
                       "    ::rohit::serializer::serialize_to<Protocol>(",
                       local_name("stream"), ", *this);\n  }\n\n");
      output.write(
          "  // Encode one bounded message using an optional standard compression backend.\n"
          "  template <template <::rohit::serializer::serialize_type> class Protocol,\n"
          "            ::rohit::type_check::output_stream SerializerStream>\n"
          "  void serialize_out(SerializerStream& ", local_name("stream"),
          ", const ::rohit::serializer::compression::encode_options& ", local_name("compression"),
          ", ::rohit::serializer::compression::encode_limits ", local_name("limits"), " = {}) const {\n"
          "    ::rohit::serializer::serialize_to<Protocol>(", local_name("stream"), ", *this, ",
          local_name("compression"), ", ", local_name("limits"), ");\n  }\n\n");
    }
  }

  // Emit direct field writes selected at compile time by the output protocol type.
  void write_serializer_out_body(rohit::type_check::output_buffer auto& out_stream,
                                 const class_node* obj) {
    if (positional_only) {
      write_positional_serializer_out_body(out_stream, obj, false);
      return;
    }
    out_stream.write(
        (std::string{
             "  // Encode fields directly using the protocol's compile-time key mode.\n  template "
             "<typename SerializeOutProtocol>\n  constexpr void serialize_out(SerializeOutProtocol& "} +
         local_name("serializer_protocol") +
         ") const {\n    static_assert(\n        SerializeOutProtocol::key_type == "
         "::rohit::serializer::serialize_key_type::none ||\n        SerializeOutProtocol::key_type "
         "== ::rohit::serializer::serialize_key_type::integer ||\n        "
         "SerializeOutProtocol::key_type == ::rohit::serializer::serialize_key_type::string,\n     "
         "  "
         " \"Unsupported serializer key type\");"));
    write_schema_value_scope(out_stream, obj);
    if (obj->reference) { out_stream.write("\n    validate_address();\n"); }
    write_auto_size_output(out_stream, *obj);
    if (obj->has_magic()) {
      out_stream.write("\n    if constexpr (!::rohit::serializer::detail::is_json_protocol<SerializeOutProtocol> &&\n"
                       "                  !requires { ", local_name("serializer_protocol"), ".protobuf_object(*this); } && !(",
                       omission_condition(obj->magic_omitted_formats, "SerializeOutProtocol"), ")) {\n      ",
                       std::string_view{obj->magic_field ? "::rohit::serializer::detail::write_typed_magic(" : ""},
                       local_name("serializer_protocol"),
                       std::string_view{obj->magic_field ? ", magic);\n    }\n" :
                         ".serialize_magic(::std::string_view{magic, sizeof(magic)});\n    }\n"});
    }
    if (protobuf_enabled) {
      out_stream.write("\n    if constexpr (requires { ", local_name("serializer_protocol"),
                       ".protobuf_object(*this); }) {\n      ", local_name("serializer_protocol"),
                       ".protobuf_object(*this);\n    } else ");
    }
    out_stream.write("\n    if constexpr (SerializeOutProtocol::key_type == "
                     "::rohit::serializer::serialize_key_type::none) {");
    write_serializer_out_body(out_stream, obj, rohit::serializer::serialize_key_type::none);
    out_stream.write("\n    } else if constexpr (SerializeOutProtocol::key_type == "
                     "::rohit::serializer::serialize_key_type::integer) {");
    write_serializer_out_body(out_stream, obj, rohit::serializer::serialize_key_type::integer);
    out_stream.write("\n    } else if constexpr (SerializeOutProtocol::key_type == "
                     "::rohit::serializer::serialize_key_type::string) {");
    out_stream.write("\n      if constexpr (::rohit::serializer::detail::is_json_protocol<SerializeOutProtocol>) {");
    write_serializer_out_body(out_stream, obj, rohit::serializer::serialize_key_type::string, true);
    out_stream.write("\n      } else {");
    write_serializer_out_body(out_stream, obj, rohit::serializer::serialize_key_type::string);
    out_stream.write("\n      }");
    out_stream.write("\n    }\n");
    write_auto_size_output_end(out_stream, *obj);
    out_stream.write("  }\n\n");
    out_stream.write("  // Encode to a buffer or byte sink satisfying the output stream concept.\n"
                     "  template <template <::rohit::serializer::serialize_type> class Protocol,\n"
                     "            ::rohit::type_check::output_stream SerializerStream>\n"
                     "  constexpr void serialize_out(SerializerStream& ",
                     local_name("stream"),
                     ") const {\n"
                     "    ::rohit::serializer::serialize_to<Protocol>(",
                     local_name("stream"), ", *this);\n  }\n\n");
    out_stream.write(
        "  // Encode one bounded message using an optional standard compression backend.\n"
        "  template <template <::rohit::serializer::serialize_type> class Protocol,\n"
        "            ::rohit::type_check::output_stream SerializerStream>\n"
        "  void serialize_out(SerializerStream& ", local_name("stream"),
        ", const ::rohit::serializer::compression::encode_options& ", local_name("compression"),
        ", ::rohit::serializer::compression::encode_limits ", local_name("limits"), " = {}) const {\n"
        "    ::rohit::serializer::serialize_to<Protocol>(", local_name("stream"), ", *this, ",
        local_name("compression"), ", ", local_name("limits"), ");\n  }\n\n");
  }

  // Pass a matching base donor without adding a protocol value charge for the parent.
  void write_parent_input(rohit::type_check::output_buffer auto& output, const parent& base,
                          std::string_view indent) {
    const auto base_type = parent_type_name(base);
    output.write(indent, "if constexpr (::std::is_same_v<StorageSource, ::std::nullptr_t>) {\n",
                 indent, "  static_cast<", base_type, "*>(this)->serialize_in(",
                 local_name("serializer_protocol"), ");\n", indent, "} else {\n", indent,
                 "  static_cast<", base_type, "*>(this)->serialize_in(",
                 local_name("serializer_protocol"), ", static_cast<", base_type, "*>(",
                 local_name("storage_donor"), "));\n", indent, "}\n");
  }

  // Donate a present field's storage; absent keyed fields retain the fresh candidate's defaults.
  void write_field_input(rohit::type_check::output_buffer auto& output, const member& field,
                         std::string_view indent, bool explicit_type = true,
                         bool named_optional = false) {
    if (!field.omitted_formats.empty()) {
      output.write(indent, "if constexpr (", omission_condition(field.omitted_formats, "SerializeInProtocol"),
                   ") { throw ::std::invalid_argument{\"Field omitted from selected format\"}; } else {\n");
    }
    const auto cpp_type = get_cpp_type(field);
    if (named_optional && field.compact == compact_encoding::none) {
      output.write(indent, "if constexpr (::rohit::serializer::detail::optional_enum<",
                   "::std::remove_cvref_t<decltype(this->", member_name(field), ")>>) {\n",
                   indent, "  ::rohit::serializer::detail::read_schema_named(",
                   local_name("serializer_protocol"), ", this->", member_name(field), ");\n",
                   indent, "} else {\n");
    }
    const bool has_storage = field.modifier == member::modifier_type::array ||
                             field.modifier == member::modifier_type::map ||
                             cpp_type == "::std::string" ||
                             field.type_name_list[0].type == object_type::class_type ||
                             field.type_name_list[0].type == object_type::unresolved;
    const auto read =
        field.compact != compact_encoding::none ?
        "::rohit::serializer::detail::read_compact<" +
            std::string{field.compact == compact_encoding::prefix ? "true" : "false"} + ">(" +
            local_name("serializer_protocol") + ", this->" + member_name(field) + ");\n" :
        local_name("serializer_protocol") +
        (explicit_type ? ".template serialize_in<" + cpp_type + ">" : ".serialize_in") + "(this->" +
        member_name(field) + ");\n";
    if (has_storage) {
      output.write(
          indent, "if constexpr (::std::is_same_v<StorageSource, ::std::nullptr_t>) {\n", indent,
          "  ", read, indent, "} else {\n", indent, "  ::rohit::serializer::detail::read_reusing(",
          local_name("serializer_protocol"), ", this->", member_name(field), ", ",
          local_name("storage_donor"), "->", member_name(field), ");\n", indent, "}\n");
    } else {
      output.write(indent, read);
    }
    if (named_optional && field.compact == compact_encoding::none) {
      output.write(indent, "}\n");
    }
    if (!field.omitted_formats.empty()) { output.write(indent, "}\n"); }
  }

  // Emit C++ serializer in body for parent key none for the parsed schema.
  void
  write_serializer_in_body_for_parent_key_none(rohit::type_check::output_buffer auto& out_stream,
                                               const class_node* obj) {
    for (const auto& parent : obj->parents) {
      write_parent_input(out_stream, parent, "      ");
    }
  }

  // Emit C++ serializer in body for parent key integer for the parsed schema.
  void
  write_serializer_in_body_for_parent_key_integer(rohit::type_check::output_buffer auto& out_stream,
                                                  const class_node* obj) {
    for (const auto& parent : obj->parents) {
      out_stream.write("      case ", parent.id, ":\n");
      write_parent_input(out_stream, parent, "        ");
      out_stream.write("        break;\n");
    }
  } // write_serializer_in_body_for_parent_key_integer

  // Emit C++ serializer in body non union key integer for the parsed schema.
  void
  write_serializer_in_body_non_union_key_integer(rohit::type_check::output_buffer auto& out_stream,
                                                 const member& member) {
    out_stream.write("      case ", member.id, ":\n");
    write_field_input(out_stream, member, "        ");
    out_stream.write("        break;\n");
  } // write_serializer_in_body_non_union_key_integer

  // Emit C++ serializer in body non union key none for the parsed schema.
  void
  write_serializer_in_body_non_union_key_none(rohit::type_check::output_buffer auto& out_stream,
                                              const member& member) {
    write_field_input(out_stream, member, "      ");
  } // write_serializer_in_body_non_union_key_none

  // Decode one active union value through its correctly renamed discriminator and storage member.
  void write_union_input(rohit::type_check::output_buffer auto& output, const member& field) {
    if (!field.omitted_formats.empty()) {
      output.write("      if constexpr (", omission_condition(field.omitted_formats, "SerializeInProtocol"),
                   ") { throw ::std::invalid_argument{\"Field omitted from selected format\"}; } else {\n");
    }
    if (field.owning_variant) {
      // Validate the index before replacing the old value; decoding the selected value
      // then follows the same partial-on-payload-failure contract as ordinary fields.
      output.write("      {\n        const auto ", local_name("union_index"), " = ",
                   local_name("serializer_protocol"), ".serialize_in_variable();\n        switch (",
                   local_name("union_index"), ") {\n");
      for (std::size_t index{}; index < field.type_name_list.size(); ++index) {
        output.write("        case ", index, ":\n          ", local_name("serializer_protocol"),
                     ".serialize_in(this->", member_name(field), ".template emplace<", index,
                     ">());\n          break;\n");
      }
      output.write("        default:\n          throw ::rohit::serializer::exception::bad_input_data{",
                   local_name("serializer_protocol"),
                   ".get_stream(), \"Invalid variant discriminator\"};\n        }\n      }\n");
      if (!field.omitted_formats.empty()) { output.write("      }\n"); }
      return;
    }
    output.write("      this->", union_tag_name(field), " = static_cast<", union_enum_name(field),
                 (std::string{">("} + local_name("serializer_protocol") +
                  ".serialize_in_variable());\n      switch (this->"),
                 union_tag_name(field), ") {\n");
    for (const auto& alternative : field.type_name_list) {
      const auto payload =
          "this->" + member_name(field) + "." + field_name(alternative.enum_name);
      output.write(
          "      case ", union_enum_name(field), "::", enum_name(alternative.enum_name),
          ":\n"
          "        ::std::construct_at(&",
          payload,
          (std::string{");\n        "} + local_name("serializer_protocol") + ".serialize_in("),
          payload,
          ");\n"
          "        break;\n");
    }
    output.write(
        (std::string{
             "      default:\n        throw ::rohit::serializer::exception::bad_input_data{"} +
         local_name("serializer_protocol") +
         ".get_stream(), \"Invalid union discriminator\"};\n      }\n"));
    if (!field.omitted_formats.empty()) { output.write("      }\n"); }
  }

  // Decode a union selected by its numeric field identifier.
  void write_serializer_in_body_union_key_integer(rohit::type_check::output_buffer auto& output,
                                                  const member& field) {
    output.write("      case ", field.id, ": {\n");
    write_union_input(output, field);
    output.write("        break;\n      }\n");
  }

  // Decode the next positional union without an unnecessary discriminator temporary.
  void write_serializer_in_body_union_key_none(rohit::type_check::output_buffer auto& output,
                                               const member& field) {
    write_union_input(output, field);
  }

  // Emit C++ serializer in body key none for the parsed schema.
  void write_serializer_in_layout(rohit::type_check::output_buffer auto& out_stream,
                                  const class_node* obj) {
    out_stream.write((std::string{"      [[maybe_unused]] auto "} + local_name("object_scope") +
                      " = ::rohit::serializer::detail::enter_decode_object(" +
                      local_name("serializer_protocol") + ");\n"));
    write_serializer_in_body_for_parent_key_none(out_stream, obj);
    for (std::size_t index = 0; index < obj->member_list.size();) {
      const auto run = fixed_field_run(obj->member_list, index);
      if (run.size() > 1) {
        // Positional input has no intervening keys; other protocol implementations keep scalar calls.
        const auto call = local_name("serializer_protocol") + ".serialize_in_fixed(" +
                          fixed_output_arguments(run, serialize_key_type::none) + ");";
        out_stream.write("      if constexpr (requires { ", call, " }) {\n        ", call,
                         "\n      } else {\n");
        for (const auto& field : run) {
          write_serializer_in_body_non_union_key_none(out_stream, field);
        }
        out_stream.write("      }\n");
        index += run.size();
        continue;
      }
      const auto& member = obj->member_list[index++];
      if (member.modifier != member::modifier_type::variant) {
        if (member.wire_only) {
          out_stream.write("      { ", get_cpp_type(member), " actual{}; ", local_name("serializer_protocol"),
                           ".serialize_in(actual); ", local_name("auto_size_wire_value"), " = actual; }\n");
        } else {
          write_serializer_in_body_non_union_key_none(out_stream, member);
        }
        if (member.auto_sizes("binary_none")) {
          out_stream.write("      ", local_name("auto_size_seen"), " = true;\n");
          if (member.size_scope == auto_size_scope::payload) {
            out_stream.write("      ", local_name("auto_size_field_bytes"), " = sizeof(", get_cpp_type(member), ");\n");
          }
        }
      } else if (member.type_name_list.size()) {
        write_serializer_in_body_union_key_none(out_stream, member);
      }
    }
  } // write_serializer_in_body_key_none

  // Decode the prefix once, then retain direct reads and fixed-width batching in each layout.
  void write_serializer_in_body_key_none(rohit::type_check::output_buffer auto& output,
                                         const class_node* original) {
    const auto filtered = native_layout(*original, "binary_none");
    const auto* object = &filtered;
    const auto* version = object->version_member();
    if (!version) {
      write_serializer_in_layout(output, object);
      return;
    }
    const auto value = "this->" + member_name(*version);
    output.write("      ", local_name("serializer_protocol"), ".serialize_in(", value, ");\n");
    write_version_check(output, *version, value, true);
    output.write("      if constexpr "
                 "(::rohit::serializer::detail::protocol_read_policy<SerializeInProtocol>() == "
                 "::rohit::serializer::read_policy::strict) {\n");
    const auto current = version_layout(*object, version->default_value);
    write_serializer_in_layout(output, &current);
    output.write("      } else {\n");
    const auto revisions = schema_version::transitions(*object);
    for (std::size_t index = 0; index < revisions.size(); ++index) {
      if (index + 1 < revisions.size()) {
        output.write(std::string_view{index == 0 ? "      if (" : "      else if ("}, value, " < ",
                     schema_version::cpp_literal(*version, revisions[index + 1]), ") {\n");
      } else {
        output.write(std::string_view{index == 0 ? "      {\n" : "      else {\n"});
      }
      const auto layout = version_layout(*object, revisions[index]);
      write_serializer_in_layout(output, &layout);
      output.write("\n      }\n");
    }
    output.write("      }\n");
  }

  // Track keyed historical fields on the stack; version placement in JSON is unrestricted.
  void write_version_reader(rohit::type_check::output_buffer auto& output,
                            const class_node& object) {
    const auto* version = object.version_member();
    if (!version) {
      return;
    }
    const auto type = type_name(object.name);
    output.write(
        "  // The current revision and oldest supported revision are schema constants.\n"
        "  static constexpr auto serializer_current_version = ",
        schema_version::cpp_literal(*version, version->default_value),
        ";\n"
        "  static constexpr auto serializer_minimum_version = ",
        schema_version::cpp_literal(*version, schema_version::minimum(*version)),
        ";\n"
        "  // Validate known historical fields after reading the discriminator in any position.\n"
        "  template <typename SerializeInProtocol, typename StorageSource>\n"
        "  struct serializer_version_reader {\n    ",
        type,
        "& target;\n"
        "    StorageSource donor;\n    ::std::array<bool, ",
        object.parents.size() + object.member_list.size(),
        "> seen{};\n"
        "    // Reject duplicate declarations before decoding their values.\n"
        "    void mark(::std::size_t index) {\n"
        "      if (seen[index]) { throw ::std::invalid_argument{\"Duplicate versioned field\"}; }\n"
        "      seen[index] = true;\n    }\n"
        "    // Match retained numeric identities without allocating a dispatch table.\n"
        "    constexpr void serialize_in_member_by_identifier(SerializeInProtocol& protocol, ::std::uint32_t "
        "key) {\n"
        "      switch (key) {\n");
    std::size_t index{};
    for (const auto& base : object.parents) {
      output.write("      case ", base.id, ": mark(", index++, "); break;\n");
    }
    for (const auto& field : object.member_list) {
      output.write("      case ", field.id, ": mark(", index++, "); break;\n");
    }
    output.write("      default: break;\n      }\n"
                 "      target.serialize_in_member_by_identifier(protocol, key, donor);\n    }\n"
                 "    // Match wire names, including union-expanded identities.\n"
                 "    constexpr void serialize_in_member_by_name(SerializeInProtocol& protocol, "
                 "::std::string_view key) {\n");
    index = 0;
    for (const auto& base : object.parents) {
      output.write("      if (key == \"", base.display_name, "\") { mark(", index++, "); }\n");
    }
    for (const auto& field : object.member_list) {
      if (field.modifier == member::modifier_type::variant) {
        for (const auto& alternative : field.type_name_list) {
          output.write("      if (key == \"", field.display_name, ":", alternative.enum_name,
                       "\") { mark(", index, "); }\n");
        }
      } else {
        output.write("      if (key == \"", field.display_name, "\") { mark(", index, "); }\n");
      }
      ++index;
    }
    output.write(
        "      target.serialize_in_member_by_name(protocol, key, donor);\n    }\n"
        "    // Require a unique version and reject fields outside that revision's lifecycle.\n"
        "    void finish() {\n");
    index = object.parents.size();
    for (const auto& field : object.member_list) {
      if (field.version) {
        output.write("      if (!seen[", index,
                     "]) { throw ::std::invalid_argument{\"Missing schema version\"}; }\n");
      }
      ++index;
    }
    const auto value = "target." + member_name(*version);
    write_version_check(output, *version, value, true);
    index = object.parents.size();
    for (const auto& field : object.member_list) {
      if (!field.created_version.empty() || !field.obsolete_version.empty()) {
        output.write("      if (seen[", index, "] && (");
        if (!field.created_version.empty()) {
          output.write(value, " < ", schema_version::cpp_literal(*version, field.created_version));
        }
        if (!field.created_version.empty() && !field.obsolete_version.empty()) {
          output.write(" || ");
        }
        if (!field.obsolete_version.empty()) {
          output.write("!(", value, " < ",
                       schema_version::cpp_literal(*version, field.obsolete_version), ")");
        }
        output.write(
            ")) { throw ::std::invalid_argument{\"Field outside schema version lifecycle\"}; }\n");
      }
      ++index;
    }
    output.write("    }\n  };\n\n");
  }

  // Retain the original field hook signature while forwarding to the typed donor implementation.
  void write_member_input_forwarder(rohit::type_check::output_buffer auto& output, bool by_name) {
    const std::string_view method =
        by_name ? "serialize_in_member_by_name" : "serialize_in_member_by_identifier";
    const std::string_view key_type = by_name ? "::std::string_view" : "::std::uint32_t";
    const auto key = local_name(by_name ? "name" : "identifier");
    output.write("  // Decode one selected field without an external storage donor.\n"
                 "  template <typename SerializeInProtocol>\n  constexpr void ",
                 method, "(SerializeInProtocol& ", local_name("serializer_protocol"), ", ",
                 key_type, " ", key, ") {\n    ", method, "(", local_name("serializer_protocol"),
                 ", ", key, ", nullptr);\n  }\n\n");
  }

  // Emit C++ serializer in body with key integer for the parsed schema.
  void write_serializer_in_body_with_key_integer(rohit::type_check::output_buffer auto& out_stream,
                                                 const class_node* obj) {
    write_member_input_forwarder(out_stream, false);
    if (obj->member_list.empty() && obj->parents.empty()) {
      // Empty managed payloads are valid; omit a default-only switch that warns under /W4.
      out_stream.write("  // Reject every numeric field in an empty schema.\n"
                       "  template <typename SerializeInProtocol, typename StorageSource>\n"
                       "  constexpr void serialize_in_member_by_identifier(SerializeInProtocol& ",
                       local_name("serializer_protocol"),
                       ", ::std::uint32_t, [[maybe_unused]] StorageSource ", local_name("storage_donor"),
                       ") {\n    throw ::rohit::serializer::exception::key_not_found{",
                       local_name("serializer_protocol"), ".get_stream(), \"Bad member identifier\"};\n  }\n\n");
      return;
    }
    out_stream.write(
        (std::string{"  // Decode the member selected by its numeric wire "
                     "identifier.\n  template <typename SerializeInProtocol, typename "
                     "StorageSource>\n"
                     "  constexpr void serialize_in_member_by_identifier(SerializeInProtocol& "} +
         local_name("serializer_protocol") +
         (std::string{", const ::std::uint32_t "} + local_name("identifier") +
          ", [[maybe_unused]] StorageSource " + local_name("storage_donor") + ") {\n    switch (" +
          local_name("identifier") + ") {\n")));

    write_serializer_in_body_for_parent_key_integer(out_stream, obj);

    for (const auto& member : obj->member_list) {
      if (member.wire_only) { continue; }
      if (member.modifier != member::modifier_type::variant) {
        write_serializer_in_body_non_union_key_integer(out_stream, member);
      } else if (member.type_name_list.size()) {
        write_serializer_in_body_union_key_integer(out_stream, member);
      }
    }

    out_stream.write(
        (std::string{
             "      default:\n        throw ::rohit::serializer::exception::key_not_found {"} +
         local_name("serializer_protocol") +
         ".get_stream(), \"Bad member identifier\"};\n    }\n  }\n\n"));
  }

  // Collect actual wire names so hash collisions share a case and always require full equality.
  void write_serializer_in_body_with_key_string(rohit::type_check::output_buffer auto& out_stream,
                                                const class_node* obj) {
    write_member_input_forwarder(out_stream, true);
    struct entry {
      std::string name;
      const parent* base{};
      const member* field{};
      const serializer::type_name* alternative{};
      bool magic{};
    };
    std::map<std::uint64_t, std::vector<entry>> groups;
    if (obj->parents.empty() && obj->member_list.empty() && !obj->has_magic()) {
      // Empty classes have no hash cases; avoid a default-only switch under MSVC /W4.
      out_stream.write(
          "  // Delegate unknown names to protocols that support compatible input.\n"
          "  template <typename SerializeInProtocol, typename StorageSource>\n"
          "  constexpr void serialize_in_member_by_name(SerializeInProtocol& ",
          local_name("serializer_protocol"), ", ::std::string_view ", local_name("name"),
          ", [[maybe_unused]] StorageSource ", local_name("storage_donor"),
          ") {\n"
          "    if constexpr (requires { ",
          local_name("serializer_protocol"), ".unknown_member(", local_name("name"),
          "); }) {\n      ", local_name("serializer_protocol"), ".unknown_member(",
          local_name("name"),
          ");\n    } else {\n      throw ::rohit::serializer::exception::key_not_found{",
          local_name("serializer_protocol"),
          ".get_stream(), \"Unknown field name\"};\n    }\n  }\n\n");
      return;
    }
    if (obj->has_magic()) {
      // Included JSON headers are consumed by the fixed-header adapter; payload dispatch rejects
      // this reserved name so flexible readers cannot silently accept an explicitly omitted header.
      groups[detail::field_name_hash("magic")].push_back({"magic", nullptr, nullptr, nullptr, true});
    }
    for (const auto& base : obj->parents) {
      groups[detail::field_name_hash(base.display_name)].push_back({base.display_name, &base});
    }
    for (const auto& field : obj->member_list) {
      if (field.wire_only) { continue; }
      if (field.modifier == member::modifier_type::variant) {
        for (const auto& alternative : field.type_name_list) {
          const auto name = field.display_name + ":" + alternative.enum_name;
          groups[detail::field_name_hash(name)].push_back({name, nullptr, &field, &alternative});
        }
      } else {
        groups[detail::field_name_hash(field.display_name)].push_back(
            {field.display_name, nullptr, &field});
      }
    }
    out_stream.write((std::string{"  // Match a wire name exactly within its hash group.\n"
                                  "  template <typename SerializeInProtocol, typename "
                                  "StorageSource>\n  void "
                                  "serialize_in_member_by_name(SerializeInProtocol& "} +
                      local_name("serializer_protocol") +
                      (std::string{", ::std::string_view "} + local_name("name") +
                       ", [[maybe_unused]] StorageSource " + local_name("storage_donor") +
                       ") {\n    switch (::rohit::serializer::detail::field_name_hash(" +
                       local_name("name") + ")) {\n")));
    for (const auto& [hash_value, entries] : groups) {
      static_cast<void>(hash_value);
      out_stream.write("      case ::rohit::serializer::detail::field_name_hash(\"",
                       entries.front().name, "\"):\n");
      for (const auto& item : entries) {
        out_stream.write((std::string{"        if ("} + local_name("name") + " == \""), item.name,
                         "\") {\n");
        if (item.magic) {
          out_stream.write("          throw ::std::invalid_argument{\"Magic header supplied as payload field\"};\n");
        } else if (item.base) {
          write_parent_input(out_stream, *item.base, "          ");
        } else if (item.alternative) {
          if (!item.field->omitted_formats.empty()) {
            out_stream.write("          if constexpr (", omission_condition(item.field->omitted_formats, "SerializeInProtocol"),
                             ") { throw ::std::invalid_argument{\"Field omitted from selected format\"}; } else {\n");
          }
          if (item.field->owning_variant) {
            const auto index = static_cast<std::size_t>(item.alternative - item.field->type_name_list.data());
            out_stream.write("          ", local_name("serializer_protocol"), ".serialize_in(this->",
                             member_name(*item.field), ".template emplace<", index, ">());\n");
          } else {
            out_stream.write(
              "          this->", union_tag_name(*item.field), " = ", union_enum_name(*item.field),
              "::", enum_name(item.alternative->enum_name),
              ";\n"
              "          ::std::construct_at(&this->",
              member_name(*item.field), ".", field_name(item.alternative->enum_name),
              (std::string{");\n          "} + local_name("serializer_protocol") +
               ".serialize_in(this->"),
              member_name(*item.field), ".", field_name(item.alternative->enum_name), ");\n");
          }
          if (!item.field->omitted_formats.empty()) { out_stream.write("          }\n"); }
        } else if (item.field->modifier == member::modifier_type::none &&
                   item.field->type_name_list[0].type == object_type::enum_type) {
          if (!item.field->omitted_formats.empty()) {
            out_stream.write("          if constexpr (", omission_condition(item.field->omitted_formats, "SerializeInProtocol"),
                             ") { throw ::std::invalid_argument{\"Field omitted from selected format\"}; } else {\n");
          }
          out_stream.write((std::string{"          ::rohit::serializer::detail::read_named_enum("} +
                            local_name("serializer_protocol") + ", this->"),
                           member_name(*item.field), ");\n");
          if (!item.field->omitted_formats.empty()) { out_stream.write("          }\n"); }
        } else {
          write_field_input(out_stream, *item.field, "          ", false,
                            item.field->modifier == member::modifier_type::none);
        }
        if (!item.magic) { out_stream.write("          return;\n"); }
        out_stream.write("        }\n");
      }
      out_stream.write("        break;\n");
    }
    out_stream.write("      default: break;\n    }\n    if constexpr (requires { ",
                     local_name("serializer_protocol"), ".unknown_member(", local_name("name"),
                     "); }) {\n      ", local_name("serializer_protocol"), ".unknown_member(",
                     local_name("name"),
                     ");\n    } else {\n      throw ::rohit::serializer::exception::key_not_found{",
                     local_name("serializer_protocol"),
                     ".get_stream(), \"Unknown field name\"};\n    }\n  }\n\n");
  }
  // Emit field reads selected at compile time, retaining keyed input dispatch where needed.
  // Wrap JSON handlers only for schemas whose fixed header is present in that format.
  void write_keyed_input(rohit::type_check::output_buffer auto& output, const class_node& object,
                         std::string_view reader) {
    std::string selected_reader{reader};
    if (!auto_size::fields(object).empty()) {
      selected_reader = local_name("auto_size_reader");
      output.write("auto ", selected_reader, " = ::rohit::serializer::detail::make_auto_size_reader(", reader,
                   ", ", local_name("auto_size_seen"), ", ", local_name("auto_size_id"), ", ",
                   local_name("auto_size_name"), ", ", local_name("auto_size_wire_destination"), ", ",
                   local_name("auto_size_wire_width"), ", &", local_name("auto_size_field_bytes"), ");\n      ");
    }
    if (object.has_magic() && !object.magic_omits("json")) {
      output.write("::rohit::serializer::detail::read_magic_object(", local_name("serializer_protocol"),
                   ", ", selected_reader, ", ", magic_value(object), ");");
    } else {
      output.write(local_name("serializer_protocol"), ".struct_serialize_in(&", selected_reader, ");");
    }
  }

  void write_serializer_in_body(rohit::type_check::output_buffer auto& out_stream,
                                const class_node* obj) {
    if (positional_only) {
      write_positional_serializer_in_body(out_stream, obj);
      return;
    }
    write_version_reader(out_stream, *obj);
    write_serializer_in_body_with_key_integer(out_stream, obj);
    write_serializer_in_body_with_key_string(out_stream, obj);
    out_stream.write("  // Decode this object using the protocol's compile-time key mode.\n"
                     "  template <typename SerializeInProtocol>\n"
                     "  ", std::string_view{obj->reference ? "" : "constexpr "},
                     "void serialize_in(SerializeInProtocol& ",
                     local_name("serializer_protocol"), ") {\n    serialize_in(",
                     local_name("serializer_protocol"), ", nullptr);\n  }\n\n");
    out_stream.write((
        std::string{
            "  // Decode fields; an internal donor supplies storage only for present fields.\n"
            "  // A supplied donor must be a distinct, non-null object and may be consumed.\n"
            "  template <typename SerializeInProtocol, typename StorageSource>\n  "} +
        std::string{obj->reference ? "" : "constexpr "} +
        "void serialize_in(SerializeInProtocol& " +
        local_name("serializer_protocol") + ", [[maybe_unused]] StorageSource " +
        local_name("storage_donor") +
        ") {\n    static_assert(\n        SerializeInProtocol::key_type == "
        "::rohit::serializer::serialize_key_type::none ||\n        SerializeInProtocol::key_type "
        "== "
        "::rohit::serializer::serialize_key_type::integer ||\n        "
        "SerializeInProtocol::key_type "
        "== ::rohit::serializer::serialize_key_type::string,\n        \"Unsupported serializer key "
        "type\");"));
    write_schema_value_scope(out_stream, obj);
    write_auto_size_input_start(out_stream, *obj);
    if (obj->reference) {
      out_stream.write("\n    // Detach before reading, but retain the previous owner until this decode finishes.\n"
          "    [[maybe_unused]] auto srl_reference_previous_binding = ::std::move(srl_reference_binding_);\n");
    }
    if (obj->has_magic()) {
      out_stream.write("\n    if constexpr (!::rohit::serializer::detail::is_json_protocol<SerializeInProtocol> &&\n"
                       "                  !requires { ", local_name("serializer_protocol"), ".protobuf_object(*this); } && !(",
                       omission_condition(obj->magic_omitted_formats, "SerializeInProtocol"), ")) {\n      ",
                       std::string_view{obj->magic_field ? "::rohit::serializer::detail::validate_typed_magic(" : ""},
                       local_name("serializer_protocol"),
                       std::string_view{obj->magic_field ? ", magic);\n    }\n" :
                         ".validate_magic(::std::string_view{magic, sizeof(magic)});\n    }\n"});
    }
    if (protobuf_enabled) {
      out_stream.write("\n    if constexpr (requires { ", local_name("serializer_protocol"),
                       ".protobuf_object(*this); }) {\n      ", local_name("serializer_protocol"),
                       ".protobuf_object(*this);\n    } else ");
    }
    out_stream.write("\n    if constexpr (SerializeInProtocol::key_type == "
                     "::rohit::serializer::serialize_key_type::none) {\n");
    write_serializer_in_body_key_none(out_stream, obj);
    out_stream.write("    } else if constexpr (SerializeInProtocol::key_type == "
                     "::rohit::serializer::serialize_key_type::integer ||\n            "
                     "SerializeInProtocol::key_type == "
                     "::rohit::serializer::serialize_key_type::string) {\n");
    if (obj->version_member()) {
      out_stream.write("      serializer_version_reader<SerializeInProtocol, StorageSource> ",
                       local_name("storage_reader"), "{*this, ", local_name("storage_donor"),
                       "};\n      ");
      write_keyed_input(out_stream, *obj, local_name("storage_reader"));
      out_stream.write("\n      ", local_name("storage_reader"), ".finish();\n");
    } else {
      out_stream.write("      if constexpr (::std::is_same_v<StorageSource, ::std::nullptr_t>) {\n        ");
      if ((obj->has_magic() && !obj->magic_omits("json")) || !auto_size::fields(*obj).empty()) {
        write_keyed_input(out_stream, *obj, "*this");
      } else {
        out_stream.write(local_name("serializer_protocol"), ".template struct_serialize_in<", type_name(obj->name), ">(this);");
      }
      out_stream.write("\n      } else {\n        ::rohit::serializer::detail::reused_object_reader<",
          type_name(obj->name), "> ", local_name("storage_reader"), "{*this, *",
          local_name("storage_donor"), "};\n        ");
      write_keyed_input(out_stream, *obj, local_name("storage_reader"));
      out_stream.write("\n      }");
    }
    out_stream.write("\n    }\n");
    write_auto_size_input_end(out_stream, *obj);
    if (obj->reference) { out_stream.write("    validate_address();\n"); }
    out_stream.write(
        "  }\n\n  // Decode from a buffer or bounded EOF-delimited byte source.\n"
        "  template <template <::rohit::serializer::serialize_type> class Protocol,\n"
        "            ::rohit::type_check::input_stream SerializerStream>\n"
        "  constexpr void serialize_in(SerializerStream&& ",
        local_name("stream"),
        ") {\n"
        "    ::rohit::serializer::serialize_from<Protocol>(",
        local_name("stream"), ", *this);\n  }\n\n"
        "  // Decode with explicit resource limits; byte sources receive exact-message validation.\n"
        "  template <template <::rohit::serializer::serialize_type> class Protocol,\n"
        "            ::rohit::type_check::input_stream SerializerStream>\n"
        "  constexpr void serialize_in(SerializerStream&& ",
        local_name("stream"), ", ::rohit::serializer::decode_limits ", local_name("limits"),
        ") {\n    ::rohit::serializer::serialize_from<Protocol>(",
        local_name("stream"), ", *this, ", local_name("limits"), ");\n  }\n");
    out_stream.write(
        "\n  // Validate one compressed frame and decode the complete uncompressed message.\n"
        "  template <template <::rohit::serializer::serialize_type> class Protocol,\n"
        "            ::rohit::type_check::input_stream SerializerStream>\n"
        "  void serialize_in(SerializerStream&& ", local_name("stream"),
        ", ::rohit::serializer::decode_limits ", local_name("limits"),
        ", const ::rohit::serializer::compression::decode_options& ", local_name("compression"),
        ") {\n    ::rohit::serializer::serialize_from<Protocol>(", local_name("stream"),
        ", *this, ", local_name("limits"), ", ", local_name("compression"), ");\n  }\n");
  }

  // Emit matching static entrypoints for owning values without changing the member APIs.
  void write_static_serializer(rohit::type_check::output_buffer auto& output,
                               const class_node* obj) {
    const auto name = type_name(obj->name);
    const auto stream = local_name("stream");
    const auto value = local_name("value");
    const auto limits = local_name("limits");
    output.write(
        "\n  // Encode a borrowed value; the caller owns the destination and any flush/close.\n"
        "  template <template <::rohit::serializer::serialize_type> class Protocol,\n"
        "            ::rohit::type_check::output_stream SerializerStream>\n"
        "  static constexpr void serialize(SerializerStream& ", stream, ", const ", name, "& ", value,
        ") {\n    ::rohit::serializer::serialize_to<Protocol>(", stream, ", ", value, ");\n  }\n");
    const auto compression = local_name("compression");
    output.write(
        "\n  // Encode one complete compressed message without taking ownership of the value.\n"
        "  template <template <::rohit::serializer::serialize_type> class Protocol,\n"
        "            ::rohit::type_check::output_stream SerializerStream>\n"
        "  static void serialize(SerializerStream& ", stream, ", const ", name, "& ", value,
        ", const ::rohit::serializer::compression::encode_options& ", compression,
        ", ::rohit::serializer::compression::encode_limits ", limits, " = {}) {\n"
        "    ::rohit::serializer::serialize_to<Protocol>(", stream, ", ", value, ", ", compression,
        ", ", limits, ");\n  }\n"
        "\n  // Return a fresh value after validating both the frame and the exact message.\n"
        "  template <template <::rohit::serializer::serialize_type> class Protocol,\n"
        "            ::rohit::type_check::input_stream SerializerStream>\n"
        "  [[nodiscard]] static ", name, " deserialize(SerializerStream&& ", stream,
        ", ::rohit::serializer::decode_limits ", limits,
        ", const ::rohit::serializer::compression::decode_options& ", compression,
        ") {\n    return ::rohit::serializer::deserialize_exact<", name, ", Protocol>(", stream,
        ", ", limits, ", ", compression, ");\n  }\n");
    for (const bool explicit_limits : {false, true}) {
      output.write(
          "\n  // Decode a new owning value; failures throw without returning a partial object.\n"
          "  // Input may be consumed; byte sources receive exact-message validation.\n"
          "  template <template <::rohit::serializer::serialize_type> class Protocol,\n"
          "            ::rohit::type_check::input_stream SerializerStream>\n"
          "  [[nodiscard]] static constexpr ", name, " deserialize(SerializerStream&& ", stream);
      if (explicit_limits) {
        output.write(", ::rohit::serializer::decode_limits ", limits);
      }
      output.write(") {\n    ", name, " ", value,
                   "{};\n    ::rohit::serializer::serialize_from<Protocol>(", stream, ", ", value);
      if (explicit_limits) {
        output.write(", ", limits);
      }
      output.write(");\n    return ", value, ";\n  }\n");
    }
  }

  // Retain positional decoding and donor reuse without generating keyed dispatch handlers.
  void write_positional_serializer_in_body(rohit::type_check::output_buffer auto& output,
                                          const class_node* object) {
    if (const auto* version = object->version_member()) {
      output.write(
          "  // The current revision and oldest supported revision are schema constants.\n"
          "  static constexpr auto serializer_current_version = ",
          schema_version::cpp_literal(*version, version->default_value),
          ";\n  static constexpr auto serializer_minimum_version = ",
          schema_version::cpp_literal(*version, schema_version::minimum(*version)), ";\n");
    }
    output.write("  // Decode positional fields with optional donor storage reuse.\n"
                 "  template <typename SerializeInProtocol, typename StorageSource = ::std::nullptr_t>\n"
                 "    requires(SerializeInProtocol::key_type == ::rohit::serializer::serialize_key_type::none &&\n"
                 "             ::rohit::serializer::detail::protocol_wire_format<SerializeInProtocol>() ==\n"
                 "                 ::rohit::serializer::wire_format::binary_none)\n"
                 "  ", std::string_view{object->reference ? "" : "constexpr "},
                 "void serialize_in(SerializeInProtocol& ", local_name("serializer_protocol"),
                 ", [[maybe_unused]] StorageSource ", local_name("storage_donor"), " = nullptr) {\n");
    write_auto_size_input_start(output, *object);
    if (object->reference) {
      output.write("    // Keep the old owner alive through the complete positional decode.\n"
          "    [[maybe_unused]] auto srl_reference_previous_binding = ::std::move(srl_reference_binding_);\n");
    }
    if (object->has_magic()) {
      output.write("    if constexpr (!(",
                   omission_condition(object->magic_omitted_formats, "SerializeInProtocol"),
                   ")) {\n      ",
                   std::string_view{object->magic_field ?
                       "::rohit::serializer::detail::validate_typed_magic(" : ""},
                   local_name("serializer_protocol"),
                   std::string_view{object->magic_field ? ", magic);\n    }\n" :
                       ".validate_magic(::std::string_view{magic, sizeof(magic)});\n    }\n"});
    }
    write_serializer_in_body_key_none(output, object);
    write_auto_size_input_end(output, *object);
    if (object->reference) { output.write("    validate_address();\n"); }
    output.write("  }\n\n");
    output.write(
        "\n  // Decode from a buffer or bounded EOF-delimited byte source.\n"
        "  template <template <::rohit::serializer::serialize_type> class Protocol,\n"
        "            ::rohit::type_check::input_stream SerializerStream>\n"
        "  constexpr void serialize_in(SerializerStream&& ",
        local_name("stream"),
        ") {\n"
        "    ::rohit::serializer::serialize_from<Protocol>(",
        local_name("stream"), ", *this);\n  }\n\n"
        "  // Decode with explicit resource limits; byte sources receive exact-message validation.\n"
        "  template <template <::rohit::serializer::serialize_type> class Protocol,\n"
        "            ::rohit::type_check::input_stream SerializerStream>\n"
        "  constexpr void serialize_in(SerializerStream&& ",
        local_name("stream"), ", ::rohit::serializer::decode_limits ", local_name("limits"),
        ") {\n    ::rohit::serializer::serialize_from<Protocol>(",
        local_name("stream"), ", *this, ", local_name("limits"), ");\n  }\n");
    output.write(
        "\n  // Validate one compressed frame and decode the complete uncompressed message.\n"
        "  template <template <::rohit::serializer::serialize_type> class Protocol,\n"
        "            ::rohit::type_check::input_stream SerializerStream>\n"
        "  void serialize_in(SerializerStream&& ", local_name("stream"),
        ", ::rohit::serializer::decode_limits ", local_name("limits"),
        ", const ::rohit::serializer::compression::decode_options& ", local_name("compression"),
        ") {\n    ::rohit::serializer::serialize_from<Protocol>(", local_name("stream"),
        ", *this, ", local_name("limits"), ", ", local_name("compression"), ");\n  }\n");
  }

  // Emit C++ serializer for the parsed schema.
  void write_serializer(rohit::type_check::output_buffer auto& out_stream, const class_node* obj) {
    if (emission_only) {
      write_positional_serializer_out_body(out_stream, obj, true);
      return;
    }
    write_serializer_out_body(out_stream, obj);
    if (constant_evaluation_enabled) {
      write_positional_serializer_out_body(out_stream, obj, true);
    }
    write_serializer_in_body(out_stream, obj);
    write_static_serializer(out_stream, obj);
    if (protobuf_enabled) { write_protobuf(out_stream, *obj); }
    if (xml_options_.enabled) { write_xml(out_stream, *obj); }
  }

  // Quote configured names as C++ byte strings without source-text injection.
  std::string xml_string(std::string_view value) {
    std::string result{"\""};
    for (const auto character : value) { result += escape_magic_byte(static_cast<unsigned char>(character)); }
    result += '"';
    return result;
  }

  // Validate XML member APIs against the complete class before durable-layout filtering removes callables.
  void validate_xml_names(const class_node& object) {
    const std::set<std::string> reserved_xml_names{"xml_root_name", "xml_runtime_xsd", "xml_validate_input",
        "xml_validate_output", "serialize_xml_out", "serialize_xml_in"};
    if (reserved_xml_names.contains(type_name(object.name))) {
      throw std::invalid_argument{"XML generated API conflicts with owning class name: " + object.name};
    }
    for (const auto& field : object.member_list) {
      if (reserved_xml_names.contains(member_name(field))) {
        throw std::invalid_argument{"XML generated member name conflicts with schema field: " + field.name};
      }
      if (!field.readonly_getter.empty() && reserved_xml_names.contains(function_name(field.readonly_getter))) {
        throw std::invalid_argument{"XML generated API conflicts with readonly getter: " + field.readonly_getter};
      }
    }
    for (const auto& method : object.functions) {
      if (reserved_xml_names.contains(function_name(method.name))) {
        throw std::invalid_argument{"XML generated member name conflicts with schema function: " + method.name};
      }
    }
    for (const auto& factory : object.constructions) {
      if (!factory.constructor && reserved_xml_names.contains(function_name(factory.name))) {
        throw std::invalid_argument{"XML generated API conflicts with named factory: " + factory.name};
      }
    }
    for (const auto& conversion : object.conversions) {
      if (reserved_xml_names.contains(function_name(conversion.name))) {
        throw std::invalid_argument{"XML generated API conflicts with conversion: " + conversion.name};
      }
    }
  }

  // Emit direct static field traversal with XSD-derived sequence, placement and occurrence checks.
  void write_xml(rohit::type_check::output_buffer auto& output, const class_node& object) {
    const auto contract = xml_schema_.bind(object);
    const auto qualified = [&](const xml::name& name) {
      return "::rohit::serializer::xml::name{" + xml_string(name.local) + ", " + xml_string(name.namespace_uri) + "}";
    };
    const auto node = local_name("xml_node");
    const auto reader = local_name("xml_reader");
    const auto budget = local_name("xml_budget");
    output.write("\n  // Return the fixed XML root contract; namespace prefixes are not identities.\n"
                 "  [[nodiscard]] static ::rohit::serializer::xml::name xml_root_name() { return ",
                 qualified(contract.root), "; }\n"
                 "  // Runtime validator support is explicitly selected during model generation.\n"
                 "  static constexpr bool xml_runtime_xsd = ",
                 std::string_view{xml_options_.runtime_xsd ? "true" : "false"}, ";\n"
                 "  static constexpr bool xml_validate_input = ",
                 std::string_view{xml_options_.validation_scope != "output" ? "true" : "false"}, ";\n"
                 "  static constexpr bool xml_validate_output = ",
                 std::string_view{xml_options_.validation_scope != "input" ? "true" : "false"}, ";\n"
                 "  // Encode this owning record using only statically selected XML bindings.\n"
                 "  void serialize_xml_out(::rohit::serializer::xml::element& ", node,
                 ", ::rohit::serializer::xml::build_budget& ", budget, ") const {\n");
    if (object.reference) { output.write("    validate_address();\n"); }
    for (const auto& field : contract.fields) {
      if (field.repeated) {
        const auto size = "this->" + member_name(*field.source) + ".size()";
        if (field.min_occurs > 0U || field.max_occurs) {
          output.write("    if (");
          if (field.min_occurs > 0U) { output.write(size, " < ", field.min_occurs, "U"); }
          if (field.min_occurs > 0U && field.max_occurs) { output.write(" || "); }
          if (field.max_occurs) { output.write(size, " > ", *field.max_occurs, "U"); }
          output.write(") { throw ::std::invalid_argument{\"XML collection violates XSD occurrence bounds\"}; }\n");
        }
      }
      if (field.text) {
        output.write("    ::rohit::serializer::xml::set_text(", node, ", this->", member_name(*field.source), ", ", budget, ");\n");
      } else if (field.wrapper) {
        output.write("    ::rohit::serializer::xml::add_wrapped_children(", node, ", ", qualified(*field.wrapper), ", ",
                     qualified(field.qualified_name), ", this->", member_name(*field.source), ", ", budget, ");\n");
      } else {
        output.write("    ::rohit::serializer::xml::", std::string_view{field.attribute ? "add_attribute" : "add_child"},
                     "(", node, ", ", qualified(field.qualified_name), ", this->", member_name(*field.source), ", ", budget, ");\n");
      }
    }
    output.write("  }\n"
                 "  // Decode into a candidate; use xml::deserialize for transactional public loading.\n"
                 "  void serialize_xml_in(const ::rohit::serializer::xml::element& ", node, ") {\n"
                 "    ::rohit::serializer::xml::field_reader ", reader, "{", node, ", ",
                 std::string_view{std::any_of(contract.fields.begin(), contract.fields.end(), [](const auto& field) { return field.text; }) ? "true" : "false"}, "};\n");
    if (object.reference) {
      output.write("    [[maybe_unused]] auto srl_reference_previous_binding = ::std::move(srl_reference_binding_);\n");
    }
    for (const auto& field : contract.fields) {
      if (field.text) {
        output.write("    ", reader, ".text_value(this->", member_name(*field.source), ");\n");
      } else if (field.wrapper) {
        output.write("    ", reader, ".wrapped_child_value(", qualified(*field.wrapper), ", ", qualified(field.qualified_name), ", this->", member_name(*field.source), ");\n");
      } else {
        output.write("    ", reader, std::string_view{field.attribute ? ".attribute_value(" : ".child_value("},
                     qualified(field.qualified_name), ", this->", member_name(*field.source), ");\n");
      }
      if (field.repeated && (field.min_occurs > 0U || field.max_occurs)) {
        const auto size = "this->" + member_name(*field.source) + ".size()";
        output.write("    if (");
        if (field.min_occurs > 0U) { output.write(size, " < ", field.min_occurs, "U"); }
        if (field.min_occurs > 0U && field.max_occurs) { output.write(" || "); }
        if (field.max_occurs) { output.write(size, " > ", *field.max_occurs, "U"); }
        output.write(") { throw ::std::invalid_argument{\"XML collection violates XSD occurrence bounds\"}; }\n");
      }
    }
    auto protocol_type = std::string{"SerializerXmlProtocol"};
    while (protocol_type == type_name(object.name)) { protocol_type += "Type"; }
    output.write("    ", reader, ".finish();\n");
    if (object.reference) { output.write("    validate_address();\n"); }
    output.write("  }\n"
                 "  // Compose this XML mapping with the common complete-object protocol API.\n"
                 "  template <typename ", protocol_type, ">\n"
                 "    requires requires(", protocol_type, "& protocol, const ", type_name(object.name),
                 "& value) { protocol.xml_object(value); }\n"
                 "  void serialize_out(", protocol_type, "& protocol) const { protocol.xml_object(*this); }\n"
                 "  // XML decoding uses a fresh candidate before replacing this object.\n"
                 "  template <typename ", protocol_type, ">\n"
                 "    requires requires(", protocol_type, "& protocol, ", type_name(object.name),
                 "& value) { protocol.xml_object(value); }\n"
                 "  void serialize_in(", protocol_type, "& protocol) { protocol.xml_object(*this); }\n");
  }

  // Convert a validated wire identifier to Protobuf's standard JSON spelling.
  std::string protobuf_json_name(std::string_view value) {
    std::string result;
    bool uppercase = false;
    for (const auto character : value) {
      if (character == '_') { uppercase = true; }
      else {
        result += uppercase && character >= 'a' && character <= 'z'
                    ? static_cast<char>(character - 'a' + 'A') : character;
        uppercase = false;
      }
    }
    return result;
  }

  // Build a protocol-template condition; omitted field branches disappear during compilation.
  std::string protobuf_field_condition(const std::vector<std::string>& omitted,
                                       std::string_view selector) {
    std::string condition;
    for (const auto* format : {"protobuf", "protojson", "textproto"}) {
      if (std::find(omitted.begin(), omitted.end(), format) == omitted.end()) { continue; }
      if (!condition.empty()) { condition += " && "; }
      condition += std::string(selector) + " != ::rohit::serializer::wire_format::" + format;
    }
    return condition;
  }

  // Emit direct typed Protobuf traversal; field IDs remain template arguments in all formats.
  void write_protobuf(rohit::type_check::output_buffer auto& output, const class_node& object) {
    if (object.version_member()) {
      throw std::invalid_argument{"Version lifecycle is not supported by Protobuf generation"};
    }
    const auto protocol = local_name("serializer_protocol");
    output.write("\n  // Reset included fields to Protobuf defaults; omitted fields retain schema defaults.\n"
                 "  template <::rohit::serializer::wire_format SelectedFormat = ::rohit::serializer::wire_format::protobuf>\n"
                 "  void serializer_protobuf_reset() {\n");
    if (object.reference) {
      output.write("    [[maybe_unused]] auto srl_reference_previous_binding = ::std::move(srl_reference_binding_);\n");
    }
    for (const auto& base : object.parents) {
      output.write("    static_cast<", parent_type_name(base),
                   "*>(this)->template serializer_protobuf_reset<SelectedFormat>();\n");
    }
    for (const auto& item : object.member_list) {
      if (item.wire_only) { continue; }
      const auto condition = protobuf_field_condition(item.omitted_formats, "SelectedFormat");
      if (!condition.empty()) {
        output.write("    if constexpr (", condition, ") {\n");
      }
      if (item.modifier == member::modifier_type::variant) {
        const auto& alternative = item.type_name_list.front();
        output.write("    this->", union_tag_name(item), " = ", union_enum_name(item), "::",
                     enum_name(alternative.enum_name), ";\n    ::std::construct_at(&this->",
                     member_name(item), ".", field_name(alternative.enum_name), ");\n");
        output.write("    ::rohit::serializer::detail::protobuf_reset<SelectedFormat>(this->", member_name(item),
                     ".", field_name(alternative.enum_name), ");\n");
      } else {
        output.write("    ::rohit::serializer::detail::protobuf_reset<SelectedFormat>(this->", member_name(item), ");\n");
      }
      if (!condition.empty()) {
        output.write("    }\n");
      }
    }
    output.write("  }\n\n  // Encode statically typed fields through the selected Protobuf protocol.\n"
                 "  template <typename ProtobufProtocol>\n  void serializer_protobuf_write([[maybe_unused]] ProtobufProtocol& ",
                 protocol, ") const {\n");
    if (object.reference) { output.write("    validate_address();\n"); }
    if (object.has_magic()) {
      const auto condition = protobuf_field_condition(object.magic_omitted_formats, "ProtobufProtocol::format");
      if (!condition.empty()) { output.write("    if constexpr (", condition, ") {\n"); }
      output.write("    ", protocol, ".template field<", object.magic_id,
                   ">(\"magic\", \"magic\", ", magic_value(object), ");\n");
      if (!condition.empty()) { output.write("    }\n"); }
    }
    for (const auto& base : object.parents) {
      output.write("    ", protocol, ".template field<", base.id, ">(\"", base.display_name,
                   "\", \"", protobuf_json_name(base.display_name), "\", *static_cast<const ",
                   parent_type_name(base), "*>(this));\n");
    }
    for (const auto& item : object.member_list) {
      if (item.wire_only) { continue; }
      const auto condition = protobuf_field_condition(item.omitted_formats, "ProtobufProtocol::format");
      if (!condition.empty()) {
        output.write("    if constexpr (", condition, ") {\n");
      }
      const auto names = "\"" + item.display_name + "\", \"" + protobuf_json_name(item.display_name) + "\"";
      if (item.modifier == member::modifier_type::variant) {
        output.write("    ", protocol, ".template message_field<", item.id, ">(", names,
                     ", [&](auto& nested) {\n      switch (this->", union_tag_name(item), ") {\n");
        for (std::size_t index = 0; index < item.type_name_list.size(); ++index) {
          const auto& alternative = item.type_name_list[index];
          output.write("      case ", union_enum_name(item), "::", enum_name(alternative.enum_name),
                       ": nested.template field<", index + 1, ">(\"", alternative.enum_name,
                       "\", \"", protobuf_json_name(alternative.enum_name), "\", this->",
                       member_name(item), ".", field_name(alternative.enum_name), "); break;\n");
        }
        output.write("      default: throw ::std::invalid_argument{\"Invalid union discriminator\"};\n"
                     "      }\n    });\n");
      } else {
        output.write("    ", protocol, ".template field<", item.id, ">(", names, ", this->",
                     member_name(item), ");\n");
      }
      if (!condition.empty()) {
        output.write("    }\n");
      }
    }
    output.write("  }\n\n  // Dispatch known Protobuf fields directly; the protocol handles unknown fields.\n"
                 "  template <typename ProtobufProtocol>\n  void serializer_protobuf_read(ProtobufProtocol& ",
                 protocol, ") {\n");
    if (object.reference) {
      output.write("    [[maybe_unused]] auto srl_reference_previous_binding = ::std::move(srl_reference_binding_);\n");
    }
    for (const auto& base : object.parents) {
      output.write("    bool protobuf_seen_", base.id, "{};\n");
    }
    for (const auto& item : object.member_list) {
      if (item.wire_only) { continue; }
      output.write("    [[maybe_unused]] bool protobuf_seen_", item.id, "{};\n");
    }
    if (object.has_magic()) {
      output.write("    [[maybe_unused]] bool protobuf_seen_magic{};\n");
    }
    output.write("    while (", protocol, ".next_field()) {\n");
    if (object.has_magic()) {
      const auto condition = protobuf_field_condition(object.magic_omitted_formats, "ProtobufProtocol::format");
      if (!condition.empty()) { output.write("      if constexpr (", condition, ") {\n"); }
      output.write("      if (", protocol, ".template match<", object.magic_id,
                   ">(\"magic\", \"magic\")) {\n");
      if (object.magic_field) {
        // Typed identities require one occurrence; retain legacy Protobuf scalar occurrence rules.
        output.write("        if (protobuf_seen_magic) { throw ::std::invalid_argument{\"Duplicate magic header\"}; }\n");
      }
      output.write("        ", protocol,
                   ".template occurrence<false>(protobuf_seen_magic);\n"
                   "        ", object.magic_field ? get_cpp_type(*object.magic_field) : "::std::string",
                   " actual_magic{};\n        if (", protocol,
                   ".null_value()) { throw ::std::invalid_argument{\"Null magic header\"}; }\n        ", protocol,
                   ".field(actual_magic);\n"
                   "        if (actual_magic != ", magic_value(object), ") {\n"
                   "          throw ::std::invalid_argument{\"Magic mismatch\"};\n"
                   "        }\n        continue;\n      }\n");
      if (!condition.empty()) { output.write("      }\n"); }
    }
    for (const auto& base : object.parents) {
      output.write("      if (", protocol, ".template match<", base.id, ">(\"", base.display_name,
                   "\", \"", protobuf_json_name(base.display_name), "\")) {\n        ", protocol,
                   ".template occurrence<false>(protobuf_seen_", base.id, ");\n        ", protocol,
                   ".field(*static_cast<", parent_type_name(base),
                   "*>(this));\n        continue;\n      }\n");
    }
    for (const auto& item : object.member_list) {
      if (item.wire_only) { continue; }
      const auto condition = protobuf_field_condition(item.omitted_formats, "ProtobufProtocol::format");
      if (!condition.empty()) {
        output.write("    if constexpr (", condition, ") {\n");
      }
      output.write("      if (", protocol, ".template match<", item.id, ">(\"", item.display_name,
                   "\", \"", protobuf_json_name(item.display_name), "\")) {\n");
      output.write("        ", protocol, ".template occurrence<",
                   std::string_view{item.modifier == member::modifier_type::array ||
                   item.modifier == member::modifier_type::map ? "true" : "false"},
                   ">(protobuf_seen_", item.id, ");\n");
      if (item.modifier == member::modifier_type::variant) {
        const auto& first_alternative = item.type_name_list.front();
        output.write("        const auto protobuf_reset_union = [&] {\n          this->",
                     union_tag_name(item), " = ", union_enum_name(item), "::",
                     enum_name(first_alternative.enum_name), ";\n          ::std::construct_at(&this->",
                     member_name(item), ".", field_name(first_alternative.enum_name),
                     ");\n          ::rohit::serializer::detail::protobuf_reset<ProtobufProtocol::format>(this->",
                     member_name(item), ".", field_name(first_alternative.enum_name),
                     ");\n        };\n        ", protocol,
                     ".reset_json_field(protobuf_reset_union);\n        if (", protocol,
                     ".null_value()) { continue; }\n        ", protocol,
                     ".message([&](auto& nested) {\n          int protobuf_selected = -1;\n"
                     "          while (nested.next_field()) {\n");
        for (std::size_t index = 0; index < item.type_name_list.size(); ++index) {
          const auto& alternative = item.type_name_list[index];
          const auto payload = "this->" + member_name(item) + "." + field_name(alternative.enum_name);
          const auto tag = union_enum_name(item) + "::" + enum_name(alternative.enum_name);
          output.write("            if (nested.template match<", index + 1, ">(\"", alternative.enum_name,
                       "\", \"", protobuf_json_name(alternative.enum_name), "\")) {\n"
                       "              if (nested.null_value()) {\n"
                       "                if (protobuf_selected == ", index, ") {\n"
                       "                  protobuf_reset_union();\n"
                       "                  protobuf_selected = -1;\n"
                       "                }\n                continue;\n              }\n"
                       "              nested.oneof(protobuf_selected, ", index, ");\n"
                       "              if (this->", union_tag_name(item), " != ", tag, ") {\n"
                       "                ::std::construct_at(&", payload, ");\n"
                       "                ::rohit::serializer::detail::protobuf_reset<ProtobufProtocol::format>(", payload, ");\n"
                       "                this->", union_tag_name(item), " = ", tag, ";\n              }\n"
                       "              nested.field(", payload, ");\n              continue;\n            }\n");
        }
        output.write("            nested.unknown();\n          }\n        });\n");
      } else {
        output.write("        ", protocol, ".field(this->", member_name(item), ");\n");
      }
      output.write("        continue;\n      }\n");
      if (!condition.empty()) {
        output.write("    }\n");
      }
    }
    output.write("      ", protocol, ".unknown();\n    }\n");
    if (object.has_magic()) {
      const auto condition = protobuf_field_condition(object.magic_omitted_formats, "ProtobufProtocol::format");
      if (!condition.empty()) { output.write("    if constexpr (", condition, ") {\n"); }
      output.write("    if (!protobuf_seen_magic) { throw ::std::invalid_argument{\"Missing magic\"}; }\n");
      if (!condition.empty()) { output.write("    }\n"); }
    }
    if (object.reference) { output.write("    validate_address();\n"); }
    output.write("  }\n");
  }

  // Build a typed view codec expression without introducing a runtime field descriptor table.
  std::string view_codec_name(const std::string& name, const syntax_node* node, storage_mode mode) {
    const std::string prefix{"::rohit::serializer::detail::"};
    if (!node && name == "string") {
      return prefix + "string_view_codec";
    }
    const auto type = storage_type_name(name, node, mode);
    return prefix +
           (node && node->type == object_type::class_type ? "object_view_codec<"
                                                          : "scalar_view_codec<") +
           type + ">";
  }

  // Compose collection and union codecs from their statically resolved element types.
  std::string view_codec_name(const member& field, storage_mode mode) {
    const auto& type = field.type_name_list.front();
    auto codec = view_codec_name(type.name, type.resolved_node, mode);
    const std::string prefix{"::rohit::serializer::detail::"};
    switch (field.modifier) {
    case member::modifier_type::none:
      return codec;
    case member::modifier_type::array:
      return prefix + "array_view_codec<" + codec + ">";
    case member::modifier_type::map:
      return prefix + "map_view_codec<" +
             view_codec_name(field.key, field.key_node, storage_mode::read_only_view) + ", " +
             codec + ">";
    case member::modifier_type::variant:
      codec = prefix + "variant_view_codec<";
      for (std::size_t index = 0; index < field.type_name_list.size(); ++index) {
        const auto& alternative = field.type_name_list[index];
        if (index != 0) {
          codec += ", ";
        }
        codec += view_codec_name(alternative.name, alternative.resolved_node, mode);
      }
      return codec + ">";
    }
    throw std::invalid_argument{"Unknown view field modifier"};
  }

  // Emit a constant-offset getter; access follows the schema field or parent's visibility.
  void write_view_getter(rohit::type_check::output_buffer auto& output, const std::string& name,
                         std::size_t index, access_type access, access_type& current_access) {
    output.write(access_section(access, current_access),
                 "  // Read this field or borrow its nested view from the mapped buffer.\n"
                 "  [[nodiscard]] auto ",
                 function_name(name),
                 "() const {\n"
                 "    return ",
                 type_name("serializer_view_field_" + std::to_string(index)),
                 "::read(this->template field_bytes<", index, ">(), this->view_limits);\n  }\n");
  }

  // Emit one concrete view representation with validated mapping and size-preserving mutation.
  void write_view_class(rohit::type_check::output_buffer auto& output, const class_node* obj,
                        storage_mode mode) {
    const auto count = obj->parents.size() + obj->member_list.size();
    const std::string byte_type =
        mode == storage_mode::read_only_view ? "const ::std::uint8_t" : "::std::uint8_t";
    const auto base = "::rohit::serializer::detail::binary_view_base<" + byte_type + ", " +
                      std::to_string(count) + ">";
    if (obj->multiple_modes()) {
      output.write("template <>\n");
    }
    output.write("class ", type_name(obj->name));
    if (obj->multiple_modes()) {
      output.write("<", storage_mode_name(mode), ">");
    }
    output.write(" : public ", base, (std::string{" {\n  using "} + type_name("view_base") + " = "),
                 base, (std::string{";\n  using "} + type_name("view_byte") + " = "), byte_type,
                 ";\n");
    std::size_t index{};
    for (const auto& parent : obj->parents) {
      output.write("  using ", type_name("serializer_view_field_" + std::to_string(index++)), " = ",
                   view_codec_name(parent.name, parent.parent_class, mode), ";\n");
    }
    for (const auto& field : obj->member_list) {
      output.write("  using ", type_name("serializer_view_field_" + std::to_string(index++)), " = ",
                   view_codec_name(field, mode), ";\n");
    }
    output.write(
        "  // Construct only through map(), validating before exposing the object.\n"
        "  explicit ",
        type_name(obj->name),
        (std::string{"(::std::span<"} + type_name("view_byte") +
         (std::string{"> "} + local_name("bytes") + ", ::rohit::serializer::decode_limits " +
          local_name("limits") + ")\n      : ") +
         type_name("view_base") +
         (std::string{"{"} + local_name("bytes") + ", " + local_name("limits") +
          ", serializer_scan} {}\npublic:\n  // Borrow an exact little-endian positional message; "
          "its storage must outlive all views.\n  [[nodiscard]] static ")),
        type_name(obj->name),
        (std::string{" map(::std::span<"} + type_name("view_byte") +
         (std::string{"> "} + local_name("bytes") +
          ",\n                    ::rohit::serializer::decode_limits " + local_name("limits") +
          " = {}) {\n    return ")),
        type_name(obj->name),
        (std::string{"{"} + local_name("bytes") + ", " + local_name("limits") +
         "};\n  }\n  // Internal schema traversal: share nested budgets and optionally record "
         "field offsets.\n  static void "
         "serializer_scan(::rohit::serializer::detail::view_scanner& " +
         local_name("scanner") + ",\n                              ::std::size_t* " +
         local_name("offsets") + ") {\n    auto " + local_name("nesting") + " = " +
         local_name("scanner") + ".enter_object();\n"));
    for (index = 0; index < count; ++index) {
      output.write(
          (std::string{"    if ("} + local_name("offsets") + ") { " + local_name("offsets") + "["),
          index, (std::string{"] = "} + local_name("scanner") + ".position(); }\n    "),
          type_name("serializer_view_field_" + std::to_string(index)),
          (std::string{"::scan("} + local_name("scanner") + ");\n"));
    }
    output.write(
        (std::string{"    if ("} + local_name("offsets") + ") { " + local_name("offsets") + "["),
        count, (std::string{"] = "} + local_name("scanner") + ".position(); }\n  }\n"));
    auto current_access = access_type::public_access;
    index = 0;
    for (const auto& parent : obj->parents) {
      write_view_getter(output, "get_base_" + std::to_string(parent.id), index++, parent.access, current_access);
    }
    for (const auto& field : obj->member_list) {
      write_view_getter(output, "get_" + field.name, index, field.access, current_access);
      if (field.modifier == member::modifier_type::variant) {
        output.write("  enum class ", union_enum_name(field), " {\n");
        for (const auto& alternative : field.type_name_list) {
          output.write("    ", enum_name(alternative.enum_name), ",\n");
        }
        output.write("  };\n"
                     "  // Return the active alternative using the schema's symbolic names.\n"
                     "  [[nodiscard]] ",
                     union_enum_name(field), " ", function_name("get_" + field.name + "_type"),
                     "() const {\n    return static_cast<", union_enum_name(field), ">(",
                     function_name("get_" + field.name), "().index());\n  }\n");
      } else if (mode == storage_mode::mutable_view &&
                 field.modifier == member::modifier_type::none &&
                 field.type_name_list.front().type != object_type::class_type) {
        const auto& type = field.type_name_list.front();
        const auto value_type = type.name == "string"
                                    ? "::std::string_view"
                                    : storage_type_name(type);
        output.write(
            "  // Update existing bytes; a size-changing replacement throws before mutation.\n"
            "  void ",
            function_name("set_" + field.name), "(", value_type,
            (std::string{" "} + local_name("value") + ") {\n    "),
            type_name("serializer_view_field_" + std::to_string(index)),
            "::write(this->template field_bytes<", index,
            (std::string{">(), "} + local_name("value") + ");\n  }\n"));
      }
      ++index;
    }
    output.write("}; // view ", type_name(obj->name), "\n\n");
  }

  // Emit the existing owning API, selecting owning specializations for nested classes.
  void write_owning_class(rohit::type_check::output_buffer auto& out_stream,
                          const class_node* obj) {
    if (obj->reference && obj->reference->value_projection) {
      // The synthetic payload field describes its wire type only; an export stores one handle.
      out_stream.write("class ", type_name(obj->name), " {\n");
      out_stream.write(reference_support::cpp_methods(*obj, type_name(obj->name),
          [&](const auto& value) { return generic_default(value); },
          [&](auto name, auto) { return field_name(name); }, {},
          [&](const auto& value) { return get_cpp_type(value); }));
      out_stream.write("}; // class ", type_name(obj->name), "\n\n");
      return;
    }
    if (obj->type == object_type::generic_definition) {
      out_stream.write("template <");
      for (std::size_t index = 0; index < obj->generic_parameters.size(); ++index) {
        const auto& parameter = obj->generic_parameters[index];
        if (index != 0) { out_stream.write(", "); }
        out_stream.write(std::string_view{parameter.kind == generic_argument_kind::dimension ? "::std::uint64_t " : "typename "}, parameter.name);
        if (!parameter.default_argument.empty()) {
          out_stream.write(" = ", generic_default(parameter.default_argument.front()));
        }
      }
      out_stream.write(">\n");
      out_stream.write(template_constraints(*obj));
    }
    if (obj->multiple_modes()) {
      out_stream.write("template <>\n");
    }
    if ((obj->attributes & class_attributes::packed) == class_attributes::packed) {
      out_stream.write("class __attribute__ ((__packed__)) ", type_name(obj->name));
    } else {
      out_stream.write("class ", type_name(obj->name));
    }
    if (obj->multiple_modes()) {
      out_stream.write("<", storage_mode_name(storage_mode::owning), ">");
    }

    if (!obj->parents.empty()) {
      out_stream.write(" : ");
      write_parent_list(out_stream, obj->parents);
    }

    out_stream.write(" {\n");
    auto current_access = access_type::private_access;
    const auto cpp_access_section = [&](auto access) { return access_section(access, current_access); };
    managed_.write_access_friends(out_stream, obj);
    if (obj->has_magic()) {
      out_stream.write(cpp_access_section(obj->magic_access),
                       "  // Fixed schema-owned header");
      if (!obj->magic_field) {
        out_stream.write(" \"");
        for (const auto byte : obj->magic_bytes) {
          out_stream.write(escape_magic_byte(static_cast<unsigned char>(byte)));
        }
        out_stream.write("\"");
      }
      out_stream.write("; no per-object storage.\n");
      if (obj->magic_field) {
        out_stream.write("  inline static constexpr ", get_cpp_type(*obj->magic_field),
                         " magic = ", magic_literal(*obj->magic_field), ";\n");
      } else {
        out_stream.write("  inline static constexpr char magic[] = {");
        for (std::size_t index = 0; index < obj->magic_bytes.size(); ++index) {
          if (index != 0) { out_stream.write(", "); }
          out_stream.write('\'', escape_magic_byte(static_cast<unsigned char>(obj->magic_bytes[index])), '\'');
        }
        out_stream.write("};\n");
      }
    }
    if (obj->type == object_type::generic_definition) {
      for (const auto& parameter : obj->generic_parameters) {
        if (parameter.kind == generic_argument_kind::dimension) {
          out_stream.write("  static_assert(", parameter.name, " > 0, \"Invalid dimension: expected positive uint64\");\n");
        } else if (parameter.stateless) {
          out_stream.write("  static_assert(::std::is_empty_v<", parameter.name,
              ">, \"Stateless generic argument requires an empty type\");\n");
        }
      }
    }
    write_ordered_body(out_stream, obj, current_access);
    out_stream.write(construction::factories(*obj, type_name(obj->name), "cpp",
        [&](const auto& value) { return generic_default(value); },
        [&](auto name, auto) { return field_name(name); },
        [&](const auto& value) { return default_value(value); },
        [&](auto name) { return function_name(name); },
        [&](const auto& node, const auto& arguments) {
          // Bind in schema order before the host call, preserving each argument's value category.
          std::string bindings;
          std::vector<std::string> forwarded;
          for (std::size_t index{}; index < arguments.size(); ++index) {
            const auto name = "srl_call_argument_" + std::to_string(index);
            bindings += "auto&& " + name + " = " + arguments[index] + "; ";
            forwarded.push_back("::std::forward<decltype(" + name + ")>(" + name + ")");
          }
          return "([&]() { " + bindings + "return " + readonly_call(obj, node, forwarded) + "; }())";
        },
        [&](const auto& expression, const auto& symbol) { return construction_expression(obj, expression, symbol); },
        cpp_access_section));
    const bool has_translator = std::any_of(obj->conversions.begin(), obj->conversions.end(), [](const auto& method) { return method.translator; });
    if (has_translator && (emission_only || constant_evaluation_enabled || protobuf_enabled)) {
      throw std::invalid_argument{"C++ translators require ordinary native JSON/binary output; emission-only, constant-evaluation and Protobuf profiles are unsupported"};
    }
    if (emission_only && !obj->conversions.empty()) {
      throw std::invalid_argument{"Borrowed emission models cannot expose owning conversions"};
    }
    out_stream.write(conversion::methods(*obj, type_name(obj->name), "cpp",
        [&](const auto& value) { return generic_default(value); },
        [&](auto name, auto) { return field_name(name); },
        [&](auto name, auto) { return function_name(name); }, cpp_access_section));
    out_stream.write(translator::methods(*obj, type_name(obj->name), "cpp",
        [&](const auto& value) { return generic_default(value); },
        [&](auto name, auto) { return field_name(name); },
        [&](const auto& value) { return default_value(value); },
        [&](const auto& method, auto view) { return translator_codecs(method, view); }));
    if (has_translator) {
      current_access = access_type::public_access;
    }
    out_stream.write(reference_support::cpp_methods(*obj, type_name(obj->name),
        [&](const auto& value) { return generic_default(value); },
        [&](auto name, auto) { return field_name(name); }, cpp_access_section,
        [&](const auto& value) { return get_cpp_type(value); }));
    write_behavior_contract(out_stream, obj, false, &current_access);
    if (emission_only && !obj->arithmetic.empty()) {
      throw std::invalid_argument{"Borrowed emission models cannot expose owning arithmetic"};
    }
    out_stream.write(arithmetic::operators(*obj,
        [&](const auto& value) { return generic_default(value); },
        [&](auto name) { return function_name(name); }, cpp_access_section));
    write_comparisons(out_stream, obj, current_access);

    out_stream.write('\n', cpp_access_section(access_type::public_access),
                     "  static constexpr bool serializer_reuses_storage = ",
                     std::string_view{emission_only ? "false" : "true"}, ";\n\n");
    if (emission_only) {
      out_stream.write("  // This model borrows its input and only supports positional output.\n"
                       "  static constexpr bool serializer_emission_only = true;\n\n");
    }
    if (constant_evaluation_enabled) {
      out_stream.write("  // Opt in to explicit constant-evaluation binary APIs without object storage.\n"
                       "  static constexpr bool serializer_constant_evaluation = true;\n"
                       "  using serializer_constant_evaluation_type = ", type_name(obj->name),
                       ";\n\n");
    }
    if (xml_options_.enabled) { validate_xml_names(*obj); }
    auto durable = native_layout(*obj, std::string_view{});
    write_serializer(out_stream, &durable);

    out_stream.write("}; // class ", type_name(obj->name), "\n\n");
    write_behavior_interface(out_stream, obj);
  }

  // Abstract dispatch operates on a concrete data receiver without making its codec type abstract.
  void write_behavior_interface(rohit::type_check::output_buffer auto& output, const class_node* owner) {
    const auto contracts = behavior::contracts(*owner);
    if (contracts.empty()) { return; }
    std::string template_header, template_arguments;
    if (owner->type == object_type::generic_definition) {
      template_header = "template <"; template_arguments = "<";
      for (std::size_t index{}; index < owner->generic_parameters.size(); ++index) {
        const auto& parameter = owner->generic_parameters[index];
        if (index) { template_header += ", "; template_arguments += ", "; }
        template_header += std::string(parameter.kind == generic_argument_kind::dimension ? "::std::uint64_t " : "typename ") + parameter.name;
        template_arguments += parameter.name;
        if (!parameter.default_argument.empty()) { template_header += " = " + generic_default(parameter.default_argument.front()); }
      }
      template_header += ">\n" + template_constraints(*owner); template_arguments += ">";
    }
    const auto model = type_name(owner->name) + template_arguments +
        (owner->multiple_modes() ? "<::rohit::serializer::storage_mode::owning>" : "");
    const auto interface_name = type_name(owner->name + "_behavior");
    const auto interface_type = interface_name + template_arguments;
    const auto adapter_name = type_name(owner->name + "_behavior_adapter");
    output.write(template_header, "class ", interface_name, " {\npublic:\n  virtual ~", interface_name, "() = default;\n");
    for (const auto* method : contracts) {
      output.write("  virtual ", method->return_type.name == "void" ? "void" : generic_default(method->return_type),
          " ", function_name(method->name), "(",
          std::string_view{method->effect == function_declaration::effect_type::readonly ? "const " : ""}, model, "& receiver");
      for (const auto& argument : method->parameters) {
        output.write(", ", generic_default(argument.type), " ", local_name(argument.name));
      }
      output.write(") const", std::string_view{method->cpp_noexcept ? " noexcept" : ""}, " = 0;\n");
    }
    output.write("};\n", template_header, "class ", adapter_name, " {\n  ", model, "* receiver_;\n  const ", interface_type,
        "* implementation_;\npublic:\n  ", adapter_name, "(", model, "& receiver, const ", interface_type,
        "& implementation) : receiver_(&receiver), implementation_(&implementation) {}\n");
    for (const auto* method : contracts) {
      output.write("  ", method->return_type.name == "void" ? "void" : generic_default(method->return_type),
          " ", function_name(method->name), "(");
      for (std::size_t index{}; index < method->parameters.size(); ++index) {
        const auto& argument = method->parameters[index];
        if (index) { output.write(", "); }
        output.write(generic_default(argument.type), " ", local_name(argument.name));
      }
      output.write(")", std::string_view{method->effect == function_declaration::effect_type::readonly ? " const" : ""});
      if (method->cpp_noexcept) {
        // Copying by-value arguments belongs to the wrapper's guarantee, not the native implementation's promise.
        output.write(" noexcept(noexcept(implementation_->", function_name(method->name), "(*receiver_");
        for (const auto& argument : method->parameters) { output.write(", ", local_name(argument.name)); }
        output.write(")))");
      }
      output.write(" { return implementation_->", function_name(method->name), "(*receiver_");
      for (const auto& argument : method->parameters) { output.write(", ", local_name(argument.name)); }
      output.write("); }\n");
    }
    output.write("};\n\n");
  }

  // Emit only requested modes; a single mode has no class template declaration.
  void write_class(rohit::type_check::output_buffer auto& output, const class_node* obj) {
    if (obj->multiple_modes()) {
      output.write("template <::rohit::serializer::storage_mode Mode>\nclass ",
                   type_name(obj->name), ";\n\n");
    }
    if (obj->has_mode(storage_mode::owning)) {
      write_owning_class(output, obj);
    }
    for (const auto mode : {storage_mode::read_only_view, storage_mode::mutable_view}) {
      if (obj->has_mode(mode)) {
        write_view_class(output, obj, mode);
      }
    }
  }

  // Find owning variants in concrete and generic declarations before selecting header support.
  bool contains_owning_variants(const std::vector<std::unique_ptr<syntax_node>>& statements) {
    for (const auto& statement : statements) {
      if (statement->type == object_type::class_type || statement->type == object_type::generic_definition) {
        const auto& object = *static_cast<const class_node*>(statement.get());
        for (const auto& field : object.member_list) {
          if (field.owning_variant) { return true; }
        }
      } else if (statement->type == object_type::namespace_type &&
                 contains_owning_variants(static_cast<const namespace_node*>(statement.get())->statements)) {
        return true;
      }
    }
    return false;
  }

  // Keep view helpers out of generated headers whose schemas request only owning objects.
  bool contains_views(const std::vector<std::unique_ptr<syntax_node>>& statements) {
    for (const auto& statement : statements) {
      if (statement->type == object_type::class_type) {
        const auto* obj = static_cast<const class_node*>(statement.get());
        if (obj->has_mode(storage_mode::read_only_view) ||
            obj->has_mode(storage_mode::mutable_view)) {
          return true;
        }
      } else if (statement->type == object_type::namespace_type &&
                 contains_views(static_cast<const namespace_node*>(statement.get())->statements)) {
        return true;
      }
    }
    return false;
  }

  // Emit enum values, allocation-free spellings, and collision-aware name conversion.
  void write_enum(rohit::type_check::output_buffer auto& out_stream, const enum_node* enum_ptr) {
    out_stream.write("enum class ", type_name(enum_ptr->name), " {\n");
    for (const auto& name : enum_ptr->enum_name_list) {
      out_stream.write("  ", enum_name(name), ",\n");
    }
    out_stream.write("};\n\n"
                     "// Validate numeric input against the declared enum alternatives.\n"
                     "[[nodiscard]] constexpr bool serializer_enum_valid(",
                     type_name(enum_ptr->name),
                     (std::string{" "} + local_name("value") + ") noexcept {\n  switch (" +
                      local_name("value") + ") {\n"));
    for (const auto& name : enum_ptr->enum_name_list) {
      out_stream.write("    case ", type_name(enum_ptr->name), "::", enum_name(name),
                       ": return true;\n");
    }
    if (emission_only) {
      out_stream.write("    default: return false;\n  }\n}\n\n");
      return;
    }
    out_stream.write("    default: return false;\n  }\n}\n\n"
                     "// Borrow the unchanged wire spelling for this enum.\n"
                     "[[nodiscard]] constexpr ::std::string_view serializer_enum_name(",
                     type_name(enum_ptr->name),
                     (std::string{" "} + local_name("value") + ") {\n  switch (" +
                      local_name("value") + ") {\n"));
    for (const auto& name : enum_ptr->enum_name_list) {
      out_stream.write("    case ", type_name(enum_ptr->name), "::", enum_name(name), ": return \"",
                       name, "\";\n");
    }
    out_stream.write(
        "    default: throw ::std::invalid_argument{\"Unknown enum value\"};\n  }\n}\n\n"
        "// Preserve the owning-string API for callers that need a copy.\n"
        "[[nodiscard]] constexpr ::std::string to_string(",
        type_name(enum_ptr->name),
        (std::string{" "} + local_name("value") +
         ") {\n  return ::std::string{serializer_enum_name(" + local_name("value") +
         ")};\n}\n\n// Resolve only a complete matching wire spelling.\n[[nodiscard]] constexpr "),
        type_name(enum_ptr->name), " ", function_name("to_" + enum_ptr->name),
        (std::string{"(const auto& "} + local_name("value") + ") {\n  const ::std::string_view " +
         local_name("name") + "{" + local_name("value") +
         "};\n  switch (::rohit::serializer::detail::field_name_hash(" + local_name("name") +
         ")) {\n"));
    std::map<std::uint64_t, std::vector<std::string_view>> groups;
    for (const auto& name : enum_ptr->enum_name_list) {
      groups[detail::field_name_hash(name)].push_back(name);
    }
    for (const auto& [hash_value, names] : groups) {
      static_cast<void>(hash_value);
      out_stream.write("    case ::rohit::serializer::detail::field_name_hash(\"", names.front(),
                       "\"):\n");
      for (const auto name : names) {
        out_stream.write((std::string{"      if ("} + local_name("name") + " == \""), name,
                         "\") { return ", type_name(enum_ptr->name), "::", enum_name(name),
                         "; }\n");
      }
      out_stream.write("      break;\n");
    }
    out_stream.write("    default: break;\n  }\n"
                     "  throw ::std::invalid_argument{\"Unknown enum name\"};\n}\n\n"
                     "// Decode a name when this enum appears in a collection.\n"
                     "constexpr void serializer_enum_from_name(",
                     type_name(enum_ptr->name),
                     (std::string{"& "} + local_name("value") + ", ::std::string_view " +
                      local_name("name") + ") { " + local_name("value") + " = "),
                     function_name("to_" + enum_ptr->name),
                     (std::string{"("} + local_name("name") + "); }\n\n"));
  }
  // Emit C++ namespace for the parsed schema.
  void write_namespace(rohit::type_check::output_buffer auto& out_stream,
                       const namespace_node* namespace_ptr) {
    std::string full_name = namespace_name(namespace_ptr->name);
    while (namespace_ptr->statements.size() == 1 &&
           namespace_ptr->statements.back()->type == object_type::namespace_type) {
      namespace_ptr = dynamic_cast<namespace_node*>(namespace_ptr->statements.back().get());
      full_name += "::";
      full_name += namespace_name(namespace_ptr->name);
    }
    out_stream.write("namespace ", full_name, " {\n");
    write_statement_list(out_stream, namespace_ptr->statements);
    out_stream.write("} // namespace ", full_name, "\n\n");
  }

  // Declare callable signature types before bodies so qualified and forward calls remain valid.
  void write_function_inventory(rohit::type_check::output_buffer auto& output,
                                const std::vector<std::unique_ptr<syntax_node>>& statements,
                                unsigned phase) {
    for (const auto& statement : statements) {
      if (statement->type == object_type::namespace_type) {
        output.write("namespace ", namespace_name(statement->name), " {\n");
        write_function_inventory(output, static_cast<const namespace_node&>(*statement).statements, phase);
        output.write("}\n");
        continue;
      }
      full_stream_auto_alloc unit;
      if (phase == 0 && statement->type == object_type::enum_type) {
        unit.write("enum class ", type_name(statement->name), ";\n");
      } else if (phase == 0 && (statement->type == object_type::class_type ||
                               statement->type == object_type::generic_definition)) {
        const auto& object = static_cast<const class_node&>(*statement);
        if (!object.generic_name.empty() && !object.supports_managed()) { continue; }
        if (object.type == object_type::generic_definition) {
          unit.write("template <");
          for (std::size_t index{}; index < object.generic_parameters.size(); ++index) {
            if (index) { unit.write(", "); }
            const auto& parameter = object.generic_parameters[index];
            unit.write(std::string_view{parameter.kind == generic_argument_kind::dimension ?
                "::std::uint64_t " : "typename "}, parameter.name);
          }
          unit.write(">\n");
          unit.write(template_constraints(object));
        } else if (object.multiple_modes()) {
          unit.write("template <::rohit::serializer::storage_mode Mode>\n");
        }
        unit.write("class ", type_name(object.name), ";\n");
      } else if (phase == 1 && statement->type == object_type::class_type) {
        const auto& object = static_cast<const class_node&>(*statement);
        if (object.generic_name.empty() || object.supports_managed()) { continue; }
        const auto* definition = generic_definitions.at(object.generic_name);
        unit.write("using ", type_name(object.name), " = ", full_type_name(definition), "<");
        for (std::size_t index{}; index < object.generic_arguments.size(); ++index) {
          if (index) { unit.write(", "); }
          unit.write(generic_default(object.generic_arguments[index]));
        }
        unit.write(">;\n");
      } else if (phase == 2 && statement->type == object_type::function_type) {
        write_behavior_method(unit, nullptr, static_cast<const function_node&>(*statement).function, false, true);
      } else { continue; }
      const std::string_view source{reinterpret_cast<const char*>(unit.begin()), unit.current_offset()};
      std::string identity = "forward:" + full_type_name(statement.get());
      if (statement->type == object_type::function_type) {
        const auto& function = static_cast<const function_node&>(*statement).function;
        for (const auto& parameter : function.parameters) { identity += ":" + behavior_parameter_type(parameter); }
      }
      write_reusable_unit(output, identity, source);
    }
  }

  // Find free functions while collecting template definitions needed by their signature aliases.
  bool contains_functions(const std::vector<std::unique_ptr<syntax_node>>& statements) {
    bool found = false;
    for (const auto& statement : statements) {
      if (statement->type == object_type::namespace_type) {
        found = contains_functions(static_cast<const namespace_node&>(*statement).statements) || found;
      } else if (statement->type == object_type::function_type) { found = true; }
      else if (statement->type == object_type::generic_definition) {
        generic_definitions.emplace(statement->get_full_name(), static_cast<const class_node*>(statement.get()));
        const auto& owner = static_cast<const class_node&>(*statement);
        found = found || std::any_of(owner.constructions.begin(), owner.constructions.end(),
            [](const auto& construction) { return construction.forward_signature_enabled; });
      } else if (statement->type == object_type::class_type) {
        const auto& owner = static_cast<const class_node&>(*statement);
        found = found || std::any_of(owner.constructions.begin(), owner.constructions.end(),
            [](const auto& construction) { return construction.forward_signature_enabled; });
      }
    }
    return found;
  }

  // Emit C++ statement list for the parsed schema.
  void write_statement_list(rohit::type_check::output_buffer auto& out_stream,
                            const std::vector<std::unique_ptr<syntax_node>>& statements) {
    if (statements.empty()) {
      return;
    }

    bool emission_namespace_open{false};
    for (const auto& statement : statements) {
      if (!owns_source(statement.get())) { continue; }
      if (emission_only) {
        if (statement->type == object_type::namespace_type && emission_namespace_open) {
          out_stream.write("} // namespace emission\n\n");
          emission_namespace_open = false;
        } else if (statement->type != object_type::namespace_type && !emission_namespace_open) {
          out_stream.write("namespace emission {\n");
          emission_namespace_open = true;
        }
      }
      if (statement->type == object_type::namespace_type) {
        write_namespace(out_stream, dynamic_cast<const namespace_node*>(statement.get()));
        continue;
      }
      full_stream_auto_alloc unit_output{};
      switch (statement->type) {
      case object_type::class_type:
        if (!static_cast<const class_node*>(statement.get())->generic_name.empty() &&
            !static_cast<const class_node*>(statement.get())->supports_managed()) {
          const auto& object = static_cast<const class_node&>(*statement);
          const auto* definition = generic_definitions.at(object.generic_name);
          unit_output.write("using ", type_name(object.name), " = ", full_type_name(definition), "<");
          for (std::size_t index = 0; index < object.generic_arguments.size(); ++index) {
            if (index != 0) { unit_output.write(", "); }
            const auto& argument = object.generic_arguments[index];
            unit_output.write(argument.kind == generic_argument_kind::dimension ?
                std::to_string(argument.dimension) + "ULL" : storage_type_name(argument));
          }
          unit_output.write(">;\n\n");
          if (!behavior::contracts(object).empty()) {
            const auto qualified = full_type_name(definition);
            const auto separator = qualified.rfind("::");
            const auto scope = separator == std::string::npos ? std::string{} : qualified.substr(0, separator + 2);
            for (const auto suffix : {"_behavior", "_behavior_adapter"}) {
              unit_output.write("using ", type_name(object.name + suffix), " = ", scope, type_name(definition->name + suffix), "<");
              for (std::size_t index{}; index < object.generic_arguments.size(); ++index) {
                if (index) { unit_output.write(", "); }
                const auto& argument = object.generic_arguments[index];
                unit_output.write(argument.kind == generic_argument_kind::dimension ?
                    std::to_string(argument.dimension) + "ULL" : storage_type_name(argument));
              }
              unit_output.write(">;\n");
            }
          }
          break;
        }
        if (!managed_.write_primary(unit_output, static_cast<const class_node*>(statement.get()))) {
          write_class(unit_output, static_cast<const class_node*>(statement.get()));
        }
        break;

      case object_type::generic_definition:
        generic_definitions.emplace(statement->get_full_name(), static_cast<const class_node*>(statement.get()));
        if (!static_cast<const class_node*>(statement.get())->supports_managed()) {
          write_class(unit_output, static_cast<const class_node*>(statement.get()));
        }
        break;

      case object_type::enum_type:
        write_enum(unit_output, dynamic_cast<const enum_node*>(statement.get()));
        break;

      case object_type::function_type:
        write_behavior_method(unit_output, nullptr, static_cast<const function_node&>(*statement).function);
        break;

      default:
        break;
      }
      const std::string_view unit_source{reinterpret_cast<const char*>(unit_output.begin()),
                                        unit_output.current_offset()};
      std::string identity = full_type_name(statement.get());
      if (statement->type == object_type::function_type) {
        for (const auto& parameter : static_cast<const function_node&>(*statement).function.parameters) {
          identity += ":" + behavior_parameter_type(parameter);
        }
      }
      write_reusable_unit(out_stream, identity, unit_source);
    }
    if (emission_namespace_open) { out_stream.write("} // namespace emission\n\n"); }
  }

  std::map<std::string, const class_node*> generic_definitions{};
  // Render only the checked schema grammar, never arbitrary source text as C++.
  std::string dimension_default(const dimension_expression& value) {
    using operation = dimension_expression::operation;
    if (value.kind == operation::literal) { return std::to_string(value.value) + "ULL"; }
    if (value.kind == operation::parameter) { return value.name; }
    return std::string{"::rohit::serializer::detail::"} +
        (value.kind == operation::add ? "dimension_add(" : "dimension_multiply(") +
        dimension_default(value.operands[0]) + ", " + dimension_default(value.operands[1]) + ")";
  }

  // Retain earlier parameter references and declaration-scope type defaults in public aliases.
  std::string generic_default(const ::rohit::serializer::type_name& value) {
    if (value.name == "void" && !value.application) { return "void"; }
    if (value.is_expected()) {
      return "::std::expected<" + generic_default(value.arguments.at(0)) + ", " +
          generic_default(value.arguments.at(1)) + ">";
    }
    if (value.common_numeric) {
      return "::std::common_type_t<" + generic_default(value.arguments.at(0)) + ", " +
          generic_default(value.arguments.at(1)) + ">";
    }
    if (value.is_optional() || value.is_sequence()) {
      return std::string{value.is_optional() ? "::std::optional<" : "::std::vector<"} +
             generic_default(value.arguments.front()) + ">";
    }
    if (value.is_associative()) {
      return "::std::map<" + generic_default(value.arguments[0]) + ", " +
             generic_default(value.arguments[1]) + ">";
    }
    if (value.kind == generic_argument_kind::dimension && value.dimension != 0) {
      return std::to_string(value.dimension) + "ULL";
    }
    if (!value.expression.empty()) { return dimension_default(value.expression.front()); }
    std::string result = storage_type_name(value);
    if (value.application) {
      result += "<";
      for (std::size_t index = 0; index < value.arguments.size(); ++index) {
        if (index != 0) { result += ", "; }
        result += generic_default(value.arguments[index]);
      }
      result += ">";
    }
    return result;
  }

  // Refuse unsupported restricted profiles without removing requested schema semantics.
  void validate_positional_profile(const std::vector<std::unique_ptr<syntax_node>>& statements) {
    for (const auto& statement : statements) {
      if (emission_only && (statement->type == object_type::namespace_type ?
              namespace_name(statement->name) : type_name(statement->name)) == "emission") {
        throw std::invalid_argument{"cpp.emission_only reserves the emission namespace name"};
      }
      if (statement->type == object_type::namespace_type) {
        validate_positional_profile(static_cast<const namespace_node&>(*statement).statements);
      } else if (emission_only && statement->type == object_type::function_type) {
        throw std::invalid_argument{"cpp.emission_only does not support namespace functions"};
      } else if (statement->type == object_type::class_type ||
                 statement->type == object_type::generic_definition) {
        const auto& object = static_cast<const class_node&>(*statement);
        if (object.supports_managed() || object.has_mode(storage_mode::read_only_view) ||
            object.has_mode(storage_mode::mutable_view)) {
          throw std::invalid_argument{"cpp.protocols binary_none does not support managed or view models"};
        }
        if (emission_only) {
          if (std::any_of(object.functions.begin(), object.functions.end(), [](const auto& method) {
                return method.has_native_signature();
              })) {
            throw std::invalid_argument{"cpp.emission_only does not support native callable signatures"};
          }
          if (!object.constructions.empty()) {
            throw std::invalid_argument{"cpp.emission_only does not support owning construction APIs"};
          }
          for (const auto& field : object.member_list) {
            if (!field.readonly_getter.empty()) {
              throw std::invalid_argument{"cpp.emission_only does not support owning readonly field exposure"};
            }
            if (field.array_exposure) {
              throw std::invalid_argument{"cpp.emission_only does not support owning array exposure"};
            }
            if (field.modifier == member::modifier_type::map) {
              throw std::invalid_argument{"cpp.emission_only does not support map fields"};
            }
            if (field.modifier == member::modifier_type::array &&
                (field.extent_expression.size() > 1 || !field.default_value.empty())) {
              throw std::invalid_argument{
                  "cpp.emission_only requires one-dimensional arrays without schema initializers"};
            }
          }
        }
      }
    }
  }

  // Generate a complete C++ header while retaining the parsed schema and original wire names.
  void emit(rohit::type_check::output_buffer auto& out_stream,
            const std::vector<std::unique_ptr<syntax_node>>& statements) {
    if (protobuf_enabled && reference_support::has_value_references(statements)) {
      throw std::invalid_argument{
          "C++ value references support native JSON/binary output; Protobuf is unsupported"};
    }
    if (reference_support::has_references(statements) &&
        (emission_only || constant_evaluation_enabled)) {
      throw std::invalid_argument{
          "C++ reference bindings require ordinary runtime owning output; "
          "cpp.emission_only and cpp.constant_evaluation are unsupported"};
    }
    if (xml_options_.enabled) {
      std::set<std::string> source_names{};
      const auto collect = [&](auto&& self, const auto& nodes) -> void {
        for (const auto& node : nodes) {
          if (node->type == object_type::namespace_type) {
            self(self, static_cast<const namespace_node&>(*node).statements);
          } else if (node->type == object_type::class_type || node->type == object_type::generic_definition) {
            source_names.insert(node->get_full_name());
          }
        }
      };
      collect(collect, statements);
      for (const auto& [class_name, configuration] : xml_options_.classes) {
        (void)configuration;
        if (!source_names.contains(class_name)) { throw std::invalid_argument{"Unknown XML configured class: " + class_name}; }
      }
    }
    const auto has_expected_results = [&](const auto& self, const auto& nodes) -> bool {
      return std::any_of(nodes.begin(), nodes.end(), [&](const auto& node) {
        if (node->type == object_type::namespace_type) {
          return self(self, static_cast<const namespace_node&>(*node).statements);
        }
        if (node->type == object_type::function_type) {
          return static_cast<const function_node&>(*node).function.return_type.is_expected();
        }
        if (node->type == object_type::class_type || node->type == object_type::generic_definition) {
          const auto& owner = static_cast<const class_node&>(*node);
          return std::any_of(owner.functions.begin(), owner.functions.end(), [](const auto& method) {
            return method.return_type.is_expected() && (!method.guard || method.guard->available != false);
          });
        }
        return false;
      });
    };
    const bool expected_results = has_expected_results(has_expected_results, statements);
    const bool has_variants = contains_owning_variants(statements);
    if (has_variants && (protobuf_enabled || emission_only)) {
      throw std::invalid_argument{"C++ variant fields do not support cpp.protobuf or cpp.emission_only"};
    }
    if (positional_only) { validate_positional_profile(statements); }
    if (protobuf_enabled) { validate_protobuf_schema(statements); }
    validate_names(statements);
    auto_size::validate(statements, class_auto_sizes);
    bind_comparison_aliases(statements);
    comparison::validate(statements, class_comparisons);
    const bool has_comparisons = std::any_of(class_comparisons.begin(), class_comparisons.end(),
        [](const auto& entry) { return entry.second.equality || entry.second.ordering; });
    if (managed_separate_values && has_comparisons) {
      std::map<std::string, const class_node*> owners;
      comparison::collect(statements, owners);
      for (const auto& [name, owner] : owners) {
        const auto* settings = comparison::find(*owner, class_comparisons);
        if (owner->supports_managed() && settings && (settings->equality || settings->ordering)) {
          throw std::invalid_argument{
              "C++ managed comparisons do not support cpp.managed_separate_values: " + name};
        }
      }
    }
    if (emission_only && has_comparisons) {
      throw std::invalid_argument{"C++ record comparisons require owning output, not cpp.emission_only"};
    }
    managed_.prepare(statements);
    if (emission_only) {
      out_stream.write("#pragma once\n\n"
                       "#include <rohit/constant_binary_output.hpp>\n"
                       "#include <rohit/digest.hpp>\n\n"
                       "#include <array>\n#include <concepts>\n#include <cstddef>\n#include <cstdint>\n#include <memory>\n"
                       "#include <span>\n#include <stdexcept>\n#include <string_view>\n"
                       "#include <type_traits>\n\n");
      std::map<std::string, const class_node*> size_owners;
      auto_size::collect(statements, size_owners);
      if (std::any_of(size_owners.begin(), size_owners.end(), [](const auto& entry) {
            return !auto_size::fields(*entry.second).empty();
          })) {
        out_stream.write("#include <rohit/auto_size.hpp>\n\n");
      }
      write_source_marker(out_stream);
      write_included_headers(out_stream);
      contains_functions(statements);
      write_statement_list(out_stream, statements);
      return;
    }
    const bool has_views = contains_views(statements);
    out_stream.write("#pragma once\n\n"
                     "#include <rohit/serializer.hpp>\n"
                     "#include <rohit/numeric.hpp>\n"
                     "#include <rohit/digest.hpp>\n");
    if (!managed_.empty()) { out_stream.write("#include <rohit/managed.hpp>\n#include <optional>\n"); }
    if (protobuf_enabled) { out_stream.write("#include <rohit/protobuf.hpp>\n"); }
    if (xml_options_.enabled) { out_stream.write("#include <rohit/xml.hpp>\n"); }
    if (has_views) {
      out_stream.write("#include <rohit/binary_view.hpp>\n");
    }
    if (has_variants) { out_stream.write("#include <variant>\n"); }
    if (reference_support::has_references(statements)) {
      out_stream.write("#include <rohit/reference.hpp>\n");
    }
    out_stream.write("\n#include <array>\n#include <concepts>\n#include <cstddef>\n");
    if (has_comparisons) { out_stream.write("#include <compare>\n"); }
    if (has_views) {
      out_stream.write("#include <span>\n");
    }
    out_stream.write("#include <cstdint>\n#include <cmath>\n"
                     "#include <functional>\n"
                     "#include <map>\n"
                     "#include <memory>\n"
                     "#include <optional>\n"
                     "#include <stdexcept>\n"
                     "#include <string>\n"
                     "#include <string_view>\n"
                     "#include <tuple>\n"
                     "#include <type_traits>\n"
                     "#include <utility>\n"
                     "#include <vector>\n\n");
    if (expected_results) {
      out_stream.write("#include <expected>\n"
          "#if !defined(__cpp_lib_expected) || __cpp_lib_expected < 202202L\n"
          "#error \"Serializer expected callable results require C++23 std::expected\"\n"
          "#endif\n\n");
    }
    write_source_marker(out_stream);
    out_stream.write(behavior::helpers("cpp"));
    out_stream.write(behavior::helpers_predicate("cpp"));
    // File preambles historically apply to the entire compilation unit, including imports.
    // Hoist them ahead of generated includes so native dependency bodies keep that visibility.
    for (const auto& statement : statements) {
      if (statement->type != object_type::native_code) { continue; }
      const auto& code = static_cast<const native_code_node&>(*statement).code;
      if (code.language != "cpp") { continue; }
      const auto native_hook = "// clang-format off\n" + behavior::source_marker(code, "cpp") +
          behavior::line_directive(code, "cpp") + code.text + behavior::reset_line("cpp") +
          "\n// clang-format on\n";
      const auto identity = "preamble:" + code.source_path + ":" + std::to_string(code.source_offset);
      write_reusable_unit(out_stream, identity, native_hook);
    }
    write_included_headers(out_stream);
    if (contains_functions(statements)) {
      for (unsigned phase{}; phase < 3; ++phase) { write_function_inventory(out_stream, statements, phase); }
    }
    write_statement_list(out_stream, statements);
    std::set<std::string> optional_applications;
    write_optional_applications(out_stream, statements, optional_applications);
    managed_.write(out_stream);
  }
};
} // namespace

// Commit output to the caller only after name validation and formatting both succeed.
std::string generate(const std::vector<std::unique_ptr<syntax_node>>& statements,
                     const cpp_options& options) {
  xml_binding::validate_options(statements, options.xml);
  static_cast<void>(coding_standard_name(options.standard));
  static_cast<void>(cpp_protocols_name(options.protocols));
  full_stream_auto_alloc raw{};
  raw.write(generated_license_notice(statements));
  emitter generator{options};
  generator.emit(raw, statements);
  const std::string_view source{reinterpret_cast<const char*>(raw.begin()), raw.current_offset()};
  return behavior::finalize_mappings(format_cpp(source, options));
}

// Preserve file-level metadata that cannot be carried by an empty declaration vector.
std::string generate_schema(const parser::parsed_schema& schema, const cpp_options& options) {
  const auto source = generate(schema.statements, options);
  return schema.statements.empty()
      ? generated_source_notices(schema.source_notices) + source : source;
}

// Plan safe relative paths, then format every artifact before the caller can publish any file.
std::vector<generated_file> generate_files(const parser::parsed_schema& schema,
    const std::filesystem::path& destination, const cpp_options& options, include_mode mode) {
  if (mode != include_mode::separate && mode != include_mode::inline_output) {
    throw std::invalid_argument{"Invalid include mode"};
  }
  xml_binding::validate_options(schema.statements, options.xml);
  if (mode == include_mode::inline_output || schema.source_files.size() <= 1) {
    return {{destination, generate_schema(schema, options)}};
  }
  static_cast<void>(coding_standard_name(options.standard));
  static_cast<void>(cpp_protocols_name(options.protocols));
  auto source_root = schema.source_files.back().parent_path();
  for (const auto& file : schema.source_files) {
    auto relative = file.lexically_relative(source_root);
    while (relative.empty() || (!relative.empty() && *relative.begin() == "..")) {
      const auto parent = source_root.parent_path();
      if (parent == source_root) {
        throw std::invalid_argument{"Separate includes require a common source filesystem root"};
      }
      source_root = parent;
      relative = file.lexically_relative(source_root);
    }
  }
  const auto bundle = destination.parent_path() / (destination.filename().string() + ".includes");
  std::vector<generated_file> files{};
  for (std::size_t index{}; index < schema.source_files.size(); ++index) {
    const auto& source_file = schema.source_files[index];
    auto path = destination;
    if (index + 1 != schema.source_files.size()) {
      auto relative = source_file.lexically_relative(source_root);
      relative.replace_extension(destination.extension());
      path = bundle / relative;
    }
    std::vector<std::string> headers{};
    for (const auto& previous : files) {
      const auto header = previous.path.lexically_relative(path.parent_path()).generic_string();
      if (header.empty() || header.find_first_of("\"\r\n") != std::string::npos) {
        throw std::invalid_argument{"Generated include paths must not contain quotes or line breaks"};
      }
      headers.push_back(header);
    }
    const auto source_path = source_file.generic_string();
    if (source_path.find_first_of("\r\n") != std::string::npos) {
      throw std::invalid_argument{"Schema paths must not contain line breaks"};
    }
    full_stream_auto_alloc raw{};
    // Every artifact retains all compilation-unit notices, including declaration-free includes.
    if (schema.statements.empty()) { raw.write(generated_source_notices(schema.source_notices)); }
    raw.write(generated_license_notice(schema.statements));
    emitter generator{options};
    generator.select_source(source_path, std::move(headers));
    generator.emit(raw, schema.statements);
    const std::string_view text{reinterpret_cast<const char*>(raw.begin()), raw.current_offset()};
    files.push_back({std::move(path), behavior::finalize_mappings(format_cpp(text, options))});
  }
  return files;
}
} // namespace rohit::serializer::writer::cpp
