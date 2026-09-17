#include "fuzz_support.hpp"

#include <rohit/runtime_simd.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <memory>

namespace {
namespace runtime = rohit::serializer::detail;

// Compare baseline and dispatched compiled scanners with a scalar implementation.
template <runtime::json_scan_kind Kind>
void check_scan(const qualification::fuzz_input& input) {
  const auto expected = runtime::scan_json_scalar<Kind>(input.bytes.data(), input.bytes.size());
  if (runtime::scan_json_baseline(input.bytes.data(), input.bytes.size(), Kind) != expected ||
      runtime::scan_json_long(input.bytes.data(), input.bytes.size(), Kind) != expected ||
      runtime::scan_json_prefix<Kind>(input.bytes.data(), input.bytes.size()) != expected) {
    std::abort();
  }
}

// Keep both ends exact even when rounding to a complete element; compare disjoint and in-place swaps.
void check_swap(qualification::fuzz_input& input, std::size_t element_bytes) {
  const auto suffix = input.bytes.size() % element_bytes;
  const auto bytes = input.bytes.subspan(suffix);
  auto expected = std::make_unique<std::uint8_t[]>(bytes.size());
  auto actual = std::make_unique<std::uint8_t[]>(bytes.size());
  for (std::size_t offset = 0; offset < bytes.size(); offset += element_bytes) {
    std::reverse_copy(bytes.data() + offset, bytes.data() + offset + element_bytes,
                      expected.get() + offset);
  }
  runtime::copy_swapped_baseline(actual.get(), bytes.data(), bytes.size(), element_bytes);
  if (!std::equal(expected.get(), expected.get() + bytes.size(), actual.get())) {
    std::abort();
  }
  runtime::copy_swapped(actual.get(), bytes.data(), bytes.size(), element_bytes);
  if (!std::equal(expected.get(), expected.get() + bytes.size(), actual.get())) {
    std::abort();
  }
  runtime::copy_swapped(bytes.data(), bytes.data(), bytes.size(), element_bytes);
  if (!std::equal(expected.get(), expected.get() + bytes.size(), bytes.begin())) {
    std::abort();
  }
}
} // namespace

// Exercise exact-size SIMD boundaries independently of whether random bytes form a valid message.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  using namespace qualification;
  if (size < fuzz_control_bytes || size - fuzz_control_bytes > fuzz_payload_bytes) {
    return 0;
  }
  fuzz_input input{data, size};
  check_scan<runtime::json_scan_kind::ascii>(input);
  check_scan<runtime::json_scan_kind::unescaped>(input);
  check_scan<runtime::json_scan_kind::whitespace>(input);
  constexpr std::size_t element_widths[]{2, 4, 8};
  check_swap(input, element_widths[input.selector % std::size(element_widths)]);
  return 0;
}
