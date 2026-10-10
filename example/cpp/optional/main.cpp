#include <fstream>
#include <iostream>
#include <iterator>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <vector>
#include <message.hpp>
#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>
#include <stdexcept>
#include <string>

// Encode directly into owned contiguous storage.
template <template <rohit::serializer::serialize_type> typename Protocol>
std::string encode(const example_model& value) {
  rohit::full_stream_auto_alloc output{};
  value.serialize_out<Protocol>(output);
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}
// Require exact input consumption and return a fresh owning model.
template <template <rohit::serializer::serialize_type> typename Protocol>
example_model decode(const std::string& bytes) {
  const auto input = rohit::make_constant_full_stream(bytes.data(), bytes.size());
  Protocol<rohit::serializer::serialize_type::in> reader{input};
  example_model value{};
  reader.serialize_in(value);
  reader.finish();
  return value;
}
// Compare every field after one round trip using canonical positional bytes.
template <template <rohit::serializer::serialize_type> typename Protocol>
void verify(const example_model& value) {
  const auto copy = decode<Protocol>(encode<Protocol>(value));
  if (encode<rohit::serializer::binary_none>(copy) !=
      encode<rohit::serializer::binary_none>(value)) {
    throw std::runtime_error{"Value mismatch"};
  }
}
// Read JSON, use standard optional APIs, and exercise all four native protocols.
int main(int argc, char** argv) {
  try {
    if (argc != 3) {
      throw std::invalid_argument{"Usage: main <input.json> <output.json>"};
    }
    std::ifstream input{argv[1], std::ios::binary};
    if (!input) {
      throw std::runtime_error{"Cannot open input"};
    }
    const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
    static_assert(std::is_same_v<decltype(example_model{}.count),
                                 std::optional<std::uint32_t>>);
    static_assert(std::is_same_v<decltype(example_model{}.numbers),
                                 std::optional<std::vector<std::uint32_t>>>);
    auto value = decode<rohit::serializer::json>(bytes);
    if (value.absent.has_value() || !value.count || value.count.value() != 7U ||
        !value.nested || value.nested->has_value() || !value.empty_numbers ||
        !value.empty_numbers->empty()) {
      throw std::runtime_error{"Optional presence mismatch"};
    }
    // Standard value construction preserves presence independently of an empty payload.
    value.count.reset();
    value.count.emplace(7U);
    value.nested.emplace(std::nullopt);
    value.empty_numbers.emplace();
    verify<rohit::serializer::json>(value);
    verify<rohit::serializer::binary_none>(value);
    verify<rohit::serializer::binary_integer>(value);
    verify<rohit::serializer::binary_string>(value);
    std::ofstream output{argv[2], std::ios::binary};
    output << encode<rohit::serializer::json>(value);
    output.close();
    if (!output) {
      throw std::runtime_error{"Cannot write output"};
    }
    std::cout << "Four protocols passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
