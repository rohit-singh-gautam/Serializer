#include <rohit/serializer.hpp>
#include <records.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr std::size_t fuzz_input_bytes = 4096;
constexpr std::size_t fuzz_string_bytes = 1024;
constexpr std::size_t fuzz_collection_elements = 64;
constexpr std::size_t fuzz_nesting_depth = 8;
constexpr std::size_t fuzz_allocation_bytes = 16384;
constexpr std::size_t fuzz_work_units = 8192;

// Exercise one exact input view with limits small enough for sustained malformed-input fuzzing.
template <typename Protocol, typename T>
void decode_input(const std::uint8_t* data, std::size_t size) {
  rohit::serializer::decode_limits limits{};
  limits.max_input_bytes = fuzz_input_bytes;
  limits.max_string_bytes = fuzz_string_bytes;
  limits.max_collection_elements = fuzz_collection_elements;
  limits.max_nesting_depth = fuzz_nesting_depth;
  limits.max_allocation_bytes = fuzz_allocation_bytes;
  limits.max_work_units = fuzz_work_units;
  const auto input = rohit::make_constant_full_stream(data, size);
  Protocol decoder{input, limits};
  T value{};
  try {
    decoder.serialize_in(value);
    decoder.finish();
  } catch (const rohit::exception::base_parser&) {
  } catch (const std::invalid_argument&) {
    // Generated enum-name conversion rejects unknown alternatives.
  }
}
} // namespace

// Feed arbitrary bytes through generated objects and primitive JSON entry points.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size > fuzz_input_bytes) { return 0; }
  using namespace rohit::serializer;
  decode_input<json<serialize_type::in>, qualification::record>(data, size);
  decode_input<json<serialize_type::in>, std::string>(data, size);
  decode_input<json<serialize_type::in>, double>(data, size);
  decode_input<json<serialize_type::in>, std::vector<std::vector<int>>>(data, size);
  decode_input<binary_none<serialize_type::in>, qualification::record>(data, size);
  decode_input<binary_integer<serialize_type::in>, qualification::record>(data, size);
  decode_input<binary_string<serialize_type::in>, qualification::record>(data, size);
  return 0;
}
