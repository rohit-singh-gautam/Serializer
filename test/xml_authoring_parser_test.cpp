// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {
namespace serializer = rohit::serializer;

// Exercise authoring through the public parser, including ordinary name/type resolution.
auto parse_authoring(std::string_view source) {
  return serializer::parser::parse(rohit::make_constant_stream(source.data(), source.size()), true);
}

// Reserve only test-owned inputs under the operating system temporary directory.
class authoring_directory {
  std::filesystem::path root_{};
public:
  // Create an exclusive directory; an existing path is never adopted for later cleanup.
  authoring_directory() {
    static std::atomic<unsigned> next{};
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = std::filesystem::temp_directory_path() /
        ("serializer-xml-authoring-" + std::to_string(tick) + "-" + std::to_string(next++));
    if (!std::filesystem::create_directory(root_)) { throw std::runtime_error{"Cannot reserve authoring test directory"}; }
  }
  // Remove the exact exclusively reserved directory after success or failure.
  ~authoring_directory() {
    std::error_code ignored;
    std::filesystem::remove_all(root_, ignored);
  }
  // Write a fixed local fixture name and return its path for parse_file.
  std::filesystem::path write(std::string_view filename, std::string_view contents) const {
    const auto path = root_ / filename;
    std::ofstream output{path, std::ios::binary};
    output << contents;
    if (!output) { throw std::runtime_error{"Cannot write authoring test fixture"}; }
    return path;
  }
};
} // namespace

// Sparse defaults and explicit overrides retain names, IDs and public field storage.
TEST(xml_authoring_parser, namespace_class_defaults_and_field_overrides) {
  const auto nodes = parse_authoring(R"(serializer version 1.20.0;
    namespace geometry {
      xml(namespace("urn:geometry"), scalars(attribute), unknown(skip));
      class vertex {
        xml(root("point"), scalars(element), unknown(preserve));
        public double x (9);
        public double y (10) xml(attribute, name("height"), namespace("urn:axes"));
        public array double values (11) xml(element, name("value"), wrapper("values"));
      }
    })");
  ASSERT_EQ(nodes.size(), 1U);
  const auto& space = static_cast<const serializer::namespace_node&>(*nodes.front());
  EXPECT_EQ(space.xml.namespace_uri, "urn:geometry");
  EXPECT_EQ(space.xml.scalar_placement, serializer::xml_field_placement::attribute);
  EXPECT_EQ(space.xml.unknown, serializer::xml_unknown_policy::skip);
  const auto& record = static_cast<const serializer::class_node&>(*space.statements.front());
  EXPECT_EQ(record.xml.root, "point");
  EXPECT_EQ(record.xml.scalar_placement, serializer::xml_field_placement::element);
  EXPECT_EQ(record.xml.unknown, serializer::xml_unknown_policy::preserve);
  ASSERT_EQ(record.member_list.size(), 3U);
  EXPECT_EQ(record.member_list.front().name, "x");
  EXPECT_EQ(record.member_list.front().id, 9U);
  EXPECT_FALSE(record.member_list.front().xml);
  ASSERT_TRUE(record.member_list[1].xml);
  EXPECT_EQ(record.member_list[1].xml->placement, serializer::xml_field_placement::attribute);
  EXPECT_EQ(record.member_list[1].xml->name, "height");
  EXPECT_EQ(record.member_list[1].xml->namespace_uri, "urn:axes");
  ASSERT_TRUE(record.member_list[2].xml);
  EXPECT_EQ(record.member_list[2].xml->wrapper, "values");
}

// Reopened namespaces share one logical sparse configuration, including later declarations.
TEST(xml_authoring_parser, reopened_namespace_uses_canonical_scope) {
  const auto nodes = parse_authoring(R"(serializer version 1.20.0;
    namespace sample { class earlier { public int32 value (1); } }
    namespace sample { xml(namespace("urn:sample")); class later { public int32 value (1); } })");
  ASSERT_EQ(nodes.size(), 2U);
  const auto& first = static_cast<const serializer::namespace_node&>(*nodes[0]);
  const auto& second = static_cast<const serializer::namespace_node&>(*nodes[1]);
  EXPECT_EQ(first.xml.namespace_uri, "urn:sample");
  EXPECT_EQ(second.statements.front()->parent_namespace, &first);
}

// Lexical annotations select XML adapters while retaining ordinary scalar/collection storage.
TEST(xml_authoring_parser, list_and_namespace_dependent_lexical_mappings) {
  const auto nodes = parse_authoring(R"(serializer version 1.20.0;
    class model {
      public array double transform (1) xml(attribute, list);
      public string key (2) xml(attribute, qname);
      public string capabilities (3) xml(attribute, prefix_list);
    })");
  const auto& model = static_cast<const serializer::class_node&>(*nodes.front());
  ASSERT_TRUE(model.member_list[0].xml);
  EXPECT_TRUE(model.member_list[0].xml->list);
  EXPECT_EQ(model.member_list[0].modifier, serializer::member::modifier_type::array);
  EXPECT_EQ(model.member_list[0].id, 1U);
  EXPECT_TRUE(model.member_list[1].xml->qname);
  EXPECT_TRUE(model.member_list[2].xml->prefix_list);
  for (const auto metadata : {"list,list", "qname,prefix_list", "list,qname", "list,prefix_list"}) {
    EXPECT_THROW(parse_authoring("serializer version 1.20.0; class bad { public string x (1) xml(attribute," +
        std::string{metadata} + "); }"), std::exception);
  }
}

// A fixed schema path retains its declaring source instead of following the working directory.
TEST(xml_authoring_parser, fixed_schema_binding_retains_declaring_source) {
  authoring_directory files;
  const auto path = files.write("model.serializer", R"(serializer version 1.20.0;
    namespace sample { using schema "schemas/model.xsd"; class model { public int32 value (1); } })");
  const auto parsed = serializer::parser::parse_file(path);
  const auto& space = static_cast<const serializer::namespace_node&>(*parsed.statements.front());
  ASSERT_TRUE(space.xml.schema);
  EXPECT_EQ(space.xml.schema->path.generic_string(), "schemas/model.xsd");
  EXPECT_EQ(space.xml.schema->source_path, std::filesystem::canonical(path).generic_string());
}

// Every included file retains its own language contract for new XML syntax.
TEST(xml_authoring_parser, older_included_language_rejects_authoring) {
  authoring_directory files;
  files.write("older.serializer", R"(serializer version 1.19.0;
    namespace sample { xml(namespace("urn:sample")); })");
  const auto path = files.write("entry.serializer", R"(serializer version 1.20.0;
    include older;
    class record { public int32 value (1); })");
  EXPECT_THROW(serializer::parser::parse_file(path), std::exception);
}

// Unknown/conflicting mapping terms and malformed strings reject rather than inventing a contract.
TEST(xml_authoring_parser, rejects_malformed_or_ambiguous_metadata) {
  for (const auto source : {
      R"(namespace s { xml(namespace("a"), namespace("b")); })",
      R"(namespace s { xml(root("record")); })",
      R"(namespace s { xml(scalars(text)); })",
      R"(namespace s { xml(unknown(accept)); })",
      R"(namespace s { xml(); })",
      R"(namespace s { using schema ""; })",
      R"(namespace s { using schema "a.xsd"; using schema "b.xsd"; })",
      R"(class s { public int32 value (1) xml(attribute, element); })",
      R"(class s { public int32 value (1) xml(text, namespace("urn:s")); })",
      R"(class s { public int32 value (1) xml(attribute, wrapper("values")); })",
      R"(class s { public int32 value (1) xml(name("")); })",
      R"(class s { public int32 value (1) xml(name("bad\q")); })",
      R"(class s { public int32 value (1) xml(attribute) xml(attribute); })"}) {
    EXPECT_THROW(parse_authoring("serializer version 1.20.0; " + std::string{source}), std::exception) << source;
  }
}

// Existing language versions cannot opt into syntax defined only by the new contract.
TEST(xml_authoring_parser, older_contract_rejects_each_authoring_form) {
  for (const auto source : {
      R"(namespace s { xml(scalars(attribute)); })",
      R"(namespace s { using schema "model.xsd"; })",
      R"(class model { public int32 value (1) xml(attribute); })",
      R"(class model { public int32 value (1); } package archive { part main : model {
        name("/model.xml"); content_type("application/xml"); relationship("urn:main"); } })"}) {
    EXPECT_THROW(parse_authoring("serializer version 1.19.0; " + std::string{source}), std::exception) << source;
  }
}

// A complete package contract retains typed content, opaque attachments and dynamic cardinalities.
TEST(xml_authoring_parser, package_roles_are_typed_and_dynamic) {
  const auto nodes = parse_authoring(R"(serializer version 1.20.0;
    namespace sample {
      class model { public int32 value (1); }
      package archive {
        part main : model { name("/data/model.xml"); content_type("application/xml");
          relationship("urn:main"); id("model_link"); compression(deflate); }
        part images : bytes { name(dynamic); content_type(dynamic); relationship("urn:image");
          source(main); occurs(0, unbounded); }
      }
    })");
  const auto& space = static_cast<const serializer::namespace_node&>(*nodes.front());
  ASSERT_EQ(space.statements.size(), 2U);
  const auto& package = static_cast<const serializer::package_node&>(*space.statements[1]);
  ASSERT_EQ(package.parts.size(), 2U);
  ASSERT_TRUE(package.parts.front().value_type);
  EXPECT_EQ(package.parts.front().value_type->type, serializer::object_type::class_type);
  EXPECT_EQ(package.parts.front().value_type->resolved_node, space.statements.front().get());
  EXPECT_EQ(package.parts.front().part_name, "/data/model.xml");
  EXPECT_EQ(package.parts.front().compression, "deflate");
  EXPECT_EQ(package.parts.front().relationship_id, "model_link");
  EXPECT_FALSE(package.parts[1].value_type);
  EXPECT_FALSE(package.parts[1].part_name);
  EXPECT_FALSE(package.parts[1].content_type);
  EXPECT_EQ(package.parts[1].source, "main");
  EXPECT_EQ(package.parts[1].min_occurs, 0U);
  EXPECT_FALSE(package.parts[1].max_occurs);
}

// Invalid roles, numeric bounds and unsupported settings cannot survive into generated writers.
TEST(xml_authoring_parser, rejects_invalid_package_contracts) {
  constexpr std::string_view prefix = "serializer version 1.20.0; class model { public int32 value (1); } ";
  constexpr std::string_view settings = "name(\"/model.xml\"); content_type(\"application/xml\"); relationship(\"urn:main\"); ";
  for (const auto extra : {"compression(gzip);", "codec(bytes);", "source(missing);", "source(main);",
      "occurs(2,1);", "occurs(0,0);", "occurs(0,184467440737095516160);", "occurs(-1,1);",
      "name(dynamic);", "unexpected(value);"}) {
    EXPECT_THROW(parse_authoring(std::string{prefix} + "package archive { part main : model { " +
        std::string{settings} + extra + " } }"), std::exception) << extra;
  }
  EXPECT_THROW(parse_authoring(std::string{prefix} + "package empty {}"), std::exception);
  EXPECT_THROW(parse_authoring(std::string{prefix} + "package archive { part main : model {} }"), std::exception);
  EXPECT_THROW(parse_authoring(std::string{prefix} + "package archive { part main : int32 { " +
      std::string{settings} + " } }"), std::exception);
}
