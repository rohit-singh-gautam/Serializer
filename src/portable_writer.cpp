#include "schema_projection.hpp"
#include "native_schema.hpp"
#include "xml_schema.hpp"
#include "xml_js_writer.hpp"
#include "xml_csharp_writer.hpp"
#include "xml_native_writer.hpp"
#include "xml_portable_writer.hpp"
#include "javascript_package_writer.hpp"
#include "go_package_writer.hpp"
#include "csharp_package_writer.hpp"
#include "generated_notices.hpp"
#include "version_writer.hpp"
#include "behavior_writer.hpp"
#include "construction_writer.hpp"
#include "conversion_writer.hpp"
#include "translator_writer.hpp"
#include "reference_portable_writer.hpp"
#include "comparison_writer.hpp"
#include "auto_size_writer.hpp"
#include <rohit/serializer_creator.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <iterator>
#include <limits>
#include <map>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rohit::serializer::writer::portable {
namespace {
constexpr std::string_view js_runtime =
#include "js_runtime.inc"
    ;
constexpr std::string_view js_compression_runtime =
#include "js_compression_runtime.inc"
    ;
constexpr std::string_view js_compression_declarations =
#include "js_compression_declarations.inc"
    ;
constexpr std::string_view javascript_package_runtime =
#include "javascript_package_runtime.inc"
#include "javascript_package_writer_runtime.inc"
#include "javascript_package_host_runtime.inc"
#include "javascript_package_opc_runtime.inc"
#include "javascript_package_mce_runtime.inc"
#include "javascript_package_profile_runtime.inc"
    ;
constexpr std::string_view javascript_package_declarations =
#include "javascript_package_declarations.inc"
    ;
constexpr std::string_view go_runtime =
#include "go_runtime.inc"
    ;
constexpr std::string_view go_compression_runtime =
#include "go_compression_runtime.inc"
    ;
constexpr std::string_view go_compression_provider_runtime =
#include "go_compression_provider_runtime.inc"
    ;
constexpr std::string_view go_package_runtime =
#include "go_package_runtime.inc"
#include "go_package_zip_reader_runtime.inc"
#include "go_package_zip_writer_runtime.inc"
#include "go_package_file_runtime.inc"
    ;
constexpr std::string_view go_package_opc_runtime =
#include "go_package_opc_runtime.inc"
#include "go_package_metadata_runtime.inc"
#include "go_package_mce_runtime.inc"
#include "go_package_catalog_runtime.inc"
#include "go_package_opc_writer_runtime.inc"
#include "go_package_loader_runtime.inc"
#include "go_package_composer_runtime.inc"
#include "go_package_native_runtime.inc"
    ;
constexpr std::string_view csharp_runtime =
#include "csharp_runtime.inc"
    ;
constexpr std::string_view csharp_compression_runtime =
#include "csharp_compression_runtime.inc"
    ;
constexpr std::string_view csharp_optional_compression_runtime =
#include "csharp_optional_compression_runtime.inc"
#include "csharp_zstd_compression_runtime.inc"
#include "csharp_lz4_compression_runtime.inc"
    ;
enum class target { js, go, csharp, typescript };

// Select one collision-free public helper family without changing existing schema or native helper names.
std::string go_compression_support(const std::set<std::string>& schema_names,
                                   bool binary_none_only, std::string& selected_prefix) {
  constexpr std::string_view suffixes[]{
      "", "Format", "None", "Zstd", "Lz4", "Gzip", "Zlib", "Deflate", "Code",
      "Unavailable", "InvalidOptions", "InvalidData", "ResourceLimit", "Error",
      "EncodeOptions", "DecodeOptions", "DefaultEncodeOptions", "DefaultDecodeOptions",
      "Available", "Compress", "Decompress", "Encode", "Decode", "Output", "NewOutput",
      "Input", "NewInput", "DecodeFrom", "Provider", "AvailableWithProvider"};
  const std::string base{"SerializerCompression"};
  auto prefix = base;
  std::size_t suffix_index{1};
  while (std::any_of(std::begin(suffixes), std::end(suffixes), [&](const auto suffix) {
    return schema_names.contains(prefix + std::string{suffix});
  })) {
    if (suffix_index == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error{"Go compression helper names exhausted"};
    }
    prefix = base + '_' + std::to_string(++suffix_index);
  }
  std::string result{go_compression_runtime};
  result += go_compression_provider_runtime;
  // Replace template markers once; a chosen prefix can itself contain the original prefix.
  const auto substitute = [&](std::string_view marker, std::string_view replacement) {
    std::size_t position{};
    while ((position = result.find(marker, position)) != std::string::npos) {
      result.replace(position, marker.size(), replacement);
      position += replacement.size();
    }
  };
  substitute("SerializerCompression", prefix);
  substitute("__srl_compression_binary_none_only__", binary_none_only ? "true" : "false");
  selected_prefix = prefix;
  return "// Generated compression helper prefix: " + prefix + "\n" + result;
}

// Derive every public ZIP helper identifier from its template before selecting a collision-free family.
// Existing model/free-function names and the separately selected compression family stay unchanged.
std::string go_package_support(const std::set<std::string>& schema_names,
                               std::string_view compression_prefix, bool with_opc,
                               std::string& selected_prefix) {
  std::string runtime{go_package_runtime};
  if (with_opc) { runtime += go_package_opc_runtime; }
  constexpr std::string_view marker{"SerializerPackage"};
  std::set<std::string> suffixes{""};
  std::size_t position{};
  while ((position = runtime.find(marker, position)) != std::string::npos) {
    auto end = position + marker.size();
    while (end < runtime.size() &&
        (std::isalnum(static_cast<unsigned char>(runtime[end])) || runtime[end] == '_')) {
      ++end;
    }
    suffixes.emplace(runtime.substr(position + marker.size(), end - position - marker.size()));
    position = end;
  }
  const std::string base{marker};
  auto prefix = base;
  std::size_t suffix_index{1};
  while (std::any_of(suffixes.begin(), suffixes.end(), [&](const auto& suffix) {
    return schema_names.contains(prefix + suffix);
  })) {
    if (suffix_index == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error{"Go package helper names exhausted"};
    }
    prefix = base + '_' + std::to_string(++suffix_index);
  }
  std::string result{std::move(runtime)};
  // Advance past each replacement so a chosen suffix is substituted exactly once.
  const auto substitute = [&](std::string_view original, std::string_view replacement) {
    std::size_t offset{};
    while ((offset = result.find(original, offset)) != std::string::npos) {
      result.replace(offset, original.size(), replacement);
      offset += replacement.size();
    }
  };
  substitute(marker, prefix);
  substitute("SerializerCompression", compression_prefix);
  selected_prefix = prefix;
  return "// Generated package helper prefix: " + prefix + "\n" + result;
}

// Add one C# helper container without reserving a previously valid model or output-unit name.
std::string csharp_compression_support(const std::set<std::string>& schema_names,
                                       std::string& selected_name) {
  const std::string base{"SerializerCompression"};
  auto name = base;
  std::size_t suffix{1};
  while (schema_names.contains(name)) {
    if (suffix == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error{"C# compression helper names exhausted"};
    }
    name = base + '_' + std::to_string(++suffix);
  }
  std::string result{csharp_compression_runtime};
  const auto closing = result.rfind("\n  }");
  if (closing == std::string::npos) {
    throw std::logic_error{"C# compression helper closing declaration is missing"};
  }
  result.insert(closing, csharp_optional_compression_runtime);
  std::size_t position{};
  while ((position = result.find(base, position)) != std::string::npos) {
    result.replace(position, base.size(), name);
    position += name.size();
  }
  selected_name = name;
  return "// Generated C# compression helper name: " + name + "\n" + result;
}

// Keep the package helper disjoint from every authored model and outer output unit.
std::string csharp_package_name(const std::set<std::string>& schema_names) {
  const std::string base{"SerializerPackage"};
  auto name = base;
  std::size_t suffix{1U};
  while (schema_names.contains(name)) {
    if (suffix == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error{"C# package helper names exhausted"};
    }
    name = base + '_' + std::to_string(++suffix);
  }
  return name;
}

// Keep one matching JS implementation/declaration container without reserving a schema name.
std::string js_compression_name(const std::set<std::string>& schema_names) {
  const std::string base{"SerializerCompression"};
  auto name = base;
  std::size_t suffix{1};
  while (schema_names.contains(name) || schema_names.contains(name + "ErrorBase")) {
    if (suffix == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error{"JavaScript compression helper names exhausted"};
    }
    name = base + '_' + std::to_string(++suffix);
  }
  return name;
}

// Keep one matching JS implementation/declaration container without reserving a schema name.
std::string js_compression_support(const std::set<std::string>& schema_names, bool declarations) {
  const std::string base{"SerializerCompression"};
  const auto name = js_compression_name(schema_names);
  std::string result{declarations ? js_compression_declarations : js_compression_runtime};
  std::size_t position{};
  while ((position = result.find(base, position)) != std::string::npos) {
    result.replace(position, base.size(), name);
    position += name.size();
  }
  return "// Generated JS/TS compression helper name: " + name + "\n" + result;
}

// Select the whole package helper family without displacing previously valid schema names.
std::string javascript_package_name(const std::set<std::string>& schema_names) {
  const std::string base{"SerializerPackage"};
  auto name = base;
  std::size_t suffix{1};
  while (schema_names.contains(name) || schema_names.contains(name + "ErrorBase") || schema_names.contains(name + "Saxes")) {
    if (suffix == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error{"JavaScript package helper names exhausted"};
    }
    name = base + '_' + std::to_string(++suffix);
  }
  return name;
}

// Bind the package implementation to independently selected package and compression helpers.
std::string javascript_package_support(const std::set<std::string>& schema_names, bool declarations) {
  const auto helper = javascript_package_name(schema_names);
  std::string result{declarations ? javascript_package_declarations : javascript_package_runtime};
  const auto replace = [&](const std::string& from, const std::string& to) {
    std::size_t position{};
    while ((position = result.find(from, position)) != std::string::npos) {
      result.replace(position, from.size(), to);
      position += to.size();
    }
  };
  replace("SerializerPackage", helper);
  replace("SerializerCompression", js_compression_name(schema_names));
  if (!declarations) {
    result = "import {SaxesParser as " + helper + "Saxes} from 'saxes';\n" + result;
  }
  return "// Generated JS/TS package helper name: " + helper + "\n" + result;
}

// C# nested model members must not shadow unqualified runtime XML helper lookups.
constexpr std::string_view csharp_xml_identifiers[]{
    "XmlExtensionLimits", "XmlExtensionOptions", "SrlXmlExtensionSlot", "SrlXmlExtensionContract", "SrlXmlForeignNode", "SrlXmlForeignChild", "SrlXmlPreserved", "SrlXmlExtensions", "SrlXmlExtensionSize", "SrlXmlExtensionNcName", "SrlXmlExtensionName", "SrlXmlExtensionReject", "SrlXmlExtensionPreserve", "SrlXmlExtensionMaxSlots", "SrlXmlExtensionMaxItems", "SrlXmlExtensionMaxPreservedBytes",
    "XmlDigestLimits", "SrlXmlDefaultDigestBytes", "SrlXmlRevisionLexical", "SrlXmlHexDigit", "SrlXmlDigestRead", "SrlXmlDigestText",
    "Html", "HtmlLimits", "HtmlEvent", "HtmlAttribute", "SrlHtmlMaxInputBytes", "SrlHtmlMaxOutputBytes",
    "SrlHtmlMaxEvents", "SrlHtmlMaxDepth", "SrlHtmlMaxAttributesPerElement", "SrlHtmlMaxTextBytes",
    "SrlXmlNode", "SrlXmlBudget", "SrlXmlFields", "SrlXmlOutput", "SrlXmlInput", "SrlXmlInputFields", "SrlXmlRule", "XmlMapLimits", "XmlMixedLimits", "SrlXmlMixedBudget", "SrlXmlMixedMaxFragments", "SrlXmlMixedMaxTextBytes", "XmlIdentityLimits", "SrlXmlIdentity", "SrlXmlIdentityScope", "SrlXmlIdentityConstraint", "SrlXmlIdentityPath", "SrlXmlIdentityField", "SrlXmlIdentityMetadata", "SrlXmlIdentityMaxConstraints", "SrlXmlIdentityMaxFields", "SrlXmlIdentityMaxDepth", "SrlXmlIdentityMaxSelectedNodes", "SrlXmlIdentityMaxTuples", "SrlXmlIdentityMaxTupleBytes", "SrlXmlIdentityMaxWorkUnits", "SrlXmlName", "SrlXmlTrim", "SrlXmlParse",
    "SrlXmlBoolean", "SrlXmlInteger", "SrlXmlFloat", "SrlXmlFloatText", "SrlXmlScalarText", "SrlXmlWrite"};

// Quote schema spelling without changing wire names or printable Unicode.
std::string quote(std::string_view text) {
  std::string result{"\""};
  for (const auto value : text) {
    switch (value) {
    case '\\':
      result += "\\\\";
      break;
    case '"':
      result += "\\\"";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    default:
      if (static_cast<unsigned char>(value) < 0x20) {
        throw std::invalid_argument{"Control character in schema name"};
      }
      result += value;
    }
  }
  return result + '"';
}

// Convert schema identifiers to deterministic native presentation names.
std::string name(std::string_view text, bool upper, bool rename) {
  std::string result{};
  bool capitalize = upper && rename;
  for (const auto value : text) {
    if (rename && value == '_') {
      capitalize = true;
    } else {
      result +=
          capitalize ? static_cast<char>(std::toupper(static_cast<unsigned char>(value))) : value;
      capitalize = false;
    }
  }
  if (rename && !upper && !result.empty()) {
    result.front() = static_cast<char>(std::tolower(static_cast<unsigned char>(result.front())));
  }
  return result;
}

// Reject malformed identifiers and language keywords before output publication.
void validate_name(const std::string& value, target language) {
  constexpr std::string_view common =
      " break case const continue default else false for if return switch true var ";
  constexpr std::string_view js =
      " await class debugger delete do enum export extends finally function implements import in "
      "instanceof interface let new null package private protected public static super this throw "
      "try typeof void while with yield constructor prototype __proto__ ";
  constexpr std::string_view go =
      " chan defer fallthrough func go goto import interface map package range select struct type ";
  constexpr std::string_view cs =
      " abstract as base bool byte catch char checked class decimal delegate do double enum event "
      "explicit extern finally fixed float goto implicit in int interface internal is lock long "
      "namespace new null object operator out override params private protected public readonly "
      "ref sbyte sealed short sizeof stackalloc static string struct this throw try typeof uint "
      "ulong unchecked unsafe ushort using virtual void volatile while ";
  const auto keywords = language == target::go ? go : language == target::csharp ? cs : js;
  const auto token = " " + value + " ";
  if (!std::regex_match(value, std::regex{"[A-Za-z_][A-Za-z_0-9]*"}) || value == "_" ||
      common.find(token) != std::string_view::npos ||
      keywords.find(token) != std::string_view::npos) {
    throw std::invalid_argument{"Invalid or reserved generated identifier: " + value};
  }
}

// Derive a fixed wire width from the schema primitive.
int width(std::string_view type) {
  if (type.ends_with("8")) {
    return 1;
  }
  if (type.ends_with("16")) {
    return 2;
  }
  if (type.ends_with("32")) {
    return 4;
  }
  if (type.ends_with("64")) {
    return 8;
  }
  throw std::invalid_argument{"Unsupported integer type: " + std::string{type}};
}

// Emit direct native field codecs from the shared resolved C++ syntax tree.
class emitter {
  target language;
  const portable_options& options;
  xml_binding::schema xml_schema_;
  std::map<const class_node*, const member*> xml_mixed_variants_{};
  std::set<const class_node*> xml_mixed_storage_only_{};
  std::string unit;
  std::string go_xml_operation_name{"XmlOperationOptions"};
  behavior::checked_helpers arithmetic_helpers{};
  std::string source{};
  std::size_t level{};
  std::vector<const syntax_node*> nodes{};
  std::vector<const package_node*> packages{};
  std::string go_package_prefix{};
  std::string csharp_package_prefix{};
  std::map<const syntax_node*, std::string> names{};
  std::map<std::string, std::size_t> wire_names{};
  std::set<std::string> globals{"ReadPolicy",   "STRICT",   "COMPATIBLE",    "FLEXIBLE",
                                "Protocol",     "Limits",   "DefaultLimits", "SrlWriter",
                                "SrlReader",    "SrlUtf8",  "SrlCompare",    "SrlReadEnum",
                                "SrlWriteEnum", "SrlField", "SrlFields",     "System"};

  // Select target-specific syntax; declaration output shares JavaScript naming.
  std::string choose(std::string_view js, std::string_view go, std::string_view cs) const {
    return std::string{language == target::go ? go : language == target::csharp ? cs : js};
  }
  // Append an indented source line.
  void line(const std::string& text = {}) {
    source.append(level * 2, ' ');
    source += text + '\n';
  }
  // Open a scope and increase indentation.
  void open(const std::string& text) {
    line(text + " {");
    ++level;
  }
  // Close the current scope.
  void close() {
    --level;
    line("}");
  }
  // Append a native statement terminator.
  void statement(const std::string& text) {
    line(text + (language == target::go ? "" : ";"));
  }
  // Open a native conditional.
  void condition(const std::string& expression) {
    open(language == target::go ? "if " + expression : "if (" + expression + ")");
  }
  // Qualify a protocol constant.
  std::string protocol(std::string_view value) const {
    return (language == target::go ? "" : "Protocol.") + std::string{value};
  }
  // Resolve a field name and Go export visibility.
  std::map<std::string, std::string> projection_fields_{};
  bool reference_fields_{};

  std::string field(std::string_view value, access_type access = access_type::public_access) const {
    if (const auto found = projection_fields_.find(std::string{value}); found != projection_fields_.end()) { return found->second; }
    const bool exported_reference_field = language == target::go && reference_fields_ &&
        (value == "document_id" || value == "owner_id" || value == "generation" || value == "field_path");
    auto result = name(value,
                       language == target::csharp ||
                           (language == target::go && access == access_type::public_access),
                       options.rename_identifiers || exported_reference_field);
    validate_name(result, language);
    if (language == target::go && access == access_type::public_access &&
        !std::isupper(static_cast<unsigned char>(result.front()))) {
      throw std::invalid_argument{"Public Go names must be exported: " + result};
    }
    return language == target::js && access != access_type::public_access ? "#" + result : result;
  }
  // Map schema scalar storage to native types.
  std::string type(const rohit::serializer::type_name& value) const {
    if (value.is_optional()) {
      const auto inner = type(value.arguments.front());
      return choose("([] | [" + inner + "])", "*" + inner, "System.ValueTuple<" + inner + ">?");
    }
    if (value.is_sequence()) {
      const auto inner = type(value.arguments.front());
      return choose(inner + "[]", "[]" + inner, "System.Collections.Generic.List<" + inner + ">");
    }
    if (value.is_associative()) {
      const auto key = type(value.arguments[0]);
      const auto inner = type(value.arguments[1]);
      return choose("Map<" + key + ", " + inner + ">", "map[" + key + "]" + inner,
                    "System.Collections.Generic.Dictionary<" + key + ", " + inner + ">");
    }
    if (value.type != object_type::primitive) {
      return (language == target::go && value.type == object_type::class_type ? "*" : "") +
             names.at(value.resolved_node);
    }
    if (value.is_digest()) {
      return choose("number[]", "[]byte", "byte[]");
    }
    if (value.name.starts_with("version")) {
      return "string";
    }
    if (language == target::js || language == target::typescript) {
      return value.name == "string"                            ? "string"
             : value.name == "bool"                            ? "boolean"
             : value.name == "int64" || value.name == "uint64" ? "bigint"
                                                               : "number";
    }
    if (language == target::go) {
      return value.name == "char"     ? "byte"
             : value.name == "float"  ? "float32"
             : value.name == "double" ? "float64"
                                      : value.name;
    }
    static const std::map<std::string, std::string> primitives{
        {"char", "byte"},     {"int8", "sbyte"}, {"uint8", "byte"},  {"int16", "short"},
        {"uint16", "ushort"}, {"int32", "int"},  {"uint32", "uint"}, {"int64", "long"},
        {"uint64", "ulong"},  {"bool", "bool"},  {"float", "float"}, {"double", "double"},
        {"string", "string"}};
    return primitives.at(value.name);
  }
  // Resolve map key metadata in the common syntax tree.
  rohit::serializer::type_name key_type(const member& value) const {
    rohit::serializer::type_name key{std::string{value.key}, nullptr};
    key.type = value.key_node ? value.key_node->type : object_type::primitive;
    key.resolved_node = value.key_node;
    return key;
  }
  // Compose the language-specific enum constant expression.
  std::string enum_constant(const rohit::serializer::type_name& value,
                            std::string_view item) const {
    return names.at(value.resolved_node) + (language == target::go ? "" : ".") +
           name(item, true, options.rename_identifiers);
  }
  // Construct a fresh schema default.
  std::string initial(const rohit::serializer::type_name& value) const {
    if (value.is_optional()) { return choose("[]", "nil", "null"); }
    if (value.is_sequence()) { return choose("[]", "make(" + type(value) + ", 0)", "new " + type(value) + "()"); }
    if (value.is_associative()) { return choose("new Map()", "make(" + type(value) + ")", "new " + type(value) + "()"); }
    if (value.is_digest()) {
      const auto size = std::to_string(value.digest_extent);
      return choose("Array(" + size + ").fill(0)", "make([]byte, " + size + ")",
                    "new byte[" + size + "]");
    }
    if (value.type == object_type::class_type) {
      return choose("new ", "New", "new ") + names.at(value.resolved_node) + "()";
    }
    if (value.type == object_type::enum_type) {
      return enum_constant(
          value, static_cast<const enum_node&>(*value.resolved_node).enum_name_list.front());
    }
    if (value.name == "string" || value.name.starts_with("version")) {
      return "\"\"";
    }
    if (value.name == "bool") {
      return "false";
    }
    if ((language == target::js || language == target::typescript) &&
        (value.name == "int64" || value.name == "uint64")) {
      return "0n";
    }
    return "0";
  }
  // Validate portable literals before translating their native spelling.
  std::string default_value(const member& value) const {
    if (value.initializer) {
      return construction::initializer(*value.initializer,
          language == target::go ? "go" : language == target::csharp ? "csharp" : "js",
          [&](const auto& item) { return type(item); },
          [&](const auto& item) { return default_value(item); },
          [&](auto item) { return field(item, access_type::public_access); });
    }
    if (!value.default_factory.empty()) {
      return construction::factory_call(names.at(value.type_name_list.front().resolved_node),
          field(value.default_factory, access_type::public_access), language == target::go ? "go" : "js");
    }
    const auto& item = value.type_name_list.front();
    if (item.name.starts_with("version")) {
      return schema_version::trim(value.default_value);
    }
    const auto first = value.default_value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
      return initial(item);
    }
    const auto last = value.default_value.find_last_not_of(" \t\r\n");
    const auto text = value.default_value.substr(first, last - first + 1);
    if (value.magic && item.name == "char") {
      // Typed magic is canonicalized to its validated byte value by the parser.
      return text;
    }
    if (item.type == object_type::enum_type) {
      const auto separator = text.rfind("::");
      const auto constant = separator == std::string::npos ? text : text.substr(separator + 2);
      if (separator != std::string::npos) {
        const auto prefix = text.substr(0, separator);
        if (prefix != item.name && prefix != item.resolved_node->name &&
            prefix != item.resolved_node->get_full_name()) {
          throw std::invalid_argument{"Enum default names the wrong type: " + value.name};
        }
      }
      const auto& values = static_cast<const enum_node&>(*item.resolved_node).enum_name_list;
      if (std::find(values.begin(), values.end(), constant) != values.end()) {
        return enum_constant(item, constant);
      }
    } else if (item.type == object_type::primitive) {
      if (item.name == "string" &&
          std::regex_match(text, std::regex{"\"([^\"\\\\]|\\\\[\"\\\\bfnrt])*\""})) {
        return text;
      }
      if (item.name == "bool" && (text == "true" || text == "false")) {
        return text;
      }
      if (item.name == "char" && text.size() >= 3 && text.front() == '\'' && text.back() == '\'') {
        if (text.size() == 3 && text[1] != '\\' && text[1] != '\'' &&
            static_cast<unsigned char>(text[1]) < 128) {
          return std::to_string(static_cast<unsigned char>(text[1]));
        }
        if (text.size() == 4 && text[1] == '\\') {
          const std::string_view escapes{"bfnrt\\\"'0"};
          constexpr std::array values{8, 12, 10, 13, 9, 92, 34, 39, 0};
          const auto index = escapes.find(text[2]);
          if (index != std::string_view::npos) {
            return std::to_string(values[index]);
          }
        }
      }
      if (item.name == "float" || item.name == "double") {
        if (std::regex_match(text, std::regex{"-?(0|[1-9][0-9]*)(\\.[0-9]+)?([eE][+-]?[0-9]+)?"})) {
          float single{};
          double real{};
          const auto parsed = item.name == "float"
                                  ? std::from_chars(text.data(), text.data() + text.size(), single)
                                  : std::from_chars(text.data(), text.data() + text.size(), real);
          if (parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()) {
            if (language == target::csharp) {
              return text + (item.name == "float" ? "F" : "D");
            }
            if ((language == target::js || language == target::typescript) &&
                item.name == "float") {
              return "Math.fround(" + text + ")";
            }
            return text;
          }
        }
      } else if (item.name.starts_with("int") || item.name.starts_with("uint")) {
        const auto bits = width(item.name) * 8;
        const bool is_unsigned = item.name.starts_with("u");
        std::uint64_t unsigned_value{};
        std::int64_t signed_value{};
        const auto parsed =
            is_unsigned ? std::from_chars(text.data(), text.data() + text.size(), unsigned_value)
                        : std::from_chars(text.data(), text.data() + text.size(), signed_value);
        bool valid = std::regex_match(text, std::regex{"-?(0|[1-9][0-9]*)"}) &&
                     parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
        if (valid && bits < std::numeric_limits<std::uint64_t>::digits) {
          valid = is_unsigned ? unsigned_value < (std::uint64_t{1} << bits)
                              : signed_value >= -(std::int64_t{1} << (bits - 1)) &&
                                    signed_value < (std::int64_t{1} << (bits - 1));
        }
        if (valid) {
          if ((language == target::js || language == target::typescript) && bits == 64) {
            return text + "n";
          }
          if (language == target::csharp && bits == 64) {
            return text + (is_unsigned ? "UL" : "L");
          }
          if (language == target::csharp && item.name == "uint32") {
            return text + "U";
          }
          return text;
        }
      }
    }
    throw std::invalid_argument{"Expected a supported literal default: " + value.name};
  }
  // Register flattened names and reject namespace-flattening collisions.
  void register_nodes(const std::vector<std::unique_ptr<syntax_node>>& values,
                      const std::string& prefix = {}) {
    for (const auto& value : values) {
      if (value->type == object_type::generic_definition || value->type == object_type::native_code) { continue; }
      const auto generated = prefix + name(value->name, true, options.rename_identifiers);
      if (value->type == object_type::namespace_type) {
        register_nodes(static_cast<const namespace_node&>(*value).statements, generated);
        continue;
      }
      validate_name(generated, language);
      const bool same_free_name = value->type == object_type::function_type &&
          std::any_of(names.begin(), names.end(), [&](const auto& item) {
            return item.second == generated && item.first->type == object_type::function_type &&
                item.first->get_full_name() == value->get_full_name();
          });
      if (same_free_name && language != target::csharp) {
        throw std::invalid_argument{choose("js", "go", "csharp") + ": standalone overload/name erasure collision: " + generated};
      }
      const bool overload = language == target::csharp && same_free_name;
      if ((!globals.insert(generated).second && !overload) || (language == target::csharp && generated == unit) ||
          generated.starts_with("srl")) {
        throw std::invalid_argument{"Generated type name collision: " + generated};
      }
      names.emplace(value.get(), generated);
      if (value->type == object_type::package_type) {
        packages.push_back(static_cast<const package_node*>(value.get()));
        continue;
      }
      nodes.push_back(value.get());
    }
  }
  // Validate native names, default construction, and the supported owning subset.
  void validate() {
    for (const auto* node : nodes) {
      if (node->type == object_type::function_type) { continue; }
      if (!globals.insert(names.at(node) + "Names").second) {
        throw std::invalid_argument{"Generated helper name collision"};
      }
      if (node->type == object_type::enum_type) {
        const auto& values = static_cast<const enum_node&>(*node).enum_name_list;
        if (values.empty()) {
          throw std::invalid_argument{"Empty enum"};
        }
        std::set<std::string> constants{};
        for (const auto& value : values) {
          const auto identifier = name(value, true, options.rename_identifiers);
          validate_name(identifier, language);
          if (!constants.insert(identifier).second) {
            throw std::invalid_argument{"Enum naming collision"};
          }
          if (language == target::go && !globals.insert(names.at(node) + identifier).second) {
            throw std::invalid_argument{"Enum global naming collision"};
          }
        }
        continue;
      }
      const auto& value = static_cast<const class_node&>(*node);
      reference_fields_ = value.reference.has_value();
      if (language == target::csharp && value.reference) {
        for (const auto helper : {"Bind", "Resolve", "ReadValue", "WithValue", "Unbind", "IsBound", "SrlValidateReferenceAddress",
            "SrlReferencePathMatches", "Encode", "Decode", "Read", "Write"}) {
          if (names.at(node) == helper) { throw std::invalid_argument{"C# reference helper/class name collision: " + names.at(node)}; }
        }
      }
      if (value.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
          (value.attributes & class_attributes::packed) != class_attributes::none) {
        throw std::invalid_argument{"Portable output supports owning classes only: " + value.name};
      }
      if (language == target::go) {
        for (const auto prefix : {"New", "Decode", "read"}) {
          if (!globals.insert(std::string{prefix} + names.at(node)).second) {
            throw std::invalid_argument{"Generated helper name collision"};
          }
        }
        if (options.xml.enabled &&
            (!globals.insert("Decode" + names.at(node) + "XML").second ||
             !globals.insert("Decode" + names.at(node) + "XMLWithMapLimits").second ||
             !globals.insert("Decode" + names.at(node) + "XMLWithMixedLimits").second ||
             !globals.insert("Decode" + names.at(node) + "XMLWithContentLimits").second ||
             !globals.insert("Decode" + names.at(node) + "XMLWithIdentityLimits").second ||
             !globals.insert("Decode" + names.at(node) + "XMLWithPolicies").second ||
             !globals.insert("Decode" + names.at(node) + "XMLWithDigestLimits").second ||
             !globals.insert("Decode" + names.at(node) + "XMLWithAllPolicies").second ||
             !globals.insert("Decode" + names.at(node) + "XMLWithAllBounds").second ||
             !globals.insert("Decode" + names.at(node) + "XMLWithExtensionOptions").second ||
             !globals.insert("srlXmlDecode" + names.at(node)).second)) {
          throw std::invalid_argument{"Generated XML helper name collision"};
        }
      }
      std::set<std::string> fields{"encode", "decode", "write", "read",        "Encode",
                                   "Decode", "Read",   "Write", names.at(node)};
      std::set<std::string> xml_methods{};
      if (options.xml.enabled) {
        if (language == target::go) {
          xml_methods = {"EncodeXML", "EncodeXMLWithMapLimits", "EncodeXMLWithMixedLimits", "EncodeXMLWithContentLimits", "EncodeXMLWithIdentityLimits", "EncodeXMLWithPolicies", "EncodeXMLWithDigestLimits", "EncodeXMLWithAllPolicies", "EncodeXMLWithAllBounds", "EncodeXMLWithExtensionOptions", "srlXmlNode", "srlXmlEncode"};
        } else if (language == target::csharp) {
          xml_methods = {"EncodeXml", "DecodeXml", "XmlNode", "XmlFromNode", "XmlWrite", "XmlRead"};
          for (const auto identifier : csharp_xml_identifiers) { xml_methods.emplace(identifier); }
          if (!xml_mixed_storage_only_.contains(&value) && !(value.reference && value.reference->value_projection)) {
            const auto contract = xml_schema_.bind(value);
            for (std::size_t index{}; index < contract.fields.size(); ++index) {
              if (xml_portable::needs_scalar_rule(contract.fields[index])) {
                xml_methods.emplace(xml_portable::scalar_rule_name(index, "csharp"));
              }
            }
          }
        } else {
          xml_methods = {"encodeXml", "decodeXml", "decodeXmlChunks", "xmlBindNamespace", "_srlXmlToken", "_srlXmlMixed", "_srlXmlRead", "_srlXmlWrite"};
        }
        fields.insert(xml_methods.begin(), xml_methods.end());
        // Authored behavior shares the same member namespace as generated XML methods.
        const auto reject_xml_method = [&](const std::string& identifier) {
          const auto checked = identifier.starts_with('#') ? identifier.substr(1) : identifier;
          if (xml_methods.contains(checked)) {
            throw std::invalid_argument{"Generated XML method collision: " + identifier};
          }
        };
        for (const auto& method : value.functions) {
          reject_xml_method(field(method.name, method.access));
        }
        for (const auto& factory : value.constructions) {
          reject_xml_method(field(factory.constructor ? "create" : factory.name, factory.access));
        }
        for (const auto& conversion : value.conversions) {
          reject_xml_method(field(conversion.name, conversion.access));
        }
      }
      if (language == target::csharp) {
        // Unqualified type/helper references inside methods must not bind to fields.
        fields.insert(globals.begin(), globals.end());
      }
      const auto add = [&](const std::string& identifier) {
        const auto checked = identifier.starts_with('#') ? identifier.substr(1) : identifier;
        validate_name(checked, language);
        if (!fields.insert(checked).second) {
          throw std::invalid_argument{"Generated field collision: " + identifier};
        }
      };
      for (const auto& item : value.member_list) {
        if (!item.array_exposure) { continue; }
        const auto access = item.array_exposure->access;
        for (const auto method : {"get", "size", "values"}) { add(field(method, access)); }
        if (!item.array_exposure->readonly) { add(field("set", access)); }
        for (const auto& method : value.functions) {
          const auto identifier = field(method.name, method.access);
          if (fields.contains(identifier.starts_with('#') ? identifier.substr(1) : identifier)) {
            throw std::invalid_argument{"Array exposure method naming collision: " + method.name};
          }
        }
        for (const auto& factory : value.constructions) {
          const auto identifier = field(factory.constructor ? "create" : factory.name, factory.access);
          if (fields.contains(identifier.starts_with('#') ? identifier.substr(1) : identifier)) {
            throw std::invalid_argument{"Array exposure factory naming collision"};
          }
        }
        for (const auto& conversion : value.conversions) {
          const auto identifier = field(conversion.name, conversion.access);
          if (fields.contains(identifier.starts_with('#') ? identifier.substr(1) : identifier)) {
            throw std::invalid_argument{"Array exposure conversion naming collision"};
          }
        }
      }
      for (std::size_t i = 0; i < value.parents.size(); ++i) {
        add(field(std::string(language == target::go ? "Base" : "base") + std::to_string(i), value.parents[i].access));
      }
      for (const auto& item : value.member_list) {
        if (item.wire_only) { continue; }
        const auto identifier = field(item.name, item.access);
        if (item.modifier != member::modifier_type::none && !item.default_value.empty() && !item.initializer) {
          throw std::invalid_argument{"Collection and union defaults are unsupported"};
        }
        if (item.modifier == member::modifier_type::map &&
            ((item.key_node && item.key_node->type != object_type::enum_type) ||
             item.key == "float" || item.key == "double")) {
          throw std::invalid_argument{"Maps require integral, boolean, string, or enum keys"};
        }
        for (const auto& alternative : item.type_name_list) {
          if (alternative.resolved_node == node &&
              (item.modifier == member::modifier_type::none ||
               item.modifier == member::modifier_type::variant || item.fixed_extent != 0)) {
            throw std::invalid_argument{"Direct self-containing owning default"};
          }
        }
        if (item.modifier == member::modifier_type::variant && !tagged_variant(item)) {
          add(identifier + "Index");
          for (const auto& alternative : item.type_name_list) {
            add(identifier + name(alternative.enum_name, true, true));
          }
        } else {
          add(identifier);
        }
        if (item.modifier == member::modifier_type::none) {
          (void)default_value(item);
        }
      }
    }
    reference_fields_ = false;
  }
  // Return the immutable pre-encoded key shared by every use of a wire name.
  std::string wire_key(const std::string& value) const {
    return choose("srlFields[", "srlFields[", "SrlFields[") + std::to_string(wire_names.at(value)) +
           "]";
  }
  // Pre-encode schema keys once, avoiding UTF-8 allocation on each encoded field.
  void emit_fields() {
    std::vector<std::string> ordered{};
    const auto add = [&](const std::string& value) {
      if (wire_names.emplace(value, ordered.size()).second) {
        ordered.push_back(value);
      }
    };
    for (const auto* node : nodes) {
      if (node->type != object_type::class_type) {
        continue;
      }
      const auto& value = static_cast<const class_node&>(*node);
      if (value.has_magic() && !value.magic_omits("json")) {
        add("magic");
      }
      for (const auto& base : value.parents) {
        add(base.display_name);
      }
      for (const auto& item : wire_members(value)) {
        if (item.modifier == member::modifier_type::variant) {
          for (const auto& alternative : item.type_name_list) {
            add(item.display_name + ":" + alternative.enum_name);
          }
        } else {
          add(item.display_name);
        }
      }
    }
    line(choose("const srlFields = [", "var srlFields = []srlField{",
                "private static readonly SrlField[] SrlFields = {"));
    ++level;
    for (const auto& value : ordered) {
      line(choose("srlMakeField(", "srlMakeField(", "new SrlField(") + quote(value) + "),");
    }
    --level;
    line(choose("];", "}", "};"));
  }
  // Emit a scalar read expression, optionally merging an ordinary nested field.
  std::string read_value(const rohit::serializer::type_name& value, bool collection,
                         const std::string& previous = {}) const {
    if (value.is_optional() || value.is_sequence() || value.is_associative()) {
      const auto suffix = value.is_optional() ? "Optional" : value.is_sequence() ? "Array" : "Map";
      const auto& inner = value.arguments.back();
      const auto decoder = [&](const rohit::serializer::type_name& item, bool context) {
        const auto expression = read_value(item, context);
        return choose("() => " + expression, "func() " + type(item) + " { return " + expression + " }",
                      "() => " + expression);
      };
      return choose("srlRead", "srlRead", "SrlRead") + suffix + "(input, " +
             (value.is_associative() ? decoder(value.arguments[0], true) + ", " : "") +
             decoder(inner, value.is_optional() ? collection : true) + ")";
    }
    if (value.is_digest()) {
      return choose("srlReadDigest", "srlReadDigest", "SrlReadDigest") +
             "(input, " + std::to_string(value.digest_extent) + ")";
    }
    if (value.type == object_type::class_type) {
      const auto identifier = names.at(value.resolved_node);
      const auto old = previous.empty() ? initial(value) : previous;
      return choose(identifier + ".read", "read" + identifier, identifier + ".Read") + "(input, " +
             old + ")";
    }
    if (value.type == object_type::enum_type) {
      const auto identifier = names.at(value.resolved_node);
      const auto expression = choose("srlReadEnum", "srlReadEnum", "SrlReadEnum") + "(input, " +
                              identifier + "Names, " + (collection ? "true" : "false") + ")";
      return language == target::go       ? identifier + "(" + expression + ")"
             : language == target::csharp ? "(" + identifier + ")" + expression
                                          : expression;
    }
    if (value.name.starts_with("version")) {
      return "input.version(" + std::string{value.name.back()} + ")";
    }
    if (value.name == "string") {
      return "input.text()";
    }
    if (value.name == "bool") {
      return choose("input.bool()", "input.boolean()", "input.boolean()");
    }
    if (value.name == "char") {
      return "input.character()";
    }
    if (value.name == "float") {
      return choose("input.floating(true)", "input.float32()", "input.float32()");
    }
    if (value.name == "double") {
      return "input.floating(false)";
    }
    const auto expression = "input.integer(" + std::to_string(width(value.name)) + ", " +
                            (value.name.starts_with("u") ? "true" : "false") + ")";
    return language == target::go       ? type(value) + "(" + expression + ")"
           : language == target::csharp ? "unchecked((" + type(value) + ")" + expression + ")"
                                        : expression;
  }
  // Write a scalar directly with explicit wire width and enum context.
  void write_value(const rohit::serializer::type_name& value, const std::string& expression,
                   bool collection) {
    if (value.is_optional()) {
      const auto present = choose(expression + ".length === 1", expression + " != nil", expression + ".HasValue");
      if (language == target::js) {
        condition("!Array.isArray(" + expression + ") || " + expression + ".length > 1");
        statement("throw new RangeError(\"Invalid optional\")");
        close();
      }
      statement("output.beginOptional(" + present + ")");
      condition(present);
      write_value(value.arguments.front(), choose("(" + expression + ")[0]", "(*(" + expression + "))", "(" + expression + ").GetValueOrDefault().Item1"), collection);
      close();
      statement("output.endArray()");
    } else if (value.is_sequence() || value.is_associative()) {
      open("");
      const auto& item = value.arguments.back();
      if (value.is_associative()) {
        const auto& key = value.arguments[0];
        if (language == target::js) {
          local("keys", "Array.from(" + expression + ".keys()).sort(srlCompare)");
        } else if (language == target::go) {
          local("keys", "make([]" + type(key) + ", 0, len(" + expression + "))");
          open("for key := range " + expression);
          statement("keys = append(keys, key)");
          close();
          line("sort.Slice(keys, func(i, j int) bool { return " +
               std::string{key.name == "bool" ? "!keys[i] && keys[j]" : "keys[i] < keys[j]"} + " })");
        } else {
          local("keys", "new System.Collections.Generic.List<" + type(key) + ">(" + expression + ".Keys)");
          statement("keys.Sort(SrlCompare)");
        }
        const auto count = choose("keys.length", "len(keys)", "keys.Count");
        statement("output.beginArray(" + count + ")");
        open(choose("for (let index = 0; index < " + count + "; ++index)", "for index := 0; index < " + count + "; index++", "for (int index = 0; index < " + count + "; ++index)"));
        local("entryKey", "keys[index]");
        statement("output.element(index)");
        condition("output.json()"); statement("output.raw(\"{\\\"key\\\":\")"); close();
        write_value(key, "entryKey", true);
        condition("output.json()"); statement("output.raw(\",\\\"value\\\":\")"); close();
        write_value(item, choose("(" + expression + ").get(entryKey)", "(" + expression + ")[entryKey]", "(" + expression + ")[entryKey]"), true);
        condition("output.json()"); statement("output.raw(\"}\")"); close();
        close();
      } else {
        const auto count = choose("(" + expression + ").length", "len(" + expression + ")", "(" + expression + ").Count");
        const auto index = "srl_index_" + std::to_string(level);
        statement("output.beginArray(" + count + ")");
        open(choose("for (let " + index + " = 0; " + index + " < " + count + "; ++" + index + ")", "for " + index + " := 0; " + index + " < " + count + "; " + index + "++", "for (int " + index + " = 0; " + index + " < " + count + "; ++" + index + ")"));
        statement("output.element(" + index + ")");
        write_value(item, "(" + expression + ")[" + index + "]", true);
        close();
      }
      statement("output.endArray()");
      close();
    } else if (value.is_digest()) {
      statement(choose("srlWriteDigest", "srlWriteDigest", "SrlWriteDigest") +
                "(output, " + expression + ", " + std::to_string(value.digest_extent) + ")");
    } else if (value.type == object_type::class_type) {
      statement(expression + choose(".write(output)", ".write(output)", ".Write(output)"));
    } else if (value.type == object_type::enum_type) {
      const auto converted = language == target::go       ? "int(" + expression + ")"
                             : language == target::csharp ? "(int)" + expression
                                                          : expression;
      statement(choose("srlWriteEnum", "srlWriteEnum", "SrlWriteEnum") + "(output, " + converted +
                ", " + names.at(value.resolved_node) + "Names, " + (collection ? "true" : "false") +
                ")");
    } else if (value.name.starts_with("version")) {
      statement("output.version(" + expression + ", " + std::string{value.name.back()} + ")");
    } else if (value.name == "string") {
      statement("output.text(" + expression + ")");
    } else if (value.name == "bool") {
      statement(choose("output.bool(", "output.boolean(", "output.boolean(") + expression + ")");
    } else if (value.name == "char") {
      statement("output.character(" + expression + ")");
    } else if (value.name == "float") {
      statement(choose("output.floating(" + expression + ", true)",
                       "output.float32(" + expression + ")", "output.float32(" + expression + ")"));
    } else if (value.name == "double") {
      statement("output.floating(" + expression + ", false)");
    } else {
      const auto converted = language == target::go       ? "uint64(" + expression + ")"
                             : language == target::csharp ? "unchecked((ulong)" + expression + ")"
                                                          : expression;
      statement("output.integer(" + converted + ", " + std::to_string(width(value.name)) + ", " +
                (value.name.starts_with("u") ? "true" : "false") + ")");
    }
  }
  // Declare a local using native inference.
  void local(const std::string& identifier, const std::string& value) {
    statement(choose("let " + identifier + " = " + value, identifier + " := " + value,
                     "var " + identifier + " = " + value));
  }
  // Open an incremental collection read loop.
  void read_loop() {
    open(choose("for (let index = 0; input.nextElement(index, count); ++index)",
                "for index := 0; input.nextElement(index, count); index++",
                "for (int index = 0; input.nextElement(index, count); ++index)"));
  }
  // Map schema collections to native typed storage.
  std::string collection_type(const member& value) const {
    const auto item = type(value.type_name_list.front());
    if (value.modifier == member::modifier_type::array) {
      return choose(item + "[]", "[]" + item, "System.Collections.Generic.List<" + item + ">");
    }
    const auto key = type(key_type(value));
    return choose("Map<" + key + ", " + item + ">", "map[" + key + "]" + item,
                  "System.Collections.Generic.Dictionary<" + key + ", " + item + ">");
  }
  // Allocate empty collections without trusting incoming sizes.
  std::string empty_collection(const member& value) const {
    if (language == target::js || language == target::typescript) {
      return value.modifier == member::modifier_type::array ? "[]" : "new Map()";
    }
    return language == target::go ? "make(" + collection_type(value) + ", 0)"
                                  : "new " + collection_type(value) + "()";
  }
  // Construct independent fixed elements while leaving variable collections empty.
  std::string initial_collection(const member& value) const {
    if (value.initializer) { return default_value(value); }
    if (value.fixed_extent != 0) {
      const auto count = std::to_string(value.fixed_extent);
      const auto element = initial(value.type_name_list.front());
      if (language == target::go) {
        return "func() " + collection_type(value) + " { values := make(" + collection_type(value) +
               ", " + count + "); for index := range values { values[index] = " + element +
               " }; return values }()";
      }
      if (language == target::csharp) {
        return "new " + collection_type(value) +
               "(System.Linq.Enumerable.Select<int, " + type(value.type_name_list.front()) +
               ">(System.Linq.Enumerable.Range(0, " + count +
               "), _ => " + element + "))";
      }
      return "Array.from({ length: " + count + " }, () => " + element + ")";
    }
    return empty_collection(value);
  }
  // Declare or initialize one field using target-specific visibility.
  void declaration(const std::string& identifier, const std::string& field_type,
                   const std::string& initial_value, access_type access) {
    if (language == target::go) {
      line(identifier + " " + field_type);
    } else if (language == target::typescript) {
      line(std::string{access == access_type::public_access ? "" : "private "} + identifier + ": " +
           field_type + ";");
    } else if (language == target::js) {
      statement("this." + identifier + " = " + initial_value);
    } else {
      statement(std::string{access == access_type::public_access ? "public " : "private "} +
                field_type + " " + identifier + " = " + initial_value);
    }
  }
  // Owning JS variants retain only the selected payload and its schema-defined name.
  bool tagged_variant(const member& value) const {
    return value.owning_variant &&
           (language == target::js || language == target::typescript);
  }
  // Emit parent composition, fields, and explicit union selectors.
  void declarations(const class_node& value, bool go_defaults = false) {
    if (value.reference && value.reference->value_projection) { return; }
    const auto declare = [&](const std::string& identifier, const std::string& field_type,
                             const std::string& initial_value, access_type access) {
      if (go_defaults) {
        line(identifier + ": " + initial_value + ",");
      } else {
        declaration(identifier, field_type, initial_value, access);
      }
    };
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      const auto& base = value.parents[i];
      const auto base_type = names.at(base.parent_class);
      declare(field(std::string(language == target::go ? "Base" : "base") + std::to_string(i), base.access),
              (language == target::go ? "*" : "") + base_type,
              choose("new ", "New", "new ") + base_type + "()", base.access);
    }
    for (const auto& item : value.member_list) {
      if (item.wire_only) { continue; }
      const auto identifier = field(item.name, item.access);
      if (item.modifier == member::modifier_type::variant && tagged_variant(item)) {
        std::string alternatives{};
        for (const auto& alternative : item.type_name_list) {
          if (!alternatives.empty()) {
            alternatives += " | ";
          }
          alternatives += "{ kind: " + quote(alternative.enum_name) + "; value: " +
                          type(alternative) + " }";
        }
        const auto& first = item.type_name_list.front();
        declare(identifier, alternatives,
                "{ kind: " + quote(first.enum_name) + ", value: " + initial(first) + " }",
                item.access);
      } else if (item.modifier == member::modifier_type::variant) {
        declare(identifier + "Index", choose("number", "int", "int"), "0", item.access);
        for (const auto& alternative : item.type_name_list) {
          declare(identifier + name(alternative.enum_name, true, true), type(alternative),
                  initial(alternative), item.access);
        }
      } else if (item.modifier == member::modifier_type::none) {
        declare(identifier, type(item.type_name_list.front()), default_value(item), item.access);
      } else {
        declare(identifier, collection_type(item), initial_collection(item), item.access);
      }
    }
  }
  // Decode collections by replacement, merging only ordinary nested object fields.
  void read_field(const member& value) {
    const auto destination = "result." + field(value.name, value.access);
    const auto& item = value.type_name_list.front();
    if (value.modifier == member::modifier_type::none) {
      if (value.compact != compact_encoding::none) {
        const auto expression = "input." + std::string{value.compact == compact_encoding::prefix
                                                         ? "compactPrefix("
                                                         : "compactVarint("} +
                                std::to_string(width(item.name)) + ")";
        const auto converted = language == target::go       ? type(item) + "(" + expression + ")"
                               : language == target::csharp ? "(" + type(item) + ")" + expression
                                                            : expression;
        statement(destination + " = " + converted);
      } else {
        statement(destination + " = " + read_value(item, false, destination));
      }
      return;
    }
    local("count", "input.beginArray()");
    if (value.fixed_extent != 0) {
      condition(choose("count !== -1 && count !== ", "count != -1 && count != ", "count != -1 && count != ") + std::to_string(value.fixed_extent));
      statement("input.fail(\"Fixed array extent mismatch\")");
      close();
    }
    std::string initial_collection = empty_collection(value);
    if (value.modifier == member::modifier_type::array && language != target::js) {
      const auto& scalar = value.type_name_list.front();
      int scalar_width{};
      if (scalar.is_optional()) {
        scalar_width = 1;
      } else if (scalar.type == object_type::primitive && scalar.name != "string" && !scalar.is_digest() &&
                 !scalar.is_sequence() && !scalar.is_associative()) {
        scalar_width = scalar.name == "bool" || scalar.name == "char" ? 1
                       : scalar.name == "float"                       ? 4
                       : scalar.name == "double"                      ? 8
                                                                      : width(scalar.name);
      }
      const auto capacity = "input.arrayCapacity(count, " + std::to_string(scalar_width) + ")";
      initial_collection = language == target::go
                               ? "make(" + collection_type(value) + ", 0, " + capacity + ")"
                               : "new " + collection_type(value) + "(" + capacity + ")";
    }
    local("values", initial_collection);
    read_loop();
    if (value.fixed_extent != 0) {
      condition("index >= " + std::to_string(value.fixed_extent));
      statement("input.fail(\"Fixed array extent mismatch\")");
      close();
    }
    if (value.modifier == member::modifier_type::array) {
      const auto expression = read_value(item, true);
      statement(choose("values.push(" + expression + ")",
                       "values = append(values, " + expression + ")",
                       "values.Add(" + expression + ")"));
    } else {
      const auto key = key_type(value);
      statement(
          choose("let entryKey", "var entryKey " + type(key), type(key) + " entryKey = default!"));
      statement(choose("let entryValue", "var entryValue " + type(item),
                       type(item) + " entryValue = default!"));
      condition("input.json()");
      local("hasKey", "false");
      local("hasValue", "false");
      statement("input.beginObject()");
      local("firstEntry", "true");
      open(choose("while (input.nextObject(firstEntry))", "for input.nextObject(firstEntry)",
                  "while (input.nextObject(firstEntry))"));
      statement("firstEntry = false");
      open(choose("switch (input.key())", "switch input.key()", "switch (input.key())"));
      line("case \"key\":");
      ++level;
      statement("entryKey = " + read_value(key, true));
      statement("hasKey = true");
      if (language != target::go) {
        statement("break");
      }
      --level;
      line("case \"value\":");
      ++level;
      statement("entryValue = " + read_value(item, true));
      statement("hasValue = true");
      if (language != target::go) {
        statement("break");
      }
      --level;
      line("default:");
      ++level;
      statement("input.fail(\"Unknown map entry field\")");
      if (language != target::go) {
        statement("break");
      }
      --level;
      close();
      close();
      statement("input.endObject()");
      condition("!hasKey || !hasValue");
      statement("input.fail(\"Incomplete map entry\")");
      close();
      --level;
      line("} else {");
      ++level;
      statement("entryKey = " + read_value(key, true));
      statement("entryValue = " + read_value(item, true));
      close();
      if (language == target::go) {
        condition("input.err == nil");
      }
      statement(choose("values.set(entryKey, entryValue)", "values[entryKey] = entryValue",
                       "values[entryKey] = entryValue"));
      if (language == target::go) {
        close();
      }
    }
    close();
    if (value.fixed_extent != 0) {
      condition(choose("values.length !== ", "len(values) != ", "values.Count != ") + std::to_string(value.fixed_extent));
      statement("input.fail(\"Fixed array extent mismatch\")");
      close();
    }
    statement(destination + " = values");
  }
  // Encode arrays and maps directly, sorting keys to preserve native map ordering.
  void write_field(const member& value, const std::string& selected_expression = {}) {
    const auto expression = selected_expression.empty() ?
        choose("this.", "value.", "this.") + field(value.name, value.access) : selected_expression;
    const auto& item = value.type_name_list.front();
    if (!value.auto_size_formats.empty()) {
      condition(omitted_protocols(value.auto_size_formats, "output"));
      statement("srlSizeOffset = output.position()");
      statement("srlSizeWidth = " + std::to_string(width(item.name)));
      write_value(item, choose(item.name == "uint64" ? "0n" : "0", "0", "0"), false);
      if (value.size_scope == auto_size_scope::payload) {
        statement("srlSizeExcludedBytes = output.position() - srlSizeFieldStart");
      }
      if (!value.wire_only) {
        --level;
        line("} else {");
        ++level;
        write_value(item, expression, false);
      }
      close();
      return;
    }
    if (value.modifier == member::modifier_type::none) {
      if (value.compact != compact_encoding::none) {
        const auto converted = language == target::go       ? "uint64(" + expression + ")"
                               : language == target::csharp ? "(ulong)" + expression
                                                            : expression;
        const auto method = value.compact == compact_encoding::prefix ? "compactPrefix"
                                                                     : "compactVarint";
        statement("output." + std::string{method} + (value.compact_strict ? "(" : "Lenient(") +
                  converted + ", " + std::to_string(width(item.name)) + ")");
      } else {
        write_value(item, expression, false);
      }
      return;
    }
    if (value.modifier == member::modifier_type::array) {
      const auto count =
          choose(expression + ".length", "len(" + expression + ")", expression + ".Count");
      if (value.fixed_extent != 0) {
        condition(count + choose(" !== ", " != ", " != ") + std::to_string(value.fixed_extent));
        statement("output.fail(\"Fixed array extent mismatch\")");
        close();
      }
      statement("output.beginArray(" + count + ")");
      open(choose("for (let index = 0; index < " + count + "; ++index)",
                  "for index := 0; index < " + count + "; index++",
                  "for (int index = 0; index < " + count + "; ++index)"));
      statement("output.element(index)");
      write_value(item, expression + "[index]", true);
      close();
    } else {
      const auto key = key_type(value);
      if (language == target::js) {
        local("keys", "Array.from(" + expression + ".keys()).sort(srlCompare)");
      } else if (language == target::go) {
        local("keys", "make([]" + type(key) + ", 0, len(" + expression + "))");
        open("for key := range " + expression);
        statement("keys = append(keys, key)");
        close();
        const auto comparison = key.name == "bool" ? "!keys[i] && keys[j]" : "keys[i] < keys[j]";
        line("sort.Slice(keys, func(i, j int) bool { return " + std::string{comparison} + " })");
      } else {
        local("keys",
              "new System.Collections.Generic.List<" + type(key) + ">(" + expression + ".Keys)");
        statement("keys.Sort(SrlCompare)");
      }
      const auto count = choose("keys.length", "len(keys)", "keys.Count");
      statement("output.beginArray(" + count + ")");
      open(choose("for (let index = 0; index < " + count + "; ++index)",
                  "for index := 0; index < " + count + "; index++",
                  "for (int index = 0; index < " + count + "; ++index)"));
      local("entryKey", "keys[index]");
      statement("output.element(index)");
      condition("output.json()");
      statement("output.raw(\"{\\\"key\\\":\")");
      close();
      write_value(key, "entryKey", true);
      condition("output.json()");
      statement("output.raw(\",\\\"value\\\":\")");
      close();
      write_value(item,
                  choose(expression + ".get(entryKey)", expression + "[entryKey]",
                         expression + "[entryKey]"),
                  true);
      condition("output.json()");
      statement("output.raw(\"}\")");
      close();
      close();
    }
    statement("output.endArray()");
  }
  // Decode a fresh selected union payload, never merge an earlier selection.
  void read_union(const member& value, int selected = -1) {
    const auto destination = "result." + field(value.name, value.access);
    if (selected >= 0) {
      const auto& item = value.type_name_list.at(static_cast<std::size_t>(selected));
      if (tagged_variant(value)) {
        statement(destination + " = { kind: " + quote(item.enum_name) +
                  ", value: " + read_value(item, true) + " }");
      } else {
        statement(destination + "Index = " + std::to_string(selected));
        statement(destination + name(item.enum_name, true, true) + " = " + read_value(item, true));
      }
      return;
    }
    open(choose("switch (input.compact())", "switch input.compact()", "switch (input.compact())"));
    for (std::size_t i = 0; i < value.type_name_list.size(); ++i) {
      line("case " + std::to_string(i) + ":");
      ++level;
      read_union(value, static_cast<int>(i));
      if (language != target::go) {
        statement("break");
      }
      --level;
    }
    line("default:");
    ++level;
    statement("input.fail(\"Unknown union alternative\")");
    if (language != target::go) {
      statement("break");
    }
    --level;
    close();
  }
  // Decode a complete field according to its schema modifier.
  void read_member(const member& value) {
    if (value.modifier == member::modifier_type::variant) {
      read_union(value);
    } else {
      read_field(value);
    }
  }
  // Select the target revision expression vocabulary.
  version_language revision_language() const {
    return language == target::go       ? version_language::go
           : language == target::csharp ? version_language::csharp
                                        : version_language::javascript;
  }
  // Validate the revision before exposing an encoded or decoded message.
  void check_version(const class_node& owner, bool reading) {
    const auto& version = *owner.version_member();
    const auto value = choose(reading ? "result." : "this.", reading ? "result." : "value.",
                              reading ? "result." : "this.") +
                       field(version.name, version.access);
    const auto lang = revision_language();
    auto invalid = version_less(lang, version, value, schema_version::minimum(version)) + " || !(" +
                   version_less(lang, version, value, version.default_value) + " || " +
                   version_equal(lang, version, value, version.default_value) + ")";
    if (reading) {
      invalid += " || (input.limits." + choose("readPolicy", "ReadPolicy", "ReadPolicy") +
                 " == " + choose("ReadPolicy.STRICT", "STRICT", "ReadPolicy.STRICT") + " && !" +
                 version_equal(lang, version, value, version.default_value) + ")";
    }
    condition(invalid);
    statement(std::string{reading ? "input" : "output"} + ".fail(\"Unsupported schema version\")");
    close();
  }
  // Record field presence and omit inactive positional fields without consuming bytes.
  void read_versioned_member(const class_node& owner, const member& item, int alternative = -1,
                             bool positional = false) {
    if (positional && item.omits("binary_none")) {
      return;
    }
    const auto omitted = omitted_protocols(item.omitted_formats, "input");
    if (!positional && !omitted.empty()) {
      condition(omitted);
      statement("input.fail(\"Field omitted from selected format\")");
      if (language == target::go) {
        statement("return result");
      }
      close();
    }
    if (!item.auto_size_formats.empty()) {
      condition(omitted_protocols(item.auto_size_formats, "input"));
      condition("srlSizeSeen");
      statement("input.fail(\"Duplicate derived record size\")");
      close();
      statement("srlSizeSeen = true");
      close();
    }
    const auto* version = owner.version_member();
    const bool payload_size = item.size_scope == auto_size_scope::payload &&
        auto_size::verifies(owner, options.class_auto_sizes);
    if (payload_size) {
      local("srlSizeFieldStart", positional ? "input.position()" : "srlKeyStart");
    }
    // Transport-only values are retained locally only when needed to verify their wire contract.
    const auto read_selected = [&]() {
      if (item.wire_only) {
        const auto decoded = read_value(item.type_name_list.front(), false);
        if (auto_size::verifies(owner, options.class_auto_sizes)) {
          statement("srlWireSize" + std::to_string(item.id) + " = " + decoded);
        } else {
          statement(choose(decoded, "_ = " + decoded, "_ = " + decoded));
        }
      } else if (alternative >= 0) {
        read_union(item, alternative);
      } else {
        read_member(item);
      }
      if (payload_size) {
        condition(omitted_protocols(item.auto_size_formats, "input"));
        statement("srlSizeExcludedBytes = input.position() - srlSizeFieldStart");
        close();
      }
    };
    if (!version) {
      read_selected();
      return;
    }
    const auto slot = std::to_string(version_slot(owner, item));
    condition("seen[" + slot + "]");
    statement("input.fail(\"Duplicate field\")");
    close();
    statement("seen[" + slot + "] = true");
    const auto active = version_active(revision_language(), item, *version,
                                       "result." + field(version->name, version->access));
    if (!active.empty()) {
      condition("input.protocol != " + protocol("BINARY_NONE") + " || (" + active + ")");
    }
    read_selected();
    if (!active.empty()) {
      close();
    }
    if (item.version) {
      check_version(owner, true);
    }
  }
  // Reference static metadata in the target's class or package scope.
  std::string magic_constant(const class_node& value) const {
    const auto identifier = names.at(&value);
    auto go_name = identifier + "Magic";
    if (value.magic_access != access_type::public_access) {
      go_name.front() = static_cast<char>(
          std::tolower(static_cast<unsigned char>(go_name.front())));
    }
    return choose(identifier + ".#srlMagic", go_name, identifier + ".Magic");
  }
  // Write typed metadata through a runtime value where Go constant conversions would overflow.
  void write_magic_constant(const class_node& value) {
    const auto expression = !projection_fields_.empty() && language == target::js ? default_value(*value.magic_field) : magic_constant(value);
    if (language == target::go) {
      // Signed negative constants cannot be converted directly to uint64 in Go.
      local("magicValue", expression);
      write_value(value.magic_field->type_name_list.front(), "magicValue", false);
    } else {
      write_value(value.magic_field->type_name_list.front(), expression, false);
    }
  }
  // Emit scalar encoding or legacy raw identity before native binary object framing.
  void magic_prefix(const class_node& value, bool reading) {
    if (!value.has_magic()) {
      return;
    }
    const auto stream = reading ? "input" : "output";
    auto active = std::string(stream) + ".protocol != " + protocol("JSON");
    const auto omitted = omitted_protocols(value.magic_omitted_formats, stream);
    if (!omitted.empty()) {
      active += " && !(" + omitted + ")";
    }
    condition(active);
    if (value.magic_field) {
      const auto& magic_type = value.magic_field->type_name_list.front();
      if (reading) {
        condition(read_value(magic_type, false) + " != " + magic_constant(value));
        statement("input.fail(\"Incorrect schema magic\")");
        if (language == target::go) {
          statement("return result");
        }
        close();
      } else {
        write_magic_constant(value);
      }
    } else {
      for (const auto byte : value.magic_bytes) {
        const auto literal = std::to_string(static_cast<unsigned char>(byte));
        if (reading) {
          condition("input.integer(1, true) != " + literal);
          statement("input.fail(\"Incorrect schema magic\")");
          if (language == target::go) {
            statement("return result");
          }
          close();
        } else {
          statement("output.integer(" + literal + ", 1, true)");
        }
      }
    }
    close();
  }
  // Select configured native omissions; other backends have separate codec dispatch.
  std::string omitted_protocols(const std::vector<std::string>& formats,
                                std::string_view stream) const {
    std::string expression;
    for (const auto& format : formats) {
      if (format != "json" && format != "binary_none" && format != "binary_integer" &&
          format != "binary_string") {
        continue;
      }
      auto constant = format;
      std::transform(constant.begin(), constant.end(), constant.begin(), [](unsigned char byte) {
        return static_cast<char>(std::toupper(byte));
      });
      expression += (expression.empty() ? "" : " || ") + std::string(stream) +
                    ".protocol == " + protocol(constant);
    }
    return expression;
  }
  // Form validated UTF-8 schema text without depending on source-file escaping.
  std::string magic_text(const class_node& value) const {
    std::string bytes;
    std::string escaped{"\""};
    constexpr std::string_view hex{"0123456789abcdef"};
    for (const auto byte : value.magic_bytes) {
      const auto number = static_cast<unsigned char>(byte);
      bytes += (bytes.empty() ? "" : ", ") + std::to_string(number);
      escaped += "\\x";
      escaped += hex[number >> 4];
      escaped += hex[number & 0x0f];
    }
    escaped += '"';
    return choose("new TextDecoder('utf-8', {fatal: true}).decode(Uint8Array.from([" + bytes +
                      "]))",
                  escaped,
                  "System.Text.Encoding.UTF8.GetString(new byte[] { " + bytes + " })");
  }
  // Write every parent and member in declaration order.
  void write_object(const class_node& value) {
    if (value.reference && value.reference->value_projection) {
      const auto& selected = value.member_list.front();
      if (language == target::go) {
        line("srlSelected, srlError := value.ReadValue()");
        line("if srlError != nil { output.err = srlError; return }");
      } else {
        local("srlSelected", language == target::csharp ? "ReadValue()" : "this.readValue()");
      }
      write_field(selected, "srlSelected");
      return;
    }
    if (value.reference) {
      if (language == target::go) {
        line("if err := value.srlValidateReferenceAddress(); err != nil { output.err = err; return }");
      } else {
        statement(choose("this.srlValidateReferenceAddress()", "", "SrlValidateReferenceAddress()"));
      }
    }
    const bool derived_size = !auto_size::fields(value).empty();
    const bool payload_size = std::any_of(value.member_list.begin(), value.member_list.end(),
        [](const auto& item) { return item.size_scope == auto_size_scope::payload; });
    if (derived_size) {
      local("srlRecordStart", "output.position()");
      local("srlSizeOffset", "-1");
      local("srlSizeWidth", "0");
      if (payload_size) {
        local("srlSizeFieldStart", "0");
        local("srlSizeExcludedBytes", "0");
      }
    }
    if (value.version_member()) {
      check_version(value, false);
    }
    magic_prefix(value, false);
    statement("output.beginObject()");
    if (language == target::go) {
      condition("output.err != nil");
      statement("return");
      close();
    }
    bool first = true;
    const bool variable_first = std::any_of(value.member_list.begin(), value.member_list.end(),
                                           [](const auto& item) { return item.omits("json"); });
    if (variable_first) {
      local("firstField", "true");
    }
    const auto first_flag = [&](bool initial) {
      return variable_first ? std::string{"firstField"} : std::string{initial ? "true" : "false"};
    };
    const bool magic_json = value.has_magic() && !value.magic_omits("json");
    if (magic_json) {
      condition("output.json()");
      statement("output.field(" + std::to_string(value.magic_id) + ", " + wire_key("magic") +
                ", true)");
      if (value.magic_field) {
        write_magic_constant(value);
      } else {
        statement("output.text(" + magic_text(value) + ")");
      }
      if (variable_first) {
        statement("firstField = false");
      }
      close();
      first = false;
    }
    if (const auto* version = value.version_member()) {
      statement("output.field(" + std::to_string(version->id) + ", " +
                wire_key(version->display_name) + ", " + first_flag(first) + ")");
      write_field(*version);
      if (variable_first) {
        statement("firstField = false");
      }
      first = false;
    }
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      const auto& base = value.parents[i];
      statement("output.field(" + std::to_string(base.id) + ", " + wire_key(base.display_name) +
                ", " + first_flag(first) + ")");
      statement(choose("this.", "value.", "this.") +
                field(std::string(language == target::go ? "Base" : "base") + std::to_string(i), base.access) +
                choose(".write(output)", ".write(output)", ".Write(output)"));
      if (variable_first) {
        statement("firstField = false");
      }
      first = false;
    }
    for (const auto& item : wire_members(value)) {
      if (item.version) {
        continue;
      }
      const auto* version = value.version_member();
      const auto active = version ? version_active(revision_language(), item, *version,
                                                   choose("this.", "value.", "this.") +
                                                       field(version->name, version->access))
                                  : std::string{};
      if (!active.empty()) {
        condition(active);
      }
      const auto omitted = omitted_protocols(item.omitted_formats, "output");
      if (!omitted.empty()) {
        condition("!(" + omitted + ")");
      }
      open("");
      if (item.modifier == member::modifier_type::variant) {
        const auto expression = choose("this.", "value.", "this.") + field(item.name, item.access);
        const auto tagged = tagged_variant(item);
        open(tagged ? "switch (" + expression + ".kind)"
                    : choose("switch (" + expression + "Index)", "switch " + expression + "Index",
                             "switch (" + expression + "Index)"));
        for (std::size_t i = 0; i < item.type_name_list.size(); ++i) {
          const auto& alternative = item.type_name_list[i];
          line("case " + (tagged ? quote(alternative.enum_name) : std::to_string(i)) + ":");
          ++level;
          statement("output.field(" + std::to_string(item.id) + ", " +
                    wire_key(item.display_name + ":" + alternative.enum_name) + ", " +
                    first_flag(first) + ")");
          condition("output.protocol == " + protocol("BINARY_NONE") +
                    " || output.protocol == " + protocol("BINARY_INTEGER"));
          statement("output.compact(" + std::to_string(i) + ")");
          close();
          write_value(alternative,
                      expression + (tagged ? ".value" : name(alternative.enum_name, true, true)),
                      true);
          if (language != target::go) {
            statement("break");
          }
          --level;
        }
        line("default:");
        ++level;
        statement("output.fail(\"Unknown union alternative\")");
        if (language != target::go) {
          statement("break");
        }
        --level;
        close();
      } else {
        if (item.size_scope == auto_size_scope::payload) {
          statement("srlSizeFieldStart = output.position()");
        }
        statement("output.field(" + std::to_string(item.id) + ", " + wire_key(item.display_name) +
                  ", " + first_flag(first) + ")");
        write_field(item);
      }
      close();
      if (variable_first) {
        statement("firstField = false");
      }
      if (!omitted.empty()) {
        close();
      }
      if (!active.empty()) {
        close();
      }
      first = false;
    }
    statement("output.endObject()");
    if (derived_size) {
      condition("srlSizeOffset >= 0");
      const auto span = std::string{"output.position() - srlRecordStart"} +
          (payload_size ? " - srlSizeExcludedBytes" : "");
      const auto bytes = choose(span, "uint64(" + span + ")", "(ulong)(" + span + ")");
      statement("output.patchInteger(srlSizeOffset, " + bytes + ", srlSizeWidth)");
      close();
    }
  }
  // Merge a composed parent's fields in input order.
  void read_parent(const parent& base, std::size_t index) {
    const auto destination = "result." + field(std::string(language == target::go ? "Base" : "base") + std::to_string(index), base.access);
    const auto identifier = names.at(base.parent_class);
    statement(destination + " = " +
              choose(identifier + ".read", "read" + identifier, identifier + ".Read") + "(input, " +
              destination + ")");
  }
  // Emit a field dispatch case with a scope for collection temporaries.
  void read_case(const class_node& owner, const std::string& key, const member& value,
                 int alternative = -1) {
    line("case " + key + ":");
    open("");
    read_versioned_member(owner, value, alternative);
    if (language != target::go) {
      statement("break");
    }
    close();
  }
  // Dispatch known keys through native switches, avoiding linear field lookup.
  void read_object(const class_node& value) {
    if (value.reference && value.reference->value_projection) {
      statement(choose("throw new Error('Export references cannot be decoded; use the ordinary owning type')",
          "input.fail(\"Export references cannot be decoded; use the ordinary owning type\")",
          "throw new System.InvalidOperationException(\"Export references cannot be decoded; use the ordinary owning type\")"));
      if (language == target::go) { statement("return result"); }
      return;
    }
    if (value.reference) { statement(choose("result.unbind()", "result.Unbind()", "result.Unbind()")); }
    const auto derived_fields = auto_size::fields(value);
    const bool verify_size = auto_size::verifies(value, options.class_auto_sizes);
    const bool payload_size = verify_size && std::any_of(derived_fields.begin(), derived_fields.end(),
        [](const auto* item) { return item->size_scope == auto_size_scope::payload; });
    if (!derived_fields.empty()) {
      local("srlSizeSeen", "false");
      if (verify_size) { local("srlRecordStart", "input.position()"); }
      if (payload_size) {
        local("srlSizeExcludedBytes", "0");
        if (options.protocols == output_protocols::all) { local("srlKeyStart", "0"); }
      }
      if (verify_size) {
        for (const auto* item : derived_fields) {
          if (item->wire_only) {
            const auto& scalar = item->type_name_list.front();
            local("srlWireSize" + std::to_string(item->id), choose(
                scalar.name == "uint64" ? "0n" : "0", type(scalar) + "(0)",
                scalar.name == "uint64" ? "0UL" : "0U"));
          }
        }
      }
    }
    magic_prefix(value, true);
    const bool magic_json = value.has_magic() && !value.magic_omits("json");
    if (magic_json) {
      local("magicSeen", "false");
    }
    if (value.version_member()) {
      local(
          "seen",
          choose("new Array(" + std::to_string(value.parents.size() + value.member_list.size()) +
                     ").fill(false)",
                 "[" + std::to_string(value.parents.size() + value.member_list.size()) + "]bool{}",
                 "new bool[" + std::to_string(value.parents.size() + value.member_list.size()) +
                     "]"));
    }
    statement("input.beginObject()");
    if (language == target::go) {
      condition("input.err != nil");
      statement("return result");
      close();
    }
    condition("input.protocol == " + protocol("BINARY_NONE"));
    if (const auto* version = value.version_member()) {
      read_versioned_member(value, *version, -1, true);
    }
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      read_parent(value.parents[i], i);
    }
    for (const auto& item : wire_members(value)) {
      if (item.version) {
        continue;
      }
      open("");
      read_versioned_member(value, item, -1, true);
      close();
    }
    const auto keyed_protocols = options.protocols == output_protocols::all;
    for (const bool numeric : {true, false}) {
      if (!keyed_protocols) { break; }
      --level;
      line(numeric ? "} else if (input.protocol == " + protocol("BINARY_INTEGER") + ") {"
                   : "} else {");
      // Go does not permit parentheses around a condition followed by an implicit semicolon;
      // the parenthesized expression here remains legal before the opening brace.
      ++level;
      if (!numeric) {
        local("first", "true");
      }
      open(choose("while (true)", "for input.err == nil", "while (true)"));
      if (payload_size) { statement("srlKeyStart = input.position()"); }
      if (numeric) {
        local("key", "input.compact()");
        condition("key == 0");
        statement("break");
        close();
      } else {
        condition("input.json()");
        condition("!input.nextObject(first)");
        statement("break");
        close();
        statement("first = false");
        close();
        local("key", "input.key()");
        condition("!input.json() && key == \"\"");
        statement("break");
        close();
      }
      if (language == target::go) {
        condition("input.err != nil");
        statement("break");
        close();
      }
      open(choose("switch (key)", "switch key", "switch (key)"));
      if (!numeric && magic_json) {
        line("case \"magic\":");
        open("");
        const auto comparison = value.magic_field
            ? read_value(value.magic_field->type_name_list.front(), false) + " != " +
                  magic_constant(value)
            : "input.text() != " + magic_text(value);
        condition("!input.json() || magicSeen || " + comparison);
        statement("input.fail(\"Incorrect or duplicate schema magic\")");
        close();
        statement("magicSeen = true");
        if (language != target::go) {
          statement("break");
        }
        close();
      }
      if (!numeric && value.has_magic() && !magic_json) {
        line("case \"magic\":");
        open("");
        statement("input.fail(\"Field omitted from selected format\")");
        statement(language == target::go ? "return result" : "break");
        close();
      }
      for (std::size_t i = 0; i < value.parents.size(); ++i) {
        const auto& base = value.parents[i];
        line("case " + (numeric ? std::to_string(base.id) : quote(base.display_name)) + ":");
        open("");
        read_parent(base, i);
        if (language != target::go) {
          statement("break");
        }
        close();
      }
      for (const auto& item : wire_members(value)) {
        if (!numeric && item.modifier == member::modifier_type::variant) {
          for (std::size_t i = 0; i < item.type_name_list.size(); ++i) {
            read_case(value, quote(item.display_name + ":" + item.type_name_list[i].enum_name),
                      item, static_cast<int>(i));
          }
        } else {
          read_case(value, numeric ? std::to_string(item.id) : quote(item.display_name), item);
        }
      }
      line("default:");
      ++level;
      statement("input.skipValue()");
      if (language != target::go) {
        statement("break");
      }
      --level;
      close();
      close();
    }
    close();
    statement("input.endObject()");
    if (magic_json) {
      condition("input.json() && !magicSeen");
      statement("input.fail(\"Missing schema magic\")");
      close();
    }
    if (const auto* version = value.version_member()) {
      condition("!seen[" + std::to_string(version_slot(value, *version)) + "]");
      statement("input.fail(\"Missing schema version\")");
      close();
      check_version(value, true);
      for (const auto& item : wire_members(value)) {
        const auto active = version_active(revision_language(), item, *version,
                                           "result." + field(version->name, version->access));
        if (!active.empty()) {
          condition("input.protocol != " + protocol("BINARY_NONE") + " && seen[" +
                    std::to_string(version_slot(value, item)) + "] && !(" + active + ")");
          statement("input.fail(\"Field outside version lifecycle\")");
          close();
        }
      }
    }
    for (const auto* size_field : derived_fields) {
      condition(omitted_protocols(size_field->auto_size_formats, "input"));
      condition("!srlSizeSeen");
      statement("input.fail(\"Missing derived record size\")");
      close();
      if (verify_size) {
        const auto stored = size_field->wire_only ? "srlWireSize" + std::to_string(size_field->id)
            : "result." + field(size_field->name, size_field->access);
        const auto span = std::string{"input.position() - srlRecordStart"} +
            (payload_size ? " - srlSizeExcludedBytes" : "");
        const auto actual = choose(size_field->type_name_list.front().name == "uint64" ?
            "BigInt(" + span + ")" : span, "uint64(" + span + ")", "(ulong)(" + span + ")");
        condition(choose(stored, "uint64(" + stored + ")", "(ulong)" + stored) + " != " + actual);
        statement("input.fail(\"Derived record size mismatch\")");
        close();
      }
      close();
    }
    if (value.reference) {
      if (language == target::go) {
        line("if input.err == nil { input.err = result.srlValidateReferenceAddress() }");
      } else {
        statement(choose("result.srlValidateReferenceAddress()", "", "result.SrlValidateReferenceAddress()"));
      }
    }
    statement("return result");
  }
  // Emit enum constants and the original-name lookup required by native codecs.
  void enumeration(const enum_node& value) {
    const auto identifier = names.at(&value);
    line("// " + identifier + " retains schema ordinals independently of generated names.");
    if (language == target::typescript) {
      line("export declare const " + identifier + ": Readonly<{");
      ++level;
      for (std::size_t i = 0; i < value.enum_name_list.size(); ++i) {
        line(name(value.enum_name_list[i], true, options.rename_identifiers) + ": " +
             std::to_string(i) + ";");
      }
      --level;
      line("}>;");
      line("export type " + identifier + " = typeof " + identifier + "[keyof typeof " + identifier +
           "];");
      return;
    }

    if (language == target::go) {
      line("type " + identifier + " int32");
      line("const (");
      ++level;
    } else if (language == target::js) {
      open("export const " + identifier + " = Object.freeze(");
    } else {
      open((language == target::typescript ? "export " : "public ") + std::string{"enum "} +
           identifier);
    }
    for (std::size_t i = 0; i < value.enum_name_list.size(); ++i) {
      const auto constant = name(value.enum_name_list[i], true, options.rename_identifiers);
      line(language == target::go
               ? identifier + constant + " " + identifier + " = " + std::to_string(i)
               : constant + (language == target::js ? ": " : " = ") + std::to_string(i) + ",");
    }
    --level;
    line(language == target::typescript ? "}" : choose("});", ")", "}"));
    if (language == target::typescript) {
      return;
    }
    std::string values{};
    for (const auto& item : value.enum_name_list) {
      if (!values.empty()) {
        values += ", ";
      }
      values += quote(item);
    }
    line(choose("const " + identifier + "Names = [" + values + "];",
                "var " + identifier + "Names = []string{" + values + "}",
                "private static readonly string[] " + identifier + "Names = {" + values + "};"));
  }
  // Emit schema behavior without entering the codec field traversal.
  void behaviors(const class_node& value) {
    const auto selected = language == target::go ? "go" : language == target::csharp ? "csharp" :
        language == target::typescript ? "typescript" : "js";
    behavior::require_native_names(value, selected, options.rename_identifiers);
    const auto model_name = names.at(&value);
    const auto contracts = behavior::contracts(value);
    behavior::require_unique_callable_names(value, selected,
        [&](auto n) { return field(n, access_type::public_access); },
        language == target::csharp ? std::function<std::string(const type_name&)>{[&](const auto& t) { return type(t); }} :
                                    std::function<std::string(const type_name&)>{});
    std::vector<const function_declaration*> external;
    if (language == target::js || language == target::typescript) {
      for (const auto& method : value.functions) {
        if (!method.expression && !behavior::native_body(method, "js")) { external.push_back(&method); }
      }
      for (const auto* method : contracts) {
        if (std::find(external.begin(), external.end(), method) == external.end()) { external.push_back(method); }
      }
      if (!external.empty() && language == target::js) {
        line("#srlBehavior = null;");
        open("attachBehavior(behavior)");
        for (const auto* method : external) {
          const auto method_name = field(method->name, access_type::public_access);
          condition("behavior == null || typeof behavior[" + quote(method_name) + "] !== 'function'");
          statement("throw new TypeError('Missing required behavior callable')"); close();
        }
        std::string bindings;
        for (const auto* method : external) {
          const auto key = quote(field(method->name, access_type::public_access));
          bindings += (bindings.empty() ? "" : ", ") + key + ": behavior[" + key + "].bind(behavior)";
        }
        statement("this.#srlBehavior = Object.freeze({" + bindings + "})"); close();
      } else if (!external.empty()) {
        std::string signature = "attachBehavior(behavior: { ";
        for (const auto* method : external) {
          signature += field(method->name, access_type::public_access) + ": (receiver: " + model_name;
          for (const auto& argument : method->parameters) {
            signature += ", " + name(argument.name, false, options.rename_identifiers) + ": " + type(argument.type);
          }
          signature += ") => " + (method->return_type.name == "void" ? "void" : type(method->return_type)) + "; ";
        }
        line(signature + "}): void;");
      }
    }
    if (language == target::csharp && !contracts.empty()) {
      open("public interface Behavior");
      for (const auto* method : contracts) {
        std::string parameters = model_name + " receiver";
        for (const auto& argument : method->parameters) {
          parameters += ", " + type(argument.type) + " " + name(argument.name, false, options.rename_identifiers);
        }
        line((method->return_type.name == "void" ? "void" : type(method->return_type)) + " " +
            field(method->name, access_type::public_access) + "(" + parameters + ");");
      }
      close();
      line("private Behavior? srlBehavior;");
      line("public void AttachBehavior(Behavior behavior) { srlBehavior = behavior ?? throw new System.ArgumentNullException(nameof(behavior)); }");
      open("public sealed class BehaviorAdapter");
      line("private readonly " + model_name + " receiver;"); line("private readonly Behavior implementation;");
      line("public BehaviorAdapter(" + model_name + " receiver, Behavior implementation) { this.receiver = receiver ?? throw new System.ArgumentNullException(nameof(receiver)); this.implementation = implementation ?? throw new System.ArgumentNullException(nameof(implementation)); }");
      for (const auto* method : contracts) {
        std::string parameters, calls = "receiver";
        for (const auto& argument : method->parameters) {
          parameters += (parameters.empty() ? "" : ", ") + type(argument.type) + " " + name(argument.name, false, options.rename_identifiers);
          calls += ", " + name(argument.name, false, options.rename_identifiers);
        }
        const auto result = method->return_type.name == "void" ? "void" : type(method->return_type);
        line("public " + result + " " + field(method->name, access_type::public_access) + "(" + parameters + ") { " +
            (result == "void" ? "" : "return ") + "implementation." + field(method->name, access_type::public_access) + "(" + calls + "); }");
      }
      close();
    }
    for (const auto& item : behavior::ordered_body(value)) {
      if (item.kind == class_body_item::kind_type::native_code) {
        const auto& code = value.native_blocks.at(item.index);
        if (code.language == selected || (language == target::js && code.language == "javascript")) {
          if (language == target::go) {
            throw std::invalid_argument{"go: class native blocks are not Go struct fields; use preamble/external files"};
          }
          source += behavior::source_marker(code, selected) + behavior::line_directive(code, selected) +
              code.text + behavior::reset_line(selected) + "\n";
        }
        continue;
      }
      if (item.kind != class_body_item::kind_type::function) { continue; }
      const auto& method = value.functions.at(item.index);
      line(behavior::source_marker(method, selected));
      if (language == target::go && method.dispatch == function_declaration::dispatch_type::abstract_method) { continue; }
      const auto identifier = field(method.name, method.access);
      const auto return_type = method.return_type.name == "void" ? choose("void", "", "void") : type(method.return_type);
      std::string arguments;
      for (const auto& parameter : method.parameters) {
        if (!arguments.empty()) { arguments += ", "; }
        const auto n = name(parameter.name, false, options.rename_identifiers);
        arguments += language == target::go ? n + " " + type(parameter.type) :
            language == target::csharp ? type(parameter.type) + " " + n :
            language == target::typescript ? n + ": " + type(parameter.type) : n;
      }
      if (language == target::typescript) {
        line(identifier + "(" + arguments + "): " + return_type + ";");
        continue;
      }
      const auto* native = behavior::native_body(method, selected);
      if (language == target::csharp && method.dispatch != function_declaration::dispatch_type::ordinary && !native && !method.expression) {
        open(std::string(method.access == access_type::public_access ? "public " : "private ") + return_type + " " + identifier + "(" + arguments + ")");
        condition("srlBehavior == null"); statement("throw new System.InvalidOperationException(\"Attach required behavior before invocation\")"); close();
        std::string calls = "this";
        for (const auto& argument : method.parameters) { calls += ", " + name(argument.name, false, options.rename_identifiers); }
        statement(std::string(return_type == "void" ? "" : "return ") + "srlBehavior." + field(method.name, access_type::public_access) + "(" + calls + ")");
        close(); continue;
      }
      if (!native && !method.expression) {
        if (language == target::go) {
          std::string signature = "var _ func(*" + model_name;
          for (const auto& parameter : method.parameters) { signature += ", " + type(parameter.type); }
          line(signature + ") " + return_type + " = (*" + model_name + ")." + identifier);
          continue;
        }
        if (language == target::csharp) {
          line(std::string(method.access == access_type::public_access ? "public " : "private ") +
              "partial " + return_type + " " + identifier + "(" + arguments + ");");
          continue;
        }
        open(identifier + "(" + arguments + ")");
        condition("this.#srlBehavior === null");
        statement("throw new Error('Attach the required behavior before invocation')"); close();
        std::string calls = "this";
        for (const auto& argument : method.parameters) {
          calls += ", " + name(argument.name, false, options.rename_identifiers);
        }
        statement("return this.#srlBehavior[" + quote(field(method.name, access_type::public_access)) + "](" + calls + ")");
        close(); continue;
      }
      std::string receiver_name = "value";
      if (language == target::go && method.expression) {
        while (std::any_of(method.parameters.begin(), method.parameters.end(), [&](const auto& argument) {
          return name(argument.name, false, options.rename_identifiers) == receiver_name;
        })) { receiver_name += "_"; }
      }
      open(language == target::go ? "func (" + receiver_name + " *" + model_name + ") " + identifier + "(" + arguments + ") " + return_type :
          language == target::csharp ? std::string(method.access == access_type::public_access ? "public " : "private ") +
              return_type + " " + identifier + "(" + arguments + ")" : identifier + "(" + arguments + ")");
      if (method.dispatch != function_declaration::dispatch_type::ordinary &&
          (language == target::js || language == target::csharp)) {
        condition(language == target::js ? "this.#srlBehavior !== null" : "srlBehavior != null");
        std::string calls = "this";
        for (const auto& argument : method.parameters) { calls += ", " + name(argument.name, false, options.rename_identifiers); }
        const auto callback = language == target::js ? "this.#srlBehavior[" + quote(field(method.name, access_type::public_access)) + "]" :
                                                    "srlBehavior." + field(method.name, access_type::public_access);
        statement(std::string(return_type == "void" ? "" : "return ") + callback + "(" + calls + ")");
        if (return_type == "void") { statement("return"); } close();
      }
      if (native) {
        source += behavior::source_marker(*native, selected) + behavior::line_directive(*native, selected) +
            native->text + behavior::reset_line(selected) + "\n";
      } else {
        const auto symbol = [&](std::string_view name_value) {
          const auto parameter = std::find_if(method.parameters.begin(), method.parameters.end(),
              [&](const auto& item) { return item.name == name_value; });
          if (parameter != method.parameters.end()) {
            return name(parameter->name, false, options.rename_identifiers);
          }
          const auto member = std::find_if(value.member_list.begin(), value.member_list.end(),
              [&](const auto& item) { return item.name == name_value; });
          return (language == target::go ? receiver_name + "." : std::string{"this."}) + field(member->name, member->access);
        };
        std::set<std::string> symbols;
        behavior::expression_symbols(*method.expression, symbols);
        const auto error = language == target::go ? "panic(\"Non-finite portable expression\")" :
            language == target::csharp ? "throw new System.ArithmeticException(\"Non-finite portable expression\")" :
                                        "throw new RangeError(\"Non-finite portable expression\")";
        for (const auto& input : symbols) {
          if (method.predicate_body) { continue; }
          const auto scalar_type = behavior::symbol_type(value, method, input);
          const auto field_input = std::find_if(value.member_list.begin(), value.member_list.end(), [&](const auto& item) { return item.name == input; });
          if ((scalar_type != "float" && scalar_type != "double" && scalar_type != "bool") ||
              (field_input != value.member_list.end() && field_input->modifier != member::modifier_type::none)) { continue; }
          if (behavior::symbol_type(value, method, input) == "bool") {
            if (language == target::js) {
              condition("typeof " + symbol(input) + " !== 'boolean'");
              statement("throw new RangeError('Portable expression requires boolean inputs')"); close();
            }
            continue;
          }
          const auto expression = language == target::go ? "float64(" + symbol(input) + ")" : symbol(input);
          condition((language == target::js ? "typeof " + expression + " !== 'number' || " : "") +
              "!" + (language == target::csharp ? "double.IsFinite(" + expression + ")" : behavior::finite(expression)));
          statement(error);
          close();
          const auto input_type = behavior::symbol_type(value, method, input);
          if (language == target::js && (input_type == "int32" || input_type == "uint32")) {
            condition("!Number.isInteger(" + expression + ") || " + expression +
                (input_type == "int32" ? " < -2147483648 || " : " < 0 || ") + expression +
                (input_type == "int32" ? " > 2147483647" : " > 4294967295"));
            statement("throw new RangeError('Portable integer conversion is outside the declared type')");
            close();
          }
        }
        behavior::expression_context context;
        context.member_name = [&](std::string_view item) { return field(item, access_type::public_access); };
        context.native_type = [&](const auto& item) { return type(item); };
        context.enum_value = [&](const auto& item, std::string_view name) { return enum_constant(item, name); };
        context.call = [&](const behavior_expression& node, const std::vector<std::string>& arguments) {
          if (node.call_constructor) { return behavior::constructor_expression(node, arguments, selected,
              [&](const auto& value) { return type(value); }, [&](auto name) { return field(name); }); }
          const auto& callee = behavior::called_function(node);
          std::string receiver = node.call_receiver ? "(" + arguments.at(0) + ")" :
              language == target::go ? receiver_name : "this";
          for (const auto base : node.call_base_path) { receiver += "." + field("base" + std::to_string(base)); }
          const auto callable = node.call_target->type == object_type::function_type ? names.at(node.call_target) : receiver + "." + field(callee.name, callee.access);
          return callable + "(" + behavior::call_arguments(arguments, node.call_receiver ? 1 : 0) + ")";
        };
        context.predicate = method.predicate_body;
        while (std::any_of(method.parameters.begin(), method.parameters.end(), [&](const auto& parameter) {
          return name(parameter.name, false, options.rename_identifiers) == context.budget_name;
        })) { context.budget_name += '_'; }
        if (behavior::expression_has_reduction(*method.expression)) {
          const auto budget = std::to_string(behavior_expression::maximum_reduction_visits);
          statement(choose("let " + context.budget_name + " = " + budget, context.budget_name + " := " + budget, "int " + context.budget_name + " = " + budget));
        }
        if (method.predicate_body && language == target::go) {
          open("return func() (srlPredicateResult bool)");
          line("defer func() { if recover() != nil { srlPredicateResult = false } }()");
        } else if (method.predicate_body) { open("try"); }
        statement(choose("const ", "", return_type + " ") + std::string{"srlBehaviorResult"} +
            choose(" = ", " := ", " = ") + behavior::expression(*method.expression, selected, symbol, arithmetic_helpers, context));
        if (method.return_type.name == "float" || method.return_type.name == "double") {
          condition("!" + (language == target::csharp ? std::string{"double.IsFinite(srlBehaviorResult)"} :
              behavior::finite(language == target::go ? "float64(srlBehaviorResult)" : "srlBehaviorResult")));
          statement(error); close();
        }
        statement("return srlBehaviorResult");
        if (method.predicate_body && language == target::go) { --level; line("}()"); }
        else if (method.predicate_body) {
          close(); open(language == target::csharp ? "catch (System.Exception srlFailure) when (srlFailure is System.ArithmeticException || srlFailure is System.NullReferenceException || srlFailure is System.ArgumentException || srlFailure is System.IndexOutOfRangeException)" : "catch (srlFailure)");
          statement("return false"); close();
        }
      }
      close();
    }
  }

  // Emit flattened namespace functions at module scope or as static C# container methods.
  void free_function(const function_node& node) {
    const auto& method = node.function;
    const auto selected = choose("js", "go", "csharp");
    const auto identifier = names.at(&node);
    const auto result_type = method.return_type.name == "void" ? choose("void", "", "void") : type(method.return_type);
    std::string parameters;
    for (const auto& parameter : method.parameters) {
      if (!parameters.empty()) { parameters += ", "; }
      const auto n = name(parameter.name, false, options.rename_identifiers);
      parameters += language == target::go ? n + " " + type(parameter.type) :
          language == target::csharp ? type(parameter.type) + " " + n :
          language == target::typescript ? n + ": " + type(parameter.type) : n;
    }
    line(behavior::source_marker(method, selected));
    if (language == target::typescript) {
      line("export declare function " + identifier + "(" + parameters + "): " + result_type + ";"); return;
    }
    const auto* native = behavior::native_body(method, selected);
    if (!native && !method.expression) {
      if (language == target::go) {
        std::string required;
        for (const auto& parameter : method.parameters) { required += (required.empty() ? "" : ", ") + type(parameter.type); }
        line("var _ func(" + required + ") " + result_type + " = " + identifier); return;
      }
      if (language == target::csharp) {
        line("public static partial " + result_type + " " + identifier + "(" + parameters + ");"); return;
      }
      throw std::invalid_argument{"js: standalone functions require an expression or selected native body"};
    }
    open(language == target::go ? "func " + identifier + "(" + parameters + ") " +
        (method.predicate_body ? "(srlPredicateResult bool)" : result_type) :
        language == target::csharp ? "public static " + result_type + " " + identifier + "(" + parameters + ")" :
        "export function " + identifier + "(" + parameters + ")");
    if (native) {
      if (options.rename_identifiers) { throw std::invalid_argument{selected + ": opaque native functions require naming = preserve"}; }
      source += behavior::source_marker(*native, selected) + behavior::line_directive(*native, selected) +
          native->text + behavior::reset_line(selected) + "\n";
    } else {
      behavior::expression_context context;
      context.member_name = [&](auto value) { return field(value); };
      context.native_type = [&](const auto& value) { return type(value); };
      context.enum_value = [&](const auto& value, auto constant) { return enum_constant(value, constant); };
      context.call = [&](const behavior_expression& call, const std::vector<std::string>& arguments) {
        if (call.call_constructor) { return behavior::constructor_expression(call, arguments, selected,
            [&](const auto& value) { return type(value); }, [&](auto name) { return field(name); }); }
        if (call.call_receiver) {
          std::string receiver = "(" + arguments.at(0) + ")";
          for (const auto base : call.call_base_path) { receiver += "." + field("base" + std::to_string(base)); }
          const auto& callee = behavior::called_function(call);
          return receiver + "." + field(callee.name, callee.access) + "(" +
              behavior::call_arguments(arguments, 1) + ")";
        }
        return names.at(call.call_target) + "(" + behavior::call_arguments(arguments) + ")";
      };
      behavior::free_expression_body(method, selected, result_type,
          [&](auto value) { return name(value, false, options.rename_identifiers); }, arithmetic_helpers, context,
          [&](const auto& text) { line(text); }, [&](const auto& text) { open(text); }, [&]() { close(); });
    }
    close();
  }

  // Dispatch adapters expose interfaces without imposing abstract construction on codec data.
  void dispatch_adapter(const class_node& value) {
    const auto contracts = behavior::contracts(value);
    if (contracts.empty() || language == target::csharp) { return; }
    const auto model_name = names.at(&value);
    const auto interface_name = name(value.name + "_behavior", true, options.rename_identifiers);
    const auto adapter_name = name(value.name + "_behavior_adapter", true, options.rename_identifiers);
    if (language == target::go) {
      open("type " + interface_name + " interface");
      for (const auto* method : contracts) {
        std::string parameters = "receiver *" + model_name;
        for (const auto& argument : method->parameters) { parameters += ", " + name(argument.name, false, options.rename_identifiers) + " " + type(argument.type); }
        line(field(method->name, access_type::public_access) + "(" + parameters + ") " + (method->return_type.name == "void" ? "" : type(method->return_type)));
      }
      close(); open("type " + adapter_name + " struct");
      line("Receiver *" + model_name); line("Implementation " + interface_name); close();
    } else if (language == target::typescript) {
      open("export interface " + interface_name);
      for (const auto* method : contracts) {
        std::string parameters = "receiver: " + model_name;
        for (const auto& argument : method->parameters) { parameters += ", " + name(argument.name, false, options.rename_identifiers) + ": " + type(argument.type); }
        line(field(method->name, access_type::public_access) + "(" + parameters + "): " + (method->return_type.name == "void" ? "void" : type(method->return_type)) + ";");
      }
      close(); open("export class " + adapter_name);
      line("constructor(receiver: " + model_name + ", implementation: " + interface_name + ");");
    } else {
      open("export class " + adapter_name); open("constructor(receiver, implementation)");
      condition("receiver == null || implementation == null"); statement("throw new TypeError('Behavior adapter requires data and implementation')"); close();
      for (const auto* method : contracts) {
        condition("typeof implementation[" + quote(field(method->name, access_type::public_access)) + "] !== 'function'");
        statement("throw new TypeError('Missing required behavior callable')"); close();
      }
      std::string bindings;
      for (const auto* method : contracts) {
        const auto key = quote(field(method->name, access_type::public_access));
        bindings += (bindings.empty() ? "" : ", ") + key + ": implementation[" + key + "].bind(implementation)";
      }
      statement("this.receiver = receiver"); statement("this.implementation = Object.freeze({" + bindings + "})"); close();
    }
    for (const auto* method : contracts) {
      const auto result_type = method->return_type.name == "void" ? (language == target::go ? "" : "void") : type(method->return_type);
      std::string parameters, calls = language == target::go ? "value.Receiver" : "this.receiver";
      for (const auto& argument : method->parameters) {
        const auto id = name(argument.name, false, options.rename_identifiers);
        parameters += (parameters.empty() ? "" : ", ") + id + (language == target::go ? " " + type(argument.type) :
            language == target::typescript ? ": " + type(argument.type) : ""); calls += ", " + id;
      }
      const auto id = field(method->name, access_type::public_access);
      if (language == target::typescript) { line(id + "(" + parameters + "): " + result_type + ";"); continue; }
      open(language == target::go ? "func (value *" + adapter_name + ") " + id + "(" + parameters + ") " + result_type : id + "(" + parameters + ")");
      if (language == target::go) { condition("value == nil || value.Receiver == nil || value.Implementation == nil"); statement("panic(\"Missing required behavior adapter implementation\")"); close(); }
      statement(std::string(result_type.empty() || result_type == "void" ? "" : "return ") +
          (language == target::go ? "value.Implementation." : "this.implementation.") + id + "(" + calls + ")"); close();
    }
    if (language != target::go) { close(); }
  }

  // Compare the selected durable values with exact integers and unordered floating NaN.
  void comparisons(const class_node& value) {
    const auto* settings = comparison::find(value, options.class_comparisons);
    if (!settings || (!settings->equality && !settings->ordering)) { return; }
    const auto identifier = names.at(&value);
    if (language == target::typescript) {
      if (settings->equality) { line("equals_value(other: " + identifier + " | null): boolean;"); }
      if (settings->ordering) { line("compare_value(other: " + identifier + " | null): -1 | 0 | 1 | null;"); }
      return;
    }
    const auto selected = comparison::parts(value, *settings);
    struct operand {
      const rohit::serializer::type_name* type;
      bool record;
      std::string left, right;
    };
    std::vector<operand> operands;
    for (std::size_t index{}; index < value.parents.size(); ++index) {
      const auto& base = value.parents[index];
      if (std::find(selected.bases.begin(), selected.bases.end(), &base) != selected.bases.end()) {
        const auto member_name = field(std::string(language == target::go ? "Base" : "base") +
                                      std::to_string(index), base.access);
        operands.push_back({nullptr, true, choose("this.", "value.", "this.") + member_name,
                            "other." + member_name});
      }
    }
    for (const auto* member : selected.fields) {
      const auto& member_type = member->type_name_list.front();
      const bool record = member_type.resolved_node &&
          (member_type.resolved_node->type == object_type::class_type ||
           member_type.resolved_node->type == object_type::generic_definition);
      const auto member_name = field(member->name, member->access);
      operands.push_back({&member_type, record, choose("this.", "value.", "this.") + member_name,
                          "other." + member_name});
    }
    const auto null_other = choose("!(other instanceof " + identifier + ")",
                                   "value == nil || other == nil", "other is null");
    if (settings->equality) {
      line("// Compare selected values; NaN remains unequal, including to itself.");
      open(choose("equals_value(other)", "func (value *" + identifier + ") EqualsValue(other *" + identifier + ") bool",
                  "public bool equals_value(" + identifier + "? other)"));
      condition(null_other); statement("return false"); close();
      for (const auto& part : operands) {
        const auto equal = part.record ? part.left + choose(".equals_value(", ".EqualsValue(", ".equals_value(") + part.right + ")" :
            part.left + choose(" === ", " == ", " == ") + part.right;
        condition("!(" + equal + ")"); statement("return false"); close();
      }
      statement("return true"); close();
    }
    if (settings->ordering) {
      line("// Return -1, 0 or 1 lexicographically; a reached NaN makes the result unordered.");
      open(choose("compare_value(other)", "func (value *" + identifier + ") CompareValue(other *" + identifier + ") (int, bool)",
                  "public int? compare_value(" + identifier + "? other)"));
      condition(null_other); statement(choose("return null", "return 0, false", "return null")); close();
      std::size_t index{};
      for (const auto& part : operands) {
        const auto result = "srlComparison" + std::to_string(index++);
        if (part.record) {
          if (language == target::go) {
            statement(result + ", " + result + "Ordered := " + part.left + ".CompareValue(" + part.right + ")");
            condition("!" + result + "Ordered || " + result + " != 0");
            statement("return " + result + ", " + result + "Ordered"); close();
          } else {
            statement(choose("const ", "", "int? ") + result + " = " + part.left + ".compare_value(" + part.right + ")");
            condition(language == target::js ? result + " === null || " + result + " !== 0" :
                      "!" + result + ".HasValue || " + result + ".Value != 0");
            statement("return " + result); close();
          }
          continue;
        }
        const auto& member_type = *part.type;
        if (member_type.name == "float" || member_type.name == "double") {
          const auto is_nan = [&](const std::string& expression) {
            return language == target::go ? "math.IsNaN(float64(" + expression + "))" :
                language == target::js ? "Number.isNaN(" + expression + ")" :
                std::string{member_type.name == "float" ? "global::System.Single" : "global::System.Double"} + ".IsNaN(" + expression + ")";
          };
          condition(is_nan(part.left) + " || " + is_nan(part.right));
          statement(choose("return null", "return 0, false", "return null")); close();
        }
        if (member_type.name == "bool") {
          condition(part.left + " != " + part.right);
          condition(part.left); statement(choose("return 1", "return 1, true", "return 1")); close();
          statement(choose("return -1", "return -1, true", "return -1")); close();
        } else if ((member_type.name == "string" && language != target::go) ||
                   (member_type.type == object_type::enum_type && language == target::csharp)) {
          const auto qualified_unit = "global::" + (options.namespace_name.empty() ? "" : options.namespace_name + ".") + unit;
          statement(choose("const ", "", "int ") + result + " = " +
                    (language == target::js ? "srlCompare" : qualified_unit + ".SrlCompare") +
                    "(" + part.left + ", " + part.right + ")");
          condition(result + " < 0"); statement("return -1"); close();
          condition(result + " > 0"); statement("return 1"); close();
        } else {
          condition(part.left + " < " + part.right);
          statement(choose("return -1", "return -1, true", "return -1")); close();
          condition(part.left + " > " + part.right);
          statement(choose("return 1", "return 1, true", "return 1")); close();
        }
      }
      statement(choose("return 0", "return 0, true", "return 0")); close();
    }
  }

  // Emit checked logical array helpers and allocating iteration snapshots.
  void array_access(const class_node& owner) {
    for (const auto& item : owner.member_list) {
      if (!item.array_exposure) { continue; }
      const auto& exposure = *item.array_exposure;
      const auto receiver = language == target::go ? "receiver" : "this";
      const auto storage = std::string{receiver} + "." + field(item.name, item.access);
      const auto& element = item.modifier == member::modifier_type::array ?
          item.type_name_list.front() : item.type_name_list.front().arguments.front();
      const auto scalar = type(element);
      const auto visibility = exposure.access == access_type::public_access ? "public " : "private ";
      const auto get = field("get", exposure.access);
      const auto set = field("set", exposure.access);
      const auto size = field("size", exposure.access);
      const auto values = field("values", exposure.access);
      std::vector<std::uint64_t> dimensions;
      for (const auto& dimension : exposure.dimensions) {
        if (dimension.kind != dimension_expression::operation::literal) {
          throw std::invalid_argument{"Portable array exposure dimensions must be resolved"};
        }
        dimensions.push_back(dimension.value);
      }
      const auto rank = dimensions.empty() ? std::size_t{1} : dimensions.size();
      std::string parameters;
      std::string offset;
      for (std::size_t index = 0; index < rank; ++index) {
        const auto coordinate = "index" + std::to_string(index);
        parameters += (parameters.empty() ? "" : ", ") + choose(coordinate, coordinate + " int", "int " + coordinate);
        offset = offset.empty() ? coordinate :
            "(" + offset + " * " + std::to_string(dimensions[index]) + " + " + coordinate + ")";
      }
      if (language == target::typescript) {
        std::string declarations;
        for (std::size_t index = 0; index < rank; ++index) {
          declarations += (declarations.empty() ? "" : ", ") + std::string{"index"} + std::to_string(index) + ": number";
        }
        const auto access = exposure.access == access_type::public_access ? "" : "private ";
        line("// Checked logical access; iteration allocates a shallow snapshot.");
        line(std::string{access} + get + "(" + declarations + "): " + scalar + ";");
        if (!exposure.readonly) { line(std::string{access} + set + "(" + declarations + ", item: " + scalar + "): void;"); }
        line(std::string{access} + size + "(): number;");
        line(std::string{access} + values + "(): " + (exposure.readonly ? "ReadonlyArray<" + scalar + ">" : scalar + "[]") + ";");
        continue;
      }
      const auto failure = [&](const std::string& message, bool bounds) {
        statement(choose("throw new " + std::string{bounds ? "RangeError" : "TypeError"} + "(" + quote(message) + ")",
            "panic(fmt.Errorf(" + quote(message) + "))",
            "throw new System." + std::string{bounds ? "IndexOutOfRangeException" : "InvalidOperationException"} + "(" + quote(message) + ")"));
      };
      line("// Return the validated logical length of the selected storage.");
      open(choose(size + "()", "func (receiver *" + names.at(&owner) + ") " + size + "() int", std::string{visibility} + "int " + size + "()"));
      if (language == target::go) {
        condition("receiver == nil"); failure("Nil array owner", false); close();
      } else {
        condition(language == target::js ? "!Array.isArray(" + storage + ")" : storage + " == null");
        failure("Invalid array storage", false); close();
      }
      const auto length = choose(storage + ".length", "len(" + storage + ")", storage + ".Count");
      if (item.fixed_extent != 0) {
        condition(length + " != " + std::to_string(item.fixed_extent));
        failure("Invalid fixed array length", false); close();
      }
      statement("return " + length);
      close();
      const auto bounds = [&]() {
        const auto size_call = std::string{receiver} + "." + size + "()";
        if (dimensions.empty()) { local("count", size_call); }
        else { statement((language == target::go ? "_ = " : "") + size_call); }
        for (std::size_t index = 0; index < rank; ++index) {
          const auto coordinate = "index" + std::to_string(index);
          const auto extent = dimensions.empty() ? "count" : std::to_string(dimensions[index]);
          const auto valid_integer = language == target::js ? "!Number.isSafeInteger(" + coordinate + ") || " : "";
          condition(valid_integer + coordinate + " < 0 || " + coordinate + " >= " + extent);
          failure("Array coordinate out of bounds", true); close();
        }
      };
      line("// Read one element; object elements retain ordinary reference semantics.");
      open(choose(get + "(" + parameters + ")", "func (receiver *" + names.at(&owner) + ") " + get + "(" + parameters + ") " + scalar,
          std::string{visibility} + scalar + " " + get + "(" + parameters + ")"));
      bounds();
      statement("return " + storage + "[" + offset + "]");
      close();
      if (!exposure.readonly) {
        line("// Replace an element after validating every coordinate and the storage.");
        open(choose(set + "(" + parameters + ", item)", "func (receiver *" + names.at(&owner) + ") " + set + "(" + parameters + ", item " + scalar + ")",
            std::string{visibility} + "void " + set + "(" + parameters + ", " + scalar + " item)"));
        bounds();
        statement(storage + "[" + offset + "] = item");
        close();
      }
      line("// Allocate an iterable snapshot; object elements are shared, not deeply copied.");
      const auto snapshot_type = std::string{exposure.readonly ? "System.Collections.Generic.IReadOnlyList<" : "System.Collections.Generic.List<"} + scalar + ">";
      open(choose(values + "()", "func (receiver *" + names.at(&owner) + ") " + values + "() []" + scalar,
          std::string{visibility} + snapshot_type + " " + values + "()"));
      if (language == target::go) {
        local("count", "receiver." + size + "()");
        local("result", "make([]" + scalar + ", count)");
        statement("copy(result, " + storage + ")");
        statement("return result");
      } else {
        statement("this." + size + "()");
        if (language == target::js) {
          statement("return " + std::string{exposure.readonly ? "Object.freeze(" : ""} + storage + ".slice()" + (exposure.readonly ? ")" : ""));
        } else {
          statement("return new System.Collections.Generic.List<" + scalar + ">(" + storage + ")" + (exposure.readonly ? ".AsReadOnly()" : ""));
        }
      }
      close();
    }
  }

  // Reuse native target output traversal while each getter borrows its source value.
  std::string translator_codecs(const conversion_declaration& method, std::string_view view) {
    if (language == target::typescript) {
      return options.xml.enabled ? "encodeXml(limits?:XmlLimits,validator?:XmlValidator):string; xmlBindNamespace<View>(this:View,prefix:string,uri:string):View;" : "";
    }
    auto saved_source = std::move(source);
    const auto saved_level = level;
    source.clear(); level = 2;
    const auto& object = ::rohit::serializer::conversion::record(method.target);
    const auto target_language = language == target::go ? "go" : language == target::csharp ? "csharp" : "js";
    auto projected = translator::output_fields(method, target_language, [&](auto value, auto access) { return field(value, access); });
    line("// Encode readonly projections without constructing the target model.");
    open(choose("encode(protocol)", "func (value " + std::string{view} + ") Encode(protocol Protocol) ([]byte, error)", "public byte[] Encode(Protocol protocol)"));
    if (options.protocols == output_protocols::binary_none) {
      line(choose("if (protocol !== Protocol.BINARY_NONE) { throw new RangeError('Protocol disabled during generation'); }", "if protocol != BINARY_NONE { return nil, fmt.Errorf(\"Protocol disabled during generation\") }", "if (protocol != Protocol.BINARY_NONE) { throw new System.ArgumentException(\"Protocol disabled during generation\"); }"));
    }
    local("output", choose("new SrlWriter(protocol)", "srlNewWriter(protocol)", "new SrlWriter(protocol)"));
    statement(choose("this.write(output)", "value.write(output)", "this.Write(output)"));
    if (language == target::go) { line("if output.err != nil { return nil, output.err }"); statement("return output.bytes, nil"); }
    else { statement("return output.bytes()"); }
    close();
    line("// Traverse target framing and field projections in schema order.");
    open(choose("write(output)", "func (value " + std::string{view} + ") write(output *srlWriter)", "internal void Write(SrlWriter output)"));
    projection_fields_ = std::move(projected);
    write_object(object);
    projection_fields_.clear(); close();
    if (options.xml.enabled && language == target::js) {
      const auto contract = xml_schema_.bind(object);
      source += xml_binding::js_translator_xml_methods(object, contract, names.at(&object),
          [&](const auto& item) { return field(item.name, item.access); },
          [&](const auto& item) { return type(item); }, options.xml);
    }
    if (options.xml.enabled && (language == target::go || language == target::csharp)) {
      const auto contract = xml_schema_.bind(object);
      const auto fields = [&](const member& item) { return field(item.name, item.access); };
      const auto types = [&](const auto& item) { return type(item); };
      const auto alternatives = [&](std::string_view alias) { return name(std::string{alias}, true, true); };
      const auto getter = [&](const member& item) { return field("get_" + item.name, access_type::public_access) + "()"; };
      source += language == target::go ? xml_portable::methods(object, contract, std::string{view}, fields, types,
          options.xml, "go", alternatives, true, getter, go_xml_operation_name) :
          xml_csharp::methods(object, contract, options.xml, view, fields, types, alternatives, false, true, getter);
    }
    auto result = std::move(source); source = std::move(saved_source); level = saved_level;
    return result;
  }

  // Emit an owning class with exact-message public APIs.
  void object(const class_node& value) {
    reference_fields_ = value.reference.has_value();
    const auto identifier = names.at(&value);
    line("// " + identifier +
         " owns its fields; concurrent mutation during encoding is unsupported.");
    open(choose("export class " + identifier, "type " + identifier + " struct",
                "public sealed " + std::string(value.functions.empty() ? "" : "partial ") + "class " + identifier));
    if (value.magic_field && language != target::go) {
      const auto& magic = *value.magic_field;
      const bool visible = value.magic_access == access_type::public_access;
      line("// Fixed scalar schema metadata has no per-object storage.");
      if (language == target::js) {
        statement("static #srlMagic = " + default_value(magic));
        if (visible) {
          open("static get magic()");
          statement("return " + identifier + ".#srlMagic");
          close();
        }
      } else if (language == target::typescript) {
        line(std::string(visible ? "" : "private ") + "static readonly magic: " +
             type(magic.type_name_list.front()) + ";");
      } else {
        statement(std::string(visible ? "public " : "private ") + "const " +
                  type(magic.type_name_list.front()) + " Magic = " + default_value(magic));
      }
    } else if (value.has_magic() && language != target::go) {
      std::string values;
      for (const auto byte : value.magic_bytes) {
        values += (values.empty() ? "" : ", ") +
                  std::to_string(static_cast<unsigned char>(byte));
      }
      const bool visible = value.magic_access == access_type::public_access;
      line("// Fixed schema metadata has no per-object storage.");
      if (language == target::js) {
        statement("static #srlMagic = Object.freeze([" + values + "])");
        if (visible) {
          open("static get magic()");
          statement("return " + identifier + ".#srlMagic");
          close();
        }
      } else if (language == target::typescript) {
        line(std::string(visible ? "" : "private ") +
             "static readonly magic: readonly number[];");
      } else {
        statement("private static readonly byte[] srlMagic = new byte[] { " + values + " }");
        // Owning C# classes are sealed, so all non-public schema metadata stays private.
        const auto visibility = visible ? "public " : "private ";
        statement(std::string(visibility) + "static System.ReadOnlySpan<byte> Magic => srlMagic");
      }
    }
    if (language == target::js) {
      for (std::size_t i = 0; i < value.parents.size(); ++i) {
        const auto& base = value.parents[i];
        if (base.access != access_type::public_access) {
          statement(field(std::string(language == target::go ? "Base" : "base") + std::to_string(i), base.access));
        }
      }
      for (const auto& item : value.member_list) {
        if (item.wire_only || item.access == access_type::public_access) {
          continue;
        }
        const auto member_identifier = field(item.name, item.access);
        if (item.modifier == member::modifier_type::variant && !tagged_variant(item)) {
          statement(member_identifier + "Index");
          for (const auto& alternative : item.type_name_list) {
            statement(member_identifier + name(alternative.enum_name, true, true));
          }
        } else {
          statement(member_identifier);
        }
      }
      line("// Construct fresh schema defaults.");
      open("constructor()");
      declarations(value);
      close();
    } else {
      declarations(value);
    }
    if (language == target::go) {
      source += reference_support::go_binding_fields(value, identifier,
          [&](const auto& item) { return type(item); });
      if (options.xml.enabled && !value.reference && !xml_mixed_storage_only_.contains(&value)) {
        const auto xml_contract = xml_schema_.bind(value);
        if (std::any_of(xml_contract.fields.begin(), xml_contract.fields.end(), xml_portable::namespace_dependent)) {
          line("// XML-only QName scopes preserve field occurrences without changing native wire fields.");
          line("srlXmlNamespaces map[int][]map[string]string");
        }
      }
      if(options.xml.enabled&&!value.reference&&!xml_mixed_storage_only_.contains(&value)&&xml_schema_.bind(value).extensions){
        line("// XML-only opaque preservation is excluded from native fields and wire codecs.");
        line("srlXmlPreserved *srlXmlPreserved");
      }
    }
    if (language != target::go) { array_access(value); behaviors(value); }
    if (language != target::go) {
      source += reference_support::portable_methods(value, identifier,
          language == target::csharp ? "csharp" : language == target::typescript ? "typescript" : "js",
          [&](const auto& item) { return type(item); },
          [&](auto item, auto access) { return field(item, access); });
    }
    if (language != target::go) {
      source += construction::factories(value, identifier,
          language == target::csharp ? "csharp" : language == target::typescript ? "typescript" : "js",
          [&](const auto& item) { return type(item); },
          [&](auto item, auto access) { return field(item, access); },
          [&](const auto& item) { return default_value(item); });
      source += conversion::methods(value, identifier,
          language == target::csharp ? "csharp" : language == target::typescript ? "typescript" : "js",
          [&](const auto& item) { return type(item); },
          [&](auto item, auto access) { return field(item, access); });
      source += translator::methods(value, identifier,
          language == target::csharp ? "csharp" : language == target::typescript ? "typescript" : "js",
          [&](const auto& item) { return type(item); },
          [&](auto item, auto access) { return field(item, access); },
          [&](const auto& item) { return default_value(item); },
          [&](const auto& method, auto view) { return translator_codecs(method, view); });
      comparisons(value);
    }
    if (language == target::typescript) {
      line("constructor();");
      const auto protocol_type = options.protocols == output_protocols::binary_none ? "1" : "Protocol";
      line("encode(protocol: " + std::string{protocol_type} + "): Uint8Array;");
      line("static decode(bytes: Uint8Array, protocol: " + std::string{protocol_type} + ", limits?: Limits): " + identifier +
           ";");
      if (options.xml.enabled) {
        if (const auto mixed = xml_mixed_variants_.find(&value); mixed != xml_mixed_variants_.end()) {
          source += xml_binding::js_mixed_fragment_methods(*mixed->second, identifier,
              [&](const auto& item) { return field(item.name, item.access); }, xml_mixed_storage_only_.contains(&value), true);
        }
        if (value.reference && value.reference->value_projection) {
          source += xml_binding::js_reference_methods(value, identifier, true);
        } else if (!xml_mixed_storage_only_.contains(&value)) {
          source += xml_binding::js_methods(value, xml_schema_.bind(value),
              identifier, [&](const auto& item) { return field(item.name, item.access); },
              [&](const auto& item) { return type(item); }, options.xml, true);
        }
      }
      close();
      dispatch_adapter(value);
      return;
    }
    if (language == target::go) {
      close();
      if (value.magic_field) {
        const auto& magic = *value.magic_field;
        line("// Fixed scalar identity is an immutable package constant.");
        statement("const " + magic_constant(value) + " " + type(magic.type_name_list.front()) +
                  " = " + default_value(magic));
      } else if (value.has_magic()) {
        std::string bytes_literal = "\"";
        constexpr std::string_view hex_digits = "0123456789abcdef";
        for (const auto byte : value.magic_bytes) {
          const auto value_byte = static_cast<unsigned char>(byte);
          bytes_literal += "\\x";
          bytes_literal += hex_digits[value_byte >> 4];
          bytes_literal += hex_digits[value_byte & 0x0f];
        }
        bytes_literal += '"';
        auto magic_name = identifier + "Magic";
        if (value.magic_access != access_type::public_access) {
          magic_name.front() = static_cast<char>(
              std::tolower(static_cast<unsigned char>(magic_name.front())));
        }
        line("// Fixed schema identity is an immutable package constant.");
        statement("const " + magic_name + " = " + bytes_literal);
      }
      line("// New" + identifier + " constructs schema defaults; the Go zero value may differ.");
      open("func New" + identifier + "() *" + identifier);
      open("return &" + identifier);
      declarations(value, true);
      close();
      close();
    }
    if (language == target::go) { array_access(value); behaviors(value); }
    if (language == target::go) {
      source += reference_support::portable_methods(value, identifier, "go",
          [&](const auto& item) { return type(item); },
          [&](auto item, auto access) { return field(item, access); });
    }
    if (language == target::go) {
      source += construction::factories(value, identifier, "go",
          [&](const auto& item) { return type(item); },
          [&](auto item, auto access) { return field(item, access); },
          [&](const auto& item) { return default_value(item); });
      source += conversion::methods(value, identifier, "go",
          [&](const auto& item) { return type(item); },
          [&](auto item, auto access) { return field(item, access); });
      source += translator::methods(value, identifier, "go",
          [&](const auto& item) { return type(item); },
          [&](auto item, auto access) { return field(item, access); },
          [&](const auto& item) { return default_value(item); },
          [&](const auto& method, auto view) { return translator_codecs(method, view); });
      comparisons(value);
    }
    line("// Encode one complete message into independent storage.");
    open(choose("encode(protocol)",
                "func (value *" + identifier + ") Encode(protocol Protocol) ([]byte, error)",
                "public byte[] Encode(Protocol protocol)"));
    if (options.protocols == output_protocols::binary_none) {
      line(choose("if (protocol !== Protocol.BINARY_NONE) { throw new RangeError('Protocol disabled during generation'); }",
                  "if protocol != BINARY_NONE { return nil, fmt.Errorf(\"Protocol disabled during generation\") }",
                  "if (protocol != Protocol.BINARY_NONE) { throw new System.ArgumentException(\"Protocol disabled during generation\"); }"));
    }
    local("output",
          choose("new SrlWriter(protocol)", "srlNewWriter(protocol)", "new SrlWriter(protocol)"));
    statement(choose("this.write(output)", "value.write(output)", "this.Write(output)"));
    if (language == target::go) {
      condition("output.err != nil");
      statement("return nil, output.err");
      close();
      statement("return output.bytes, nil");
    } else {
      statement("return output.bytes()");
    }
    close();
    line("// Decode a fresh value and expose it only after exact-message validation.");
    open(choose("static decode(bytes, protocol, limits = new Limits())",
                "func Decode" + identifier +
                    "(bytes []byte, protocol Protocol, policies ...Limits) (*" + identifier +
                    ", error)",
                "public static " + identifier +
                    " Decode(System.ReadOnlyMemory<byte> bytes, Protocol protocol, Limits? limits "
                    "= null)"));
    if (options.protocols == output_protocols::binary_none) {
      line(choose("if (protocol !== Protocol.BINARY_NONE) { throw new RangeError('Protocol disabled during generation'); }",
                  "if protocol != BINARY_NONE { return nil, fmt.Errorf(\"Protocol disabled during generation\") }",
                  "if (protocol != Protocol.BINARY_NONE) { throw new System.ArgumentException(\"Protocol disabled during generation\"); }"));
    }
    if (language == target::go) {
      local("limits", "DefaultLimits()");
      condition("len(policies) > 1");
      statement("return nil, fmt.Errorf(\"Expected at most one limits policy\")");
      close();
      condition("len(policies) == 1");
      statement("limits = policies[0]");
      close();
    }
    local("input",
          choose("new SrlReader(bytes, protocol, limits)", "srlNewReader(bytes, protocol, limits)",
                 "new SrlReader(bytes, protocol, limits ?? new Limits())"));
    local("result", choose(identifier + ".read(input, new " + identifier + "())",
                           "read" + identifier + "(input, New" + identifier + "())",
                           identifier + ".Read(input, new " + identifier + "())"));
    statement("input.finish()");
    if (language == target::go) {
      condition("input.err != nil");
      statement("return nil, input.err");
      close();
      statement("return result, nil");
    } else {
      statement("return result");
    }
    close();
    line("// Write schema fields directly in declaration order.");
    open(choose("write(output)", "func (value *" + identifier + ") write(output *srlWriter)",
                "internal void Write(SrlWriter output)"));
    if (language == target::js && options.protocols == output_protocols::binary_none) {
      line("if (output.protocol !== Protocol.BINARY_NONE) { throw new RangeError('Protocol disabled during generation'); }");
    }
    if (language == target::go) {
      condition("value == nil");
      statement("output.fail(\"Nil object\")");
      statement("return");
      close();
    }
    write_object(value);
    close();
    line("// Merge repeated nested fields within the fresh decoding candidate.");
    open(choose(
        "static read(input, result)",
        "func read" + identifier + "(input *srlReader, result *" + identifier + ") *" + identifier,
        "internal static " + identifier + " Read(SrlReader input, " + identifier + " result)"));
    if (language == target::js && options.protocols == output_protocols::binary_none) {
      line("if (input.protocol !== Protocol.BINARY_NONE) { throw new RangeError('Protocol disabled during generation'); }");
    }
    if (language == target::go) {
      condition("input.err != nil");
      statement("return result");
      close();
    }
    read_object(value);
    close();
    if (options.xml.enabled) {
      const auto field_name = [&](const auto& item) { return field(item.name, item.access); };
      const auto type_name = [&](const auto& item) { return type(item); };
      const auto alternative_name = [&](std::string_view alias) { return name(std::string{alias}, true, true); };
      if (language == target::js) {
        if (const auto mixed = xml_mixed_variants_.find(&value); mixed != xml_mixed_variants_.end()) {
          source += xml_binding::js_mixed_fragment_methods(*mixed->second, identifier, field_name,
              xml_mixed_storage_only_.contains(&value), false);
        }
      }
      if (language == target::csharp) {
        if (const auto mixed = xml_mixed_variants_.find(&value); mixed != xml_mixed_variants_.end()) {
          source += xml_csharp::mixed_fragment_methods(value, *mixed->second, field_name, type_name, alternative_name);
        }
      }
      if (language == target::js && value.reference && value.reference->value_projection) {
        source += xml_binding::js_reference_methods(value, identifier, false);
      } else if (value.reference && value.reference->value_projection) {
        if (language == target::go) {
          source += xml_portable::reference_methods(value, identifier, type_name, "go");
        } else if (language == target::csharp) {
          source += xml_portable::managed_reference_methods(value, identifier,
              [&](const auto& item, const auto& expression, bool encoding) {
                return xml_csharp::scalar_value(item, expression, encoding, type_name);
              }, "csharp");
        }
      } else if (!xml_mixed_storage_only_.contains(&value)) {
        const auto contract = xml_schema_.bind(value);
        if (language == target::go) {
          source += xml_portable::methods(value, contract, identifier, field_name, type_name,
                                          options.xml, "go", alternative_name, false, {}, go_xml_operation_name,
                                          [&]{std::string candidate="Decode"+identifier+"XMLWithOperation";while(globals.contains(candidate)){candidate+="_2";}globals.insert(candidate);return candidate;}());
        } else if (language == target::csharp) {
          const bool needs_base_bridges = std::any_of(names.begin(), names.end(), [&](const auto& entry) {
            if (entry.first->type != object_type::class_type) { return false; }
            const auto& owner = static_cast<const class_node&>(*entry.first);
            return std::any_of(owner.parents.begin(), owner.parents.end(), [&](const auto& parent) {
              return parent.parent_class == &value;
            });
          });
          source += xml_csharp::methods(value, contract, options.xml, identifier, field_name,
              type_name, alternative_name, needs_base_bridges);
        } else {
          source += xml_binding::js_methods(value, contract, identifier, field_name, type_name,
                                            options.xml);
        }
      }
    }
    if (language != target::go) {
      close();
    }
    dispatch_adapter(value);
    reference_fields_ = false;
  }

public:
  // Bind immutable generation policy for one compilation unit.
  emitter(target language, const portable_options& options, std::string_view unit_name)
      : language{language}, options{options}, xml_schema_{options.xml},
        unit{name(unit_name, true, options.rename_identifiers)} {}
  // Validate all declarations before returning complete native source.
  std::string generate(const std::vector<std::unique_ptr<syntax_node>>& values) {
    if (options.xml.enabled) {
      for (const auto identifier : {"XmlLimits", "XmlOptions", "XmlValidator", "XmlSchema",
                                    "DefaultXmlLimits", "DefaultXmlOptions"}) {
        globals.insert(identifier);
      }
      if (language == target::go) {
        globals.insert("XmlMapLimits"); globals.insert("DefaultXmlMapLimits");
        globals.insert("XmlMixedLimits"); globals.insert("DefaultXmlMixedLimits");
        globals.insert("XmlIdentityLimits"); globals.insert("DefaultXmlIdentityLimits");
        for (const auto identifier : {"XmlExtensionLimits", "XmlExtensionOptions", "DefaultXmlExtensionLimits", "DefaultXmlExtensionOptions", "XmlDigestLimits", "DefaultXmlDigestLimits", "srlXmlDigestPolicy", "srlXmlDefaultDigestBytes",
                 "srlXmlRevisionLexical", "srlXmlRevisionNumber", "srlXmlHexDigit", "srlXmlDigestRead", "srlXmlDigestText"}) {
          globals.insert(identifier);
        }
        for (const auto identifier : {"HtmlLimits", "HtmlAttribute", "HtmlEvent", "DefaultHtmlLimits",
                 "HtmlSupportedSubset", "HtmlParseFragment", "HtmlWriteFragment", "srlHtmlPolicy",
                 "srlHtmlTags", "srlHtmlVoids", "srlHtmlPhrasing", "srlHtmlSpace", "srlHtmlText",
                 "srlHtmlDecode", "srlHtmlChild", "srlHtmlAttribute", "srlHtmlOutput", "srlHtmlParser"}) {
          globals.insert(identifier);
        }
      }
      if (language == target::js || language == target::typescript) {
        for (const auto identifier : {"Set", "WeakMap", "Symbol", "Promise", "srlXmlProjectionContracts", "srlXmlProjectionReadonly", "srlXmlProjectionContract", "srlXmlProjectionInput",
                 "SrlXmlParser", "SrlXmlWriter", "SrlXmlFields", "SrlXmlSaxes", "SrlXmlTokenFrame", "SrlXmlMapFrame", "SrlXmlVariantFrame", "SrlXmlNcName", "SrlXmlNameRule", "SrlXmlTokenRule", "SrlXmlLexicalError", "SrlXmlLimitError", "srlXmlPackageQuotaDepth", "srlXmlWithPackageQuota", "srlXmlQuotaError", "XmlMapLimits", "XmlPatternLimits", "XmlMixedLimits", "XmlIdentityLimits", "SrlXmlIdentity",
                 "srlXmlMixedAppend", "srlXmlMixedFlush", "srlXmlMixedCharacters", "srlXmlMixedWrite",
                 "XmlExtensionLimits", "XmlExtensionOptions", "SrlXmlExtensions", "SrlXmlForeignFrame", "srlXmlExtensionState", "srlXmlExtensionMetadataBytes",
                 "srlXmlExtensionScope", "srlXmlExtensionName", "srlXmlExtensionBindings", "srlXmlExtensionOutput", "srlXmlExtensionEmit", "srlXmlExtensionAt", "srlXmlExtensionPositionEnd",
                 "Html", "HtmlLimits", "HtmlEvent", "HtmlAttribute",
                 "srlXmlModelNamespace", "srlXmlModelPrefix", "srlXmlModelRevisionName", "srlXmlModelMagicName", "srlXmlDigestMaxBytes",
                 "srlXmlModelRevision", "srlXmlModelCompare", "srlXmlModelSelect", "srlXmlModelInput", "srlXmlModelOutput", "srlXmlDigest",
                 "XmlSchema", "srlXmlSchemaNamespace", "srlXmlSchemaSecret", "srlXmlProviderScan",
                 "srlXmlNamespaceState", "srlXmlCompleteName", "srlXmlScalarQName", "srlXmlRememberNamespaces",
                 "srlXmlBindNamespace", "srlXmlFieldScope", "srlXmlReadField", "srlXmlPrepareField", "srlXmlMergeBindings",
                 "srlXmlScalarCache", "srlXmlListRules", "srlXmlSingleBits", "srlXmlIntegerKinds", "srlXmlIntegerWidths",
                 "srlXmlScalarWhitespace", "srlXmlDecimal", "srlXmlDecimalCompare", "srlXmlScientificDigits",
                 "srlXmlSingleMidpoint", "srlXmlSingle", "srlXmlScalarValue", "srlXmlScalarEqual", "srlXmlScalarRule",
                 "srlXmlLexicalTokens", "srlXmlListRule", "srlXmlListRead", "srlXmlListWrite", "srlXmlPortablePattern", "srlXmlCheckedText"}) {
          globals.insert(identifier);
        }
      } else if (language == target::csharp) {
        for (const auto identifier : csharp_xml_identifiers) { globals.emplace(identifier); }
      }
    }
    if (language == target::csharp) {
      validate_name(unit, language);
      if (!globals.insert(unit).second) {
        throw std::invalid_argument{"C# outer class shadows a runtime type"};
      }
    }
    if (language == target::go) {
      for (const auto identifier :
           {"binary",       "json",    "fmt",    "math",    "regexp",      "sort",
            "strconv",      "strings", "utf8",   "bool",    "byte",        "int",
            "int8",         "int16",   "int32",  "int64",   "uint",        "uint8",
            "uint16",       "uint32",  "uint64", "float32", "float64",     "string",
            "error",        "nil",     "iota",   "len",     "cap",         "make",
            "append",       "copy",    "new",    "JSON",    "BINARY_NONE", "BINARY_INTEGER",
            "BINARY_STRING"}) {
        globals.insert(identifier);
      }
    } else if (language == target::js || language == target::typescript) {
      for (const auto identifier :
           {"Array", "BigInt", "DataView", "Error", "Infinity", "JSON", "Map", "Math", "NaN",
            "Number", "Object", "RangeError", "String", "TextDecoder", "TextEncoder", "TypeError",
            "Uint8Array", "undefined"}) {
        globals.insert(identifier);
      }
    }
    if (options.xml.enabled) {
      // Identify storage before registration: C# inventories static scalar helpers while registering native names.
      const auto collect_mixed = [&](auto&& self, const auto& statements) -> void {
        for (const auto& node : statements) {
          if (node->type == object_type::namespace_type) {
            self(self, static_cast<const namespace_node&>(*node).statements);
          } else if (node->type == object_type::class_type) {
            const auto& owner = static_cast<const class_node&>(*node);
            const auto configured = options.xml.classes.find(owner.get_full_name());
            if (configured == options.xml.classes.end() || (!configured->second.contracts.contains("mixed") &&
                !configured->second.contracts.contains("ordered"))) { continue; }
            const auto binding = xml_schema_.bind(owner);
            if (binding.mixed) {
              xml_mixed_variants_.emplace(binding.mixed->fragment, binding.mixed->variant);
              if (!options.xml.classes.contains(binding.mixed->fragment->get_full_name())) {
                xml_mixed_storage_only_.insert(binding.mixed->fragment);
              }
            }
          }
        }
      };
      collect_mixed(collect_mixed, values);
    }
    register_nodes(values);
    if (language == target::go) {
      for (const auto* package : packages) {
        for (const auto suffix : {"Input", "Value", "LoadOptions", "SaveOptions", "Encode", "Write", "Load", "Save"}) {
          if (!globals.insert(names.at(package) + suffix).second) {
            throw std::invalid_argument{"Go package companion name collision: " + names.at(package) + suffix};
          }
        }
      }
    }
    if ((language == target::js || language == target::typescript) &&
        std::any_of(nodes.begin(), nodes.end(), [](const auto* node) {
          return node->type == object_type::class_type && static_cast<const class_node&>(*node).reference &&
              static_cast<const class_node&>(*node).reference->mode != reference_declaration::mode_type::shared;
        }) && !globals.insert("WeakRef").second) {
      throw std::invalid_argument{"JavaScript weak reference runtime name collision: WeakRef"};
    }
    if (language == target::go) {
      for (const auto* node : nodes) {
        if (node->type != object_type::class_type || !static_cast<const class_node&>(*node).reference) { continue; }
        for (const auto& generated : {names.at(node) + "Scope", "New" + names.at(node) + "Scope"}) {
          if (!globals.insert(generated).second) { throw std::invalid_argument{"Go reference scope name collision: " + generated}; }
        }
      }
    }
    behavior::require_unique_free_names(nodes, choose("js", "go", "csharp"), [&](const auto& node) { return names.at(&node); },
        language == target::csharp ? std::function<std::string(const type_name&)>{[&](const auto& value) { return type(value); }} :
        std::function<std::string(const type_name&)>{});
    validate();
    if(language==target::go&&options.xml.enabled){
      while(globals.contains(go_xml_operation_name)){go_xml_operation_name+="_2";}
      globals.insert(go_xml_operation_name);
    }
    arithmetic_helpers = behavior::select_helpers(nodes,
        [&](const auto& node) { return names.at(&node); },
        [&](const auto name) { return field(name, language == target::go ?
            access_type::private_access : access_type::public_access); });
    source += generated_license_notice(values);
    if (language == target::go) {
      validate_name(options.package_name, language);
      line("package " + options.package_name);
      if (options.xml.enabled) { source += xml_portable::imports("go"); }
      source += behavior::preambles(values, "go");
      source += "import (srlCompressionBytes \"bytes\";srlCompressionFlate \"compress/flate\";"
          "srlCompressionGzip \"compress/gzip\";srlCompressionZlib \"compress/zlib\";"
          "srlCompressionCRC32 \"hash/crc32\";srlCompressionIO \"io\")\n";
      source += "import (srlPackageTime \"time\";srlPackageOS \"os\";srlPackageIO \"io\";"
          "srlPackageErrors \"errors\";srlPackageBytes \"bytes\";srlPackageStrings \"strings\";"
          "srlPackageUTF8 \"unicode/utf8\";srlPackageBinary \"encoding/binary\";srlPackageSort \"sort\";"
          "srlPackageCRC32 \"hash/crc32\";srlPackageFilepath \"path/filepath\";srlPackageRuntime \"runtime\")\n";
      if (!packages.empty()) {
        source += "import (srlPackageIP \"net/netip\";srlPackageXML \"encoding/xml\";"
            "srlPackageRegexp \"regexp\";srlPackageStrconv \"strconv\")\n";
      }
      source += go_runtime;
      std::string compression_prefix;
      source += go_compression_support(globals, options.protocols == output_protocols::binary_none,
                                       compression_prefix);
      source += go_package_support(globals, compression_prefix, !packages.empty(), go_package_prefix);
      if (options.xml.enabled) {
        auto xml_runtime=xml_portable::runtime("go");
        constexpr std::string_view marker{"@xml_operation_options@"};
        for(std::size_t offset{};(offset=xml_runtime.find(marker,offset))!=std::string::npos;offset+=go_xml_operation_name.size()){xml_runtime.replace(offset,marker.size(),go_xml_operation_name);}
        source += xml_runtime;
      }
      source += behavior::helpers("go", arithmetic_helpers);
    } else if (language == target::csharp) {
      line("#nullable enable");
      source += behavior::preambles(values, "csharp");
      if (!options.namespace_name.empty()) {
        std::size_t start{};
        do {
          const auto end = options.namespace_name.find('.', start);
          validate_name(
              options.namespace_name.substr(start, end == std::string::npos ? end : end - start),
              language);
          if (end == std::string::npos) {
            break;
          }
          start = end + 1;
        } while (true);
        open("namespace " + options.namespace_name);
      }
      open("public static partial class " + unit);
      source += "// Serializer runtime support begins.\n";
      source += csharp_runtime;
      std::string compression_prefix;
      source += csharp_compression_support(globals, compression_prefix);
      csharp_package_prefix = csharp_package_name(globals);
      source += "// Generated C# package helper name: " + csharp_package_prefix + '\n';
      source += package_codegen::csharp_runtime(csharp_package_prefix, compression_prefix, !packages.empty());
      if (options.xml.enabled) { source += xml_csharp::runtime(); }
      source += behavior::helpers("csharp", arithmetic_helpers);
    } else if (language == target::js) {
      source += behavior::preambles(values, "js");
      source += js_runtime;
      source += js_compression_support(globals, false);
      if (!packages.empty()) { source += javascript_package_support(globals, false); }
      if (options.xml.enabled) { source += xml_binding::js_runtime(); }
      source += behavior::helpers("js", arithmetic_helpers);
      source += behavior::helpers_typed("js", arithmetic_helpers);
    } else {
      source += behavior::preambles(values, "typescript");
      line("export declare const Protocol: Readonly<{JSON: 0; BINARY_NONE: 1; BINARY_INTEGER: 2; "
           "BINARY_STRING: 3}>;");
      line("export type Protocol = typeof Protocol[keyof typeof Protocol];");
      line(
          "export declare const ReadPolicy: Readonly<{ STRICT: 0; COMPATIBLE: 1; FLEXIBLE: 2 }>; ");
      line("export declare class Limits {");
      line("  readonly maxBytes: number; readonly maxStringBytes: number; readonly maxElements: "
           "number; readonly maxDepth: number; readonly readPolicy: number;");
      line("  constructor(maxBytes?: number, maxStringBytes?: number, maxElements?: number, "
           "maxDepth?: number, readPolicy?: number);");
      line("}");
      source += js_compression_support(globals, true);
      if (!packages.empty()) { source += javascript_package_support(globals, true); }
      if (options.xml.enabled) { source += xml_binding::js_declarations(); }
    }
    if (language != target::typescript) {
      emit_fields();
    }
    if (language == target::csharp) { source += "// Serializer schema declarations begin.\n"; }
    for (const auto* node : nodes) {
      if (node->type == object_type::enum_type) {
        enumeration(static_cast<const enum_node&>(*node));
      } else if (node->type == object_type::function_type) {
        free_function(static_cast<const function_node&>(*node));
      } else {
        object(static_cast<const class_node&>(*node));
      }
    }
    for (const auto* package : packages) {
      if (language == target::go) {
        auto selected_prefix = "SerializerPackageCodecs" + names.at(package);
        const auto selected_base = selected_prefix;
        std::size_t selected_suffix{1U};
        while (std::any_of(globals.begin(), globals.end(), [&](const auto& symbol) {
          return symbol.starts_with(selected_prefix);
        })) {
          if (selected_suffix == std::numeric_limits<std::size_t>::max()) {
            throw std::length_error{"Go selected codec names exhausted"};
          }
          selected_prefix = selected_base + '_' + std::to_string(++selected_suffix);
        }
        for (const auto suffix : {"Encode", "Load", "Write", "Save", "LoadCanonical", "LoadCanonicalWithCodecs", "LoadMode"}) {
          globals.insert(selected_prefix + suffix);
        }
        source += package_codegen::go(*package, names.at(package), go_package_prefix,
            [&](const auto& value) { return type(value); },
            [&](const auto value) { return field(value); }, [&](const auto value) { return quote(value); },options.xml,selected_prefix);
      } else if (language == target::csharp) {
        const auto qualified_name = (options.namespace_name.empty() ? "" : options.namespace_name + ".") +
            unit + "." + names.at(package);
        source += package_codegen::csharp(*package, names.at(package), qualified_name, csharp_package_prefix,
            [&](const auto& value) { return type(value); },
            [&](const auto value) { return field(value); },
            [&](const auto value) { return quote(value); }, options.xml);
      } else {
        source += package_codegen::javascript(*package, names.at(package), javascript_package_name(globals),
            language == target::typescript, [&](const auto& value) { return type(value); },
            [&](const auto value) { return field(value); }, [&](const auto value) { return quote(value); },options.xml);
      }
    }
    if (language == target::csharp) {
      close();
      if (!options.namespace_name.empty()) {
        close();
      }
    }
    return source;
  }
};
} // namespace

// Dispatch native owning output while preserving the compiler's C++-only implementation.
std::string generate(const std::vector<std::unique_ptr<syntax_node>>& statements,
                     std::string_view language, std::string_view unit_name,
                     const portable_options& requested) {
  auto options = requested;
  options.xml = xml_binding::effective_options(statements, options.xml);
  xml_binding::validate_options(statements, options.xml, language);
  behavior::require_callable_backend(statements, language);
  require_unmanaged_backend(statements);
  auto_size::validate(statements, options.class_auto_sizes);
  comparison::validate(statements, options.class_comparisons, true);
  if (language == "c" || language == "swift" || language == "kotlin" ||
      language == "rust" || language == "python") {
    native::schema comparison_schema{statements};
    comparison_schema.class_comparisons = options.class_comparisons;
    comparison_schema.class_auto_sizes = options.class_auto_sizes;
    comparison_schema.xml = options.xml;
    comparison_schema.protocols = options.protocols;
    if (language == "c") {
      return behavior::finalize_mappings(generated_license_notice(statements) + native::c(comparison_schema) +
          (options.xml.enabled ? xml_codegen::generate_native(comparison_schema, options.xml, language) : ""));
    }
    if (language == "swift") {
      return behavior::finalize_mappings(generated_license_notice(statements) + native::swift(comparison_schema) +
          (options.xml.enabled ? xml_codegen::generate_native(comparison_schema, options.xml, language) : ""));
    }
    if (language == "kotlin") {
      return behavior::finalize_mappings(generated_license_notice(statements) +
             native::kotlin(comparison_schema, options.package_name) +
             (options.xml.enabled ? xml_codegen::generate_native(comparison_schema, options.xml, language) : ""));
    }
    if (language == "rust") {
      return behavior::finalize_mappings(generated_license_notice(statements) + native::rust(comparison_schema));
    }
    if (language == "python") {
      return behavior::finalize_mappings(generated_license_notice(statements, "#") + native::python(comparison_schema));
    }
  }
  const auto selected = language == "js"       ? target::js
                        : language == "go"     ? target::go
                        : language == "csharp" ? target::csharp
                        : language == "typescript"
                            ? target::typescript
                            : throw std::invalid_argument{"Unknown portable language"};
  return behavior::finalize_mappings(emitter{selected, options, unit_name}.generate(statements));
}

// Preserve declaration-free source notices without changing the established vector API.
std::string generate_schema(const parser::parsed_schema& schema, std::string_view language,
                            std::string_view unit_name, const portable_options& options) {
  const auto source = generate(schema.statements, language, unit_name, options);
  return schema.statements.empty()
      ? generated_source_notices(schema.source_notices, language == "python" ? "#" : "//") + source
      : source;
}
} // namespace rohit::serializer::writer::portable
