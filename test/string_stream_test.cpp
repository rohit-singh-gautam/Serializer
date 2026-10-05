#include <rohit/serializer.hpp>

#include <gtest/gtest.h>
#include <person.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {
namespace codec = rohit::serializer;

template <codec::serialize_type Direction>
using json_protocol = codec::json<Direction>;
template <codec::serialize_type Direction>
using positional_protocol = codec::binary_none<Direction>;
template <codec::serialize_type Direction>
using integer_protocol = codec::binary_integer<Direction>;
template <codec::serialize_type Direction>
using string_protocol = codec::binary_string<Direction>;

static_assert(rohit::type_check::output_buffer<rohit::string_stream>);
static_assert(rohit::type_check::input_buffer<const rohit::string_stream>);
static_assert(rohit::type_check::schema_input_buffer<rohit::string_stream>);
static_assert(!std::is_copy_constructible_v<rohit::string_stream>);
static_assert(std::is_nothrow_move_constructible_v<rohit::string_stream>);
static_assert(!std::is_convertible_v<rohit::string_stream*, rohit::full_stream*>);

// Compare real generated records with the existing bytes and decode the exact written prefix.
template <template <codec::serialize_type> class Protocol>
void check_generated_round_trip() {
  const test::test1::person original{"Ada\n\"Lovelace\" \xE2\x82\xAC", 123456};
  rohit::full_stream_auto_alloc native;
  original.serialize_out<Protocol>(native);
  const std::string_view expected{reinterpret_cast<const char*>(native.begin()),
                                  native.current_offset()};
  rohit::string_stream output;
  original.serialize_out<Protocol>(output);
  EXPECT_EQ(output.view(), expected);
  const auto input = rohit::make_constant_stream(output.view().data(), output.view().size());
  const auto decoded = codec::deserialize_exact<test::test1::person, Protocol>(input);
  EXPECT_EQ(decoded.name, original.name);
  EXPECT_EQ(decoded.id, original.id);
  EXPECT_TRUE(input.full());
}
} // namespace

// Empty and reserved streams expose only consumed bytes; binary NUL bytes survive extraction.
TEST(string_stream, preserves_written_extent_and_embedded_nuls) {
  rohit::string_stream output;
  EXPECT_TRUE(output.view().empty());
  const auto* empty = output.begin();
  output.reserve(0);
  EXPECT_EQ(output.begin(), empty);
  output.reserve(128);
  EXPECT_GE(output.capacity(), 128U);
  EXPECT_EQ(output.current_offset(), 0U);
  constexpr char text[] = "A\0\"\\\n";
  const std::string expected{text, sizeof(text) - 1};
  output.append(expected);
  EXPECT_EQ(output.view(), expected);
  EXPECT_EQ(output.str(), expected);
  EXPECT_EQ(std::move(output).str(), expected);
  EXPECT_TRUE(output.view().empty());
  output.write("reused");
  EXPECT_EQ(output.view(), "reused");
}

// Existing writable storage is stable until a reservation exceeds it; growth preserves the cursor.
TEST(string_stream, reserves_before_unchecked_memory_writes) {
  rohit::string_stream output{64};
  const auto* start = output.begin();
  output.reserve(4);
  auto* bytes = output.get_curr_and_increase_unchecked(4);
  std::copy_n("data", 4, bytes);
  EXPECT_EQ(output.begin(), start);
  const auto remaining = output.remaining_buffer();
  output.reserve(remaining);
  EXPECT_EQ(output.begin(), start);
  EXPECT_EQ(output.current_offset(), 4U);
  output.reserve(remaining + 1);
  EXPECT_EQ(output.current_offset(), 4U);
  EXPECT_EQ(output.view(), "data");
  EXPECT_EQ(output.curr(), output.begin() + output.current_offset());
  EXPECT_EQ(output.end(), output.begin() + output.capacity());
}

// All checked cursor operations can grow storage, while rewinding preserves the written prefix.
TEST(string_stream, grows_through_checked_cursor_operations) {
  rohit::string_stream output;
  *output++ = 'a';
  output.reserve(1);
  *output = 'b';
  ++output;
  auto* range = output.get_curr_and_increase(2);
  std::copy_n("cd", 2, range);
  output += 1;
  output.begin()[4] = 'e';
  const auto view = output + 1;
  output.begin()[5] = 'f';
  EXPECT_EQ(view.curr(), output.curr());
  EXPECT_EQ(output.view(), "abcdef");
  --output;
  EXPECT_EQ(output.view(), "abcde");
  const auto capacity = output.capacity();
  output.reset();
  EXPECT_TRUE(output.view().empty());
  EXPECT_EQ(output.capacity(), capacity);
  EXPECT_THROW(--output, rohit::exception::stream_underflow_exception);
}

// Self-appends and mixed aliased fragments survive a forced small-buffer/heap relocation.
TEST(string_stream, rebases_aliased_appends_and_text_batches) {
  for (const std::size_t initial_bytes : {1U, 128U}) {
    rohit::string_stream output{initial_bytes};
    const std::string prefix(output.capacity(), 'x');
    output.append(prefix);
    output.append(output.begin(), output.current_offset());
    EXPECT_EQ(output.view(), prefix + prefix);
    const std::string snapshot{output.view()};
    const auto first = output.view().substr(0, snapshot.size() / 2);
    const auto second = output.view().substr(snapshot.size() / 2);
    const auto expected = snapshot + std::string{first} + ':' + std::string{second};
    output.write(first, ':', second);
    EXPECT_EQ(output.view(), expected);
  }
}

// Raw byte batching snapshots its arguments before string growth can move their references.
TEST(string_stream, snapshots_aliased_raw_bytes_before_growth) {
  rohit::string_stream output{3};
  const std::string prefix(output.capacity(), 'x');
  output.append(prefix);
  output.write_raw(output.begin()[0], output.begin()[1], output.begin()[2]);
  EXPECT_EQ(output.view(), prefix + "xxx");
}

// Escaped JSON appends rebase internal sources; overlapping transforms snapshot before writing.
TEST(string_stream, preserves_aliased_transformed_input) {
  const std::string text = "a\"b\n" + std::string(128, 'x');
  rohit::full_stream_auto_alloc reference;
  codec::json_out<false, rohit::full_stream_auto_alloc> native{reference};
  native.serialize_out(text);
  const std::string expected{reinterpret_cast<const char*>(reference.begin()),
                             reference.current_offset()};
  rohit::string_stream output{text};
  codec::json_out<false, rohit::string_stream> encoder{output};
  encoder.serialize_out(output.view());
  EXPECT_EQ(output.view(), text + expected);
  output.reset();
  output.append_transformed(output.begin(), text.size(), text.size(),
                            [](std::uint8_t* destination, const std::uint8_t* source,
                               std::size_t size) noexcept {
                              std::copy_n(source, size, destination);
                            });
  EXPECT_EQ(output.view(), text);
}

// Move construction/assignment must rebase small strings and preserve partially consumed buffers.
TEST(string_stream, moves_storage_and_cursor_and_reuses_sources) {
  for (const std::size_t initial_bytes : {1U, 128U}) {
    rohit::string_stream source{initial_bytes};
    source.write("abc");
    const auto capacity = source.capacity();
    rohit::string_stream moved{std::move(source)};
    EXPECT_EQ(moved.view(), "abc");
    EXPECT_EQ(moved.capacity(), capacity);
    source.write("source");
    EXPECT_EQ(source.view(), "source");
    EXPECT_EQ(moved.view(), "abc");
    rohit::string_stream assigned{std::string{"previous"}};
    assigned = std::move(moved);
    EXPECT_EQ(assigned.view(), "abc");
    EXPECT_TRUE(moved.view().empty());
    moved.write("moved");
    assigned.append('d');
    EXPECT_EQ(assigned.view(), "abcd");
    EXPECT_EQ(moved.view(), "moved");
    auto& same = assigned;
    assigned = std::move(same);
    EXPECT_EQ(assigned.view(), "abcd");
  }
}

// Oversized reservations and invalid internal ranges fail before changing storage or cursor.
TEST(string_stream, rejects_invalid_growth_transactionally) {
  rohit::string_stream output{std::string{"prefix"}};
  const auto* begin = output.begin();
  const auto* cursor = output.curr();
  const auto capacity = output.capacity();
  EXPECT_THROW(output.reserve(std::numeric_limits<std::size_t>::max()),
               rohit::exception::stream_overflow_exception);
  EXPECT_THROW(output.append(output.end() - 1, 2), rohit::exception::stream_overflow_exception);
  EXPECT_EQ(output.begin(), begin);
  EXPECT_EQ(output.curr(), cursor);
  EXPECT_EQ(output.capacity(), capacity);
  EXPECT_EQ(output.view(), "prefix");
  output.write("-recovered");
  EXPECT_EQ(output.view(), "prefix-recovered");
}

// The new storage policy keeps all four native generated protocol outputs byte-for-byte identical.
TEST(string_stream, round_trips_generated_native_protocols) {
  check_generated_round_trip<json_protocol>();
  check_generated_round_trip<positional_protocol>();
  check_generated_round_trip<integer_protocol>();
  check_generated_round_trip<string_protocol>();
}

// Beautified JSON retains exact whitespace, escaping, and the explicitly appended trailing LF.
TEST(string_stream, matches_beautified_json_bytes) {
  const test::test1::person original{"Ada\n\"Lovelace\"", 123456};
  rohit::full_stream_auto_alloc reference;
  codec::json_out<true, rohit::full_stream_auto_alloc> native{reference, codec::format::beautify};
  native.serialize_out(original);
  reference.write('\n');
  rohit::string_stream output;
  codec::json_out<true, rohit::string_stream> encoder{output, codec::format::beautify};
  encoder.serialize_out(original);
  output.write('\n');
  const std::string_view expected{reinterpret_cast<const char*>(reference.begin()),
                                  reference.current_offset()};
  EXPECT_EQ(output.view(), expected);
  EXPECT_EQ(std::move(output).str(), expected);
}
