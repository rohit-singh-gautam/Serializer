#pragma once

#include <document.hpp>
#include <rohit/compression.hpp>
#include <rohit/serializer.hpp>

#include <cstddef>
#include <exception>
#include <iostream>
#include <string_view>
#include <utility>

namespace compression_example {
namespace codec = rohit::serializer;
namespace compression = codec::compression;
using document = compression_model::document;
inline constexpr std::size_t maximum_message_bytes = compression::mebibyte;
inline constexpr std::size_t maximum_compressed_bytes = 2 * maximum_message_bytes;
inline constexpr std::size_t maximum_window_bytes = compression::mebibyte;

struct example_result {
  std::size_t serialized_bytes;
  std::size_t transmitted_bytes;
};

// Construct a deterministic document exceeding 64 KiB without external files or user data.
document make_document();
// Bound parsed fields and cumulative decoder work independently of compression storage.
codec::decode_limits message_limits();
// Leave room for compression overhead and expanding input within a separate output budget.
compression::encode_limits output_limits();
// Select an exact standard format with separate compressed, expanded, and history limits.
compression::decode_options input_options(compression::format format);
// Compare every field and return byte counts for this fixture, without asserting a size improvement.
example_result verify_round_trip(const document& expected, const document& actual,
                                 std::size_t transmitted_bytes);

// Report verified fixture byte counts; translate exceptions into a failing executable/CTest status.
template <typename Operation>
int run_example(std::string_view name, Operation&& operation) {
  try {
    const auto result = std::forward<Operation>(operation)();
    std::cout << name << ": verified " << result.serialized_bytes << " serialized bytes -> "
              << result.transmitted_bytes << " transmitted bytes\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << name << ": " << error.what() << '\n';
    return 1;
  }
}
} // namespace compression_example
