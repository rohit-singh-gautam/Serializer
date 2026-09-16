#include <account.hpp>
#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
// Construct the shared cross-language fixture, including unsigned and UTF-8 boundaries.
demo::account fixture() {
  demo::account value{};
  value.record_id = 42;
  value.account_id = std::numeric_limits<std::uint64_t>::max();
  value.display_name = "Ada \"Lovelace\" \xf0\x9f\x9a\x80";
  value.scores = {-1, 0, 100};
  value.counters.emplace("visits", 7);
  value.payload_type = demo::account::e_payload::ratio;
  value.payload.ratio = 1.5f;
  value.other.status = demo::account_state::active;
  value.history = {demo::account_state::waiting_for_review, demo::account_state::active};
  return value;
}

// Encode C++ fixtures, or decode Java output and check the canonical C++ re-encoding.
template <template <rohit::serializer::serialize_type> typename Protocol>
void exchange(const std::filesystem::path& directory, std::string_view name, bool verify) {
  auto value = fixture();
  rohit::full_stream_auto_alloc expected{};
  value.serialize_out<Protocol>(expected);
  if (!verify) {
    expected.write_to_file_till_offset(directory / ("cpp_" + std::string{name} + ".bin"));
    return;
  }
  const auto input =
      rohit::make_stream_from_file(directory / ("java_" + std::string{name} + ".bin"));
  Protocol<rohit::serializer::serialize_type::in> decoder{input};
  demo::account decoded{};
  decoder.serialize_in(decoded);
  decoder.finish();
  rohit::full_stream_auto_alloc actual{};
  decoded.serialize_out<Protocol>(actual);
  const std::string_view expected_text{reinterpret_cast<const char*>(expected.begin()),
                                       expected.current_offset()};
  const std::string_view actual_text{reinterpret_cast<const char*>(actual.begin()),
                                     actual.current_offset()};
  if (expected_text != actual_text) {
    throw std::runtime_error{"Cross-language mismatch: " + std::string{name}};
  }
}
} // namespace

// Run one side of the CTest-managed two-way interoperability check.
int main(int argc, char** argv) {
  try {
    if (argc != 3) {
      return 2;
    }
    const std::filesystem::path directory{argv[1]};
    std::filesystem::create_directories(directory);
    const bool verify = std::string_view{argv[2]} == "verify";
    exchange<rohit::serializer::json>(directory, "JSON", verify);
    exchange<rohit::serializer::binary_none>(directory, "BINARY_NONE", verify);
    exchange<rohit::serializer::binary_integer>(directory, "BINARY_INTEGER", verify);
    exchange<rohit::serializer::binary_string>(directory, "BINARY_STRING", verify);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
