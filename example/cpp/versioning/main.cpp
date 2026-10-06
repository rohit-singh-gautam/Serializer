#include <fstream>
#include <iostream>
#include <iterator>
#include <message.hpp>
#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>
#include <stdexcept>
#include <string>

// Compatibility accepts the declared inclusive revision interval.
template <rohit::serializer::serialize_type Direction>
using compatible_json =
    rohit::serializer::json<Direction, rohit::stream, rohit::serializer::read_policy::compatible>;
template <rohit::serializer::serialize_type Direction>
using compatible_binary =
    rohit::serializer::binary_none<Direction, rohit::stream,
                                   rohit::serializer::binary_text_validation::strict,
                                   rohit::serializer::read_policy::compatible>;

// Encode directly into owned contiguous storage.
template <template <rohit::serializer::serialize_type> typename Protocol>
std::string encode(const auto& value) {
  rohit::full_stream_auto_alloc output{};
  value.template serialize_out<Protocol>(output);
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}
// Require exact input consumption and return a fresh owning model.
template <template <rohit::serializer::serialize_type> typename Protocol,
          typename Model = example_model>
Model decode(const std::string& bytes) {
  const auto input = rohit::make_constant_full_stream(bytes.data(), bytes.size());
  Protocol<rohit::serializer::serialize_type::in> reader{input};
  Model value{};
  reader.serialize_in(value);
  reader.finish();
  return value;
}
// Compare every field after one round trip using canonical positional bytes.
template <template <rohit::serializer::serialize_type> typename Protocol, typename Model>
void verify(const Model& value) {
  const auto copy = decode<Protocol, Model>(encode<Protocol>(value));
  if (encode<rohit::serializer::binary_none>(copy) !=
      encode<rohit::serializer::binary_none>(value)) {
    throw std::runtime_error{"Value mismatch"};
  }
}
// Read JSON, edit a typed field, and exercise all four native protocols.
int main(int argc, char** argv) {
  try {
    if (argc != 3) {
      throw std::invalid_argument{"Usage: main <input.json> <output.json>"};
    }
    {
      const uint16_version typed{};
      if (encode<rohit::serializer::binary_none>(typed) !=
          std::string{static_cast<char>(44), static_cast<char>(1)}) {
        throw std::runtime_error{"Revision width"};
      }
      verify<rohit::serializer::json>(typed);
      verify<rohit::serializer::binary_none>(typed);
      verify<rohit::serializer::binary_integer>(typed);
      verify<rohit::serializer::binary_string>(typed);
    }
    {
      const uint32_version typed{};
      if (encode<rohit::serializer::binary_none>(typed) !=
          std::string{static_cast<char>(112), static_cast<char>(17), static_cast<char>(1),
                      static_cast<char>(0)}) {
        throw std::runtime_error{"Revision width"};
      }
      verify<rohit::serializer::json>(typed);
      verify<rohit::serializer::binary_none>(typed);
      verify<rohit::serializer::binary_integer>(typed);
      verify<rohit::serializer::binary_string>(typed);
    }
    {
      const uint64_version typed{};
      if (encode<rohit::serializer::binary_none>(typed) !=
          std::string{static_cast<char>(255), static_cast<char>(255), static_cast<char>(255),
                      static_cast<char>(255), static_cast<char>(255), static_cast<char>(255),
                      static_cast<char>(255), static_cast<char>(255)}) {
        throw std::runtime_error{"Revision width"};
      }
      verify<rohit::serializer::json>(typed);
      verify<rohit::serializer::binary_none>(typed);
      verify<rohit::serializer::binary_integer>(typed);
      verify<rohit::serializer::binary_string>(typed);
    }
    {
      const float_version typed{};
      if (encode<rohit::serializer::binary_none>(typed) !=
          std::string{static_cast<char>(205), static_cast<char>(204), static_cast<char>(204),
                      static_cast<char>(61)}) {
        throw std::runtime_error{"Revision width"};
      }
      verify<rohit::serializer::json>(typed);
      verify<rohit::serializer::binary_none>(typed);
      verify<rohit::serializer::binary_integer>(typed);
      verify<rohit::serializer::binary_string>(typed);
    }
    {
      const double_version typed{};
      if (encode<rohit::serializer::binary_none>(typed) !=
          std::string{static_cast<char>(0), static_cast<char>(0), static_cast<char>(0),
                      static_cast<char>(0), static_cast<char>(0), static_cast<char>(0),
                      static_cast<char>(4), static_cast<char>(64)}) {
        throw std::runtime_error{"Revision width"};
      }
      verify<rohit::serializer::json>(typed);
      verify<rohit::serializer::binary_none>(typed);
      verify<rohit::serializer::binary_integer>(typed);
      verify<rohit::serializer::binary_string>(typed);
    }
    {
      const dotted2_version typed{};
      if (encode<rohit::serializer::binary_none>(typed) !=
          std::string{static_cast<char>(1), static_cast<char>(0), static_cast<char>(10),
                      static_cast<char>(0)}) {
        throw std::runtime_error{"Revision width"};
      }
      verify<rohit::serializer::json>(typed);
      verify<rohit::serializer::binary_none>(typed);
      verify<rohit::serializer::binary_integer>(typed);
      verify<rohit::serializer::binary_string>(typed);
    }
    {
      const dotted3_version typed{};
      if (encode<rohit::serializer::binary_none>(typed) !=
          std::string{static_cast<char>(1), static_cast<char>(0), static_cast<char>(10),
                      static_cast<char>(0), static_cast<char>(0), static_cast<char>(0)}) {
        throw std::runtime_error{"Revision width"};
      }
      verify<rohit::serializer::json>(typed);
      verify<rohit::serializer::binary_none>(typed);
      verify<rohit::serializer::binary_integer>(typed);
      verify<rohit::serializer::binary_string>(typed);
    }
    {
      const dotted4_version typed{};
      if (encode<rohit::serializer::binary_none>(typed) !=
          std::string{static_cast<char>(1), static_cast<char>(0), static_cast<char>(10),
                      static_cast<char>(0), static_cast<char>(0), static_cast<char>(0),
                      static_cast<char>(4), static_cast<char>(0)}) {
        throw std::runtime_error{"Revision width"};
      }
      verify<rohit::serializer::json>(typed);
      verify<rohit::serializer::binary_none>(typed);
      verify<rohit::serializer::binary_integer>(typed);
      verify<rohit::serializer::binary_string>(typed);
    }
    std::ifstream input{argv[1], std::ios::binary};
    if (!input) {
      throw std::runtime_error{"Cannot open input"};
    }
    const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
    auto value = decode<compatible_json>(bytes);
    // Read historical positional data before explicitly migrating the replacement.
    const auto old_bytes = encode<rohit::serializer::binary_none>(value);
    const auto old = decode<compatible_binary>(old_bytes);
    if (old.old_name != "Ada" || old.version != 8) {
      throw std::runtime_error{"Historical mismatch"};
    }
    value.name = value.old_name;
    value.version = 10;
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
    std::cout << "Version migration and four protocols passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
