#include <inferred_arrays.hpp>
#include <rohit/serializer.hpp>
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <exception>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

static_assert(std::is_same_v<decltype(inferred_array_test::record::signature), std::array<char, 6>>);
static_assert(std::is_same_v<decltype(inferred_array_test::record::numbers), std::array<std::uint64_t, 2>>);
static_assert(std::is_same_v<decltype(inferred_array_test::record::dynamic_values), std::vector<std::uint32_t>>);
static_assert(std::is_same_v<decltype(inferred_array_test::concrete_record::values), std::array<std::uint32_t, 3>>);

namespace {
// Verify inferred storage retains normal sequence encoding and exact-cardinality decoding.
template <template <rohit::serializer::serialize_type> class Protocol>
void verify_arrays() {
  inferred_array_test::record value{};
  rohit::string_stream output;
  rohit::serializer::serialize_to<Protocol>(output, value);
  const auto bytes = std::move(output).str();
  auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
  const auto copy = rohit::serializer::deserialize_exact<inferred_array_test::record, Protocol>(input);
  EXPECT_EQ(copy.signature, value.signature);
  EXPECT_EQ(copy.numbers, value.numbers);
  EXPECT_EQ(copy.fractions, value.fractions);
  EXPECT_EQ(copy.separated_numbers, value.separated_numbers);
  EXPECT_EQ(copy.choices, value.choices);
  EXPECT_EQ(copy.names, value.names);
  EXPECT_EQ(copy.children.front().left, value.children.front().left);
  EXPECT_EQ(copy.children.back().right, value.children.back().right);
}
} // namespace

// Deduce decoded bytes rather than source spelling, and retain ordinary dynamic arrays.
TEST(inferred_array, defaults_deduce_sizes_for_text_scalars_enums_and_generic_types) {
  const inferred_array_test::record value{};
  EXPECT_EQ(value.signature, (std::array{'S', 'R', 'L', '\0', static_cast<char>(0xc3), static_cast<char>(0xa9)}));
  EXPECT_EQ(value.numbers.back(), std::numeric_limits<std::uint64_t>::max());
  EXPECT_FLOAT_EQ(value.fractions.front(), 0.1F);
  EXPECT_EQ(value.choices.back(), inferred_array_test::kind::second);
  EXPECT_EQ(value.dynamic_values, (std::vector<std::uint32_t>{1, 2}));
  EXPECT_EQ(value.names.front(), "a,b");
  EXPECT_EQ(value.children.front().left, 1U);
  EXPECT_EQ(value.children.back().right, 4U);
  EXPECT_EQ(inferred_array_test::concrete_record{}.values, (std::array<std::uint32_t, 3>{1, 2, 3}));
}

// Keep inferred arrays interoperable with existing fixed sequence codecs.
TEST(inferred_array, all_native_binary_codecs_preserve_elements) {
  verify_arrays<rohit::serializer::binary_none>();
  verify_arrays<rohit::serializer::binary_integer>();
  verify_arrays<rohit::serializer::binary_string>();
}

// Reject input that disagrees with the deduced contract rather than resizing fixed storage.
TEST(inferred_array, json_rejects_wrong_cardinality) {
  const std::string bytes = "{\"numbers\":[1]}";
  auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
  EXPECT_THROW((static_cast<void>(rohit::serializer::deserialize_exact<inferred_array_test::record, rohit::serializer::json>(input))), std::exception);
}

// Inferred generic arrays retain their exact shape in native JSON.
TEST(inferred_array, generic_json_round_trip_preserves_deduced_cardinality) {
  const inferred_array_test::concrete_record value{};
  rohit::string_stream output;
  rohit::serializer::serialize_to<rohit::serializer::json>(output, value);
  const auto bytes = std::move(output).str();
  EXPECT_EQ(bytes, "{\"values\":[1,2,3]}");
  auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
  const auto copy = rohit::serializer::deserialize_exact<inferred_array_test::concrete_record,
      rohit::serializer::json>(input);
  EXPECT_EQ(copy.values, value.values);
}

// Numeric digit separators do not hide the comma between deduced float elements.
TEST(inferred_array, digit_separators_preserve_element_boundaries) {
  const inferred_array_test::record value{};
  EXPECT_EQ(value.separated_numbers, (std::array<float, 2>{1000.0F, 2.0F}));
}
