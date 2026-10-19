// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/xml_model_contract.hpp"
#include "../src/xml_schema.hpp"

#include <gtest/gtest.h>
#include <rohit/stream.hpp>

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace binding = rohit::serializer::writer::xml_binding;
namespace schema = rohit::serializer;

// Use the production parser and retain its owning AST throughout contract validation.
auto parse(std::string_view text) {
  return schema::parser::parse(rohit::make_constant_stream(text.data(), text.size()));
}
// Select a top-level concrete fixture without relying on generic lowering order.
const schema::class_node& record(const auto& nodes, std::string_view name) {
  for (const auto& node : nodes) {
    if (node->name == name) {
      return static_cast<const schema::class_node&>(*node);
    }
  }
  throw std::logic_error{"Missing fixture record"};
}
} // namespace

// Different type arguments retain distinct identities even when their XML spelling is configured alike.
TEST(xml_model_contract, concrete_generic_identity_and_ambiguous_roots) {
  const auto nodes =
      parse("serializer version 1; class box<T> { public T value; } "
            "instantiate integer_box = box<uint32>; instantiate text_box = box<string>;");
  const std::array models{binding::bind_model_contract(record(nodes, "integer_box")),
                          binding::bind_model_contract(record(nodes, "text_box"))};
  EXPECT_TRUE(models[0].concrete_generic);
  EXPECT_NE(models[0].concrete_identity, models[1].concrete_identity);
  const std::array<schema::xml::name, 2> roots{
      {{"integer", "urn:generic"}, {"text", "urn:generic"}}};
  EXPECT_NO_THROW(binding::validate_model_names(models, roots));
  const std::array<schema::xml::name, 2> ambiguous{
      {{"box", "urn:generic"}, {"box", "urn:generic"}}};
  EXPECT_THROW(binding::validate_model_names(models, ambiguous), std::invalid_argument);
  EXPECT_THROW((void)binding::bind_model_contract(record(nodes, "box")), std::invalid_argument);
}

// Base declaration order, then local declaration order, defines the single flattened sequence.
TEST(xml_model_contract, inherited_fields_are_base_first_and_collisions_reject) {
  const auto nodes = parse("serializer version 1; class first { public uint32 a; } "
                           "class second { public string b; } class root : public first, public "
                           "second { public bool c; }");
  const auto contract = binding::bind_model_contract(record(nodes, "root"));
  ASSERT_EQ(contract.flattened_fields.size(), 3U);
  EXPECT_EQ(contract.flattened_fields[0]->name, "a");
  EXPECT_EQ(contract.flattened_fields[1]->name, "b");
  EXPECT_EQ(contract.flattened_fields[2]->name, "c");
  const auto collisions =
      parse("serializer version 1; class a { public uint32 value; } "
            "class b { public string value; } class root : public a, public b { }");
  EXPECT_THROW((void)binding::bind_model_contract(record(collisions, "root")),
               std::invalid_argument);
}

// Repeated base occurrences and cycles have no implicit identity or flattening merge rule.
TEST(xml_model_contract, diamonds_and_unresolved_bases_reject) {
  const auto nodes =
      parse("serializer version 1; class a { public uint32 value; } "
            "class b : public a { } class c : public a { } class root : public b, public c { }");
  EXPECT_THROW((void)binding::bind_model_contract(record(nodes, "root")), std::invalid_argument);
  schema::class_node unresolved{
      schema::object_type::class_type, "root", nullptr, schema::class_attributes::none, {}};
  unresolved.parents.push_back({});
  EXPECT_THROW((void)binding::bind_model_contract(unresolved), std::invalid_argument);
}

// Historical layouts include retired fields only below retirement and new fields from creation onward.
TEST(xml_model_contract, revisions_and_magic_select_exact_layouts) {
  const auto nodes = parse("serializer version 1; class root { public magic { 'SRL' }; "
                           "public version { 3 } compatibility { 1 }; public uint32 kept; "
                           "obsolete(2) public string old; created(2) public string current; }");
  const auto contract = binding::bind_model_contract(record(nodes, "root"));
  ASSERT_TRUE(contract.magic);
  EXPECT_EQ(*contract.magic, "53524c");
  const auto old = binding::select_model_layout(contract, "1", *contract.magic);
  const auto current = binding::select_model_layout(contract, "3", *contract.magic);
  ASSERT_EQ(old.size(), 2U);
  EXPECT_EQ(old.back()->name, "old");
  EXPECT_EQ(current.back()->name, "current");
  EXPECT_THROW((void)binding::select_model_layout(contract, "0", *contract.magic),
               std::invalid_argument);
  EXPECT_THROW((void)binding::select_model_layout(contract, "4", *contract.magic),
               std::invalid_argument);
  EXPECT_THROW((void)binding::select_model_layout(contract, "3"), std::invalid_argument);
  EXPECT_THROW((void)binding::select_model_layout(contract, "3", "53524D"), std::invalid_argument);
}

// Actual XML metadata uses reserved expanded identities; missing/duplicate/wrong-scope data rejects.
TEST(xml_model_contract, xml_root_metadata_selects_historical_and_current_layouts) {
  const auto nodes = parse("serializer version 1; class root { public magic { 'SRL' }; "
                           "public version { 3 } compatibility { 1 }; obsolete(2) public string "
                           "old; created(2) public string current; }");
  const auto contract = binding::bind_model_contract(record(nodes, "root"));
  schema::xml::element root{};
  root.qualified_name = {"root", "urn:document"};
  root.attributes = binding::model_metadata_attributes(contract, "1");
  EXPECT_EQ(binding::select_model_layout(contract, root).front()->name, "old");
  root.attributes = binding::model_metadata_attributes(contract);
  ASSERT_EQ(root.attributes.size(), 2U);
  EXPECT_EQ(root.attributes.front().value, "3");
  EXPECT_EQ(binding::select_model_layout(contract, root).front()->name, "current");
  root.attributes.push_back(root.attributes.front());
  EXPECT_THROW((void)binding::select_model_layout(contract, root), std::invalid_argument);
  root.attributes.pop_back();
  root.attributes.front().qualified_name.namespace_uri = "urn:wrong";
  EXPECT_THROW((void)binding::select_model_layout(contract, root), std::invalid_argument);
  root.attributes = binding::model_metadata_attributes(contract);
  root.attributes.push_back({{"unknown", std::string{binding::model_metadata_namespace}}, "value"});
  EXPECT_THROW((void)binding::select_model_layout(contract, root), std::invalid_argument);
}

// Dotted revision XML values contain ordinary unquoted text and compare components numerically.
TEST(xml_model_contract, dotted_revision_and_unversioned_metadata) {
  const auto nodes =
      parse("serializer version 1; class root { "
            "public version version3 { \"1.10.0\" } compatibility { \"1.2.0\" }; "
            "created(\"1.3.0\") public string value; } class plain { public bool flag; }");
  const auto contract = binding::bind_model_contract(record(nodes, "root"));
  EXPECT_TRUE(binding::select_model_layout(contract, "1.2.0").empty());
  EXPECT_EQ(binding::select_model_layout(contract, "1.10.0").size(), 1U);
  EXPECT_EQ(binding::model_metadata_attributes(contract).front().value, "1.10.0");
  EXPECT_THROW((void)binding::select_model_layout(contract, "1.11.0"), std::invalid_argument);
  const auto plain = binding::bind_model_contract(record(nodes, "plain"));
  EXPECT_THROW((void)binding::select_model_layout(plain, "1"), std::invalid_argument);
}

// XML digests expose bytes, not hash computation or native container layout; malformed input is local.
TEST(xml_model_contract, digest_extent_hexadecimal_and_resource_checks) {
  const auto nodes = parse("serializer version 1.3.0; class root { public digest[2] bytes; }");
  const auto contract = binding::bind_model_contract(record(nodes, "root"));
  EXPECT_TRUE(contract.digest);
  const auto& type = contract.flattened_fields.front()->type_name_list.front();
  EXPECT_EQ(binding::decode_model_digest(type, "00fF"), (std::vector<std::uint8_t>{0U, 255U}));
  EXPECT_THROW((void)binding::decode_model_digest(type, "00"), std::invalid_argument);
  EXPECT_THROW((void)binding::decode_model_digest(type, "00zz"), std::invalid_argument);
  EXPECT_THROW((void)binding::decode_model_digest(type, "00ff", 1U), std::invalid_argument);
  auto inconsistent = type;
  inconsistent.digest = rohit::digest_algorithm::sha256;
  EXPECT_THROW((void)binding::decode_model_digest(inconsistent, "00ff"), std::invalid_argument);
  schema::xml::operation_context cancelled{};
  cancelled.is_cancelled = [] { return true; };
  EXPECT_THROW((void)binding::decode_model_digest(type, "00ff", 2U, &cancelled), std::exception);
}

// Native managed state is represented by data values only; views cannot acquire owning decoded data.
TEST(xml_model_contract, managed_snapshot_and_borrowed_direction_are_explicit) {
  const auto nodes = parse("serializer version 1; class root managed { public uint32 value; } "
                           "class output_view view readonly { public string label; }");
  const auto managed = binding::bind_model_contract(record(nodes, "root"));
  EXPECT_EQ(managed.projection, binding::projection_kind::managed_snapshot);
  EXPECT_NO_THROW(binding::validate_model_direction(managed, true));
  const auto view = binding::bind_model_contract(record(nodes, "output_view"));
  EXPECT_EQ(view.projection, binding::projection_kind::borrowed_output);
  EXPECT_NO_THROW(binding::validate_model_direction(view, false));
  EXPECT_THROW(binding::validate_model_direction(view, true), std::invalid_argument);
}

// Production binding retains dotted revision metadata while keeping the discriminator out of fields.
TEST(xml_model_contract, binder_retains_lifecycle_metadata_and_gates_generated_backends) {
  if (!schema::xml::available()) {
    GTEST_SKIP() << "Authoritative XML binding requires the XML provider";
  }
  const auto nodes =
      parse("serializer version 1; class root { public magic { 'SRL' }; "
            "public version version3 { \"1.10.0\" } compatibility { \"1.2.0\" }; "
            "obsolete(\"1.3.0\") public string old; created(\"1.3.0\") public string current; }");
  schema::writer::xml_options options{};
  options.enabled = true;
  const auto contract = binding::schema{options}.bind(record(nodes, "root"));
  EXPECT_TRUE(contract.model.advanced);
  ASSERT_EQ(contract.fields.size(), 2U);
  EXPECT_FALSE(contract.fields.front().source->version);
  const auto attributes = binding::model_metadata_attributes(contract.model);
  EXPECT_EQ(attributes.front().value, "1.10.0");
  EXPECT_THROW(binding::validate_backend(contract, "cpp"), std::invalid_argument);
}

// Concrete generic/inherited bindings reach the shared IR, and reserved metadata cannot be shadowed.
TEST(xml_model_contract, binder_generic_inheritance_and_reserved_name_checks) {
  if (!schema::xml::available()) {
    GTEST_SKIP() << "Authoritative XML binding requires the XML provider";
  }
  const auto nodes =
      parse("serializer version 1.20.0; class base { public uint32 first; } "
            "class box<T> : public base { public T second; } instantiate root = box<string>;");
  schema::writer::xml_options options{};
  options.enabled = true;
  const auto contract = binding::schema{options}.bind(record(nodes, "root"));
  EXPECT_TRUE(contract.model.concrete_generic);
  EXPECT_TRUE(contract.model.inherited);
  ASSERT_EQ(contract.fields.size(), 2U);
  EXPECT_EQ(contract.fields.front().source->name, "first");
  const auto legacy = parse("serializer version 1.20.0; class root { "
                            "public string field xml(attribute, name(\"revision\"), "
                            "namespace(\"urn:serializer:xml:model\")); }");
  EXPECT_NO_THROW((void)binding::schema{options}.bind(record(legacy, "root")));
  const auto collision = parse("serializer version 1.20.0; class root { public version { 1 }; "
                               "public string field xml(attribute, name(\"revision\"), "
                               "namespace(\"urn:serializer:xml:model\")); }");
  EXPECT_THROW((void)binding::schema{options}.bind(record(collision, "root")),
               std::invalid_argument);
}
