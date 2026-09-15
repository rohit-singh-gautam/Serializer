#include <rohit/serializer_creator.hpp>
#include <views.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {
using rohit::serializer::storage_mode;
using rohit::serializer::serialize_type;
using owning_record = view_test::record<storage_mode::owning>;
using readonly_record = view_test::record<storage_mode::read_only_view>;
using mutable_record = view_test::record<storage_mode::mutable_view>;

// Detect writable access without instantiating an invalid expression outside a template.
template <typename View>
concept has_value_setter = requires(View& view) { view.set_value(std::uint32_t{7}); };

// Verify that a view cannot be created from immutable storage when it permits mutation.
template <typename View>
concept maps_const_bytes = requires(std::span<const std::uint8_t> bytes) { View::map(bytes); };

static_assert(!has_value_setter<view_test::readonly_record>);
static_assert(has_value_setter<view_test::mutable_record>);
static_assert(maps_const_bytes<readonly_record>);
static_assert(!maps_const_bytes<mutable_record>);
static_assert(readonly_record::wire_endian == std::endian::little);
static_assert(std::is_default_constructible_v<view_test::owning_readonly<storage_mode::owning>>);
static_assert(std::is_default_constructible_v<view_test::owning_mutable<storage_mode::owning>>);
static_assert(has_value_setter<view_test::both_views<storage_mode::mutable_view>>);
static_assert(!has_value_setter<view_test::both_views<storage_mode::read_only_view>>);

// Produce schema-generated source for parser/generator assertions when tests are later enabled.
std::string generate(std::string_view schema) {
  const auto input = rohit::make_constant_stream(schema.data(), schema.size());
  auto statements = rohit::serializer::parser::parse(input);
  rohit::full_stream_auto_alloc output{1024};
  rohit::serializer::writer::cpp::write(output, statements);
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}
} // namespace

// Exercise aliasing, nested views, parents, arrays, maps, active unions, and round-trip decoding.
TEST(binary_view, edits_preserve_the_encoded_layout) {
  owning_record original{};
  original.code = 0x12345678;
  original.label = "base";
  original.enabled = true;
  original.score = 1.0f;
  original.values = {10, 20};
  original.labels = {{1, "one"}, {2, "two"}};
  original.nested.code = 30;
  original.nested.label = "child";
  original.children.push_back(original.nested);
  original.payload.number = 42;
  original.status = view_test::phase::idle;
  rohit::full_stream_auto_alloc output{256};
  original.serialize_out<rohit::serializer::binary_none>(output);
  const auto size = output.current_offset();
  auto bytes = std::span<std::uint8_t>{output.begin(), size};
  auto editor = mutable_record::map(bytes);
  auto reader = readonly_record::map(bytes);
  EXPECT_EQ(reader.get_base_1().get_code(), 0x12345678U);
  EXPECT_EQ(bytes[0], 0x78);
  editor.get_base_1().set_label("root");
  editor.set_enabled(false);
  editor.set_score(-2.0f);
  editor.get_values().set(1, std::uint16_t{250});
  editor.get_labels().set_value(0, "ONE");
  editor.get_nested().set_code(31);
  editor.get_children().at(0).set_label("other");
  editor.get_payload().set<0>(std::uint32_t{99});
  editor.set_status(view_test::phase::active);
  EXPECT_EQ(reader.get_base_1().get_label(), "root");
  EXPECT_FALSE(reader.get_enabled());
  EXPECT_EQ(reader.get_score(), -2.0f);
  EXPECT_EQ(reader.get_values().at(1), 250);
  EXPECT_EQ(reader.get_labels().key_at(0), 1U);
  EXPECT_EQ(reader.get_labels().value_at(0), "ONE");
  EXPECT_EQ(reader.get_nested().get_code(), 31U);
  EXPECT_EQ(reader.get_children().at(0).get_label(), "other");
  EXPECT_EQ(reader.get_payload().get<0>(), 99U);
  EXPECT_EQ(reader.get_status(), view_test::phase::active);
  EXPECT_EQ(output.current_offset(), size);

  const std::vector<std::uint8_t> before{bytes.begin(), bytes.end()};
  EXPECT_THROW(editor.get_base_1().set_label("too long"), std::length_error);
  EXPECT_THROW(editor.get_payload().set<1>(1.0f), std::invalid_argument);
  EXPECT_THROW(editor.set_status(static_cast<view_test::phase>(7)), std::invalid_argument);
  EXPECT_THROW(editor.get_values().set(2, 3), std::out_of_range);
  EXPECT_TRUE(std::equal(before.begin(), before.end(), bytes.begin()));

  owning_record decoded{};
  const auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
  rohit::serializer::binary_none<serialize_type::in> decoder{input};
  decoder.serialize_in(decoded);
  decoder.finish();
  EXPECT_EQ(decoded.label, "root");
  EXPECT_EQ(decoded.children.front().label, "other");
  EXPECT_EQ(decoded.payload.number, 99U);

  rohit::full_stream_auto_alloc copy{1};
  rohit::serializer::binary_none<serialize_type::out> encoder{copy};
  encoder.serialize_out(editor);
  ASSERT_EQ(copy.current_offset(), bytes.size());
  EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(), copy.begin()));
}

// Reject malformed extents, trailing input, and resource exhaustion during mapping.
TEST(binary_view, mapping_limits_and_single_mode_classes) {
  std::array<std::uint8_t, 4> bytes{0x78, 0x56, 0x34, 0x12};
  auto editor = view_test::mutable_record::map(bytes);
  editor.set_value(0xabcdef01);
  EXPECT_EQ(view_test::readonly_record::map(bytes).get_value(), 0xabcdef01U);
  EXPECT_EQ(global_view::map(bytes).get_child().get_value(), 0xabcdef01U);
  EXPECT_THROW(view_test::readonly_record::map(std::span{bytes}.first(3)),
               rohit::serializer::exception::bad_input_data);
  std::array<std::uint8_t, 5> extra{};
  EXPECT_THROW(view_test::readonly_record::map(extra), rohit::serializer::exception::bad_input_data);
  rohit::serializer::decode_limits limits{};
  limits.max_input_bytes = 3;
  EXPECT_THROW(view_test::readonly_record::map(bytes, limits), rohit::serializer::exception::resource_limit);
  EXPECT_TRUE(view_test::empty::map({}).serialized_bytes().empty());
  std::array<std::uint8_t, 2> recursive{1, 0};
  EXPECT_EQ(view_test::recursive::map(recursive).get_children().at(0).get_children().size(), 0U);
  limits = {};
  limits.max_nesting_depth = 1;
  EXPECT_THROW(view_test::recursive::map(recursive, limits), rohit::serializer::exception::resource_limit);
}

// Attribute permutations and defaults must produce identical selected representations.
TEST(binary_view, schema_modes_and_diagnostics) {
  EXPECT_EQ(generate("class person view {}"), generate("class person readonly mutable view {}"));
  EXPECT_EQ(generate("class person {}"), generate("class person owning {}"));
  EXPECT_EQ(generate("class person view owning {}"), generate("class person owning view {}"));
  const auto single = generate("class person readonly view {}");
  EXPECT_EQ(single.find("template <rohit::serializer::storage_mode"), std::string::npos);
  EXPECT_EQ(generate("class person {}").find("rohit/binary_view.hpp"), std::string::npos);
  for (const auto schema : {
      "class person readonly {}", "class person mutable {}", "class person packed view {}",
      "class child {} class person view { public child value; }",
      "class child view {} class person { public child value; }",
      "class map view {}", "class get_value view { public uint32 value; }"}) {
    EXPECT_THROW(generate(schema), rohit::serializer::exception::bad_class) << schema;
  }
}

// Explicit byte order changes fixed-width payloads while compact prefixes remain identical.
TEST(binary_view, explicit_codec_byte_order) {
  using little = rohit::serializer::binary_none<serialize_type::out>;
  using big = rohit::serializer::binary<serialize_type::out,
      rohit::serializer::serialize_key_type::none, std::endian::big>;
  static_assert(little::wire_endian == std::endian::little);
  static_assert(big::wire_endian == std::endian::big);
  std::array<std::uint8_t, 10> little_bytes{}, big_bytes{};
  rohit::full_stream little_stream{little_bytes.data(), little_bytes.size()};
  rohit::full_stream big_stream{big_bytes.data(), big_bytes.size()};
  little little_encoder{little_stream};
  big big_encoder{big_stream};
  little_encoder.serialize_out_variable(64);
  big_encoder.serialize_out_variable(64);
  little_encoder.serialize_out(std::uint32_t{0x12345678});
  big_encoder.serialize_out(std::uint32_t{0x12345678});
  little_encoder.serialize_out(1.0f);
  big_encoder.serialize_out(1.0f);
  EXPECT_EQ(little_bytes, (std::array<std::uint8_t, 10>{0x40, 0x40, 0x78, 0x56, 0x34, 0x12, 0, 0, 0x80, 0x3f}));
  EXPECT_EQ(big_bytes, (std::array<std::uint8_t, 10>{0x40, 0x40, 0x12, 0x34, 0x56, 0x78, 0x3f, 0x80, 0, 0}));
  const auto input = rohit::make_constant_stream(big_bytes.data(), big_bytes.size());
  rohit::serializer::binary<serialize_type::in, rohit::serializer::serialize_key_type::none,
      std::endian::big> decoder{input};
  EXPECT_EQ(decoder.serialize_in_variable(), 64U);
  std::uint32_t value{};
  float fraction{};
  decoder.serialize_in(value);
  decoder.serialize_in(fraction);
  decoder.finish();
  EXPECT_EQ(value, 0x12345678U);
  EXPECT_EQ(fraction, 1.0f);
}
