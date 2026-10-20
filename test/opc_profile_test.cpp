// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <opc_profile.hpp>
#include <rohit/package.hpp>
#include <rohit/document_profile.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {
namespace package = rohit::serializer::package;

// Create a small model whose placement is derived entirely from inline defaults.
profile::model fixture_model() {
  profile::model value{};
  value.name = "sample & geometry";
  profile::point point{};
  point.x = 1.0;
  point.y = 2.0;
  point.z = 3.0;
  value.points.push_back(point);
  return value;
}

// Author complete typed-part inputs independently of the generated package composition logic.
std::string collection_input(std::size_t invalid_index) {
  std::ostringstream output;
  package::opc_writer writer{output};
  const std::array labels{"item0", "item1", "item2"};
  for (std::size_t index{}; index < labels.size(); ++index) {
    const auto path = std::string{"/Models/model"} + std::to_string(index) + ".xml";
    const auto scalar = index == invalid_index ? "invalid-double" : "1";
    const auto xml = std::string{"<model xmlns='urn:serializer:package-test' name='"} +
                     labels[index] + "'><point x='" + scalar + "' y='2' z='3'/></model>";
    writer.add_part(path, "application/vnd.serializer.test-model+xml",
                    {reinterpret_cast<const std::uint8_t*>(xml.data()), xml.size()});
    writer.add_relationship(
        {"/", "record" + std::to_string(index), "urn:serializer:collection-model", path});
  }
  writer.finish();
  return output.str();
}
} // namespace

// Generated role metadata is an owning inventory usable before any payload decoder is invoked.
TEST(opc_profile, generated_profile_metadata_matches_typed_roots_and_defaults) {
  namespace document = rohit::serializer::document;
  const auto profiles = profile::archive::document_profiles();
  ASSERT_EQ(profiles.size(), 2U);
  const auto& main = profiles.front();
  EXPECT_EQ(main.role, "main");
  EXPECT_EQ(main.part_name, std::optional<std::string>{"/3D/model.xml"});
  EXPECT_EQ(main.content_type, std::optional<std::string>{"application/vnd.serializer.test-model+xml"});
  EXPECT_EQ(main.envelope.compression, rohit::serializer::compression::format::deflate);
  ASSERT_TRUE(main.decoder);
  EXPECT_EQ(main.decoder->model_identity, "profile::model");
  EXPECT_EQ(main.decoder->root, (rohit::serializer::xml::name{"model", "urn:serializer:package-test"}));
  const std::array inventory{*main.decoder};
  EXPECT_EQ(document::select_decoder(inventory, "profile::model", main.decoder->root, {},
      main.decoder->vocabularies), &inventory.front());
  EXPECT_EQ(document::select_decoder(inventory, "profile::point", main.decoder->root, {},
      main.decoder->vocabularies), nullptr);
  EXPECT_FALSE(profiles.back().decoder);
  EXPECT_EQ(profiles.back().envelope.format, document::payload_format::opaque_bytes);
  EXPECT_EQ(profiles.back().source, "main");
  EXPECT_EQ(profiles.back().min_occurs, 0U);
  EXPECT_FALSE(profiles.back().max_occurs);
}

// A pre-existing package name that matches the additive metadata factory retains valid Save/load APIs.
TEST(opc_profile, metadata_factory_avoids_package_constructor_name_collision) {
  const auto metadata = profile::document_profiles::document_profiles_metadata();
  ASSERT_EQ(metadata.size(), 1U);
  EXPECT_EQ(metadata.front().role, "main");
  const auto model = fixture_model();
  std::ostringstream output{std::ios::binary};
  profile::document_profiles::write(output, {&model});
  std::istringstream source{output.str(), std::ios::binary};
  const auto decoded = profile::document_profiles::load(source);
  EXPECT_EQ(decoded.value.main.name, model.name);
}

// The generated facade owns complete assembly, relationship discovery and typed decoding.
TEST(opc_profile, complete_package_with_dynamic_attachments_round_trips) {
  const auto model = fixture_model();
  constexpr std::array<std::uint8_t, 4U> pixels{0U, 1U, 254U, 255U};
  const std::array attachments{
      package::binary_part_view{pixels, "/Textures/a.bin", "application/octet-stream", 0U}};
  const profile::archive::input input{&model, attachments};
  std::ostringstream output{std::ios::binary};
  profile::archive::write(output, input);
  std::istringstream source{output.str(), std::ios::binary};
  const auto decoded = profile::archive::load(source);
  EXPECT_EQ(decoded.value.main.name, model.name);
  ASSERT_EQ(decoded.value.main.points.size(), 1U);
  EXPECT_EQ(decoded.value.main.points.front().z, 3.0);
  ASSERT_EQ(decoded.value.attachments.size(), 1U);
  EXPECT_EQ(decoded.value.attachments.front().metadata.name, "/Textures/a.bin");
  EXPECT_EQ(decoded.value.attachments.front().bytes,
      (std::vector<std::uint8_t>{pixels.begin(), pixels.end()}));
}

// Invalid role cardinality and unbound parents reject before a complete output is exposed.
TEST(opc_profile, required_model_and_source_occurrences_are_checked) {
  std::ostringstream output{std::ios::binary};
  EXPECT_THROW(profile::archive::write(output, {}), package::error);
  EXPECT_TRUE(output.str().empty());
  const auto model = fixture_model();
  const std::array attachments{
      package::binary_part_view{{}, "/bad.bin", "application/octet-stream", 1U}};
  EXPECT_THROW(profile::archive::write(output, {&model, attachments}), package::error);
}

// XML, ZIP and metadata all charge the caller's single cumulative operation budget.
TEST(opc_profile, shared_budget_and_deadline_propagate_to_generated_xml) {
  const auto model = fixture_model();
  package::limits limits{};
  limits.max_decoder_allocation_bytes = 1U;
  auto shared = std::make_shared<package::budget>(limits);
  std::ostringstream output{std::ios::binary};
  EXPECT_THROW(profile::archive::write(output, {&model, {}}, shared), package::error);
  limits = {};
  limits.deadline = std::chrono::steady_clock::now();
  shared = std::make_shared<package::budget>(limits);
  EXPECT_THROW(profile::archive::write(output, {&model, {}}, shared), package::error);
}

// First/middle/last typed decoding failures preserve the prior aggregate and skip domain publication.
TEST(opc_profile, multi_part_codec_failure_preserves_complete_destination) {
  const auto metadata = profile::collection_archive::document_profiles();
  const auto model_count = metadata.front().max_occurs.value();
  std::istringstream valid{collection_input(model_count)};
  auto destination = profile::collection_archive::load(valid);
  ASSERT_EQ(destination.value.models.size(), model_count);
  EXPECT_EQ(destination.value.models.front().value.name, "item0");
  for (std::size_t invalid_index{}; invalid_index < model_count; ++invalid_index) {
    SCOPED_TRACE(invalid_index);
    std::istringstream invalid{collection_input(invalid_index)};
    std::size_t validations{};
    try {
      destination = profile::collection_archive::load(invalid, {}, {},
                                                      [&](const auto&, auto&) { ++validations; });
      FAIL() << "Expected generated typed-part decoding failure";
    } catch (const package::error& error) {
      EXPECT_EQ(error.code, package::error_code::codec);
      EXPECT_EQ(error.part, "/Models/model" + std::to_string(invalid_index) + ".xml");
      EXPECT_EQ(error.relationship_id, "record" + std::to_string(invalid_index));
    }
    EXPECT_EQ(validations, 0U);
    ASSERT_EQ(destination.value.models.size(), model_count);
    EXPECT_EQ(destination.value.models.front().value.name, "item0");
    EXPECT_EQ(destination.value.models.back().value.name, "item2");
  }
}
