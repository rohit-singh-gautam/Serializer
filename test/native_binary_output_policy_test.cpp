// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <native_output_policy.hpp>
#include <rohit/serializer.hpp>

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace codec = rohit::serializer;
using record = native_output_policy::record;
using keys = codec::serialize_key_type;
using normalization = codec::binary_scalar_normalization;

// Observe output reservations to keep normalization's floating collections on the bulk path.
class reservation_stream : public rohit::full_stream_auto_alloc {
public:
  std::size_t reservations{};
  // Forward the production reservation policy after recording one request.
  void reserve(std::size_t bytes) override {
    ++reservations;
    rohit::full_stream_auto_alloc::reserve(bytes);
  }
};

// Keep negative zeros in fixed batches, both array kinds, nested optionals, and map payloads.
record sample() {
  record value{};
  value.first = -0.0F;
  value.second = -0.0;
  value.count = 17;
  value.values = {-0.0, 0.0, 2.5};
  value.fixed_values = {-0.0F, 0.0F, -2.5F};
  value.nested = std::vector<std::optional<double>>{-0.0, std::nullopt, 1.25};
  value.labels = {{"zero", -0.0}, {"absent", std::nullopt}};
  value.status = native_output_policy::state::active;
  value.text = "valid text";
  return value;
}

// Borrow exactly the written bytes without counting spare output capacity.
std::string_view written(const rohit::full_stream& output) {
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}

// Check keyed traversal parity, exact limits, default preservation, and normalized readback.
template <keys Keys, std::endian Endian>
void verify_policy() {
  const auto value = sample();
  rohit::full_stream_auto_alloc normal_output;
  codec::binary<codec::serialize_type::out, Keys, Endian> normal{normal_output};
  value.serialize_out(normal);
  const auto bytes = normal_output.current_offset();
  EXPECT_EQ((codec::binary_size<Keys, Endian>(value, bytes)), bytes);
  EXPECT_THROW((static_cast<void>(codec::binary_size<Keys, Endian>(value, bytes - 1))),
               rohit::exception::stream_overflow_exception);

  rohit::full_stream_auto_alloc normalized_output;
  codec::binary<codec::serialize_type::out, Keys, Endian, rohit::stream,
                codec::binary_text_validation::strict, codec::read_policy::strict,
                normalization::positive_zero> normalized{normalized_output};
  value.serialize_out(normalized);
  EXPECT_EQ(normalized_output.current_offset(), bytes);
  EXPECT_NE(written(normal_output), written(normalized_output));
  record restored{};
  const auto normalized_bytes = written(normalized_output);
  auto input = rohit::make_constant_stream(normalized_bytes.data(), normalized_bytes.size());
  codec::binary<codec::serialize_type::in, Keys, Endian, decltype(input)> decoder{input};
  restored.serialize_in(decoder);
  decoder.finish();
  EXPECT_EQ(std::bit_cast<std::uint32_t>(restored.first), 0u);
  EXPECT_EQ(std::bit_cast<std::uint64_t>(restored.second), 0u);
  EXPECT_EQ(std::bit_cast<std::uint64_t>(restored.values.front()), 0u);
  EXPECT_EQ(std::bit_cast<std::uint32_t>(restored.fixed_values.front()), 0u);
  ASSERT_TRUE(restored.nested);
  ASSERT_TRUE(restored.nested->front());
  EXPECT_EQ(std::bit_cast<std::uint64_t>(*restored.nested->front()), 0u);
  EXPECT_EQ(std::bit_cast<std::uint64_t>(*restored.labels.at("zero")), 0u);
  EXPECT_EQ(restored.values.back(), 2.5);
  EXPECT_EQ(restored.fixed_values.back(), -2.5F);
  EXPECT_EQ(std::bit_cast<std::uint64_t>(value.second), std::uint64_t{1} << 63);
}

// Compare synchronous borrowed traversal with its owning wire equivalent under identical policies.
template <keys Keys, std::endian Endian, normalization Policy, typename Owned, typename Borrowed>
void verify_borrowed_equivalent(const Owned& owned, const Borrowed& borrowed) {
  using encoder = codec::binary<codec::serialize_type::out, Keys, Endian, rohit::stream,
      codec::binary_text_validation::strict, codec::read_policy::strict, Policy>;
  rohit::full_stream_auto_alloc owning_output;
  encoder owning_encoder{owning_output};
  owning_encoder.serialize_out(owned);
  rohit::full_stream_auto_alloc borrowed_output;
  encoder borrowed_encoder{borrowed_output};
  borrowed_encoder.serialize_out(borrowed);
  EXPECT_EQ(written(borrowed_output), written(owning_output));
  const auto bytes = borrowed_output.current_offset();
  EXPECT_EQ((codec::binary_size<Keys, Endian, codec::binary_text_validation::strict, Policy>(
      borrowed, bytes)), bytes);
  ASSERT_NE(bytes, 0u);
  EXPECT_THROW((static_cast<void>(codec::binary_size<Keys, Endian, codec::binary_text_validation::strict, Policy>(
      borrowed, bytes - 1))), rohit::exception::stream_overflow_exception);
}

// Cover fixed/dynamic spans and transparent references throughout the native owning value graph.
template <keys Keys, std::endian Endian, normalization Policy>
void verify_borrowed_values() {
  auto value = sample();
  const std::span<const double> dynamic_values{value.values};
  verify_borrowed_equivalent<Keys, Endian, Policy>(value.values, dynamic_values);
  verify_borrowed_equivalent<Keys, Endian, Policy>(value.values, std::cref(dynamic_values));
  verify_borrowed_equivalent<Keys, Endian, Policy>(value.fixed_values,
      std::span<const float, 3>{value.fixed_values});
  verify_borrowed_equivalent<Keys, Endian, Policy>(value.values, std::span<double>{value.values});
  verify_borrowed_equivalent<Keys, Endian, Policy>(value.second, std::ref(value.second));
  verify_borrowed_equivalent<Keys, Endian, Policy>(value.text, std::cref(value.text));
  verify_borrowed_equivalent<Keys, Endian, Policy>(value, std::ref(value));
  verify_borrowed_equivalent<Keys, Endian, Policy>(value, std::cref(value));
  const std::array flags{true, false, true};
  verify_borrowed_equivalent<Keys, Endian, Policy>(flags, std::span{flags});

  const std::array<record, 2> records{sample(), sample()};
  const std::span<const record, 2> record_span{records};
  verify_borrowed_equivalent<Keys, Endian, Policy>(records, record_span);
  verify_borrowed_equivalent<Keys, Endian, Policy>(records, std::span<const record>{records});
  verify_borrowed_equivalent<Keys, Endian, Policy>(std::optional{records}, std::optional{record_span});
  const std::array references{std::cref(records[0]), std::cref(records[1])};
  verify_borrowed_equivalent<Keys, Endian, Policy>(records, std::span{references});

  const std::map<std::string, record> owning_records{{"first", records[0]}, {"second", records[1]}};
  const std::map<std::string, std::reference_wrapper<const record>> borrowed_records{
      {"first", std::cref(records[0])}, {"second", std::cref(records[1])}};
  verify_borrowed_equivalent<Keys, Endian, Policy>(owning_records, borrowed_records);
  const std::map<std::string, std::optional<std::vector<double>>> owning_optional{
      {"present", value.values}, {"absent", std::nullopt}};
  const std::map<std::string, std::optional<std::span<const double>>> borrowed_optional{
      {"present", dynamic_values}, {"absent", std::nullopt}};
  verify_borrowed_equivalent<Keys, Endian, Policy>(owning_optional, borrowed_optional);
  const std::array<std::vector<double>, 2> owning_nested{value.values, std::vector<double>{}};
  const std::array<std::span<const double>, 2> borrowed_nested{dynamic_values, std::span<const double>{}};
  verify_borrowed_equivalent<Keys, Endian, Policy>(owning_nested, borrowed_nested);
  EXPECT_EQ(std::bit_cast<std::uint64_t>(value.second), std::uint64_t{1} << 63);
  EXPECT_EQ(std::bit_cast<std::uint64_t>(value.values.front()), std::uint64_t{1} << 63);
  EXPECT_EQ(std::bit_cast<std::uint32_t>(value.fixed_values.front()), std::uint32_t{1} << 31);
  EXPECT_EQ(std::bit_cast<std::uint64_t>(records.front().second), std::uint64_t{1} << 63);
}

// A source with deleted copy/move operations makes accidental borrowed element copying ill-formed.
class borrow_only_scalar {
  double value_;
public:
  // Bind the immutable source scalar without creating owned serialization storage.
  explicit borrow_only_scalar(double value) : value_{value} {}
  borrow_only_scalar(const borrow_only_scalar&) = delete;
  borrow_only_scalar(borrow_only_scalar&&) = delete;
  // Encode the referent directly so arrays can visit it by const reference.
  template <typename Protocol>
  void serialize_out(Protocol& protocol) const { protocol.serialize_out(value_); }
};

// Exercise reference forwarding through the generated schema-native optional enum hook.
template <bool Borrow>
struct named_optional_source {
  const std::optional<native_output_policy::state>& value;
  // Borrow schema context only for this call and preserve the named optional enum representation.
  template <typename Protocol>
  void serialize_out(Protocol& protocol) const {
    const auto context = protocol.schema_values();
    if constexpr (Borrow) { protocol.serialize_out_named_optional(std::cref(value)); }
    else { protocol.serialize_out_named_optional(value); }
  }
};

// Keep the named field context through optional layers containing borrowed enum referents.
template <typename Value>
struct named_borrowed_optional_source {
  const Value& value;
  // Recursively borrow the caller's wrapped value for one schema-native output operation.
  template <typename Protocol>
  void serialize_out(Protocol& protocol) const {
    const auto context = protocol.schema_values();
    protocol.serialize_out_named_optional(value);
  }
};
} // namespace

// Exercise generated fixed batches and production field traversal under all native output modes.
TEST(native_binary_output_policy, generated_counting_and_zero_normalization) {
  verify_policy<keys::none, std::endian::little>();
  verify_policy<keys::integer, std::endian::little>();
  verify_policy<keys::string, std::endian::little>();
  verify_policy<keys::none, std::endian::big>();
  verify_policy<keys::integer, std::endian::big>();
  verify_policy<keys::string, std::endian::big>();
  const auto value = sample();
  EXPECT_EQ(codec::binary_integer_size(value), codec::binary_size<keys::integer>(value));
  EXPECT_EQ(codec::binary_string_size(value), codec::binary_size<keys::string>(value));
}

// Counting preserves semantic validation and atomic failed batches while consuming no storage.
TEST(native_binary_output_policy, counter_bounds_and_validation) {
  static_assert(!rohit::type_check::output_buffer<codec::binary_count_stream>);
  codec::binary_count_stream output{4};
  codec::binary_integer<codec::serialize_type::out, codec::binary_count_stream> encoder{output};
  EXPECT_THROW((encoder.struct_serialize_out_fixed(std::pair{1u, std::uint32_t{2}},
                                                  std::pair{2u, std::uint32_t{3}})),
               rohit::exception::stream_overflow_exception);
  EXPECT_EQ(output.size(), 0u);
  EXPECT_THROW((encoder.struct_serialize_out(std::pair{0u, std::uint32_t{2}})),
               std::invalid_argument);
  EXPECT_EQ(output.size(), 0u);
  auto invalid = sample();
  invalid.text = std::string{1, static_cast<char>(0xff)};
  EXPECT_THROW(static_cast<void>(codec::binary_string_size(invalid)), std::exception);
}

// Retain a single payload reservation for a long normalized array and preserve nonzero bit patterns.
TEST(native_binary_output_policy, bulk_normalization_preserves_special_values) {
  reservation_stream output;
  codec::binary_none<codec::serialize_type::out, reservation_stream,
                     codec::binary_text_validation::strict, codec::read_policy::strict,
                     normalization::positive_zero> encoder{output};
  const auto nan_bits = std::uint64_t{0x7ff8123456789abc};
  std::vector<double> values(1024, -0.0);
  values[1] = std::bit_cast<double>(nan_bits);
  values[2] = std::numeric_limits<double>::infinity();
  encoder.serialize_out(values);
  EXPECT_EQ(output.reservations, 2u);
  std::vector<double> restored;
  const auto bytes = written(output);
  auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
  codec::binary_none<codec::serialize_type::in, decltype(input)> decoder{input};
  decoder.serialize_in(restored);
  EXPECT_EQ(std::bit_cast<std::uint64_t>(restored.front()), 0u);
  EXPECT_EQ(std::bit_cast<std::uint64_t>(restored[1]), nan_bits);
  EXPECT_EQ(restored[2], std::numeric_limits<double>::infinity());
}

// Borrowed native output and counting retain every key, endian and scalar policy combination.
TEST(native_binary_output_policy, borrowed_spans_and_references) {
  verify_borrowed_values<keys::none, std::endian::little, normalization::preserve>();
  verify_borrowed_values<keys::integer, std::endian::little, normalization::preserve>();
  verify_borrowed_values<keys::string, std::endian::little, normalization::preserve>();
  verify_borrowed_values<keys::none, std::endian::big, normalization::preserve>();
  verify_borrowed_values<keys::integer, std::endian::big, normalization::preserve>();
  verify_borrowed_values<keys::string, std::endian::big, normalization::preserve>();
  verify_borrowed_values<keys::none, std::endian::little, normalization::positive_zero>();
  verify_borrowed_values<keys::integer, std::endian::little, normalization::positive_zero>();
  verify_borrowed_values<keys::string, std::endian::little, normalization::positive_zero>();
  verify_borrowed_values<keys::none, std::endian::big, normalization::positive_zero>();
  verify_borrowed_values<keys::integer, std::endian::big, normalization::positive_zero>();
  verify_borrowed_values<keys::string, std::endian::big, normalization::positive_zero>();
}

// Enforce no-copy borrowing, named enum transparency, and one bulk span payload reservation.
TEST(native_binary_output_policy, borrowed_lifetimes_and_bulk_paths) {
  const std::array<borrow_only_scalar, 2> source{borrow_only_scalar{-0.0}, borrow_only_scalar{2.5}};
  verify_borrowed_equivalent<keys::integer, std::endian::big, normalization::positive_zero>(
      std::array{-0.0, 2.5}, std::span{source});
  verify_borrowed_equivalent<keys::string, std::endian::little, normalization::preserve>(
      -0.0, std::cref(source.front()));
  const std::optional state{native_output_policy::state::active};
  verify_borrowed_equivalent<keys::string, std::endian::little, normalization::preserve>(
      named_optional_source<false>{state}, named_optional_source<true>{state});
  const auto ordinal = native_output_policy::state::active;
  const std::optional borrowed_state{std::cref(ordinal)};
  verify_borrowed_equivalent<keys::string, std::endian::little, normalization::preserve>(
      named_optional_source<false>{state}, named_borrowed_optional_source{borrowed_state});
  const std::optional<std::optional<native_output_policy::state>> nested_state{state};
  const std::optional<std::optional<std::reference_wrapper<const native_output_policy::state>>>
      nested_borrowed_state{borrowed_state};
  verify_borrowed_equivalent<keys::string, std::endian::big, normalization::preserve>(
      named_borrowed_optional_source{nested_state}, named_borrowed_optional_source{nested_borrowed_state});
  reservation_stream output;
  codec::binary_none<codec::serialize_type::out, reservation_stream,
      codec::binary_text_validation::strict, codec::read_policy::strict,
      normalization::positive_zero> encoder{output};
  std::array<double, 1024> zeros{};
  zeros.fill(-0.0);
  encoder.serialize_out(std::span{zeros});
  EXPECT_EQ(output.reservations, 2u);
  EXPECT_EQ(codec::binary_size<keys::none>(std::span{zeros}), output.current_offset());
  EXPECT_EQ(std::bit_cast<std::uint64_t>(zeros.front()), std::uint64_t{1} << 63);
  const std::string invalid{1, static_cast<char>(0xff)};
  EXPECT_THROW(static_cast<void>(codec::binary_string_size(std::cref(invalid))), std::exception);
}
