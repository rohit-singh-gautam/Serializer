#include "schema_projection.hpp"
#include "native_schema.hpp"
#include "version_writer.hpp"
#include "behavior_writer.hpp"
#include "predicate_helpers.hpp"
#include "statement_behavior_writer.hpp"
#include "auto_size_writer.hpp"
#include "construction_writer.hpp"
#include "conversion_writer.hpp"
#include "translator_writer.hpp"
#include "reference_portable_writer.hpp"
#include "comparison_writer.hpp"
#include "xml_schema.hpp"
#include "python_package_writer.hpp"
#include "python_xml_writer.hpp"
#include "python_xml_identity.hpp"
#include <rohit/digest.hpp>

#include <functional>
#include <algorithm>
#include <iterator>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace rohit::serializer::writer::native {
namespace {
constexpr std::string_view runtime =
#include "python_runtime.inc"
    ;
constexpr std::string_view compression_runtime =
#include "python_compression_runtime.inc"
    ;
constexpr std::string_view xml_runtime =
#include "python_xml_runtime.inc"
    ;
constexpr std::string_view package_runtime =
#include "python_package_runtime.inc"
    ;
constexpr std::string_view package_mce_runtime =
#include "python_package_mce_runtime.inc"
    ;
constexpr std::string_view xml_rich_runtime =
#include "python_xml_rich_runtime.inc"
    ;
constexpr std::string_view html_runtime =
#include "python_html_runtime.inc"
    ;
constexpr std::string_view xml_identity_runtime =
#include "python_xml_identity_runtime.inc"
    ;
constexpr std::string_view xml_scalar_runtime =
#include "python_xml_scalar_runtime.inc"
;
constexpr std::string_view xml_model_runtime =
#include "python_xml_model_runtime.inc"
;
constexpr std::string_view xml_extension_runtime =
#include "python_xml_extension_runtime.inc"
;
constexpr std::string_view xml_projection_runtime =
#include "python_xml_projection_runtime.inc"
;
constexpr std::string_view keywords =
    " False None True and as assert async await break class continue def del elif else except "
    "finally for from global if import in is lambda nonlocal not or pass raise return try while "
    "with yield match case self cls ";

// Emit direct Python field operations without a generic intermediate object tree.
class python_emitter {
  const schema& model;
  xml_binding::schema xml_contract;
  behavior::checked_helpers arithmetic_helpers{};
  std::string output{runtime};
  std::size_t indent{};
  bool dynamic_first{};
  std::set<const class_node*> mixed_fragments{};
  // Resolve the scalar/record beneath a supported optional or repeated XML field.
  const type_name& xml_leaf_type(const type_name& value) const {
    const auto* selected = &value;
    std::set<const syntax_node*> references;
    while (true) {
      while (selected->is_optional() || selected->is_sequence()) { selected = &selected->arguments.front(); }
      if (selected->type == object_type::class_type) {
        const auto& owner = static_cast<const class_node&>(*selected->resolved_node);
        if (owner.reference && owner.reference->value_projection) {
          if (!references.insert(&owner).second) {
            throw std::invalid_argument{"Cyclic readonly XML value projection"};
          }
          selected = &owner.reference->value_type;
          continue;
        }
      }
      return *selected;
    }
  }
  // Resolve a field's effective XML value without replacing its native reference/container storage.
  const type_name& xml_leaf(const member& value) const {
    return xml_leaf_type(value.type_name_list.front());
  }
  // Identify readonly handles beneath native optional/repeated layers before their values are borrowed.
  bool xml_value_projection(const type_name& value) const {
    const auto* selected = &value;
    while (selected->is_optional() || selected->is_sequence()) { selected = &selected->arguments.front(); }
    if (selected->type != object_type::class_type) { return false; }
    const auto& owner = static_cast<const class_node&>(*selected->resolved_node);
    return owner.reference && owner.reference->value_projection;
  }
  // Resolve a flattened XML member to the existing composed Python base-object storage path.
  std::string xml_member_path(const class_node& owner, const member& selected) const {
    const auto locate = [&](auto&& self, const class_node& current, const std::string& prefix) -> std::string {
      for (const auto& item : current.member_list) {
        if (&item == &selected) { return prefix + field(item.name, item.access); }
      }
      for (std::size_t index{}; index < current.parents.size(); ++index) {
        const auto& base = current.parents[index];
        if (base.parent_class) {
          auto result = self(self, *base.parent_class,
              prefix + field("base" + std::to_string(index), base.access) + '.');
          if (!result.empty()) { return result; }
        }
      }
      return {};
    };
    auto result = locate(locate, owner, {});
    if (result.empty()) { throw std::invalid_argument{"Flattened Python XML member has no owning path"}; }
    return result;
  }
  // Convert one XML scalar using exact generated type and numeric bounds.
  std::string xml_scalar(const type_name& value, const std::string& expression, bool encoding) const {
    if (value.is_digest()) {
      const auto extent = value.digest_extent ? value.digest_extent : rohit::digest_size(value.digest);
      return "_xml_digest(" + expression + ", " + std::to_string(extent) + ", " +
          (encoding ? "True" : "False") + ")";
    }
    if (value.type == object_type::enum_type) {
      const auto& name = model.names.at(value.resolved_node);
      return encoding ? "_xml_enum(" + expression + ", _" + name + "_names, " + name + ")" :
          name + "(_" + name + "_names.index(" + expression + "))";
    }
    const auto kind = value.name == "string" ? "string" : value.name == "bool" ? "bool" :
        value.name == "float" || value.name == "float32" ? "float" :
        value.name == "double" || value.name == "float64" ? "double" : "integer";
    return "_xml_scalar(" + expression + ", " + quote(kind) + ", " +
        std::to_string(width(value)) + ", " + (value.name.starts_with('u') ? "True" : "False") +
        ", " + (encoding ? "True" : "False") + ")";
  }
  // Emit a required map payload's static owning codec without an intermediate XML representation.
  std::string xml_token_value(const type_name& value, const xml::name& name, bool attribute) const {
    const auto expanded = quote(name.namespace_uri.empty() ? name.local : "{" + name.namespace_uri + "}" + name.local);
    const auto& leaf = xml_leaf_type(value);
    const bool record = leaf.type == object_type::class_type;
    return "{'name': " + expanded + ", 'mode': " + quote(attribute ? "attribute" : "child") +
        ", 'wrapper': None, 'minimum': 1, 'maximum': 1, 'optional': False, 'presence': None, "
        "'repeated': False, 'extent': 0, 'default': None, 'map': None, 'variant': None, 'projection_payload': " +
        (xml_value_projection(value) ? "True" : "False") + ", 'model': " +
        (record ? "lambda: " + model.names.at(leaf.resolved_node) : "None") +
        ", 'read': " + (record ? "None" : "lambda text, scope, context: " + xml_scalar(leaf, "text", false)) +
        ", 'write': " + (record ? "None" : "lambda value, scope, context: " + xml_scalar(leaf, "value", true)) + "}";
  }
  // Export readonly record handles through their original target contract; input cannot invent an owner binding.
  void xml_reference(const class_node& value) {
    const auto& selected = xml_leaf_type(value.reference->value_type);
    const bool record = selected.type == object_type::class_type;
    line("_xml_export_only = True");
    line("_xml_token_supported = False");
    line("_xml_token_fields = _xml_token_all = ()");
    line("_xml_identity = _xml_mixed = _xml_extensions = None");
    line("_xml_token_inherited = False");
    line("_xml_model = {'required': False}");
    open("def encode_xml(self, limits=None, schema=None, context=None)");
    line("\"\"\"Borrow the selected owning record during one synchronous XML export.\"\"\"");
    if (record) { line("return self.read_value().encode_xml(limits, schema, context)"); }
    else { line("raise ValueError('Scalar XML references require their owning field mapping')"); }
    --indent;
    open("def _xml_node(self, name, budget, depth)");
    line("\"\"\"Reuse the selected value's declared mapping without constructing an owning target.\"\"\"");
    if (record) { line("return self.read_value()._xml_node(name, budget, depth)"); }
    else {
      line("node = budget.node(name, depth)");
      line("node[3] = " + xml_scalar(selected, "self.read_value()", true));
      line("return node");
    }
    --indent;
    for (const auto* method : {"decode_xml(cls, *args, **kwargs)", "decode_xml_chunks(cls, *args, **kwargs)", "_xml_from_node(cls, *args, **kwargs)"}) {
      line("@classmethod"); open("def " + std::string{method});
      line("\"\"\"Decode the owning target instead; readonly projections cannot bind an external owner from XML.\"\"\"");
      line("raise ValueError('Readonly XML projections are export-only; decode the owning target')");
      --indent;
    }
  }
  // Emit whole-object XML entry points and direct static member mapping for one owning record.
  void xml_object(const class_node& value) {
    if (!model.xml.enabled) { return; }
    if (value.reference && value.reference->value_projection) { xml_reference(value); return; }
    if (mixed_fragments.contains(&value)) {
      line("_xml_token_supported = False");
      line("_xml_token_fields = _xml_token_all = ()");
      line("_xml_identity = _xml_mixed = None");
      line("_xml_model = {'required': False}");
      line("_xml_extensions = None");
      line("_xml_token_inherited = False");
      for (const auto* method : {"encode_xml(self, *args, **kwargs)", "_xml_node(self, *args, **kwargs)"}) {
        open("def " + std::string{method});
        line("\"\"\"Mixed fragment storage has no independent XML root contract.\"\"\"");
        line("raise ValueError('Mixed fragment storage has no standalone XML codec')");
        --indent;
      }
      for (const auto* method : {"decode_xml(cls, *args, **kwargs)", "decode_xml_chunks(cls, *args, **kwargs)", "_xml_from_node(cls, *args, **kwargs)"}) {
        line("@classmethod");
        open("def " + std::string{method});
        line("\"\"\"Require the owning mixed-content record's declared XML contract.\"\"\"");
        line("raise ValueError('Mixed fragment storage has no standalone XML codec')");
        --indent;
      }
      return;
    }
    const auto contract = xml_contract.bind(value);
    const auto name = model.names.at(&value);
    const auto expanded = [&](const xml::name& item) {
      return quote(item.namespace_uri.empty() ? item.local : "{" + item.namespace_uri + "}" + item.local);
    };
    // Generated methods and authored members use the same Python class namespace.
    const auto reject_method = [&](const std::string& selected) {
      if (selected == "encode_xml" || selected == "decode_xml" || selected == "_xml_node" || selected == "_xml_from_node" ||
          selected == "_srl_xml_state" || selected == "xml_bind_namespace" || selected == "_xml_rules" ||
          selected == "_xml_record" || selected == "_xml_choices" || selected == "decode_xml_chunks" ||
          selected == "_xml_token_fields" || selected == "_xml_token_supported" || selected == "_xml_token_all" ||
          selected == "_xml_identity" || selected == "_xml_token_inherited" || selected == "_xml_mixed" ||
          selected == "_xml_model" || selected == "_xml_extensions" || selected == "_xml_export_only") {
        throw std::invalid_argument{"Python XML method collides with declared member: " + selected};
      }
    };
    for (const auto& item : value.member_list) { reject_method(field(item.name, item.access)); }
    for (const auto& method : value.functions) { reject_method(field(method.name, method.access)); }
    for (const auto& factory : value.constructions) {
      reject_method(field(factory.constructor ? "create" : factory.name, factory.access));
    }
    for (const auto& conversion : value.conversions) {
      reject_method(field(conversion.name, conversion.access));
    }
    line("_xml_export_only = False");
    line("_xml_rules = (");
    ++indent;
    for (const auto& item : contract.fields) { line(python_xml::field(item, xml_leaf(*item.source)) + ','); }
    --indent;
    line(")");
    line("_xml_record = " + python_xml::record(contract));
    std::string choices{"_xml_choices = ("};
    for (const auto& choice : contract.choices) {
      if (choice.repeated) { continue; }
      choices += "((";
      for (const auto* alternative : choice.alternatives) { choices += quote(xml_member_path(value, *alternative)) + ','; }
      const auto empty = std::any_of(choice.branches.begin(), choice.branches.end(),
          [](const auto& branch) { return branch.minimum == 0U; });
      choices += "), " + std::to_string(empty ? 0U : choice.minimum) + ", " + std::to_string(choice.maximum) + "),";
    }
    line(choices + ")");
    bool token_supported = !python_xml::needs_namespace_state(contract) && contract.wildcards.empty() &&
        (!contract.unknown || *contract.unknown == xml_unknown_policy::reject) &&
        std::none_of(contract.choices.begin(), contract.choices.end(),
            [](const auto& selected) { return selected.repeated; });
    line("_xml_token_fields = (");
    ++indent;
    std::size_t token_index{};
    for (const auto& item : contract.fields) {
      const auto& source_field = *item.source;
      const auto& leaf = xml_leaf(source_field);
      const bool optional = source_field.type_name_list.front().is_optional();
      const auto* container = &source_field.type_name_list.front();
      if (container->is_optional()) { container = &container->arguments.front(); }
      const bool typed_list = item.list && (container->is_sequence() || source_field.modifier == member::modifier_type::array);
      if (item.wrapper && optional && !item.presence.explicit_policy && !item.map_binding) { token_supported = false; }
      const auto rule = name + "._xml_rules[" + std::to_string(token_index++) + "]";
      const auto checked = "_xml_checked(text, " + rule + ", scope, context)";
      const auto read = typed_list ? "[" + xml_scalar(leaf, "srl_item", false) + " for srl_item in _xml_tokens(" + checked + ")]" :
          xml_scalar(leaf, checked, false);
      const auto raw = typed_list ? "' '.join(" + xml_scalar(leaf, "srl_item", true) + " for srl_item in value)" :
          xml_scalar(leaf, "value", true);
      const bool record = leaf.type == object_type::class_type;
      std::string map_binding{"None"};
      if (item.map_binding) {
        const auto& selected = *item.map_binding;
        const bool attribute = selected.key_placement == xml_binding::map_key_placement::attribute;
        map_binding = "{'entry': " + expanded(selected.entry) + ", 'attribute': " + (attribute ? "True" : "False") +
            ", 'optional': " + (selected.optional_container ? "True" : "False") +
            ", 'key': " + xml_token_value(selected.key_type, selected.key, attribute) +
            ", 'value': " + xml_token_value(selected.value_type, selected.value, false) + "}";
      }
      std::string variant_binding{"None"};
      if (item.variant_binding) {
        const auto& selected = *item.variant_binding;
        variant_binding = "{'selection': " + quote(selected.selection == xml_binding::variant_selection::element ? "element" : "xsi_type") +
            ", 'alternatives': (";
        for (const auto& alternative : selected.alternatives) {
          auto payload = xml_token_value(alternative.native_type, alternative.tag, false);
          payload.pop_back();
          payload += ", 'member': " + quote(xml_member_path(value, source_field) + "_" + snake(alternative.alias)) +
              ", 'type': " + python_xml::name(alternative.type_identity) + ", 'variant_payload': True}";
          variant_binding += payload + ',';
        }
        variant_binding += ")}";
      }
      line("{'member': " + quote(xml_member_path(value, source_field)) + ", 'name': " + expanded(item.qualified_name) +
          ", 'mode': " + quote(item.attribute ? "attribute" : item.text ? "text" : "child") +
          ", 'wrapper': " + (item.wrapper ? expanded(*item.wrapper) : "None") +
          ", 'minimum': " + std::to_string(item.min_occurs) +
          ", 'maximum': " + (item.max_occurs ? std::to_string(*item.max_occurs) : "None") +
          ", 'optional': " + (optional ? "True" : "False") +
          ", 'presence': " + (item.presence.explicit_policy ? quote(
              item.presence.policy == xml_binding::presence_policy::required ? "required" :
              item.presence.policy == xml_binding::presence_policy::absent ? "absent" :
              item.presence.policy == xml_binding::presence_policy::nil ? "nil" : "absent_or_nil") : "None") +
          ", 'repeated': " + (item.repeated && !item.list ? "True" : "False") +
          ", 'extent': " + std::to_string(typed_list ? 0U : source_field.fixed_extent) +
          ", 'default': " + (item.fixed_value ? quote(*item.fixed_value) : item.default_value ? quote(*item.default_value) : "None") +
          ", 'created': " + (source_field.created_version.empty() ? "None" :
              python_xml::revision_value(*contract.model.revision, source_field.created_version)) +
          ", 'obsolete': " + (source_field.obsolete_version.empty() ? "None" :
              python_xml::revision_value(*contract.model.revision, source_field.obsolete_version)) +
          ", 'map': " + map_binding +
          ", 'variant': " + variant_binding +
          ", 'value_projection': " + (xml_value_projection(source_field.type_name_list.front()) ? "True" : "False") +
          ", 'model': " + (record ? "lambda: " + model.names.at(leaf.resolved_node) : "None") +
          ", 'read': " + (record ? "None" : "lambda text, scope, context: " + read) +
          ", 'write': " + (record ? "None" : "lambda value, scope, context: _xml_checked(" + raw + ", " + rule + ", scope, context)") + "},");
    }
    --indent;
    line(")");
    std::string model_binding{"_xml_model = {'revision': "};
    if (const auto* revision = contract.model.revision) {
      const auto& type = revision->type_name_list.front();
      model_binding += "{'member': " + quote(xml_member_path(value, *revision)) + ", 'kind': " + quote(type.name) +
          ", 'width': " + std::to_string(width(type)) + ", 'minimum': " +
          python_xml::revision_value(*revision, schema_version::minimum(*revision)) + ", 'current': " +
          python_xml::revision_value(*revision, revision->default_value) + '}';
    } else { model_binding += "None"; }
    model_binding += ", 'magic': " + (contract.model.magic ? quote(*contract.model.magic) : "None") +
        ", 'required': " + (contract.model.digest || contract.model.revision || contract.model.magic ? "True" : "False") + "}";
    line(model_binding);
    line("_xml_extensions = " + (contract.extensions ? python_xml::extensions(*contract.extensions) : "None"));
    line("_xml_token_supported = " + std::string{token_supported ? "True" : "False"});
    std::string all_groups{"_xml_token_all = ("};
    for (const auto& group : contract.all_groups) {
      all_groups += "((";
      for (const auto* alternative : group.alternatives) {
        const auto found = std::find_if(contract.fields.begin(), contract.fields.end(),
            [&](const auto& selected) { return selected.source == alternative; });
        all_groups += std::to_string(std::distance(contract.fields.begin(), found)) + ',';
      }
      all_groups += "), " + std::to_string(group.minimum) + "),";
    }
    line(all_groups + ")");
    line("_xml_identity = " + (contract.identity ? python_xml::identity(*contract.identity) : "None"));
    line("_xml_token_inherited = " + std::string{contract.model.inherited ? "True" : "False"});
    std::string mixed_binding{"None"};
    if (contract.mixed) {
      const auto& selected = *contract.mixed;
      const auto variant = xml_member_path(*selected.fragment, *selected.variant);
      mixed_binding = "{'member': " + quote(xml_member_path(value, *selected.source)) +
          ", 'fragment': lambda: " + model.names.at(selected.fragment) +
          ", 'index': " + quote(variant + "_index") + ", 'text_index': " + std::to_string(selected.text_alternative) +
          ", 'text_member': " + quote(variant + "_" + snake(selected.text_branch)) + ", 'children': (";
      for (const auto& child : selected.children) {
        auto payload = xml_token_value(*child.value_type, child.qualified_name, false);
        payload.pop_back();
        mixed_binding += payload + ", 'alternative': " + std::to_string(child.alternative) +
            ", 'member': " + quote(variant + "_" + snake(child.branch)) + "},";
      }
      mixed_binding += ")}";
    }
    line("_xml_mixed = " + mixed_binding);
    if (python_xml::needs_state(contract)) {
      open("def xml_bind_namespace(self, prefix, uri)");
      line("\"\"\"Bind XML-only QName/prefix-list presentation without adding native wire fields.\"\"\"");
      line("_xml_bind_namespace(self, prefix, uri)");
      --indent;
    }
    open("def encode_xml(self, limits=None, schema=None, context=None)");
    line("\"\"\"Encode one complete XML document with fixed generated mapping.\"\"\"");
    line("budget = _XmlBudget(limits, context)");
    line("context = budget.context");
    line("if not _xml_token_capable(type(self)) and _xml_token_required(type(self)): raise ValueError('This XML mapping requires a token-compatible owning graph')");
    line("data = _xml_token_encode(self, " + expanded(contract.root) + ", budget.limits, context) if _xml_token_capable(type(self)) else _xml_write(self._xml_node(" + expanded(contract.root) + ", budget, 1), budget.limits, context)");
    if (!model.xml.runtime_xsd) { line("if schema is not None: raise ValueError('Runtime XSD was not enabled during generation')"); }
    else if (model.xml.validation_scope != "input") { line("if schema is not None: schema.validate(data, budget.limits, context)"); }
    line("return data"); --indent;
    line("@classmethod");
    open("def decode_xml(cls, data, limits=None, schema=None, context=None)");
    line("\"\"\"Return a fresh model after exact XML shape and optional XSD validation.\"\"\"");
    line("if _xml_export_only(cls): raise ValueError('Readonly XML projections are export-only; decode the owning target')");
    line("context = _xml_pattern_context(limits, context)");
    if (!model.xml.runtime_xsd) { line("if schema is not None: raise ValueError('Runtime XSD was not enabled during generation')"); }
    else if (model.xml.validation_scope != "output") { line("if schema is not None: schema.validate(data, limits, context)"); }
    line("if not _xml_token_capable(cls) and _xml_token_required(cls): raise ValueError('This XML mapping requires a token-compatible owning graph')");
    line("if _xml_token_capable(cls): return _xml_token_decode(cls, " + expanded(contract.root) + ", (data,), limits, context)");
    line("context = _xml_namespace_context(limits, context)");
    line("node = _xml_parse(data, limits, context)");
    line("if node[0] != " + expanded(contract.root) + ": raise ValueError('Unexpected XML root or namespace')");
    line("if getattr(context, '_xml_root_type', None) is not None and _XML_TOKEN_TYPE_NAME in node[1]:");
    line("    if _xml_name(*_xml_resolve_qname(node[1][_XML_TOKEN_TYPE_NAME], node[4])) != context._xml_root_type: raise ValueError('Unexpected package XML root type')");
    line("    node[1].pop(_XML_TOKEN_TYPE_NAME)");
    line("return cls._xml_from_node(node, context)"); --indent;
    line("@classmethod");
    open("def decode_xml_chunks(cls, chunks, limits=None, context=None)");
    line("\"\"\"Decode UTF-8 slices without an XML tree; return only after exact document EOF.\"\"\"");
    line("if _xml_export_only(cls): raise ValueError('Readonly XML projections are export-only; decode the owning target')");
    line("if not _xml_token_capable(cls): raise ValueError('This XML mapping requires the whole-document codec')");
    line("return _xml_token_decode(cls, " + expanded(contract.root) + ", chunks, limits, context)");
    --indent;
    open("def _xml_node(self, name, budget, depth)");
    line("\"\"\"Populate the statically bound XML tree under a shared document budget.\"\"\"");
    line("node = budget.node(name, depth)");
    line("state = _xml_state_open(self, node, budget)");
    line("srl_parent_context = budget.context");
    line("extension_cursor = [0]");
    line("_xml_choices(self, self._xml_choices)");
    std::size_t xml_index{};
    std::size_t xml_position{};
    for (const auto& item : contract.fields) {
      const auto& source_field = *item.source;
      line("budget.context = _xml_field_context(srl_parent_context, " + quote(source_field.name) + ")");
      const auto& leaf = xml_leaf(source_field);
      const auto access = "self." + xml_member_path(value, source_field);
      const bool optional = source_field.type_name_list.front().is_optional();
      const auto* container = &source_field.type_name_list.front();
      if (container->is_optional()) { container = &container->arguments.front(); }
      const bool typed_list = item.list && (container->is_sequence() || source_field.modifier == member::modifier_type::array);
      const auto field_index = std::to_string(xml_index++);
      const auto rule = "self._xml_rules[" + field_index + "]";
      const bool scoped_value = item.qname || item.prefix_list ||
          python_xml::needs_namespace_state(item.facets);
      const auto position = std::to_string(xml_position);
      if (!item.attribute && !item.text) {
        line("_xml_extension_children(node, state, extension_cursor, " + std::to_string(xml_position++) + ", 0, budget, depth)");
      }
      std::string selected = access;
      if (item.wrapper) {
        line("wrapper = budget.node(" + expanded(*item.wrapper) + ", depth + 1)");
        line("node[2].append(wrapper)");
      }
      const auto parent = item.wrapper ? std::string{"wrapper"} : std::string{"node"};
      const auto child_depth = item.wrapper ? "depth + 2" : "depth + 1";
      if (optional) { line("if not isinstance(" + access + ", tuple) or len(" + access + ") > 1: raise ValueError('Invalid XML optional')"); open("if " + access); selected += "[0]"; }
      if (item.repeated && !item.list) {
        line("if len(" + access + ") < " + std::to_string(item.min_occurs) +
             (item.max_occurs ? " or len(" + access + ") > " + std::to_string(*item.max_occurs) : "") + ": raise ValueError('XML field occurrence mismatch')");
        if (source_field.fixed_extent) { line("if len(" + access + ") != " + std::to_string(source_field.fixed_extent) + ": raise ValueError('XML fixed extent mismatch')"); }
        open("for srl_item_index, item in enumerate(" + access + ")"); selected = "item";
        if (!item.wrapper) {
          line("if srl_item_index: _xml_extension_children(node, state, extension_cursor, " + position + ", srl_item_index, budget, depth, len(" + access + "))");
        }
      }
      if (xml_value_projection(source_field.type_name_list.front())) { selected += ".read_value()"; }
      const auto raw = typed_list ? "' '.join(" + xml_scalar(leaf, "srl_item", true) + " for srl_item in " + selected + ")" :
          xml_scalar(leaf, selected, true);
      const auto lexical = [&] {
        const bool child_scope = scoped_value && !item.attribute && !item.text;
        const auto scope = child_scope ? "child[4]" : "node[4]";
        return "_xml_checked(" + (child_scope ? "srl_namespace_text" : raw) + ", " + rule + ", " + scope + ", budget.context)";
      };
      if (item.text) {
        line("if len(node) > 6: node[6][0] = budget.context");
        line("node[3] = budget.text(" + lexical() + ")");
      } else if (item.attribute) {
        line("_xml_attribute(node, " + expanded(item.qualified_name) + ", " + lexical() + ", budget)");
      } else if (leaf.type == object_type::class_type) {
        line(parent + "[2].append(" + selected + "._xml_node(" + expanded(item.qualified_name) + ", budget, " + child_depth + "))");
      } else {
        line("child = budget.node(" + expanded(item.qualified_name) + ", " + child_depth + ")");
        if (scoped_value) {
          line("srl_namespace_text = " + raw);
          line("child[4] = _xml_scalar_scope_open(state, " + field_index + ", " +
              (item.repeated && !item.list ? "srl_item_index" : "0") + ", node[4], budget, " + rule + ", srl_namespace_text)");
        }
        line("child[3] = budget.text(" + lexical() + ")");
        line(parent + "[2].append(child)");
      }
      if (item.repeated && !item.list) {
        --indent;
        if (!item.wrapper) {
          line("if " + access + ": _xml_extension_children(node, state, extension_cursor, " + position + ", len(" + access + "), budget, depth, len(" + access + "))");
        }
      }
      if (optional) { --indent; }
    }
    line("budget.context = srl_parent_context");
    line("_xml_extension_children(node, state, extension_cursor, " + std::to_string(xml_position) + ", 0, budget, depth)");
    line("if state is not None and extension_cursor[0] != len(state[2]): raise ValueError('Invalid XML extension position')");
    line("_xml_extensions_validate(node, self._xml_record, budget.context)");
    line("return node"); --indent;
    line("@classmethod"); open("def _xml_from_node(cls, node, context=None)");
    line("\"\"\"Read exact generated members into an unpublished owning candidate.\"\"\"");
    line("if _xml_export_only(cls): raise ValueError('Readonly XML projections are export-only; decode the owning target')");
    line("fields, result = _XmlFieldsRich(node, " + std::string{std::any_of(contract.fields.begin(), contract.fields.end(), [](const auto& item) { return item.text; }) ? "True" : "False"} + ", cls._xml_record, context), cls()");
    line("srl_parent_context = context");
    xml_index = 0;
    for (const auto& item : contract.fields) {
      const auto& source_field = *item.source;
      line("context = _xml_field_context(srl_parent_context, " + quote(source_field.name) + ")");
      line("fields.context = context");
      const auto& leaf = xml_leaf(source_field);
      const bool optional = source_field.type_name_list.front().is_optional();
      const auto* container = &source_field.type_name_list.front();
      if (container->is_optional()) { container = &container->arguments.front(); }
      const bool typed_list = item.list && (container->is_sequence() || source_field.modifier == member::modifier_type::array);
      const auto field_index = std::to_string(xml_index++);
      const auto rule = "cls._xml_rules[" + field_index + "]";
      const bool scoped_value = item.qname || item.prefix_list ||
          python_xml::needs_namespace_state(item.facets);
      const auto access = "result." + xml_member_path(value, source_field);
      const auto read_value = [&](const std::string& expression) {
        if (leaf.type == object_type::class_type) {
          return model.names.at(leaf.resolved_node) + "._xml_from_node(" + expression + ", context)";
        }
        auto text = item.attribute || item.text ? expression : "_xml_read_scalar(" + expression + ")";
        if (!item.attribute && (item.default_value || item.fixed_value)) {
          const auto qualified = item.qname_fixed ? item.qname_fixed : item.qname_default;
          const auto qualified_list = item.qname_list_fixed ? item.qname_list_fixed : item.qname_list_default;
          const auto fallback = qualified_list ? "_xml_qualify_qname_list(" + python_xml::names(*qualified_list) + ", " +
              (item.text ? "node[4]" : expression + "[4]") + ", fields.reserved_prefixes())" : qualified ? "_xml_qualify_qname(" + python_xml::name(*qualified) + ", " +
              (item.text ? "node[4]" : expression + "[4]") + ")" :
              quote(item.fixed_value ? *item.fixed_value : *item.default_value);
          text = "(" + text + " or " + fallback + ")";
        }
        const auto scope = item.attribute || item.text ? "node[4]" : scoped_value ?
            "fields.scalar_scope(" + field_index + ", " +
                (item.repeated && !item.list ? "srl_item_index" : "0") + ", " + expression + ")" :
            expression + "[4]";
        auto checked = "_xml_checked(" + text + ", " + rule + ", " + scope + ", context)";
        if ((item.attribute || item.text) && (item.qname || item.prefix_list || item.facets.list_item_kind == "QName")) {
          checked = "_xml_promote_lexical(" + checked + ", " + rule + ", " + scope + ", node[4])";
        }
        return typed_list ? "[" + xml_scalar(leaf, "srl_item", false) + " for srl_item in _xml_tokens(" + checked + ")]" :
            xml_scalar(leaf, checked, false);
      };
      if (item.text) {
        line(access + " = " + read_value("fields.text()"));
      } else if (item.attribute) {
        line("text = fields.attribute(" + expanded(item.qualified_name) + ", " + (optional || item.default_value || item.fixed_value ? "True" : "False") + ")");
        if (item.default_value || item.fixed_value) {
          const auto qualified = item.qname_fixed ? item.qname_fixed : item.qname_default;
          const auto qualified_list = item.qname_list_fixed ? item.qname_list_fixed : item.qname_list_default;
          const auto fallback = qualified_list ? "_xml_qualify_qname_list(" + python_xml::names(*qualified_list) + ", node[4], fields.reserved_prefixes())" :
              qualified ? "_xml_qualify_qname(" + python_xml::name(*qualified) + ", node[4], fields.reserved_prefixes())" :
              quote(item.fixed_value ? *item.fixed_value : *item.default_value);
          line("if text is None: text = " + fallback);
        }
        line(access + " = " + (optional ? "() if text is None else (" + read_value("text") + ",)" : read_value("text")));
      } else {
        line("children = fields." + std::string{item.wrapper ? "wrapped(" + expanded(*item.wrapper) + ", " : "children("} + expanded(item.qualified_name) + ", " + std::to_string(item.min_occurs) + ", " +
            (item.max_occurs ? std::to_string(*item.max_occurs) : "None") + ")");
        if (item.repeated && !item.list) {
          if (source_field.fixed_extent) { line("if len(children) != " + std::to_string(source_field.fixed_extent) + ": raise ValueError('XML fixed extent mismatch')"); }
          line(access + " = [" + read_value("child") + " for srl_item_index, child in enumerate(children)]");
        } else { line(access + " = " + (optional ? "() if not children else (" + read_value("children[0]") + ",)" : read_value("children[0]"))); }
      }
    }
    line("context = fields.context = srl_parent_context");
    line("fields.finish()");
    line("_xml_choices(result, cls._xml_choices)");
    if (python_xml::needs_state(contract)) { line("result._srl_xml_state = fields.state()"); }
    line("return result"); --indent;
  }
  // Restrict protocol tests to fields whose schema explicitly requests native exclusions.
  std::string omitted_condition(const std::vector<std::string>& formats, std::string_view owner) {
    std::string values{};
    for (const auto& format : formats) {
      const auto name = format == "json" ? "JSON" : format == "binary_none" ? "BINARY_NONE" :
                        format == "binary_integer" ? "BINARY_INTEGER" : format == "binary_string" ? "BINARY_STRING" : "";
      if (*name != '\0') { values += "Protocol." + std::string{name} + ","; }
    }
    return values.empty() ? std::string{} : std::string{owner} + ".protocol in (" + values + ")";
  }
  // Append one indented Python statement.
  void line(const std::string& text = {}) {
    output.append(indent * 4, ' ');
    output += text + '\n';
  }
  // Enter a Python suite.
  void open(const std::string& text) {
    line(text + ':');
    ++indent;
  }
  // Select the schema's exported or conventionally private attribute name.
  std::map<std::string, std::string> projection_fields_{};
  bool value_reference_fields_{};

  std::string field(std::string_view name, access_type access = access_type::public_access) const {
    if (const auto found = projection_fields_.find(std::string{name}); found != projection_fields_.end()) { return found->second; }
    if (value_reference_fields_ && name == "srl_value") { return "_srl_value"; }
    auto result = snake(name);
    validate_identifier(result, keywords);
    return (access == access_type::public_access ? "" : "_") + result;
  }
  // Resolve a default without sharing mutable objects between instances.
  std::string initial(const type_name& value, const std::string& literal_value = {}) const {
    if (value.is_optional()) { return "()"; }
    if (value.is_sequence()) { return "[]"; }
    if (value.is_associative()) { return "{}"; }
    if (value.is_digest()) { return "[0] * " + std::to_string(value.digest_extent); }
    if (value.type == object_type::class_type) {
      return model.names.at(value.resolved_node) + "()";
    }
    if (value.type == object_type::enum_type) {
      const auto& constants = static_cast<const enum_node&>(*value.resolved_node).enum_name_list;
      return model.names.at(value.resolved_node) + "." +
             pascal(literal_value.empty() ? constants.front() : literal_value);
    }
    if (value.name == "bool") {
      return literal_value == "true" ? "True" : "False";
    }
    if (value.name == "string" || value.name.starts_with("version")) {
      return literal_value.empty() ? "\"\"" : literal_value;
    }
    const auto number = literal_value.empty() ? "0" : literal_value;
    return value.name == "float" ? "_FLOAT.unpack(_FLOAT.pack(" + number + "))[0]" : number;
  }
  // Construct independent typed defaults, including nested owned records and fixed arrays.
  std::string default_value(const member& value) const {
    if (value.initializer) {
      return construction::initializer(*value.initializer, "python",
          [&](const auto& item) { return type(item); },
          [&](const auto& item) { return initial(item.type_name_list.front(), literal(item)); },
          [&](auto item) { return field(item); });
    }
    return value.default_factory.empty() ? initial(value.type_name_list.front(), literal(value)) :
        construction::factory_call(type(value.type_name_list.front()), field(value.default_factory), "python");
  }
  // Map schema scalars to public Python type annotations.
  std::string type(const type_name& value) const {
    if (value.is_optional()) { return "tuple[()] | tuple[" + type(value.arguments.front()) + "]"; }
    if (value.is_sequence()) { return "list[" + type(value.arguments.front()) + "]"; }
    if (value.is_associative()) { return "dict[" + type(value.arguments[0]) + ", " + type(value.arguments[1]) + "]"; }
    if (value.type != object_type::primitive) {
      return model.names.at(value.resolved_node);
    }
    if (value.is_digest()) { return "list[int]"; }
    return (value.name == "string" || value.name.starts_with("version")) ? "str"
           : value.name == "bool"                                        ? "bool"
           : value.name == "float" || value.name == "double"             ? "float"
                                                                         : "int";
  }
  // Build a scalar decoding expression; ordinary nested fields merge in order.
  std::string read(const type_name& value, bool collection, const std::string& old = {}) const {
    if (value.is_optional()) {
      return "_read_optional(input, lambda: " + read(value.arguments.front(), collection) + ")";
    }
    if (value.is_sequence()) {
      return "[" + read(value.arguments.front(), true) + " for _ in input.entries()]";
    }
    if (value.is_associative()) {
      return "_read_map(input, lambda: " + read(value.arguments[0], true) +
             ", lambda: " + read(value.arguments[1], true) + ")";
    }
    if (value.is_digest()) {
      return "_read_digest(input, " + std::to_string(value.digest_extent) + ")";
    }
    if (value.type == object_type::class_type) {
      return model.names.at(value.resolved_node) + "._read(input, " + (old.empty() ? "None" : old) +
             ")";
    }
    if (value.type == object_type::enum_type) {
      const auto name = model.names.at(value.resolved_node);
      return "input.enumeration(" + name + ", _" + name + "_names, " +
             (collection ? "True" : "False") + ")";
    }
    if (value.name.starts_with("version")) {
      return "_read_version(input, " + std::string{value.name.back()} + ")";
    }
    if (value.name == "string") {
      return "input.text()";
    }
    if (value.name == "bool") {
      return "input.boolean()";
    }
    if (value.name == "char") {
      return "input.character()";
    }
    if (value.name == "float" || value.name == "double") {
      return "input.floating(" + std::string{value.name == "float" ? "True" : "False"} + ")";
    }
    return "input.integer(" + std::to_string(width(value)) + ", " +
           (value.name.starts_with('u') ? "True" : "False") + ")";
  }
  // Write one typed scalar directly to the output buffer.
  void write(const type_name& value, const std::string& expression, bool collection) {
    if (value.is_optional()) {
      line("if not isinstance(" + expression + ", tuple) or len(" + expression + ") > 1: raise ValueError('Invalid optional')");
      line("out.begin_optional(bool(" + expression + "))");
      open("if " + expression);
      write(value.arguments.front(), "(" + expression + ")[0]", collection);
      --indent;
      line("out.end_optional()");
    } else if (value.is_sequence() || value.is_associative()) {
      line("out.begin_array(len(" + expression + "))");
      const auto suffix = std::to_string(indent);
      const auto item = "item_" + suffix;
      open("for index_" + suffix + ", " + item + " in enumerate(" +
           (value.is_associative() ? "sorted(" + expression + ")" : expression) + ")");
      line("out.element(index_" + suffix + ")");
      if (value.is_associative()) {
        line("if out.protocol == Protocol.JSON: out.begin_object(); out.field(0, 0, True)");
        write(value.arguments[0], item, true);
        line("if out.protocol == Protocol.JSON: out.field(0, 1, False)");
        write(value.arguments[1], "(" + expression + ")[" + item + "]", true);
        line("if out.protocol == Protocol.JSON: out.end_object()");
      } else {
        write(value.arguments.front(), item, true);
      }
      --indent;
      line("out.end_array()");
    } else if (value.is_digest()) {
      line("_write_digest(out, " + expression + ", " + std::to_string(value.digest_extent) + ")");
    } else if (value.type == object_type::class_type) {
      line(expression + "._write(out)");
    } else if (value.type == object_type::enum_type) {
      line("out.enumeration(" + expression + ", _" + model.names.at(value.resolved_node) +
           "_names, " + (collection ? "True" : "False") + ")");
    } else if (value.name.starts_with("version")) {
      line("_write_version(out, " + expression + ", " + std::string{value.name.back()} + ")");
    } else if (value.name == "string") {
      line("out.text(" + expression + ")");
    } else if (value.name == "bool") {
      line("out.boolean(" + expression + ")");
    } else if (value.name == "char") {
      line("out.character(" + expression + ")");
    } else if (value.name == "float" || value.name == "double") {
      line("out.floating(" + expression + ", " + (value.name == "float" ? "True" : "False") + ")");
    } else {
      line("out.integer(" + expression + ", " + std::to_string(width(value)) + ", " +
           (value.name.starts_with('u') ? "True" : "False") + ")");
    }
  }
  // Emit a wire key reference without allocating key bytes per message.
  void key(std::uint32_t id, const std::string& text, bool first) {
    line("out.field(" + std::to_string(id) + ", " + std::to_string(model.keys.at(text)) + ", " +
         (dynamic_first ? "_srl_first" : first ? "True" : "False") + ")");
    if (dynamic_first) { line("_srl_first = False"); }
  }
  // Replace collections and reset the selected union payload when decoding a field.
  void read_member(const member& value, int alternative = -1) {
    const auto omitted = omitted_condition(value.omitted_formats, "input");
    if (!omitted.empty()) {
      line("if " + omitted + ": raise ValueError('Field omitted from selected format')");
    }
    const auto destination = "result." + field(value.name, value.access);
    const auto& item = value.type_name_list.front();
    if (value.modifier == member::modifier_type::none) {
      if (value.compact != compact_encoding::none) {
        line(destination + " = input.compact_value(" + std::to_string(width(item)) + ", " +
             (value.compact == compact_encoding::prefix ? "True" : "False") + ")");
        return;
      }
      line(destination + " = " + read(item, false, destination));
      return;
    }
    if (value.modifier == member::modifier_type::variant) {
      line(destination +
           "_index = " + (alternative < 0 ? "input.compact()" : std::to_string(alternative)));
      open("match " + destination + "_index");
      for (std::size_t i = 0; i < value.type_name_list.size(); ++i) {
        const auto& alt = value.type_name_list[i];
        open("case " + std::to_string(i));
        line(destination + "_" + snake(alt.enum_name) + " = " + read(alt, true));
        --indent;
      }
      line("case _: raise ValueError('Unknown union alternative')");
      --indent;
      return;
    }
    if (value.modifier == member::modifier_type::array) {
      const auto extent = value.fixed_extent ? std::to_string(value.fixed_extent) : std::string{};
      line(destination + " = [" + read(item, true) + " for _ in input.entries(" + extent + ")]");
      return;
    }
    line("values = {}");
    open("for _ in input.entries()");
    open("if input.protocol == Protocol.JSON");
    line("has_key = has_value = False");
    open("for entry in input.fields(())");
    open("if entry == 'key'");
    line("entry_key = " + read(map_key(value), true));
    line("has_key = True");
    --indent;
    open("elif entry == 'value'");
    line("entry_value = " + read(item, true));
    line("has_value = True");
    --indent;
    line("else: raise ValueError('Unknown map entry field')");
    --indent;
    line("if not has_key or not has_value: raise ValueError('Incomplete map entry')");
    --indent;
    open("else");
    line("entry_key = " + read(map_key(value), true));
    line("entry_value = " + read(item, true));
    --indent;
    line("values[entry_key] = entry_value");
    --indent;
    line(destination + " = values");
  }
  // Write collections in deterministic key order without copying their values.
  void write_member(const member& value, bool first, bool keyed = true,
                    const std::string& selected_expression = {}) {
    const auto source = selected_expression.empty() ? "self." + field(value.name, value.access) : selected_expression;
    if (value.modifier == member::modifier_type::variant) {
      open("match " + source + "_index");
      for (std::size_t i = 0; i < value.type_name_list.size(); ++i) {
        const auto& alt = value.type_name_list[i];
        open("case " + std::to_string(i));
        key(value.id, value.display_name + ":" + alt.enum_name, first);
        line("if out.protocol in (Protocol.BINARY_NONE, Protocol.BINARY_INTEGER): out.compact(" +
             std::to_string(i) + ")");
        write(alt, source + "_" + snake(alt.enum_name), true);
        --indent;
      }
      line("case _: raise ValueError('Unknown union alternative')");
      --indent;
      return;
    }
    if (keyed) { key(value.id, value.display_name, first); }
    const auto& item = value.type_name_list.front();
    if (value.modifier == member::modifier_type::none) {
      if (value.compact != compact_encoding::none) {
        line("out.compact_value(" + source + ", " + std::to_string(width(item)) + ", " +
             (value.compact == compact_encoding::prefix ? "True" : "False") + ", " +
             (value.compact_strict ? "True" : "False") + ")");
        return;
      }
      write(item, source, false);
      return;
    }
    if (value.fixed_extent != 0) {
      line("if len(" + source + ") != " + std::to_string(value.fixed_extent) +
           ": raise ValueError('Fixed array extent mismatch')");
    }
    line("out.begin_array(len(" + source + "))");
    open("for index, item in enumerate(" +
         (value.modifier == member::modifier_type::map ? "sorted(" + source + ")" : source) + ")");
    line("out.element(index)");
    if (value.modifier == member::modifier_type::map) {
      line("if out.protocol == Protocol.JSON: out.begin_object()");
      open("if out.protocol == Protocol.JSON");
      key(0, "key", true);
      --indent;
      write(map_key(value), "item", true);
      open("if out.protocol == Protocol.JSON");
      key(0, "value", false);
      --indent;
      write(item, source + "[item]", true);
      line("if out.protocol == Protocol.JSON: out.end_object()");
    } else {
      write(item, "item", true);
    }
    --indent;
    line("out.end_array()");
  }
  // Check one revision without adding any branches to unversioned models.
  void check_version(const class_node& object, const std::string& expression, bool input) {
    const auto& version = *object.version_member();
    const auto language = version_language::python;
    auto invalid = version_less(language, version, expression, schema_version::minimum(version)) +
                   " or " + version_at_least(language, version, expression, version.default_value) +
                   " and not " +
                   version_equal(language, version, expression, version.default_value);
    if (version.type_name_list.front().name == "float" ||
        version.type_name_list.front().name == "double") {
      invalid += " or not _math.isfinite(" + expression + ")";
    }
    if (input) {
      invalid += " or (input.limits.read_policy == ReadPolicy.STRICT and not " +
                 version_equal(language, version, expression, version.default_value) + ")";
    }
    line("if " + invalid + ": raise ValueError('Unsupported schema version')");
  }
  // Track keyed presence, but consume no positional bytes for inactive historical fields.
  void read_versioned_member(const class_node& object, const member& item, int alternative = -1) {
    const auto* version = object.version_member();
    if (!version) {
      read_member(item, alternative);
      return;
    }
    line("if " + std::to_string(item.id) +
         " in seen: raise ValueError('Duplicate versioned field')");
    line("seen.add(" + std::to_string(item.id) + ")");
    const auto active = version_active(version_language::python, item, *version,
                                       "result." + field(version->name, version->access));
    if (!active.empty()) {
      open("if input.protocol != Protocol.BINARY_NONE or (" + active + ")");
    }
    read_member(item, alternative);
    if (!active.empty()) {
      --indent;
    }
    if (item.version) {
      check_version(object, "result." + field(item.name, item.access), true);
    }
  }
  // Emit native/pure methods and validate explicit external behavior attachments.
  void behaviors(const class_node& value) {
    std::vector<const function_declaration*> external;
    for (const auto& method : value.functions) {

      if (!method.expression && !behavior::native_body(method, "python")) { external.push_back(&method); }
    }
    for (const auto* method : behavior::contracts(value)) {
      if (std::find(external.begin(), external.end(), method) == external.end()) { external.push_back(method); }
    }
    behavior::require_unique_callable_names(value, "python", [&](auto n) { return field(n); });
    if (!external.empty()) {
      open("def attach_behavior(self, behavior)");
      for (const auto* method : external) {
        open("if not callable(behavior.get(" + quote(method->name) + "))");
        line("raise TypeError('Missing required behavior callable')"); --indent;
      }
      line("self._srl_behavior = dict(behavior)"); --indent;
    }
    for (const auto& item : behavior::ordered_body(value)) {
      if (item.kind == class_body_item::kind_type::native_code) {
        const auto& block = value.native_blocks.at(item.index);
        if (block.language == "python") { output += std::string(indent * 4, ' ') + behavior::source_marker(block, "python") + behavior::python_block(block.text, indent * 4); }
        continue;
      }
      if (item.kind != class_body_item::kind_type::function) { continue; }
      const auto& method = value.functions.at(item.index);
      line(behavior::source_marker(method, "python").substr(0, behavior::source_marker(method, "python").size() - 1));
      std::string parameters = "self", arguments = "self";
      for (const auto& argument : method.parameters) {
        parameters += ", " + field(argument.name); arguments += ", " + field(argument.name);
      }
      open("def " + field(method.name, method.access) + "(" + parameters + ")");
      if (method.dispatch != function_declaration::dispatch_type::ordinary &&
          (method.expression || behavior::native_body(method, "python"))) {
        open("if self._srl_behavior is not None");
        line("return self._srl_behavior[" + quote(method.name) + "](" + arguments + ")"); --indent;
      }
      if (const auto* native = behavior::native_body(method, "python")) {
        output += std::string(indent * 4, ' ') + behavior::source_marker(*native, "python") + behavior::python_block(native->text, indent * 4);
      } else if (method.expression) {
        const auto symbol = [&](std::string_view name) {
          const auto argument = std::find_if(method.parameters.begin(), method.parameters.end(),
              [&](const auto& item) { return item.name == name; });
          if (argument != method.parameters.end()) { return field(name); }
          const auto member = std::find_if(value.member_list.begin(), value.member_list.end(),
              [&](const auto& item) { return item.name == name; });
          return "self." + field(member->name, member->access);
        };
        if (method.predicate_body) { open("try"); }
        std::map<std::string, const behavior_expression*> symbols;
        if (!method.predicate_body) { behavior::scalar_symbols(*method.expression, symbols); }
        for (const auto& [name, node] : symbols) {
          const auto text = symbol(name);
          if (node->result_type == "bool") {
            line(arithmetic_helpers.checked + "_boolean(" + text + ")");
          } else if (node->result_type.starts_with("enum:")) {
            const auto maximum = std::to_string(static_cast<const enum_node&>(*node->resolved_type->resolved_node).enum_name_list.size() - 1);
            line(arithmetic_helpers.checked + "_integer(" + text + ", 0, " + maximum + ", " + type(*node->resolved_type) + ")");
          } else if (node->result_type.starts_with("int") || node->result_type.starts_with("uint") || node->result_type == "char") {
            const auto [minimum, maximum] = behavior::integer_range(node->result_type);
            line(arithmetic_helpers.checked + "_integer(" + text + ", " + minimum + ", " + maximum + ")");
          } else {
            line(arithmetic_helpers.checked + "(" + text + ")");
          }
        }
        behavior::expression_context context;
        context.predicate = method.predicate_body;
        context.member_name = [&](auto member) { return field(member); };
        context.native_type = [&](const auto& value_type) { return type(value_type); };
        context.enum_value = [&](const auto& value_type, auto enumerator) { return type(value_type) + "." + pascal(enumerator); };
        context.call = [&](const behavior_expression& node, const std::vector<std::string>& arguments) {
          if (node.call_constructor) { return behavior::constructor_expression(node, arguments, "python",
              [&](const auto& value) { return type(value); }, [&](auto name) { return field(name); }); }
          const auto& target = behavior::called_function(node);
          std::string receiver = node.call_receiver ? "(" + arguments.at(0) + ")" : "self";
          for (const auto base : node.call_base_path) { receiver += "." + field("base" + std::to_string(base)); }
          const auto callable = node.call_target->type == object_type::function_type ? snake(model.names.at(node.call_target)) : receiver + "." + field(target.name, target.access);
          return callable + "(" + behavior::call_arguments(arguments, node.call_receiver ? 1 : 0) + ")";
        };
        while (std::any_of(method.parameters.begin(), method.parameters.end(), [&](const auto& parameter) {
          return field(parameter.name) == context.budget_name;
        })) { context.budget_name += '_'; }
        if (behavior::has_reduction(*method.expression)) {
          line(context.budget_name + " = [" + std::to_string(behavior_expression::maximum_reduction_visits) + "]");
        }
        line("_srl_result = " + behavior::expression(*method.expression, "python", symbol, arithmetic_helpers, context));
        if (method.return_type.name == "float" || method.return_type.name == "double") {
          open("if not _math.isfinite(_srl_result)");
          line("raise ValueError('Non-finite portable result')"); --indent;
        }
        line("return _srl_result");
        if (method.predicate_body) {
          --indent;
          open("except (ArithmeticError, ValueError, TypeError, AttributeError, IndexError)");
          line("return False"); --indent;
        }
      } else {
        open("if self._srl_behavior is None");
        line("raise RuntimeError('Attach required behavior before invocation')"); --indent;
        line("return self._srl_behavior[" + quote(method.name) + "](" + arguments + ")");
      }
      --indent;
    }
  }

  // Emit flattened namespace functions with the same checked expression contract as methods.
  void free_function(const function_node& node) {
    const auto& method = node.function;
    std::string parameters;
    for (const auto& parameter : method.parameters) {
      if (!parameters.empty()) { parameters += ", "; }
      parameters += field(parameter.name);
    }
    line(behavior::source_marker(method, "python"));
    open("def " + snake(model.names.at(&node)) + "(" + parameters + ")");
    if (const auto* native = behavior::native_body(method, "python")) {
      output += behavior::source_marker(*native, "python") + behavior::python_block(native->text, indent * 4);
    } else if (method.expression) {
      behavior::expression_context context;
      context.member_name = [&](auto value) { return field(value); };
      context.native_type = [&](const auto& value) { return type(value); };
      context.enum_value = [&](const auto& value, auto name) { return type(value) + "." + pascal(name); };
      context.call = [&](const behavior_expression& call, const std::vector<std::string>& arguments) {
        if (call.call_constructor) { return behavior::constructor_expression(call, arguments, "python",
            [&](const auto& value) { return type(value); }, [&](auto name) { return field(name); }); }
        if (call.call_receiver) {
          std::string receiver = "(" + arguments.at(0) + ")";
          for (const auto base : call.call_base_path) { receiver += "." + field("base" + std::to_string(base)); }
          const auto& callee = behavior::called_function(call);
          return receiver + "." + field(callee.name, callee.access) + "(" +
              behavior::call_arguments(arguments, 1) + ")";
        }
        return snake(model.names.at(call.call_target)) + "(" + behavior::call_arguments(arguments) + ")";
      };
      behavior::free_expression_body(method, "python", type(method.return_type), [&](auto value) { return field(value); },
          arithmetic_helpers, context, [&](const auto& text) { line(text); },
          [&](const auto& text) { open(text); }, [&]() { --indent; });
    } else { throw std::invalid_argument{"python: standalone functions require an expression or selected native body"}; }
    --indent;
  }

  // An ABC checks abstract completeness; adapters retain explicit data and implementation objects.
  void dispatch_adapter(const class_node& value) {
    const auto contracts = behavior::contracts(value);
    if (contracts.empty()) { return; }
    const auto model_name = model.names.at(&value);
    open("class " + model_name + "Behavior(_ABC)");
    for (const auto* method : contracts) {
      line("@_abstractmethod");
      std::string parameters = "self, receiver";
      for (const auto& argument : method->parameters) { parameters += ", " + field(argument.name); }
      open("def " + field(method->name) + "(" + parameters + ")");
      line("raise NotImplementedError('Required behavior implementation')"); --indent;
    }
    --indent; open("class " + model_name + "BehaviorAdapter");
    open("def __init__(self, receiver, implementation)");
    open("if not isinstance(receiver, " + model_name + ") or not isinstance(implementation, " + model_name + "Behavior)");
    line("raise TypeError('Behavior adapter requires matching data and complete behavior')"); --indent;
    line("self.receiver = receiver"); line("self.implementation = implementation"); --indent;
    for (const auto* method : contracts) {
      std::string parameters = "self", calls = "self.receiver";
      for (const auto& argument : method->parameters) { parameters += ", " + field(argument.name); calls += ", " + field(argument.name); }
      open("def " + field(method->name) + "(" + parameters + ")");
      line("return self.implementation." + field(method->name) + "(" + calls + ")"); --indent;
    }
    --indent;
  }

  // Emit opt-in comparisons over borrowed selected values without replacing Python object identity.
  void comparisons(const class_node& value) {
    const auto* options = comparison::find(value, model.class_comparisons);
    if (!options) { return; }
    const auto selected = comparison::parts(value, *options);
    struct operand {
      std::string name;
      bool nested{};
      bool floating{};
    };
    std::vector<operand> operands;
    for (const auto* base : selected.bases) {
      const auto index = static_cast<std::size_t>(base - value.parents.data());
      operands.push_back({field("base" + std::to_string(index), base->access), true, false});
    }
    for (const auto* item : selected.fields) {
      const auto& type = item->type_name_list.front();
      operands.push_back({field(item->name, item->access), type.resolved_node &&
          type.resolved_node->type == object_type::class_type,
          type.name == "float" || type.name == "double"});
    }
    const auto name = model.names.at(&value);
    if (options->equality) {
      open("def equals_value(self, other: " + name + ") -> bool");
      line("\"\"\"Compare selected values synchronously; floating NaN is unequal.\"\"\"");
      open("if not isinstance(other, " + name + ")");
      line("return False");
      --indent;
      std::string expression;
      for (const auto& item : operands) {
        if (!expression.empty()) { expression += " and "; }
        expression += item.nested ? "self." + item.name + ".equals_value(other." + item.name + ")" :
            "self." + item.name + " == other." + item.name;
      }
      line("return " + (expression.empty() ? std::string{"True"} : expression));
      --indent;
    }
    if (options->ordering) {
      open("def compare_value(self, other: " + name + ") -> int | None");
      line("\"\"\"Compare selected values lexically; None denotes an unordered floating value.\"\"\"");
      open("if not isinstance(other, " + name + ")");
      line("return None");
      --indent;
      for (const auto& item : operands) {
        const auto left = "self." + item.name, right = "other." + item.name;
        if (item.nested) {
          line("srl_order = " + left + ".compare_value(" + right + ")");
          open("if srl_order != 0");
          line("return srl_order");
          --indent;
        } else {
          if (item.floating) {
            open("if _math.isnan(" + left + ") or _math.isnan(" + right + ")");
            line("return None");
            --indent;
          }
          open("if " + left + " < " + right);
          line("return -1");
          --indent;
          open("if " + left + " > " + right);
          line("return 1");
          --indent;
        }
      }
      line("return 0");
      --indent;
    }
  }

  // Emit logical indexing and shallow snapshots while preserving owning list fields.
  void array_access(const class_node& owner) {
    for (const auto& item : owner.member_list) {
      if (!item.array_exposure) { continue; }
      const auto& exposure = *item.array_exposure;
      const auto storage = "self." + field(item.name, item.access);
      const auto& element = item.modifier == member::modifier_type::array ?
          item.type_name_list.front() : item.type_name_list.front().arguments.front();
      const auto size = field("size", exposure.access);
      std::vector<std::uint64_t> dimensions;
      for (const auto& dimension : exposure.dimensions) {
        if (dimension.kind != dimension_expression::operation::literal) {
          throw std::invalid_argument{"python: array exposure dimensions must be resolved"};
        }
        dimensions.push_back(dimension.value);
      }
      const auto rank = dimensions.empty() ? std::size_t{1} : dimensions.size();
      std::string parameters;
      std::string offset;
      for (std::size_t index = 0; index < rank; ++index) {
        const auto coordinate = "index" + std::to_string(index);
        parameters += ", " + coordinate + ": int";
        offset = offset.empty() ? coordinate :
            "(" + offset + " * " + std::to_string(dimensions[index]) + " + " + coordinate + ")";
      }
      open("def " + size + "(self) -> int");
      line("\"\"\"Return the validated logical length of the selected storage.\"\"\"");
      line("if not isinstance(" + storage + ", list): raise ValueError('Invalid array storage')");
      if (item.fixed_extent != 0) {
        line("if len(" + storage + ") != " + std::to_string(item.fixed_extent) + ": raise ValueError('Invalid fixed array length')");
      }
      line("return len(" + storage + ")");
      --indent;
      const auto bounds = [&]() {
        line("count = self." + size + "()");
        for (std::size_t index = 0; index < rank; ++index) {
          const auto coordinate = "index" + std::to_string(index);
          const auto extent = dimensions.empty() ? "count" : std::to_string(dimensions[index]);
          line("if not isinstance(" + coordinate + ", int) or isinstance(" + coordinate +
              ", bool) or " + coordinate + " < 0 or " + coordinate + " >= " + extent +
              ": raise IndexError('Array coordinate out of bounds')");
        }
      };
      open("def " + field("get", exposure.access) + "(self" + parameters + ") -> " + type(element));
      line("\"\"\"Read one element; object values retain their ordinary reference semantics.\"\"\"");
      bounds();
      line("return " + storage + "[" + offset + "]");
      --indent;
      if (!exposure.readonly) {
        open("def " + field("set", exposure.access) + "(self" + parameters + ", item: " + type(element) + ") -> None");
        line("\"\"\"Replace one element after storage and coordinate validation.\"\"\"");
        bounds();
        line(storage + "[" + offset + "] = item");
        --indent;
      }
      open("def " + field("values", exposure.access) + "(self) -> " +
          (exposure.readonly ? "tuple[" + type(element) + ", ...]" : "list[" + type(element) + "]"));
      line("\"\"\"Allocate an iterable snapshot; contained objects are shared, not deeply copied.\"\"\"");
      line("self." + size + "()");
      line(std::string{"return "} + (exposure.readonly ? "tuple(" : "list(") + storage + ")");
      --indent;
    }
  }

  // Emit slot-based owning models with independent defaults and exact decode APIs.
  // Encode readonly getters through the target's ordinary field and metadata traversal.
  std::string translator_codecs(const conversion_declaration& method, std::string_view) {
    auto saved_output = std::move(output);
    const auto saved_indent = indent;
    const auto saved_first = dynamic_first;
    output.clear(); indent = 1;
    const auto& target = ::rohit::serializer::conversion::record(method.target);
    auto projected = translator::output_fields(method, "python", [&](auto value, auto access) { return field(value, access); });
    open("def encode(self, protocol: Protocol) -> bytes");
    line("\"\"\"Encode readonly projections without constructing the target.\"\"\"");
    if (model.protocols == output_protocols::binary_none) {
      line("if _protocol(protocol) != Protocol.BINARY_NONE: raise ValueError('Protocol disabled during generation')");
    }
    line("out = _Writer(protocol)"); line("self._write(out)"); line("return bytes(out.data)"); --indent;
    open("def _write(self, out)");
    projection_fields_ = std::move(projected);
    dynamic_first = target.has_magic() && !target.magic_omits("json");
    for (const auto& item : wire_members(target)) {
      dynamic_first = dynamic_first || !omitted_condition(item.omitted_formats, "out").empty();
    }
    write_output_body(target);
    projection_fields_.clear(); --indent;
    if (model.xml.enabled) {
      const auto contract = xml_contract.bind(target);
      const auto target_name = model.names.at(&target);
      const auto root = quote(contract.root.namespace_uri.empty() ? contract.root.local :
          "{" + contract.root.namespace_uri + "}" + contract.root.local);
      const auto slots = translator::slots(method, "python", [&](auto value, auto access) { return field(value, access); });
      const auto getters = translator::output_fields(method, "python", [&](auto value, auto access) { return field(value, access); });
      std::string accessors{"{"};
      std::size_t parent_index{};
      for (const auto& slot : slots) {
        const auto access = slot.parent ? target.parents[parent_index++].access : slot.field.access;
        auto getter = getters.at(slot.field.name);
        if (!getter.ends_with("()")) { throw std::logic_error{"Translator XML accessor is not a checked getter"}; }
        getter.resize(getter.size() - 2U);
        accessors += quote(field(slot.field.name, access)) + ": self." + getter + ',';
      }
      accessors += '}';
      const auto facade = "_XmlProjectionAccess(" + target_name + ", " + accessors + ", self)";
      if (python_xml::needs_state(contract)) {
        open("def xml_bind_namespace(self, prefix, uri)");
        line("\"\"\"Bind XML-only QName presentation on this scoped view without changing source storage.\"\"\"");
        line("_xml_bind_namespace(" + facade + ", prefix, uri)"); --indent;
      }
      open("def encode_xml(self, limits=None, schema=None, context=None)");
      line("\"\"\"Export borrowed target getters without constructing an owning target or copying nested values.\"\"\"");
      line("budget = _XmlBudget(limits, context)"); line("context = budget.context");
      line("if not _xml_token_capable(" + target_name + ") and _xml_token_required(" + target_name + "): raise ValueError('This XML mapping requires a token-compatible owning graph')");
      line("selected = " + facade);
      line("data = _xml_token_encode(selected, " + root + ", budget.limits, context) if _xml_token_capable(" + target_name + ") else _xml_write(" + target_name + "._xml_node(selected, " + root + ", budget, 1), budget.limits, context)");
      if (!model.xml.runtime_xsd) { line("if schema is not None: raise ValueError('Runtime XSD was not enabled during generation')"); }
      else if (model.xml.validation_scope != "input") { line("if schema is not None: schema.validate(data, budget.limits, context)"); }
      line("return data"); --indent;
    }
    auto result = std::move(output); output = std::move(saved_output); indent = saved_indent; dynamic_first = saved_first;
    return result;
  }

  // Encode owning or projected fields through the same target traversal.
  void write_output_body(const class_node& value) {
    const auto name = model.names.at(&value);
    line("\"\"\"Write typed fields in schema order.\"\"\"");
    if (value.reference && value.reference->value_projection) {
      line("srl_selected = self.read_value()");
      write_member(value.member_list.front(), true, false, "srl_selected");
      return;
    }
    if (value.reference) { line("self._srl_validate_reference_address()"); }
    if (!auto_size::fields(value).empty()) { line("_srl_size_start = len(out.data)"); }
    if (value.has_magic()) {
      const auto omitted = omitted_condition(value.magic_omitted_formats, "out");
      open("if out.protocol != Protocol.JSON" + (omitted.empty() ? "" : " and not (" + omitted + ")"));
      if (value.magic_field) {
        write(value.magic_field->type_name_list.front(),
              name + "." + field("magic", value.magic_access), false);
      } else {
        open("for magic_byte in " + name + "." + field("magic", value.magic_access));
        line("out.integer(magic_byte, 1, True)");
        --indent;
      }
      --indent;
    }
    if (const auto* version = value.version_member()) {
      check_version(value, "self." + field(version->name, version->access), false);
    }
    line("out.begin_object()");
    if (dynamic_first) { line("_srl_first = True"); }
    if (value.has_magic() && !value.magic_omits("json")) {
      open("if out.protocol == Protocol.JSON");
      key(value.magic_id, "magic", true);
      if (value.magic_field) {
        write(value.magic_field->type_name_list.front(),
              name + "." + field("magic", value.magic_access), false);
      } else {
        line("out.text(" + name + "." + field("magic", value.magic_access) + ".decode('utf-8'))");
      }
      --indent;
    }
    bool first = true;
    if (const auto* version = value.version_member()) {
      write_member(*version, true);
      first = false;
    }
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      const auto& base = value.parents[i];
      key(base.id, base.display_name, first);
      line("self." + field("base" + std::to_string(i), base.access) + "._write(out)");
      first = false;
    }
    for (const auto& item : wire_members(value)) {
      if (item.version) {
        continue;
      }
      const auto* version = value.version_member();
      const auto active = version ? version_active(version_language::python, item, *version,
                                                   "self." + field(version->name, version->access))
                                  : std::string{};
      if (!active.empty()) {
        open("if " + active);
      }
      const auto omitted = omitted_condition(item.omitted_formats, "out");
      if (!omitted.empty()) { open("if not (" + omitted + ")"); }
      if (!item.auto_size_formats.empty()) {
        line("_srl_size_key_" + std::to_string(item.id) + " = len(out.data)");
        key(item.id, item.display_name, first);
        const auto selected_size = omitted_condition(item.auto_size_formats, "out");
        open("if " + selected_size);
        line("_srl_size_pos_" + std::to_string(item.id) + " = len(out.data)");
        line("out.integer(0, " + std::to_string(width(item.type_name_list.front())) + ", True)");
        --indent;
        if (!item.wire_only) {
          open("else");
          write(item.type_name_list.front(), "self." + field(item.name, item.access), false);
          --indent;
        }
      } else {
        write_member(item, first);
      }
      if (!omitted.empty()) { --indent; }
      if (!active.empty()) {
        --indent;
      }
      first = false;
    }
    line("out.end_object()");
    for (const auto* item : auto_size::fields(value)) {
      const auto active = omitted_condition(item->auto_size_formats, "out");
      const auto bytes = std::to_string(width(item->type_name_list.front()));
      const auto position = "_srl_size_pos_" + std::to_string(item->id);
      open("if " + active);
      line("out.data[" + position + ":" + position + " + " + bytes +
           "] = (len(out.data) - _srl_size_start" +
           (item->size_scope == auto_size_scope::payload ? " - (" + position + " - _srl_size_key_" + std::to_string(item->id) + " + " + bytes + ")" : "") +
           ").to_bytes(" + bytes + ", 'little')");
      --indent;
    }
  }

  void object(const class_node& value) {
    value_reference_fields_ = value.reference && value.reference->value_projection;
    std::set<std::string> array_methods;
    for (const auto& item : value.member_list) {
      if (!item.array_exposure) { continue; }
      for (const auto method : {"get", "size", "values", "set"}) {
        if (std::string_view{method} == "set" && item.array_exposure->readonly) { continue; }
        if (!array_methods.insert(field(method, item.array_exposure->access)).second) {
          throw std::invalid_argument{"python: array exposure helper collision"};
        }
      }
    }
    const auto check_array_name = [&](const std::string& name) {
      if (array_methods.contains(name)) { throw std::invalid_argument{"python: array exposure naming collision: " + name}; }
    };
    for (const auto& item : value.member_list) { check_array_name(field(item.name, item.access)); }
    for (const auto& method : value.functions) { check_array_name(field(method.name, method.access)); }
    for (const auto& factory : value.constructions) { check_array_name(field(factory.constructor ? "create" : factory.name, factory.access)); }
    for (const auto& conversion : value.conversions) { check_array_name(field(conversion.name, conversion.access)); }
    dynamic_first = value.has_magic() && !value.magic_omits("json");
    for (const auto& item : value.member_list) {
      dynamic_first = dynamic_first || !omitted_condition(item.omitted_formats, "out").empty();
    }
    const auto name = model.names.at(&value);
    open("class " + name);
    line("\"\"\"Owning schema model; do not mutate during encoding.\"\"\"");
    if (value.magic_field) {
      const auto& magic = *value.magic_field;
      line(field("magic", value.magic_access) + " = " +
           initial(magic.type_name_list.front(), literal(magic)));
    } else if (value.has_magic()) {
      std::string bytes = "bytes((";
      for (const auto byte : value.magic_bytes) {
        bytes += std::to_string(static_cast<unsigned char>(byte)) + ",";
      }
      line(field("magic", value.magic_access) + " = " + bytes + "))");
    }
    std::vector<std::pair<std::string, std::pair<std::string, std::string>>> fields;
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      const auto& base = value.parents[i];
      const auto type_name = model.names.at(base.parent_class);
      fields.push_back(
          {field("base" + std::to_string(i), base.access), {type_name, type_name + "()"}});
    }
    for (const auto& item : value.member_list) {
      if (value.reference && value.reference->value_projection) { continue; }
      if (item.wire_only) { continue; }
      const auto f = field(item.name, item.access);
      const auto& t = item.type_name_list.front();
      if (item.modifier == member::modifier_type::variant) {
        fields.push_back({f + "_index", {"int", "0"}});
        for (const auto& alt : item.type_name_list) {
          fields.push_back({f + "_" + snake(alt.enum_name), {type(alt), initial(alt)}});
        }
      } else if (item.modifier == member::modifier_type::none) {
        fields.push_back({f, {type(t), default_value(item)}});
      } else {
        fields.push_back({f,
                          {item.modifier == member::modifier_type::array
                               ? "list[" + type(t) + "]"
                               : "dict[" + type(map_key(item)) + ", " + type(t) + "]",
                           item.initializer ? default_value(item) : item.fixed_extent ? "[" + initial(t) + " for _ in range(" +
                               std::to_string(item.fixed_extent) + ")]" :
                           item.modifier == member::modifier_type::array ? "[]" : "{}"}});
      }
    }
    std::string slots = "__slots__ = (";
    if (std::any_of(model.nodes.begin(), model.nodes.end(), [&](const auto* node) {
          if (node->type != object_type::class_type) { return false; }
          const auto& owner = static_cast<const class_node&>(*node);
          return owner.reference && owner.reference->owner.resolved_node == &value &&
              owner.reference->mode != reference_declaration::mode_type::shared;
        })) { slots += "\"__weakref__\", "; }
    if (value.reference) { slots += "\"_srl_reference\", "; }
    const bool xml_state = model.xml.enabled && !value.reference && !mixed_fragments.contains(&value) && python_xml::needs_state(xml_contract.bind(value));
    if (xml_state) { slots += "\"_srl_xml_state\", "; }
    for (const auto& f : fields) {
      slots += quote(f.first) + ", ";
    }
    const bool external_behavior = !behavior::contracts(value).empty() || std::any_of(value.functions.begin(), value.functions.end(),
        [](const auto& method) { return !method.expression && !behavior::native_body(method, "python"); });
    if (external_behavior) { slots += "\"_srl_behavior\", "; }
    line(slots + ")");
    for (const auto& f : fields) {
      line(f.first + ": " + f.second.first);
    }
    open("def __init__(self)");
    line("\"\"\"Construct independent schema defaults.\"\"\"");
    for (const auto& f : fields) {
      line("self." + f.first + " = " + f.second.second);
    }
    if (external_behavior) { line("self._srl_behavior = None"); }
    if (value.reference) { line("self._srl_reference = None"); }
    if (xml_state) { line("self._srl_xml_state = None"); }
    --indent;
    output += reference_support::portable_methods(value, name, "python",
        [&](const auto& item) { return type(item); },
        [&](auto item, auto access) { return field(item, access); });
    array_access(value);
    xml_object(value);
    behaviors(value);
    comparisons(value);
    output += behavior::python_block(construction::factories(value, name, "python",
        [&](const auto& item) { return type(item); },
        [&](auto item, auto access) { return field(item, access); },
        [&](const auto& item) { return default_value(item); }), indent * 4);
    output += behavior::python_block(conversion::methods(value, name, "python",
        [&](const auto& item) { return type(item); },
        [&](auto item, auto access) { return field(item, access); }), indent * 4);
    output += behavior::python_block(translator::methods(value, name, "python",
        [&](const auto& item) { return type(item); },
        [&](auto item, auto access) { return field(item, access); },
        [&](const auto& item) { return default_value(item); },
        [&](const auto& method, auto view) { return translator_codecs(method, view); }), indent * 4);
    open("def encode(self, protocol: Protocol) -> bytes");
    line("\"\"\"Encode one complete message into independent bytes.\"\"\"");
    if (model.protocols == output_protocols::binary_none) {
      line("if _protocol(protocol) != Protocol.BINARY_NONE: raise ValueError('Protocol disabled during generation')");
    }
    line("out = _Writer(protocol)");
    line("self._write(out)");
    line("return bytes(out.data)");
    --indent;
    line("@classmethod");
    open("def decode(cls, data, protocol: Protocol, limits: Limits | None = None) -> " + name);
    line("\"\"\"Return a fresh value after exact-message validation.\"\"\"");
    if (model.protocols == output_protocols::binary_none) {
      line("if _protocol(protocol) != Protocol.BINARY_NONE: raise ValueError('Protocol disabled during generation')");
    }
    line("input = _Reader(data, protocol, limits)");
    line("result = cls._read(input)");
    line("input.finish()");
    line("return result");
    --indent;
    open("def _write(self, out)");
    write_output_body(value);
    --indent;
    line("@classmethod");
    open("def _read(cls, input, result=None)");
    line("\"\"\"Merge ordinary nested fields while preserving input order.\"\"\"");
    if (value.reference && value.reference->value_projection) {
      line("raise ValueError('Export references cannot be decoded; use the ordinary owning type')");
      indent -= 2;
      return;
    }
    if (!auto_size::fields(value).empty()) {
      line("_srl_size_start = input.offset");
      for (const auto* item : auto_size::fields(value)) {
        line("_srl_size_seen_" + std::to_string(item->id) + " = False");
      }
    }
    if (value.has_magic()) {
      const auto omitted = omitted_condition(value.magic_omitted_formats, "input");
      open("if input.protocol != Protocol.JSON" + (omitted.empty() ? "" : " and not (" + omitted + ")"));
      if (value.magic_field) {
        line("if " + read(value.magic_field->type_name_list.front(), false) + " != cls." +
             field("magic", value.magic_access) + ": raise ValueError('Binary magic header mismatch')");
      } else {
        open("for magic_byte in cls." + field("magic", value.magic_access));
        line("if input.integer(1, True) != magic_byte: raise ValueError('Binary magic header mismatch')");
        --indent;
      }
      --indent;
    }
    line("if result is None: result = cls()");
    if (value.reference) { line("result.unbind()"); }
    const bool json_magic = value.has_magic() && !value.magic_omits("json");
    if (json_magic) { line("_srl_magic_seen = False"); }
    if (value.version_member()) {
      line("seen = set()");
    }
    std::string ids = "(";
    if (const auto* version = value.version_member()) {
      ids += std::to_string(version->id) + ",";
    }
    for (const auto& base : value.parents) {
      ids += std::to_string(base.id) + ",";
    }
    for (const auto& item : wire_members(value)) {
      if (!item.version) {
        if (!item.omits("binary_none")) { ids += std::to_string(item.id) + ","; }
      }
    }
    if (!auto_size::fields(value).empty()) { line("_srl_size_field_start = input.offset"); }
    open("for key in input.fields(" + ids + "))");
    open("match key");
    if (json_magic) {
      open("case 'magic' if input.protocol == Protocol.JSON");
      line("if _srl_magic_seen: raise ValueError('Duplicate magic header')");
      const auto magic_value = value.magic_field
          ? read(value.magic_field->type_name_list.front(), false) : "input.text()";
      const auto expected = "cls." + field("magic", value.magic_access) +
                            (value.magic_field ? "" : ".decode('utf-8')");
      line("if " + magic_value + " != " + expected +
           ": raise ValueError('JSON magic header mismatch')");
      line("_srl_magic_seen = True");
      --indent;
    } else if (value.has_magic()) {
      open("case 'magic' if input.protocol == Protocol.JSON");
      line("raise ValueError('Magic header omitted from JSON')");
      --indent;
    }
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      const auto& base = value.parents[i];
      const auto dst = "result." + field("base" + std::to_string(i), base.access);
      open("case " + std::to_string(base.id) + " | " + quote(base.display_name));
      line(dst + " = " + model.names.at(base.parent_class) + "._read(input, " + dst + ")");
      --indent;
    }
    for (const auto& item : wire_members(value)) {
      open("case " + std::to_string(item.id) +
           (item.modifier == member::modifier_type::variant || model.protocols == output_protocols::binary_none ? ""
                                                            : " | " + quote(item.display_name)));
      if (!item.auto_size_formats.empty()) {
        open("if " + omitted_condition(item.auto_size_formats, "input"));
        line("if _srl_size_seen_" + std::to_string(item.id) + ": raise ValueError('Duplicate auto_size field')");
        line("_srl_size_seen_" + std::to_string(item.id) + " = True");
        --indent;
      }
      if (item.wire_only) {
        line("if not (" + omitted_condition(item.auto_size_formats, "input") + "): raise ValueError('Field omitted from selected format')");
        line("_srl_size_value_" + std::to_string(item.id) + " = " + read(item.type_name_list.front(), false));
      } else {
        read_versioned_member(value, item);
      }
      if (item.size_scope == auto_size_scope::payload) {
        line("_srl_size_field_bytes_" + std::to_string(item.id) + " = input.offset - _srl_size_field_start");
      }
      --indent;
      if (item.modifier == member::modifier_type::variant && model.protocols == output_protocols::all) {
        for (std::size_t i = 0; i < item.type_name_list.size(); ++i) {
          open("case " + quote(item.display_name + ":" + item.type_name_list[i].enum_name));
          read_versioned_member(value, item, static_cast<int>(i));
          --indent;
        }
      }
    }
    line("case _: input.skip_value()");
    --indent;
    if (!auto_size::fields(value).empty()) { line("_srl_size_field_start = input.offset"); }
    --indent;
    if (json_magic) {
      line("if input.protocol == Protocol.JSON and not _srl_magic_seen: raise ValueError('Missing magic header')");
    }
    if (const auto* version = value.version_member()) {
      line("if " + std::to_string(version->id) +
           " not in seen: raise ValueError('Missing schema version')");
      check_version(value, "result." + field(version->name, version->access), true);
      for (const auto& item : wire_members(value)) {
        const auto active = version_active(version_language::python, item, *version,
                                           "result." + field(version->name, version->access));
        if (!active.empty()) {
          line("if input.protocol != Protocol.BINARY_NONE and " + std::to_string(item.id) +
               " in seen and not (" + active +
               "): raise ValueError('Field outside version lifecycle')");
        }
      }
    }
    for (const auto* item : auto_size::fields(value)) {
      open("if " + omitted_condition(item->auto_size_formats, "input"));
      line("if not _srl_size_seen_" + std::to_string(item->id) + ": raise ValueError('Missing auto_size field')");
      if (auto_size::verifies(value, model.class_auto_sizes)) {
        line("if " + (item->wire_only ? "_srl_size_value_" + std::to_string(item->id) : "result." + field(item->name, item->access)) +
             " != input.offset - _srl_size_start" +
             (item->size_scope == auto_size_scope::payload ? " - _srl_size_field_bytes_" + std::to_string(item->id) : "") +
             ": raise ValueError('auto_size field does not match record bytes')");
      }
      --indent;
    }
    if (value.reference) { line("result._srl_validate_reference_address()"); }
    line("return result");
    indent -= 2;
  }

public:
  // Bind the validated schema for one output module.
  explicit python_emitter(const schema& value) : model{value}, xml_contract{value.xml} {
    if (model.xml.enabled) {
      for (const auto* node : model.nodes) {
        if (node->type != object_type::class_type) { continue; }
        const auto configured = model.xml.classes.find(node->get_full_name());
        if (configured == model.xml.classes.end() || !configured->second.contracts.contains("mixed")) { continue; }
        const auto binding = xml_contract.bind(static_cast<const class_node&>(*node));
        if (binding.mixed && !model.xml.classes.contains(binding.mixed->fragment->get_full_name())) {
          mixed_fragments.insert(binding.mixed->fragment);
        }
      }
    }
    arithmetic_helpers = behavior::select_helpers(model.nodes,
        [&](const auto& node) { return model.names.at(&node); },
        [&](const auto name) { return field(name); });
  }
  // Generate a complete module with pre-encoded field keys and typed models.
  std::string generate() {
    behavior::require_unique_free_names(model.nodes, "python", [&](const auto& node) { return snake(model.names.at(&node)); });
    output += compression_runtime;
    if (model.protocols == output_protocols::binary_none) {
      line("_Compression._protocols = (1,)");
    }
    if (std::none_of(model.names.begin(), model.names.end(), [](const auto& entry) {
          return entry.second == "Compression";
        })) {
      line("Compression = _Compression");
    }
    line("from abc import ABC as _ABC, abstractmethod as _abstractmethod");
    if (!model.packages.empty()) {
      for (const auto& [node, selected] : model.names) {
        static_cast<void>(node);
        validate_identifier(selected, " PackageLimits PackageBudget PackageError PackageRelationship "
            "BinaryPart ModelPart PreservedExtensions ExtensionPolicy PackageResult ");
      }
    }
    if (model.xml.enabled) {
      output += xml_runtime;
      output += xml_rich_runtime;
      output += xml_identity_runtime;
      output += xml_scalar_runtime;
      output += xml_model_runtime;
      output += xml_extension_runtime;
      output += xml_projection_runtime;
      output += html_runtime;
    }
    if (!model.packages.empty()) { output += package_runtime; output += package_mce_runtime; }
    output += behavior::helpers("python", arithmetic_helpers);
    output += behavior::helpers_predicate("python", arithmetic_helpers);
    output += behavior::helpers_typed("python", arithmetic_helpers);
    for (const auto& code : model.preambles) {
      if (code.language == "python") { output += behavior::source_marker(code, "python") + code.text + "\n"; }
    }
    line("_FIELDS = (");
    ++indent;
    for (const auto& key : model.ordered_keys) {
      line("_field(" + quote(key) + "),");
    }
    --indent;
    line(")");
    for (const auto* node : model.nodes) {
      if (node->type == object_type::function_type) { free_function(static_cast<const function_node&>(*node)); continue; }
      if (model.xml.enabled && (model.names.at(node) == "XmlSchema" || model.names.at(node) == "Html" ||
          model.names.at(node) == "HtmlEvent" || model.names.at(node) == "HtmlAttribute" ||
          model.names.at(node) == "HtmlLimits" || model.names.at(node) == "XmlIdentityLimits" ||
          model.names.at(node) == "XmlMapLimits" || model.names.at(node) == "XmlMixedLimits" ||
          model.names.at(node) == "XmlPatternLimits" || model.names.at(node) == "XmlExtensionLimits" ||
          model.names.at(node) == "XmlExtensionOptions")) {
        throw std::invalid_argument{"Python XML type collides with the runtime schema helper"};
      }
      validate_identifier(
          model.names.at(node),
          " ValueError TypeError OverflowError RuntimeError RecursionError MemoryError "
          "UnicodeError UnicodeDecodeError UnicodeEncodeError Exception BaseException ");
      if (node->type == object_type::class_type) {
        object(static_cast<const class_node&>(*node));
        dispatch_adapter(static_cast<const class_node&>(*node));
        continue;
      }
      const auto& value = static_cast<const enum_node&>(*node);
      const auto& name = model.names.at(node);
      open("class " + name + "(_IntEnum)");
      line("\"\"\"Stable schema ordinals; names retain their wire spelling.\"\"\"");
      std::string names = "_" + name + "_names = (";
      for (std::size_t i = 0; i < value.enum_name_list.size(); ++i) {
        const auto constant = pascal(value.enum_name_list[i]);
        validate_identifier(constant, keywords);
        line(constant + " = " + std::to_string(i));
        names += quote(value.enum_name_list[i]) + ",";
      }
      --indent;
      line(names + ")");
    }
    if (model.xml.enabled) {
      for (const auto* node : model.nodes) {
        if (node->type != object_type::class_type) { continue; }
        const auto& selected = static_cast<const class_node&>(*node);
        if (selected.reference || mixed_fragments.contains(&selected)) { continue; }
        const auto root = xml_contract.bind(selected).root;
        line("_XML_TYPES[" + quote(root.namespace_uri.empty() ? root.local : "{" + root.namespace_uri + "}" + root.local) + "] = " + model.names.at(node));
      }
    }
    for (const auto* package : model.packages) {
      output += package_codegen::python(*package, model.names.at(package),
          [&](const auto& selected) { return type(selected); },
          [&](const auto selected) { return field(selected); },
          [&](const auto selected) { return quote(selected); }, model.xml);
    }
    return output;
  }
};
} // namespace
// Keep Python source generation inside the native C++ compiler.
std::string python(const schema& value) {
  return python_emitter{value}.generate();
}
} // namespace rohit::serializer::writer::native
