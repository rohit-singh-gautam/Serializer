#include <rohit/binary_view.hpp>
#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <gtest/gtest.h>
#include <views.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace codec = rohit::serializer;
namespace view = rohit::serializer::detail;
using uint_codec = view::scalar_view_codec<std::uint16_t>;
using string_codec = view::string_view_codec;
using readonly_array = view::binary_array_view<uint_codec, const std::uint8_t>;
using mutable_array = view::binary_array_view<uint_codec, std::uint8_t>;
using readonly_map = view::binary_map_view<string_codec, string_codec, const std::uint8_t>;
using mutable_map = view::binary_map_view<string_codec, string_codec, std::uint8_t>;

// Detect array mutation without requiring an invalid expression in a concrete test.
template <typename Iterator, typename Value>
concept has_element_setter =
    requires(const Iterator& iterator, const Value& value) { iterator.set(value); };

// Detect map-value or nested-object mutation while keeping keys immutable.
template <typename Iterator, typename Value>
concept has_value_setter =
    requires(Iterator& iterator, const Value& value) { iterator.set_value(value); };

static_assert(std::forward_iterator<readonly_array::iterator>);
static_assert(std::forward_iterator<mutable_array::iterator>);
static_assert(std::forward_iterator<readonly_map::iterator>);
static_assert(std::forward_iterator<mutable_map::iterator>);
static_assert(std::ranges::forward_range<const readonly_array>);
static_assert(std::ranges::forward_range<const mutable_map>);
static_assert(!std::ranges::random_access_range<readonly_array>);
static_assert(!has_element_setter<readonly_array::iterator, std::uint16_t>);
static_assert(has_element_setter<mutable_array::iterator, std::uint16_t>);
static_assert(!has_value_setter<readonly_map::iterator, std::string_view>);
static_assert(has_value_setter<mutable_map::iterator, std::string_view>);
static_assert(!has_element_setter<mutable_map::iterator, std::string_view>);

// Follow the generated mapping contract before exposing an internal collection view.
template <typename Codec, typename Byte, std::size_t Extent>
auto map_collection(std::span<Byte, Extent> bytes, codec::decode_limits limits = {}) {
  const auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
  view::view_scanner scanner{input, limits};
  Codec::scan(scanner);
  scanner.finish();
  return Codec::read(std::span<Byte>{bytes}, limits);
}

// Count entry scans to detect a quadratic regression without relying on timing measurements.
template <typename Codec>
struct counting_codec : Codec {
  inline static std::size_t scans{};
  // Retain production validation while recording each traversed component.
  static void scan(view::view_scanner& scanner) {
    ++scans;
    Codec::scan(scanner);
  }
};

enum class variable_enum : std::uint32_t { first = 1, second = 2, medium = 64, large = 16384 };

// Match generated enum validation for values spanning three different compact widths.
constexpr bool serializer_enum_valid(variable_enum value) {
  return value == variable_enum::first || value == variable_enum::second ||
         value == variable_enum::medium || value == variable_enum::large;
}
} // namespace

// Fixed-width traversal uses direct offsets, independent iterator copies, and reusable boundaries.
TEST(binary_view_iterator, scalar_arrays_support_multiple_passes_and_updates) {
  using counted = counting_codec<uint_codec>;
  using array_codec = view::array_view_codec<counted>;
  std::array<std::uint8_t, 7> bytes{3, 1, 0, 2, 0, 3, 0};
  auto values = map_collection<array_codec>(std::span{bytes});
  const auto reader = map_collection<array_codec>(std::span<const std::uint8_t>{bytes});
  counted::scans = 0;
  auto current = values.begin();
  auto copy = current;
  auto previous = current++;
  EXPECT_EQ(copy, previous);
  EXPECT_NE(copy, current);
  EXPECT_EQ(*copy, 1);
  EXPECT_EQ(*current, 2);
  current.set(std::uint16_t{20});
  EXPECT_EQ(reader.at(1), 20);
  EXPECT_EQ(*++copy, 20);
  EXPECT_EQ(copy, current);
  EXPECT_EQ(counted::scans, 0u);
  EXPECT_EQ(std::distance(values.begin(), values.end()), 3);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 20, 3}));
  EXPECT_EQ(counted::scans, 0u);

  // Iterators retain their span and limits after the lightweight wrapper has been destroyed.
  auto detached = map_collection<array_codec>(std::span{bytes}).begin();
  EXPECT_EQ(*detached, 1);
  EXPECT_EQ(*++detached, 20);
}

// Each variable-width element is located once per pass, even if it is dereferenced repeatedly.
TEST(binary_view_iterator, string_array_traversal_is_linear) {
  using counted = counting_codec<string_codec>;
  using array_codec = view::array_view_codec<counted>;
  const std::vector<std::string> expected{"", "a", std::string(64, 'b'), "tail"};
  rohit::full_stream_auto_alloc output{};
  codec::binary_none<codec::serialize_type::out> encoder{output};
  encoder.serialize_out(expected);
  auto values = map_collection<array_codec>(std::span{output.begin(), output.current_offset()});
  counted::scans = 0;
  const auto end = values.end();
  EXPECT_EQ(counted::scans, 0u);
  std::size_t index{};
  for (auto iterator = values.begin(); iterator != end; ++iterator, ++index) {
    EXPECT_EQ(*iterator, expected[index]);
    EXPECT_EQ(*iterator, expected[index]);
    EXPECT_EQ(counted::scans, index + 1);
    iterator.set(std::string(expected[index].size(), 'x'));
    EXPECT_EQ(*iterator, std::string(expected[index].size(), 'x'));
  }
  EXPECT_EQ(index, expected.size());
  EXPECT_EQ(counted::scans, expected.size());

  auto original = values.begin();
  auto advanced = original;
  ++advanced;
  EXPECT_EQ(*original, "");
  EXPECT_EQ(*advanced, "x");
  EXPECT_NE(original, advanced);
  EXPECT_EQ(++original, advanced);
  const std::vector<std::uint8_t> before(output.begin(), output.begin() + output.current_offset());
  EXPECT_THROW(advanced.set("longer"), std::length_error);
  EXPECT_TRUE(std::ranges::equal(before, std::span{output.begin(), output.current_offset()}));
}

// Variable keys and values share one scan per entry; duplicate keys retain wire order and identity.
TEST(binary_view_iterator, map_pairs_preserve_wire_order_and_mutate_only_values) {
  using counted = counting_codec<string_codec>;
  using map_codec = view::map_view_codec<counted, counted>;
  std::array<std::uint8_t, 12> bytes{3, 1, 'k', 1, 'a', 1, 'k', 2, 'b', 'c', 0, 0};
  auto entries = map_collection<map_codec>(std::span{bytes});
  const auto reader = map_collection<map_codec>(std::span<const std::uint8_t>{bytes});
  counted::scans = 0;
  std::vector<std::pair<std::string_view, std::string_view>> decoded;
  for (const auto [key, value] : entries) {
    decoded.emplace_back(key, value);
  }
  EXPECT_EQ(counted::scans, 2 * entries.size());
  EXPECT_EQ(decoded, (decltype(decoded){{"k", "a"}, {"k", "bc"}, {"", ""}}));

  auto first = entries.begin();
  auto second = first;
  ++second;
  const auto scans = counted::scans;
  second.set_value("BC");
  EXPECT_EQ((*second).first, "k");
  EXPECT_EQ((*second).second, "BC");
  EXPECT_EQ((*first).second, "a");
  EXPECT_EQ(counted::scans, scans);
  EXPECT_EQ(reader.value_at(1), "BC");
  EXPECT_EQ(reader.key_at(1), "k");
  EXPECT_THROW(second.set_value("changed size"), std::length_error);
  EXPECT_EQ((*second).second, "BC");
}

// Map entries also support fixed-width pairs and mixed fixed/variable components.
TEST(binary_view_iterator, fixed_and_mixed_map_entries) {
  std::array<std::uint8_t, 9> fixed{2, 1, 0, 2, 0, 3, 0, 4, 0};
  auto pairs = map_collection<view::map_view_codec<uint_codec, uint_codec>>(std::span{fixed});
  const std::array<std::pair<std::uint16_t, std::uint16_t>, 2> expected{{{1, 2}, {3, 4}}};
  EXPECT_TRUE(std::ranges::equal(pairs, expected));
  auto second = pairs.begin();
  ++second;
  second.set_value(std::uint16_t{40});
  EXPECT_EQ((*second).first, 3);
  EXPECT_EQ((*second).second, 40);

  const std::array<std::uint8_t, 8> mixed{2, 1, 0, 1, 'a', 2, 0, 0};
  auto entries = map_collection<view::map_view_codec<uint_codec, string_codec>>(std::span{mixed});
  std::vector<std::pair<std::uint16_t, std::string_view>> decoded;
  for (const auto entry : entries) {
    decoded.push_back(entry);
  }
  EXPECT_EQ(decoded, (decltype(decoded){{1, "a"}, {2, ""}}));
}

// Empty, singleton, and zero-byte object entries must use element counts to distinguish positions.
TEST(binary_view_iterator, empty_and_zero_byte_elements_have_correct_end_positions) {
  // Use stream storage while exposing only the one-byte count, never spare capacity.
  rohit::full_stream_auto_alloc empty_storage{};
  empty_storage.append(std::uint8_t{0});
  const std::span<const std::uint8_t> empty{empty_storage.begin(), empty_storage.current_offset()};
  auto array = map_collection<view::array_view_codec<string_codec>>(std::span{empty});
  auto map = map_collection<view::map_view_codec<uint_codec, uint_codec>>(std::span{empty});
  EXPECT_EQ(array.begin(), array.end());
  EXPECT_EQ(map.begin(), map.end());
  auto end = array.end();
  EXPECT_THROW(*end, std::out_of_range);
  EXPECT_THROW(++end, std::out_of_range);
  EXPECT_EQ(end, array.end());
  EXPECT_EQ(decltype(end){}, decltype(end){});

  rohit::full_stream_auto_alloc object_storage{};
  object_storage.append(std::uint8_t{3});
  const std::span<const std::uint8_t> objects{object_storage.begin(), object_storage.current_offset()};
  using object_codec = view::array_view_codec<view::object_view_codec<view_test::empty>>;
  auto entries = map_collection<object_codec>(std::span{objects});
  auto iterator = entries.begin();
  const auto first = iterator++;
  EXPECT_NE(first, iterator);
  std::size_t count{};
  for (const auto object : entries) {
    EXPECT_TRUE(object.serialized_bytes().empty());
    ++count;
  }
  EXPECT_EQ(count, 3u);

  const std::array<std::uint8_t, 3> single{1, 7, 0};
  auto scalar = map_collection<view::array_view_codec<uint_codec>>(std::span{single});
  EXPECT_EQ(*scalar.begin(), 7);
  EXPECT_EQ(++scalar.begin(), scalar.end());
}

// Compact widths and accepted overlong count prefixes must remain unchanged during iteration.
TEST(binary_view_iterator, enum_widths_and_mapping_limits_remain_validated) {
  std::array<std::uint8_t, 8> bytes{0x40, 3, 1, 0x40, 0x40, 0x80, 0x40, 0};
  using array_codec = view::array_view_codec<view::scalar_view_codec<variable_enum>>;
  codec::decode_limits limits{};
  limits.max_allocation_bytes = 0;
  limits.max_input_bytes = bytes.size();
  limits.max_collection_elements = 3;
  limits.max_nesting_depth = 1;
  auto values = map_collection<array_codec>(std::span{bytes}, limits);
  EXPECT_TRUE(std::ranges::equal(
      values, std::array{variable_enum::first, variable_enum::medium, variable_enum::large}));
  auto first = values.begin();
  EXPECT_THROW(first.set(variable_enum::medium), std::length_error);
  first.set(variable_enum::second);
  EXPECT_EQ(*first, variable_enum::second);
  EXPECT_EQ(bytes[0], 0x40);
  EXPECT_THROW(values.end().set(variable_enum::first), std::out_of_range);
  --limits.max_input_bytes;
  EXPECT_THROW(map_collection<array_codec>(std::span{bytes}, limits),
               codec::exception::resource_limit);
}

// Complete mapping enforces cumulative budgets before any per-entry iterator scan can begin.
TEST(binary_view_iterator, exact_mapping_budgets_allow_complete_iteration) {
  const std::array<std::uint8_t, 7> bytes{3, 0, 1, 'a', 2, 'b', 'c'};
  using array_codec = view::array_view_codec<string_codec>;
  codec::decode_limits limits{};
  limits.max_allocation_bytes = 0;
  limits.max_input_bytes = bytes.size();
  limits.max_collection_elements = 3;
  limits.max_string_bytes = 2;
  limits.max_nesting_depth = 1;
  // String scans charge their bytes; the array also charges one aggregate and its declared slots.
  limits.max_work_units = bytes.size() + 1 + limits.max_collection_elements;
  auto values = map_collection<array_codec>(std::span{bytes}, limits);
  EXPECT_TRUE(std::ranges::equal(values, std::array<std::string_view, 3>{"", "a", "bc"}));
  --limits.max_work_units;
  EXPECT_THROW(map_collection<array_codec>(std::span{bytes}, limits),
               codec::exception::resource_limit);
}

// Generated nested views retain their own offsets and keep mutable map keys read-only.
TEST(binary_view_iterator, nested_views_keep_mutability_and_temporary_lifetimes) {
  std::array<std::uint8_t, 9> bytes{1, 5, 0, 0, 0, 7, 0, 0, 0};
  using key_codec = view::object_view_codec<view_test::readonly_record>;
  using value_codec = view::object_view_codec<view_test::mutable_record>;
  auto entries = map_collection<view::map_view_codec<key_codec, value_codec>>(std::span{bytes});
  auto [key, value] = *entries.begin();
  static_assert(!has_value_setter<decltype(key), std::uint32_t>);
  static_assert(has_value_setter<decltype(value), std::uint32_t>);
  EXPECT_EQ(key.get_value(), 5u);
  value.set_value(8);
  EXPECT_EQ((*entries.begin()).second.get_value(), 8u);
  EXPECT_EQ((*entries.begin()).first.get_value(), 5u);

  const std::array<std::uint8_t, 4> recursive{2, 1, 0, 0};
  auto children = view_test::recursive::map(recursive).get_children();
  auto first = children.begin();
  auto second = first;
  ++second;
  EXPECT_EQ((*first).get_children().size(), 1u);
  EXPECT_EQ((*second).get_children().size(), 0u);
  EXPECT_EQ(++second, children.end());
  auto grandchildren = (*first).get_children();
  EXPECT_EQ((*grandchildren.begin()).get_children().size(), 0u);
}
