// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <managed_protobuf.hpp>
#include <rohit/managed.hpp>
#include <rohit/protobuf.hpp>
#include <rohit/serializer.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace {
namespace codec = rohit::serializer;
namespace managed = rohit::managed;
using root = managed_protobuf_example::document;
using traits = managed::model_traits<root>;
using store_type = managed::model_store<root>;

// The selected Protobuf codec round-trips the existing separated identity/value wrapper.
template <template <codec::serialize_type> class Protocol>
void round_trip() {
  store_type store{root{42, "A"}, {31, 32}};
  const auto encoded = codec::serialize_as<std::string, Protocol>(*store.read());
  auto copy = codec::deserialize_exact<traits::storage_type, Protocol>(
      rohit::make_constant_stream(encoded.data(), encoded.size()));
  EXPECT_EQ(copy.persistent_id, 1u);
  EXPECT_EQ(traits::payload(copy).number, 42u);
  EXPECT_EQ(traits::payload(copy).title, "A");
  store_type imported{managed::identified_baseline, std::move(copy), {31, 32}, 1};
  EXPECT_EQ(traits::payload(*imported.read()).number, 42u);
  imported.execute_transaction([](auto& transaction) { transaction.root().set_number(99); }).throw_if_failed();
  imported.undo();
  EXPECT_EQ(traits::payload(*imported.read()).number, 42u);
}
} // namespace

// Wrapper field 1 holds the local native identity; field 2 holds the original ordinary payload.
TEST(managed_protobuf, separate_wrapper_has_standard_exact_wire_tags) {
  store_type store{root{42, "A"}, {31, 32}};
  const std::string golden{"\x08\x01\x12\x05\x08\x2a\x12\x01\x41", 9};
  EXPECT_EQ((codec::serialize_as<std::string, codec::protobuf_binary>(*store.read())), golden);
  const std::string ordinary_golden{"\x08\x2a\x12\x01\x41", 5};
  EXPECT_EQ((codec::serialize_as<std::string, codec::protobuf_binary>(root{42, "A"})), ordinary_golden);
}

// Protobuf support for separated storage neither replaces native Save envelopes nor changes history.
TEST(managed_protobuf, binary_json_and_text_round_trip) {
  round_trip<codec::protobuf_binary>();
  round_trip<codec::protojson>();
  round_trip<codec::textproto>();
}
