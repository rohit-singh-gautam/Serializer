// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <opc_profile.hpp>
#include <opc_identity.hpp>
#include <rohit/package.hpp>
#include <rohit/document_profile.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
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

// Keep each owner independently valid while repeating equal scoped keys across typed parts.
identity_xml::registry identity_model() {
  identity_xml::registry result{};
  identity_xml::request request{};
  request.id = 7U;
  result.requests.push_back(request);
  result.requests.push_back(request);
  identity_xml::entry entry{};
  entry.id = 7U;
  entry.label = "label";
  result.entries.push_back(entry);
  return result;
}

// Author three typed identity parts independently of the generated profile's encode path.
std::string identity_input() {
  std::ostringstream output;
  package::opc_writer writer{output};
  constexpr std::string_view payload = "<registry xmlns='urn:serializer:identity-test'>"
      "<ref id='0007'/><ref id='7'/><entry id='7' label='label'/></registry>";
  for (std::size_t index{}; index < 3U; ++index) {
    const auto path = "/Registries/registry" + std::to_string(index) + ".xml";
    writer.add_part(path, "application/vnd.serializer.identity+xml",
        {reinterpret_cast<const std::uint8_t*>(payload.data()), payload.size()});
    writer.add_relationship({"/", "record" + std::to_string(index),
        "urn:serializer:identity-registry", path});
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

// Fixed roles remember discovered names so preserved links can follow the canonical writer path.
TEST(opc_profile, preserved_links_follow_fixed_role_relocation) {
  const auto model = fixture_model();
  constexpr std::array<std::uint8_t, 3> payload{1U, 2U, 3U};
  std::ostringstream original;
  package::opc_writer writer{original};
  package::write_xml_part(writer, "/Alternate/root.xml", "application/vnd.serializer.test-model+xml", model);
  writer.add_part("/Extensions/vendor.bin", "application/octet-stream", payload);
  writer.add_relationship({"/", "main", "urn:serializer:main-model", "Alternate/root.xml"});
  writer.add_relationship({"/", "alias", "urn:vendor:alias", "alternate/ROOT.xml?view=1#selected"});
  writer.add_relationship({"/Alternate/root.xml", "outbound", "urn:vendor:outbound", "../Extensions/vendor.bin#payload"});
  writer.add_relationship({"/Alternate/root.xml", "self", "urn:vendor:self", "?view=2#self"});
  writer.add_relationship({"/Alternate/root.xml", "external", "urn:vendor:external",
      "../Alternate/root.xml?view=3#external", package::target_mode::external});
  writer.add_relationship({"/Extensions/vendor.bin", "back", "urn:vendor:back", "../Alternate/root.xml#back"});
  writer.finish();
  const package::extension_policy policy{package::unknown_policy::preserve, package::unknown_policy::preserve};
  std::istringstream source{original.str()};
  const auto loaded = profile::archive::load(source, {}, policy);
  ASSERT_EQ(loaded.extensions.relocations.size(), 1U);
  EXPECT_EQ(loaded.extensions.relocations.front().original, "/Alternate/root.xml");
  EXPECT_EQ(loaded.extensions.relocations.front().canonical, "/3D/model.xml");
  std::ostringstream rewritten;
  profile::archive::write(rewritten, {&loaded.value.main, {}, &loaded.extensions});
  std::istringstream verification{rewritten.str()};
  package::opc_reader reader{verification};
  EXPECT_EQ(reader.find("/Alternate/root.xml"), nullptr);
  ASSERT_NE(reader.find("/3D/model.xml"), nullptr);
  EXPECT_EQ(reader.read("/Extensions/vendor.bin"), (std::vector<std::uint8_t>{payload.begin(), payload.end()}));
  const auto check = [&](std::string_view source_name, std::string_view role, std::string_view target) {
    const auto links = reader.find_relationships(source_name, role);
    ASSERT_EQ(links.size(), 1U);
    EXPECT_EQ(links.front()->target, target);
  };
  check("/", "urn:vendor:alias", "/3D/model.xml?view=1#selected");
  check("/3D/model.xml", "urn:vendor:outbound", "/Extensions/vendor.bin#payload");
  check("/3D/model.xml", "urn:vendor:self", "/3D/model.xml?view=2#self");
  check("/3D/model.xml", "urn:vendor:external", "../Alternate/root.xml?view=3#external");
  check("/Extensions/vendor.bin", "urn:vendor:back", "/3D/model.xml#back");
  std::istringstream reopened_source{rewritten.str()};
  const auto reopened = profile::archive::load(reopened_source, {}, policy);
  EXPECT_EQ(reopened.value.main.name, loaded.value.main.name);
  EXPECT_EQ(reopened.extensions.relationships.size(), loaded.extensions.relationships.size());
  EXPECT_EQ(loaded.extensions.relocations.front().original, "/Alternate/root.xml");
}

// Simultaneous relocation rejects ambiguous origins and honors the shared scratch/work policies.
TEST(opc_profile, relocation_conflicts_and_resource_exhaustion_reject) {
  package::preserved_extensions extensions;
  extensions.relocations = {{"/old.xml", "/first.xml"}, {"/OLD.xml", "/second.xml"}};
  std::ostringstream output;
  package::opc_writer writer{output};
  EXPECT_THROW(package::write_preserved(writer, extensions), package::error);
  EXPECT_TRUE(output.str().empty());
  extensions.relocations.resize(1);
  package::limits limits;
  limits.max_buffer_bytes = 0;
  auto shared = std::make_shared<package::budget>(limits);
  package::opc_writer bounded{output, shared};
  EXPECT_THROW(package::write_preserved(bounded, extensions), package::error);
  limits.max_buffer_bytes = package::mebibyte;
  limits.max_work_units = 0;
  shared = std::make_shared<package::budget>(limits);
  package::opc_writer no_work{output, shared};
  EXPECT_THROW(package::write_preserved(no_work, extensions), package::error);
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

// The facade identifies an exhausting native field and the generated link before stream output.
TEST(opc_profile, generated_write_retains_field_part_and_link_budget_diagnostics) {
  auto model = fixture_model();
  model.name.assign(4096U, 'a');
  auto shared = std::make_shared<package::budget>(package::limits{.max_work_units = 128U});
  std::ostringstream output;
  try {
    profile::archive::write(output, {&model, {}}, shared);
    FAIL() << "Expected generated XML work exhaustion";
  } catch (const package::error& failure) {
    EXPECT_EQ(failure.code, package::error_code::resource_limit);
    EXPECT_EQ(failure.part, "/3D/model.xml");
    EXPECT_EQ(failure.field_path, "name");
    EXPECT_EQ(failure.relationship_id, "srl_main_0");
  }
  EXPECT_TRUE(output.str().empty());
  EXPECT_LT(shared->remaining(package::resource::work_units), 128U);
}

// Retained-tree rendering remains a documented outer-location boundary after native traversal.
TEST(opc_profile, deferred_tree_emission_reports_outer_location_without_inventing_a_field) {
  namespace xml = rohit::serializer::xml;
  const auto model = fixture_model();
  package::budget shared;
  auto context = shared.xml_context("/model.xml", "modelLink");
  xml::element tree{};
  tree.qualified_name = profile::model::xml_root_name();
  xml::build_budget builder{{}, &context};
  model.serialize_xml_out(tree, builder);
  EXPECT_EQ(context.current_field, nullptr);
  shared.consume(package::resource::work_units, shared.remaining(package::resource::work_units));
  try {
    static_cast<void>(xml::write(tree, {}, &context));
    FAIL() << "Expected deferred emission budget failure";
  } catch (const package::error& failure) {
    EXPECT_EQ(failure.code, package::error_code::resource_limit);
    EXPECT_EQ(failure.part, "/model.xml");
    EXPECT_EQ(failure.relationship_id, "modelLink");
    EXPECT_TRUE(failure.field_path.empty());
  }
  EXPECT_EQ(model.name, "sample & geometry");
}

// Duplicate forward requests each consume admission even when their tuple values are equal.
TEST(opc_profile, identity_reference_budget_is_cumulative_across_typed_parts_and_load_is_atomic) {
  const auto encoded = identity_input();
  std::istringstream valid{encoded};
  auto destination = identity_package::archive::load(valid);
  ASSERT_EQ(destination.value.registries.size(), 3U);
  std::size_t validations{};
  // Three OPC links and four complete tuples in the first owner spend seven admissions.
  std::istringstream bounded{encoded};
  try {
    destination = identity_package::archive::load(bounded,
        package::limits{.max_references = 7U}, {}, [&](const auto&, auto&) { ++validations; });
    FAIL() << "Expected second-owner identity admission failure";
  } catch (const package::error& failure) {
    EXPECT_EQ(failure.code, package::error_code::resource_limit);
    EXPECT_EQ(failure.part, "/Registries/registry1.xml");
    EXPECT_EQ(failure.relationship_id, "record1");
    EXPECT_NE(failure.field_path.find("references"), std::string::npos);
    EXPECT_TRUE(failure.reference.empty());
  }
  EXPECT_EQ(validations, 0U);
  ASSERT_EQ(destination.value.registries.size(), 3U);
  EXPECT_EQ(destination.value.registries.back().value.entries.front().id, 7U);
}

// Generated Save stages the same XML reference budget before replacing any previous generation.
TEST(opc_profile, identity_reference_exhaustion_preserves_previous_save_and_borrowed_models) {
  const auto model = identity_model();
  const std::array parts{
      package::model_part_view<identity_xml::registry>{&model, "/Registries/a.xml"},
      package::model_part_view<identity_xml::registry>{&model, "/Registries/b.xml"},
      package::model_part_view<identity_xml::registry>{&model, "/Registries/c.xml"}};
  const auto path = std::filesystem::temp_directory_path() /
      ("serializer-opc-reference-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".opc");
  struct remove_file {
    std::filesystem::path value;
    // Remove only this test's uniquely named temporary output, including on assertion failure.
    ~remove_file() { std::error_code ignored; std::filesystem::remove(value, ignored); }
  } cleanup{path};
  { std::ofstream previous{path, std::ios::binary}; previous << "previous generation"; }
  try {
    identity_package::archive::save(path, {parts}, package::limits{.max_references = 2U});
    FAIL() << "Expected Save identity admission failure";
  } catch (const package::error& failure) {
    EXPECT_EQ(failure.code, package::error_code::resource_limit);
    EXPECT_EQ(failure.part, "/Registries/a.xml");
    EXPECT_EQ(failure.relationship_id, "srl_registries_0");
    EXPECT_FALSE(failure.field_path.empty());
  }
  std::ifstream previous{path, std::ios::binary};
  EXPECT_EQ((std::string{std::istreambuf_iterator<char>{previous}, {}}), "previous generation");
  EXPECT_EQ(model.entries.front().id, 7U);
  EXPECT_EQ(model.requests.size(), 2U);
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
