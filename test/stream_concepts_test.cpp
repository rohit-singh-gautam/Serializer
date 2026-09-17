#include "stream_test_support.hpp"

#include <constant_names.hpp>
#include <fixed_fields.hpp>
#include <gtest/gtest.h>
#include <person.hpp>
#include <rohit/serializer.hpp>
#include <rohit/serializer_creator.hpp>
#include <views.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
namespace codec = rohit::serializer;

using stream_test::input_cursor;
using stream_test::output_buffer;

// Reject a structurally incomplete stream at the constrained API boundary.
struct incomplete_stream {};
static_assert(rohit::type_check::input_buffer<input_cursor>);
static_assert(rohit::type_check::input_buffer<rohit::full_stream&>);
static_assert(rohit::type_check::input_buffer<const rohit::full_stream&>);
static_assert(rohit::type_check::output_buffer<output_buffer>);
static_assert(rohit::type_check::write_stream<rohit::fixed_buffer>);
static_assert(!std::is_base_of_v<rohit::stream, input_cursor>);
static_assert(!std::is_base_of_v<rohit::stream, output_buffer>);
static_assert(rohit::type_check::input_stream<std::istream>);
static_assert(rohit::type_check::output_stream<std::ostream>);
static_assert(!rohit::type_check::input_stream<const std::istream>);
static_assert(!rohit::type_check::output_stream<const output_buffer>);
static_assert(!rohit::type_check::input_stream<incomplete_stream>);
static_assert(!rohit::type_check::output_stream<incomplete_stream>);

// Detect invalid generated stream calls without instantiating their bodies.
template <typename T>
concept accepts_output = requires(const test::test1::person& value, T& output) {
  value.template serialize_out<codec::json>(output);
};
static_assert(accepts_output<output_buffer>);
static_assert(accepts_output<std::ostream>);
static_assert(!accepts_output<incomplete_stream>);

// The explicit-limit overload has the same structural input constraints as the default overload.
template <typename T>
concept accepts_limited_input = requires(test::test1::person& value, T& input,
                                        codec::decode_limits limits) {
  value.template serialize_in<codec::json>(input, limits);
};
static_assert(accepts_limited_input<input_cursor>);
static_assert(accepts_limited_input<const input_cursor>);
static_assert(accepts_limited_input<std::istream>);
static_assert(!accepts_limited_input<incomplete_stream>);

// Compare custom storage and iostream output with the established native byte contract.
template <template <codec::serialize_type> class Protocol>
void check_round_trip() {
  const test::test1::person original{"Ada\n\"Lovelace\"", 123456};
  rohit::full_stream_auto_alloc native;
  original.serialize_out<Protocol>(native);
  const std::string expected{reinterpret_cast<const char*>(native.begin()),
                             native.current_offset()};
  output_buffer custom;
  original.serialize_out<Protocol>(custom);
  EXPECT_EQ(custom.bytes(), expected);
  const input_cursor input{expected};
  test::test1::person decoded{};
  decoded.serialize_in<Protocol>(input);
  EXPECT_TRUE(input.full());
  EXPECT_EQ(decoded.name, original.name);
  EXPECT_EQ(decoded.id, original.id);

  const input_cursor limited_input{expected};
  codec::decode_limits limits;
  limits.max_input_bytes = expected.size();
  decoded = {};
  decoded.serialize_in<Protocol>(limited_input, limits);
  EXPECT_TRUE(limited_input.full());
  EXPECT_EQ(decoded.name, original.name);
  EXPECT_EQ(decoded.id, original.id);

  std::stringstream io;
  original.serialize_out<Protocol>(io);
  EXPECT_EQ(io.str(), expected);
  decoded = {};
  decoded.serialize_in<Protocol>(io);
  EXPECT_EQ(decoded.name, original.name);
  EXPECT_EQ(decoded.id, original.id);
}

// A failed bulk write must surface even when ostream exceptions are disabled.
class failing_output : public std::streambuf {
  // Simulate a sink that accepts only a prefix of every write.
  std::streamsize xsputn(const char*, std::streamsize count) override {
    return count / 2;
  }
};

// This source deliberately offers no seek operations.
class nonseekable_input : public std::streambuf {
  std::string storage;

public:
  // Retain the supplied bytes for the lifetime of the stream buffer.
  explicit nonseekable_input(std::string text) : storage{std::move(text)} {
    setg(storage.data(), storage.data(), storage.data() + storage.size());
  }
};

// A structural byte sink exercises implicit adaptation without any standard-stream base class.
struct counting_output {
  std::string bytes{};
  std::size_t writes{};
  // Retain the complete supplied byte range and count batched writes.
  void write(const char* data, std::streamsize size) {
    bytes.append(data, static_cast<std::size_t>(size));
    ++writes;
  }
  // This in-memory sink never reports an I/O failure.
  bool fail() const {
    return false;
  }
};

// A legacy custom protocol inherits codec operations but must retain its own construction behavior.
template <codec::serialize_type Direction>
class legacy_json : public codec::json<Direction> {
  using base = codec::json<Direction>;
  using input_type = std::conditional_t<Direction == codec::serialize_type::in, const rohit::stream,
                                        rohit::stream>;

public:
  static inline bool constructed{};
  // Preserve the original single-stream constructor without requiring a limits overload.
  explicit legacy_json(input_type& input) : base{input} {
    constructed = true;
  }
};

// Compare multi-buffer output, including strings larger than an ordinary staging buffer.
template <template <codec::serialize_type> class Protocol>
void check_large_output() {
  constant_names::record value{};
  value.label = std::string(20000, 'x');
  value.tags = std::vector<std::string>(100, std::string(1000, 'y'));
  value.totals = {{"first", 1}, {"last", 100}};
  rohit::full_stream_auto_alloc expected;
  value.serialize_out<Protocol>(expected);
  counting_output output;
  value.serialize_out<Protocol>(output);
  EXPECT_GT(output.writes, 1);
  EXPECT_EQ(output.bytes, std::string(reinterpret_cast<const char*>(expected.begin()),
                                      expected.current_offset()));
}
} // namespace

TEST(stream_concepts, custom_buffers_and_iostreams_preserve_protocol_bytes) {
  check_round_trip<codec::json>();
  check_round_trip<codec::binary_none>();
  check_round_trip<codec::binary_integer>();
  check_round_trip<codec::binary_string>();
}

TEST(stream_concepts, concrete_codec_supports_custom_cursor_and_diagnostics) {
  const input_cursor input{"{\"fullname\":\"Ada\",\"ID\":8} trailing"};
  codec::decode_limits limits;
  limits.diagnostics.include_input_excerpt = true;
  codec::json<codec::serialize_type::in, input_cursor> decoder{input, limits};
  test::test1::person value{};
  decoder.serialize_in(value);
  try {
    decoder.finish();
    FAIL() << "Trailing data was accepted";
  } catch (const codec::exception::bad_input_data& error) {
    EXPECT_NE(std::string{error.what()}.find("trailing"), std::string::npos);
  }
}

TEST(stream_concepts, custom_output_keeps_fixed_field_batches_and_formatting) {
  const fixed_fields::scalars value{7, -25, 90000, 1.5F, true, -2.25, 'z'};
  output_buffer output;
  value.serialize_out<codec::binary_integer>(output);
  const auto bytes = output.bytes();
  const input_cursor retained_input{bytes};
  codec::binary_integer<codec::serialize_type::in, input_cursor> decoder{retained_input};
  fixed_fields::scalars decoded{};
  decoder.serialize_in(decoded);
  decoder.finish();
  EXPECT_EQ(decoded.sequence, value.sequence);
  EXPECT_EQ(decoded.delta, value.delta);
  EXPECT_EQ(decoded.reading, value.reading);

  output_buffer text;
  codec::json_out<true, output_buffer> encoder{text, codec::format::beautify};
  value.serialize_out(encoder);
  EXPECT_NE(text.bytes().find('\n'), std::string::npos);
}

TEST(stream_concepts, nonseekable_input_and_exception_masks_are_supported) {
  nonseekable_input buffer{"{\"fullname\":\"Ada\",\"ID\":8}"};
  std::istream input{&buffer};
  const auto mask = std::ios::eofbit | std::ios::failbit | std::ios::badbit;
  input.exceptions(mask);
  test::test1::person value{};
  value.serialize_in<codec::json>(input);
  EXPECT_EQ(value.name, "Ada");
  EXPECT_EQ(value.id, 8);
  EXPECT_EQ(input.exceptions(), mask);
  EXPECT_TRUE(input.eof());
}

TEST(stream_concepts, enforces_message_limits_and_exact_iostream_input) {
  const std::string json = "{\"fullname\":\"Ada\",\"ID\":8}";
  codec::decode_limits limits;
  limits.max_input_bytes = json.size();
  test::test1::person value{};
  std::istringstream exact{json};
  EXPECT_NO_THROW(value.serialize_in<codec::json>(exact, limits));
  std::istringstream oversized{json + " "};
  EXPECT_THROW(value.serialize_in<codec::json>(oversized, limits), std::length_error);
  limits.max_input_bytes = json.size() + 3;
  std::istringstream trailing{json + " {}"};
  EXPECT_THROW(value.serialize_in<codec::json>(trailing, limits), codec::exception::bad_input_data);
  std::istringstream truncated{json.substr(0, json.size() - 1)};
  EXPECT_THROW(value.serialize_in<codec::json>(truncated, limits), codec::exception::bad_input_data);
}

TEST(stream_concepts, generated_limits_apply_to_custom_buffers_and_memory_input) {
  const std::string json = "{\"fullname\":\"Ada\",\"ID\":8}";
  codec::decode_limits limits;
  limits.max_string_bytes = 2;
  test::test1::person value{};
  const input_cursor custom{json};
  EXPECT_THROW(value.serialize_in<codec::json>(custom, limits), codec::exception::resource_limit);
  std::istringstream memory{json};
  EXPECT_THROW(value.serialize_in<codec::json>(memory, limits), codec::exception::resource_limit);
}

TEST(stream_concepts, reports_external_io_failures) {
  failing_output buffer;
  std::ostream output{&buffer};
  test::test1::person value{};
  EXPECT_THROW(value.serialize_out<codec::json>(output), std::ios_base::failure);
  std::istringstream input{"{}"};
  input.setstate(std::ios::badbit);
  EXPECT_THROW(value.serialize_in<codec::json>(input), std::ios_base::failure);
}

TEST(stream_concepts, accepts_temporary_const_input_views) {
  test::test1::person value{};
  const std::string json = "{\"fullname\":\"Ada\",\"ID\":8}";
  value.serialize_in<codec::json>(rohit::make_constant_full_stream(json));
  EXPECT_EQ(value.name, "Ada");
}

TEST(stream_concepts, binary_views_support_custom_output_and_iostreams) {
  const std::array<std::uint8_t, sizeof(std::uint32_t)> bytes{7, 0, 0, 0};
  const auto view = view_test::readonly_record::map(bytes);
  output_buffer custom;
  view.serialize_out<codec::binary_none>(custom);
  EXPECT_EQ(custom.bytes(), std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
  std::ostringstream io;
  view.serialize_out<codec::binary_none>(io);
  EXPECT_EQ(io.str(), custom.bytes());
  output_buffer through_protocol;
  codec::binary_none<codec::serialize_type::out, output_buffer> encoder{through_protocol};
  encoder.serialize_out(view);
  EXPECT_EQ(through_protocol.bytes(), custom.bytes());
}

TEST(stream_concepts, schema_compiler_accepts_independent_streams) {
  const std::string schema = "serializer version 1; class record { public uint32 value; }";
  const input_cursor input{schema};
  auto statements = codec::parser::parse(input, true);
  EXPECT_TRUE(input.full());
  codec::writer::cpp_options options;
  options.format = false;
  output_buffer custom;
  codec::writer::cpp::write(custom, statements, options);
  std::ostringstream output;
  codec::writer::cpp::write(output, statements, options);
  EXPECT_EQ(output.str(), custom.bytes());
  EXPECT_NE(output.str().find("type_check::output_stream"), std::string::npos);
  std::istringstream source{schema};
  auto from_stream = codec::parser::parse(source, true);
  std::ostringstream java;
  codec::writer::java::write(java, from_stream, "Schema");
  EXPECT_NE(java.str().find("class Schema"), std::string::npos);
}

TEST(stream_concepts, parser_propagates_consumed_bytes_on_error) {
  const input_cursor input{"valid_identifier @"};
  EXPECT_EQ(codec::parser::parse_identifier(input), "valid_identifier");
  EXPECT_EQ(input.remaining_buffer(), 2);
  EXPECT_THROW(codec::parser::parse(input), codec::exception::bad_identifier);
  EXPECT_EQ(input.remaining_buffer(), 1);
}

TEST(stream_concepts, implicit_output_wrapper_flushes_complete_batches) {
  check_large_output<codec::json>();
  check_large_output<codec::binary_none>();
  check_large_output<codec::binary_integer>();
  check_large_output<codec::binary_string>();
}

TEST(stream_concepts, buffered_output_retains_aliases_across_reservations) {
  counting_output output;
  rohit::buffered_output_stream<counting_output> buffer{output, 8};
  buffer.append("abcd");
  const std::string_view alias{reinterpret_cast<const char*>(buffer.begin()), 4};
  codec::json_out<false, decltype(buffer)> encoder{buffer};
  encoder.serialize_out(alias);
  EXPECT_EQ(output.writes, 0);
  buffer.append("0123456789");
  buffer.finish();
  EXPECT_EQ(output.bytes, "abcd\"abcd\"0123456789");
  EXPECT_GE(output.writes, 2);
}

TEST(stream_concepts, failed_buffered_output_cannot_repeat_a_partial_write) {
  failing_output sink;
  std::ostream output{&sink};
  rohit::buffered_output_stream<std::ostream> buffer{output, 4};
  buffer.append("1234");
  EXPECT_THROW(buffer.append('5'), std::ios_base::failure);
  output.clear();
  EXPECT_THROW(buffer.finish(), std::ios_base::failure);
  EXPECT_THROW(buffer.append('6'), std::ios_base::failure);
}

TEST(stream_concepts, memory_input_borrows_only_the_unread_suffix) {
  const std::string json = "{\"fullname\":\"Ada\",\"ID\":8}";
  std::istringstream input{"prefix" + json};
  constexpr std::streamoff prefix_bytes = 6;
  input.seekg(prefix_bytes);
  const auto* expected = reinterpret_cast<const std::uint8_t*>(input.view().data()) + prefix_bytes;
  const auto view = rohit::borrow_stream_bytes(input, json.size());
  EXPECT_EQ(view.curr(), expected);
  EXPECT_EQ(view.remaining_buffer(), json.size());
  EXPECT_TRUE(input.eof());

  std::stringstream stream{"prefix" + json};
  stream.seekg(prefix_bytes);
  test::test1::person value{};
  value.serialize_in<codec::json>(stream);
  EXPECT_EQ(value.name, "Ada");
  EXPECT_EQ(value.id, 8);
}

TEST(stream_concepts, erased_memory_input_keeps_the_generic_fallback) {
  std::istringstream memory{"{\"fullname\":\"Ada\",\"ID\":8}"};
  std::istream& input = memory;
  static_assert(!rohit::detail::memory_input_stream<decltype(input)>);
  test::test1::person value{};
  value.serialize_in<codec::json>(input);
  EXPECT_EQ(value.id, 8);
  EXPECT_TRUE(input.eof());
}

TEST(stream_concepts, memory_input_accepts_normal_eof_with_exceptions_enabled) {
  std::istringstream input{"{\"fullname\":\"Ada\",\"ID\":8}"};
  const auto mask = std::ios::eofbit | std::ios::failbit | std::ios::badbit;
  input.exceptions(mask);
  test::test1::person value{};
  value.serialize_in<codec::json>(input);
  EXPECT_EQ(value.name, "Ada");
  EXPECT_TRUE(input.eof());
  EXPECT_EQ(input.exceptions(), mask);
}

TEST(stream_concepts, file_streams_round_trip_multiple_io_batches) {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto path = std::filesystem::temp_directory_path() /
                    ("serializer_stream_concepts_" + std::to_string(stamp) + ".bin");
  // Remove only this test's file, including when a codec or filesystem operation throws.
  struct temporary_file {
    std::filesystem::path path;
    // Cleanup must not mask the original test failure.
    ~temporary_file() {
      std::error_code ignored;
      std::filesystem::remove(path, ignored);
    }
  } cleanup{path};
  const test::test1::person original{std::string(150000, 'x'), 700};
  {
    std::ofstream output{path, std::ios::binary};
    output.exceptions(std::ios::failbit | std::ios::badbit);
    original.serialize_out<codec::binary_integer>(output);
    output.close();
  }
  std::ifstream input{path, std::ios::binary};
  test::test1::person decoded{};
  decoded.serialize_in<codec::binary_integer>(input);
  EXPECT_EQ(decoded.name, original.name);
  EXPECT_EQ(decoded.id, original.id);
  EXPECT_TRUE(input.eof());
}

TEST(stream_concepts, generic_input_checks_exact_limits_without_overreading) {
  const std::string json = "{\"fullname\":\"Ada\",\"ID\":8}";
  codec::decode_limits limits;
  limits.max_input_bytes = json.size();
  nonseekable_input exact_bytes{json};
  std::istream exact{&exact_bytes};
  exact.exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);
  test::test1::person value{};
  EXPECT_NO_THROW(codec::serialize_from<codec::json>(exact, value, limits));
  EXPECT_EQ(value.name, "Ada");
  nonseekable_input oversized_bytes{json + "!"};
  std::istream oversized{&oversized_bytes};
  EXPECT_THROW(codec::serialize_from<codec::json>(oversized, value, limits), std::length_error);
  EXPECT_EQ(oversized.peek(), '!');
  nonseekable_input empty_bytes{""};
  std::istream empty{&empty_bytes};
  EXPECT_EQ(rohit::read_stream_bytes(empty, 0).current_offset(), 0);
}

TEST(stream_concepts, inherited_rebind_hooks_preserve_custom_protocol_behavior) {
  legacy_json<codec::serialize_type::out>::constructed = false;
  legacy_json<codec::serialize_type::in>::constructed = false;
  const test::test1::person original{"Ada", 8};
  rohit::full_stream_auto_alloc bytes;
  original.serialize_out<legacy_json>(bytes);
  EXPECT_TRUE(legacy_json<codec::serialize_type::out>::constructed);
  const auto input = rohit::make_constant_full_stream(bytes.begin(), bytes.current_offset());
  test::test1::person decoded{};
  decoded.serialize_in<legacy_json>(input);
  EXPECT_TRUE(legacy_json<codec::serialize_type::in>::constructed);
  EXPECT_EQ(decoded.name, original.name);
}
