#include <message.hpp>
#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
constexpr std::array languages{"cpp", "java", "js", "go", "csharp"};
constexpr int variant_count = 3;

// Construct the shared fixture independently, preserving schema defaults and boundary values.
interop::message fixture(int variant) {
  interop::message value{};
  value.text = "Ada \"Lovelace\" \xf0\x9f\x9a\x80\n" + std::string(64, 'x');
  value.numbers = {std::numeric_limits<std::int32_t>::min(), -1, 0,
                   std::numeric_limits<std::int32_t>::max()};
  value.decimals = {-0.0f, 1.5f, -2.25f};
  value.flags = {false, true};
  value.labels = {"", "\xc3\xa9", "\xf0\x9f\x9a\x80"};
  interop::detail child{};
  child.code = -9;
  child.note = "child";
  value.children = {interop::detail{}, child};
  value.states = {interop::state::paused, interop::state::ready};
  value.counts = {
      {"\xf0\x9f\x9a\x80", std::numeric_limits<std::uint64_t>::max()}, {"\xc3\xa9", 7}, {"a", 0}};
  value.indexed = {{std::numeric_limits<std::uint64_t>::max(), child}, {0, interop::detail{}}};
  value.toggles = {{true, "yes"}, {false, "no"}};
  value.enums = {{interop::state::paused, -2}, {interop::state::ready, 1}};
  value.payload_type = static_cast<interop::message::e_payload>(variant);
  if (variant == 0) {
    value.payload.number = -1234567890123456789LL;
  } else if (variant == 1) {
    value.payload.ratio = -3.5f;
  } else {
    value.payload.state = interop::state::paused;
  }
  return value;
}

// Encode a complete message for byte comparison without overlaying native object layout.
template <template <rohit::serializer::serialize_type> typename Protocol>
std::string encode(const interop::message& value) {
  rohit::full_stream_auto_alloc output{};
  value.serialize_out<Protocol>(output);
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}

// Emit C++ data or verify every producer by complete canonical positional re-encoding.
template <template <rohit::serializer::serialize_type> typename Protocol>
void exchange(const std::filesystem::path& directory, std::string_view protocol, bool emit) {
  for (int variant = 0; variant < variant_count; ++variant) {
    const auto expected = fixture(variant);
    const auto encoded = encode<Protocol>(expected);
    const auto canonical = encode<rohit::serializer::binary_none>(expected);
    const auto suffix = "_" + std::string{protocol} + "_" + std::to_string(variant) + ".bin";
    if (emit) {
      std::ofstream file{directory / ("cpp" + suffix), std::ios::binary};
      file.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
      if (!file) {
        throw std::runtime_error{"Cannot write fixture"};
      }
    } else {
      for (const auto* language : languages) {
        std::ifstream file{directory / (language + suffix), std::ios::binary};
        if (!file) {
          throw std::runtime_error{std::string{"Cannot read fixture: "} + language + suffix};
        }
        const std::string bytes{std::istreambuf_iterator<char>{file}, {}};
        const auto input = rohit::make_constant_full_stream(bytes.data(), bytes.size());
        Protocol<rohit::serializer::serialize_type::in> decoder{input};
        interop::message actual{};
        decoder.serialize_in(actual);
        decoder.finish();
        if (encode<rohit::serializer::binary_none>(actual) != canonical) {
          throw std::runtime_error{std::string{language} + " -> cpp value mismatch: " + suffix};
        }
        if (protocol != "JSON" && bytes != encoded) {
          throw std::runtime_error{std::string{"Canonical binary mismatch: "} + language + suffix};
        }
      }
    }
  }
}
} // namespace

// Run the C++ side of the all-language interoperability example.
int main(int argc, char** argv) {
  try {
    if (argc != 3 ||
        (std::string_view{argv[2]} != "emit" && std::string_view{argv[2]} != "verify")) {
      throw std::invalid_argument{"Usage: interop_cpp <directory> emit|verify"};
    }
    const std::filesystem::path directory{argv[1]};
    std::filesystem::create_directories(directory);
    const bool emit = std::string_view{argv[2]} == "emit";
    exchange<rohit::serializer::json>(directory, "JSON", emit);
    exchange<rohit::serializer::binary_none>(directory, "BINARY_NONE", emit);
    exchange<rohit::serializer::binary_integer>(directory, "BINARY_INTEGER", emit);
    exchange<rohit::serializer::binary_string>(directory, "BINARY_STRING", emit);
    std::cout << "cpp " << argv[2] << " passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
