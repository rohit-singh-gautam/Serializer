#include <rohit/serializer.hpp>
#include <rohit/serializer_creator.hpp>

#include <assessment.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace {
using json_in = rohit::serializer::json<rohit::serializer::serialize_type::in>;
using binary_in = rohit::serializer::binary_none<rohit::serializer::serialize_type::in>;
using binary_out = rohit::serializer::binary_none<rohit::serializer::serialize_type::out>;

// Decode against the exact supplied bytes, including the end-of-message check.
template <typename T>
void read_json(std::string_view text, T& value, rohit::serializer::decode_limits limits = {}) {
  const auto input = rohit::make_constant_full_stream(text.data(), text.size());
  json_in decoder{input, limits};
  decoder.serialize_in(value);
  decoder.finish();
}

// Check fresh and reused generated destinations through the same exact-size wire message.
template <template <rohit::serializer::serialize_type> class Protocol>
void check_generated_round_trip() {
  assessment::record original{};
  original.marker = 9;
  original.child.name = "quoted \"text\"\n";
  original.child.numbers = {1, -2, 3};
  original.child.phases = {assessment::phase::idle, assessment::phase::done};
  original.child.status = assessment::phase::active;
  original.entries.emplace(2, original.child);
  rohit::full_stream_auto_alloc output{1};
  original.template serialize_out<Protocol>(output);
  assessment::record decoded{};
  for (int attempt = 0; attempt < 2; ++attempt) {
    const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
    Protocol<rohit::serializer::serialize_type::in> decoder{input};
    decoder.serialize_in(decoded);
    decoder.finish();
    EXPECT_EQ(decoded.marker, original.marker);
    EXPECT_EQ(decoded.child.name, original.child.name);
    EXPECT_EQ(decoded.child.numbers, original.child.numbers);
    EXPECT_EQ(decoded.child.phases, original.child.phases);
    EXPECT_EQ(decoded.child.status, original.child.status);
    ASSERT_EQ(decoded.entries.size(), original.entries.size());
    EXPECT_EQ(decoded.entries.at(2).name, original.child.name);
  }
}

// Preserve IDs and wire names across a rename while making unknown-field rejection explicit.
template <template <rohit::serializer::serialize_type> class Protocol>
void check_schema_evolution() {
  assessment::older original{};
  original.count = 42;
  rohit::full_stream_auto_alloc output{1};
  original.template serialize_out<Protocol>(output);
  const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
  Protocol<rohit::serializer::serialize_type::in> decoder{input};
  assessment::newer decoded{};
  decoder.serialize_in(decoded);
  decoder.finish();
  EXPECT_EQ(decoded.renamed_count, original.count);
  EXPECT_EQ(decoded.added, 7U);

  output.reset();
  decoded.template serialize_out<Protocol>(output);
  const auto newer_input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
  Protocol<rohit::serializer::serialize_type::in> older_decoder{newer_input};
  EXPECT_THROW(older_decoder.serialize_in(original), rohit::serializer::exception::key_not_found);
}
} // namespace

// Reject numeric prefixes and overflow rather than silently saturating or wrapping.
TEST(decoder_qualification, json_number_grammar_and_range) {
  for (const auto text : {"", "+1", "01", "-", "1.", "1e", "1e+", "1x", "NaN", "Infinity"}) {
    double value = 17;
    EXPECT_THROW(read_json(text, value), rohit::exception::base_parser) << text;
    EXPECT_EQ(value, 17);
  }
  std::int16_t value = 17;
  EXPECT_THROW(read_json("32768", value), rohit::serializer::exception::numeric_range);
  EXPECT_EQ(value, 17);
  EXPECT_NO_THROW(read_json("-32768", value));
  EXPECT_EQ(value, std::numeric_limits<std::int16_t>::min());
}

// Accept only the exact JSON boolean and null spellings.
TEST(decoder_qualification, json_literals) {
  for (const auto text : {"True", "FALSE", "truex", "fals", "null"}) {
    bool value{};
    EXPECT_THROW(read_json(text, value), rohit::exception::base_parser);
  }
  std::nullptr_t null_value{};
  EXPECT_NO_THROW(read_json("null", null_value));
  EXPECT_THROW(read_json("nullx", null_value), rohit::exception::base_parser);
}

// Decode escaped Unicode and controls while replacing previous string contents.
TEST(decoder_qualification, json_unicode_and_reuse) {
  std::string value = "old data";
  read_json(R"("A\u0000\uD83D\uDE00\n\"\\")", value);
  const std::string expected{"A\0\xf0\x9f\x98\x80\n\"\\", 9};
  EXPECT_EQ(value, expected);
  read_json(R"("")", value);
  EXPECT_TRUE(value.empty());
  for (const auto text : {R"("\uD800")", R"("\uDC00")", R"("\q")", "\"unterminated", "\"\n\""}) {
    EXPECT_THROW(read_json(text, value), rohit::exception::base_parser);
  }
  const std::string invalid_utf8{"\"\xc0\xaf\"", 4};
  EXPECT_THROW(read_json(invalid_utf8, value), rohit::exception::base_parser);
}

// JSON output uses enough precision for exact binary64 round trips, including negative zero.
TEST(decoder_qualification, json_float_round_trip) {
  for (const double value : {0.0, -0.0, 0.1, std::numeric_limits<double>::min(),
                             std::numeric_limits<double>::max(), std::numeric_limits<double>::denorm_min()}) {
    rohit::full_stream_auto_alloc output{1};
    rohit::serializer::json_out<false> encoder{output};
    encoder.serialize_out(value);
    double decoded{};
    read_json(std::string_view{reinterpret_cast<const char*>(output.begin()), output.current_offset()}, decoded);
    EXPECT_EQ(std::bit_cast<std::uint64_t>(decoded), std::bit_cast<std::uint64_t>(value));
  }
  rohit::full_stream_auto_alloc output{1};
  rohit::serializer::json_out<false> encoder{output};
  EXPECT_THROW(encoder.serialize_out(std::numeric_limits<double>::infinity()), std::invalid_argument);
  EXPECT_EQ(output.current_offset(), 0);
}

// An odd byte offset must not require aligned integer or floating-point accesses.
TEST(decoder_qualification, binary_scalars_and_exact_boundaries) {
  rohit::full_stream_auto_alloc output{1};
  binary_out encoder{output};
  encoder.serialize_out('x');
  encoder.serialize_out(1.0f);
  encoder.serialize_out(-2.0);
  constexpr std::array<std::uint8_t, 13> expected{
      'x', 0x3f, 0x80, 0, 0, 0xc0, 0, 0, 0, 0, 0, 0, 0};
  ASSERT_EQ(output.current_offset(), expected.size());
  EXPECT_TRUE(std::equal(expected.begin(), expected.end(), output.begin()));
  const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
  binary_in decoder{input};
  char marker{};
  float first{};
  double second{};
  decoder.serialize_in(marker);
  decoder.serialize_in(first);
  decoder.serialize_in(second);
  decoder.finish();
  EXPECT_EQ(first, 1.0f);
  EXPECT_EQ(second, -2.0);
  for (const std::uint32_t value : {63U, 64U, 16383U, 16384U, 4194303U, 4194304U, 1073741823U}) {
    output.reset();
    encoder.serialize_out_variable(value);
    const auto complete = rohit::make_constant_full_stream(output.begin(), output.current_offset());
    binary_in exact{complete};
    EXPECT_EQ(exact.serialize_in_variable(), value);
    for (std::size_t size = 0; size < output.current_offset(); ++size) {
      const auto truncated = rohit::make_constant_full_stream(output.begin(), size);
      binary_in partial{truncated};
      EXPECT_THROW(partial.serialize_in_variable(), rohit::serializer::exception::bad_input_data);
      EXPECT_EQ(truncated.current_offset(), 0);
    }
  }
}

// Limits are checked before storage growth and shared across nested values.
TEST(decoder_qualification, resource_limits_and_partial_results) {
  rohit::serializer::decode_limits limits{};
  limits.max_string_bytes = 2;
  std::string text = "previous";
  EXPECT_THROW(read_json(R"("long")", text, limits), rohit::serializer::exception::resource_limit);
  EXPECT_EQ(text, "previous");
  limits = {};
  limits.max_collection_elements = 2;
  std::vector<int> values;
  EXPECT_THROW(read_json("[1,2,3]", values, limits), rohit::serializer::exception::resource_limit);
  EXPECT_EQ(values, (std::vector<int>{1, 2}));
  limits = {};
  limits.max_nesting_depth = 1;
  std::vector<std::vector<int>> nested;
  EXPECT_THROW(read_json("[[1]]", nested, limits), rohit::serializer::exception::resource_limit);
  limits = {};
  limits.max_input_bytes = 2;
  int integer{};
  EXPECT_THROW(read_json("123", integer, limits), rohit::serializer::exception::resource_limit);
  limits = {};
  limits.max_allocation_bytes = 0;
  EXPECT_THROW(read_json("[1]", values, limits), rohit::serializer::exception::resource_limit);
}

// Repeated fields and map keys use their last completed value without accumulating old contents.
TEST(decoder_qualification, replacement_and_duplicates) {
  std::map<int, std::string> values{{9, "previous"}};
  read_json(R"([{"key":1,"value":"first"},{"key":1,"value":"last"}])", values);
  ASSERT_EQ(values.size(), 1U);
  EXPECT_EQ(values.at(1), "last");
  read_json("[]", values);
  EXPECT_TRUE(values.empty());

  assessment::item item{};
  item.numbers = {99};
  read_json(R"({"na\u006de":"escaped key","numbers":[1],"numbers":[2,3],"status":"done"})", item);
  EXPECT_EQ(item.name, "escaped key");
  EXPECT_EQ(item.numbers, (std::vector<std::int32_t>{2, 3}));
  EXPECT_EQ(item.status, assessment::phase::done);
  EXPECT_THROW(read_json(R"({"status":"unknown"})", item), rohit::serializer::exception::bad_input_data);
  EXPECT_EQ(item.status, assessment::phase::done);
}

// A single decoder shares cumulative string storage and work accounting across separate reads.
TEST(decoder_qualification, cumulative_budgets) {
  constexpr std::string_view text = R"("ab" "cd")";
  const auto input = rohit::make_constant_full_stream(text.data(), text.size());
  rohit::serializer::decode_limits limits{};
  limits.max_allocation_bytes = 3;
  json_in decoder{input, limits};
  std::string value;
  decoder.serialize_in(value);
  EXPECT_EQ(value, "ab");
  EXPECT_THROW(decoder.serialize_in(value), rohit::serializer::exception::resource_limit);
  EXPECT_EQ(value, "ab");

  limits = {};
  limits.max_work_units = 1;
  int number = 7;
  EXPECT_THROW(read_json("1", number, limits), rohit::serializer::exception::resource_limit);
  EXPECT_EQ(number, 7);
}

// All protocols handle nested containers, enum collections, reuse, and empty generated objects.
TEST(decoder_qualification, generated_modes) {
  check_generated_round_trip<rohit::serializer::json>();
  check_generated_round_trip<rohit::serializer::binary_none>();
  check_generated_round_trip<rohit::serializer::binary_integer>();
  check_generated_round_trip<rohit::serializer::binary_string>();
  assessment::empty value{};
  EXPECT_NO_THROW(read_json("{}", value));
  EXPECT_THROW(read_json("{\"unknown\":1}", value), rohit::serializer::exception::key_not_found);
  rohit::full_stream_auto_alloc output{1};
  value.serialize_out<rohit::serializer::json>(output);
  EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(output.begin()), output.current_offset()), "{}");
}

// Schema evolution uses explicit IDs and rejects ambiguous ID/name assignments.
TEST(decoder_qualification, schema_identity_validation) {
  for (const auto text : {
      "class sample stable_ids { public uint8 a; }",
      "class sample { public uint8 a (2); public uint8 b; }",
      "class sample { public uint8 a (\"same\"); public uint8 b (\"same\"); }",
      "class sample { public uint8 a (0); }",
      "class sample { public uint8 a (429496729600); }",
      "class sample { public uint8 a (\"\"); }"}) {
    const auto input = rohit::make_constant_full_stream(text, std::char_traits<char>::length(text));
    EXPECT_THROW(rohit::serializer::parser::parse(input), rohit::exception::base_parser);
  }
  const std::string schema = "class sample stable_ids { public uint8 a (7); } // final comment";
  const auto input = rohit::make_constant_full_stream(schema);
  EXPECT_NO_THROW(rohit::serializer::parser::parse(input));
}

// New keyed readers retain defaults for missing fields; old readers still reject added fields.
TEST(decoder_qualification, schema_evolution) {
  check_schema_evolution<rohit::serializer::json>();
  check_schema_evolution<rohit::serializer::binary_integer>();
  check_schema_evolution<rohit::serializer::binary_string>();
}

// Payload excerpts require explicit opt-in and remain bounded.
TEST(decoder_qualification, diagnostics) {
  const std::string secret = "private-payload";
  const auto input = rohit::make_constant_full_stream(secret);
  const rohit::serializer::exception::bad_input_data error{input};
  EXPECT_EQ(error.code(), rohit::exception::parser_error_code::invalid_input);
  EXPECT_EQ(std::string_view{error.what()}.find(secret), std::string_view::npos);
  const rohit::serializer::exception::bad_input_data excerpt{
      input, "Invalid input", {.include_input_excerpt = true, .maximum_excerpt_bytes = 3}};
  EXPECT_NE(std::string_view{excerpt.what()}.find("pri"), std::string_view::npos);
  EXPECT_EQ(std::string_view{excerpt.what()}.find(secret), std::string_view::npos);
}
