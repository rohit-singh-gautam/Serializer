#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>
#include <gtest/gtest.h>
#include <protobuf.hpp>
#include <protobuf_reference.pb.h>

#include <limits>
#include <string>

namespace {
namespace codec = rohit::serializer;

// Encode generated Serializer objects using compile-time protocol selection.
template <template <codec::serialize_type> class Protocol>
std::string encode(const protobuf_test::record& value) {
  rohit::full_stream_auto_alloc stream;
  value.serialize_out<Protocol>(stream);
  return {reinterpret_cast<const char*>(stream.begin()), stream.current_offset()};
}

// Parse a reference-runtime message into the generated Serializer owning object.
template <template <codec::serialize_type> class Protocol>
protobuf_test::record decode(const std::string& bytes) {
  const auto stream = rohit::make_constant_stream(bytes.data(), bytes.size());
  protobuf_test::record value;
  value.serialize_in<Protocol>(stream);
  return value;
}

// Require agreement on all represented values independently of encoding order/default omission.
void check(const protobuf_test::record& value, const serializer_reference::Record& reference) {
  EXPECT_EQ(value.signed_value, reference.signed_value());
  EXPECT_EQ(value.large_value, reference.large_value());
  EXPECT_EQ(value.display_name, reference.display_name());
  EXPECT_EQ(value.flag, reference.flag());
  EXPECT_EQ(value.fraction, reference.fraction());
  EXPECT_EQ(value.precise, reference.precise());
  EXPECT_EQ(static_cast<int>(value.selection), static_cast<int>(reference.selection()));
  EXPECT_EQ(value.numbers,
            (std::vector<std::int32_t>{reference.numbers().begin(), reference.numbers().end()}));
  EXPECT_EQ(value.counts.size(), reference.counts().size());
  for (const auto& [key, mapped] : value.counts) {
    EXPECT_EQ(mapped, reference.counts().at(key));
  }
  EXPECT_EQ(value.nested.number, reference.nested().number());
  EXPECT_EQ(value.nested.text, reference.nested().text());
  EXPECT_EQ(value.payload_type, protobuf_test::record::e_payload::selected);
  EXPECT_EQ(static_cast<int>(value.payload.selected),
            static_cast<int>(reference.payload().selected()));
  ASSERT_EQ(value.flags.size(), static_cast<std::size_t>(reference.flags_size()));
  for (int index = 0; index < reference.flags_size(); ++index) {
    EXPECT_EQ(value.flags[static_cast<std::size_t>(index)], reference.flags(index));
  }
  EXPECT_EQ(value.small, reference.small());
  EXPECT_EQ(static_cast<unsigned char>(value.character), reference.character());
}

// Exchange all three standard formats with Google's independent generated codec implementation.
TEST(protobuf_interoperability, both_directions_all_formats) {
  serializer_reference::Record reference;
  reference.set_signed_value(-150);
  reference.set_large_value(std::numeric_limits<std::uint64_t>::max());
  reference.set_display_name("Ada\n\"\\\0text", 11);
  reference.set_flag(true);
  reference.set_fraction(-0.25F);
  reference.set_precise(1.0e120);
  reference.set_selection(serializer_reference::second_value);
  reference.add_numbers(0);
  reference.add_numbers(-1);
  reference.add_numbers(150);
  (*reference.mutable_counts())["x"] = std::numeric_limits<std::uint64_t>::max();
  reference.mutable_nested()->set_number(-42);
  reference.mutable_nested()->set_text("nested");
  reference.mutable_payload()->set_selected(serializer_reference::second_value);
  reference.add_flags(true);
  reference.add_flags(false);
  reference.set_small(-128);
  reference.set_character(255);

  const auto from_binary = decode<codec::protobuf_binary>(reference.SerializeAsString());
  check(from_binary, reference);
  std::string json;
  ASSERT_TRUE(google::protobuf::util::MessageToJsonString(reference, &json).ok());
  check(decode<codec::protojson>(json), reference);
  std::string text;
  ASSERT_TRUE(google::protobuf::TextFormat::PrintToString(reference, &text));
  check(decode<codec::textproto>(text), reference);

  serializer_reference::Record binary_copy;
  ASSERT_TRUE(binary_copy.ParseFromString(encode<codec::protobuf_binary>(from_binary)));
  check(from_binary, binary_copy);
  serializer_reference::Record json_copy;
  ASSERT_TRUE(
      google::protobuf::util::JsonStringToMessage(encode<codec::protojson>(from_binary), &json_copy)
          .ok());
  check(from_binary, json_copy);
  serializer_reference::Record text_copy;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(encode<codec::textproto>(from_binary),
                                                            &text_copy));
  check(from_binary, text_copy);
}
} // namespace
