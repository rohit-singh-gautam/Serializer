#include <message.hpp>
#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
constexpr bool unsigned_char = SERIALIZER_TEST_CHAR_UNSIGNED != 0;
static_assert(std::numeric_limits<char>::is_signed != unsigned_char);
constexpr std::string_view producer = unsigned_char ? "cpp_unsigned" : "cpp_signed";
constexpr std::array<std::uint8_t, 4> keys{255, 127, 0, 128};
constexpr std::uint32_t value_base = 1000;

// Construct values independently of other runtimes and their map iteration order.
char_map fixture(bool ascii_only) {
  char_map value{};
  for (const auto key : keys) {
    if (!ascii_only || key <= 127) {
      value.entries.emplace(static_cast<char>(key), value_base + key);
    }
  }
  return value;
}

// Require preservation of unsigned byte identity and values, regardless of native ordering.
void verify_value(const char_map& value, bool ascii_only) {
  const auto expected = fixture(ascii_only);
  if (value.entries != expected.entries) {
    throw std::runtime_error{"Character-map keys or values changed"};
  }
}

// Emit one native ordering or semantically decode every producer's complete message.
template <template <rohit::serializer::serialize_type> class Protocol>
void exchange(const std::filesystem::path& directory, std::string_view protocol,
              bool emit, bool ascii_only = false) {
  if (emit) {
    rohit::full_stream_auto_alloc output{};
    fixture(ascii_only).serialize_out<Protocol>(output);
    output.write_to_file_till_offset(
        directory / (std::string{producer} + "_" + std::string{protocol} + ".bin"));
    return;
  }
  std::ifstream manifest{directory / "producers.txt"};
  if (!manifest) { throw std::runtime_error{"Cannot read producer manifest"}; }
  for (std::string name; manifest >> name;) {
    const auto input = rohit::make_stream_from_file(
        directory / (name + "_" + std::string{protocol} + ".bin"));
    const auto value = rohit::serializer::deserialize_exact<char_map, Protocol>(input);
    verify_value(value, ascii_only);
  }
}

// High-bit byte keys remain invalid in JSON's single-ASCII-character representation.
void verify_json_rejection() {
  bool rejected{};
  try {
    rohit::full_stream_auto_alloc output{};
    fixture(false).serialize_out<rohit::serializer::json>(output);
  } catch (const std::exception&) {
    rejected = true;
  }
  if (!rejected) { throw std::runtime_error{"JSON accepted a high-bit character key"}; }
}
} // namespace

// Run a signed- or unsigned-char C++ participant in the maintained semantic matrix.
int main(int argc, char** argv) {
  try {
    if (argc != 3) { throw std::invalid_argument{"Expected fixture directory and emit/verify"}; }
    const std::filesystem::path directory{argv[1]};
    const std::string_view mode{argv[2]};
    if (mode != "emit" && mode != "verify") { throw std::invalid_argument{"Unknown mode"}; }
    const bool emit = mode == "emit";
    verify_json_rejection();
    exchange<rohit::serializer::json>(directory, "json", emit, true);
    exchange<rohit::serializer::binary_none>(directory, "positional", emit);
    exchange<rohit::serializer::binary_integer>(directory, "integer", emit);
    exchange<rohit::serializer::binary_string>(directory, "string", emit);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
