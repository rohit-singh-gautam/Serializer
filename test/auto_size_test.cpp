// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <derived_sizes.hpp>
#include <rohit/serializer_creator.hpp>
#include <rohit/schema_compatibility.hpp>
#include <rohit/constant_binary_output.hpp>
#include <gtest/gtest.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {
namespace codec = rohit::serializer;
using keys = codec::serialize_key_type;

// Encode through the production native protocol and retain only its written bytes.
template <keys Keys, typename Value>
std::string encode(const Value& value) {
  rohit::full_stream_auto_alloc output;
  codec::binary<codec::serialize_type::out, Keys> protocol{output};
  protocol.serialize_out(value);
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}

// Require exact bounded input consumption as well as the generated size validation.
template <keys Keys, typename Value>
Value decode(const std::string& bytes) {
  auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
  codec::binary<codec::serialize_type::in, Keys, std::endian::little, decltype(input)> protocol{input};
  Value result{};
  protocol.serialize_in(result);
  protocol.finish();
  return result;
}

// Resolve new annotations with their explicit language contract.
auto parse(std::string_view source) {
  return codec::parser::parse(rohit::make_constant_stream(source.data(), source.size()));
}

// Verify nested record-local spans and unchanged stored metadata across every native key mode.
template <keys Keys>
void verify_records() {
  sized_envelope source{};
  source.encoded_size = 123;
  source.payload.encoded_size = 456;
  source.payload.text = "payload";
  source.payload.samples = {3, 5, 7};
  source.items = {source.payload, sized_payload{}};
  source.items.back().text = "different";
  const auto bytes = encode<Keys>(source);
  const auto restored = decode<Keys, sized_envelope>(bytes);
  EXPECT_EQ(restored.encoded_size, bytes.size());
  EXPECT_EQ(restored.payload.encoded_size, encode<Keys>(source.payload).size());
  ASSERT_EQ(restored.items.size(), 2u);
  EXPECT_EQ(restored.items[1].encoded_size, encode<Keys>(source.items[1]).size());
  EXPECT_EQ(source.encoded_size, 123u);
  EXPECT_EQ(source.payload.encoded_size, 456u);
  EXPECT_EQ((codec::binary_size<Keys>(source)), bytes.size());
  EXPECT_THROW((codec::binary_size<Keys>(source, bytes.size() - 1)),
               rohit::exception::stream_overflow_exception);
  auto truncated = bytes;
  truncated.pop_back();
  EXPECT_THROW((decode<Keys, sized_envelope>(truncated)), std::exception);

  wire_sized wire{};
  wire.text = "transport metadata";
  EXPECT_EQ((decode<Keys, wire_sized>(encode<Keys>(wire))).text, wire.text);
  payload_sized payload{};
  payload.encoded_size = 17;
  payload.text = "payload scope";
  const auto payload_bytes = encode<Keys>(payload);
  constexpr auto key_bytes = Keys == keys::none ? 0u : Keys == keys::integer ? 1u : 13u;
  EXPECT_EQ((decode<Keys, payload_sized>(payload_bytes)).encoded_size,
            payload_bytes.size() - key_bytes - sizeof(std::uint32_t));
  EXPECT_EQ(payload.encoded_size, 17u);
}
} // namespace

// Wire metadata contributes bytes, never object storage; every native mode shares one contract.
TEST(auto_size, record_spans_and_wire_storage) {
  static_assert(sizeof(wire_sized) == sizeof(std::string));
  verify_records<keys::none>();
  verify_records<keys::integer>();
  verify_records<keys::string>();
}

// A forged value, missing header, or duplicate header remains invalid independently of framing.
TEST(auto_size, selected_header_is_required_and_verified) {
  sized_payload source{};
  source.text = "text";
  const auto bytes = encode<keys::integer>(source);
  auto mismatch = bytes;
  mismatch[1] ^= 1;
  EXPECT_THROW((decode<keys::integer, sized_payload>(mismatch)), std::exception);
  auto missing = bytes;
  missing.erase(0, 1 + sizeof(std::uint32_t));
  EXPECT_THROW((decode<keys::integer, sized_payload>(missing)), std::exception);
  auto duplicate = bytes;
  duplicate.insert(0, bytes.substr(0, 1 + sizeof(std::uint32_t)));
  EXPECT_THROW((decode<keys::integer, sized_payload>(duplicate)), std::exception);
}

// Payload scope observes the actual accepted compact key width rather than assuming canonical input.
TEST(auto_size, payload_scope_accepts_noncanonical_header_key) {
  payload_sized source{};
  source.text = "text";
  auto bytes = encode<keys::integer>(source);
  bytes.insert(0, 1, static_cast<char>(0x40));
  const auto restored = decode<keys::integer, payload_sized>(bytes);
  EXPECT_EQ(restored.encoded_size, bytes.size() - 2 - sizeof(std::uint32_t));
}

// Complement lists are canonical, version gated, and never silently accept unsupported size forms.
TEST(auto_size, annotation_validation_and_complements) {
  const auto schema = parse("serializer version 1.9.0; class sample { public uint32 size(1) auto_size(binary_positional) omit(all_except(binary_none)); }");
  const auto& owner = static_cast<const codec::class_node&>(*schema.back());
  EXPECT_EQ(owner.member_list.front().auto_size_formats, std::vector<std::string>{"binary_none"});
  EXPECT_EQ(owner.member_list.front().omitted_formats.size(), 6u);
  for (const auto suffix : {"auto_size()", "auto_size(json)", "auto_size(binary_none,binary_positional)",
                            "omit(all_except())", "omit(all_except(json,json))", "omit(all_except(missing))"}) {
    EXPECT_THROW(parse(std::string{"serializer version 1.9.0; class sample { public uint32 size(1) "} + suffix + "; }"), std::exception);
  }
  EXPECT_THROW(parse("serializer version 1.8.0; class sample { public uint32 size auto_size(binary_none); }"), std::exception);
  EXPECT_THROW(parse("serializer version 1.9.0; class sample { public wire_only uint32 size(1) auto_size(binary_none); }"), std::exception);
  EXPECT_THROW(parse("serializer version 1.9.0; class sample { public uint16 size(1) auto_size(binary_none); }"), std::exception);
  EXPECT_THROW(parse("serializer version 1.9.0; class sample { public wire_only magic uint32 signature(1) {7}; }"), std::exception);
}

// Storage removal preserves size wire bytes, while changing derivation or scope changes validation.
TEST(auto_size, compatibility_tracks_meaning_and_ignores_storage) {
  const auto schema = [](std::string_view declaration) {
    return codec::parser::parsed_schema{parse(std::string{"serializer version 1.9.0; class sample { public "} +
      std::string{declaration} + "; public string text(2); }"), {}};
  };
  const auto stored = schema("uint32 size(1) auto_size(binary_integer) omit(all_except(binary_integer))");
  const auto wire = schema("wire_only uint32 size(1) auto_size(binary_integer) omit(all_except(binary_integer))");
  const auto payload = schema("uint32 size(1) auto_size(binary_integer) size_scope(payload) omit(all_except(binary_integer))");
  const auto ordinary = schema("uint32 size(1) omit(all_except(binary_integer))");
  EXPECT_TRUE(codec::check_schema_compatibility(stored, wire, codec::compatibility_protocol::binary_integer).empty());
  EXPECT_FALSE(codec::check_schema_compatibility(stored, payload, codec::compatibility_protocol::binary_integer).empty());
  EXPECT_FALSE(codec::check_schema_compatibility(ordinary, stored, codec::compatibility_protocol::binary_integer).empty());
  EXPECT_TRUE(codec::check_schema_compatibility(stored, payload, codec::compatibility_protocol::json).empty());
}

// Class-specific policies diagnose typos and never silently attach to unrelated records.
TEST(auto_size, configuration_requires_size_metadata) {
  namespace writer = codec::writer;
  auto schema = parse("serializer version 1.9.0; class ordinary { public uint32 value; }");
  writer::cpp_options options; options.format = false;
  options.class_auto_sizes["missing"].validation = writer::auto_size_validation::retain;
  EXPECT_THROW(writer::cpp::generate(schema, options), std::exception);
  options.class_auto_sizes.clear();
  options.class_auto_sizes["ordinary"].validation = writer::auto_size_validation::retain;
  EXPECT_THROW(writer::cpp::generate(schema, options), std::exception);
}

// Raw counters safely qualify multi-gigabyte extents without allocating or constructing invalid spans.
TEST(auto_size, count_only_header_width_bounds) {
  constexpr auto maximum_size = std::numeric_limits<std::uint32_t>::max();
  if constexpr (rohit::detail::maximum_buffer_bytes > maximum_size) {
    codec::binary_count_stream count{};
    codec::binary_out_base<keys::none, std::endian::little, codec::binary_count_stream> protocol{count};
    const auto guard = codec::detail::make_auto_size_count_guard<std::uint32_t>(protocol, true);
    count.commit_counted_bytes(maximum_size);
    EXPECT_NO_THROW(guard.finish());
    count.commit_counted_bytes(1);
    EXPECT_THROW(guard.finish(), rohit::exception::stream_overflow_exception);

    codec::detail::binary_count_sink payload_count{};
    codec::detail::constant_binary_out<codec::detail::binary_count_sink> payload_protocol{payload_count};
    constexpr auto header_bytes = sizeof(std::uint32_t);
    const auto payload_guard = codec::detail::make_auto_size_count_guard<std::uint32_t>(
        payload_protocol, true, header_bytes);
    payload_count.commit_counted_bytes(static_cast<std::size_t>(maximum_size) + header_bytes);
    EXPECT_NO_THROW(payload_guard.finish());
    payload_count.commit_counted_bytes(1);
    EXPECT_THROW(payload_guard.finish(), rohit::exception::stream_overflow_exception);
    const auto inactive = codec::detail::make_auto_size_count_guard<std::uint32_t>(payload_protocol, false);
    EXPECT_NO_THROW(inactive.finish());
  }
  namespace writer = codec::writer;
  const auto schema = parse("serializer version 1.9.0; class sample { public uint32 size(1) auto_size(binary_none); }");
  writer::cpp_options options; options.format = false;
  const auto generated = writer::cpp::generate(schema, options);
  EXPECT_NE(generated.find("make_auto_size_count_guard<::std::uint32_t>"), std::string::npos);
  EXPECT_NE(generated.find("auto_size_count_guard_1.finish()"), std::string::npos);
}
