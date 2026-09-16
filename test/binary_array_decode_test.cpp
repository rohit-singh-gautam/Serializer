#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {
namespace codec = rohit::serializer;

constexpr auto sample_values =
    std::to_array<std::uint16_t>({0, 1, 2, 255, 256, 257, 1024, 32768, 65535});
constexpr std::size_t count_prefix_bytes = 1;
constexpr auto payload_bytes = sample_values.size() * sizeof(sample_values.front());
constexpr auto encoded_bytes = count_prefix_bytes + payload_bytes;
// One vector value, one aggregate, count bytes, declared slots, element values, and payload bytes.
constexpr auto array_work_units = 2 + count_prefix_bytes + 2 * sample_values.size() + payload_bytes;
constexpr auto preflight_work_units = array_work_units - sample_values.size();
static_assert(sample_values.size() <= codec::constants::variable_one_byte_max);

// Encode through scalar writes so the bulk decoder cannot hide a matching bulk encoder defect.
template <codec::serialize_key_type Keys, std::endian Endian>
void write_sample(rohit::full_stream& output) {
  codec::binary<codec::serialize_type::out, Keys, Endian> encoder{output};
  encoder.serialize_out_variable(sample_values.size());
  for (const auto value : sample_values) {
    encoder.serialize_out(value);
  }
}

// Cover every work boundary, pre-allocation rejection, and cumulative accounting in one session.
template <codec::serialize_key_type Keys, std::endian Endian>
void check_array_limits() {
  using protocol = codec::binary<codec::serialize_type::in, Keys, Endian>;
  rohit::full_stream_auto_alloc output{};
  write_sample<Keys, Endian>(output);
  ASSERT_EQ(output.current_offset(), encoded_bytes);
  const std::vector<std::uint16_t> original{42, 43};

  for (std::size_t budget = 0; budget <= array_work_units + 1; ++budget) {
    SCOPED_TRACE(budget);
    const auto input = rohit::make_constant_full_stream(output.begin(), encoded_bytes);
    codec::decode_limits limits{};
    limits.max_work_units = budget;
    protocol decoder{input, limits};
    auto decoded = original;
    if (budget < array_work_units) {
      EXPECT_THROW(decoder.serialize_in(decoded), codec::exception::resource_limit);
    } else {
      EXPECT_NO_THROW(decoder.serialize_in(decoded));
      decoder.finish();
    }
    if (budget < preflight_work_units) {
      EXPECT_EQ(decoded, original);
      EXPECT_EQ(input.current_offset(), budget < 2 + count_prefix_bytes ? 0 : count_prefix_bytes);
    } else {
      constexpr auto initial_work_units = 2 + count_prefix_bytes + sample_values.size();
      const auto completed =
          std::min(sample_values.size(),
                   (budget - initial_work_units) / (1 + sizeof(sample_values.front())));
      ASSERT_EQ(decoded.size(), completed);
      EXPECT_TRUE(std::equal(decoded.begin(), decoded.end(), sample_values.begin()));
      EXPECT_EQ(input.current_offset(),
                count_prefix_bytes + completed * sizeof(sample_values.front()));
    }
  }

  // Short input and input-byte budgets must fail before clearing or growing the destination.
  for (std::size_t size = 0; size < encoded_bytes; ++size) {
    const auto truncated = rohit::make_constant_full_stream(output.begin(), size);
    protocol truncated_decoder{truncated};
    auto decoded = original;
    EXPECT_THROW(truncated_decoder.serialize_in(decoded), codec::exception::bad_input_data);
    EXPECT_EQ(decoded, original);
    EXPECT_EQ(truncated.current_offset(), size == 0 ? 0 : count_prefix_bytes);

    const auto input = rohit::make_constant_full_stream(output.begin(), encoded_bytes);
    codec::decode_limits limits{};
    limits.max_input_bytes = size;
    protocol limited_decoder{input, limits};
    EXPECT_THROW(limited_decoder.serialize_in(decoded), codec::exception::resource_limit);
    EXPECT_EQ(decoded, original);
    EXPECT_EQ(input.current_offset(), size == 0 ? 0 : count_prefix_bytes);
  }

  std::array<codec::decode_limits, 3> rejecting_limits{};
  rejecting_limits[0].max_collection_elements = sample_values.size() - 1;
  rejecting_limits[1].max_allocation_bytes = payload_bytes - 1;
  rejecting_limits[2].max_nesting_depth = 0;
  for (const auto& limits : rejecting_limits) {
    const auto input = rohit::make_constant_full_stream(output.begin(), encoded_bytes);
    protocol decoder{input, limits};
    auto decoded = original;
    decoded.reserve(sample_values.size());
    const auto* storage = decoded.data();
    EXPECT_THROW(decoder.serialize_in(decoded), codec::exception::resource_limit);
    EXPECT_EQ(decoded, original);
    EXPECT_EQ(decoded.data(), storage);
    EXPECT_EQ(input.current_offset(), limits.max_nesting_depth == 0 ? 0 : count_prefix_bytes);
  }

  write_sample<Keys, Endian>(output);
  std::array<codec::decode_limits, 3> cumulative_limits{};
  cumulative_limits[0].max_input_bytes = 2 * encoded_bytes - 1;
  cumulative_limits[1].max_allocation_bytes = 2 * payload_bytes - 1;
  cumulative_limits[2].max_work_units = 2 * array_work_units - 1;
  for (std::size_t index = 0; index < cumulative_limits.size(); ++index) {
    const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
    protocol decoder{input, cumulative_limits[index]};
    std::vector<std::uint16_t> decoded;
    decoder.serialize_in(decoded);
    ASSERT_EQ(decoded.size(), sample_values.size());
    EXPECT_TRUE(std::equal(decoded.begin(), decoded.end(), sample_values.begin()));
    EXPECT_THROW(decoder.serialize_in(decoded), codec::exception::resource_limit);
    const auto completed = index == 2 ? sample_values.size() - 1 : 0;
    EXPECT_EQ(input.current_offset(),
              encoded_bytes + count_prefix_bytes + completed * sizeof(sample_values.front()));
    EXPECT_EQ(decoded.size(), index == 2 ? completed : sample_values.size());
    EXPECT_TRUE(std::equal(decoded.begin(), decoded.end(), sample_values.begin()));
  }

  // The exact combined limits allow two arrays, including reuse of existing vector storage.
  const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
  codec::decode_limits exact_limits{};
  exact_limits.max_input_bytes = 2 * encoded_bytes;
  exact_limits.max_allocation_bytes = 2 * payload_bytes;
  exact_limits.max_work_units = 2 * array_work_units;
  exact_limits.max_collection_elements = sample_values.size();
  exact_limits.max_nesting_depth = 1;
  protocol decoder{input, exact_limits};
  std::vector<std::uint16_t> decoded;
  decoder.serialize_in(decoded);
  const auto* storage = decoded.data();
  decoder.serialize_in(decoded);
  decoder.finish();
  EXPECT_EQ(decoded.data(), storage);
  ASSERT_EQ(decoded.size(), sample_values.size());
  EXPECT_TRUE(std::equal(decoded.begin(), decoded.end(), sample_values.begin()));
}

enum class sample_enum : std::uint8_t { first = 1, last = 3 };

// Model a generated enum's validation hook so invalid elements must take the scalar path.
constexpr bool serializer_enum_valid(sample_enum value) {
  return value == sample_enum::first || value == sample_enum::last;
}
} // namespace

// All binary protocols share the optimized decoder and its original limit/failure semantics.
TEST(binary_array_decode, preserves_resource_limits_and_partial_failures) {
  check_array_limits<codec::serialize_key_type::none, std::endian::little>();
  check_array_limits<codec::serialize_key_type::integer, std::endian::little>();
  check_array_limits<codec::serialize_key_type::string, std::endian::little>();
  check_array_limits<codec::serialize_key_type::none, std::endian::big>();
  check_array_limits<codec::serialize_key_type::integer, std::endian::big>();
  check_array_limits<codec::serialize_key_type::string, std::endian::big>();
}

// Empty arrays consume only the prefix and aggregate work, with no storage or element charges.
TEST(binary_array_decode, clears_empty_arrays_with_exact_limits) {
  const std::uint8_t empty_count{};
  const auto input = rohit::make_constant_full_stream(&empty_count, sizeof(empty_count));
  codec::decode_limits limits{};
  limits.max_input_bytes = sizeof(empty_count);
  limits.max_work_units = 2 + sizeof(empty_count);
  limits.max_collection_elements = 0;
  limits.max_allocation_bytes = 0;
  limits.max_nesting_depth = 1;
  codec::binary_none<codec::serialize_type::in> decoder{input, limits};
  std::vector<std::uint64_t> decoded{42};
  const auto capacity = decoded.capacity();
  decoder.serialize_in(decoded);
  decoder.finish();
  EXPECT_TRUE(decoded.empty());
  EXPECT_EQ(decoded.capacity(), capacity);
}

// Booleans and enums require validation of each element even though their C++ storage is small.
TEST(binary_array_decode, retains_boolean_and_enum_validation) {
  using protocol = codec::binary_none<codec::serialize_type::in>;
  const auto boolean_bytes = std::to_array<std::uint8_t>({3, 1, 2, 0});
  const auto boolean_input =
      rohit::make_constant_full_stream(boolean_bytes.data(), boolean_bytes.size());
  protocol boolean_decoder{boolean_input};
  std::vector<bool> booleans{false, false};
  EXPECT_THROW(boolean_decoder.serialize_in(booleans), codec::exception::bad_input_data);
  EXPECT_EQ(booleans, (std::vector<bool>{true}));
  EXPECT_EQ(boolean_input.current_offset(), 2u);

  const auto enum_bytes = std::to_array<std::uint8_t>({3, 1, 2, 3});
  const auto enum_input = rohit::make_constant_full_stream(enum_bytes.data(), enum_bytes.size());
  protocol enum_decoder{enum_input};
  std::vector<sample_enum> enums{sample_enum::last};
  EXPECT_THROW(enum_decoder.serialize_in(enums), codec::exception::bad_input_data);
  EXPECT_EQ(enums, (std::vector<sample_enum>{sample_enum::first}));
  EXPECT_EQ(enum_input.current_offset(), 3u);
}
