// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "owning_variants_samples.hpp"
#include <owning_variant_cpp.hpp>

#include <rohit/output_options.hpp>
#include <rohit/schema_compatibility.hpp>
#include <rohit/serializer_creator.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {
namespace codec = rohit::serializer;
namespace samples = owning_variants_samples;

// Resolve an in-memory schema through the production parser including its language contract.
codec::parser::parsed_schema schema(std::string_view source) {
  const auto input = rohit::make_constant_full_stream(source.data(), source.size());
  return {codec::parser::parse(input), {}};
}

// Compare the selected payload semantically without relying on a hand-maintained discriminator.
void expect_sample(const owning_variants::record& value, std::size_t index) {
  ASSERT_EQ(value.payload.index(), index);
  EXPECT_EQ(static_cast<std::size_t>(value.get_payload_type()), index);
  switch (index) {
  case 0: EXPECT_EQ(std::get<0>(value.payload), 17U); break;
  case 1: EXPECT_EQ(std::get<1>(value.payload), "owned text"); break;
  case 2: {
    const auto& event = std::get<2>(value.payload);
    EXPECT_EQ(event.text, "nested event");
    EXPECT_EQ(event.numbers, (std::vector<std::uint32_t>{1U, 2U, 3U}));
    EXPECT_EQ(event.labels.at("alpha"), "one");
    EXPECT_EQ(event.labels.at("beta"), "two");
    break;
  }
  case 3: EXPECT_EQ(std::get<3>(value.payload), "alias text"); break;
  default: FAIL() << "Unknown sample";
  }
}

// Reuse destinations across every arm and reject all truncated messages through each native codec.
template <template <codec::serialize_type> class Protocol>
void check_round_trip() {
  auto destination = samples::sample(2);
  for (std::size_t index{}; index < std::variant_size_v<owning_variants::record::u_payload>; ++index) {
    const auto bytes = samples::encode<Protocol>(samples::sample(index));
    samples::decode<Protocol>(bytes, destination);
    expect_sample(destination, index);
    for (std::size_t length{}; length < bytes.size(); ++length) {
      SCOPED_TRACE(length);
      owning_variants::record partial{};
      const std::vector<std::uint8_t> truncated{bytes.begin(), bytes.begin() + length};
      EXPECT_THROW(samples::decode<Protocol>(truncated, partial), rohit::exception::base_parser);
    }
    auto trailing = bytes;
    trailing.push_back(0xffU);
    EXPECT_THROW(samples::decode<Protocol>(trailing, destination), rohit::exception::base_parser);
  }
}

// Qualify the invariant that union and variant differ in ownership while retaining the same wire data.
template <template <codec::serialize_type> class Protocol>
void check_raw_parity() {
  using owned = owning_variants::primitive_record;
  owning_variants::raw_record raw{};
  owned variant{};
  raw.payload.code = 42U;
  variant.emplace_payload<owned::e_payload::code>(42U);
  EXPECT_EQ(samples::encode<Protocol>(raw), samples::encode<Protocol>(variant));
  samples::decode<Protocol>(samples::encode<Protocol>(raw), variant);
  EXPECT_EQ(std::get<0>(variant.payload), 42U);
  raw.payload_type = owning_variants::raw_record::e_payload::measure;
  std::construct_at(&raw.payload.measure, 1.25);
  variant.emplace_payload<owned::e_payload::measure>(1.25);
  EXPECT_EQ(samples::encode<Protocol>(raw), samples::encode<Protocol>(variant));
  samples::decode<Protocol>(samples::encode<Protocol>(variant), raw);
  EXPECT_EQ(raw.payload_type, owning_variants::raw_record::e_payload::measure);
  EXPECT_EQ(raw.payload.measure, 1.25);
}
}

// Normal value semantics retain nested owned memory and duplicate C++ types remain distinct arms.
TEST(owning_variants, owns_nested_values_and_switches_safely) {
  using record = owning_variants::record;
  static_assert(std::is_copy_constructible_v<record> && std::is_move_constructible_v<record>);
  static_assert(std::is_destructible_v<record>);
  record default_value{};
  EXPECT_EQ(default_value.get_payload_type(), record::e_payload::code);
  EXPECT_EQ(std::get<0>(default_value.payload), 0U);
  auto original = samples::sample(2);
  auto copy = original;
  std::get<2>(original.payload).text = "changed";
  std::get<2>(original.payload).numbers.clear();
  expect_sample(copy, 2);
  auto moved = std::move(copy);
  expect_sample(moved, 2);
  original = moved;
  expect_sample(original, 2);
  std::size_t visits{};
  const auto& constant_value = original;
  constant_value.visit_payload([&](const auto& payload) {
    ++visits;
    using payload_type = std::remove_cvref_t<decltype(payload)>;
    if constexpr (std::is_same_v<payload_type, owning_variants::text_payload>) {
      EXPECT_EQ(payload.text, "nested event");
    } else {
      FAIL() << "Visited an inactive payload";
    }
  });
  EXPECT_EQ(visits, 1U);
  original.visit_payload([](auto& payload) {
    if constexpr (std::is_same_v<std::remove_cvref_t<decltype(payload)>, owning_variants::text_payload>) {
      payload.numbers.push_back(4U);
    }
  });
  EXPECT_EQ(std::get<2>(original.payload).numbers.back(), 4U);
  EXPECT_EQ(std::get<2>(moved.payload).numbers.size(), 3U);
  original.emplace_payload<record::e_payload::text>("temporary");
  original.emplace_payload<record::e_payload::alias>("alias text");
  expect_sample(original, 3);
  EXPECT_EQ(record::to_string(original.get_payload_type()), "alias");
  EXPECT_EQ(record::to_e_payload("alias"), record::e_payload::alias);
}

// Generated ownership works through all four established native protocols.
TEST(owning_variants, four_protocol_round_trips_and_truncation) {
  check_round_trip<codec::binary_none>();
  check_round_trip<codec::binary_integer>();
  check_round_trip<codec::binary_string>();
  check_round_trip<codec::json>();
  check_raw_parity<codec::binary_none>();
  check_raw_parity<codec::binary_integer>();
  check_raw_parity<codec::binary_string>();
  check_raw_parity<codec::json>();
}

// Unknown alternative identifiers fail before replacing an existing owned payload.
TEST(owning_variants, rejects_unknown_discriminators_and_respects_limits) {
  auto value = samples::sample(2);
  const std::vector<std::uint8_t> invalid_index{4U};
  EXPECT_THROW(samples::decode<codec::binary_none>(invalid_index, value), codec::exception::bad_input_data);
  expect_sample(value, 2);
  const std::vector<std::uint8_t> invalid_keyed_index{1U, 4U, 0U};
  EXPECT_THROW(samples::decode<codec::binary_integer>(invalid_keyed_index, value), codec::exception::bad_input_data);
  expect_sample(value, 2);
  constexpr std::string_view unknown{"{\"payload:missing\":0}"};
  const std::vector<std::uint8_t> unknown_bytes{unknown.begin(), unknown.end()};
  EXPECT_THROW(samples::decode<codec::json>(unknown_bytes, value), codec::exception::key_not_found);
  expect_sample(value, 2);
  codec::decode_limits limits{};
  limits.max_string_bytes = 3U;
  EXPECT_THROW(samples::decode<codec::binary_none>(samples::encode<codec::binary_none>(samples::sample(1)), value, limits),
               codec::exception::resource_limit);
  value = samples::sample(2);
  limits = {};
  limits.max_collection_elements = 2U;
  EXPECT_THROW(samples::decode<codec::json>(samples::encode<codec::json>(samples::sample(2)), value, limits),
               codec::exception::resource_limit);
}

// The new keyword is language-gated; raw union's existing declarations and generated API persist.
TEST(owning_variants, parser_contract_and_codegen_compatibility) {
  for (const std::string_view version : {"1", "1.0.0", "1.1.0", "1.2.0", "1.3.0"}) {
    const auto text = "serializer version " + std::string{version} +
        "; class record { public variant(uint32 = code, string = text) payload; }";
    EXPECT_THROW(schema(text), rohit::exception::base_parser);
  }
  const auto owned = schema("serializer version 1.4.0; class record { public variant(uint32 = code, string = text) payload; }");
  const auto& object = *static_cast<const codec::class_node*>(owned.statements.front().get());
  ASSERT_EQ(object.member_list.size(), 1U);
  EXPECT_TRUE(object.member_list.front().owning_variant);
  codec::writer::cpp_options options{};
  options.format = false;
  const auto generated = codec::writer::cpp::generate(owned.statements, options);
  EXPECT_NE(generated.find("::std::variant<"), std::string::npos);
  EXPECT_EQ(generated.find("payload_type{}"), std::string::npos);
  options.protobuf = true;
  EXPECT_THROW(codec::writer::cpp::generate(owned.statements, options), std::invalid_argument);
  options.protobuf = false;
  options.protocols = codec::writer::cpp_protocols::binary_none;
  options.constant_evaluation = true;
  options.emission_only = true;
  EXPECT_THROW(codec::writer::cpp::generate(owned.statements, options), std::invalid_argument);
  const auto raw = schema("serializer version 1; class record { public union(uint32 = code, double = measure) payload; }");
  options = {};
  options.format = false;
  const auto old_source = codec::writer::cpp::generate(raw.statements, options);
  EXPECT_NE(old_source.find("union u_payload"), std::string::npos);
  EXPECT_NE(old_source.find("payload_type{}"), std::string::npos);
  EXPECT_EQ(old_source.find("std::variant"), std::string::npos);
}

// Ownership selection does not introduce a schema wire migration in either reader direction.
TEST(owning_variants, compatibility_checker_compares_wire_shape) {
  const auto raw = schema("serializer version 1; class record { public union(uint32 = code, double = measure) payload; }");
  const auto owned = schema("serializer version 1.4.0; class record { public variant(uint32 = code, double = measure) payload; }");
  for (const auto protocol : {codec::compatibility_protocol::binary_none, codec::compatibility_protocol::binary_integer,
                             codec::compatibility_protocol::binary_string, codec::compatibility_protocol::json}) {
    EXPECT_TRUE(codec::check_schema_compatibility(raw, owned, protocol).empty());
    EXPECT_TRUE(codec::check_schema_compatibility(owned, raw, protocol).empty());
  }
}

// Existing user types and generic parameters named variant remain valid with older headers.
TEST(owning_variants, preserves_existing_variant_identifiers) {
  for (const std::string_view version : {"1", "1.0.0", "1.1.0", "1.2.0", "1.3.0", "1.4.0"}) {
    const auto text = "serializer version " + std::string{version} +
        "; class variant { public uint32 number; } "
        "class holder { public variant value; } "
        "namespace legacy { class variant { public string text; } } "
        "class qualified { public legacy::variant value; } "
        "class generic<variant> { public variant value; }";
    const auto parsed = schema(text);
    const auto found = std::find_if(parsed.statements.begin(), parsed.statements.end(), [](const auto& node) {
      return node->type == codec::object_type::class_type && node->name == "holder";
    });
    ASSERT_NE(found, parsed.statements.end());
    const auto& holder = *static_cast<const codec::class_node*>(found->get());
    EXPECT_FALSE(holder.member_list.front().owning_variant);
    EXPECT_EQ(holder.member_list.front().modifier, codec::member::modifier_type::none);
    codec::writer::cpp_options options{};
    options.format = false;
    EXPECT_EQ(codec::writer::cpp::generate(parsed.statements, options).find("std::variant<"), std::string::npos);
  }
}

// Generic alternatives keep their actual owning type and compile through generated C++ templates.
TEST(owning_variants, generic_and_binary_view_storage) {
  using generic_record = owning_variant_cpp::holder<std::string>;
  generic_record generic{};
  generic.emplace_payload<generic_record::e_payload::value>("generic owned text");
  generic_record decoded{};
  samples::decode<codec::json>(samples::encode<codec::json>(generic), decoded);
  EXPECT_EQ(std::get<0>(decoded.payload), "generic owned text");

  using owned = owning_variant_cpp::view_record<codec::storage_mode::owning>;
  using readonly_view = owning_variant_cpp::view_record<codec::storage_mode::read_only_view>;
  using mutable_view = owning_variant_cpp::view_record<codec::storage_mode::mutable_view>;
  owned value{};
  value.emplace_payload<owned::e_payload::measure>(1.25);
  auto bytes = samples::encode<codec::binary_none>(value);
  auto reader = readonly_view::map(std::span<const std::uint8_t>{bytes});
  EXPECT_EQ(reader.get_payload_type(), readonly_view::e_payload::measure);
  EXPECT_EQ(reader.get_payload().template get<1>(), 1.25);
  auto editor = mutable_view::map(std::span<std::uint8_t>{bytes});
  editor.get_payload().template set<1>(2.5);
  samples::decode<codec::binary_none>(bytes, value);
  EXPECT_EQ(value.get_payload_type(), owned::e_payload::measure);
  EXPECT_EQ(std::get<1>(value.payload), 2.5);
}

// Generic-only schemas need variant support and the same unsupported-profile diagnostics.
TEST(owning_variants, generic_only_generation_selects_header_and_profiles) {
  const auto parsed = schema("serializer version 1.4.0; class holder<T> { public variant(T = value, uint32 = code) payload; }");
  codec::writer::cpp_options options{};
  options.format = false;
  const auto source = codec::writer::cpp::generate(parsed.statements, options);
  EXPECT_NE(source.find("#include <variant>"), std::string::npos);
  EXPECT_NE(source.find("::std::variant<T, ::std::uint32_t>"), std::string::npos);
  options.protobuf = true;
  EXPECT_THROW(codec::writer::cpp::generate(parsed.statements, options), std::invalid_argument);
  options.protobuf = false;
  options.protocols = codec::writer::cpp_protocols::binary_none;
  options.constant_evaluation = true;
  options.emission_only = true;
  EXPECT_THROW(codec::writer::cpp::generate(parsed.statements, options), std::invalid_argument);
  const auto raw = "serializer version 1.4.0; class holder<T> { public union(T = value, uint32 = code) payload; }";
  EXPECT_THROW(schema(raw), rohit::exception::base_parser);
}

// New generated helper names are validated before producing invalid C++ declarations.
TEST(owning_variants, validates_typed_helper_name_collisions) {
  codec::writer::cpp_options options{};
  options.format = false;
  for (const std::string_view name : {"get_payload_type", "emplace_payload", "visit_payload"}) {
    const auto parsed = schema("serializer version 1.4.0; class record { public uint32 " + std::string{name} +
        "; public variant(uint32 = code, string = text) payload; }");
    EXPECT_THROW(codec::writer::cpp::generate(parsed.statements, options), std::invalid_argument);
  }
  const auto available_name = schema("serializer version 1.4.0; class record { public uint32 payload_type; "
      "public variant(uint32 = code, string = text) payload; }");
  EXPECT_NO_THROW(codec::writer::cpp::generate(available_name.statements, options));
}

// Helper template identifiers are fresh even when outer generic names require repeated suffixes.
TEST(owning_variants, preserves_generic_parameter_names) {
  using record = owning_variant_cpp::parameter_names<std::string, std::vector<std::uint32_t>, double,
                                                    std::string, std::vector<std::uint32_t>, double>;
  record value{};
  value.emplace_payload<record::e_payload::fifth>(std::vector<std::uint32_t>{2U, 4U, 6U});
  const auto inspect = [](const auto& alternative) -> std::size_t {
    using value_type = std::remove_cvref_t<decltype(alternative)>;
    if constexpr (std::is_same_v<value_type, std::vector<std::uint32_t>>) {
      return alternative.size();
    } else {
      return 0U;
    }
  };
  EXPECT_EQ(value.visit_payload(inspect), 3U);
  EXPECT_EQ(std::as_const(value).visit_payload(inspect), 3U);
  EXPECT_EQ(record::to_string(value.get_payload_type()), "fifth");
  EXPECT_EQ(record::to_e_payload(std::string_view{"fifth"}), record::e_payload::fifth);
  record decoded{};
  samples::decode<codec::json>(samples::encode<codec::json>(value), decoded);
  EXPECT_EQ(decoded.get_payload_type(), record::e_payload::fifth);
  EXPECT_EQ(std::get<4>(decoded.payload), (std::vector<std::uint32_t>{2U, 4U, 6U}));
}
