// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <core.hpp>
#include <rohit/stream_io.hpp>
#include <rohit/xml.hpp>
#include <gtest/gtest.h>

#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace xml = rohit::serializer::xml;
using model = core_model::xsd_c_t_model_139298458;

// Read the bounded authored XML fixture without making any 3MF package or geometry claims.
std::string document() {
  std::ifstream input{SERIALIZER_CORE_MODEL_FIXTURE, std::ios::binary};
  if (!input) { throw std::runtime_error{"Cannot read Core model fixture"}; }
  const auto bytes = rohit::read_stream_bytes(input, xml::limits{}.max_input_bytes);
  return {reinterpret_cast<const char*>(bytes.begin()), bytes.current_offset()};
}

// Change one deliberate token and fail if the fixture no longer contains that control.
std::string replace_one(std::string input, std::string_view before, std::string_view after) {
  const auto position = input.find(before);
  if (position == std::string::npos) { throw std::runtime_error{"Core fixture token missing"}; }
  input.replace(position, before.size(), after);
  return input;
}

// Include foreign children before, between and after known members of the resources choice.
std::string extended_document() {
  return replace_one(document(), "<resources>",
      "<resources xmlns:e=\"urn:example:extension\"><e:custom/>"
      "<basematerials id=\"2\"><base name=\"A\" displaycolor=\"#AABBCC\"/></basematerials>"
      "<e:custom/><basematerials id=\"3\"><base name=\"B\" displaycolor=\"#000000FF\"/></basematerials>"
      "<e:custom/>");
}
} // namespace

TEST(xml_core_generated, official_schema_generated_cpp_reads_and_roundtrips_authored_mesh) {
  const auto value = xml::deserialize<model>(document());
  ASSERT_EQ(value.xsd_metadata.size(), 1U);
  EXPECT_EQ(value.xsd_metadata.front().xsd_value, "Tetrahedron");
  ASSERT_EQ(value.xsd_resources.xsd_object.size(), 1U);
  const auto& object = value.xsd_resources.xsd_object.front();
  EXPECT_EQ(object.xsd_id, "1");
  ASSERT_TRUE(object.xsd_mesh);
  EXPECT_EQ(object.xsd_mesh->xsd_vertices.xsd_vertex.size(), 4U);
  EXPECT_EQ(object.xsd_mesh->xsd_triangles.xsd_triangle.size(), 4U);
  ASSERT_EQ(value.xsd_build.xsd_item.size(), 1U);
  EXPECT_EQ(value.xsd_build.xsd_item.front().xsd_objectid, "1");
  const auto copy = xml::deserialize<model>(xml::serialize(value));
  ASSERT_EQ(copy.xsd_resources.xsd_object.size(), 1U);
  EXPECT_EQ(copy.xsd_resources.xsd_object.front().xsd_id, object.xsd_id);
}

TEST(xml_core_generated, lexical_union_qname_numeric_and_matrix_rules_are_checked) {
  const auto source = document();
  EXPECT_NO_THROW((void)xml::deserialize<model>(replace_one(source, "xml:lang=\"en\"", "xml:lang=\"\"")));
  EXPECT_THROW((void)xml::deserialize<model>(replace_one(source, "xml:lang=\"en\"", "xml:lang=\"9en\"")), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize<model>(replace_one(source, "id=\"1\"", "id=\"0\"")), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize<model>(replace_one(source, "id=\"1\"", "id=\"2147483648\"")), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize<model>(replace_one(source, "x=\"0\"", "x=\"INF\"")), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize<model>(replace_one(source, "x=\"0\"", "x=\"1.\"")), std::invalid_argument);
  EXPECT_NO_THROW((void)xml::deserialize<model>(replace_one(source, "x=\"0\"", "x=\"+01\"")));
  EXPECT_THROW((void)xml::deserialize<model>(replace_one(source, "name=\"Title\"", "name=\"unbound:Title\"")), std::invalid_argument);
  EXPECT_NO_THROW((void)xml::deserialize<model>(replace_one(source, "<item objectid=\"1\"/>",
      "<item objectid=\"1\" transform=\"1 0 0 0 1 0 0 0 1 0 0 0\"/>")));
  EXPECT_THROW((void)xml::deserialize<model>(replace_one(source, "<item objectid=\"1\"/>",
      "<item objectid=\"1\" transform=\"1 0\"/>")), std::invalid_argument);
}

TEST(xml_core_generated, repeated_known_resources_and_positioned_foreign_content_are_checked) {
  const auto source = extended_document();
  const auto value = xml::deserialize<model>(source);
  EXPECT_EQ(value.xsd_resources.xsd_basematerials.size(), 2U);
  EXPECT_NO_THROW((void)xml::deserialize<model>(xml::serialize(value)));
  EXPECT_THROW((void)xml::deserialize<model>(replace_one(source,
      "displaycolor=\"#AABBCC\"", "displaycolor=\"#AB\"")), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize<model>(replace_one(source,
      "</object>", "</object><e:custom/>")), std::invalid_argument);
}
