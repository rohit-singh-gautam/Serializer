#include "versioning.hpp"

#include <rohit/serializer.hpp>
#include <rohit/serializer_creator.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace codec = rohit::serializer;
using record = versioning_test::record;

// Return an independently encoded historical layout, including its version prefix.
std::vector<std::uint8_t> historical_positional(std::uint8_t version) {
  std::vector<std::uint8_t> bytes{version, 42, 0, 0, 0, 0, 0, 0, 0};
  if (version >= 9) {
    bytes.push_back(0);
  }
  const auto text = version < 10 ? std::string_view{"old"} : std::string_view{"new"};
  bytes.push_back(static_cast<std::uint8_t>(text.size()));
  bytes.insert(bytes.end(), text.begin(), text.end());
  return bytes;
}

// Decode one exact positional message with a compile-time read policy.
template <codec::read_policy Policy>
record read_positional(const std::vector<std::uint8_t>& bytes) {
  auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
  codec::binary_none<codec::serialize_type::in, decltype(input),
                     codec::binary_text_validation::strict, Policy>
      decoder{input};
  record value{};
  decoder.serialize_in(value);
  decoder.finish();
  return value;
}

// Decode keyed JSON into a fresh model with explicit revision and unknown-field handling.
template <codec::read_policy Policy>
record read_json(std::string_view text) {
  auto input = rohit::make_constant_stream(text.data(), text.size());
  codec::json<codec::serialize_type::in, decltype(input), Policy> decoder{input};
  record value{};
  decoder.serialize_in(value);
  decoder.finish();
  return value;
}

// Parse one schema without retaining the source buffer.
auto parse_schema(std::string_view text) {
  auto input = rohit::make_constant_stream(text.data(), text.size());
  return codec::parser::parse(input);
}

// Old bytes select historical layouts, while strict reading admits only the current revision.
TEST(Versioning, ReadsFrozenPositionalLayouts) {
  for (const auto revision : {std::uint8_t{8}, std::uint8_t{9}, std::uint8_t{10}}) {
    const auto value =
        read_positional<codec::read_policy::compatible>(historical_positional(revision));
    EXPECT_EQ(value.version, revision);
    EXPECT_EQ(value.id, 42);
    EXPECT_EQ(value.enabled, revision == 8);
    EXPECT_EQ(value.old_name, revision < 10 ? "old" : "");
    EXPECT_EQ(value.name, "new");
    if (revision != 10) {
      EXPECT_THROW(read_positional<codec::read_policy::strict>(historical_positional(revision)),
                   std::invalid_argument);
    }
  }
  EXPECT_EQ(read_positional<codec::read_policy::strict>(historical_positional(10)).id, 42);
  EXPECT_THROW(read_positional<codec::read_policy::flexible>(historical_positional(7)),
               std::invalid_argument);
  EXPECT_THROW(read_positional<codec::read_policy::flexible>(historical_positional(11)),
               std::invalid_argument);
}

// Writers omit inactive fields while retaining exactly the independent historical bytes.
TEST(Versioning, WritesFrozenPositionalLayouts) {
  for (const auto revision : {std::uint8_t{8}, std::uint8_t{9}, std::uint8_t{10}}) {
    record value{};
    value.version = revision;
    value.id = 42;
    value.enabled = false;
    value.old_name = "old";
    value.name = "new";
    rohit::full_stream_auto_alloc output;
    value.serialize_out<codec::binary_none>(output);
    const std::vector<std::uint8_t> actual{output.begin(),
                                           output.begin() + output.current_offset()};
    EXPECT_EQ(actual, historical_positional(revision));
  }
}

// Historical strict JSON uses declared lifetimes and accepts the version at any property position.
TEST(Versioning, ValidatesJsonRevisionAndLifecycle) {
  const auto value =
      read_json<codec::read_policy::compatible>(R"({"old_name":"old","id":42,"version":8})");
  EXPECT_EQ(value.old_name, "old");
  EXPECT_TRUE(value.enabled);
  EXPECT_THROW(read_json<codec::read_policy::strict>(R"({"version":8})"), std::invalid_argument);
  EXPECT_THROW(read_json<codec::read_policy::compatible>(R"({"version":8,"name":"wrong"})"),
               std::invalid_argument);
  EXPECT_THROW(read_json<codec::read_policy::flexible>(R"({"version":10,"old_name":"wrong"})"),
               std::invalid_argument);
  EXPECT_THROW(read_json<codec::read_policy::compatible>(R"({"id":42})"), std::invalid_argument);
  EXPECT_THROW(read_json<codec::read_policy::compatible>(R"({"version":10,"version":10})"),
               std::invalid_argument);
  EXPECT_THROW(read_json<codec::read_policy::compatible>(R"({"version":10,"extra":1})"),
               rohit::serializer::exception::key_not_found);
  EXPECT_EQ(
      read_json<codec::read_policy::flexible>(R"({"extra":{"nested":[1,2]},"version":10,"id":42})")
          .id,
      42);
  EXPECT_THROW(read_json<codec::read_policy::flexible>(R"({"version":11,"extra":1})"),
               std::invalid_argument);
}

// Version component comparisons are numeric and their binary prefix has no sequence count.
TEST(Versioning, DottedVersionsRoundTrip) {
  EXPECT_LT(codec::version3(1, 2, 0), codec::version3(1, 10, 0));
  versioning_test::dotted original{};
  original.ver = codec::version3{1, 2, 0};
  original.id = 42;
  original.old_value = 9;
  rohit::full_stream_auto_alloc output;
  original.serialize_out<codec::binary_none>(output);
  const std::array<std::uint8_t, 6> prefix{1, 0, 2, 0, 0, 0};
  EXPECT_TRUE(std::equal(prefix.begin(), prefix.end(), output.begin()));
  auto input = rohit::make_constant_stream(output.begin(), output.current_offset());
  codec::binary_none<codec::serialize_type::in, decltype(input),
                     codec::binary_text_validation::strict, codec::read_policy::compatible>
      decoder{input};
  versioning_test::dotted decoded{};
  decoder.serialize_in(decoded);
  decoder.finish();
  EXPECT_EQ(decoded.ver, original.ver);
  EXPECT_EQ(decoded.old_value, 9);
  EXPECT_EQ(decoded.value, 42);
  EXPECT_THROW(codec::version3::parse("1.02.3"), std::invalid_argument);
  EXPECT_THROW(codec::version3::parse("1.2.65536"), std::invalid_argument);
}

// Floating discriminators exclude nonfinite revisions independently of the policy.
TEST(Versioning, FloatingVersionsRejectNonfiniteValues) {
  versioning_test::floating value{};
  value.version = std::numeric_limits<double>::infinity();
  rohit::full_stream_auto_alloc output;
  EXPECT_THROW(value.serialize_out<codec::binary_none>(output), std::invalid_argument);
}

// Every native key mode preserves historical definitions while flexible binary rejects unknown values.
TEST(Versioning, KeyedBinaryRetainsHistoricalFields) {
  const auto check = []<codec::serialize_key_type Keys>() {
    record original{};
    original.version = 8;
    original.id = 42;
    original.old_name = "old";
    rohit::full_stream_auto_alloc output;
    codec::binary<codec::serialize_type::out, Keys, std::endian::little, decltype(output)> writer{
        output};
    writer.serialize_out(original);
    auto input = rohit::make_constant_stream(output.begin(), output.current_offset());
    codec::binary<codec::serialize_type::in, Keys, std::endian::little, decltype(input),
                  codec::binary_text_validation::strict, codec::read_policy::compatible>
        reader{input};
    record decoded{};
    reader.serialize_in(decoded);
    reader.finish();
    EXPECT_EQ(decoded.version, 8);
    EXPECT_EQ(decoded.id, 42);
    EXPECT_EQ(decoded.old_name, "old");
  };
  check.operator()<codec::serialize_key_type::integer>();
  check.operator()<codec::serialize_key_type::string>();
}

// Dotted JSON uses canonical strings and compares component values before selecting fields.
TEST(Versioning, DottedJsonUsesCanonicalStrings) {
  constexpr std::string_view text = R"({"id":42,"old_value":9,"schema_version":"1.2.0"})";
  auto input = rohit::make_constant_stream(text.data(), text.size());
  codec::json<codec::serialize_type::in, decltype(input), codec::read_policy::compatible> reader{
      input};
  versioning_test::dotted value{};
  reader.serialize_in(value);
  reader.finish();
  EXPECT_EQ(value.ver, codec::version3(1, 2, 0));
  rohit::full_stream_auto_alloc output;
  value.serialize_out<codec::json>(output);
  const std::string actual{reinterpret_cast<const char*>(output.begin()), output.current_offset()};
  EXPECT_NE(actual.find("\"schema_version\":\"1.2.0\""), std::string::npos);
  EXPECT_NE(actual.find("\"old_value\""), std::string::npos);
  EXPECT_EQ(actual.find("\"value\""), std::string::npos);
}

// Invalid lifetimes, reservation conflicts, and ambiguous discriminators fail during parsing.
TEST(Versioning, RejectsInvalidSchemaContracts) {
  for (const auto text :
       {"class x { public version; }", "class x { public version { 256 }; }",
        "class x { public version { 010 }; }", "class x { public version float { .5 }; }",
        "class x { public version float { 1. }; }",
        "class x { public version uint34 version { 1 }; }",
        "class x { public version { 2 } compatibility { 3 }; }",
        "class x { public version { 1 }; public version ver (2) { 1 }; }",
        "class x { public version version (1) { 1 }; public uint32 value (1); }",
        "class x { public version { 1 }; created(2) public uint32 value (2); }",
        "class x { created(1) public uint32 value; }",
        "class x { public version { 3 }; created(2) obsolete(2) public uint32 value (2); }",
        "class x { public version { 3 }; created(3) replaced(missing) public uint32 value (2); }",
        "class x { reserve id { 2 }; public uint32 value (2); }",
        "class x { reserve variable { value }; public uint32 value; }",
        "class x { reserve display { \"wire\" }; public uint32 value (\"wire\"); }",
        "class x { reserve id { 2, 2 }; }", "class x { reserve id {}; }"}) {
    EXPECT_THROW(parse_schema(text), rohit::exception::base_parser) << text;
  }
}
} // namespace

// Default version identities avoid declared and reserved IDs even when the discriminator is last.
TEST(Versioning, AllocatesDefaultVersionIdFromFreeIdentities) {
  constexpr std::string_view text =
      "class x stable_ids { public uint32 revision (1); reserve id {2}; public version { 1 }; }";
  const auto input = rohit::make_constant_stream(text.data(), text.size());
  const auto schema = codec::parser::parse(input);
  const auto& value = static_cast<const codec::class_node&>(*schema.front());
  ASSERT_NE(value.version_member(), nullptr);
  EXPECT_EQ(value.version_member()->id, 3);
}
