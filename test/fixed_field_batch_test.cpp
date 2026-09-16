#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <fixed_fields.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace codec = rohit::serializer;

// Read exactly the committed bytes, independent of spare output capacity.
std::string_view written(const rohit::full_stream& output) {
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}

// Count real stream-policy invocations, including reservations during growth.
class counting_stream : public rohit::full_stream_auto_alloc {
public:
  using rohit::full_stream_auto_alloc::full_stream_auto_alloc;
  std::size_t reservations{};
  // Forward every reservation to the production stream after recording the call.
  void reserve(std::size_t bytes) override {
    ++reservations;
    rohit::full_stream_auto_alloc::reserve(bytes);
  }
};

// Reject a batch despite available storage to ensure custom reservation policies remain authoritative.
class rejecting_stream : public rohit::full_stream_auto_alloc {
public:
  std::size_t reservations{};
  // Record and reject the operation before its cursor or bytes can change.
  void reserve(std::size_t) override {
    ++reservations;
    throw rohit::exception::stream_overflow_exception{};
  }
};

// Observe generated input grouping while executing the production positional batch reader.
class counting_input : public codec::binary_none<codec::serialize_type::in> {
  using base = codec::binary_none<codec::serialize_type::in>;

public:
  using base::base;
  std::size_t batches{};
  // Count a generated group and retain its normal preflight, validation, and accounting.
  template <codec::detail::binary_fixed_scalar... Values>
  void serialize_in_fixed(Values&... values) {
    ++batches;
    base::serialize_in_fixed(values...);
  }
};

// Hide the optional batch hook to exercise generated legacy/custom-protocol output calls.
template <codec::serialize_key_type Keys, std::endian Endian>
class scalar_output : public codec::binary<codec::serialize_type::out, Keys, Endian> {
  using base = codec::binary<codec::serialize_type::out, Keys, Endian>;

public:
  using base::base;
  // This protocol deliberately implements only the original field-writing API.
  template <typename... Values>
  void struct_serialize_out_fixed(Values...) = delete;
};

// Hide the optional positional batch hook while retaining the production scalar reader and budgets.
template <std::endian Endian>
class scalar_input
    : public codec::binary<codec::serialize_type::in, codec::serialize_key_type::none, Endian> {
  using base = codec::binary<codec::serialize_type::in, codec::serialize_key_type::none, Endian>;

public:
  using base::base;
  // Require the generator's scalar fallback for every group of fields.
  template <typename... Values>
  void serialize_in_fixed(Values&...) = delete;
};

// Choose values with distinctive bytes, signed bits, a Boolean, and both floating-point widths.
fixed_fields::scalars sample() {
  fixed_fields::scalars value{};
  value.marker = 0xab;
  value.delta = -2;
  value.sequence = 0x12345678;
  value.ratio = 1.0f;
  value.enabled = true;
  value.reading = -2.0;
  value.suffix = 'Z';
  return value;
}

// Cover every binary key mode, including compact IDs of all four prefix widths.
template <codec::serialize_key_type Keys, std::endian Endian>
void check_generated_output() {
  const auto value = sample();
  counting_stream output{1};
  codec::binary<codec::serialize_type::out, Keys, Endian> encoder{output};
  value.serialize_out(encoder);
  counting_stream reference{};
  scalar_output<Keys, Endian> scalar{reference};
  value.serialize_out(scalar);
  EXPECT_EQ(written(output), written(reference));
  EXPECT_EQ(output.reservations, Keys == codec::serialize_key_type::none ? 1u : 2u);
  EXPECT_GT(reference.reservations, output.reservations);

  // Place exact-size input at an odd address and preserve every field bit on decode/re-encode.
  std::vector<std::uint8_t> unaligned(output.current_offset() + 1);
  std::copy_n(output.begin(), output.current_offset(), unaligned.begin() + 1);
  const auto input =
      rohit::make_constant_full_stream(unaligned.data() + 1, output.current_offset());
  codec::binary<codec::serialize_type::in, Keys, Endian> decoder{input};
  fixed_fields::scalars decoded{};
  decoded.serialize_in(decoder);
  decoder.finish();
  rohit::full_stream_auto_alloc round_trip{};
  scalar_output<Keys, Endian> checked{round_trip};
  decoded.serialize_out(checked);
  EXPECT_EQ(written(round_trip), written(reference));
}

// Independently fixed positional bytes guard against matching mistakes in the two codec paths.
template <std::endian Endian>
void check_golden_bytes() {
  constexpr auto little =
      std::to_array<std::uint8_t>({0xab, 0xfe, 0xff, 0x78, 0x56, 0x34, 0x12, 0, 0,    0x80, 0x3f,
                                   1,    0,    0,    0,    0,    0,    0,    0, 0xc0, 'Z'});
  constexpr auto big =
      std::to_array<std::uint8_t>({0xab, 0xff, 0xfe, 0x12, 0x34, 0x56, 0x78, 0x3f, 0x80, 0,  0,
                                   1,    0xc0, 0,    0,    0,    0,    0,    0,    0,    'Z'});
  const auto& expected = Endian == std::endian::little ? little : big;
  rohit::full_stream_auto_alloc output{};
  codec::binary<codec::serialize_type::out, codec::serialize_key_type::none, Endian> encoder{
      output};
  sample().serialize_out(encoder);
  ASSERT_EQ(output.current_offset(), expected.size());
  EXPECT_TRUE(std::equal(expected.begin(), expected.end(), output.begin()));
}

struct read_result {
  fixed_fields::scalars value{};
  std::uint32_t following_value{99};
  std::size_t consumed{};
  std::string error{};
};

// Capture exact diagnostics, cursor position, and partial destination values for a decoder variant.
template <typename Decoder>
read_result read_case(std::span<const std::uint8_t> bytes, codec::decode_limits limits) {
  read_result result{};
  const auto input = rohit::make_constant_full_stream(bytes.data(), bytes.size());
  Decoder decoder{input, limits};
  try {
    result.value.serialize_in(decoder);
    decoder.serialize_in(result.following_value);
    decoder.finish();
  } catch (const rohit::exception::base_parser& error) {
    result.error = error.what();
  }
  result.consumed = input.current_offset();
  return result;
}

// Compare all logical state using the original scalar writer, including floating-point bits.
template <std::endian Endian>
void compare_readers(std::span<const std::uint8_t> bytes, codec::decode_limits limits = {}) {
  using batch_input =
      codec::binary<codec::serialize_type::in, codec::serialize_key_type::none, Endian>;
  const auto batched = read_case<batch_input>(bytes, limits);
  const auto scalar = read_case<scalar_input<Endian>>(bytes, limits);
  EXPECT_EQ(batched.error, scalar.error);
  EXPECT_EQ(batched.consumed, scalar.consumed);
  EXPECT_EQ(batched.following_value, scalar.following_value);
  rohit::full_stream_auto_alloc actual{};
  rohit::full_stream_auto_alloc expected{};
  scalar_output<codec::serialize_key_type::none, Endian> actual_writer{actual};
  scalar_output<codec::serialize_key_type::none, Endian> expected_writer{expected};
  batched.value.serialize_out(actual_writer);
  scalar.value.serialize_out(expected_writer);
  EXPECT_EQ(written(actual), written(expected));
}

// Exercise every truncation and budget boundary, including malformed Boolean error precedence.
template <std::endian Endian>
void check_read_boundaries() {
  rohit::full_stream_auto_alloc output{};
  scalar_output<codec::serialize_key_type::none, Endian> encoder{output};
  sample().serialize_out(encoder);
  encoder.serialize_out(std::uint32_t{17});
  const std::span<const std::uint8_t> complete{output.begin(), output.current_offset()};
  for (std::size_t size = 0; size <= complete.size(); ++size) {
    SCOPED_TRACE(size);
    compare_readers<Endian>(complete.first(size));
    codec::decode_limits limits{};
    limits.max_input_bytes = size;
    compare_readers<Endian>(complete, limits);
  }
  // The opening object scope and individual value charges fit well below this conservative bound.
  const auto work_ceiling = 4 * complete.size();
  for (std::size_t budget = 0; budget <= work_ceiling; ++budget) {
    SCOPED_TRACE(budget);
    codec::decode_limits limits{};
    limits.max_work_units = budget;
    compare_readers<Endian>(complete, limits);
  }
  constexpr auto boolean_offset =
      sizeof(std::uint8_t) + sizeof(std::int16_t) + sizeof(std::uint32_t) + sizeof(float);
  output.begin()[boolean_offset] = 2;
  for (std::size_t budget = 0; budget <= work_ceiling; ++budget) {
    codec::decode_limits limits{};
    limits.max_work_units = budget;
    compare_readers<Endian>(complete, limits);
  }
  codec::decode_limits no_nesting{};
  no_nesting.max_nesting_depth = 0;
  compare_readers<Endian>(complete, no_nesting);
}
} // namespace

// Reservation reduction must preserve exact bytes in every key mode and configured byte order.
TEST(fixed_field_batch, binary_modes_and_golden_bytes) {
  check_generated_output<codec::serialize_key_type::none, std::endian::little>();
  check_generated_output<codec::serialize_key_type::integer, std::endian::little>();
  check_generated_output<codec::serialize_key_type::string, std::endian::little>();
  check_generated_output<codec::serialize_key_type::none, std::endian::big>();
  check_generated_output<codec::serialize_key_type::integer, std::endian::big>();
  check_generated_output<codec::serialize_key_type::string, std::endian::big>();
  check_golden_bytes<std::endian::little>();
  check_golden_bytes<std::endian::big>();
}

// Failure must preserve the original scalar path's completed fields, diagnostics, and cumulative work.
TEST(fixed_field_batch, input_boundaries_and_limits) {
  check_read_boundaries<std::endian::little>();
  check_read_boundaries<std::endian::big>();
}

// Output reserves complete batches before touching bytes and always honors stream policy overrides.
TEST(fixed_field_batch, reservation_failure_is_atomic_per_batch) {
  constexpr std::size_t sample_wire_bytes = 21; // The independent golden representation above.
  std::array<std::uint8_t, sample_wire_bytes> storage{};
  constexpr std::uint8_t untouched = 0xcc;
  for (std::size_t capacity = 0; capacity < storage.size(); ++capacity) {
    storage.fill(untouched);
    rohit::full_stream output{storage.data(), capacity};
    codec::binary_none<codec::serialize_type::out> encoder{output};
    EXPECT_THROW(sample().serialize_out(encoder), rohit::exception::stream_overflow_exception);
    EXPECT_EQ(output.current_offset(), 0u);
    EXPECT_TRUE(
        std::all_of(storage.begin(), storage.end(), [](auto value) { return value == untouched; }));
  }
  rohit::full_stream exact{storage.data(), storage.size()};
  codec::binary_none<codec::serialize_type::out> exact_encoder{exact};
  EXPECT_NO_THROW(sample().serialize_out(exact_encoder));
  EXPECT_EQ(exact.current_offset(), storage.size());

  storage.fill(untouched);
  rohit::full_stream output{storage.data(), storage.size()};
  codec::binary_none<codec::serialize_type::out> encoder{output};
  encoder.serialize_out(std::uint8_t{0xee});
  EXPECT_THROW(sample().serialize_out(encoder), rohit::exception::stream_overflow_exception);
  EXPECT_EQ(output.current_offset(), 1u);
  EXPECT_EQ(storage[0], 0xee);

  rejecting_stream rejected{};
  codec::binary_none<codec::serialize_type::out> policy_encoder{rejected};
  EXPECT_THROW(sample().serialize_out(policy_encoder), rohit::exception::stream_overflow_exception);
  EXPECT_EQ(rejected.reservations, 1u);
  EXPECT_EQ(rejected.current_offset(), 0u);
}

// Bound unrolled packs and restart batching around variable values, parents, objects, enums, and unions.
TEST(fixed_field_batch, generated_run_boundaries) {
  fixed_fields::long_run long_value{};
  long_value.a0 = 1;
  long_value.a17 = 17;
  counting_stream long_output{};
  codec::binary_none<codec::serialize_type::out> long_encoder{long_output};
  long_value.serialize_out(long_encoder);
  EXPECT_EQ(long_output.current_offset(), 18u);
  EXPECT_EQ(long_output.reservations, 2u);
  const auto long_input =
      rohit::make_constant_full_stream(long_output.begin(), long_output.current_offset());
  counting_input long_decoder{long_input};
  fixed_fields::long_run long_decoded{};
  long_decoded.serialize_in(long_decoder);
  long_decoder.finish();
  EXPECT_EQ(long_decoder.batches, 2u);
  EXPECT_EQ(long_decoded.a0, long_value.a0);
  EXPECT_EQ(long_decoded.a17, long_value.a17);

  fixed_fields::segmented value{};
  static_cast<fixed_fields::scalars&>(value) = sample();
  value.before_a = 1;
  value.before_b = 2;
  value.label = "text";
  value.after_a = 3;
  value.after_b = 4;
  value.status = fixed_fields::phase::active;
  value.tail_a = 5;
  value.tail_b = 6;
  value.child = sample();
  value.choice.number = 7;
  counting_stream output{};
  codec::binary_none<codec::serialize_type::out> encoder{output};
  value.serialize_out(encoder);
  EXPECT_EQ(output.reservations, 10u);
  const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
  codec::binary_none<codec::serialize_type::in> decoder{input};
  fixed_fields::segmented decoded{};
  decoded.serialize_in(decoder);
  decoder.finish();
  rohit::full_stream_auto_alloc reference{};
  scalar_output<codec::serialize_key_type::none, std::endian::little> scalar{reference};
  decoded.serialize_out(scalar);
  EXPECT_EQ(written(reference), written(output));

  // JSON must retain opening braces, separators, names, and enum/union spellings across these runs.
  rohit::full_stream_auto_alloc json_output{};
  codec::json<codec::serialize_type::out> json_encoder{json_output};
  value.serialize_out(json_encoder);
  const auto json_input =
      rohit::make_constant_full_stream(json_output.begin(), json_output.current_offset());
  codec::json<codec::serialize_type::in> json_decoder{json_input};
  fixed_fields::segmented json_decoded{};
  json_decoded.serialize_in(json_decoder);
  json_decoder.finish();
  reference.reset();
  json_decoded.serialize_out(scalar);
  EXPECT_EQ(written(reference), written(output));
}

// Preserve wide signed/unsigned values, signed zero, and NaN payload bits in direct batch calls.
TEST(fixed_field_batch, scalar_representations_and_named_prefixes) {
  const auto nan = std::bit_cast<double>(std::uint64_t{0x7ff8000000000042});
  counting_stream output{};
  codec::binary_none<codec::serialize_type::out> encoder{output};
  encoder.struct_serialize_out_fixed(std::int8_t{-1}, std::uint16_t{0xffff}, std::int32_t{-1},
                                     std::numeric_limits<std::int64_t>::min(),
                                     std::numeric_limits<std::uint64_t>::max(), -0.0f, nan);
  EXPECT_EQ(output.reservations, 1u);
  const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
  codec::binary_none<codec::serialize_type::in> decoder{input};
  std::int8_t a{};
  std::uint16_t b{};
  std::int32_t c{};
  std::int64_t d{};
  std::uint64_t e{};
  float f{};
  double g{};
  decoder.serialize_in_fixed(a, b, c, d, e, f, g);
  decoder.finish();
  EXPECT_EQ(a, -1);
  EXPECT_EQ(b, 0xffff);
  EXPECT_EQ(c, -1);
  EXPECT_EQ(d, std::numeric_limits<std::int64_t>::min());
  EXPECT_EQ(e, std::numeric_limits<std::uint64_t>::max());
  EXPECT_EQ(std::bit_cast<std::uint32_t>(f), std::bit_cast<std::uint32_t>(-0.0f));
  EXPECT_EQ(std::bit_cast<std::uint64_t>(g), std::bit_cast<std::uint64_t>(nan));

  output.reset();
  output.reservations = 0;
  codec::binary_string<codec::serialize_type::out> named{output};
  const std::string long_name(64, 'n');
  const auto first = std::make_pair(std::string_view{long_name}, std::uint32_t{42});
  const auto second = std::make_pair(std::string_view{"flag"}, true);
  named.struct_serialize_out_fixed(first, second);
  EXPECT_EQ(output.reservations, 1u);
  rohit::full_stream_auto_alloc reference{};
  codec::binary_string<codec::serialize_type::out> scalar{reference};
  scalar.struct_serialize_out(first);
  scalar.struct_serialize_out(second);
  EXPECT_EQ(written(output), written(reference));

  output.reset();
  output.reservations = 0;
  EXPECT_THROW(named.struct_serialize_out_fixed(first, std::make_pair(std::string_view{}, true)),
               std::invalid_argument);
  EXPECT_EQ(output.current_offset(), 0u);
  EXPECT_EQ(output.reservations, 0u);
  codec::binary_integer<codec::serialize_type::out> integer{output};
  EXPECT_THROW(integer.struct_serialize_out_fixed(std::make_pair(std::uint32_t{1}, false),
                                                  std::make_pair(std::uint32_t{0}, true)),
               std::invalid_argument);
  EXPECT_EQ(output.current_offset(), 0u);
  EXPECT_EQ(output.reservations, 0u);
  EXPECT_THROW(integer.struct_serialize_out_fixed(
                   std::make_pair(std::uint32_t{1}, false),
                   std::make_pair(std::numeric_limits<std::uint32_t>::max(), true)),
               std::out_of_range);
  EXPECT_EQ(output.current_offset(), 0u);
  EXPECT_EQ(output.reservations, 0u);
}
