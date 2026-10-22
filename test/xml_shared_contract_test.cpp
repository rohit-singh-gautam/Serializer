// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/xml_schema.hpp"
#include <rohit/output_options.hpp>
#include <rohit/stream.hpp>
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace binding = rohit::serializer::writer::xml_binding;
namespace writer = rohit::serializer::writer;

// Reserve unique temporary INI storage and remove only that fixture's directory.
class configuration_file {
  std::filesystem::path directory_{};
public:
  // Write the common XML defaults and the test's explicit contract selections.
  explicit configuration_file(std::string_view body) {
    static std::atomic<unsigned> sequence{};
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    directory_ = std::filesystem::temp_directory_path() /
        ("serializer-xml-contract-" + std::to_string(tick) + "-" + std::to_string(sequence++));
    if (!std::filesystem::create_directory(directory_)) { throw std::runtime_error{"Cannot reserve fixture"}; }
    std::ofstream output{directory_ / "contract.ini", std::ios::binary};
    output << "[xml]\nenabled=true\n[cpp]\nformat=false\n[xml.class.root]\nnamespace=urn:root\n" << body;
    if (!output) { throw std::runtime_error{"Cannot write fixture"}; }
  }
  // Delete the exact directory created by this object, including failure fixtures.
  ~configuration_file() { std::error_code ignored{}; std::filesystem::remove_all(directory_, ignored); }
  // Read through the production strict configuration parser.
  writer::output_options read() const { return writer::read_output_options(directory_ / "contract.ini"); }
};

// Parse using the normal versioned schema path before binding a shared contract.
auto parse(std::string_view text) {
  return rohit::serializer::parser::parse(rohit::make_constant_stream(text.data(), text.size()));
}

// Fixture dependencies precede the last top-level root record.
const rohit::serializer::class_node& root_at(const auto& statements) {
  return static_cast<const rohit::serializer::class_node&>(*statements.back());
}
} // namespace

// Native optional layers retain absent/nil/value distinctions, including ordinary empty values.
TEST(xml_shared_contract, explicit_presence_configuration) {
  const auto model = parse("serializer version 1.7.0; class root { public optional<optional<string>> value; }");
  configuration_file file{"presence.value=absent_or_nil\n"};
  const auto options = file.read();
  const auto contract = binding::schema{options.cpp.xml}.bind(root_at(model));
  ASSERT_EQ(contract.fields.size(), 1U);
  const auto& presence = contract.fields.front().presence;
  EXPECT_EQ(presence.optional_depth, 2U);
  EXPECT_EQ(binding::classify_presence(presence, nullptr), binding::presence_state::absent);
  rohit::serializer::xml::element empty{};
  EXPECT_EQ(binding::classify_presence(presence, &empty), binding::presence_state::value);
  empty.attributes.push_back({{"nil", "http://www.w3.org/2001/XMLSchema-instance"}, "true"});
  EXPECT_EQ(binding::classify_presence(presence, &empty), binding::presence_state::nil);
  EXPECT_NO_THROW((void)writer::cpp::generate(model, options.cpp));
}

TEST(xml_shared_contract, cpp_xml_helpers_reject_collisions_without_reserving_native_schema_names) {
  writer::cpp_options native{}; native.format = false;
  auto enabled = native; enabled.xml.enabled = true;
  for (const auto source : {
      "serializer version 1.20.0; class root { public uint32 value; public uint32 srl_xml_field_value; }",
      "serializer version 1.20.0; class root { public uint32 value; public function uint32 srl_xml_field_value() readonly expression(1); }",
      "serializer version 1.20.0; class base { public function uint32 srl_xml_field_value() readonly expression(1); } class root : public base { public uint32 value; }",
      "serializer version 1.20.0; class box<T> { public T value; public uint32 srl_owner_type; } instantiate root = box<uint32>;",
      "serializer version 1.20.0; class box<srl_owner_type> { public srl_owner_type value; } instantiate root = box<uint32>;",
      "serializer version 1.20.0; class box<node> { public node value; } instantiate root = box<uint32>;"}) {
    const auto model = parse(source);
    EXPECT_NO_THROW((void)writer::cpp::generate(model, native)) << source;
    EXPECT_THROW((void)writer::cpp::generate(model, enabled), std::invalid_argument) << source;
  }
  EXPECT_THROW((void)parse("serializer version 1.20.0; class root { public uint32 value; public factory srl_xml_field_value() {} }"), std::exception);
  const auto renamed = parse("serializer version 1.20.0; class box<SrlReadOnly> { public SrlReadOnly value; } instantiate root = box<uint32>;");
  EXPECT_NO_THROW((void)writer::cpp::generate(renamed, enabled));
}

// Invalid states, orphan keys and misspelled selectors fail at production parser/binder boundaries.
TEST(xml_shared_contract, ambiguous_or_unused_configuration_rejects) {
  const auto model = parse("serializer version 1.7.0; class root { public optional<string> value; }");
  for (const auto text : {"presence.value=absent_or_nil\n", "presence.missing=absent\n",
      "presence.value=nil\nattributes=value\n", "map.value.key=key\n",
      "variant.value.tag.x=x\n", "mixed.child.node=node\n"}) {
    configuration_file file{text};
    const auto options = file.read();
    EXPECT_THROW((void)binding::schema{options.cpp.xml}.bind(root_at(model)), std::invalid_argument) << text;
  }
  for (const auto text : {"presence.value=empty\n", "map.value.unknown=x\n",
      "variant.value.mode=element\n", "extension.slot.policy=forgive\n"}) {
    configuration_file file{text};
    EXPECT_THROW((void)file.read(), std::invalid_argument) << text;
  }
  const auto array_model = parse("serializer version 1; class root { public array string values; }");
  configuration_file bad_uri{"wrapper.values={urn:bad space}items\n"};
  const auto uri_options = bad_uri.read();
  EXPECT_THROW((void)binding::schema{uri_options.cpp.xml}.bind(root_at(array_model)), std::invalid_argument);
  const auto generic_model = parse("serializer version 1; class root<T> { public T value; }");
  configuration_file generic{"presence.missing=absent\n"};
  const auto generic_options = generic.read();
  EXPECT_THROW(binding::validate_options(generic_model, generic_options.cpp.xml, "python"),
      std::invalid_argument);
}

// Required wrappers carry explicit entry/key/value names and scalar attribute placement.
TEST(xml_shared_contract, map_configuration) {
  const auto model = parse("serializer version 1; class root { public map(uint32) string values; }");
  configuration_file file{"wrapper.values=entries\nmap.values=entry\nmap.values.key=id\n"
      "map.values.value=data\nmap.values.key_placement=attribute\n"};
  const auto options = file.read();
  const auto contract = binding::schema{options.cpp.xml}.bind(root_at(model));
  ASSERT_TRUE(contract.fields.front().map_binding);
  const auto& map = *contract.fields.front().map_binding;
  EXPECT_EQ(map.wrapper.local, "entries");
  EXPECT_EQ(map.entry.namespace_uri, "urn:root");
  EXPECT_TRUE(map.key.namespace_uri.empty());
  EXPECT_EQ(map.value.local, "data");
  EXPECT_NO_THROW(binding::validate_backend(contract, "cpp"));
  EXPECT_THROW((void)binding::schema{options.cpp.xml}.export_xsd({&root_at(model)}), std::invalid_argument);
  configuration_file renamed{"wrapper.values=entries\nmap.values=entry\nname.values=unused\n"};
  const auto renamed_options = renamed.read();
  EXPECT_THROW((void)binding::schema{renamed_options.cpp.xml}.bind(root_at(model)), std::invalid_argument);
  const auto qname_model = parse("serializer version 1.20.0; class root { "
      "public map(string) string values xml(qname); }");
  EXPECT_THROW((void)binding::schema{options.cpp.xml}.bind(root_at(qname_model)), std::invalid_argument);
}

// Conflicting expanded tags reject in actual generation before returning an artifact.
TEST(xml_shared_contract, variant_tag_generation_conflicts) {
  const auto model = parse("serializer version 1.4.0; class root { public variant(uint32=code,string=text) value; }");
  configuration_file invalid{"wrapper.value=choice\nvariant.value=element\n"
      "variant.value.tag.code={urn:tag}same\nvariant.value.tag.text={urn:tag}same\n"};
  const auto bad = invalid.read();
  EXPECT_THROW((void)writer::cpp::generate(model, bad.cpp), std::invalid_argument);
  configuration_file valid{"wrapper.value=choice\nvariant.value=element\n"
      "variant.value.tag.code={urn:tag}number\nvariant.value.tag.text={urn:tag}text\n"};
  const auto options = valid.read();
  const auto contract = binding::schema{options.cpp.xml}.bind(root_at(model));
  ASSERT_TRUE(contract.fields.front().variant_binding);
  EXPECT_EQ(contract.fields.front().variant_binding->alternatives[0].tag.local, "number");
  EXPECT_NO_THROW((void)writer::cpp::generate(model, options.cpp));
  configuration_file type_only{"wrapper.value=choice\nvariant.value=element\n"
      "variant.value.type.code={urn:types}Code\n"};
  const auto type_options = type_only.read();
  const auto type_contract = binding::schema{type_options.cpp.xml}.bind(root_at(model));
  const auto& alternative = type_contract.fields.front().variant_binding->alternatives.front();
  EXPECT_EQ(alternative.tag, (rohit::serializer::xml::name{"code", "urn:root"}));
  EXPECT_EQ(alternative.type_identity, (rohit::serializer::xml::name{"Code", "urn:types"}));
  configuration_file renamed{"wrapper.value=choice\nvariant.value=element\nname.value=unused\n"};
  const auto renamed_options = renamed.read();
  EXPECT_THROW((void)binding::schema{renamed_options.cpp.xml}.bind(root_at(model)), std::invalid_argument);
}

// One ordered native variant sequence owns all text/child positions.
TEST(xml_shared_contract, ordered_mixed_configuration) {
  const auto model = parse("serializer version 1.4.0; class child { public uint32 id; } "
      "class fragment { public variant(string=text,child=node) value; } "
      "class root { public array fragment content; }");
  configuration_file file{"mixed=content\nmixed.variant=value\nmixed.text=text\n"
      "mixed.child.node={urn:child}item\n"};
  const auto options = file.read();
  const auto contract = binding::schema{options.cpp.xml}.bind(root_at(model));
  ASSERT_TRUE(contract.mixed);
  EXPECT_TRUE(contract.fields.empty());
  EXPECT_EQ(contract.mixed->children.front().qualified_name.local, "item");
  EXPECT_NO_THROW(binding::validate_backend(contract, "cpp"));
}

// Every owning backend shares the element-only contract without adding a text branch.
TEST(xml_shared_contract, ordered_element_only_configuration) {
  const auto model = parse("serializer version 1.21.0; class first { public uint32 id; } "
      "class second { public uint32 id; } "
      "class fragment { private variant(first=one,second=two) payload; } "
      "class root { public array fragment content; }");
  configuration_file file{"ordered=content\nordered.variant=payload\nordered.child.one={urn:child}item\n"};
  const auto options = file.read();
  const auto contract = binding::schema{options.cpp.xml}.bind(root_at(model));
  ASSERT_TRUE(contract.mixed);
  EXPECT_TRUE(contract.mixed->element_only);
  EXPECT_EQ(contract.mixed->text_alternative, contract.mixed->variant->type_name_list.size());
  EXPECT_EQ(contract.mixed->children.size(), 2U);
  EXPECT_EQ(contract.mixed->children.front().qualified_name, (rohit::serializer::xml::name{"item", "urn:child"}));
  EXPECT_NO_THROW((void)writer::cpp::generate(model, options.cpp));
  for (const auto language : {"python", "js", "typescript", "go", "java", "csharp", "kotlin", "swift", "c", "rust"}) {
    EXPECT_NO_THROW(binding::validate_backend(contract, language)) << language;
  }
  EXPECT_THROW(binding::validate_backend(contract, "xsd"), std::invalid_argument);
}

// Explicit selection never changes legacy mixed semantics or accepts a partially configured mapping.
TEST(xml_shared_contract, ordered_configuration_rejects_ambiguous_or_incompatible_shapes) {
  const auto model = parse("serializer version 1.21.0; class first { public uint32 id; } "
      "class second { public uint32 id; } "
      "class fragment { private variant(first=one,second=two) value; } "
      "class root { public array fragment content; }");
  for (const auto body : {"mixed=content\n", "mixed=content\nordered=content\n", "ordered.child.one=item\n",
      "ordered=content\nmixed.child.one=item\n", "ordered=content\nmixed.text=text\n",
      "ordered=content\nordered.child.missing=item\n", "ordered=content\nordered.child.one=item\nordered.child.two=item\n"}) {
    configuration_file file{body};
    const auto options = file.read();
    EXPECT_THROW((void)binding::schema{options.cpp.xml}.bind(root_at(model)), std::invalid_argument) << body;
  }
  const auto text_model = parse("serializer version 1.21.0; class first { public uint32 id; } "
      "class fragment { private variant(string=text,first=one) value; } "
      "class root { public array fragment content; }");
  configuration_file file{"ordered=content\n"};
  const auto options = file.read();
  EXPECT_THROW((void)binding::schema{options.cpp.xml}.bind(root_at(text_model)), std::invalid_argument);
}

// Vocabulary capability and exact wildcard positions survive strict configuration binding.
TEST(xml_shared_contract, positioned_extension_configuration) {
  const auto model = parse("serializer version 1; class root { public uint32 known; }");
  configuration_file file{"extension.foreign.kind=element\nextension.foreign.before=known\n"
      "extension.foreign.namespaces=urn:extension urn:other\nextension.foreign.policy=preserve\n"
      "extension.foreign.minimum=0\nextension.foreign.maximum=2\nextensions.required=urn:extension\n"};
  const auto options = file.read();
  const auto contract = binding::schema{options.cpp.xml}.bind(root_at(model));
  ASSERT_TRUE(contract.extensions);
  ASSERT_EQ(contract.extensions->slots.size(), 1U);
  EXPECT_EQ(contract.extensions->slots.front().before_field, 0U);
  EXPECT_EQ(contract.extensions->required_namespaces.front(), "urn:extension");
  EXPECT_NO_THROW(binding::validate_backend(contract, "cpp"));
  EXPECT_THROW((void)binding::compare_contracts(contract, contract), std::invalid_argument);
}

// Two native optional layers preserve absent/nil/empty collection states at an explicit wrapper.
TEST(xml_shared_contract, nested_optional_collection_has_consistent_presence_and_item_metadata) {
  const auto model = parse("serializer version 1.7.0; class root { "
      "public optional<optional<array<string>>> values; }");
  configuration_file file{"presence.values=absent_or_nil\nwrapper.values=entries\n"};
  const auto options = file.read();
  const auto contract = binding::schema{options.cpp.xml}.bind(root_at(model));
  ASSERT_EQ(contract.fields.size(), 1U);
  const auto& field = contract.fields.front();
  EXPECT_EQ(field.presence.optional_depth, 2U);
  EXPECT_TRUE(field.presence.collection);
  EXPECT_TRUE(field.presence.wrapper);
  EXPECT_TRUE(field.presence.allows_absent);
  EXPECT_TRUE(field.presence.allows_nil);
  EXPECT_TRUE(field.repeated);
  EXPECT_EQ(field.min_occurs, 0U);
  EXPECT_FALSE(field.max_occurs.has_value());
  ASSERT_TRUE(field.wrapper);
  EXPECT_EQ(field.wrapper->local, "entries");
  rohit::serializer::xml::element empty_wrapper{};
  EXPECT_EQ(binding::classify_presence(field.presence, nullptr), binding::presence_state::absent);
  EXPECT_EQ(binding::classify_presence(field.presence, &empty_wrapper), binding::presence_state::value);
  empty_wrapper.attributes.push_back({{"nil", "http://www.w3.org/2001/XMLSchema-instance"}, "true"});
  EXPECT_EQ(binding::classify_presence(field.presence, &empty_wrapper), binding::presence_state::nil);
  EXPECT_NO_THROW(binding::validate_backend(contract, "cpp"));
}
