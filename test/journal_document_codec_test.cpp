// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/managed_journal_document.hpp>

#include <ledger.hpp>
#include <rohit/serializer.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace codec = rohit::serializer;
namespace managed = rohit::managed;
using ledger = ledger_example::ledger;
using traits = managed::model_traits<ledger>;
using bytes = std::vector<std::uint8_t>;

// Encode an owning ledger through a generated protocol selected by the application.
template <template <codec::serialize_type> class Protocol>
bytes encode_ledger(const ledger& value) {
  rohit::full_stream_auto_alloc output;
  value.template serialize_out<Protocol>(output);
  return {output.begin(), output.begin() + output.current_offset()};
}

// Decode exactly one external value, rejecting truncated input and trailing data.
template <template <codec::serialize_type> class Protocol>
ledger decode_ledger(std::span<const std::uint8_t> input) {
  auto stream = rohit::make_constant_full_stream(input.data(), input.size());
  return codec::deserialize_exact<ledger, Protocol>(stream);
}

// Use nonzero root and child identities to qualify both directions of normalization.
ledger identified_ledger() {
  ledger value{"Ledger", {{10, {"Entry", 200}}}};
  value.persistent_id = 19;
  value.entries.at(10).persistent_id = 23;
  return value;
}
} // namespace

// JSON callbacks see values without recovery identities and cannot replace those identities on load.
TEST(journal_document_codec, typed_json_normalizes_root_and_descendant_identities) {
  bool encoded_without_identities{};
  const auto adapter = managed::make_journal_document_codec<ledger>(
      "application/json",
      [&](const ledger& value) {
        encoded_without_identities =
            value.persistent_id == 0 && value.entries.at(10).persistent_id == 0;
        return encode_ledger<codec::json>(value);
      },
      decode_ledger<codec::json>);
  auto value = identified_ledger();
  const auto external = adapter->encode(managed::detail::encode(value));
  EXPECT_EQ(adapter->format, "application/json");
  EXPECT_TRUE(encoded_without_identities);
  EXPECT_EQ(decode_ledger<codec::json>(external).name, value.name);

  const auto normalized = adapter->decode(encode_ledger<codec::json>(value));
  const auto restored = managed::detail::decode<ledger>(normalized, {});
  EXPECT_EQ(restored.persistent_id, 0u);
  EXPECT_EQ(restored.entries.at(10).persistent_id, 0u);
  EXPECT_EQ(restored.entries.at(10).memo, "Entry");
  EXPECT_EQ(restored.entries.at(10).amount_minor_units, 200);
  traits::visit_entities(value, [](auto& identity, auto) { identity = 0; });
  EXPECT_EQ(normalized, managed::detail::encode(value));
}

// Format selection remains application-defined rather than restricted to JSON document exports.
TEST(journal_document_codec, accepts_an_independently_selected_external_protocol) {
  const auto adapter = managed::make_journal_document_codec<ledger>(
      "application/x-serializer-binary-string", encode_ledger<codec::binary_string>,
      decode_ledger<codec::binary_string>);
  auto value = identified_ledger();
  const auto external = adapter->encode(managed::detail::encode(value));
  traits::visit_entities(value, [](auto& identity, auto) { identity = 0; });
  EXPECT_EQ(external, encode_ledger<codec::binary_string>(value));
  EXPECT_EQ(adapter->decode(external), managed::detail::encode(value));
}

// Bounds apply before invoking the encoder and while materializing decoded native storage.
TEST(journal_document_codec, bounds_native_conversions_and_rejects_trailing_input) {
  bool encoder_called{};
  codec::decode_limits limits;
  limits.max_input_bytes = 64;
  const auto adapter = managed::make_journal_document_codec<ledger>(
      "application/json",
      [&](const ledger& value) {
        encoder_called = true;
        return encode_ledger<codec::json>(value);
      },
      decode_ledger<codec::json>, limits);
  ledger oversized{std::string(limits.max_input_bytes * 2, 'x')};
  EXPECT_THROW(adapter->encode(managed::detail::encode(oversized)),
               codec::exception::resource_limit);
  EXPECT_FALSE(encoder_called);
  EXPECT_THROW(adapter->decode(encode_ledger<codec::json>(oversized)),
               rohit::exception::stream_overflow_exception);

  const auto unbounded = managed::make_journal_document_codec<ledger>(
      "application/json", encode_ledger<codec::json>, decode_ledger<codec::json>);
  auto invalid = managed::detail::encode(identified_ledger());
  invalid.push_back(0);
  EXPECT_THROW(unbounded->encode(invalid), std::exception);
}

// Application codec failures propagate without publishing partial native output.
TEST(journal_document_codec, propagates_callback_failures_and_rejects_empty_format) {
  const auto adapter = managed::make_journal_document_codec<ledger>(
      "application/custom",
      [](const ledger&) -> bytes { throw std::runtime_error{"Encoding failed"}; },
      [](std::span<const std::uint8_t>) -> ledger {
        throw std::invalid_argument{"Decoding failed"};
      });
  EXPECT_THROW(adapter->encode(managed::detail::encode(identified_ledger())), std::runtime_error);
  EXPECT_THROW(adapter->decode({}), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(managed::make_journal_document_codec<ledger>(
                   "", encode_ledger<codec::json>, decode_ledger<codec::json>)),
               std::invalid_argument);
}
