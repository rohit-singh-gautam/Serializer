#include <gtest/gtest.h>
#include <protobuf.hpp>
#include <rohit/protobuf.hpp>
#include <rohit/serializer_creator.hpp>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace codec = rohit::serializer;

// Encode through the same compile-time protocol API used by the established codecs.
template <template <codec::serialize_type> class Protocol, typename T>
std::string encode(const T& value) {
  rohit::full_stream_auto_alloc stream;
  value.template serialize_out<Protocol>(stream);
  const auto encoded_size = stream.current_offset();
  // Empty Protobuf messages leave the lazily allocated stream without storage.
  if (encoded_size == 0) {
    return {};
  }
  return {reinterpret_cast<const char*>(stream.begin()), encoded_size};
}

// Decode an exact message through generated direct field dispatch.
template <template <codec::serialize_type> class Protocol, typename T = protobuf_test::record>
T decode(std::string_view bytes, codec::decode_limits limits = {}) {
  const auto stream = rohit::make_constant_stream(bytes.data(), bytes.size());
  Protocol<codec::serialize_type::in> protocol{stream, limits};
  T value{};
  value.serialize_in(protocol);
  protocol.finish();
  return value;
}

// Exercise scalar boundaries, packed arrays, maps, nesting, and union selection together.
template <template <codec::serialize_type> class Protocol>
void round_trip() {
  protobuf_test::record value{};
  value.signed_value = -150;
  value.large_value = std::numeric_limits<std::uint64_t>::max();
  value.display_name = "Ada\n\"\\";
  value.flag = true;
  value.fraction = -0.25F;
  value.precise = 1.0e120;
  value.selection = protobuf_test::choice::second_value;
  value.numbers = {0, -1, 150};
  value.counts = {{"one", 1}, {"maximum", value.large_value}};
  value.nested.number = -42;
  value.nested.text = "nested";
  value.payload_type = protobuf_test::record::e_payload::selected;
  std::construct_at(&value.payload.selected, protobuf_test::choice::second_value);
  value.flags = {true, false};
  value.small = -128;
  value.character = static_cast<char>(255);
  const auto encoded = encode<Protocol>(value);
  const auto result = decode<Protocol>(encoded);
  EXPECT_EQ(encode<Protocol>(result), encoded);
  EXPECT_EQ(result.counts, value.counts);
  EXPECT_EQ(result.numbers, value.numbers);
  EXPECT_EQ(result.payload_type, value.payload_type);
  EXPECT_EQ(result.payload.selected, value.payload.selected);
  EXPECT_EQ(result.character, value.character);
}

// Verify all three formats are concrete compile-time protocol specializations.
TEST(protobuf_codec, round_trips) {
  round_trip<codec::protobuf_binary>();
  round_trip<codec::protojson>();
  round_trip<codec::textproto>();
}

// Check published Protobuf tag/varint bytes independently of our own decoder.
TEST(protobuf_codec, golden_binary) {
  protobuf_test::child value{};
  value.number = 150;
  value.text = "A";
  EXPECT_EQ(encode<codec::protobuf_binary>(value), std::string("\x08\x96\x01\x12\x01"
                                                               "A",
                                                               6));
  const auto copy = decode<codec::protobuf_binary, protobuf_test::child>("\x08\x96\x01");
  EXPECT_EQ(copy.number, 150);
  EXPECT_TRUE(copy.text.empty());
}

// Verify ProtoJSON's quoted uint64s, lowerCamel names, nulls, and object-shaped maps.
TEST(protobuf_codec, protojson_rules) {
  const auto copy = decode<codec::protojson>(
      R"({"largeValue":"18446744073709551615","signed_value":1.5e2,"displayName":null,"counts":{"x":"42"}})");
  EXPECT_EQ(copy.large_value, std::numeric_limits<std::uint64_t>::max());
  EXPECT_EQ(copy.signed_value, 150);
  EXPECT_EQ(copy.counts.at("x"), 42);
  const auto json = encode<codec::protojson>(copy);
  EXPECT_NE(json.find("\"largeValue\":\"18446744073709551615\""), std::string::npos);
  EXPECT_NE(json.find("\"counts\":{\"x\":\"42\"}"), std::string::npos);
  EXPECT_THROW(decode<codec::protojson>(R"({"numbers":[null]})"), std::exception);
  EXPECT_THROW(decode<codec::protojson>(R"({"signedValue":1.5})"), std::exception);
}

// Duplicate JSON fields replace complete values, including aliases and explicit null defaults.
TEST(protobuf_codec, protojson_duplicate_replacement) {
  const auto copy = decode<codec::protojson>(
      R"({"signed_value":1,"signedValue":2,"displayName":"old","display_name":null,)"
      R"("nested":{"number":1,"text":"old"},"nested":{"number":2},)"
      R"("numbers":[1,2],"numbers":[3],"counts":{"old":"1"},"counts":{"new":"2"}})");
  EXPECT_EQ(copy.signed_value, 2);
  EXPECT_TRUE(copy.display_name.empty());
  EXPECT_EQ(copy.nested.number, 2);
  EXPECT_TRUE(copy.nested.text.empty());
  EXPECT_EQ(copy.numbers, (std::vector<std::int32_t>{3}));
  EXPECT_EQ(copy.counts, (std::map<std::string, std::uint64_t>{{"new", 2}}));

  const auto cleared = decode<codec::protojson>(
      R"({"signedValue":42,"signed_value":null,"nested":{"number":3},"nested":null,)"
      R"("numbers":[1],"numbers":null,"counts":{"old":"1"},"counts":null})");
  EXPECT_EQ(cleared.signed_value, 0);
  EXPECT_EQ(cleared.nested.number, 0);
  EXPECT_TRUE(cleared.nested.text.empty());
  EXPECT_TRUE(cleared.numbers.empty());
  EXPECT_TRUE(cleared.counts.empty());
}

// Each JSON union wrapper is a replacement; null alternatives clear only their own prior value.
TEST(protobuf_codec, protojson_union_replacement) {
  for (const auto wire : {
           R"({"payload":{"selected":"second_value"},"payload":{}})",
           R"({"payload":{"selected":"second_value"},"payload":null})",
           R"({"payload":{"number":42,"number":null}})",
           R"({"payload":{"selected":"second_value","selected":null}})"}) {
    const auto copy = decode<codec::protojson>(wire);
    EXPECT_EQ(copy.payload_type, protobuf_test::record::e_payload::number);
    EXPECT_EQ(copy.payload.number, 0);
  }
  const auto copy = decode<codec::protojson>(
      R"({"payload":{"number":42},"payload":{"selected":"second_value","number":null}})");
  EXPECT_EQ(copy.payload_type, protobuf_test::record::e_payload::selected);
  EXPECT_EQ(copy.payload.selected, protobuf_test::choice::second_value);
  const auto replaced = decode<codec::protojson>(
      R"({"payload":{"number":42,"number":null,"selected":"second_value"}})");
  EXPECT_EQ(replaced.payload_type, protobuf_test::record::e_payload::selected);
  EXPECT_EQ(replaced.payload.selected, protobuf_test::choice::second_value);
  EXPECT_THROW(decode<codec::protojson>(
                   R"({"payload":{"number":42,"selected":"second_value"}})"),
               codec::exception::bad_input_data);
}

// TextProto charges decoded bytes once, cumulatively across escapes, fragments, and fields.
TEST(protobuf_codec, textproto_string_storage_limits) {
  codec::decode_limits limits{};
  limits.max_allocation_bytes = 4;
  const auto copy = decode<codec::textproto>(
      "display_name: 'a' '\\u0062' nested { text: '\\u00e9' }", limits);
  EXPECT_EQ(copy.display_name, "ab");
  EXPECT_EQ(copy.nested.text, "\xc3\xa9");
  limits.max_allocation_bytes = 3;
  EXPECT_THROW(decode<codec::textproto>(
                   "display_name: 'a' '\\u0062' nested { text: '\\u00e9' }", limits),
               codec::exception::resource_limit);
  limits.max_allocation_bytes = 0;
  EXPECT_TRUE(decode<codec::textproto>("display_name: ''", limits).display_name.empty());
  limits = {};
  limits.max_string_bytes = 12;
  EXPECT_THROW(decode<codec::textproto>("display_name: 'abcdefghijk\\U0001f600'", limits),
               codec::exception::resource_limit);
}

// Decode long whitespace gaps between every token without changing quoted string contents.
TEST(protobuf_codec, protojson_whitespace_runs) {
  constexpr std::string_view tokens[] = {"{",
                                         "\"signedValue\"",
                                         ":",
                                         "-42",
                                         ",",
                                         "\"displayName\"",
                                         ":",
                                         R"("keep  spaces\tand\nescapes")",
                                         ",",
                                         "\"numbers\"",
                                         ":",
                                         "[",
                                         "1",
                                         ",",
                                         "-2",
                                         ",",
                                         "3",
                                         "]",
                                         ",",
                                         "\"counts\"",
                                         ":",
                                         "{",
                                         "\"x\"",
                                         ":",
                                         "\"42\"",
                                         "}",
                                         ",",
                                         "\"nested\"",
                                         ":",
                                         "{",
                                         "\"number\"",
                                         ":",
                                         "7",
                                         ",",
                                         "\"text\"",
                                         ":",
                                         "\"nested  text\"",
                                         "}",
                                         "}"};
  constexpr std::string_view whitespace = " \t\r\n";
  for (const std::size_t gap_size : {1u, 7u, 8u, 15u, 16u, 23u, 24u, 31u, 32u, 39u, 40u, 97u}) {
    std::string gap(gap_size, ' ');
    for (std::size_t index = 0; index < gap.size(); ++index) {
      gap[index] = whitespace[index % whitespace.size()];
    }
    std::string wire = gap;
    for (const auto token : tokens) {
      wire += token;
      wire += gap;
    }
    const auto copy = decode<codec::protojson>(wire);
    EXPECT_EQ(copy.signed_value, -42);
    EXPECT_EQ(copy.display_name, "keep  spaces\tand\nescapes");
    EXPECT_EQ(copy.numbers, (std::vector<std::int32_t>{1, -2, 3}));
    EXPECT_EQ(copy.counts.at("x"), 42);
    EXPECT_EQ(copy.nested.number, 7);
    EXPECT_EQ(copy.nested.text, "nested  text");
  }
}

// Invalid whitespace after a valid field must still reject the replacement object.
TEST(protobuf_codec, protojson_invalid_whitespace) {
  const std::string gap(65, ' ');
  for (const auto invalid : {"\v", "\f", "\xc2\xa0", "\xe2\x80\xa8", "# comment\n"}) {
    const auto wire = "{\"signedValue\":1," + gap + invalid + "\"small\":2}";
    const auto input = rohit::make_constant_stream(wire.data(), wire.size());
    protobuf_test::record value{};
    value.signed_value = 99;
    EXPECT_THROW(value.serialize_in<codec::protojson>(input), codec::exception::bad_input_data);
    EXPECT_EQ(value.signed_value, 99);
  }
}

// Accept standard TextProto comments, list syntax, alternate braces, and escaped strings.
TEST(protobuf_codec, textproto_rules) {
  const auto copy = decode<codec::textproto>(
      "# comment\nsigned_value: -0x2a; numbers: [1, 02, 3] nested < number: 7 > display_name: 'a' "
      "\"\\142\\x63\" selection: second_value");
  EXPECT_EQ(copy.signed_value, -42);
  EXPECT_EQ(copy.numbers, (std::vector<std::int32_t>{1, 2, 3}));
  EXPECT_EQ(copy.nested.number, 7);
  EXPECT_EQ(copy.display_name, "abc");
  const auto maps = decode<codec::textproto>("counts: [{key:'x' value:1}, {key:'y' value:2}]");
  EXPECT_EQ(maps.counts.at("y"), 2);
  EXPECT_THROW(decode<codec::textproto>("small: 1 small: 2"), std::exception);
  EXPECT_THROW(decode<codec::protojson>(R"({"payload":{"number":1,"selected":"first_value"}})"),
               std::exception);
}

// Unknown binary tags are skippable; named formats and malformed wire data fail.
TEST(protobuf_codec, unknown_and_malformed) {
  const auto copy = decode<codec::protobuf_binary>(std::string("\xf8\x07\x01\x08\x2a", 5));
  EXPECT_EQ(copy.signed_value, 42);
  EXPECT_THROW(decode<codec::protobuf_binary>(std::string("\x1a\x05x", 3)), std::exception);
  EXPECT_THROW(decode<codec::protobuf_binary>(std::string("\x08\x80", 2)), std::exception);
  EXPECT_THROW(decode<codec::protobuf_binary>(std::string("\0", 1)), std::exception);
  EXPECT_THROW(decode<codec::protojson>("{\"unknown\":1}"), std::exception);
  EXPECT_THROW(decode<codec::textproto>("unknown: 1"), std::exception);
  EXPECT_THROW(decode<codec::protojson>("{\"small\":128}"), std::exception);
}

// Limits are shared across repeated fields and nested messages before destination allocation.
TEST(protobuf_codec, limits_and_transactional_failure) {
  codec::decode_limits limits{};
  limits.max_collection_elements = 2;
  EXPECT_THROW(decode<codec::protojson>("{\"numbers\":[1,2],\"flags\":[true]}", limits),
               std::exception);
  limits.max_string_bytes = 2;
  EXPECT_THROW(decode<codec::textproto>("display_name: 'abcd'", limits), std::exception);
  protobuf_test::record value{};
  value.signed_value = 99;
  const std::string invalid{"{\"signedValue\":1,\"small\":999}"};
  const auto input = rohit::make_constant_stream(invalid.data(), invalid.size());
  EXPECT_THROW(value.serialize_in<codec::protojson>(input), std::exception);
  EXPECT_EQ(value.signed_value, 99);
}

// A compatible schema must be validated before publishing any generated source.
TEST(protobuf_codec, generator_rejects_incompatible_schemas) {
  const auto generate = [](std::string_view text) {
    const auto input = rohit::make_constant_stream(text.data(), text.size());
    const auto schema = codec::parser::parse(input);
    codec::writer::cpp_options options{};
    options.format = false;
    options.protobuf = true;
    rohit::full_stream_auto_alloc output;
    codec::writer::cpp::write(output, schema, options);
  };
  EXPECT_THROW(generate("class example { public uint32 field (19000); }"), std::invalid_argument);
  EXPECT_THROW(generate("class example { public uint32 field (536870912); }"),
               std::invalid_argument);
  EXPECT_THROW(generate("class example { public map(float) uint32 field; }"),
               std::invalid_argument);
  EXPECT_THROW(generate("class example { public uint32 field (\"not-valid\", 1); }"),
               std::exception);
  EXPECT_THROW(generate("class example { public uint32 a (\"some_name\", 1); public uint32 b "
                        "(\"someName\", 2); }"),
               std::invalid_argument);
  EXPECT_THROW(generate("class example view { public uint32 field; }"), std::invalid_argument);
}

// Readers accept both packed and unpacked repetitions and merge nested message occurrences.
TEST(protobuf_codec, repeated_and_nested_binary_segments) {
  const auto copy = decode<codec::protobuf_binary>(
      std::string{"\x40\x01\x42\x02\x02\x03\x52\x02\x08\x07\x52\x03\x12\x01x", 15});
  EXPECT_EQ(copy.numbers, (std::vector<std::int32_t>{1, 2, 3}));
  EXPECT_EQ(copy.nested.number, 7);
  EXPECT_EQ(copy.nested.text, "x");
  const auto negative = decode<codec::protobuf_binary>(std::string{"\x08\xff\xff\xff\xff\x0f", 6});
  EXPECT_EQ(negative.signed_value, -1);
  EXPECT_THROW(decode<codec::protobuf_binary>(std::string{"\x42\x01\x80", 3}), std::exception);
  EXPECT_THROW(decode<codec::protobuf_binary>(std::string{"\x52\x01\x08\x01", 4}), std::exception);
  const auto union_copy =
      decode<codec::protobuf_binary>(std::string{"\x5a\x02\x10\x01\x5a\x00", 6});
  EXPECT_EQ(union_copy.payload_type, protobuf_test::record::e_payload::selected);
  EXPECT_EQ(union_copy.payload.selected, protobuf_test::choice::second_value);
}

// Preserve native nonfinite values in the Protobuf-specific text spellings.
TEST(protobuf_codec, special_float_and_json_number_rules) {
  const auto copy =
      decode<codec::protojson>(R"({"fraction":"NaN","precise":"-Infinity","signedValue":0.0})");
  EXPECT_TRUE(std::isnan(copy.fraction));
  EXPECT_EQ(copy.precise, -std::numeric_limits<double>::infinity());
  EXPECT_EQ(copy.signed_value, 0);
  EXPECT_NE(encode<codec::textproto>(copy).find("fraction: nan"), std::string::npos);
  EXPECT_THROW(decode<codec::protojson>(R"({"signedValue":01})"), std::exception);
  EXPECT_THROW(decode<codec::protojson>(R"({"precise":NaN})"), std::exception);
  EXPECT_THROW(decode<codec::protojson>(R"({"signedValue":1e100})"), std::exception);
  EXPECT_THROW(decode<codec::protojson>(R"({"signedValue":1e-1})"), std::exception);
  EXPECT_THROW(decode<codec::protojson>(R"({"largeValue":"18446744073709551616"})"),
               std::exception);
}

// Parent composition and empty roots use the same compile-time protocol entry points.
TEST(protobuf_codec, parents_empty_and_limits) {
  protobuf_test::derived value{};
  value.number = 7;
  value.text = "base";
  value.extra = 42;
  const auto binary = encode<codec::protobuf_binary>(value);
  const auto copy = decode<codec::protobuf_binary, protobuf_test::derived>(binary);
  EXPECT_EQ(copy.number, 7);
  EXPECT_EQ(copy.text, "base");
  EXPECT_EQ(copy.extra, 42);
  EXPECT_EQ(encode<codec::protojson>(protobuf_test::empty{}), "{}");
  EXPECT_EQ(encode<codec::protobuf_binary>(protobuf_test::empty{}), "");
  static_cast<void>(decode<codec::protobuf_binary, protobuf_test::empty>(""));
  codec::decode_limits limits{};
  limits.max_input_bytes = 1;
  EXPECT_THROW(decode<codec::protobuf_binary>(std::string{"\x08\x96\x01", 3}, limits),
               std::exception);
  limits = {};
  limits.max_nesting_depth = 1;
  EXPECT_THROW(decode<codec::protojson>(R"({"nested":{}})", limits), std::exception);
  limits = {};
  limits.max_allocation_bytes = 1;
  EXPECT_THROW(decode<codec::protojson>(R"({"numbers":[1]})", limits), std::exception);
  EXPECT_THROW(decode<codec::textproto>("display_name: '\\777'"), std::exception);
}
} // namespace
