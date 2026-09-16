#include <rohit/protobuf.hpp>
#include <rohit/runtime_simd.hpp>
#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <assessment.hpp>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace codec = rohit::serializer;
namespace simd = codec::detail;
using json_input = codec::json<codec::serialize_type::in>;
using protojson_input = codec::protojson<codec::serialize_type::in>;
constexpr auto whitespace_kind = simd::json_scan_kind::whitespace;

// Generate mixed legal whitespace, including CR/LF pairs that cross vector boundaries.
std::string whitespace_run(std::size_t size) {
  constexpr std::string_view pattern = " \t\r\n";
  std::string result(size, ' ');
  for (std::size_t index = 0; index < size; ++index) {
    result[index] = pattern[index % pattern.size()];
  }
  return result;
}

// Decorate only JSON punctuation outside strings, leaving number tokens and string contents intact.
std::string add_whitespace(std::string_view input, std::size_t gap_size) {
  const auto gap = whitespace_run(gap_size);
  std::string result = gap;
  bool quoted{};
  bool escaped{};
  for (const char byte : input) {
    if (quoted) {
      result += byte;
      if (escaped) {
        escaped = false;
      } else if (byte == '\\') {
        escaped = true;
      } else if (byte == '"') {
        quoted = false;
      }
    } else if (byte == '"') {
      result += byte;
      quoted = true;
    } else if (std::string_view{"{}[]:,"}.find(byte) != std::string_view::npos) {
      result += gap;
      result += byte;
      result += gap;
    } else {
      result += byte;
    }
  }
  result += gap;
  return result;
}

// Compare every selectable backend to the caller's independent expected stopping position.
void check_scan(const std::uint8_t* data, std::size_t size, std::size_t expected) {
  EXPECT_EQ(simd::scan_json_scalar<whitespace_kind>(data, size), expected);
  EXPECT_EQ(simd::scan_json_baseline(data, size, whitespace_kind), expected);
  EXPECT_EQ(simd::scan_json_long(data, size, whitespace_kind), expected);
  EXPECT_EQ(simd::scan_json_prefix<whitespace_kind>(data, size), expected);
}

// Finish may consume trailing whitespace only within the remaining input and work budgets.
template <typename Decoder>
void check_finish_limits() {
  constexpr std::size_t whitespace_bytes = 97;
  const auto padding = whitespace_run(whitespace_bytes);
  for (const bool trailing_token : {false, true}) {
    const auto wire = padding + (trailing_token ? "x" : "");
    for (std::size_t budget = 0; budget <= wire.size() + 1; ++budget) {
      for (const bool work_budget : {false, true}) {
        codec::decode_limits limits{};
        limits.max_allocation_bytes = 0;
        if (work_budget) {
          limits.max_work_units = budget;
        } else {
          limits.max_input_bytes = budget;
        }
        const auto input = rohit::make_constant_full_stream(wire.data(), wire.size());
        Decoder decoder{input, limits};
        if (budget < wire.size()) {
          EXPECT_THROW(decoder.finish(), codec::exception::resource_limit);
          EXPECT_EQ(input.current_offset(), budget);
        } else if (trailing_token) {
          EXPECT_THROW(decoder.finish(), codec::exception::bad_input_data);
          EXPECT_EQ(input.current_offset(), whitespace_bytes);
        } else {
          EXPECT_NO_THROW(decoder.finish());
          EXPECT_EQ(input.current_offset(), wire.size());
        }
      }
    }
  }
}
} // namespace

// Only 09, 0a, 0d, and 20 are JSON whitespace, regardless of position, alignment, or backend.
TEST(json_whitespace_scan, all_byte_values_and_stop_positions) {
  constexpr std::size_t suffix_bytes = 41;
  for (const std::size_t alignment : {0u, 1u, 7u, 15u, 31u}) {
    for (std::size_t prefix = 0; prefix <= 72; ++prefix) {
      const auto size = prefix + suffix_bytes;
      auto storage = std::make_unique<std::uint8_t[]>(alignment + size);
      auto* data = storage.get() + alignment;
      const auto whitespace = whitespace_run(size);
      std::memcpy(data, whitespace.data(), size);
      for (unsigned byte = 0; byte <= std::numeric_limits<std::uint8_t>::max(); ++byte) {
        data[prefix] = static_cast<std::uint8_t>(byte);
        const auto expected =
            byte == 0x09 || byte == 0x0a || byte == 0x0d || byte == 0x20 ? size : prefix;
        check_scan(data, size, expected);
      }
    }
  }
}

// Full runs and final-byte stops must not read beyond exact allocations, including scalar tails.
TEST(json_whitespace_scan, exact_bounds_and_empty_input) {
  check_scan(nullptr, 0, 0);
  for (std::size_t size = 0; size <= 160; ++size) {
    const auto whitespace = whitespace_run(size);
    for (std::size_t alignment = 0; alignment < 32; ++alignment) {
      auto storage = std::make_unique<std::uint8_t[]>(alignment + size);
      auto* data = storage.get() + alignment;
      if (size != 0) {
        std::memcpy(data, whitespace.data(), size);
      }
      check_scan(data, size, size);
      if (size != 0) {
        data[size - 1] = '}';
        check_scan(data, size, size - 1);
      }
    }
  }
}

// Generated object, enum, and array readers retain punctuation and string contents across long gaps.
TEST(json_whitespace_scan, generated_json_and_formatted_output) {
  constexpr std::string_view compact =
      R"({"name":"keep  spaces\tand\nescapes","numbers":[1,-2,3],"phases":["idle","done"],"status":"active"})";
  for (const std::size_t gap :
       {0u, 1u, 7u, 8u, 15u, 16u, 23u, 24u, 31u, 32u, 39u, 40u, 65u, 127u}) {
    const auto wire = add_whitespace(compact, gap);
    auto storage = std::make_unique<std::uint8_t[]>(wire.size() + 1);
    std::memcpy(storage.get() + 1, wire.data(), wire.size());
    const auto input = rohit::make_constant_full_stream(storage.get() + 1, wire.size());
    json_input decoder{input};
    assessment::item value{};
    value.serialize_in(decoder);
    decoder.finish();
    EXPECT_EQ(value.name, "keep  spaces\tand\nescapes");
    EXPECT_EQ(value.numbers, (std::vector<std::int32_t>{1, -2, 3}));
    EXPECT_EQ(value.phases,
              (std::vector<assessment::phase>{assessment::phase::idle, assessment::phase::done}));
    EXPECT_EQ(value.status, assessment::phase::active);
    EXPECT_EQ(input.current_offset(), wire.size());
    for (auto format :
         {codec::format::compress, codec::format::beautify, codec::format::beautify_vertical}) {
      const std::string indentation(64, ' ');
      format.indent_text = indentation;
      rohit::full_stream_auto_alloc output{};
      codec::json_out<true> encoder{output, format};
      value.serialize_out(encoder);
      const auto formatted_input =
          rohit::make_constant_full_stream(output.begin(), output.current_offset());
      json_input formatted_decoder{formatted_input};
      assessment::item copy{};
      copy.serialize_in(formatted_decoder);
      formatted_decoder.finish();
      EXPECT_EQ(copy.name, value.name);
      EXPECT_EQ(copy.numbers, value.numbers);
      EXPECT_EQ(copy.phases, value.phases);
      EXPECT_EQ(copy.status, value.status);
    }
  }
}

// Preserve limit precedence and the exact consumed prefix, including a token just beyond a budget.
TEST(json_whitespace_scan, bounded_finish_and_work_accounting) {
  check_finish_limits<json_input>();
  check_finish_limits<protojson_input>();
  constexpr std::string_view token = "true";
  const auto tail = whitespace_run(97);
  const auto wire = std::string{token} + tail;
  for (std::size_t suffix_budget = 1; suffix_budget <= tail.size(); ++suffix_budget) {
    for (const bool work_budget : {false, true}) {
      codec::decode_limits limits{};
      if (work_budget) {
        limits.max_work_units = token.size() + suffix_budget + 1; // One decoded value plus bytes.
      } else {
        limits.max_input_bytes = token.size() + suffix_budget;
      }
      const auto input = rohit::make_constant_full_stream(wire.data(), wire.size());
      json_input decoder{input, limits};
      bool value{};
      decoder.serialize_in(value);
      EXPECT_TRUE(value);
      if (suffix_budget < tail.size()) {
        EXPECT_THROW(decoder.finish(), codec::exception::resource_limit);
      } else {
        EXPECT_NO_THROW(decoder.finish());
      }
      EXPECT_EQ(input.current_offset(), token.size() + suffix_budget);
    }
  }
}

// JSON does not inherit locale/C whitespace rules, and whitespace alone is not a decoded value.
TEST(json_whitespace_scan, rejects_other_whitespace_and_missing_values) {
  const auto prefix = whitespace_run(65);
  for (const unsigned byte : {0x00u, 0x0bu, 0x0cu, 0x1fu, 0x85u, 0xa0u, 0xffu}) {
    const auto wire = prefix + std::string(1, static_cast<char>(byte)) + "true";
    const auto input = rohit::make_constant_full_stream(wire.data(), wire.size());
    json_input decoder{input};
    bool value{};
    EXPECT_THROW(decoder.serialize_in(value), codec::exception::bad_input_data);
    EXPECT_FALSE(value);
    EXPECT_EQ(input.current_offset(), prefix.size());
  }
  for (const auto suffix : {"\xc2\xa0", "\xe2\x80\xa8", "\xe2\x80\xa9", "# comment"}) {
    const auto wire = prefix + suffix;
    const auto input = rohit::make_constant_full_stream(wire.data(), wire.size());
    json_input decoder{input};
    EXPECT_THROW(decoder.finish(), codec::exception::bad_input_data);
    EXPECT_EQ(input.current_offset(), prefix.size());
  }
  const auto input = rohit::make_constant_full_stream(prefix.data(), prefix.size());
  json_input decoder{input};
  bool value{};
  EXPECT_THROW(decoder.serialize_in(value), codec::exception::bad_input_data);
  EXPECT_FALSE(value);
  EXPECT_EQ(input.current_offset(), prefix.size());
}
