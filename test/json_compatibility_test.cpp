#include "generics.hpp"
#include <gtest/gtest.h>
#include <optional>
#include <rohit/serializer.hpp>
#include <string>
#include <string_view>

namespace {
namespace codec = rohit::serializer;
using record = native_generics::result<std::optional<std::string>>;

// Read generated fields with an explicit compatibility policy and caller-owned resource limits.
template <codec::json_read_policy Policy = codec::json_read_policy::compatible>
record read_record(std::string_view bytes, codec::decode_limits limits = {}) {
  auto stream = rohit::make_constant_stream(bytes.data(), bytes.size());
  codec::json<codec::serialize_type::in, rohit::stream, Policy> input(stream, limits);
  record result{};
  input.serialize_in(result);
  input.finish();
  return result;
}

// Native optional values retain their JSON null representation in generated generic fields.
TEST(JsonCompatibility, RoundTripsNullableGeneratedFields) {
  for (const auto& value : {std::optional<std::string>{}, std::optional<std::string>{"text"}}) {
    record original{};
    original.value = value;
    original.success = true;
    rohit::full_stream_auto_alloc stream;
    codec::json_out<false, rohit::full_stream_auto_alloc> output(stream);
    output.serialize_out(original);
    const auto decoded =
        read_record({reinterpret_cast<const char*>(stream.begin()), stream.current_offset()});
    EXPECT_EQ(decoded.value, value);
    EXPECT_TRUE(decoded.success);
  }
}

// New fields can contain complete arbitrary JSON without changing known fields or strict defaults.
TEST(JsonCompatibility, SkipsAdditiveFieldsAndPreservesStrictDefault) {
  constexpr std::string_view bytes =
      R"({"future":{"array":[null,true,false,"text",1e9999,{}]},"value":"old","success":true})";
  const auto decoded = read_record(bytes);
  EXPECT_EQ(decoded.value, "old");
  EXPECT_TRUE(decoded.success);
  EXPECT_THROW(read_record<codec::json_read_policy::strict>(bytes),
               codec::exception::key_not_found);
}

// Reject ambiguous known and unknown names, including escapes and nested unknown objects.
TEST(JsonCompatibility, RejectsDuplicateKeysMalformedValuesAndTrailingData) {
  for (const auto* bytes :
       {R"({"success":true,"success":false})", R"({"future":1,"future":2})",
        R"({"success":true,"succe\u0073s":false})", R"({"future":{"x":1,"x":2}})",
        R"({"future":[1,]})", R"({"future":01})", R"({"future":tru})", R"({} {})"}) {
    EXPECT_THROW(read_record(bytes), codec::exception::bad_input_data) << bytes;
  }
}

// Skipped values remain bound by every budget enforced for typed fields.
TEST(JsonCompatibility, BoundsUnknownFields) {
  codec::decode_limits limits{};
  limits.max_nesting_depth = 2;
  EXPECT_THROW(read_record(R"({"future":[[[]]]})", limits), codec::exception::resource_limit);
  limits = {};
  limits.max_collection_elements = 1;
  EXPECT_THROW(read_record(R"({"future":[1,2]})", limits), codec::exception::resource_limit);
  limits = {};
  limits.max_allocation_bytes = 1;
  EXPECT_THROW(read_record(R"({"future":null})", limits), codec::exception::resource_limit);
  limits = {};
  limits.max_string_bytes = 2;
  EXPECT_THROW(read_record(R"({"x":"long"})", limits), codec::exception::resource_limit);
  limits = {};
  limits.max_input_bytes = 4;
  EXPECT_THROW(read_record(R"({"future":true})", limits), codec::exception::resource_limit);
  limits = {};
  limits.max_work_units = 4;
  EXPECT_THROW(read_record(R"({"future":true})", limits), codec::exception::resource_limit);
}
} // namespace
