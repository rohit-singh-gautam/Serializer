#include "../src/schema_scan.hpp"

#include <rohit/decode.hpp>
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>

namespace {

namespace scanner = rohit::serializer::parser::detail;

// Check both direct backends and the short-token entry point against the expected boundary.
template <scanner::scan_kind Kind>
void expect_scan(const std::uint8_t* data, std::size_t size, std::size_t expected) {
  EXPECT_EQ(scanner::scan_prefix<Kind>(data, size), expected);
  EXPECT_EQ(scanner::scan_baseline(data, size, Kind), expected);
  EXPECT_EQ(scanner::scan_long(data, size, Kind), expected);
}

// Exercise every byte value at scalar/vector transitions with no readable padding after input.
template <scanner::scan_kind Kind>
void check_byte_classification(std::string_view accepted, bool inverted, std::uint8_t filler) {
  constexpr std::array prefix_sizes{0u,  7u,  8u,  15u, 16u, 23u, 24u, 31u, 32u,
                                    39u, 40u, 63u, 64u, 71u, 72u, 95u, 96u};
  constexpr std::array alignments{0u, 1u, 7u, 15u, 31u};
  constexpr std::size_t suffix_size = 64;
  for (const auto alignment : alignments) {
    for (const auto prefix : prefix_sizes) {
      const auto size = prefix + 1 + suffix_size;
      auto storage = std::make_unique<std::uint8_t[]>(alignment + size);
      auto* data = storage.get() + alignment;
      std::fill_n(data, size, filler);
      for (unsigned byte = 0; byte <= std::numeric_limits<std::uint8_t>::max(); ++byte) {
        data[prefix] = static_cast<std::uint8_t>(byte);
        const bool found = accepted.find(static_cast<char>(byte)) != std::string_view::npos;
        const auto expected = (found != inverted) ? size : prefix;
        SCOPED_TRACE(::testing::Message()
                     << "alignment=" << alignment << ", prefix=" << prefix << ", byte=" << byte);
        expect_scan<Kind>(data, size, expected);
      }
    }
  }
}

// Exact-size allocations allow a later sanitizer run to detect vector reads past the input.
template <scanner::scan_kind Kind>
void check_tails(std::uint8_t filler) {
  expect_scan<Kind>(nullptr, 0, 0);
  constexpr std::size_t maximum_size = 128;
  constexpr std::size_t maximum_alignment = 32;
  for (std::size_t alignment = 0; alignment < maximum_alignment; ++alignment) {
    for (std::size_t size = 1; size <= maximum_size; ++size) {
      auto storage = std::make_unique<std::uint8_t[]>(alignment + size);
      auto* data = storage.get() + alignment;
      std::fill_n(data, size, filler);
      expect_scan<Kind>(data, size, size);
    }
  }
}

} // namespace

TEST(schema_scan, ascii_classification) {
  check_byte_classification<scanner::scan_kind::whitespace>(" \t\r\n", false, ' ');
  check_byte_classification<scanner::scan_kind::identifier>(
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_", false, 'a');
  check_byte_classification<scanner::scan_kind::line_comment>("\n", true, 'x');
  check_byte_classification<scanner::scan_kind::block_comment>("*", true, 'x');
}

TEST(schema_scan, empty_and_unpadded_tails) {
  check_tails<scanner::scan_kind::whitespace>(' ');
  check_tails<scanner::scan_kind::identifier>('a');
  check_tails<scanner::scan_kind::line_comment>('x');
  check_tails<scanner::scan_kind::block_comment>('x');
}

TEST(schema_scan, identifiers_preserve_delimiters_and_qualified_names) {
  constexpr std::size_t maximum_size = 128;
  for (std::size_t size = 1; size <= maximum_size; ++size) {
    const std::string name = "_" + std::string(size, 'a') + "09_Z";
    std::string source = name + ";";
    rohit::full_stream input{source.data(), source.size()};
    EXPECT_EQ(rohit::serializer::parser::parse_identifier(input), name);
    ASSERT_EQ(input.remaining_buffer(), 1);
    EXPECT_EQ(*input, ';');

    const std::string qualified = name + "::" + name;
    std::string qualified_source = qualified + " ";
    rohit::full_stream qualified_input{qualified_source.data(), qualified_source.size()};
    EXPECT_EQ(rohit::serializer::parser::parse_hierarchical_identifier(qualified_input), qualified);
    ASSERT_EQ(qualified_input.remaining_buffer(), 1);
    EXPECT_EQ(*qualified_input, ' ');
  }
}

TEST(schema_scan, comments_and_whitespace_across_block_boundaries) {
  constexpr std::size_t maximum_size = 128;
  for (std::size_t size = 0; size <= maximum_size; ++size) {
    const std::string name = "record_" + std::string(size, 'a');
    std::string source = std::string(size, ' ') + "/*" + std::string(size, 'x') + "*/\r\n//" +
                         std::string(size, 'x') + "\r\nclass " + name + " {}//final comment";
    rohit::full_stream input{source.data(), source.size()};
    const auto statements = rohit::serializer::parser::parse(input);
    ASSERT_EQ(statements.size(), 1);
    EXPECT_EQ(statements.front()->name, name);
    EXPECT_TRUE(input.full());
  }
}

TEST(schema_scan, member_metadata_preserves_wire_names_and_ids) {
  constexpr std::size_t maximum_size = 128;
  for (std::size_t size = 1; size <= maximum_size; ++size) {
    const std::string wire_name = "wire_" + std::string(size, 'a');
    std::string source = "public uint32 account_id (\"" + wire_name + "\", 17);";
    rohit::full_stream input{source.data(), source.size()};
    const auto field = rohit::serializer::parser::parse_member(input, 1, nullptr);
    EXPECT_EQ(field.name, "account_id");
    EXPECT_EQ(field.display_name, wire_name);
    EXPECT_EQ(field.id, 17);
    EXPECT_TRUE(input.full());
  }
}

TEST(schema_scan, unterminated_comments_preserve_error_cursor) {
  constexpr std::size_t maximum_size = 128;
  constexpr std::array fillers{'x', '*'};
  for (std::size_t size = 0; size <= maximum_size; ++size) {
    for (const auto filler : fillers) {
      std::string source = "/*" + std::string(size, filler);
      rohit::full_stream input{source.data(), source.size()};
      EXPECT_THROW(rohit::serializer::parser::parse(input),
                   rohit::serializer::exception::bad_input_data);
      EXPECT_EQ(input.remaining_buffer(), size == 0 ? 0u : 1u);
    }
  }
}
