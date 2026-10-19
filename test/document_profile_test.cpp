// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <gtest/gtest.h>
#include <rohit/document_profile.hpp>

#include <array>
#include <stdexcept>

namespace document = rohit::serializer::document;
namespace compression = rohit::serializer::compression;
namespace xml = rohit::serializer::xml;

// Missing settings have explicit XML/uncompressed/unpackaged defaults; runtime selection is checked.
TEST(document_profile, defaults_and_runtime_overrides) {
  const auto defaults = document::parse_profile({});
  EXPECT_EQ(defaults.format, document::payload_format::xml);
  EXPECT_EQ(document::parse_profile({{"format", "bytes"}}).format,
            document::payload_format::opaque_bytes);
  const auto runtime =
      document::resolve_profile(defaults, {.format = document::payload_format::json,
                                           .compression = compression::format::gzip,
                                           .compression_level = 9});
  EXPECT_EQ(runtime.format, document::payload_format::json);
  EXPECT_EQ(runtime.compression, compression::format::gzip);
  EXPECT_EQ(defaults.compression, compression::format::none);
  EXPECT_THROW(
      (void)document::resolve_profile(defaults, {.compression = compression::format::gzip,
                                                 .container = document::container_format::opc}),
      std::invalid_argument);
}

// Fixed profiles reject divergent overrides and strict configuration never ignores misspelled options.
TEST(document_profile, fixed_profiles_and_invalid_configuration) {
  const auto fixed = document::parse_profile({{"policy", "fixed"}, {"compression", "deflate"}});
  EXPECT_NO_THROW(
      (void)document::resolve_profile(fixed, {.compression = compression::format::deflate}));
  EXPECT_THROW((void)document::resolve_profile(fixed, {.format = document::payload_format::json}),
               std::invalid_argument);
  EXPECT_THROW((void)document::parse_profile({{"compression", "custom"}}), std::invalid_argument);
  EXPECT_THROW((void)document::parse_profile({{"levle", "3"}}), std::invalid_argument);
  EXPECT_THROW((void)document::parse_profile({{"level", "3tail"}}), std::invalid_argument);
  EXPECT_THROW((void)document::parse_profile({{"compression", "gzip"}, {"level", "10"}}),
               std::invalid_argument);
}

// Expected schema type, expanded root/type and declared vocabulary must all agree before decoding.
TEST(document_profile, dispatch_never_selects_wrong_root_type_or_vocabulary) {
  const std::array<document::dispatch_metadata, 1> inventory{{{"model::shape",
                                                               {"shape", "urn:shape"},
                                                               xml::name{"shapeType", "urn:shape"},
                                                               true,
                                                               {"urn:shape"}}}};
  const std::array<std::string, 1> understood{"urn:shape"};
  EXPECT_NO_THROW(document::validate_dispatch_inventory(inventory));
  const auto& entry = inventory.front();
  EXPECT_EQ(document::select_decoder(inventory, "model::shape", entry.root, entry.type, understood),
            &entry);
  EXPECT_EQ(document::select_decoder(inventory, "other", entry.root, entry.type, understood),
            nullptr);
  EXPECT_EQ(document::select_decoder(inventory, "model::shape", {"shape", "urn:other"}, entry.type,
                                     understood),
            nullptr);
  EXPECT_EQ(document::select_decoder(inventory, "model::shape", entry.root,
                                     xml::name{"other", "urn:shape"}, understood),
            nullptr);
  EXPECT_EQ(document::select_decoder(inventory, "model::shape", entry.root, {}, understood),
            nullptr);
  EXPECT_EQ(document::select_decoder(inventory, "model::shape", entry.root, entry.type), nullptr);
}

// QName type dispatch resolves the original document prefix scope rather than comparing lexical text.
TEST(document_profile, prefixed_type_resolution_and_ambiguity) {
  const std::array<document::dispatch_metadata, 1> inventory{
      {{"shape", {"shape", "urn:shape"}, xml::name{"type", "urn:type"}, true, {}}}};
  xml::element root{};
  root.qualified_name = inventory.front().root;
  root.namespace_bindings.emplace("t", "urn:type");
  root.attributes.push_back({{"type", "http://www.w3.org/2001/XMLSchema-instance"}, "t:type"});
  EXPECT_EQ(document::select_decoder(inventory, "shape", root), &inventory.front());
  root.attributes.front().value = "unbound:type";
  EXPECT_EQ(document::select_decoder(inventory, "shape", root), nullptr);
  const std::array duplicates{inventory.front(), inventory.front()};
  EXPECT_THROW(document::validate_dispatch_inventory(duplicates), std::invalid_argument);
  EXPECT_EQ(
      document::select_decoder(duplicates, "shape", inventory.front().root, inventory.front().type),
      nullptr);
}
