// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/document_profile_contract.hpp"

#include <gtest/gtest.h>
#include <rohit/stream.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace writer = rohit::serializer::writer;
namespace schema = rohit::serializer;

// Existing schema package syntax compiles expected-type/vocabulary metadata without a grammar change.
TEST(document_profile_contract, package_roles_derive_typed_dispatch_and_compression_defaults) {
  constexpr std::string_view text =
      "serializer version 1.20.0; "
      "class model { xml(namespace(\"urn:profile\"), root(\"document\")); public uint32 value; } "
      "package archive { part main : model { name(\"/main.xml\"); "
      "content_type(\"application/xml\"); "
      "relationship(\"urn:main\"); compression(deflate); } "
      "part attachments : bytes { name(dynamic); content_type(dynamic); "
      "relationship(\"urn:attachment\"); "
      "source(main); occurs(0, unbounded); } }";
  const auto nodes = schema::parser::parse(rohit::make_constant_stream(text.data(), text.size()));
  const auto& package = static_cast<const schema::package_node&>(*nodes.back());
  writer::xml_options options{};
  options.enabled = true;
  const auto contract = writer::package_codegen::compile_package_profile(package, options);
  EXPECT_EQ(contract.container, schema::document::container_format::opc);
  ASSERT_EQ(contract.parts.size(), 2U);
  const auto& main = contract.parts.front();
  EXPECT_EQ(main.envelope.compression, schema::compression::format::deflate);
  EXPECT_EQ(main.part_name, std::optional<std::string>{"/main.xml"});
  EXPECT_EQ(main.content_type, std::optional<std::string>{"application/xml"});
  EXPECT_EQ(main.source, "/");
  ASSERT_TRUE(main.decoder);
  EXPECT_EQ(main.decoder->model_identity, "model");
  EXPECT_EQ(main.decoder->root, (schema::xml::name{"document", "urn:profile"}));
  EXPECT_EQ(main.decoder->vocabularies, (std::vector<std::string>{"urn:profile"}));
  EXPECT_FALSE(contract.parts.back().decoder);
  EXPECT_EQ(contract.parts.back().envelope.format, schema::document::payload_format::opaque_bytes);
  EXPECT_EQ(contract.parts.back().source, "main");
  EXPECT_EQ(contract.parts.back().min_occurs, 0U);
  EXPECT_FALSE(contract.parts.back().max_occurs);
  const auto generated = writer::package_codegen::cpp_profile_metadata(package, options);
  EXPECT_NE(generated.find("document_profiles()"), std::string::npos);
  EXPECT_NE(generated.find("urn:profile"), std::string::npos);
  EXPECT_NE(generated.find("::rohit::serializer::compression::format::deflate"), std::string::npos);
  EXPECT_NE(generated.find("::rohit::serializer::document::payload_format::opaque_bytes"),
            std::string::npos);
}
