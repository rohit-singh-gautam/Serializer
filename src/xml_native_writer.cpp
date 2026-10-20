// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "xml_native_writer.hpp"
#include "xml_native_choice_writer.hpp"
#include "xml_mobile_map_writer.hpp"
#include "xml_mobile_variant_writer.hpp"
#include "xml_mobile_mixed_writer.hpp"
#include "xml_mobile_scalar_writer.hpp"
#include "xml_mobile_namespace.hpp"
#include "xml_mobile_identity_writer.hpp"
#include "xml_mobile_extension_writer.hpp"
#include "xml_portable_writer.hpp"
#include "xml_c_container_writer.hpp"
#include "xml_c_variant_writer.hpp"
#include "xml_c_mixed_writer.hpp"
#include "xml_native_model_writer.hpp"
#include "xml_native_scalar_writer.hpp"
#include "xml_native_identity_writer.hpp"
#include "xml_native_extension_writer.hpp"
#include "xml_native_extension_frames.hpp"
#include "translator_writer.hpp"
#include "value_reference_native_writer.hpp"
#include "xml_schema.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <map>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rohit::serializer::writer::xml_codegen {
namespace {
constexpr std::string_view kotlin_runtime =
#include "xml_kotlin_runtime.inc"
#include "xml_kotlin_output_runtime.inc"
#include "xml_kotlin_map_runtime.inc"
#include "xml_kotlin_variant_runtime.inc"
#include "xml_kotlin_namespace_runtime.inc"
#include "xml_kotlin_mixed_runtime.inc"
#include "xml_kotlin_html_runtime.inc"
#include "xml_kotlin_scalar_runtime.inc"
#include "xml_kotlin_model_runtime.inc"
#include "xml_kotlin_identity_runtime.inc"
#include "xml_kotlin_extension_runtime.inc"
#include "xml_kotlin_tokens_runtime.inc"
;
constexpr std::string_view swift_runtime =
#include "xml_swift_runtime.inc"
#include "xml_swift_output_runtime.inc"
#include "xml_swift_map_runtime.inc"
#include "xml_swift_variant_runtime.inc"
#include "xml_swift_namespace_runtime.inc"
#include "xml_swift_mixed_runtime.inc"
#include "xml_swift_html_runtime.inc"
#include "xml_swift_scalar_runtime.inc"
#include "xml_swift_model_runtime.inc"
#include "xml_swift_identity_runtime.inc"
#include "xml_swift_extension_runtime.inc"
#include "xml_swift_attribute_runtime.inc"
#include "xml_swift_tokens_runtime.inc"
#include "xml_swift_extension_tokens_runtime.inc"
;
constexpr std::string_view c_runtime =
#include "xml_c_map_runtime.inc"
#include "xml_c_mixed_runtime.inc"
#include "xml_c_model_runtime.inc"
#include "xml_c_runtime.inc"
#include "xml_c_operation_provider_runtime.inc"
#include "xml_c_tokens_runtime.inc"
#include "xml_c_variant_runtime.inc"
#include "xml_c_html_runtime.inc"
#include "xml_c_scalar_runtime.inc"
#include "xml_c_operation_scalar_runtime.inc"
#include "xml_c_identity_runtime.inc"
#include "xml_c_operation_reader_runtime.inc"
#include "xml_c_operation_identity_runtime.inc"
#include "xml_c_extension_runtime.inc"
;
// Emit reflection-free field bindings over native optional XML parsing providers.
class generator {
  const native::schema& model_;
  xml_binding::schema bindings_;
  const xml_options& options_;
  std::string language_;
  bool swift_;
  bool c_;
  const class_node* current_owner_{};
  std::map<const member*, std::string> scalar_rules_{};
  std::function<std::string(const member&)> mobile_projection_{};
  std::map<const class_node*, bool> mobile_extension_graphs_{};
  std::string output_{};
  std::size_t indent_{};
  // Append one generated line at the active indentation.
  void line(std::string_view value = {}) { output_.append(indent_ * 4U, ' '); output_ += value; output_ += '\n'; }
  // Enter a source scope.
  void open(const std::string& value) { line(value + " {"); ++indent_; }
  // Complete a source scope.
  void close(std::string_view suffix = {}) { --indent_; line("}" + std::string{suffix}); }
  // Select target syntax without changing schema metadata.
  std::string select(std::string_view swift, std::string_view kotlin) const { return std::string{swift_ ? swift : kotlin}; }
  // Derive the native presentation spelling used by existing generators.
  std::string field_name(std::string_view value) const {
    if (c_) { return native::snake(value); }
    auto result = native::pascal(value);
    if (!result.empty()) { result.front() = static_cast<char>(std::tolower(static_cast<unsigned char>(result.front()))); }
    return result;
  }
  // Locate one flattened field through the existing composed native base ownership graph.
  std::optional<std::string> member_path(const class_node& owner, const member& source) const {
    for (const auto& item : owner.member_list) {
      if (&item != &source) { continue; }
      if (!c_ && (!swift_ || (&owner != current_owner_ && item.access != access_type::public_access))) {
        return "srlXmlField" + native::pascal(item.name);
      }
      return field_name(item.name);
    }
    for (std::size_t index{}; index < owner.parents.size(); ++index) {
      const auto& parent = owner.parents[index];
      if (const auto nested = member_path(*parent.parent_class, source)) {
        const bool bridge = !c_ && parent.access != access_type::public_access && (!swift_ || &owner != current_owner_);
        return std::string{bridge ? "srlXmlBase" : "base"} + std::to_string(index) + '.' + *nested;
      }
    }
    return {};
  }
  // Resolve by member identity so private fields and renamed flattened XML bindings stay aligned.
  std::string member_name(const member& source) const {
    if (mobile_projection_) { return mobile_projection_(source); }
    const auto result = member_path(*current_owner_, source);
    if (!result) { throw std::logic_error{"XML field does not belong to its owning native graph"}; }
    return *result;
  }
  // Retain namespace scope history only when this complete owning input graph can preserve foreign XML.
  bool mobile_extension_graph(const class_node& source) {
    if (const auto found = mobile_extension_graphs_.find(&source); found != mobile_extension_graphs_.end()) { return found->second; }
    std::set<const class_node*> visited{};
    std::function<bool(const class_node&)> inspect;
    std::function<bool(const type_name&)> inspect_type;
    inspect = [&](const class_node& owner) -> bool {
      if (owner.reference || !visited.insert(&owner).second) { return false; }
      const auto contract = bindings_.bind(owner);
      if (contract.extensions || mobile_namespace_value(contract)) { return true; }
      return std::any_of(contract.fields.begin(), contract.fields.end(), [&](const auto& field) {
        return std::any_of(field.source->type_name_list.begin(), field.source->type_name_list.end(), inspect_type) ||
            (field.source->key_type && inspect_type(*field.source->key_type));
      });
    };
    inspect_type = [&](const type_name& value) {
      return (value.type == object_type::class_type && value.resolved_node &&
          inspect(static_cast<const class_node&>(*value.resolved_node))) ||
          std::any_of(value.arguments.begin(), value.arguments.end(), inspect_type);
    };
    const auto result = inspect(source);
    mobile_extension_graphs_.emplace(&source, result); return result;
  }
  // Keep fixed-array helper names tied to their declaring C record rather than a derived wrapper.
  std::string array_name(const member& source) const {
    const auto declaring = [&](auto&& self, const class_node& owner) -> const class_node* {
      for (const auto& item : owner.member_list) { if (&item == &source) { return &owner; } }
      for (const auto& parent : owner.parents) { if (const auto* found = self(self, *parent.parent_class)) { return found; } }
      return nullptr;
    };
    const auto* owner = declaring(declaring, *current_owner_);
    if (!owner) { throw std::logic_error{"XML array does not belong to its owning native graph"}; }
    return native::snake(model_.names.at(owner)) + '_' + field_name(source.name) + "_array";
  }
  // Preserve configured UTF-8 source literals and Kotlin interpolation characters.
  std::string quote(std::string_view value) const {
    auto result = native::quote(value);
    if (!swift_ && !c_) {
      std::size_t offset{};
      while ((offset = result.find('$', offset)) != std::string::npos) { result.insert(offset, "\\"); offset += 2U; }
    }
    return result;
  }
  // Return the scalar/record type under the supported presence/collection mapping.
  const type_name& leaf(const member& value) const {
    const auto* type = &value.type_name_list.front();
    while (type->is_optional()) { type = &type->arguments.front(); }
    if (type->is_sequence()) { type = &type->arguments.front(); }
    if (!c_ && type->type == object_type::class_type && type->resolved_node) {
      const auto& owner = static_cast<const class_node&>(*type->resolved_node);
      if (owner.reference && owner.reference->value_projection) { return owner.reference->value_type; }
    }
    return *type;
  }
  // Resolve exact native model and scalar type names.
  std::string type(const type_name& value) const {
    if (value.type != object_type::primitive) { return c_ ? native::snake(model_.names.at(value.resolved_node)) : model_.names.at(value.resolved_node); }
    if (value.is_digest()) { return c_ ? "srl_buffer" : select("[UInt8]", "UByteArray"); }
    if (value.name == "string" || value.name.starts_with("version")) { return c_ ? "srl_string" : "String"; }
    if (value.name == "bool") { return c_ ? "bool" : select("Bool", "Boolean"); }
    if (value.name == "float") { return c_ ? "float" : "Float"; }
    if (value.name == "double") { return c_ ? "double" : "Double"; }
    if (c_) { return value.name + "_t"; }
    if (swift_) { return std::string{value.name.starts_with('u') ? "UInt" : "Int"} + std::to_string(native::width(value) * 8); }
    return std::string{value.name.starts_with('u') ? "U" : ""} +
        (native::width(value) == 1 ? "Byte" : native::width(value) == 2 ? "Short" : native::width(value) == 4 ? "Int" : "Long");
  }
  // Emit a checked expanded-name literal for native XML trees.
  std::string name(const xml::name& value) const {
    return "SrlXmlName(" + select("local: ", "") + quote(value.local) + ", " + select("namespace: ", "") + quote(value.namespace_uri) + ")";
  }
  // Format scalar lexical text without escaping until the XML writer owns it.
  std::string scalar_text(const type_name& value, const std::string& expression) const {
    if (value.is_digest()) {
      return select("try ", "") + "srlXmlDigestText(" + expression + ", " + std::to_string(value.digest_extent) + ", output.digestLimits" + (swift_ ? ")" : ",output.packageContext())");
    }
    if (value.name.starts_with("version")) {
      return select("try ", "") + "srlXmlRevisionLexical(" + expression + ", " + quote(value.name) + ")";
    }
    if (value.name == "string") { return expression; }
    if (value.type == object_type::enum_type) {
      return type(value) + ".srlNames[" + expression + select(".rawValue]", ".ordinal]");
    }
    if (value.name == "bool") { return swift_ ? "(" + expression + " ? \"true\" : \"false\")" : expression + ".toString()"; }
    if (value.name == "float" || value.name == "double") {
      return "srlXmlFloatText(" + expression + ")";
    }
    return swift_ ? "String(" + expression + ")" : expression + ".toString()";
  }
  // Decode one supported lexical scalar or nested record using exact range checks.
  std::string read_value(const type_name& value, const std::string& raw, bool attribute) const {
    if (value.type == object_type::class_type) { return select("try ", "") + type(value) + ".srlXmlRead(" + raw + ")"; }
    const auto text = attribute ? raw : select("try ", "") + "srlXmlText(" + raw + ")";
    if (value.is_digest()) {
      return select("try ", "") + "srlXmlDigestRead(" + text + ", " + std::to_string(value.digest_extent) +
          ", " + (swift_ ? "digestLimits" : "input.digestLimits,input.packageContext()") + ")";
    }
    if (value.name.starts_with("version")) {
      return select("try ", "") + "srlXmlRevisionLexical(" + text + ", " + quote(value.name) + ")";
    }
    if (value.name == "string") { return text; }
    if (value.type == object_type::enum_type) { return select("try ", "") + "srlXml" + type(value) + "Enum(" + text + ")"; }
    if (value.name == "bool") { return select("try ", "") + "srlXmlBoolean(" + text + ")"; }
    if (value.name == "float" || value.name == "double") {
      return select("try ", "") + (value.name == "float" ? "srlXmlFloat32(" : "srlXmlFloat(") + text + ")";
    }
    return swift_ ? "try srlXmlInteger(" + text + ", " + type(value) + ".self)" :
        "srlXmlIntegerText(" + text + ").to" + type(value) + "()";
  }
  // Apply the immutable field rule before native conversion or escaped token publication.
  std::string field_scalar(const xml_binding::field& field, std::string expression, bool encoding,
      std::string namespace_scope = {}) const {
    const auto found = scalar_rules_.find(field.source);
    const auto& fallback = field.fixed_value ? field.fixed_value : field.default_value;
    if (!encoding && fallback) {
      expression = "SrlXmlRule.defaultText(" + expression + ',' + quote(*fallback) + ',' + (field.attribute ? "true" : "false") + ')';
    }
    const auto limit = encoding ? "output.listLimit" : swift_ ? "listLimit" : "input.listLimit";
    const auto quota = swift_ ? std::string{encoding ? ",output.scalarQuota,output.scalarOperation" : ",extensionOptions.srlPackageLimit,extensionOptions.srlPackageConsume"} :
        std::string{encoding ? ",output.packageContext()" : ",input.packageContext()"};
    if (found != scalar_rules_.end()) {
      expression = select("try ", "") + found->second + (encoding && fallback && !field.attribute ?
          ".output(" + expression + ',' + quote(*fallback) + ',' + limit + quota + ')' : ".check(" + expression + ',' + limit + quota + ')');
    }
    if (mobile_namespace_value(field)) {
      if (namespace_scope.empty()) {
        namespace_scope = encoding ? select("output.namespace", "output::namespace") :
            select("state.value.srlXmlNamespaces.owner", "namespaces.owner");
      }
      expression = select("try ", "") + "srlXmlNamespaceChecked(" + expression + ',' +
          (field.facets.list_item_kind == "QName" || field.list ? "true" : "false") + ',' +
          (field.prefix_list ? "true" : "false") + ',' + namespace_scope + ',' + limit + quota + ')';
    }
    return expression;
  }
  // Decode lexical lists into native owning arrays while retaining optional and fixed extents.
  std::string field_read(const xml_binding::field& field, const std::string& raw,
      const std::string& namespace_scope = {}) const {
    const auto checked = field_scalar(field, raw, false, namespace_scope);
    if (!field.list || !field.presence.collection) { return read_value(leaf(*field.source), checked, true); }
    const auto item = read_value(leaf(*field.source), "item", true);
    if (swift_) {
      return "try srlXmlReadList(" + checked + ", " + std::to_string(field.source->fixed_extent) + ", listLimit,extensionOptions.srlPackageLimit) { item in " + item + " }";
    }
    auto result = "srlXmlReadList(" + checked + ", " + std::to_string(field.source->fixed_extent) + ", input.listLimit,input.packageContext()) { item -> " + item + " }";
    const auto& value = leaf(*field.source);
    if (field.source->modifier == member::modifier_type::array && value.type == object_type::primitive && value.name != "string") {
      result += ".to" + type(value) + "Array()";
    }
    return result;
  }
  // Preserve Swift's byte-exact native string-map keys while using ordinary scalar values elsewhere.
  std::string map_type(const type_name& value, bool key) const {
    return swift_ && key && value.name == "string" ? "WireString" : type(value);
  }
  // Convert map values through the same checked scalar adapters as ordinary generated fields.
  std::string map_scalar(const type_name& value, const std::string& expression, bool encoding, bool key) const {
    if (swift_ && key && value.name == "string") {
      return encoding ? expression + ".value" : "WireString(" + expression + ")";
    }
    return encoding ? scalar_text(value, expression) : read_value(value, expression, true);
  }
  // Dispatch a native map's output or direct input without adding an intermediate XML model.
  std::string map_source(const xml_binding::field& field, const std::string& target, bool encoding) const {
    const auto native_type = [&](const type_name& value, bool key) { return map_type(value, key); };
    const auto native_scalar = [&](const type_name& value, const std::string& expression, bool output, bool key) {
      return map_scalar(value, expression, output, key);
    };
    const auto expanded = [&](const xml::name& value) { return name(value); };
    if (encoding) { return mobile_map_write(*field.map_binding, target, native_scalar, expanded, swift_); }
    return swift_ ? swift_map_frame(*field.map_binding, target, native_type, native_scalar, expanded) :
        kotlin_map_read(*field.map_binding, target, native_type, native_scalar, expanded);
  }
  // Preserve the native selector and per-alternative fields behind XML-only private bridges.
  std::string variant_source(const xml_binding::field& field, const std::string& target, bool encoding) const {
    const auto native_type = [&](const type_name& value) { return type(value); };
    const auto native_scalar = [&](const type_name& value, const std::string& expression, bool output) {
      return output ? scalar_text(value, expression) : read_value(value, expression, true);
    };
    const auto expanded = [&](const xml::name& value) { return name(value); };
    if (encoding) { return mobile_variant_write(*field.variant_binding, target, native_scalar, expanded, swift_); }
    return swift_ ? swift_variant_frame(*field.variant_binding, target, native_type, native_scalar, expanded) :
        kotlin_variant_read(*field.variant_binding, target, native_type, native_scalar, expanded);
  }
  // Traverse the native ordered fragment sequence through its declaring-owner XML bridges.
  std::string mixed_source(const xml_binding::mixed_contract& mixed, const std::string& target, bool encoding) const {
    const auto variant = member_path(*mixed.fragment, *mixed.variant);
    if (!variant) { throw std::logic_error{"XML mixed variant is absent from its native fragment"}; }
    const auto expanded = [&](const xml::name& value) { return name(value); };
    const auto native_type = [&](const type_name& value) { return type(value); };
    if (encoding) { return mobile_mixed_write(mixed, target, *variant, expanded, swift_); }
    return swift_ ? swift_mixed_frame(mixed, target, type(leaf(*mixed.source)), *variant, expanded, native_type) :
        kotlin_mixed_read(mixed, target, type(leaf(*mixed.source)), *variant, expanded, native_type);
  }
  // Keep lifecycle comparisons on the exact existing native discriminator type.
  version_language mobile_version_language() const { return swift_ ? version_language::swift : version_language::kotlin; }
  // Resolve private inherited discriminator storage through the same native ownership paths.
  std::string mobile_revision_path(const xml_binding::record& contract, const std::string& owner) const {
    return owner + member_name(*contract.model.revision);
  }
  // Scope one generated field to its historical interval without changing native storage.
  model_emission_scope mobile_active_scope(const xml_binding::record& contract, const member& field,
      const std::string& owner) {
    const auto condition = contract.model.revision ? native_xml_active(contract, field,
        mobile_version_language(), mobile_revision_path(contract, owner)) : std::string{};
    if (!condition.empty()) { open(swift_ ? "if " + condition : "if (" + condition + ")"); }
    return model_emission_scope{condition.empty() ? std::function<void()>{} : std::function<void()>{[&] { close(); }}};
  }
  // Require model metadata before traversing ordinary fields or selecting an accepted layout.
  void mobile_model_input(const xml_binding::record& contract, const std::string& owner) {
    if (const auto* revision = contract.model.revision) {
      const auto& value = revision->type_name_list.front();
      const auto selected = name({"revision", std::string{xml_binding::model_metadata_namespace}});
      line(select("let ", "val ") + "revisionText = " + select("try ", "") + "srlXmlRevisionLexical(" +
          (swift_ ? "try fields.take(" + selected + ")!" : "fields.attribute(" + selected + ", false)!!") + ',' + quote(value.name) + ')');
      line(mobile_revision_path(contract, owner) + " = " + read_value(value, "revisionText", true));
      const auto invalid = native_xml_revision_invalid(*revision, mobile_version_language(), mobile_revision_path(contract, owner));
      line(swift_ ? "guard !(" + invalid + ") else { throw SerializerError.invalid(\"Unsupported XML revision\") }" :
          "require(!(" + invalid + ")) { \"Unsupported XML revision\" }");
    }
    if (contract.model.magic) {
      const auto selected = name({"magic", std::string{xml_binding::model_metadata_namespace}});
      const auto expression = swift_ ? "try fields.take(" + selected + ")" : "fields.attribute(" + selected + ", false)";
      line(swift_ ? "guard " + expression + " == " + quote(*contract.model.magic) + " else { throw SerializerError.invalid(\"XML magic mismatch\") }" :
          "require(" + expression + " == " + quote(*contract.model.magic) + ") { \"XML magic mismatch\" }");
    }
  }
  // Publish exact expanded revision/magic attributes without mutating the owning model.
  void mobile_model_output(const xml_binding::record& contract) {
    if (const auto* revision = contract.model.revision) {
      const auto value = mobile_revision_path(contract, swift_ ? "self." : "this.");
      const auto invalid = native_xml_revision_invalid(*revision, mobile_version_language(), value);
      line(swift_ ? "guard !(" + invalid + ") else { throw SerializerError.invalid(\"Unsupported XML revision\") }" :
          "require(!(" + invalid + ")) { \"Unsupported XML revision\" }");
      const auto lexical = scalar_text(revision->type_name_list.front(), value);
      line(select("let ", "val ") + "revisionText = " + select("try ", "") + "srlXmlRevisionLexical(" + lexical + ',' + quote(revision->type_name_list.front().name) + ')');
      const auto selected = name({"revision", std::string{xml_binding::model_metadata_namespace}});
      line(select("try ", "") + "output.attribute(SrlXmlAttribute(" + select("name: ", "") + selected + ", " + select("value: ", "") + "revisionText))");
    }
    if (contract.model.magic) {
      const auto selected = name({"magic", std::string{xml_binding::model_metadata_namespace}});
      line(select("try ", "") + "output.attribute(SrlXmlAttribute(" + select("name: ", "") + selected + ", " + select("value: ", "") + quote(*contract.model.magic) + "))");
    }
  }
  // Emit one statically selected XML scalar, nested record, or attribute token.
  void emit_mobile_value(const xml_binding::field& field, const std::string& expression) {
    const auto& value_type = leaf(*field.source);
    const auto selected = field.value_projection && value_type.type != object_type::class_type
        ? (swift_ ? "(try " + expression + ".readValue())" : expression + ".readValue()") : expression;
    const auto lexical = field.list && field.presence.collection ? select("try ", "") +
        "SrlXmlRule.listText(" + expression + select(".map { item in ", ".map { item -> ") +
        scalar_text(value_type, "item") + " }, " + std::to_string(field.source->fixed_extent) + ", output.listLimit" + (swift_ ? ",output.scalarQuota)" : ",output.packageContext())") : scalar_text(value_type, selected);
    const auto text = field_scalar(field, lexical, true);
    if (field.text) {
      line(select("try ", "") + "output.text(" + text + ")");
    } else if (field.attribute) {
      line(swift_ ? "try output.attribute(SrlXmlAttribute(name: " + name(field.qualified_name) + ", value: " + text + "))" :
          "output.attribute(SrlXmlAttribute(" + name(field.qualified_name) + ", " + text + "))");
    } else if (value_type.type == object_type::class_type) {
      line(select("try ", "") + expression + ".srlXmlEmit(output, " + name(field.qualified_name) + ")");
    } else {
      const auto scope = mobile_namespace_value(field) ? ", namespaces.scope(" +
          quote(*member_path(*current_owner_, *field.source)) + ')' : std::string{};
      line(select("try ", "") + "output.open(" + name(field.qualified_name) + scope + ")");
      line(select("try ", "") + "output.text(" + text + ")");
      line(select("try ", "") + "output.close()");
    }
  }
  // Decode a Kotlin scalar from the active token or recurse into a concrete nested record.
  std::string kotlin_token_value(const xml_binding::field& field) const {
    const auto& value_type = leaf(*field.source);
    if (value_type.type == object_type::class_type) {
      return type(value_type) + ".srlXmlRead(input, " + name(field.qualified_name) + ")";
    }
    if (mobile_namespace_value(field)) {
      return "namespaces.scalar(input, " + name(field.qualified_name) + ',' +
          quote(*member_path(*current_owner_, *field.source)) + ") { text, scope -> " +
          field_read(field, "text", "scope") + " }";
    }
    return field_read(field, "input.scalar(" + name(field.qualified_name) + ")");
  }
  // Select explicit state traversal and optional wrapped collections without changing native storage.
  static bool requires_presence(const xml_binding::field& field) {
    return field.presence.explicit_policy ||
        (!field.list && field.presence.collection && field.presence.optional_depth != 0U);
  }
  // Replay foreign roots after one directly repeated known child, without counting wrapper contents.
  void mobile_extension_repeat(const std::string& position) {
    if (position.empty()) { return; }
    line("extensionItems" + position + " += 1");
    line(select("try ", "") + "extensions.emitAt(output, " + position + ", extensionItems" + position + ")");
  }
  // Emit the actual nonnil scalar, record or wrapped sequence after optional layers are checked.
  void emit_kotlin_present(const xml_binding::field& field, const std::string& value, const std::string& extension_position = {}) {
    if (field.wrapper) { line("output.open(" + name(*field.wrapper) + ")"); }
    if (field.presence.collection) {
      if (field.min_occurs > 0U) {
        line("require(" + value + ".size >= " + std::to_string(field.min_occurs) + ") { \"XSD occurrence bound\" }");
      }
      if (field.max_occurs) {
        line("require(" + value + ".size <= " + std::to_string(*field.max_occurs) + ") { \"XSD occurrence bound\" }");
      }
      if (field.source->fixed_extent != 0U) {
        line("require(" + value + ".size == " + std::to_string(field.source->fixed_extent) + ") { \"XML fixed array size\" }");
      }
      open("for (item in " + value + ")");
      emit_mobile_value(field, "item");
      mobile_extension_repeat(extension_position);
      close();
    } else { emit_mobile_value(field, value); }
    if (field.wrapper) { line("output.close()"); }
  }
  // Preserve absent/nil/value distinctions instead of inferring state from an empty payload.
  void emit_kotlin_presence(const xml_binding::field& field, const std::string& value, const std::string& extension_position = {}) {
    const auto& presence = field.presence;
    open("run");
    auto payload = value;
    if (presence.allows_absent) {
      open("if (" + payload + ".isPresent)");
      payload += ".get()";
    }
    if (presence.allows_nil) {
      open("if (!" + payload + ".isPresent)");
      line("output.open(" + name(field.wrapper ? *field.wrapper : field.qualified_name) + ")");
      line("output.attribute(SrlXmlAttribute(srlXmlNilName, \"true\"))");
      line("output.close()");
      close(" else {");
      ++indent_;
      emit_kotlin_present(field, payload + ".get()", extension_position);
      --indent_;
      line("}");
    } else { emit_kotlin_present(field, payload, extension_position); }
    if (presence.allows_absent) { close(); }
    close();
  }
  // Emit a fully unwrapped Swift scalar, record or collection with exact occurrence bounds.
  void emit_swift_present(const xml_binding::field& field, const std::string& value, const std::string& extension_position = {}) {
    if (field.wrapper) { line("try output.open(" + name(*field.wrapper) + ")"); }
    if (field.presence.collection) {
      if (field.min_occurs > 0U) {
        line("guard " + value + ".count >= " + std::to_string(field.min_occurs) + " else { throw SerializerError.invalid(\"XSD occurrence bound\") }");
      }
      if (field.max_occurs) {
        line("guard " + value + ".count <= " + std::to_string(*field.max_occurs) + " else { throw SerializerError.invalid(\"XSD occurrence bound\") }");
      }
      if (field.source->fixed_extent != 0U) {
        line("guard " + value + ".count == " + std::to_string(field.source->fixed_extent) + " else { throw SerializerError.invalid(\"XML fixed array size\") }");
      }
      open("for item in " + value); emit_mobile_value(field, "item"); mobile_extension_repeat(extension_position); close();
    } else { emit_mobile_value(field, value); }
    if (field.wrapper) { line("try output.close()"); }
  }
  // Preserve all native Optional layers when choosing absent, nil and present Swift output.
  void emit_swift_presence(const xml_binding::field& field, const std::string& value, const std::string& extension_position = {}) {
    const auto& presence = field.presence;
    open("do");
    auto payload = value;
    if (presence.allows_absent) {
      open("if let present = " + payload);
      payload = "present";
    }
    if (presence.allows_nil) {
      open("if let item = " + payload);
      emit_swift_present(field, "item", extension_position);
      close(" else {"); ++indent_;
      line("try output.open(" + name(field.wrapper ? *field.wrapper : field.qualified_name) + ")");
      line("try output.attribute(SrlXmlAttribute(name: SrlXmlName(local: \"nil\", namespace: \"http://www.w3.org/2001/XMLSchema-instance\"), value: \"true\"))");
      line("try output.close()"); close();
    } else { emit_swift_present(field, payload, extension_position); }
    if (presence.allows_absent) { close(); }
    close();
  }
  // Decode one presence-controlled field transactionally, retaining all native optional layers.
  void read_kotlin_presence(const xml_binding::field& field, const std::string& target, const std::string& extension_position = {}) {
    const auto& presence = field.presence;
    open("run");
    open("when (input.presence(" + name(field.wrapper ? *field.wrapper : field.qualified_name) + ", " +
        (presence.allows_absent ? "true" : "false") + ", " + (presence.allows_nil ? "true" : "false") + "))");
    if (presence.allows_absent) { line("'a' -> " + target + " = java.util.Optional.empty()"); }
    if (presence.allows_nil) {
      line("'n' -> " + target + " = " + (presence.optional_depth == 2U ?
          "java.util.Optional.of(java.util.Optional.empty())" : "java.util.Optional.empty()"));
    }
    open("'v' ->");
    if (field.wrapper) { line("val wrapper = input.open(" + name(*field.wrapper) + ")"); }
    std::string payload{"payload"};
    if (presence.collection) {
      const auto& value_type = leaf(*field.source);
      line("val payload = mutableListOf<" + type(value_type) + ">()");
      open("while (input.has(" + name(field.qualified_name) + "))");
      if (field.max_occurs) {
        line("require(payload.size < " + std::to_string(*field.max_occurs) + ") { \"XSD occurrence bound\" }");
      }
      if (field.source->fixed_extent != 0U) {
        line("require(payload.size < " + std::to_string(field.source->fixed_extent) + ") { \"XML fixed array size\" }");
      }
      line("payload.add(" + kotlin_token_value(field) + ")");
      if (!extension_position.empty()) { line("extensions.readAt(input, " + extension_position + ", payload.size)"); }
      close();
      if (field.min_occurs > 0U) {
        line("require(payload.size >= " + std::to_string(field.min_occurs) + ") { \"XSD occurrence bound\" }");
      }
      if (field.source->fixed_extent != 0U) {
        line("require(payload.size == " + std::to_string(field.source->fixed_extent) + ") { \"XML fixed array size\" }");
      }
      if (field.source->modifier == member::modifier_type::array && value_type.type == object_type::primitive &&
          value_type.name != "string") {
        payload += ".to" + type(value_type) + "Array()";
      }
    } else { line("val payload = " + kotlin_token_value(field)); }
    if (field.wrapper) { line("wrapper.finish()"); }
    for (std::size_t depth{}; depth < presence.optional_depth; ++depth) {
      payload = "java.util.Optional.of(" + payload + ")";
    }
    line(target + " = " + payload);
    close();
    line("else -> throw IllegalArgumentException(\"Invalid XML presence state\")");
    close();
    close();
  }
  // Dispatch a complete singleton all group using only a fixed set of seen flags.
  void kotlin_token_all(const xml_binding::record& contract) {
    if (std::none_of(contract.fields.begin(), contract.fields.end(),
            [](const auto& field) { return !field.attribute; })) {
      return;
    }
    line("val seen = BooleanArray(" + std::to_string(contract.fields.size()) + ")");
    const bool optional_group = contract.all_groups.front().minimum == 0U;
    if (optional_group) { line("var allCount = 0"); }
    line("input.skipSpace()");
    open("while (input.kind == 's')");
    open("when (input.name())");
    for (std::size_t index{}; index < contract.fields.size(); ++index) {
      const auto& field = contract.fields[index];
      if (field.attribute) { continue; }
      open(name(field.qualified_name) + " ->");
      line("input.packageField(" + quote(field.source->name) + ")");
      const auto position = "seen[" + std::to_string(index) + "]";
      line("require(!" + position + ") { \"Duplicate XML all child\" }");
      line(position + " = true");
      const auto target = "result." + member_name(*field.source);
      const bool optional = field.source->type_name_list.front().is_optional();
      if (requires_presence(field)) { read_kotlin_presence(field, target); }
      else {
        line(target + " = " + (optional ? "java.util.Optional.of(" + kotlin_token_value(field) + ")" :
            kotlin_token_value(field)));
      }
      close();
    }
    line("else -> throw IllegalArgumentException(\"Unexpected XML all child\")");
    close();
    if (optional_group) { line("++allCount"); }
    line("input.skipSpace()");
    close();
    if (optional_group) { open("if (allCount != 0)"); }
    for (std::size_t index{}; index < contract.fields.size(); ++index) {
      const auto& field = contract.fields[index];
      if (!field.attribute && field.min_occurs != 0U) {
        line("require(seen[" + std::to_string(index) + "]) { \"Required XML all child missing\" }");
      }
    }
    if (optional_group) { close(); }
  }
  // Visit Kotlin fields directly while retaining only active attributes and scalar text.
  void kotlin_token_record(const class_node& source, const xml_binding::record& contract) {
    const auto model_name = model_.names.at(&source);
    line("// Read direct provider tokens into an unpublished owning candidate.");
    if (native_xml_export_graph(source)) { line("@Suppress(\"UNUSED_PARAMETER\")"); }
    open("internal fun " + model_name + ".Companion.srlXmlRead(input: SrlXmlInput, name: SrlXmlName): " + model_name);
    if (native_xml_export_graph(source)) {
      line("throw IllegalArgumentException(\"Export-only XML reference graph: decode into the owning target\")");
      close(); return;
    }
    std::string package_children{"if (input.packageActive) mapOf("};
    std::string package_attributes{"if (input.packageActive) mapOf("};
    for (const auto& field : contract.fields) {
      auto& selected = field.attribute ? package_attributes : package_children;
      selected += name(field.wrapper ? *field.wrapper : field.qualified_name) + " to " + quote(field.source->name) + ',';
    }
    package_children += ") else emptyMap()";
    package_attributes += ") else emptyMap()";
    line("val srlPackageScope = input.packageRecord(" + package_children + "," + package_attributes + ")");
    open("try");
    if (contract.identity) {
      line("input.observe(" + mobile_identity_metadata(*contract.identity, [&](auto value) { return quote(value); },
          [&](const auto& value) { return name(value); }, false) + ')');
    }
    if (contract.extensions) {
      line("val extensions = SrlXmlExtensions.input(srlXmlExtensions" + model_name + ", input.namespaceScope(), input.extensionOptions, input.packageContext())");
    }
    if (mobile_namespace_value(contract)) { line("val namespaceScope = input.namespaceValueScope()"); }
    line("val fields = input.open(name)");
    line("val result = " + model_name + "()");
    if (mobile_namespace_value(contract)) {
      line("val namespaces = result.srlXmlNamespaces");
      line("namespaces.owner = namespaceScope");
    }
    mobile_model_input(contract, "result.");
    for (const bool attributes : {true, false}) {
      if (!attributes && contract.extensions) { line("fields.extensions(extensions)"); }
      if (!attributes && !contract.all_groups.empty()) {
        kotlin_token_all(contract);
        continue;
      }
      for (const auto& field : contract.fields) {
        if (field.attribute != attributes) { continue; }
        line("input.packageField(" + quote(field.source->name) + ")");
        open("try");
        const model_emission_scope package_field_end{[&] {
          close(" catch (failure: IllegalArgumentException) {"); ++indent_;
          line("input.packageFailure(); throw failure"); close();
        }};
        const auto position = std::to_string(&field - contract.fields.data());
        const auto boundary = contract.extensions && !field.attribute && field.repeated && !field.wrapper && !field.list ? position : std::string{};
        if (contract.extensions && !field.attribute && !field.text) { line("extensions.readAt(input, " + position + ", 0)"); }
        const auto active = mobile_active_scope(contract, *field.source, "result.");
        const auto target = "result." + member_name(*field.source);
        const auto& value_type = leaf(*field.source);
        const bool optional = field.source->type_name_list.front().is_optional();
        if (field.variant_binding) { line(variant_source(field, target, false)); }
        else if (field.map_binding) { line(map_source(field, target, false)); }
        else if (!field.attribute && requires_presence(field)) {
          read_kotlin_presence(field, target, boundary);
        } else if (field.attribute) {
          const bool defaulted = field.default_value || field.fixed_value;
          const auto raw = "fields.attribute(" + name(field.qualified_name) + ", " + (optional || defaulted ? "true" : "false") + ")";
          line(target + " = " + (optional && !defaulted ? raw + "?.let { java.util.Optional.of(" +
              field_read(field, "it") + ") } ?: java.util.Optional.empty()" :
              (optional ? "java.util.Optional.of(" : "") + field_read(field, raw + (defaulted ? "" : "!!")) + (optional ? ")" : "")));
        } else if (field.text) {
          line(target + " = " + field_read(field, "fields.text()"));
        } else if (field.repeated) {
          open("run");
          if (field.wrapper) { line("val wrapper = input.open(" + name(*field.wrapper) + ")"); }
          line("val items = mutableListOf<" + type(value_type) + ">()");
          open("while (input.has(" + name(field.qualified_name) + "))");
          if (field.max_occurs) {
            line("require(items.size < " + std::to_string(*field.max_occurs) + ") { \"XSD occurrence bound\" }");
          }
          if (field.source->fixed_extent != 0U) {
            line("require(items.size < " + std::to_string(field.source->fixed_extent) + ") { \"XML fixed array size\" }");
          }
          line("items.add(" + kotlin_token_value(field) + ")");
          if (!boundary.empty()) { line("extensions.readAt(input, " + boundary + ", items.size)"); }
          close();
          if (field.wrapper) { line("wrapper.finish()"); }
          if (field.min_occurs > 0U) {
            line("require(items.size >= " + std::to_string(field.min_occurs) + ") { \"XSD occurrence bound\" }");
          }
          if (field.source->fixed_extent != 0U) {
            line("require(items.size == " + std::to_string(field.source->fixed_extent) + ") { \"XML fixed array size\" }");
          }
          const bool primitive_array = field.source->modifier == member::modifier_type::array &&
              value_type.type == object_type::primitive && value_type.name != "string" &&
              !value_type.is_digest() && !value_type.name.starts_with("version");
          line(target + " = items" + (primitive_array ? ".to" + type(value_type) + "Array()" : ""));
          close();
        } else if (optional) {
          line(target + " = if (input.has(" + name(field.qualified_name) + ")) java.util.Optional.of(" +
              kotlin_token_value(field) + ") else java.util.Optional.empty()");
        } else {
          line(target + " = " + kotlin_token_value(field));
        }
      }
    }
    if (contract.mixed) { line(mixed_source(*contract.mixed, "result." + member_name(*contract.mixed->source), false)); }
    line(native_choice_checks(contract, [&](const member& field) { return "result." + member_name(field); }, "kotlin"));
    if (contract.extensions) {
      line("extensions.readAt(input, " + std::to_string(contract.fields.size()) + ", 0)");
      line("extensions.finish()"); line("result.srlXmlExtensions = extensions.state");
    }
    line("fields.finish()");
    line("return result");
    close(" finally {"); ++indent_; line("srlPackageScope?.close()"); close();
    close();
  }
  // Return one Swift typed child frame whose completed value updates only its private owner.
  void swift_token_value(const xml_binding::field& field, const std::string& target, bool append,
      std::string_view child = "childName", std::string_view attributes = "childAttributes") {
    const auto& value_type = leaf(*field.source);
    if (value_type.type == object_type::class_type) {
      line("return try " + type(value_type) + ".srlXmlFrame(" + std::string{child} + ", " +
          std::string{attributes} + ", mapLimits, mixedLimits, digestLimits, identityLimits, extensionOptions, listLimit) { item in");
    } else {
      line("return try " + std::string{mobile_namespace_value(field) ? "srlXmlNamespaceScalarFrame" : "srlXmlScalarFrame"} +
          "(" + std::string{child} + ", " + std::string{attributes} + ") { " +
          (mobile_namespace_value(field) ? "text, scope in" : "text in"));
    }
    ++indent_;
    if (value_type.type != object_type::class_type) {
      line("let item = " + field_read(field, "text", mobile_namespace_value(field) ? "scope" : ""));
      if (mobile_namespace_value(field)) {
        line("state.value.srlXmlNamespaces.scalar(" + quote(*member_path(*current_owner_, *field.source)) + ", scope)");
      }
    }
    auto item = std::string{"item"};
    if (!append) {
      for (std::size_t depth{}; depth < field.presence.optional_depth; ++depth) {
        item = "Optional.some(" + item + ")";
      }
    }
    line(target + (append ? ".append(item)" : " = " + item));
    close();
  }
  // Decode one collection wrapper while retaining destination items and its scalar child state.
  void swift_token_wrapper(const xml_binding::field& field, const std::string& target,
      std::string_view attributes = "childAttributes") {
    line("guard " + std::string{attributes} + ".isEmpty else { throw SerializerError.invalid(\"Unexpected XML wrapper attribute\") }");
    line("let wrapper = SrlXmlTokenFrame(childName)");
    line("var items: [" + type(leaf(*field.source)) + "] = []");
    line("wrapper.child = { itemName, itemAttributes in");
    ++indent_;
    line("guard itemName == " + name(field.qualified_name) + " else { throw SerializerError.invalid(\"Unexpected XML wrapper child\") }");
    const auto maximum = field.source->fixed_extent != 0U ?
        std::optional<std::size_t>{field.source->fixed_extent} : field.max_occurs;
    if (maximum) {
      line("guard items.count < " + std::to_string(*maximum) + " else { throw SerializerError.invalid(\"XML collection occurrence bound\") }");
    }
    swift_token_value(field, "items", true, "itemName", "itemAttributes");
    close();
    line("wrapper.finish = {");
    ++indent_;
    const auto minimum = field.source->fixed_extent != 0U ? field.source->fixed_extent : field.min_occurs;
    if (minimum > 0U) {
      line("guard items.count >= " + std::to_string(minimum) + " else { throw SerializerError.invalid(\"XML collection occurrence bound\") }");
    }
    auto items = std::string{"items"};
    for (std::size_t depth{}; depth < field.presence.optional_depth; ++depth) {
      items = "Optional.some(" + items + ")";
    }
    line(target + " = " + items);
    close();
    line("return wrapper");
  }
  // Generate exact ordered Swift input with active frames rather than retained XML nodes.
  void swift_token_record(const class_node& source, const xml_binding::record& contract) {
    const auto model_name = model_.names.at(&source);
    const bool unordered = !contract.all_groups.empty();
    line("// Construct a direct typed frame; its owning candidate is private until complete EOF.");
    open("fileprivate static func srlXmlFrame(_ name: SrlXmlName, _ attributes: [SrlXmlAttribute], _ mapLimits: XmlMapLimits, _ mixedLimits: XmlMixedLimits, _ digestLimits: XmlDigestLimits, _ identityLimits: XmlIdentityLimits, _ extensionOptions: XmlExtensionOptions, _ listLimit: Int, _ complete: @escaping (" +
        model_name + ") throws -> Void) throws -> SrlXmlTokenFrame");
    if (native_xml_export_graph(source)) {
      line("throw SerializerError.invalid(\"Export-only XML reference graph: decode into the owning target\")");
      close(); return;
    }
    line("let state = SrlXmlValueBox(" + model_name + "())");
    const auto has_attributes = contract.extensions || contract.model.revision || contract.model.magic || std::any_of(contract.fields.begin(), contract.fields.end(),
        [](const auto& field) { return field.attribute; });
    if (has_attributes) { line("let fields = SrlXmlInputAttributes(attributes)"); }
    else { line("guard attributes.isEmpty else { throw SerializerError.invalid(\"Unexpected XML attribute\") }"); }
    mobile_model_input(contract, "state.value.");
    std::vector<const xml_binding::field*> elements{};
    const xml_binding::field* text_field{};
    std::string namespace_attributes{};
    for (const auto& field : contract.fields) {
      const auto attribute_start = output_.size();
      {
        const auto target = "state.value." + member_name(*field.source);
        const auto active = mobile_active_scope(contract, *field.source, "state.value.");
        const bool optional = field.source->type_name_list.front().is_optional();
        if (field.attribute) {
          if (optional && !field.default_value && !field.fixed_value) {
            line("if let text = try fields.take(" + name(field.qualified_name) + ", true) {");
            ++indent_;
            line(target + " = " + field_read(field, "text"));
            close(" else {"); ++indent_; line(target + " = nil"); close();
          } else {
            line("do {"); ++indent_;
            const bool defaulted = field.default_value || field.fixed_value;
            line("let text = try fields.take(" + name(field.qualified_name) + (defaulted ? ", true)" : ")!") );
            line(target + " = " + field_read(field, "text"));
            close();
          }
        } else if (field.text) { text_field = &field; }
        else {
          elements.push_back(&field);
          if (optional) { line(target + " = nil"); }
          else if (field.repeated) { line(target + " = []"); }
        }
      }
      if (field.attribute && mobile_namespace_value(field)) {
        namespace_attributes += output_.substr(attribute_start);
        output_.resize(attribute_start);
      }
    }
    if (has_attributes && !contract.extensions && !mobile_namespace_value(contract)) { line("try fields.finish()"); }
    line("let frame = SrlXmlTokenFrame(name)");
    if (contract.extensions) { line("var extensions: SrlXmlExtensions?"); }
    if (contract.extensions || mobile_namespace_value(contract)) {
      if (mobile_namespace_value(contract)) { line("frame.namespaceValue = true"); }
      line("frame.scopeStart = { scope in"); ++indent_;
      if (mobile_namespace_value(contract)) {
        line("state.value.srlXmlNamespaces.owner = scope");
        line("if state.value.srlXmlNamespaces.owner[WireString(\"\")] == nil { state.value.srlXmlNamespaces.owner[WireString(\"\")]=\"\" }");
        output_ += namespace_attributes;
      }
      if (contract.extensions) {
        line("let tracker = try SrlXmlExtensions.input(srlXmlExtensions" + model_name + ", scope, extensionOptions)");
        line("try fields.extensions(tracker); extensions = tracker");
      }
      if (has_attributes) { line("try fields.finish()"); }
      close();
    }
    if (contract.identity) {
      line("frame.identity = try SrlXmlIdentity(" + mobile_identity_metadata(*contract.identity,
          [&](auto value) { return quote(value); }, [&](const auto& value) { return name(value); }, true) + ", identityLimits)");
    }
    if (contract.mixed) {
      line(mixed_source(*contract.mixed, "state.value." + member_name(*contract.mixed->source), false));
      line("return frame"); close(); return;
    }
    if (text_field) {
      line("var text = \"\"");
      line("frame.text = { text.append(contentsOf: $0) }");
    }
    if (!elements.empty()) {
      std::string minimums{"["};
      for (std::size_t index{}; index < elements.size(); ++index) {
        if (index != 0U) { minimums += ", "; }
        const auto& field = *elements[index];
        const auto minimum = field.wrapper ? ((field.map_binding ? field.map_binding->optional_container : field.presence.allows_absent) ? 0U : 1U) :
            !field.list && field.source->fixed_extent != 0U ? field.source->fixed_extent : field.min_occurs;
        const auto active = contract.model.revision ? native_xml_active(contract, *field.source,
            mobile_version_language(), mobile_revision_path(contract, "state.value.")) : std::string{};
        minimums += active.empty() ? std::to_string(minimum) : "(" + active + " ? " + std::to_string(minimum) + " : 0)";
      }
      minimums += "]";
      line("let minimums = " + minimums);
      line("var counts = [Int](repeating: 0, count: " + std::to_string(elements.size()) + ")");
      if (!unordered) { line("var cursor = 0"); }
      line("frame.child = { childName, childAttributes in"); ++indent_;
      for (std::size_t index{}; index < elements.size(); ++index) {
        const auto& field = *elements[index];
        const auto selected = field.wrapper ? *field.wrapper : field.qualified_name;
        const auto position = std::to_string(index);
        open("if childName == " + name(selected));
        if (contract.model.revision) {
          const auto active = native_xml_active(contract, *field.source, mobile_version_language(), mobile_revision_path(contract, "state.value."));
          if (!active.empty()) { line("guard " + active + " else { throw SerializerError.invalid(\"XML field outside revision lifecycle\") }"); }
        }
        if (!unordered) {
          line("guard cursor <= " + position + " else { throw SerializerError.invalid(\"Unexpected XML field order\") }");
          open("while cursor < " + position);
          line("guard counts[cursor] >= minimums[cursor] else { throw SerializerError.invalid(\"Required XML field missing\") }");
          line("cursor += 1"); close();
        }
        const auto maximum = field.wrapper ? std::optional<std::size_t>{1U} :
            !field.list && field.source->fixed_extent != 0U ? std::optional<std::size_t>{field.source->fixed_extent} : field.max_occurs;
        if (maximum) {
          line("guard counts[" + position + "] < " + std::to_string(*maximum) + " else { throw SerializerError.invalid(\"XML field occurrence bound\") }");
        }
        line("counts[" + position + "] += 1");
        const auto target = "state.value." + member_name(*field.source);
        if (field.variant_binding) { line(variant_source(field, target, false)); close(); continue; }
        if (field.map_binding) { line(map_source(field, target, false)); close(); continue; }
        auto attributes = std::string{"childAttributes"};
        if (field.presence.allows_nil) {
          line("let presence = try srlXmlPresence(childAttributes)");
          open("if presence.isNil");
          line("let nilFrame = SrlXmlTokenFrame(childName)");
          line("nilFrame.text = { text in guard text.isEmpty else { throw SerializerError.invalid(\"Nil XML element contains text\") } }");
          auto nil_value = std::string{"Optional.none"};
          for (std::size_t depth{1U}; depth < field.presence.optional_depth; ++depth) {
            nil_value = "Optional.some(" + nil_value + ")";
          }
          line("nilFrame.finish = { " + target + " = " + nil_value + " }");
          line("return nilFrame"); close();
          attributes = "presence.attributes";
        }
        if (field.wrapper) { swift_token_wrapper(field, target, attributes); }
        else { swift_token_value(field, target, field.repeated, "childName", attributes); }
        close();
      }
      line("throw SerializerError.invalid(\"Unexpected XML field\")");
      close();
    }
    if (contract.extensions) {
      std::string names{"["}, positions{"["}, repeated{"["};
      for (const auto* field : elements) {
        names += name(field->wrapper ? *field->wrapper : field->qualified_name) + ',';
        positions += std::to_string(field - contract.fields.data()) + ',';
        repeated += std::string{field->repeated && !field->wrapper && !field->list ? "true," : "false,"};
      }
      names += ']'; positions += ']'; repeated += ']';
      line("frame.knownChildren = " + names);
      if (elements.empty()) { line("var cursor = 0"); }
      line("frame.foreignChild = { childName, childAttributes, namespaceScope in"); ++indent_;
      line("guard let tracker = extensions else { throw SerializerError.invalid(\"XML extension scope missing\") }");
      line("let selected = try tracker.position(childName, cursor, " + std::string{elements.empty() ? "[]" : "counts"} + ", " +
          (elements.empty() ? "[]" : "minimums") + ", " + positions + ", " + repeated + ", " + std::to_string(contract.fields.size()) + ")");
      line("cursor = selected.cursor");
      line("let preserve = try tracker.admit(childName, false, selected.position)");
      line("return try srlXmlForeignFrame(childName, childAttributes, preserve ? namespaceScope() : [:], tracker, preserve) { node in"); ++indent_;
      line("try tracker.child(selected.position, selected.item, node)"); close(); close();
    }
    line("frame.finish = {"); ++indent_;
    if (contract.extensions) {
      line("guard let tracker = extensions else { throw SerializerError.invalid(\"XML extension scope missing\") }");
      line("try tracker.finish(); state.value.srlXmlExtensions = tracker.state");
    }
    if (text_field) {
      line("state.value." + member_name(*text_field->source) + " = " +
          field_read(*text_field, "text"));
    }
    if (!elements.empty()) {
      const bool optional_group = unordered && contract.all_groups.front().minimum == 0U;
      if (optional_group) { open("if counts.contains(where: { $0 != 0 })"); }
      open("for index in counts.indices");
      line("guard counts[index] >= minimums[index] else { throw SerializerError.invalid(\"Required XML field missing\") }"); close();
      if (optional_group) { close(); }
    }
    line(native_choice_checks(contract, [&](const member& field) { return "state.value." + member_name(field); }, "swift"));
    line("try complete(state.value)"); close();
    line("return frame"); close();
  }
  // Reuse the target field traversal for owning values and scoped readonly getters.
  void mobile_emit_method(const class_node& source, const xml_binding::record& contract, const std::string& model_name) {
    line("// Emit statically bound tokens without retaining completed XML descendants.");
    open(swift_ ? "fileprivate func srlXmlEmit(_ output: SrlXmlOutput, _ name: SrlXmlName, _ typeIdentity: SrlXmlName? = nil) throws" :
        "internal fun " + model_name + ".srlXmlEmit(output: SrlXmlOutput, name: SrlXmlName, typeIdentity: SrlXmlName? = null)");
    if (source.reference) {
      const auto& payload = source.reference->value_type;
      if (payload.type != object_type::class_type) {
        line(swift_ ? "throw SerializerError.invalid(\"Scalar XML reference requires an owning field mapping\")" :
            "throw IllegalArgumentException(\"Scalar XML reference requires an owning field mapping\")");
      } else if (swift_) {
        line("guard let holder = srlOwner else { throw SerializerError.invalid(\"Unbound or expired export reference\") }");
        const auto value = reference_support::native_value_path(source, "swift", "holder.value",
            [&](auto name) { return field_name(name); });
        line("try " + value + ".srlXmlEmit(output, name, typeIdentity)");
      } else {
        line("this.withValue { selected -> selected.srlXmlEmit(output, name, typeIdentity) }");
      }
      close(); return;
    }
    if (!swift_) { line("val srlPackageScope = output.packageRecord()"); open("try"); }
    line(native_choice_checks(contract, [&](const member& field) { return (swift_ ? "self." : "this.") + member_name(field); }, language_));
    if (contract.extensions) {
      line(select("let extensions = try ", "val extensions = ") + "SrlXmlExtensions.output(srlXmlExtensions" + model_.names.at(&source) + ", " +
          (mobile_projection_ ? select("nil", "null") : select("self.srlXmlExtensions", "this.srlXmlExtensions")) + ", output.extensionOptions" + (swift_ ? "" : ", output.packageContext()") + ")");
    }
    if (mobile_namespace_value(contract)) {
      if (mobile_projection_) {
        line(select("throw SerializerError.invalid(\"QName XML projections require owning namespace state\")",
            "throw IllegalArgumentException(\"QName XML projections require owning namespace state\")"));
        if (!swift_) { close(" finally {"); ++indent_; line("srlPackageScope?.close()"); close(); }
        close(); return;
      }
      line(select("let namespaces = try ", "val namespaces = ") + "SrlXmlNamespaceOutput(" +
          select("self.srlXmlNamespaces", "this.srlXmlNamespaces") +
          (contract.extensions ? ", extensions.scope" : swift_ ? "" : ", null") + (swift_ ? "" : ", output.packageContext()") + ")");
    }
    line(select("try ", "") + "output.open(name" + (mobile_namespace_value(contract) ? ", namespaces.owner" :
        contract.extensions ? ", extensions.scope" : "") + ")");
    if (contract.identity) {
      line(select("try ", "") + "output.observe(" + mobile_identity_metadata(*contract.identity,
          [&](auto value) { return quote(value); }, [&](const auto& value) { return name(value); }, swift_) + ')');
    }
    mobile_model_output(contract);
    line(swift_ ? "if let identity = typeIdentity { try output.typeName(identity) }" : "if (typeIdentity != null) { output.typeName(typeIdentity) }");
    for (const bool attributes : {true, false}) {
    if (!attributes && contract.extensions) { line(select("try ", "") + "extensions.emitAttributes(output)"); }
    for (const auto& field : contract.fields) {
      if (field.attribute != attributes) { continue; }
      if (!swift_) { line("output.packageField(" + quote(field.source->name) + ")"); open("try"); }
      const model_emission_scope package_field_end{swift_ ? std::function<void()>{} : std::function<void()>{[&] {
        close(" catch (failure: IllegalArgumentException) {"); ++indent_;
        line("output.packageFailure(); throw failure"); close();
      }}};
      const auto position = std::to_string(&field - contract.fields.data());
      const auto boundary = contract.extensions && !field.attribute && field.repeated && !field.wrapper && !field.list ? position : std::string{};
      if (contract.extensions && !field.attribute && !field.text) { line(select("try ", "") + "extensions.emitAt(output, " + position + ", 0)"); }
      if (!boundary.empty()) { line("var extensionItems" + boundary + " = 0"); }
      const model_emission_scope extension_end{contract.extensions && !field.attribute && !field.text ? std::function<void()>{[&, position] {
        line(select("try ", "") + "extensions.endPosition(" + position + ")");
      }} : std::function<void()>{}};
      const auto active = mobile_active_scope(contract, *field.source, swift_ ? "self." : "this.");
      const auto value = (swift_ ? "self." : "this.") + member_name(*field.source);
      if (field.variant_binding) { line(variant_source(field, value, true)); continue; }
      if (field.map_binding) { line(map_source(field, value, true)); continue; }
      if (!swift_ && !field.attribute && requires_presence(field)) {
        emit_kotlin_presence(field, value, boundary);
        continue;
      }
      if (swift_ && !field.attribute && requires_presence(field)) {
        emit_swift_presence(field, value, boundary);
        continue;
      }
      if (field.wrapper) {
        open(swift_ ? "do" : "run");
        line(select("try ", "") + "output.open(" + name(*field.wrapper) + ")");
      }
      if (field.repeated) {
        const auto count = value + select(".count", ".size");
        if (field.source->fixed_extent != 0U) {
          line(swift_ ? "guard " + count + " == " + std::to_string(field.source->fixed_extent) + " else { throw SerializerError.invalid(\"XML fixed array size\") }" :
              "require(" + count + " == " + std::to_string(field.source->fixed_extent) + ") { \"XML fixed array size\" }");
        }
        if (field.min_occurs > 0U || field.max_occurs) {
          std::string condition = field.min_occurs > 0U ? count + " >= " + std::to_string(field.min_occurs) : "";
          if (field.max_occurs) { condition += (condition.empty() ? "" : " && ") + count + " <= " + std::to_string(*field.max_occurs); }
          line(swift_ ? "guard " + condition + " else { throw SerializerError.invalid(\"XSD occurrence bound\") }" : "require(" + condition + ") { \"XSD occurrence bound\" }");
        }
        open(swift_ ? "for item in " + value : "for (item in " + value + ")");
        emit_mobile_value(field, "item");
        mobile_extension_repeat(boundary); close();
      } else if (field.source->type_name_list.front().is_optional()) {
        open(swift_ ? "if let item = " + value : "if (" + value + ".isPresent)");
        if (!swift_) { line("val item = " + value + ".get()"); }
        emit_mobile_value(field, "item"); close();
      } else {
        open(swift_ ? "do" : "run"); emit_mobile_value(field, value); close();
      }
      if (field.wrapper) { line(select("try ", "") + "output.close()"); close(); }
    }
    }
    if (contract.mixed) { line(mixed_source(*contract.mixed, (swift_ ? "self." : "this.") + member_name(*contract.mixed->source), true)); }
    if (contract.extensions) {
      const auto end = std::to_string(contract.fields.size());
      line(select("try ", "") + "extensions.emitAt(output, " + end + ", 0)");
      line(select("try ", "") + "extensions.endPosition(" + end + ")");
      line(select("try ", "") + "extensions.finishOutput()");
    }
    line(select("try ", "") + "output.close()");
    if (!swift_) { close(" finally {"); ++indent_; line("srlPackageScope?.close()"); close(); }
    close();
  }
  // Emit complete-object XML methods for one native Swift/Kotlin owning record.
  void mobile_record(const class_node& source, const xml_binding::record& contract) {
    current_owner_ = &source;
    const auto model_name = model_.names.at(&source);
    const bool export_graph = native_xml_export_graph(source);
    scalar_rules_.clear();
    for (std::size_t index{}; index < contract.fields.size(); ++index) {
      const auto& field = contract.fields[index];
      if (!xml_portable::needs_scalar_rule(field)) { continue; }
      const auto identifier = "srlXmlRule" + model_name + std::to_string(index);
      scalar_rules_.emplace(field.source, identifier);
      line(select("fileprivate let ", "private val ") + identifier + " = " +
          mobile_scalar_rule(field, [&](std::string_view value) { return quote(value); }, swift_));
    }
    if (contract.extensions) {
      line(select("fileprivate let ", "private val ") + "srlXmlExtensions" + model_name + " = " +
          mobile_extension_metadata(*contract.extensions, [&](auto value) { return quote(value); },
              [&](const auto& value) { return name(value); }, swift_));
    }
    if (swift_) { open("extension " + model_name); }
    if (mobile_namespace_value(contract) && !source.reference) {
      line("// Bind newly authored lexical QName values without adding native wire fields.");
      open(swift_ ? "public mutating func xmlBindNamespace(_ prefix: String, _ uri: String) throws" :
          "fun " + model_name + ".xmlBindNamespace(prefix: String, uri: String)");
      line(select("var selected = self.srlXmlNamespaces.owner", "val selected = this.srlXmlNamespaces.owner.toMutableMap()"));
      line(select("selected[WireString(prefix)] = uri", "selected[prefix] = uri"));
      line(select("try ", "") + "srlXmlExtensionBindings(selected)");
      line(select("self.srlXmlNamespaces.owner = selected", "this.srlXmlNamespaces.owner = selected"));
      close();
    }
    line("// Encode one complete object with the fixed XML root and field mapping.");
    const auto validator = options_.runtime_xsd ? select(", validator: (any XmlValidator)? = nil", ", validator: XmlValidator? = null") : "";
    const auto legacy_validator = options_.runtime_xsd ? select(", validator: (any XmlValidator)?", ", validator: XmlValidator?") : "";
    const auto forward_validator = options_.runtime_xsd ? select(", validator: validator", ", validator = validator") : "";
    line("// Retain the original function signature for explicitly typed method references.");
    open(swift_ ? "public func encodeXml(_ limits: XmlLimits" + legacy_validator + ") throws -> [UInt8]" :
        "fun " + model_name + ".encodeXml(limits: XmlLimits" + legacy_validator + "): ByteArray");
    line(select("return try self.encodeXml(limits", "return this.encodeXml(limits") + forward_validator +
        select(", mapLimits: XmlMapLimits(), mixedLimits: XmlMixedLimits(), digestLimits: XmlDigestLimits(), identityLimits: XmlIdentityLimits(), extensionOptions: XmlExtensionOptions())", ", mapLimits = XmlMapLimits(), mixedLimits = XmlMixedLimits(), digestLimits = XmlDigestLimits(), identityLimits = XmlIdentityLimits(), extensionOptions = XmlExtensionOptions())"));
    close();
    line("// Retain the original transactional decode signature alongside companion policies.");
    open(swift_ ? "public static func decodeXml(_ bytes: [UInt8], _ limits: XmlLimits" + legacy_validator + ") throws -> " + model_name :
        "fun " + model_name + ".Companion.decodeXml(bytes: ByteArray, limits: XmlLimits" + legacy_validator + "): " + model_name);
    line(select("return try ", "return ") + model_name + ".decodeXml(bytes, limits" + forward_validator +
        select(", mapLimits: XmlMapLimits(), mixedLimits: XmlMixedLimits(), digestLimits: XmlDigestLimits(), identityLimits: XmlIdentityLimits(), extensionOptions: XmlExtensionOptions())", ", mapLimits = XmlMapLimits(), mixedLimits = XmlMixedLimits(), digestLimits = XmlDigestLimits(), identityLimits = XmlIdentityLimits(), extensionOptions = XmlExtensionOptions())"));
    close();
    line("// Encode a complete object with optional independent map and mixed-content policies.");
    open(swift_ ? "public func encodeXml(_ limits: XmlLimits = XmlLimits()" + validator + ", mapLimits: XmlMapLimits = XmlMapLimits(), mixedLimits: XmlMixedLimits = XmlMixedLimits(), digestLimits: XmlDigestLimits = XmlDigestLimits(), identityLimits: XmlIdentityLimits = XmlIdentityLimits(), extensionOptions: XmlExtensionOptions = XmlExtensionOptions()) throws -> [UInt8]" :
        "fun " + model_name + ".encodeXml(limits: XmlLimits = XmlLimits()" + validator + ", mapLimits: XmlMapLimits = XmlMapLimits(), mixedLimits: XmlMixedLimits = XmlMixedLimits(), digestLimits: XmlDigestLimits = XmlDigestLimits(), identityLimits: XmlIdentityLimits = XmlIdentityLimits(), extensionOptions: XmlExtensionOptions = XmlExtensionOptions()): ByteArray");
    line(select("let output = try ", "val output = ") + "SrlXmlOutput(limits, mapLimits, mixedLimits, digestLimits, identityLimits, extensionOptions)");
    line(select("try self.srlXmlEmit(output, ", "this.srlXmlEmit(output, ") + name(contract.root) + ")");
    line(select("let bytes = try output.finish()", "val bytes = output.finish()"));
    if (options_.runtime_xsd && options_.validation_scope != "input") {
      line(select("try validator?.validate(bytes, limits)", "validator?.validate(bytes, limits)"));
    }
    line("return bytes");
    close();
    line("// Parse and validate a fresh candidate before returning it to the caller.");
    open(swift_ ? "public static func decodeXml(_ bytes: [UInt8], _ limits: XmlLimits = XmlLimits()" + validator + ", mapLimits: XmlMapLimits = XmlMapLimits(), mixedLimits: XmlMixedLimits = XmlMixedLimits(), digestLimits: XmlDigestLimits = XmlDigestLimits(), identityLimits: XmlIdentityLimits = XmlIdentityLimits(), extensionOptions: XmlExtensionOptions = XmlExtensionOptions()) throws -> " + model_name :
        "fun " + model_name + ".Companion.decodeXml(bytes: ByteArray, limits: XmlLimits = XmlLimits()" + validator + ", mapLimits: XmlMapLimits = XmlMapLimits(), mixedLimits: XmlMixedLimits = XmlMixedLimits(), digestLimits: XmlDigestLimits = XmlDigestLimits(), identityLimits: XmlIdentityLimits = XmlIdentityLimits(), extensionOptions: XmlExtensionOptions = XmlExtensionOptions()): " + model_name);
    if (export_graph) {
      line(swift_ ? "throw SerializerError.invalid(\"Export-only XML reference graph: decode into the owning target\")" :
          "throw IllegalArgumentException(\"Export-only XML reference graph: decode into the owning target\")");
    } else {
    if (swift_) { line("var extensionOptions=extensionOptions;extensionOptions.srlPackageLimit=limits.srlPackageLimit\ntry mapLimits.validate(); try mixedLimits.validate(); try digestLimits.validate(); try identityLimits.validate(); try extensionOptions.validate()"); }
    if (options_.runtime_xsd && options_.validation_scope != "output") {
      line(select("try validator?.validate(bytes, limits)", "validator?.validate(bytes, limits)"));
    }
    if (swift_) {
      line("return try srlXmlDecodeTokens(bytes, limits, " + name(contract.root) + ", mapLimits, mixedLimits, digestLimits, identityLimits, extensionOptions, " + model_name + ".srlXmlFrame)");
    } else {
      line("SrlXmlInput(bytes, limits, mapLimits, mixedLimits, digestLimits, identityLimits, extensionOptions, " +
          std::string{mobile_extension_graph(source) ? "true" : "false"} + ").use { input ->");
      ++indent_;
      line("limits.srlPackageRootType?.let { input.expectType(it,false) }");
      line("val result = " + model_name + ".srlXmlRead(input, " + name(contract.root) + ")");
      line("input.finish()");
      line("return result");
      close();
    }
    }
    close();
    mobile_emit_method(source, contract, model_name);
    if (swift_) {
    line("// Read every required/optional field and reject all remaining XML members.");
    open(swift_ ? "fileprivate static func srlXmlRead(_ node: SrlXmlNode) throws -> " + model_name :
        "internal fun " + model_name + ".Companion.srlXmlRead(node: SrlXmlNode): " + model_name);
    const bool token_map_only = contract.extensions || export_graph || contract.model.revision || contract.model.magic || contract.model.digest || contract.mixed || std::any_of(contract.fields.begin(), contract.fields.end(), [](const auto& field) {
      return field.map_binding || field.variant_binding || xml_portable::needs_scalar_rule(field);
    });
    if (token_map_only) {
      line("throw SerializerError.invalid(\"XML maps require the direct typed token codec\")");
    } else {
    const auto has_text = std::any_of(contract.fields.begin(), contract.fields.end(), [](const auto& field) { return field.text; });
    line(select("let reader = try SrlXmlReader(node, ", "val reader = SrlXmlReader(node, ") + (has_text ? "true)" : "false)"));
    line(select(contract.fields.empty() ? "let " : "var ", "val ") + "result = " + model_name + "()");
    std::size_t index{};
    for (const auto& field : contract.fields) {
      const auto target = "result." + member_name(*field.source);
      const auto& value_type = leaf(*field.source);
      const auto raw = "raw" + std::to_string(index++);
      if (field.text) {
        line(select("let ", "val ") + raw + " = " + select("try ", "") + "reader.text()");
        line(target + " = " + read_value(value_type, raw, true));
      } else if (field.repeated) {
        line(select("let ", "val ") + raw + " = " + std::string{field.wrapper ? select("try ", "") + "reader.wrapped(" + name(*field.wrapper) + ", " : "reader.repeated("} + name(field.qualified_name) + ")");
        if (field.source->fixed_extent != 0U) {
          line(swift_ ? "guard " + raw + ".count == " + std::to_string(field.source->fixed_extent) + " else { throw SerializerError.invalid(\"XML fixed array size\") }" :
              "require(" + raw + ".size == " + std::to_string(field.source->fixed_extent) + ") { \"XML fixed array size\" }");
        }
        std::string condition;
        if (field.min_occurs > 0U) { condition = raw + select(".count", ".size") + " >= " + std::to_string(field.min_occurs); }
        if (field.max_occurs) { condition += (condition.empty() ? "" : " && ") + raw + select(".count", ".size") + " <= " + std::to_string(*field.max_occurs); }
        if (!condition.empty()) { line(swift_ ? "guard " + condition + " else { throw SerializerError.invalid(\"XSD occurrence bound\") }" : "require(" + condition + ") { \"XSD occurrence bound\" }"); }
        auto expression = select("try ", "") + raw + select(".map { item in ", ".map { item -> ") + read_value(value_type, "item", false) + " }";
        if (!swift_) {
          const bool primitive_array = field.source->modifier == member::modifier_type::array &&
              value_type.type == object_type::primitive && value_type.name != "string";
          expression += primitive_array ? ".to" + type(value_type) + "Array()" : ".toMutableList()";
        }
        line(target + " = " + expression);
      } else {
        const bool optional = field.source->type_name_list.front().is_optional();
        const auto read = "reader." + std::string{field.attribute ? "attribute" : "child"} + "(" + name(field.qualified_name) + ", " + (optional ? "true" : "false") + ")";
        if (optional) {
          line(select("let ", "val ") + raw + " = " + select("try ", "") + read);
          if (swift_) {
            open("if let item = " + raw); line(target + " = " + read_value(value_type, "item", field.attribute)); close(" else {");
            ++indent_; line(target + " = nil"); --indent_; line("}");
          } else {
            line(target + " = " + raw + "?.let { java.util.Optional.of(" + read_value(value_type, "it", field.attribute) + ") } ?: java.util.Optional.empty()");
          }
        } else {
          line(select("let ", "val ") + raw + " = " + select("try ", "") + read + select("!", "!!"));
          line(target + " = " + read_value(value_type, raw, field.attribute));
        }
      }
    }
    line(select("try ", "") + "reader.finish()"); line("return result");
    }
    close();
    }
    if (!swift_) { kotlin_token_record(source, contract); }
    else { swift_token_record(source, contract); }
    if (swift_) { close(); }
  }
  // Export scoped target getters through the owning traversal without constructing a target.
  void mobile_translator_record(const class_node& owner, const conversion_declaration& method) {
    const auto& target = ::rohit::serializer::conversion::record(method.target);
    const auto contract = bindings_.bind(target);
    if (std::any_of(contract.fields.begin(), contract.fields.end(), [](const auto& field) { return field.variant_binding.has_value(); })) {
      throw std::invalid_argument{"Readonly XML translators require a native target getter for the complete variant storage"};
    }
    current_owner_ = &target;
    const auto target_name = model_.names.at(&target);
    scalar_rules_.clear();
    for (std::size_t index{}; index < contract.fields.size(); ++index) {
      const auto& field = contract.fields[index];
      if (xml_portable::needs_scalar_rule(field)) {
        scalar_rules_.emplace(field.source, "srlXmlRule" + target_name + std::to_string(index));
      }
    }
    const auto view = model_.names.at(&owner) + '.' + translator::view_name(model_.names.at(&owner), method,
        [&](auto name, auto) { return field_name(name); }, language_);
    const auto slots = translator::slots(method, language_, [&](auto name, auto) { return field_name(name); });
    mobile_projection_ = [&](const member& field) {
      for (const auto& declared : target.member_list) {
        if (&declared == &field) { return field_name("get_" + field.name) + "()"; }
      }
      const auto path = member_path(target, field);
      if (!path) { throw std::logic_error{"Readonly XML target field is outside its native graph"}; }
      const auto separator = path->find('.');
      auto root = path->substr(0U, separator);
      if (root.starts_with("srlXmlBase")) { root = "base" + root.substr(std::string{"srlXmlBase"}.size()); }
      const auto found = std::find_if(slots.begin(), slots.end(), [&](const auto& slot) { return field_name(slot.field.name) == root; });
      if (found == slots.end() || separator == std::string::npos) {
        throw std::logic_error{"Readonly XML native base getter is absent"};
      }
      return field_name("get_" + found->field.name) + "()." + path->substr(separator + 1U);
    };
    if (swift_) { open("extension " + view); }
    const auto validator = options_.runtime_xsd ? select(", validator: (any XmlValidator)? = nil", ", validator: XmlValidator? = null") : "";
    line("// Borrow readonly getters only for this synchronous XML traversal.");
    open(swift_ ? "public func encodeXml(_ limits: XmlLimits = XmlLimits()" + validator + ", mapLimits: XmlMapLimits = XmlMapLimits(), mixedLimits: XmlMixedLimits = XmlMixedLimits(), digestLimits: XmlDigestLimits = XmlDigestLimits(), identityLimits: XmlIdentityLimits = XmlIdentityLimits(), extensionOptions: XmlExtensionOptions = XmlExtensionOptions()) throws -> [UInt8]" :
        "fun " + view + ".encodeXml(limits: XmlLimits = XmlLimits()" + validator + ", mapLimits: XmlMapLimits = XmlMapLimits(), mixedLimits: XmlMixedLimits = XmlMixedLimits(), digestLimits: XmlDigestLimits = XmlDigestLimits(), identityLimits: XmlIdentityLimits = XmlIdentityLimits(), extensionOptions: XmlExtensionOptions = XmlExtensionOptions()): ByteArray");
    line(select("let output = try ", "val output = ") + "SrlXmlOutput(limits, mapLimits, mixedLimits, digestLimits, identityLimits, extensionOptions)");
    line(select("try self.srlXmlEmit(output, ", "this.srlXmlEmit(output, ") + name(contract.root) + ')');
    line(select("let bytes = try output.finish()", "val bytes = output.finish()"));
    if (options_.runtime_xsd && options_.validation_scope != "input") { line(select("try validator?.validate(bytes, limits)", "validator?.validate(bytes, limits)")); }
    line("return bytes"); close();
    mobile_emit_method(target, contract, view);
    if (swift_) { close(); }
    mobile_projection_ = {};
  }
  // Emit C's explicitly owned and transactional optional XML APIs.
  void c_record(const class_node& source, const xml_binding::record& contract,
      const std::string& projection_name = {}, const std::function<std::string(const member&)>& projection = {});
public:
  // Bind the same checked XML contract across all native generation targets.
  generator(const native::schema& model, const xml_options& options, std::string_view language)
      : model_{model}, bindings_{options}, options_{options}, language_{language},
        swift_{language == "swift"}, c_{language == "c"} {}
  // Keep callbacks and the borrowed owner cursor within one emission pass.
  generator(const generator&) = delete;
  // Prevent copying another pass over active generator state.
  generator& operator=(const generator&) = delete;
  // Preserve callback captures bound to this generator instance.
  generator(generator&&) = delete;
  // Prevent invalidating callbacks while replacing an emission pass.
  generator& operator=(generator&&) = delete;
  // Generate the provider adapter once and direct typed bindings for every selected record.
  std::string generate();
};

// Emit C provider calls and typed assignments without reflection or an intermediate codec.
void generator::c_record(const class_node& source, const xml_binding::record& contract,
    const std::string& projection_name, const std::function<std::string(const member&)>& projection) {
  current_owner_ = &source;
  const auto n = projection_name.empty() ? native::snake(model_.names.at(&source)) : projection_name;
  const bool output_only = !projection_name.empty();
  const bool export_graph = native_xml_export_graph(source);
  const bool extension_graph = native_extension_graph(source, bindings_);
  if (extension_graph) { validate_native_extension_graph(source, bindings_, "c"); }
  if ((extension_graph || contract.extensions) && (output_only || export_graph)) {
    throw std::invalid_argument{"Native XML extension companions require an owning graph; export/reference extension adapters are not implemented for c"};
  }
  const auto output_start = output_.size();
  std::string reference_helpers{};
  std::size_t reference_helper_index{};
  // Readonly getter pointers lend target-shaped fields without constructing a target record.
  const auto member_access = [&](const member& field) {
    return projection ? projection(field) : "value->" + member_name(field);
  };
  const auto storage_type = [&](auto&& self, const type_name& value) -> std::string {
    if (value.is_optional() || value.is_sequence() || value.is_associative()) {
      auto result = "srl_" + value.name;
      for (const auto& item : value.arguments) {
        const auto inner = self(self, item);
        result += "_" + std::to_string(inner.size()) + "_" + inner;
      }
      return result;
    }
    return type(value);
  };
  const auto revision_path = [&] { return contract.model.revision ? member_access(*contract.model.revision) : std::string{}; };
  // Keep lifecycle closure paired with each field even when a container emitter continues early.
  const auto active_scope = [&](const member& field) -> model_emission_scope {
    const auto condition = native_xml_active(contract, field, version_language::c, revision_path());
    if (!condition.empty()) { open("if (" + condition + ")"); }
    return model_emission_scope{condition.empty() ? std::function<void()>{} : std::function<void()>{[&] { close(); }}};
  };
  // Preserve declaring-owner raw map helpers and the existing generic map storage names.
  const auto map_storage = [&](const xml_binding::field& field) {
    const auto& source_type = field.source->type_name_list.front();
    if (field.source->modifier == member::modifier_type::map) {
      auto result = array_name(*field.source); result.replace(result.size()-sizeof("array")+1U, sizeof("array")-1U, "map"); return result;
    }
    return storage_type(storage_type, source_type.is_optional() ? source_type.arguments.front() : source_type);
  };
  // Resolve mixed fragment storage through the same raw/generic collection helpers as native codecs.
  const auto mixed_storage = [&] {
    const auto& mixed = *contract.mixed;
    const auto& source_type = mixed.source->type_name_list.front();
    const auto* item = &source_type;
    while (item->is_sequence()) { item = &item->arguments.front(); }
    return std::pair{mixed.source->modifier == member::modifier_type::array ? array_name(*mixed.source) :
        storage_type(storage_type, source_type), type(*item)};
  };
  // Capture existing typed emitters for an isolated generated helper without duplicating conversions.
  const auto capture = [&](auto&& action) {
    auto saved = std::move(output_); const auto saved_indent = indent_; output_.clear(); indent_ = 0U;
    action(); auto generated = std::move(output_); output_ = std::move(saved); indent_ = saved_indent; return generated;
  };
  const auto identity_graph = native_identity_graph(source, contract, bindings_);
  const bool identity_enabled = std::any_of(identity_graph.begin(), identity_graph.end(),
      [](const auto& node) { return node.scope.has_value(); });
  const auto identity_identifier = "srl_xml_identity_graph_" + n;
  if (extension_graph) {
    line(native_extension_metadata(contract, source.get_full_name(), n + "_srl_xml_extension_model",
        [&](const class_node& child) {
          return native_extension_graph(child, bindings_) ? native::snake(model_.names.at(&child)) + "_srl_xml_extension_model" : std::string{};
        }, [&](std::string_view value) { return quote(value); }, false));
  }
  line(c_identity_declaration(identity_identifier, identity_graph, [&](std::string_view value) { return quote(value); }));
  line(c_identity_diagnostic_declaration(identity_identifier, identity_graph, [&](std::string_view value) { return quote(value); }));
  line("/* Validate original/final XML using bounded active paths and tuple indexes, without native copies. */");
  open("static inline srl_status " + n + "_srl_xml_identity_check(const uint8_t* bytes,size_t size,const srl_limits* limits,const srl_xml_identity_limits* identity_limits)");
  if (identity_enabled) {
    line("const srl_limits bounds=limits?*limits:srl_default_limits();const srl_xml_identity_limits policy=identity_limits?*identity_limits:srl_xml_default_identity_limits();");
    line("return srl_xml_identity_validate(bytes,size,&bounds,&policy," + identity_identifier + ",sizeof(" + identity_identifier + ")/sizeof(" + identity_identifier + "[0]));");
  } else { line("(void)bytes;(void)size;(void)limits;(void)identity_limits;return srl_ok;"); }
  close();
  line("/* Admit shared tuple resources using parallel native-name diagnostics. */");
  open("static inline srl_status srl_xml_operation_identity_check_" + n + "(const uint8_t* bytes,size_t size,const srl_limits* limits,const srl_xml_identity_limits* identity_limits,srl_xml_operation* operation)");
  line("const srl_limits bounds=limits?*limits:srl_default_limits();const srl_xml_identity_limits policy=identity_limits?*identity_limits:srl_xml_default_identity_limits();");
  line("return srl_xml_operation_identity_validate(bytes,size,&bounds,&policy," + identity_identifier + ",sizeof(" + identity_identifier + ")/sizeof(" + identity_identifier + "[0])," + identity_identifier + "_diagnostics,operation);");
  close();
  std::map<const member*, std::string> native_scalar_rules;
  for (std::size_t index{}; index < contract.fields.size(); ++index) {
    const auto& field = contract.fields[index];
    if (!xml_portable::needs_scalar_rule(field)) { continue; }
    const auto identifier = "srl_xml_scalar_" + n + '_' + std::to_string(index);
    native_scalar_rules.emplace(field.source, identifier);
    line(c_scalar_declaration(identifier, field, [&](std::string_view text) { return quote(text); }));
  }
  bool token_read = true;
  const auto free_leaf = [&](const type_name& value, const std::string& target) {
    if (value.is_digest()) { line("srl_buffer_free(&" + target + ");"); }
    else if (value.name == "string" || value.name.starts_with("version")) { line("srl_string_free(&" + target + ");"); }
    else if (value.type == object_type::class_type) { line(type(value) + "_free(&" + target + ");"); }
  };
  const auto initialize_leaf = [&](const type_name& value, const std::string& target) {
    if (value.type == object_type::class_type) {
      line("if (" + std::string{token_read ? "srl_xml_operation_init_" + type(value) : type(value) + "_init"} + "(&" + target +
          (token_read ? ",srl_xml_operation_input_context(input->reader)" : "") + ") != srl_ok) { return false; }");
    }
  };
  // Explicit XML states retain each native optional layer and require wrappers for collections.
  const auto controlled_presence = [](const xml_binding::field& value) {
    return !value.attribute && !value.list && (value.presence.explicit_policy ||
        (value.presence.collection && value.presence.optional_depth != 0U));
  };
  // Release a reused optional slot through its actual owning storage before resetting its state.
  const auto free_owned = [&](auto&& self, const type_name& value, const std::string& target) -> void {
    if (value.is_optional()) {
      open("if (" + target + ".has_value)"); self(self, value.arguments.front(), target + ".value"); close();
    } else if (value.is_sequence()) { line(storage_type(storage_type, value) + "_clear(&" + target + ");"); }
    else { free_leaf(value, target); }
  };
  // Format one admitted native scalar while retaining its exact temporary ownership.
  const auto format_leaf = [&](const type_name& value_type, const std::string& expression) {
    if (value_type.is_digest()) {
      line("srl_buffer digest_text={0}; const srl_status digest_status=srl_xml_operation_digest_text(operation,&" + expression + "," + std::to_string(value_type.digest_extent) + "u,out->digest_limits,&digest_text); if (digest_status != srl_ok) { out->writer.status=digest_status; return false; }");
      line("const char* text=(const char*)digest_text.data; const size_t text_size=digest_text.size;");
    } else if (value_type.name == "string" || value_type.name.starts_with("version")) {
      line("const char* text = " + expression + ".data ? " + expression + ".data : \"\"; const size_t text_size = " + expression + ".size;");
    } else if (value_type.type == object_type::enum_type) {
      const auto count = static_cast<const enum_node&>(*value_type.resolved_node).enum_name_list.size();
      line("if ((unsigned)" + expression + " >= " + std::to_string(count) + "u) { return false; }");
      line("const char* text = " + type(value_type) + "_srl_names[(unsigned)" + expression + "]; const size_t text_size = strlen(text);");
    } else if (value_type.name == "bool") {
      line("const char* text = " + expression + " ? \"true\" : \"false\"; const size_t text_size = strlen(text);");
    } else if (value_type.name == "float" || value_type.name == "double") {
      line("srl_writer number = {0}; number.protocol = srl_json;");
      line("if (isnan(" + expression + ")) { srl_raw(&number, \"NaN\"); } else if (isinf(" + expression + ")) { srl_raw(&number, " + expression + " < 0 ? \"-INF\" : \"INF\"); } else { srl_write_float(&number, (double)" + expression + ", " + (value_type.name == "float" ? "true" : "false") + "); }");
      line("srl_put_byte(&number, 0); if (number.status != srl_ok) { srl_buffer_free(&number.buffer); return false; }");
      line("const char* text = (const char*)number.buffer.data; const size_t text_size = number.buffer.size-1;");
    } else {
      line("char number[srl_number_scratch_bytes]; const int written = snprintf(number, sizeof(number), " +
           std::string{value_type.name.starts_with('u') ? "\"%\" PRIu64, (uint64_t)" : "\"%\" PRId64, (int64_t)"} + expression + ");");
      line("if (written < 0 || (size_t)written >= sizeof(number)) { return false; } const char* text = number; const size_t text_size = (size_t)written;");
    }
  };
  // Release only the temporary spelling storage allocated by the matching scalar formatter.
  const auto release_leaf_text = [&](const type_name& value_type) {
    if (value_type.is_digest()) { line("srl_buffer_free(&digest_text);"); }
    if (value_type.name == "float" || value_type.name == "double") { line("srl_buffer_free(&number.buffer);"); }
  };
  std::function<void(const xml_binding::field&, const std::string&, std::size_t, const type_name*)> emit_leaf;
  emit_leaf = [&](const xml_binding::field& field, const std::string& expression, std::size_t prefix,
      const type_name* explicit_type) {
    line("srl_xml_operation_select(operation," + quote(source.get_full_name() + '.' + (field.source ? field.source->name : std::string{"$value"})) + ");");
    line("if (!srl_xml_operation_charge(operation,srl_xml_operation_work,1u)) { out->writer.status=srl_invalid; return false; }");
    const auto& value_type = explicit_type ? *explicit_type : leaf(*field.source);
    const auto* owner = value_type.type == object_type::class_type ? static_cast<const class_node*>(value_type.resolved_node) : nullptr;
    if (owner && owner->reference && owner->reference->value_projection) {
      const auto helper = "srl_xml_reference_" + n + '_' + std::to_string(reference_helper_index++);
      const auto& payload = owner->reference->value_type;
      reference_helpers += capture([&] {
        line("/* Emit one borrowed selected field; the caller pins its owner for this entire function. */");
        open("static inline bool " + helper + "(xmlTextWriterPtr writer, srl_xml_output* out, const " + type(payload) + "* selected,srl_xml_operation* operation)");
        emit_leaf(field, "(*selected)", prefix, &payload); line("return true;"); close();
      });
      open(""); line("const " + type(payload) + "* selected=NULL;");
      line("if (" + type(value_type) + "_read_value(&" + expression + ",&selected) != srl_ok) { return false; }");
      const bool borrowed = owner->reference->mode == reference_declaration::mode_type::borrowed;
      if (!borrowed) {
        line("srl_reference_scope* scope=" + expression + ".srl_reference; if (srl_reference_scope_retain(scope) != srl_ok) { return false; }");
      }
      line("const bool emitted=" + helper + "(writer,out,selected,operation);");
      if (!borrowed) { line("srl_reference_scope_release(scope);"); }
      line("if (!emitted) { return false; }"); close(); return;
    }
    if (value_type.type == object_type::class_type) {
      line("if (!srl_xml_operation_write_" + type(value_type) + "(writer, out, &" + expression + ", " +
           quote(field.qualified_name.local) + ", " + quote(field.qualified_name.namespace_uri) + ",operation)) { return false; }");
      return;
    }
    if (!field.attribute && !field.text) {
      line("if (!srl_xml_operation_node(out,operation)) { return false; }");
    line("if (++out->nodes > out->limits.max_elements || out->writer.depth >= out->limits.max_depth || xmlTextWriterStartElementNS(writer, " +
           std::string{field.qualified_name.namespace_uri.empty() ? "NULL" : "BAD_CAST \"srl\""} + ", BAD_CAST " + quote(field.qualified_name.local) + ", " +
           (field.qualified_name.namespace_uri.empty() ? "NULL" : "BAD_CAST " + quote(field.qualified_name.namespace_uri)) + ") < 0) { return false; }");
      line("if (!srl_xml_operation_output_type(writer,out,operation)) { return false; }");
    }
    if (field.list) {
      const auto helper = "srl_xml_list_item_" + n + '_' + std::to_string(reference_helper_index++);
      reference_helpers += capture([&] {
        line("/* Append a borrowed native list item while keeping temporary lexical storage local. */");
        open("static inline bool " + helper + "(const " + type(value_type) + "* item, srl_xml_output* out, srl_buffer* joined,srl_xml_operation* operation)");
        format_leaf(value_type, "(*item)");
        line("const srl_status status=srl_xml_operation_scalar_append(operation,joined,text,text_size,out->limits.max_string_bytes);");
        release_leaf_text(value_type);
        line("if (status!=srl_ok) { out->writer.status=status; return false; } return true;"); close();
      });
      line("if ((" + expression + ".size&&!" + expression + ".data)||" + expression + ".size>out->limits.max_elements) { out->writer.status=srl_limit; return false; }");
      if (field.source->fixed_extent != 0U) { line("if (" + expression + ".size!=" + std::to_string(field.source->fixed_extent) + "u) { return false; }"); }
      line("srl_buffer joined={0};");
      line("for (size_t index=0;index<" + expression + ".size;++index) { if (!" + helper + "(&" + expression + ".data[index],out,&joined,operation)) { srl_buffer_free(&joined); return false; } }");
      line("const char* text=(const char*)joined.data; const size_t text_size=joined.size;");
    } else { format_leaf(value_type, expression); }
    const auto rule = native_scalar_rules.find(field.source);
    if (rule != native_scalar_rules.end()) {
      const auto fallback = field.fixed_value ? field.fixed_value : field.default_value;
      open("if (text_size>out->limits.max_string_bytes)");
      if (field.list) { line("srl_buffer_free(&joined);"); } else { release_leaf_text(value_type); }
      line("out->writer.status=srl_limit; return false;"); close();
      line("srl_string normalized={0}; const srl_status scalar_status=srl_xml_operation_scalar_output(operation,&" + rule->second +
          ",text,text_size," + (fallback && !field.attribute ? quote(*fallback) : "NULL") + ",out->limits.max_elements,&normalized);");
      if (field.list) { line("srl_buffer_free(&joined);"); } else { release_leaf_text(value_type); }
      line("if (scalar_status!=srl_ok) { out->writer.status=scalar_status; return false; }");
    }
    line("const bool emitted = srl_xml_operation_emit_text(operation,writer, out, " +
         (field.attribute ? quote(field.qualified_name.local) : "NULL") + ", " +
         quote(field.qualified_name.namespace_uri) + ", " + quote("a" + std::to_string(prefix)) + ", " +
         (rule != native_scalar_rules.end() ? "normalized.data, normalized.size" : "text, text_size") + ");");
    if (rule != native_scalar_rules.end()) { line("srl_string_free(&normalized);"); }
    else if (field.list) { line("srl_buffer_free(&joined);"); }
    else { release_leaf_text(value_type); }
    line("if (!emitted) { return false; }");
    if (!field.attribute && !field.text) { line("if (xmlTextWriterEndElement(writer) < 0) { return false; }"); }
  };
  const auto read_leaf_checked = [&](const type_name& value_type, const std::string& target, const std::string& raw,
      const std::string& cleanup) {
    if (value_type.is_digest()) {
      line("const srl_status digest_status=srl_xml_operation_digest_read(srl_xml_operation_input_context(input->reader),(const char*)" + raw + ",(size_t)xmlStrlen(" + raw + ")," + std::to_string(value_type.digest_extent) + "u,input->digest_limits,&" + target + "); if (digest_status != srl_ok) { input->status=digest_status; " + cleanup + " return false; }");
    } else if (value_type.name == "string" || value_type.name.starts_with("version")) {
      line("if (srl_xml_operation_string_set(" + std::string{token_read ? "srl_xml_operation_input_context(input->reader)" : "NULL"} + ",&" + target + ", (const char*)" + raw + ", (size_t)xmlStrlen(" + raw + ")) != srl_ok) { " + cleanup + " return false; }");
    } else if (value_type.type == object_type::enum_type) {
      const auto count = static_cast<const enum_node&>(*value_type.resolved_node).enum_name_list.size();
      line("size_t ordinal = 0; while (ordinal < " + std::to_string(count) + "u && strcmp((const char*)" + raw + ", " + type(value_type) + "_srl_names[ordinal]) != 0) { ++ordinal; }");
      line("if (ordinal == " + std::to_string(count) + "u) { " + cleanup + " return false; } " + target + " = (" + type(value_type) + ")ordinal;");
    } else if (value_type.name == "bool") {
      line("if (!srl_xml_boolean((const char*)" + raw + ", &" + target + ")) { " + cleanup + " return false; }");
    } else if (value_type.name == "float" || value_type.name == "double") {
      line("double number = 0; if (!srl_xml_float((const char*)" + raw + ", " + (value_type.name == "float" ? "true" : "false") + ", &number)) { " + cleanup + " return false; } " + target + " = (" + type(value_type) + ")number;");
    } else {
      const bool positive = value_type.name.starts_with('u');
      line(std::string{positive ? "uint64_t" : "int64_t"} + " number = 0; if (!srl_xml_" +
           (positive ? "unsigned" : "signed") + "((const char*)" + raw + ", " + std::to_string(native::width(value_type) * 8) +
           "u, &number)) { " + cleanup + " return false; } " + target + " = (" + type(value_type) + ")number;");
    }
  };
  // Keep the legacy scalar conversion ownership contract for maps, variants and unchanged native fields.
  const auto read_leaf = [&](const type_name& value_type, const std::string& target, const std::string& raw) {
    read_leaf_checked(value_type, target, raw, "xmlFree(" + raw + ");");
  };
  // Validate a scalar/list before assigning candidate-owned fields; every failure releases the lexical buffers.
  const auto read_field_leaf = [&](const xml_binding::field& field, const type_name& value_type,
      const std::string& target, const std::string& raw) {
    line("srl_xml_operation_select(srl_xml_operation_input_context(input->reader)," + quote(source.get_full_name() + '.' + field.source->name) + ");");
    const auto rule = native_scalar_rules.find(field.source);
    if (rule == native_scalar_rules.end()) { read_leaf(value_type, target, raw); return; }
    open("");
    line("const char* lexical=(const char*)" + raw + "; size_t lexical_size=(size_t)xmlStrlen(" + raw + ");");
    const auto fallback = field.fixed_value ? field.fixed_value : field.default_value;
    if (fallback && !field.attribute) {
      line("if (!lexical_size) { lexical=" + quote(*fallback) + "; lexical_size=" + std::to_string(fallback->size()) + "u; }");
    }
    line("srl_string normalized={0}; const srl_status scalar_status=srl_xml_operation_scalar_check(srl_xml_operation_input_context(input->reader),&" + rule->second +
        ",lexical,lexical_size,limits->max_elements,&normalized); if (scalar_status!=srl_ok) { input->status=scalar_status; xmlFree(" + raw + "); return false; }");
    const auto cleanup = "srl_string_free(&normalized); xmlFree(" + raw + ");";
    if (field.list) {
      const auto& original = field.source->type_name_list.front();
      const auto& collection = original.is_optional() ? original.arguments.front() : original;
      const auto collection_name = field.source->modifier == member::modifier_type::array ? array_name(*field.source) : storage_type(storage_type, collection);
      line("size_t count=0; const srl_status counted=srl_xml_operation_scalar_count(srl_xml_operation_input_context(input->reader),normalized.data,limits->max_elements,&count); if (counted!=srl_ok) { input->status=counted; " + cleanup + " return false; }");
      if (field.source->fixed_extent != 0U) {
        line("if (count!=" + std::to_string(field.source->fixed_extent) + "u) { " + cleanup + " return false; }");
      }
      line(collection_name + "_clear(&" + target + ");");
      line("if (count>SIZE_MAX/sizeof(" + target + ".data[0])) { input->status=srl_limit; " + cleanup + " return false; }");
      line("if(!srl_xml_operation_input_allocate(input,count*sizeof(" + target + ".data[0]))){" + cleanup + "return false;}");
      line("if (count) { " + target + ".data=(" + type(value_type) + "*)calloc(count,sizeof(" + target + ".data[0])); if (!" + target + ".data) { input->status=srl_allocation; " + cleanup + " return false; } }");
      line(target + ".size=count; " + target + ".capacity=count; size_t offset=0,begin=0,item_size=0,index=0;");
      open("while (srl_xml_scalar_token(normalized.data,&offset,&begin,&item_size))");
      line("const char delimiter=normalized.data[begin+item_size]; normalized.data[begin+item_size]=0;");
      read_leaf_checked(value_type, target + ".data[index++]", "BAD_CAST (normalized.data+begin)", cleanup);
      line("normalized.data[begin+item_size]=delimiter;"); close();
    } else { read_leaf_checked(value_type, target, "BAD_CAST normalized.data", cleanup); }
    line("srl_string_free(&normalized);"); close();
  };
  // Entry helpers own no temporary pointer array, so each scalar failure can return immediately.
  for (const auto& field : contract.fields) {
    if (!field.map_binding) { continue; }
    const auto& map = *field.map_binding;
    const auto helper = "srl_xml_map_" + n + '_' + field_name(field.source->name);
    line(c_map_key_size(map, helper + "_key_size", [&](const type_name& value) { return type(value); }));
    open("static inline bool " + helper + "_entry(xmlTextWriterPtr writer, srl_xml_output* out, const " + map_storage(field) + "_entry* entry,srl_xml_operation* operation)");
    line("if (!srl_xml_operation_node(out,operation)) { return false; }");
    line("if (++out->nodes > out->limits.max_elements || ++out->writer.depth > out->limits.max_depth || xmlTextWriterStartElementNS(writer," +
        std::string{map.entry.namespace_uri.empty() ? "NULL" : "BAD_CAST \"srl\""} + ",BAD_CAST " + quote(map.entry.local) + "," +
        (map.entry.namespace_uri.empty() ? "NULL" : "BAD_CAST " + quote(map.entry.namespace_uri)) + ") < 0) { return false; }");
    auto key = field; key.qualified_name = map.key; key.attribute = map.key_placement == xml_binding::map_key_placement::attribute;
    open(""); emit_leaf(key, "entry->key", 0U, &map.key_type); close();
    auto value = field; value.qualified_name = map.value; value.attribute = false;
    open(""); emit_leaf(value, "entry->value", 0U, &map.value_type); close();
    line("--out->writer.depth; return xmlTextWriterEndElement(writer) >= 0;"); close();
  }
  line("/* Write statically mapped fields through the optional bounded libxml2 provider. */");
  open("static inline bool srl_xml_operation_write_" + n + "(xmlTextWriterPtr writer, srl_xml_output* out, const " + n + "* value, const char* local, const char* uri,srl_xml_operation* operation)");
  const bool reference_record = source.reference && source.reference->value_type.type == object_type::class_type;
  if (reference_record) {
    const auto& payload = source.reference->value_type;
    line("const " + type(payload) + "* selected=NULL; if (" + n + "_read_value(value,&selected) != srl_ok) { return false; }");
    const bool borrowed = source.reference->mode == reference_declaration::mode_type::borrowed;
    if (!borrowed) { line("srl_reference_scope* scope=value->srl_reference; if (srl_reference_scope_retain(scope) != srl_ok) { return false; }"); }
    line("const bool emitted=srl_xml_operation_write_" + type(payload) + "(writer,out,selected,local,uri,operation);");
    if (!borrowed) { line("srl_reference_scope_release(scope);"); }
    line("return emitted;");
  } else {
  line(native_choice_checks(contract, [&](const member& field) { return member_access(field); }, "c"));
  line("if (!srl_xml_operation_node(out,operation)) { return false; }");
    line("if (++out->nodes > out->limits.max_elements || ++out->writer.depth > out->limits.max_depth || xmlTextWriterStartElementNS(writer, *uri ? BAD_CAST \"srl\" : NULL, BAD_CAST local, *uri ? BAD_CAST uri : NULL) < 0) { return false; }");
  line("if (!srl_xml_operation_output_type(writer,out,operation)) { return false; }");
  if (contract.model.revision) {
    const auto& revision = *contract.model.revision;
    const auto& revision_type = revision.type_name_list.front();
    line("if (" + native_xml_revision_invalid(revision, version_language::c, revision_path()) + ") { return false; }");
    if (revision_type.name == "float" || revision_type.name == "double") {
      line("if (signbit(" + revision_path() + ") || !isfinite(" + revision_path() + ")) { return false; }");
    }
    xml_binding::field metadata{}; metadata.source = &revision; metadata.attribute = true;
    metadata.qualified_name = {"revision", std::string{xml_binding::model_metadata_namespace}};
    open(""); emit_leaf(metadata, revision_path(), std::numeric_limits<std::size_t>::max()-1U, nullptr); close();
  }
  if (contract.model.magic) {
    line("if (!srl_xml_operation_emit_text(operation,writer,out,\"magic\"," + quote(xml_binding::model_metadata_namespace) + ",\"srlmodel\"," + quote(*contract.model.magic) + "," + std::to_string(contract.model.magic->size()) + "u)) { return false; }");
  }
  if (source.reference) {
    type_name reference_type{std::string{source.name}, source.parent_namespace}; reference_type.type = object_type::class_type;
    reference_type.name = source.name; reference_type.resolved_node = &source;
    xml_binding::field text{}; text.text = true;
    emit_leaf(text, "(*value)", 0U, &reference_type);
  }
  if (contract.fields.empty()) { line("(void)value;"); }
  std::size_t prefix{};
  for (const bool attributes : {true, false}) {
    for (const auto& field : contract.fields) {
      if (field.attribute != attributes) { continue; }
      line("srl_xml_operation_select(operation," + quote(source.get_full_name() + '.' + field.source->name) + ");");
      const auto lifecycle = active_scope(*field.source);
      const auto expression = member_access(*field.source);
      const auto& source_type = field.source->type_name_list.front();
      if (field.map_binding) {
        const auto helper = "srl_xml_map_" + n + '_' + field_name(field.source->name);
        line(c_map_method(field, expression, map_storage(field), helper + "_entry", helper + "_key_size", false, {}, {}, {})); continue;
      }
      if (field.variant_binding) {
        line(c_variant_method(field, expression, false, {},
            [&](const type_name& value_type, const xml::name& name, const std::string& value) {
              auto selected = field; selected.qualified_name = name;
              return capture([&] { emit_leaf(selected, value, prefix++, &value_type); });
            })); continue;
      }
      if (controlled_presence(field)) {
        const auto& presence = field.presence;
        auto payload = expression;
        open("");
        if (presence.allows_absent) { open("if (" + payload + ".has_value)"); payload += ".value"; }
        if (presence.allows_nil) {
          open("if (!" + payload + ".has_value)");
          const auto& element = field.wrapper ? *field.wrapper : field.qualified_name;
          line("if (!srl_xml_operation_node(out,operation)) { return false; }");
    line("if (++out->nodes > out->limits.max_elements || out->writer.depth >= out->limits.max_depth || xmlTextWriterStartElementNS(writer, " +
              std::string{element.namespace_uri.empty() ? "NULL" : "BAD_CAST \"srl\""} + ", BAD_CAST " + quote(element.local) + ", " +
              (element.namespace_uri.empty() ? "NULL" : "BAD_CAST " + quote(element.namespace_uri)) + ") < 0) { return false; }");
          line("if (!srl_xml_operation_emit_text(operation,writer, out, \"nil\", \"http://www.w3.org/2001/XMLSchema-instance\", \"xsi\", \"true\", sizeof(\"true\")-1) || xmlTextWriterEndElement(writer) < 0) { return false; }");
          close(" else {"); ++indent_; payload += ".value";
        }
        if (field.wrapper) {
          line("if (!srl_xml_operation_node(out,operation)) { return false; }");
    line("if (++out->nodes > out->limits.max_elements || ++out->writer.depth > out->limits.max_depth || xmlTextWriterStartElementNS(writer, " +
              std::string{field.wrapper->namespace_uri.empty() ? "NULL" : "BAD_CAST \"srl\""} + ", BAD_CAST " + quote(field.wrapper->local) + ", " +
              (field.wrapper->namespace_uri.empty() ? "NULL" : "BAD_CAST " + quote(field.wrapper->namespace_uri)) + ") < 0) { return false; }");
        }
        if (presence.collection) {
          line("if(" + payload + ".size>out->limits.max_elements){return srl_xml_operation_output_limit(out,operation);}");
          line("if ((" + payload + ".size && !" + payload + ".data) || " + payload + ".size > out->limits.max_elements) { return false; }");
          const auto minimum = field.source->fixed_extent != 0U ? field.source->fixed_extent : field.min_occurs;
          const auto maximum = field.source->fixed_extent != 0U ? std::optional<std::size_t>{field.source->fixed_extent} : field.max_occurs;
          if (minimum > 0U) { line("if (" + payload + ".size < " + std::to_string(minimum) + "u) { return false; }"); }
          if (maximum) { line("if (" + payload + ".size > " + std::to_string(*maximum) + "u) { return false; }"); }
          open("for (size_t index = 0; index < " + payload + ".size; ++index)");
          emit_leaf(field, payload + ".data[index]", prefix++, nullptr); close();
        } else { open(""); emit_leaf(field, payload, prefix++, nullptr); close(); }
        if (field.wrapper) { line("--out->writer.depth; if (xmlTextWriterEndElement(writer) < 0) { return false; }"); }
        if (presence.allows_nil) { close(); }
        if (presence.allows_absent) { close(); }
        close(); continue;
      }
      if (field.wrapper) {
        line("if (!srl_xml_operation_node(out,operation)) { return false; }");
    line("if (++out->nodes > out->limits.max_elements || ++out->writer.depth > out->limits.max_depth || xmlTextWriterStartElementNS(writer, " +
            std::string{field.wrapper->namespace_uri.empty() ? "NULL" : "BAD_CAST \"srl\""} + ", BAD_CAST " + quote(field.wrapper->local) + ", " +
            (field.wrapper->namespace_uri.empty() ? "NULL" : "BAD_CAST " + quote(field.wrapper->namespace_uri)) + ") < 0) { return false; }");
      }
      if (field.repeated) {
        line("if(" + expression + ".size>out->limits.max_elements){return srl_xml_operation_output_limit(out,operation);}");
        line("if ((" + expression + ".size && !" + expression + ".data) || " + expression + ".size > out->limits.max_elements) { return false; }");
        if (field.min_occurs > 0U) { line("if (" + expression + ".size < " + std::to_string(field.min_occurs) + "u) { return false; }"); }
        if (field.max_occurs) { line("if (" + expression + ".size > " + std::to_string(*field.max_occurs) + "u) { return false; }"); }
        if (field.source->fixed_extent != 0U) { line("if (" + expression + ".size != " + std::to_string(field.source->fixed_extent) + "u) { return false; }"); }
        open("for (size_t index = 0; index < " + expression + ".size; ++index)");
        emit_leaf(field, expression + ".data[index]", prefix++, nullptr); close();
      } else if (source_type.is_optional()) {
        open("if (" + expression + ".has_value)"); emit_leaf(field, expression + ".value", prefix++, nullptr); close();
      } else { open(""); emit_leaf(field, expression, prefix++, nullptr); close(); }
      if (field.wrapper) { line("--out->writer.depth; if (xmlTextWriterEndElement(writer) < 0) { return false; }"); }
    }
  }
  if (contract.mixed) {
    const auto lifecycle = active_scope(*contract.mixed->source);
    const auto [collection, fragment] = mixed_storage();
    line(c_mixed_method(*contract.mixed, member_access(*contract.mixed->source), collection, fragment,
        field_name(contract.mixed->variant->name), false, {},
        [&](const type_name& value_type, const xml::name& name, const std::string& value) {
          xml_binding::field selected{}; selected.source = contract.mixed->source; selected.qualified_name = name;
          return capture([&] { emit_leaf(selected, value, prefix++, &value_type); });
        },source.get_full_name() + '.' + contract.mixed->source->name));
  }
  line("--out->writer.depth; return xmlTextWriterEndElement(writer) >= 0;");
  }
  close();
  line("/* Preserve the original nested-writer signature with no attached operation. */");
  open("static inline bool " + n + "_srl_xml_write(xmlTextWriterPtr writer,srl_xml_output* out,const " + n + "* value,const char* local,const char* uri)");
  line("return srl_xml_operation_write_" + n + "(writer,out,value,local,uri,NULL);");
  close();
  if (!output_only) {
  token_read = false;
  line("/* Decode a complete record into candidate-owned fields without publishing partial state. */");
  open("static inline bool " + n + "_srl_xml_read(xmlNodePtr node, " + n + "* value, const srl_limits* limits)");
  const bool token_presence_only = export_graph || contract.model.revision || contract.model.magic || contract.model.digest || contract.mixed || std::any_of(contract.fields.begin(), contract.fields.end(),
      [&](const auto& field) { return controlled_presence(field) || field.map_binding || field.variant_binding || xml_portable::needs_scalar_rule(field); });
  if (token_presence_only) {
    line("(void)node; (void)value; (void)limits; return false;");
  } else {
  line("(void)limits;");
  const auto has_text = std::any_of(contract.fields.begin(), contract.fields.end(), [](const auto& field) { return field.text; });
  if (!has_text) { line("if (!srl_xml_record(node)) { return false; }"); }
  line("xmlNodePtr cursor = srl_xml_next(node->children); size_t used_attributes = 0;");
  if (contract.fields.empty()) { line("(void)value; (void)limits;"); }
  for (const auto& field : contract.fields) {
    const auto target = member_access(*field.source);
    const auto& value_type = leaf(*field.source);
    const auto& source_type = field.source->type_name_list.front();
    const auto matches = "srl_xml_name(cursor, " + quote(field.qualified_name.local) + ", " + quote(field.qualified_name.namespace_uri) + ")";
    open("");
    if (field.wrapper) {
      line("if (!srl_xml_name(cursor, " + quote(field.wrapper->local) + ", " + quote(field.wrapper->namespace_uri) + ") || !srl_xml_record(cursor) || cursor->properties) { return false; }");
      line("xmlNodePtr after_wrapper = srl_xml_next(cursor->next); cursor = srl_xml_next(cursor->children);");
    }
    if (field.text) {
      line("if (cursor) { return false; } xmlChar* text = xmlNodeGetContent(node); if (!text) { return false; }");
      read_leaf(value_type, target, "text"); line("xmlFree(text);");
    } else if (field.repeated) {
      const auto collection_name = field.source->modifier == member::modifier_type::array ?
          array_name(*field.source) : storage_type(storage_type, source_type);
      line(collection_name + "_clear(&" + target + ");");
      open("while (" + matches + ")");
      line("if (" + target + ".size >= limits->max_elements) { return false; }");
      open("if (" + target + ".size == " + target + ".capacity)");
      line("size_t capacity = " + target + ".capacity ? " + target + ".capacity : 4u;");
      line("if (capacity > SIZE_MAX/2u) { return false; } capacity *= 2u; if (capacity > limits->max_elements) { capacity = limits->max_elements; }");
      line("if (capacity <= " + target + ".size || capacity > SIZE_MAX/sizeof(" + target + ".data[0])) { return false; }");
      line("void* memory = realloc(" + target + ".data, capacity*sizeof(" + target + ".data[0])); if (!memory) { return false; } " + target + ".data = (" + type(value_type) + "*)memory; " + target + ".capacity = capacity;"); close();
      line("memset(&" + target + ".data[" + target + ".size], 0, sizeof(" + target + ".data[0]));");
      line("const size_t slot = " + target + ".size++;");
      const auto slot = target + ".data[slot]";
      initialize_leaf(value_type, slot);
      if (value_type.type == object_type::class_type) {
        line("if (!" + type(value_type) + "_srl_xml_read(cursor, &" + slot + ", limits)) { return false; }");
      } else {
        line("xmlChar* text = srl_xml_text(cursor); if (!text) { return false; }");
        read_leaf(value_type, slot, "text"); line("xmlFree(text);");
      }
      line("cursor = srl_xml_next(cursor->next);"); close();
      if (field.min_occurs > 0U) { line("if (" + target + ".size < " + std::to_string(field.min_occurs) + "u) { return false; }"); }
      if (field.max_occurs) { line("if (" + target + ".size > " + std::to_string(*field.max_occurs) + "u) { return false; }"); }
      if (field.source->fixed_extent != 0U) { line("if (" + target + ".size != " + std::to_string(field.source->fixed_extent) + "u) { return false; }"); }
    } else {
      const bool optional = source_type.is_optional();
      if (optional) {
        free_owned(free_owned, source_type, target);
        line("memset(&" + target + ", 0, sizeof(" + target + "));");
      }
      const auto destination = target + (optional ? ".value" : "");
      if (field.attribute) {
        line("xmlChar* text = xmlGetNsProp(node, BAD_CAST " + quote(field.qualified_name.local) + ", " +
             (field.qualified_name.namespace_uri.empty() ? "NULL" : "BAD_CAST " + quote(field.qualified_name.namespace_uri)) + ");");
        if (!optional) { line("if (!text) { return false; }"); }
        open("if (text)");
        line("++used_attributes;");
        if (optional) { line(target + ".has_value = true;"); }
        read_leaf(value_type, destination, "text"); line("xmlFree(text);"); close();
      } else {
        if (!optional) { line("if (!" + matches + ") { return false; }"); }
        open("if (" + matches + ")");
        if (optional) { line(target + ".has_value = true;"); initialize_leaf(value_type, destination); }
        if (value_type.type == object_type::class_type) {
          line("if (!" + type(value_type) + "_srl_xml_read(cursor, &" + destination + ", limits)) { return false; }");
        } else {
          line("xmlChar* text = srl_xml_text(cursor); if (!text) { return false; }");
          read_leaf(value_type, destination, "text"); line("xmlFree(text);");
        }
        line("cursor = srl_xml_next(cursor->next);"); close();
      }
    }
    if (field.wrapper) { line("if (cursor) { return false; } cursor = after_wrapper;"); }
    close();
  }
  line("size_t attribute_count = 0; for (xmlAttrPtr attribute = node->properties; attribute; attribute = attribute->next) { ++attribute_count; }");
  line("return cursor == NULL && used_attributes == attribute_count;");
  }
  close();
  token_read = true;
  // A scalar owns only its active lexical buffer; nested records recurse into their candidate.
  const auto read_token_value = [&](const xml_binding::field& field, const std::string& destination,
      const type_name* explicit_type = nullptr) {
    const auto& value_type = explicit_type ? *explicit_type : leaf(*field.source);
    if (value_type.type == object_type::class_type) {
      line("if (!" + type(value_type) + "_srl_xml_read_tokens(input, &" + destination + ", limits, " +
          quote(field.qualified_name.local) + ", " + quote(field.qualified_name.namespace_uri) + ")) { return false; }");
    } else {
      line("xmlChar* text = srl_xml_input_scalar(input, " + quote(field.qualified_name.local) + ", " +
          quote(field.qualified_name.namespace_uri) + "); if (!text) { return false; }");
      read_field_leaf(field, value_type, destination, "text"); line("xmlFree(text);");
    }
  };
  // Decode a controlled field into candidate-owned optional and collection storage.
  const auto read_presence = [&](const xml_binding::field& field, const std::string& target) {
    const auto& presence = field.presence;
    const auto& element = field.wrapper ? *field.wrapper : field.qualified_name;
    const auto& source_type = field.source->type_name_list.front();
    const auto& value_type = leaf(*field.source);
    open("");
    if (presence.optional_depth != 0U) {
      free_owned(free_owned, source_type, target);
      line("memset(&" + target + ", 0, sizeof(" + target + "));");
    }
    line("const char state = srl_xml_input_presence(input, " + quote(element.local) + ", " + quote(element.namespace_uri) + ", " +
        (presence.allows_absent ? "true" : "false") + ", " + (presence.allows_nil ? "true" : "false") + ");");
    line("if (!state) { return false; }");
    if (presence.optional_depth == 2U) { line("if (state != 'a') { " + target + ".has_value = true; }"); }
    open("if (state == 'v')");
    auto payload = target;
    for (std::size_t depth{}; depth < presence.optional_depth; ++depth) {
      line(payload + ".has_value = true;"); payload += ".value";
    }
    if (presence.collection) {
      line("if (!srl_xml_input_has(input, " + quote(element.local) + ", " + quote(element.namespace_uri) +
          ") || input->member_attributes > (input->nil_attribute ? 1u : 0u) || !srl_xml_input_next(input)) { return false; }");
      const auto collection_name = field.source->modifier == member::modifier_type::array ?
          array_name(*field.source) : storage_type(storage_type, *presence.value_type);
      line(collection_name + "_clear(&" + payload + ");");
      open("while (srl_xml_input_has(input, " + quote(field.qualified_name.local) + ", " + quote(field.qualified_name.namespace_uri) + "))");
      line("if(" + payload + ".size>=limits->max_elements){return srl_xml_operation_input_limit(input);}");
      const auto maximum = field.source->fixed_extent != 0U ? std::optional<std::size_t>{field.source->fixed_extent} : field.max_occurs;
      line("if (" + payload + ".size >= limits->max_elements" +
          (maximum ? " || " + payload + ".size >= " + std::to_string(*maximum) + "u" : "") + ") { return false; }");
      open("if (" + payload + ".size == " + payload + ".capacity)");
      line("const size_t initial_capacity_elements = 4u; size_t capacity = " + payload + ".capacity ? " + payload + ".capacity : initial_capacity_elements;");
      line("if (capacity > SIZE_MAX/2u) { return false; } capacity *= 2u; if (capacity > limits->max_elements) { capacity = limits->max_elements; }");
      line("if (capacity <= " + payload + ".size || capacity > SIZE_MAX/sizeof(" + payload + ".data[0])) { return false; }");
      line("if(!srl_xml_operation_input_allocate(input,(capacity-" + payload + ".capacity)*sizeof(" + payload + ".data[0]))){return false;}");
      line("void* memory = realloc(" + payload + ".data, capacity*sizeof(" + payload + ".data[0])); if (!memory) { return false; } " + payload + ".data = (" + type(value_type) + "*)memory; " + payload + ".capacity = capacity;"); close();
      line("memset(&" + payload + ".data[" + payload + ".size], 0, sizeof(" + payload + ".data[0])); const size_t slot = " + payload + ".size++;");
      initialize_leaf(value_type, payload + ".data[slot]"); read_token_value(field, payload + ".data[slot]"); close();
      const auto minimum = field.source->fixed_extent != 0U ? field.source->fixed_extent : field.min_occurs;
      if (minimum > 0U) { line("if (" + payload + ".size < " + std::to_string(minimum) + "u) { return false; }"); }
      line("if (!srl_xml_input_close(input, " + quote(element.local) + ", " + quote(element.namespace_uri) + ")) { return false; }");
    } else {
      if (presence.optional_depth != 0U) { initialize_leaf(value_type, payload); }
      read_token_value(field, payload);
    }
    close(); close();
  };
  line("/* Consume provider tokens directly into a private owning candidate and exact ordered fields. */");
  open("static inline bool " + n + "_srl_xml_read_tokens(srl_xml_input* input, " + n + "* value, const srl_limits* limits, const char* local, const char* uri)");
  if (export_graph) {
    line("(void)input; (void)value; (void)limits; (void)local; (void)uri; return false;");
  } else {
  line("(void)value; (void)limits;");
  line("if (!srl_xml_input_has(input, local, uri)) { return false; }");
  line("const size_t attribute_count = input->member_attributes - (input->nil_attribute ? 1u : 0u); size_t used_attributes = 0;");
  if (contract.model.revision) {
    const auto& revision = *contract.model.revision;
    const auto& revision_type = revision.type_name_list.front();
    open("");
    line("xmlChar* text=xmlTextReaderGetAttributeNs(input->reader,BAD_CAST \"revision\",BAD_CAST " + quote(xml_binding::model_metadata_namespace) + "); if (!text) { return false; } ++used_attributes;");
    if (!revision_type.name.starts_with("version")) {
      line("if (!srl_xml_revision_number((const char*)text," + std::string{revision_type.name == "float" || revision_type.name == "double" ? "true" : "false"} + ")) { xmlFree(text); return false; }");
    }
    read_leaf(revision_type, revision_path(), "text"); line("xmlFree(text);");
    line("if (" + native_xml_revision_invalid(revision, version_language::c, revision_path()) + ") { return false; }");
    close();
  }
  if (contract.model.magic) {
    open("");
    line("xmlChar* text=xmlTextReaderGetAttributeNs(input->reader,BAD_CAST \"magic\",BAD_CAST " + quote(xml_binding::model_metadata_namespace) + "); if (!text) { return false; }");
    line("const bool matches=strcmp((const char*)text," + quote(*contract.model.magic) + ")==0; xmlFree(text); if (!matches) { return false; } ++used_attributes;");
    close();
  }

  for (const auto& field : contract.fields) {
    if (!field.attribute) { continue; }
    const auto lifecycle = active_scope(*field.source);
    const auto target = member_access(*field.source);
    const auto& value_type = leaf(*field.source);
    const bool optional = field.source->type_name_list.front().is_optional();
    open("");
    if (optional) {
      free_owned(free_owned, field.source->type_name_list.front(), target);
      line("memset(&" + target + ", 0, sizeof(" + target + "));");
    }
    line("xmlChar* text = xmlTextReaderGetAttributeNs(input->reader, BAD_CAST " + quote(field.qualified_name.local) + ", " +
        (field.qualified_name.namespace_uri.empty() ? "NULL" : "BAD_CAST " + quote(field.qualified_name.namespace_uri)) + ");");
    const auto fallback = field.fixed_value ? field.fixed_value : field.default_value;
    const bool defaultable = fallback && field.min_occurs == 0U;
    if (defaultable) {
      line("if (text) { ++used_attributes; } else { text=xmlStrdup(BAD_CAST " + quote(*fallback) + "); if (!text) { input->status=srl_allocation; return false; } }");
    } else if (!optional) { line("if (!text) { return false; }"); }
    open("if (text)");
    if (!defaultable) { line("++used_attributes;"); }
    if (optional) { line(target + ".has_value = true;"); }
    read_field_leaf(field, value_type, target + (optional ? ".value" : ""), "text"); line("xmlFree(text);");
    close(); close();
  }
  line("if (used_attributes != attribute_count || !srl_xml_input_next(input)) { return false; }");
  for (const auto& field : contract.fields) {
    if (field.attribute) { continue; }
    if (!contract.all_groups.empty()) { continue; }
    const auto lifecycle = active_scope(*field.source);
    const auto target = member_access(*field.source);
    if (field.map_binding) {
      const auto helper = "srl_xml_map_" + n + '_' + field_name(field.source->name);
      line(c_map_method(field, target, map_storage(field), helper + "_entry", helper + "_key_size", true,
          [&](const type_name& value, const std::string& destination) { return capture([&] { initialize_leaf(value, destination); }); },
          [&](const type_name& value, const xml::name& name, const std::string& destination) {
            auto selected = field; selected.qualified_name = name;
            return capture([&] { read_token_value(selected, destination, &value); });
          },
          [&](const type_name& value, const std::string& destination, const std::string& text) {
            return capture([&] { read_leaf(value, destination, text); });
          })); continue;
    }
    if (field.variant_binding) {
      line(c_variant_method(field, target, true,
          [&](const type_name& value_type, const xml::name& name, const std::string& value) {
            auto selected = field; selected.qualified_name = name;
            return capture([&] { read_token_value(selected, value, &value_type); });
          }, {})); continue;
    }
    if (controlled_presence(field)) { read_presence(field, target); continue; }
    const auto& value_type = leaf(*field.source);
    const auto& source_type = field.source->type_name_list.front();
    open("");
    if (field.wrapper) {
      line("if (!srl_xml_input_has(input, " + quote(field.wrapper->local) + ", " + quote(field.wrapper->namespace_uri) +
          ") || input->member_attributes || !srl_xml_input_next(input)) { return false; }");
    }
    if (field.text) {
      line("xmlChar* text = srl_xml_input_text(input); if (!text) { return false; }");
      read_field_leaf(field, value_type, target, "text"); line("xmlFree(text);");
    } else if (field.repeated) {
      const auto collection_name = field.source->modifier == member::modifier_type::array ?
          array_name(*field.source) : storage_type(storage_type, source_type);
      line(collection_name + "_clear(&" + target + ");");
      open("while (srl_xml_input_has(input, " + quote(field.qualified_name.local) + ", " + quote(field.qualified_name.namespace_uri) + "))");
      line("if(" + target + ".size>=limits->max_elements){return srl_xml_operation_input_limit(input);}");
      const auto maximum = field.source->fixed_extent != 0U ?
          std::optional<std::size_t>{field.source->fixed_extent} : field.max_occurs;
      line("if (" + target + ".size >= limits->max_elements" +
          (maximum ? " || " + target + ".size >= " + std::to_string(*maximum) + "u" : "") + ") { return false; }");
      open("if (" + target + ".size == " + target + ".capacity)");
      line("const size_t initial_capacity_elements = 4u;");
      line("size_t capacity = " + target + ".capacity ? " + target + ".capacity : initial_capacity_elements;");
      line("if (capacity > SIZE_MAX/2u) { return false; } capacity *= 2u; if (capacity > limits->max_elements) { capacity = limits->max_elements; }");
      line("if (capacity <= " + target + ".size || capacity > SIZE_MAX/sizeof(" + target + ".data[0])) { return false; }");
      line("if(!srl_xml_operation_input_allocate(input,(capacity-" + target + ".capacity)*sizeof(" + target + ".data[0]))){return false;}");
      line("void* memory = realloc(" + target + ".data, capacity*sizeof(" + target + ".data[0])); if (!memory) { return false; } " + target + ".data = (" + type(value_type) + "*)memory; " + target + ".capacity = capacity;"); close();
      line("memset(&" + target + ".data[" + target + ".size], 0, sizeof(" + target + ".data[0]));");
      line("const size_t slot = " + target + ".size++;");
      const auto destination = target + ".data[slot]";
      initialize_leaf(value_type, destination);
      read_token_value(field, destination);
      close();
      const auto minimum = field.source->fixed_extent != 0U ? field.source->fixed_extent : field.min_occurs;
      if (minimum > 0U) { line("if (" + target + ".size < " + std::to_string(minimum) + "u) { return false; }"); }
    } else {
      const bool optional = source_type.is_optional();
      if (optional) {
        free_owned(free_owned, source_type, target);
        line("memset(&" + target + ", 0, sizeof(" + target + "));");
        open("if (srl_xml_input_has(input, " + quote(field.qualified_name.local) + ", " + quote(field.qualified_name.namespace_uri) + "))");
        line(target + ".has_value = true;"); initialize_leaf(value_type, target + ".value");
      }
      read_token_value(field, target + (optional ? ".value" : ""));
      if (optional) { close(); }
    }
    if (field.wrapper) {
      line("if (!srl_xml_input_close(input, " + quote(field.wrapper->local) + ", " + quote(field.wrapper->namespace_uri) + ")) { return false; }");
    }
    close();
  }
  if (!contract.all_groups.empty() && std::any_of(contract.fields.begin(), contract.fields.end(),
          [](const auto& field) { return !field.attribute; })) {
    line("bool seen[" + std::to_string(contract.fields.size()) + "] = {false};");
    const bool optional_group = contract.all_groups.front().minimum == 0U;
    if (optional_group) { line("size_t all_count = 0;"); }
    for (const auto& field : contract.fields) {
      if (field.attribute || !field.source->type_name_list.front().is_optional()) { continue; }
      const auto target = member_access(*field.source);
      free_owned(free_owned, field.source->type_name_list.front(), target);
      line("memset(&" + target + ", 0, sizeof(" + target + "));");
    }
    open("while (srl_xml_input_space(input) && input->kind == XML_READER_TYPE_ELEMENT)");
    for (std::size_t field_index{}; field_index < contract.fields.size(); ++field_index) {
      const auto& field = contract.fields[field_index];
      if (field.attribute) { continue; }
      const auto target = member_access(*field.source);
      const bool optional = field.source->type_name_list.front().is_optional();
      open("if (srl_xml_input_has(input, " + quote(field.qualified_name.local) + ", " + quote(field.qualified_name.namespace_uri) + "))");
      const auto position = "seen[" + std::to_string(field_index) + "]";
      line("if (" + position + ") { return false; } " + position + " = true;");
      if (optional_group) { line("++all_count;"); }
      if (controlled_presence(field)) {
        read_presence(field, target);
        line("continue;"); close(); continue;
      }
      if (optional) {
        line(target + ".has_value = true;"); initialize_leaf(leaf(*field.source), target + ".value");
      }
      read_token_value(field, target + (optional ? ".value" : ""));
      line("continue;"); close();
    }
    line("return false;"); close();
    if (optional_group) { open("if (all_count != 0)"); }
    for (std::size_t field_index{}; field_index < contract.fields.size(); ++field_index) {
      const auto& field = contract.fields[field_index];
      if (!field.attribute && field.min_occurs != 0U) {
        line("if (!seen[" + std::to_string(field_index) + "]) { return false; }");
      }
    }
    if (optional_group) { close(); }
  }
  line(native_choice_checks(contract, [&](const member& field) { return member_access(field); }, "c"));
  if (contract.mixed) {
    const auto lifecycle = active_scope(*contract.mixed->source);
    const auto [collection, fragment] = mixed_storage();
    line(c_mixed_method(*contract.mixed, member_access(*contract.mixed->source), collection, fragment,
        field_name(contract.mixed->variant->name), true,
        [&](const type_name& value_type, const xml::name& name, const std::string& value) {
          xml_binding::field selected{}; selected.source = contract.mixed->source; selected.qualified_name = name;
          return capture([&] { read_token_value(selected, value, &value_type); });
        }, {},source.get_full_name() + '.' + contract.mixed->source->name));
  }
  line("return srl_xml_input_close(input, local, uri);");
  }
  close();
  }
  line("/* Replace initialized result bytes only after the complete XML document is finalized. */");
  const auto validator_parameter = options_.runtime_xsd ? ", const srl_xml_validation* validation" : "";
  open("static inline srl_status srl_xml_operation_encode_" + n + "(const " + n + "* value, srl_buffer* result, const srl_limits* limits" + validator_parameter + ", const srl_xml_map_limits* map_limits, const srl_xml_mixed_limits* mixed_limits, const srl_xml_digest_limits* digest_limits, const srl_xml_identity_limits* identity_limits, bool check_identity, const srl_limits* transport_limits,srl_xml_operation* operation)");
  if (options_.runtime_xsd && options_.validation_scope == "input") { line("(void)validation;"); }
  line("if (!value || !result) { return srl_invalid; } srl_xml_output out = {0}; out.limits = limits ? *limits : srl_default_limits();");
  line("/* Bound private namespace framing independently; modeled strings/lists retain the original limits. */");
  line("out.framing_bytes = transport_limits ? transport_limits->max_bytes : out.limits.max_bytes;");
  line("out.map_limits = map_limits ? *map_limits : srl_xml_default_map_limits();");
  line("out.mixed_limits = mixed_limits ? *mixed_limits : srl_xml_default_mixed_limits();");
  line("out.digest_limits = digest_limits ? *digest_limits : srl_xml_default_digest_limits();");
  line("srl_xml_operation_select(operation," + quote(source.get_full_name()) + ");if(!srl_xml_operation_check(operation)){return srl_invalid;}");
  line("srl_xml_operation_output output_operation={&out,operation};xmlOutputBufferPtr buffer = xmlOutputBufferCreateIO(operation?srl_xml_operation_output_write:srl_xml_output_write, srl_xml_output_close, operation?(void*)&output_operation:(void*)&out, NULL); if (!buffer) { return srl_xml_operation_fail(operation,srl_allocation); }");
  line("xmlTextWriterPtr writer = xmlNewTextWriter(buffer); if (!writer) { xmlOutputBufferClose(buffer); return srl_xml_operation_fail(operation,srl_allocation); }");
  line("const bool good = srl_xml_operation_write_" + n + "(writer, &out, value, " + quote(contract.root.local) + ", " + quote(contract.root.namespace_uri) + ",operation) && xmlTextWriterFlush(writer) >= 0; xmlFreeTextWriter(writer);");
  line("if (!good || out.writer.status != srl_ok) { srl_buffer_free(&out.writer.buffer); return srl_xml_operation_fail(operation,out.writer.status == srl_ok ? srl_invalid : out.writer.status); }");
  line("if(operation){const srl_status observed=srl_xml_operation_observe_output(out.writer.buffer.data,out.writer.buffer.size," + identity_identifier + ",sizeof(" + identity_identifier + ")/sizeof(" + identity_identifier + "[0])," + identity_identifier + "_diagnostics,operation);if(observed!=srl_ok){srl_buffer_free(&out.writer.buffer);return observed;}}");
  line("if (check_identity) { const srl_status identity_status=operation?srl_xml_operation_identity_check_" + n + "(out.writer.buffer.data,out.writer.buffer.size,&out.limits,identity_limits,operation):" + n + "_srl_xml_identity_check(out.writer.buffer.data,out.writer.buffer.size,&out.limits,identity_limits); if(identity_status!=srl_ok) {srl_buffer_free(&out.writer.buffer);return srl_xml_operation_fail(operation,identity_status);} }");
  if (options_.runtime_xsd && options_.validation_scope != "input") {
    line("if (validation && validation->validate) { const srl_status validated = validation->validate(out.writer.buffer.data, out.writer.buffer.size, &out.limits, validation->context); if (validated != srl_ok) { srl_buffer_free(&out.writer.buffer); return srl_xml_operation_fail(operation,validated); } }");
  }
  line("if(!srl_xml_operation_check(operation)){srl_buffer_free(&out.writer.buffer);return srl_invalid;}srl_buffer_free(result); *result = out.writer.buffer; return srl_ok;"); close();
  line("/* Preserve the original known-model encoder and its function pointer type. */");
  open("static inline srl_status " + n + "_srl_xml_encode_known(const " + n + "* value,srl_buffer* result,const srl_limits* limits" + validator_parameter + ",const srl_xml_map_limits* map_limits,const srl_xml_mixed_limits* mixed_limits,const srl_xml_digest_limits* digest_limits,const srl_xml_identity_limits* identity_limits,bool check_identity,const srl_limits* transport_limits)");
  line("return srl_xml_operation_encode_" + n + "(value,result,limits" + (options_.runtime_xsd ? ",validation" : "") + ",map_limits,mixed_limits,digest_limits,identity_limits,check_identity,transport_limits,NULL);");close();
  line("/* Preserve every existing policy arity while applying default independent identity admission. */");
  open("static inline srl_status " + n + "_encode_xml_with_all_policies(const " + n + "* value, srl_buffer* result, const srl_limits* limits" + validator_parameter + ", const srl_xml_map_limits* map_limits, const srl_xml_mixed_limits* mixed_limits, const srl_xml_digest_limits* digest_limits)");
  if (extension_graph) {
    line("(void)value;(void)result;(void)limits;(void)map_limits;(void)mixed_limits;(void)digest_limits;");
    if (options_.runtime_xsd) { line("(void)validation;"); }
    line("/* Positioned state requires the initialized XML document companion. */ return srl_invalid;");
  } else { line("return " + n + "_srl_xml_encode_known(value,result,limits" + (options_.runtime_xsd ? ",validation" : "") + ",map_limits,mixed_limits,digest_limits,NULL,true,NULL);"); }
  close();
  line("/* Select bounded owner-local identity indexes independently of native/XML limits. */");
  open("static inline srl_status " + n + "_encode_xml_with_identity_limits(const " + n + "* value,srl_buffer* result,const srl_limits* limits" + validator_parameter + ",const srl_xml_identity_limits* identity_limits)");
  if (extension_graph) {
    line("(void)value;(void)result;(void)limits;(void)identity_limits;");
    if (options_.runtime_xsd) { line("(void)validation;"); }
    line("return srl_invalid;");
  } else { line("return " + n + "_srl_xml_encode_known(value,result,limits" + (options_.runtime_xsd ? ",validation" : "") + ",NULL,NULL,NULL,identity_limits,true,NULL);"); }
  close();
  line("/* Preserve existing policy arities while forwarding default digest admission. */");
  open("static inline srl_status " + n + "_encode_xml_with_policies(const " + n + "* value, srl_buffer* result, const srl_limits* limits" + validator_parameter + ", const srl_xml_map_limits* map_limits, const srl_xml_mixed_limits* mixed_limits)");
  line("return " + n + "_encode_xml_with_all_policies(value,result,limits" + (options_.runtime_xsd ? ",validation" : "") + ",map_limits,mixed_limits,NULL);"); close();
  line("/* Select independent decoded digest byte admission through every nested field. */");
  open("static inline srl_status " + n + "_encode_xml_with_digest_limits(const " + n + "* value, srl_buffer* result, const srl_limits* limits" + validator_parameter + ", const srl_xml_digest_limits* digest_limits)");
  line("return " + n + "_encode_xml_with_all_policies(value,result,limits" + (options_.runtime_xsd ? ",validation" : "") + ",NULL,NULL,digest_limits);"); close();
  line("/* Preserve independent map-policy selection while applying default ordered-content limits. */");
  open("static inline srl_status " + n + "_encode_xml_with_map_limits(const " + n + "* value, srl_buffer* result, const srl_limits* limits" + validator_parameter + ", const srl_xml_map_limits* map_limits)");
  line("return " + n + "_encode_xml_with_policies(value,result,limits" + (options_.runtime_xsd ? ",validation" : "") + ",map_limits,NULL);"); close();
  line("/* Select independent ordered-content limits without changing existing native XML limits. */");
  open("static inline srl_status " + n + "_encode_xml_with_mixed_limits(const " + n + "* value, srl_buffer* result, const srl_limits* limits" + validator_parameter + ", const srl_xml_mixed_limits* mixed_limits)");
  line("return " + n + "_encode_xml_with_policies(value,result,limits" + (options_.runtime_xsd ? ",validation" : "") + ",NULL,mixed_limits);"); close();
  line("/* Preserve the existing complete-document entrypoint and its default nested map policy. */");
  open("static inline srl_status " + n + "_encode_xml(const " + n + "* value, srl_buffer* result, const srl_limits* limits" + validator_parameter + ")");
  line("return " + n + "_encode_xml_with_map_limits(value,result,limits" + (options_.runtime_xsd ? ",validation" : "") + ",NULL);"); close();
  if (!output_only) {
  line("/* Decode exactly into a fresh candidate; syntax/contract failure retains the destination. */");
  open("static inline srl_status srl_xml_operation_decode_" + n + "(" + n + "* result, const uint8_t* bytes, size_t size, const srl_limits* limits" + validator_parameter + ", const srl_xml_map_limits* map_limits, const srl_xml_mixed_limits* mixed_limits, const srl_xml_digest_limits* digest_limits,const srl_xml_identity_limits* identity_limits,bool check_identity,const srl_limits* transport_limits,srl_xml_operation* operation,bool package_type)");
  if (export_graph) {
    line("(void)result; (void)bytes; (void)size; (void)limits; (void)map_limits; (void)mixed_limits; (void)digest_limits; (void)identity_limits; (void)check_identity; (void)transport_limits;(void)operation;(void)package_type;");
    if (options_.runtime_xsd) { line("(void)validation;"); }
    line("return srl_invalid;");
  } else {
  if (options_.runtime_xsd && options_.validation_scope == "output") { line("(void)validation;"); }
  line("if (!result || (size && !bytes)) { return srl_invalid; } const srl_limits bounds = limits ? *limits : srl_default_limits();");
  line("srl_xml_operation_select(operation," + quote(source.get_full_name()) + ");if(!srl_xml_operation_check(operation)){return srl_invalid;}");
  line("if (check_identity) {const srl_status identity_status=operation?srl_xml_operation_identity_check_" + n + "(bytes,size,&bounds,identity_limits,operation):" + n + "_srl_xml_identity_check(bytes,size,&bounds,identity_limits);if(identity_status!=srl_ok){return srl_xml_operation_fail(operation,identity_status);}}");
  line("/* Canonical namespace framing may need a larger provider budget; native field/list bounds stay original. */");
  line("srl_xml_operation_reader reader_operation={0};reader_operation.operation=operation;reader_operation.graph=" + identity_identifier + ";reader_operation.diagnostics=" + identity_identifier + "_diagnostics;reader_operation.graph_count=sizeof(" + identity_identifier + ")/sizeof(" + identity_identifier + "[0]);");
  line("srl_xml_input input; const srl_status checked = srl_xml_operation_input_init(&input, bytes, size, transport_limits ? transport_limits : &bounds,operation?&reader_operation:NULL);");
  line("if (checked != srl_ok) { srl_xml_input_free(&input);free(reader_operation.path);return srl_xml_operation_fail(operation,checked); }");
  const auto operation_type_uri = "urn:serializer:native:" + (source.parent_namespace ? get_full_name_for_namespace(source.parent_namespace) : std::string{});
  line("if(package_type){srl_xml_type_identity selected;const bool typed=srl_xml_input_type(&input,&selected);const bool authorized=typed&&(!selected.present||(xmlStrEqual(selected.uri,BAD_CAST " + quote(operation_type_uri) + ")&&xmlStrEqual(selected.local,BAD_CAST " + quote(source.name) + ")));srl_xml_type_free(&selected);if(!authorized){const srl_status status=input.status;srl_xml_input_free(&input);free(reader_operation.path);return srl_xml_operation_fail(operation,status==srl_ok?srl_invalid:status);}}");
  line("input.map_limits = map_limits ? *map_limits : srl_xml_default_map_limits();");
  line("input.mixed_limits = mixed_limits ? *mixed_limits : srl_xml_default_mixed_limits();");
  line("input.digest_limits = digest_limits ? *digest_limits : srl_xml_default_digest_limits();");
  if (options_.runtime_xsd && options_.validation_scope != "output") {
    line("if (validation && validation->validate) { const srl_status validated = validation->validate(bytes, size, &bounds, validation->context); if (validated != srl_ok) { srl_xml_input_free(&input);free(reader_operation.path);return srl_xml_operation_fail(operation,validated); } }");
  }
  line(n + " candidate = {0}; const srl_status initialized = srl_xml_operation_init_" + n + "(&candidate,operation);");
  line("const bool good = initialized == srl_ok && " + n + "_srl_xml_read_tokens(&input, &candidate, &bounds, " + quote(contract.root.local) + ", " + quote(contract.root.namespace_uri) + ") && srl_xml_input_finish(&input);");
  line("const srl_status parsed = input.status; srl_xml_input_free(&input);free(reader_operation.path);");
  line("if (!good||!srl_xml_operation_check(operation)) { " + n + "_free(&candidate);return srl_xml_operation_fail(operation,initialized != srl_ok ? initialized : parsed == srl_ok ? srl_invalid : parsed); } " + n + "_free(result); *result = candidate; return srl_ok;");
  }
  close();
  line("/* Preserve the original known-model decoder and its complete argument list. */");
  open("static inline srl_status " + n + "_srl_xml_decode_known(" + n + "* result,const uint8_t* bytes,size_t size,const srl_limits* limits" + validator_parameter + ",const srl_xml_map_limits* map_limits,const srl_xml_mixed_limits* mixed_limits,const srl_xml_digest_limits* digest_limits,const srl_xml_identity_limits* identity_limits,bool check_identity,const srl_limits* transport_limits)");
  line("return srl_xml_operation_decode_" + n + "(result,bytes,size,limits" + (options_.runtime_xsd ? ",validation" : "") + ",map_limits,mixed_limits,digest_limits,identity_limits,check_identity,transport_limits,NULL,false);");close();
  if (!model_.packages.empty() && !extension_graph && !export_graph) {
    const auto type_uri = "urn:serializer:native:" +
        (source.parent_namespace ? get_full_name_for_namespace(source.parent_namespace) : std::string{});
    line("/* Decode a package-authorized root type without changing ordinary XML attribute membership. */");
    open("static inline srl_status " + n + "_srl_xml_decode_package(" + n + "* result,const uint8_t* bytes,size_t size,const srl_limits* limits)");
    line("if(!result||(size&&!bytes)){return srl_invalid;}const srl_limits bounds=limits?*limits:srl_default_limits();srl_xml_input input;const srl_status checked=srl_xml_input_init(&input,bytes,size,&bounds);if(checked!=srl_ok){srl_xml_input_free(&input);return checked;}");
    line("srl_xml_type_identity selected;const bool typed=srl_xml_input_type(&input,&selected);const bool authorized=typed&&(!selected.present||(xmlStrEqual(selected.uri,BAD_CAST " + quote(type_uri) + ")&&xmlStrEqual(selected.local,BAD_CAST " + quote(source.name) + ")));srl_xml_type_free(&selected);if(!authorized){const srl_status status=input.status;srl_xml_input_free(&input);return status==srl_ok?srl_invalid:status;}");
    line(n + " candidate={0};const srl_status initialized=" + n + "_init(&candidate);const bool good=initialized==srl_ok&&" + n + "_srl_xml_read_tokens(&input,&candidate,&bounds," + quote(contract.root.local) + "," + quote(contract.root.namespace_uri) + ")&&srl_xml_input_finish(&input);const srl_status status=input.status;srl_xml_input_free(&input);if(!good){" + n + "_free(&candidate);return initialized!=srl_ok?initialized:status==srl_ok?srl_invalid:status;}" + n + "_free(result);*result=candidate;return srl_ok;");
    close();
  }
  line("/* Preserve existing native policy signatures while checking complete keyref scopes before publication. */");
  open("static inline srl_status " + n + "_decode_xml_with_all_policies(" + n + "* result,const uint8_t* bytes,size_t size,const srl_limits* limits" + validator_parameter + ",const srl_xml_map_limits* map_limits,const srl_xml_mixed_limits* mixed_limits,const srl_xml_digest_limits* digest_limits)");
  if (extension_graph) {
    line("(void)result;(void)bytes;(void)size;(void)limits;(void)map_limits;(void)mixed_limits;(void)digest_limits;");
    if (options_.runtime_xsd) { line("(void)validation;"); }
    line("return srl_invalid;");
  } else { line("return " + n + "_srl_xml_decode_known(result,bytes,size,limits" + (options_.runtime_xsd ? ",validation" : "") + ",map_limits,mixed_limits,digest_limits,NULL,true,NULL);"); }
  close();
  line("/* Select independent key/unique/keyref tuple/index admission before replacing an initialized owner. */");
  open("static inline srl_status " + n + "_decode_xml_with_identity_limits(" + n + "* result,const uint8_t* bytes,size_t size,const srl_limits* limits" + validator_parameter + ",const srl_xml_identity_limits* identity_limits)");
  if (extension_graph) {
    line("(void)result;(void)bytes;(void)size;(void)limits;(void)identity_limits;");
    if (options_.runtime_xsd) { line("(void)validation;"); }
    line("return srl_invalid;");
  } else { line("return " + n + "_srl_xml_decode_known(result,bytes,size,limits" + (options_.runtime_xsd ? ",validation" : "") + ",NULL,NULL,NULL,identity_limits,true,NULL);"); }
  close();
  line("/* Preserve existing policy arities while forwarding default digest admission. */");
  open("static inline srl_status " + n + "_decode_xml_with_policies(" + n + "* result, const uint8_t* bytes, size_t size, const srl_limits* limits" + validator_parameter + ", const srl_xml_map_limits* map_limits, const srl_xml_mixed_limits* mixed_limits)");
  line("return " + n + "_decode_xml_with_all_policies(result,bytes,size,limits" + (options_.runtime_xsd ? ",validation" : "") + ",map_limits,mixed_limits,NULL);"); close();
  line("/* Select independent decoded digest byte admission through every nested field. */");
  open("static inline srl_status " + n + "_decode_xml_with_digest_limits(" + n + "* result, const uint8_t* bytes, size_t size, const srl_limits* limits" + validator_parameter + ", const srl_xml_digest_limits* digest_limits)");
  line("return " + n + "_decode_xml_with_all_policies(result,bytes,size,limits" + (options_.runtime_xsd ? ",validation" : "") + ",NULL,NULL,digest_limits);"); close();
  line("/* Keep the existing map-policy entrypoint and default nested mixed-content admission. */");
  open("static inline srl_status " + n + "_decode_xml_with_map_limits(" + n + "* result, const uint8_t* bytes, size_t size, const srl_limits* limits" + validator_parameter + ", const srl_xml_map_limits* map_limits)");
  line("return " + n + "_decode_xml_with_policies(result,bytes,size,limits" + (options_.runtime_xsd ? ",validation" : "") + ",map_limits,NULL);"); close();
  line("/* Decode one bounded ordered fragment sequence before replacing the destination. */");
  open("static inline srl_status " + n + "_decode_xml_with_mixed_limits(" + n + "* result, const uint8_t* bytes, size_t size, const srl_limits* limits" + validator_parameter + ", const srl_xml_mixed_limits* mixed_limits)");
  line("return " + n + "_decode_xml_with_policies(result,bytes,size,limits" + (options_.runtime_xsd ? ",validation" : "") + ",NULL,mixed_limits);"); close();
  line("/* Preserve transactional decoding and the existing default map admission contract. */");
  open("static inline srl_status " + n + "_decode_xml(" + n + "* result, const uint8_t* bytes, size_t size, const srl_limits* limits" + validator_parameter + ")");
  line("return " + n + "_decode_xml_with_map_limits(result,bytes,size,limits" + (options_.runtime_xsd ? ",validation" : "") + ",NULL);"); close();
  }
  line("/* Operation-owned codec descriptors introduce no authored-model suffix names. */");
  open("static inline srl_status srl_xml_operation_codec_encode_" + n + "(const void* value,srl_buffer* result,const srl_limits* limits,const srl_xml_validation* validation,srl_xml_operation* operation)");
  line("(void)validation;");
  if (extension_graph) {
    line("(void)value;(void)result;(void)limits;return srl_xml_operation_fail(operation,srl_invalid);");
  } else {
    line("const srl_xml_operation_policies* policies=operation?operation->policies:NULL;");
    line("return srl_xml_operation_encode_" + n + "((const " + n + "*)value,result,limits" + (options_.runtime_xsd ? ",validation" : "") + ",policies?policies->map:NULL,policies?policies->mixed:NULL,policies?policies->digest:NULL,policies?policies->identity:NULL,true,NULL,operation);");
  }
  close();
  if (!output_only) {
    for (const bool package : {false, true}) {
      line("/* Decode a private typed candidate, optionally accepting its exact package root type. */");
      open("static inline srl_status srl_xml_operation_codec_decode_" + std::string{package ? "package_" : ""} + n + "(void* value,const uint8_t* bytes,size_t size,const srl_limits* limits,const srl_xml_validation* validation,srl_xml_operation* operation)");
      line("(void)validation;");
      if (extension_graph || export_graph) {
        line("(void)value;(void)bytes;(void)size;(void)limits;return srl_xml_operation_fail(operation,srl_invalid);");
      } else {
        line("const srl_xml_operation_policies* policies=operation?operation->policies:NULL;");
        line("return srl_xml_operation_decode_" + n + "((" + n + "*)value,bytes,size,limits" + (options_.runtime_xsd ? ",validation" : "") + ",policies?policies->map:NULL,policies?policies->mixed:NULL,policies?policies->digest:NULL,policies?policies->identity:NULL,true,NULL,operation," + (package ? "true" : "false") + ");");
      }
      close();
    }
  }
  line("/* Return immutable compiler-owned function pointers; caller operations own every mutable value. */");
  open("static inline const srl_xml_operation_codec* srl_xml_operation_codec_" + n + "(void)");
  line("static const srl_xml_operation_codec value={srl_xml_operation_codec_encode_" + n + ',' +
      (output_only ? "NULL,NULL" : "srl_xml_operation_codec_decode_" + n + ",srl_xml_operation_codec_decode_package_" + n) + "};return &value;");
  close();
  if (extension_graph) { line(c_extension_frames(n, options_)); }
  output_.insert(output_start, reference_helpers);
}

// Build target-native support and direct bindings after the owning model declarations.
std::string generator::generate() {
  if (language_ != "c" && language_ != "swift" && language_ != "kotlin") {
    throw std::invalid_argument{"Unsupported native XML generator target"};
  }
  output_ = (c_ ? c_scalar_constants() + native_identity_constants(false) + native_extension_constants(false) : mobile_identity_constants(swift_) + mobile_extension_constants(swift_)) +
      std::string{c_ ? c_runtime : swift_ ? swift_runtime : kotlin_runtime};
  if (!c_) { output_ += mobile_scalar_constants(swift_); }
  std::set<const class_node*> mixed_storage_only{};
  {
    for (const auto* node : model_.nodes) {
      if (node->type != object_type::class_type) { continue; }
      const auto& owner = static_cast<const class_node&>(*node);
      const auto configured = options_.classes.find(owner.get_full_name());
      if (configured == options_.classes.end() || !configured->second.contracts.contains("mixed")) { continue; }
      const auto contract = bindings_.bind(owner);
      if (contract.mixed && !options_.classes.contains(contract.mixed->fragment->get_full_name())) {
        mixed_storage_only.insert(contract.mixed->fragment);
      }
    }
  }
  std::set<std::string> reserved_runtime_names{"XmlLimits", "XmlMapLimits", "XmlMixedLimits", "XmlDigestLimits", "XmlIdentityLimits", "SrlXmlIdentityPath", "SrlXmlIdentityField", "SrlXmlIdentityConstraint", "SrlXmlIdentityScope", "SrlXmlIdentity", "XmlValidator", "PreparedXmlSchema", "Html", "HtmlLimits", "HtmlEvent", "HtmlAttribute", "SrlXmlRule", "SrlXmlScalarLimit", "SrlXmlScalarWork", "SrlXmlName",
      "SrlXmlAttribute", "SrlXmlNode", "SrlXmlReader", "SrlXmlParser", "SrlXmlBudget", "SrlXmlOutput"};
  std::set<std::string> reserved_runtime_functions{"srlXmlEscape", "srlXmlWrite", "srlXmlParse", "srlXmlHasDoctype",
      "srlXmlText", "srlXmlBoolean", "srlXmlFloat", "srlXmlFloat32", "srlXmlFloatText", "srlXmlInteger",
      "srlXmlIntegerText", "srlXmlUtf8Size", "srlXmlWhitespace", "srlXmlNameStart", "srlXmlNcName", "srlXmlTypeQName", "srlXmlVariantType", "srlXmlReadList", "srlXmlRevisionLexical", "srlXmlHexDigit", "srlXmlDigestRead", "srlXmlDigestText"};
  std::set<std::string> reserved_mobile_methods{"encodeXml", "decodeXml", "srlXmlFill", "srlXmlRead", "srlXmlEmit"};
  if (swift_) {
    reserved_runtime_names.insert({"SrlXmlValueBox", "SrlXmlTokenFrame", "SrlXmlInputAttributes", "SrlXmlTokenParser", "SrlXmlLexicalAttributes"});
    reserved_runtime_functions.insert({"srlXmlProviderFailure", "srlXmlPresence", "srlXmlScalarFrame", "srlXmlTokenPreflight", "srlXmlDecodeTokens", "srlXmlForeignFrame"});
    reserved_mobile_methods.insert("srlXmlFrame");
  } else if (!c_) {
    reserved_runtime_names.insert({"SrlXmlInput", "SrlXmlInputFields"});
    reserved_runtime_functions.insert({"srlXmlCheckUtf8", "srlXmlTokenCheck", "getSrlXmlNilName"});
  }
  if (!c_) {
    reserved_runtime_names.insert({"XmlExtensionLimits", "XmlExtensionOptions", "SrlXmlExtensionSlot", "SrlXmlExtensionContract",
        "SrlXmlForeignContent", "SrlXmlForeignNode", "SrlXmlForeignChild", "SrlXmlPreserved", "SrlXmlExtensions",
        "SrlXmlNamespaceState", "SrlXmlNamespaceOutput"});
    reserved_runtime_functions.insert({"srlXmlExtensionBindings", "srlXmlNamespaceChecked", "srlXmlNamespaceScalarFrame"});
    reserved_mobile_methods.insert("xmlBindNamespace");
  }
  // These additional Foundation/provider types are referenced unqualified only by XML support.
  const std::set<std::string> swift_provider_names{"XMLParser", "XMLParserDelegate", "NSObject", "NSError", "CharacterSet"};
  std::set<std::string> generated_c_functions{};
  std::set<std::string> generated_enum_functions{};
  for (const auto* node : model_.nodes) {
    const auto n = native::snake(model_.names.at(node));
    if (node->type == object_type::class_type) {
      if (mixed_storage_only.contains(static_cast<const class_node*>(node))) { continue; }
      for (const auto& method : static_cast<const class_node&>(*node).conversions) {
        if (!method.translator) { continue; }
        const auto view = translator::view_name(n, method, [&](auto name, auto) { return field_name(name); }, "c");
        for (const auto suffix : {"_encode_xml", "_encode_xml_with_map_limits", "_encode_xml_with_mixed_limits",
             "_encode_xml_with_digest_limits", "_encode_xml_with_policies", "_encode_xml_with_all_policies", "_srl_xml_write"}) {
          generated_c_functions.insert(view + suffix);
        }
      }
      generated_c_functions.insert({n + "_encode_xml", n + "_decode_xml", n + "_encode_xml_with_map_limits", n + "_decode_xml_with_map_limits",
          n + "_encode_xml_with_digest_limits", n + "_decode_xml_with_digest_limits", n + "_encode_xml_with_all_policies", n + "_decode_xml_with_all_policies",
          n + "_encode_xml_with_identity_limits", n + "_decode_xml_with_identity_limits", n + "_srl_xml_identity_check", n + "_srl_xml_encode_known", n + "_srl_xml_decode_known",
          n + "_encode_xml_with_mixed_limits", n + "_decode_xml_with_mixed_limits", n + "_encode_xml_with_policies", n + "_decode_xml_with_policies",
          n + "_srl_xml_write", n + "_srl_xml_read", n + "_srl_xml_read_tokens"});
      if (!model_.packages.empty()) { generated_c_functions.insert(n + "_srl_xml_decode_package"); }
      if (native_extension_graph(static_cast<const class_node&>(*node), bindings_)) {
        generated_c_functions.insert({n + "_xml_document", n + "_xml_document_init", n + "_xml_document_free",
            n + "_decode_xml_document", n + "_encode_xml_document", n + "_srl_xml_extension_model"});
      }
    } else if (node->type == object_type::enum_type) {
      generated_enum_functions.insert("srlXml" + model_.names.at(node) + "Enum");
    }
  }
  const auto check_c_name = [&](const std::string& name) {
    if (name.starts_with("srl_xml_") || name.starts_with("srl_ext_") || name.starts_with("srl_html_") || generated_c_functions.contains(name)) {
      throw std::invalid_argument{"XML generated C symbol conflicts with schema declaration: " + name};
    }
  };
  for (const auto* node : model_.nodes) {
    if (!c_ && node->type != object_type::function_type && reserved_runtime_names.contains(model_.names.at(node))) {
      throw std::invalid_argument{"XML runtime name conflicts with native declaration: " + model_.names.at(node)};
    }
    if (swift_ && node->type != object_type::function_type && swift_provider_names.contains(model_.names.at(node))) {
      throw std::invalid_argument{"XML provider name conflicts with Swift declaration: " + model_.names.at(node)};
    }
    if (c_) {
      const auto n = native::snake(model_.names.at(node));
      check_c_name(n);
      if (node->type == object_type::enum_type) {
        for (const auto& enumerator : static_cast<const enum_node&>(*node).enum_name_list) {
          check_c_name(n + "_" + field_name(enumerator));
        }
      }
    } else if (node->type == object_type::function_type) {
      const auto generated_name = field_name(model_.names.at(node));
      if (reserved_runtime_functions.contains(generated_name) || generated_enum_functions.contains(generated_name)) {
        throw std::invalid_argument{"XML runtime helper conflicts with authored function: " + generated_name};
      }
    }
    if (node->type != object_type::class_type) { continue; }
    const auto& source = static_cast<const class_node&>(*node);
    const auto n = native::snake(model_.names.at(node));
    std::set<std::string> mobile_names = reserved_mobile_methods;
    for (const auto& field : source.member_list) {
      if (!swift_ && !c_ && !field.omits("xml")) {
        const auto bridge = "srlXmlField" + native::pascal(field.name);
        mobile_names.insert({bridge, "get" + native::pascal(bridge), "set" + native::pascal(bridge)});
      }
    }
    for (const auto& field : source.member_list) {
      if (!c_ && mobile_names.contains(field_name(field.name))) {
        throw std::invalid_argument{"XML generated API conflicts with native field: " + field.name};
      }
    }
    const auto check_method = [&](std::string_view name) {
      if (c_) { check_c_name(n + "_" + field_name(name)); }
      else if (mobile_names.contains(field_name(name))) {
        throw std::invalid_argument{"XML generated API conflicts with native callable: " + std::string{name}};
      }
    };
    for (const auto& method : source.functions) {
      check_method(method.name);
    }
    for (const auto& factory : source.constructions) { if (!factory.constructor) { check_method(factory.name); } }
    for (const auto& conversion : source.conversions) { check_method(conversion.name); }
  }
  if (!c_) {
    if (swift_) {
      output_ += R"swift(
// Applications may provide an explicit trusted prepared XSD validator when enabled in generation.
public protocol XmlValidator { func validate(_ bytes: [UInt8], _ limits: XmlLimits) throws }
// Parse exact integer lexical values and reject representational overflow.
fileprivate func srlXmlInteger<T: FixedWidthInteger>(_ text: String, _ type: T.Type) throws -> T {
    let value = text.trimmingCharacters(in: CharacterSet(charactersIn: " \t\r\n"))
    guard value.range(of: "^[+-]?[0-9]+$", options: .regularExpression) != nil else { throw SerializerError.invalid("Invalid XML integer") }
    let normalized = value.hasPrefix("-") && value.dropFirst().allSatisfy({ $0 == "0" }) ? "0" : value
    guard let result = T(normalized) else { throw SerializerError.invalid("Overflowing XML integer") }
    return result
}
// Retain canonical IEEE special values while bounding binary32 conversion.
fileprivate func srlXmlFloat32(_ text: String) throws -> Float {
    let number = try srlXmlFloat(text)
    if !number.isFinite { return Float(number) }
    let lexical = text.trimmingCharacters(in: CharacterSet(charactersIn: " \t\r\n"))
    guard let result = Float(lexical) else { throw SerializerError.invalid("Invalid XML binary32") }
    guard !number.isFinite || result.isFinite else { throw SerializerError.invalid("XML binary32 overflow") }
    return result
}
// Use XSD spellings for nonfinite values and Swift's round-trip finite formatting.
fileprivate func srlXmlFloatText(_ number: Double) -> String {
    if number.isNaN { return "NaN" }; if number.isInfinite { return number < 0 ? "-INF" : "INF" }; return String(number)
}
// Keep binary32 output short instead of formatting an artificially widened binary64 value.
fileprivate func srlXmlFloatText(_ number: Float) -> String {
    if number.isNaN { return "NaN" }; if number.isInfinite { return number < 0 ? "-INF" : "INF" }; return String(number)
}
)swift";
    } else {
      output_ += R"kotlin(
// Runtime schema validation is explicit and can reuse one trusted prepared provider schema.
fun interface XmlValidator { fun validate(bytes: ByteArray, limits: XmlLimits) }
// Prepare a standalone trusted XSD once; external DTD/schema resolution is prohibited.
class PreparedXmlSchema(bytes: ByteArray, limits: XmlLimits = XmlLimits()) : XmlValidator {
    private val schema: javax.xml.validation.Schema
    init {
        val pending = mutableListOf(srlXmlParse(bytes, limits))
        var index = 0
        while (index < pending.size) {
            val node = pending[index++]
            require(node.name.namespace != "http://www.w3.org/2001/XMLSchema" ||
                node.name.local !in setOf("include", "import", "redefine", "override")) { "Runtime XSD external references are unsupported" }
            pending.addAll(node.children)
        }
        val factory = javax.xml.validation.SchemaFactory.newInstance(javax.xml.XMLConstants.W3C_XML_SCHEMA_NS_URI)
        factory.setFeature(javax.xml.XMLConstants.FEATURE_SECURE_PROCESSING, true)
        factory.setProperty(javax.xml.XMLConstants.ACCESS_EXTERNAL_DTD, "")
        factory.setProperty(javax.xml.XMLConstants.ACCESS_EXTERNAL_SCHEMA, "")
        factory.resourceResolver = org.w3c.dom.ls.LSResourceResolver { _, _, _, _, _ -> throw IllegalArgumentException("External XSD references prohibited") }
        schema = factory.newSchema(javax.xml.transform.stream.StreamSource(java.io.ByteArrayInputStream(bytes)))
    }
    override fun validate(bytes: ByteArray, limits: XmlLimits) {
        SrlXmlInput(bytes, limits).use { input ->
            while (input.kind != '\u0000') input.advance()
            input.finish()
        }
        val validator = schema.newValidator()
        validator.setProperty(javax.xml.XMLConstants.ACCESS_EXTERNAL_DTD, "")
        validator.setProperty(javax.xml.XMLConstants.ACCESS_EXTERNAL_SCHEMA, "")
        validator.validate(javax.xml.transform.stream.StreamSource(java.io.ByteArrayInputStream(bytes)))
    }
}
// Exact XML integer grammar is checked before native width/range conversion.
internal fun srlXmlIntegerText(text: String): String {
    val value = text.trim(' ', '\t', '\r', '\n')
    require(Regex("[+-]?[0-9]+").matches(value)) { "Invalid XML integer" }
    return if (value.startsWith('-') && value.drop(1).all { it == '0' }) "0" else value
}
// Reject finite-to-infinite narrowing when binding XML binary32 values.
internal fun srlXmlFloat32(text: String): Float {
    val number = srlXmlFloat(text)
    if (!number.isFinite()) return number.toFloat()
    val result = text.trim(' ', '\t', '\r', '\n').toFloat()
    require(!number.isFinite() || result.isFinite()) { "XML binary32 overflow" }; return result
}
// Emit canonical XSD special-value spellings.
internal fun srlXmlFloatText(number: Double): String = when {
    number.isNaN() -> "NaN"
    number == Double.POSITIVE_INFINITY -> "INF"
    number == Double.NEGATIVE_INFINITY -> "-INF"
    else -> number.toString()
}
// Preserve the shortest native binary32 representation without widening its lexical output.
internal fun srlXmlFloatText(number: Float): String = when {
    number.isNaN() -> "NaN"
    number == Float.POSITIVE_INFINITY -> "INF"
    number == Float.NEGATIVE_INFINITY -> "-INF"
    else -> number.toString()
}
)kotlin";
    }
  }
  if (c_) {
    for (const auto* node : model_.nodes) {
      if (node->type != object_type::class_type) { continue; }
      if (mixed_storage_only.contains(static_cast<const class_node*>(node))) { continue; }
      const auto n = native::snake(model_.names.at(node));
      line("/* Internal XML declarations support nested owning records. */");
      line("static inline bool " + n + "_srl_xml_write(xmlTextWriterPtr writer, srl_xml_output* out, const " + n + "* value, const char* local, const char* uri);");
      line("static inline bool srl_xml_operation_write_" + n + "(xmlTextWriterPtr writer,srl_xml_output* out,const " + n + "* value,const char* local,const char* uri,srl_xml_operation* operation);");
      line("static inline bool " + n + "_srl_xml_read(xmlNodePtr node, " + n + "* value, const srl_limits* limits);");
      line("static inline bool " + n + "_srl_xml_read_tokens(srl_xml_input* input, " + n + "* value, const srl_limits* limits, const char* local, const char* uri);");
      if (native_extension_graph(static_cast<const class_node&>(*node), bindings_)) {
        line("static inline const srl_xml_extension_model* " + n + "_srl_xml_extension_model(void);");
      }
    }
  }
  for (const auto* node : model_.nodes) {
    if (!c_ && node->type == object_type::enum_type) {
      const auto n = model_.names.at(node);
      if (swift_) {
        open("fileprivate func srlXml" + n + "Enum(_ text: String) throws -> " + n);
        line("guard let index = " + n + ".srlNames.firstIndex(of: text), let result = " + n + "(rawValue: index) else { throw SerializerError.invalid(\"Unknown XML enum\") }");
        line("return result"); close();
      } else {
        open("internal fun srlXml" + n + "Enum(text: String): " + n);
        line("val index = " + n + ".srlNames.indexOf(text)");
        line("require(index >= 0) { \"Unknown XML enum\" }; return " + n + ".entries[index]"); close();
      }
    }
    if (node->type != object_type::class_type) { continue; }
    const auto& source = static_cast<const class_node&>(*node);
    if (mixed_storage_only.contains(&source)) { continue; }
    auto contract = bindings_.bind(source);
    if (c_) {
      // Native value-reference handles contain process-local fields; only the explicitly borrowed payload is XML data.
      if (source.reference) { contract.fields.clear(); }
      if (source.reference && source.reference->value_type.type == object_type::class_type) {
        contract.root = bindings_.bind(static_cast<const class_node&>(*source.reference->value_type.resolved_node)).root;
      }
      c_record(source, contract);
    } else {
      if (source.reference) {
        contract.fields.clear();
        if (source.reference->value_type.type == object_type::class_type) {
          contract.root = bindings_.bind(static_cast<const class_node&>(*source.reference->value_type.resolved_node)).root;
        }
      }
      mobile_record(source, contract);
    }
  }
  if (c_) {
    for (const auto* node : model_.nodes) {
      if (node->type != object_type::class_type) { continue; }
      const auto& source = static_cast<const class_node&>(*node);
      for (const auto& method : source.conversions) {
        if (!method.translator) { continue; }
        const auto& target = ::rohit::serializer::conversion::record(method.target);
        const auto target_contract = bindings_.bind(target);
        if (std::any_of(target_contract.fields.begin(), target_contract.fields.end(), [](const auto& field) { return field.variant_binding.has_value(); })) {
          throw std::invalid_argument{"Readonly XML translators require a native target getter for the complete variant storage"};
        }
        const auto view = translator::view_name(native::snake(model_.names.at(node)), method,
            [&](auto name, auto) { return field_name(name); }, "c");
        const auto slots = translator::slots(method, "c", [&](auto name, auto) { return field_name(name); });
        c_record(target, target_contract, view, [&](const member& field) {
          const auto path = member_name(field);
          const auto separator = path.find('.');
          const auto root = path.substr(0, separator);
          const auto found = std::find_if(slots.begin(), slots.end(), [&](const auto& slot) { return field_name(slot.field.name) == root; });
          if (found == slots.end()) { throw std::logic_error{"Readonly XML translator getter is absent"}; }
          const auto getter = view + '_' + field_name("get_" + found->field.name) + "(value)";
          return separator == std::string::npos ? "(*" + getter + ")" : "(" + getter + "->" + path.substr(separator+1U) + ")";
        });
      }
    }
  }
  if (!c_) {
    for (const auto* node : model_.nodes) {
      if (node->type != object_type::class_type) { continue; }
      const auto& source = static_cast<const class_node&>(*node);
      for (const auto& method : source.conversions) {
        if (method.translator) { mobile_translator_record(source, method); }
      }
    }
  }
  return output_;
}
} // namespace

std::string generate_native(const native::schema& model, const xml_options& options, std::string_view language) {
  if (!options.enabled) { return {}; }
  return generator{model, options, language}.generate();
}
} // namespace rohit::serializer::writer::xml_codegen
