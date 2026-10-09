// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <digests.hpp>
#include <rohit/digest.hpp>
#include <rohit/serializer.hpp>

#include <gtest/gtest.h>

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {
namespace schema = rohit::serializer;

// Encode one generated value through its real protocol and retain the owned wire bytes.
template <template <schema::serialize_type> class Protocol, typename Value>
std::vector<std::uint8_t> digest_bytes(const Value& value) {
  rohit::full_stream_auto_alloc output{};
  value.template serialize_out<Protocol>(output);
  return {output.begin(), output.begin() + output.current_offset()};
}

// Check complete generated round trips and reject every truncated message before assignment.
template <template <schema::serialize_type> class Protocol>
void check_digest_record(const digest_fixture::record& value) {
  const auto bytes = digest_bytes<Protocol>(value);
  const auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
  const auto decoded = schema::deserialize_exact<digest_fixture::record, Protocol>(input);
  EXPECT_EQ(decoded.raw, value.raw);
  EXPECT_EQ(decoded.standard, value.standard);
  EXPECT_EQ(decoded.custom, value.custom);
  EXPECT_EQ(decoded.hashes, value.hashes);
  EXPECT_EQ(decoded.values, value.values);
  auto destination = value;
  for (std::size_t length = 0; length < bytes.size(); ++length) {
    SCOPED_TRACE(length);
    const auto truncated = rohit::make_constant_stream(bytes.data(), length);
    EXPECT_THROW((destination = schema::deserialize_exact<digest_fixture::record, Protocol>(truncated)),
                 rohit::exception::base_parser);
    EXPECT_EQ(destination.raw, value.raw);
    EXPECT_EQ(destination.standard, value.standard);
    EXPECT_EQ(destination.custom, value.custom);
    EXPECT_EQ(destination.hashes, value.hashes);
    EXPECT_EQ(destination.values, value.values);
  }
}

// Compare digest wire bytes with existing uint8 array codecs rather than a duplicate implementation.
template <template <schema::serialize_type> class Protocol>
void check_digest_array_wire() {
  digest_fixture::fixed_blob digest{};
  digest_fixture::fixed_bytes bytes{};
  digest.value = {0, 128, 255};
  bytes.value = digest.value;
  EXPECT_EQ(digest_bytes<Protocol>(digest), digest_bytes<Protocol>(bytes));
  digest_fixture::variable_blob variable{};
  digest_fixture::variable_bytes variable_bytes{};
  variable.value = {0, 128, 255};
  variable_bytes.value = variable.value;
  EXPECT_EQ(digest_bytes<Protocol>(variable), digest_bytes<Protocol>(variable_bytes));
}
} // namespace

// Explicit hashing helpers return the exact generated storage type; ordinary serialization stores it.
TEST(digest_codec, helpers_and_collections_round_trip_all_native_protocols) {
  digest_fixture::record value{};
  static_assert(std::same_as<decltype(value.standard),
      decltype(rohit::make_digest<rohit::digest_algorithm::sha256>(std::string_view{}))>);
  value.raw = {0, 127, 128, 255};
  value.standard = rohit::make_digest<rohit::digest_algorithm::sha256>(std::string_view{"abc"});
  value.custom.fill(0xa5);
  value.hashes = {
      rohit::make_digest<rohit::digest_algorithm::md5>(std::string_view{"abc"}),
      rohit::make_digest<rohit::digest_algorithm::md5>(std::string_view{""})};
  value.values.emplace(7, rohit::make_digest<rohit::digest_algorithm::sha3_256>(std::string_view{"abc"}));
  check_digest_record<schema::json>(value);
  check_digest_record<schema::binary_none>(value);
  check_digest_record<schema::binary_integer>(value);
  check_digest_record<schema::binary_string>(value);
}

// A digest uses the established count-and-bytes wire format and numeric JSON array representation.
TEST(digest_codec, matches_existing_byte_array_contract) {
  check_digest_array_wire<schema::json>();
  check_digest_array_wire<schema::binary_none>();
  check_digest_array_wire<schema::binary_integer>();
  check_digest_array_wire<schema::binary_string>();
  digest_fixture::fixed_blob value{};
  value.value = {0, 128, 255};
  constexpr std::array<std::uint8_t, 4> expected{3, 0, 128, 255};
  const auto binary = digest_bytes<schema::binary_none>(value);
  EXPECT_EQ(binary, (std::vector<std::uint8_t>{expected.begin(), expected.end()}));
  const auto json = digest_bytes<schema::json>(value);
  EXPECT_EQ((std::string_view{reinterpret_cast<const char*>(json.data()), json.size()}),
            "{\"value\":[0,128,255]}");
}

// Both short and oversized digest values fail before a fresh decoded object can replace its owner.
TEST(digest_codec, rejects_wrong_lengths_and_keeps_failed_fresh_decode_transactional) {
  digest_fixture::fixed_blob destination{};
  destination.value.fill(9);
  const auto original = destination.value;
  for (const std::string_view text : {"{\"value\":[1,2]}", "{\"value\":[1,2,3,4]}",
       "{\"value\":[1,2,256]}", "{\"value\":[1,2,-1]}", "{\"value\":[1,2,3]}x"}) {
    SCOPED_TRACE(text);
    const auto input = rohit::make_constant_stream(text.data(), text.size());
    EXPECT_THROW((destination = schema::deserialize_exact<digest_fixture::fixed_blob, schema::json>(input)),
                 rohit::exception::base_parser);
    EXPECT_EQ(destination.value, original);
  }
  for (const auto bytes : {std::vector<std::uint8_t>{2, 1, 2},
                          std::vector<std::uint8_t>{4, 1, 2, 3, 4},
                          std::vector<std::uint8_t>{3, 1, 2, 3, 0}}) {
    const auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
    EXPECT_THROW((destination = schema::deserialize_exact<digest_fixture::fixed_blob, schema::binary_none>(input)),
                 rohit::exception::base_parser);
    EXPECT_EQ(destination.value, original);
  }
}

// Bare digest bytes remain caller-defined while the standard collection limits bound decoding.
TEST(digest_codec, bare_digest_has_no_implicit_algorithm_and_obeys_limits) {
  digest_fixture::variable_blob value{};
  value.value = {42};
  const auto bytes = digest_bytes<schema::binary_none>(value);
  EXPECT_EQ(bytes, (std::vector<std::uint8_t>{1, 42}));
  const auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
  EXPECT_EQ((schema::deserialize_exact<digest_fixture::variable_blob, schema::binary_none>(input).value),
            value.value);
  constexpr std::string_view oversized{"{\"value\":[1,2,3,4]}"};
  const auto limited_input = rohit::make_constant_stream(oversized.data(), oversized.size());
  schema::decode_limits limits{};
  limits.max_collection_elements = 3;
  EXPECT_THROW((static_cast<void>(schema::deserialize_exact<digest_fixture::variable_blob, schema::json>(limited_input, limits))),
               schema::exception::resource_limit);
}
