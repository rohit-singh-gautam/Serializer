#include <fstream>
#include <iostream>
#include <iterator>
#include <message.hpp>
#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>
#include <stdexcept>
#include <string>
#include <variant>

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
// Reject an unsupported revision through the generated version checks for each encoder.
template <template <rohit::serializer::serialize_type> typename Protocol>
void verify_unsupported_version(const example_model& value) {
  auto unsupported = value;
  unsupported.version = example_model::serializer_current_version + 1U;
  try {
    (void)encode<Protocol>(unsupported);
  } catch (const std::invalid_argument&) {
    return;
  }
  throw std::runtime_error{"Unsupported version was encoded"};
}
// Construct and replace owning alternatives without a parallel discriminator.
void verify_ownership() {
  sample original{};
  original.raw_type = sample::e_raw::code;
  original.raw.code = -42;
  auto& text = original.emplace_payload<sample::e_payload::text>();
  text.message = "owned by the generated variant";
  text.tags = {"first", "second"};

  auto copy = original;
  auto& collection = copy.emplace_payload<sample::e_payload::collection>();
  collection.numbers = {1, 2, 3};
  collection.labels = {{"region", "south"}};
  if (original.get_payload_type() != sample::e_payload::text ||
      std::get<text_payload>(original.payload).message != text.message ||
      copy.get_payload_type() != sample::e_payload::collection) {
    throw std::runtime_error{"Owning variant copy or replacement failed"};
  }
  example_model value{};
  value.samples = {original, copy};
  verify<rohit::serializer::json>(value);
  verify<rohit::serializer::binary_none>(value);
  verify<rohit::serializer::binary_integer>(value);
  verify<rohit::serializer::binary_string>(value);
}

// Read both union/variant alternatives and exercise all four native protocols.
int main(int argc, char** argv) {
  try {
    verify_ownership();
    if (argc != 3) {
      throw std::invalid_argument{"Usage: main <input.json> <output.json>"};
    }
    std::ifstream input{argv[1], std::ios::binary};
    if (!input) {
      throw std::runtime_error{"Cannot open input"};
    }
    const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
    auto value = decode<rohit::serializer::json>(bytes);
    verify<rohit::serializer::json>(value);
    verify<rohit::serializer::binary_none>(value);
    verify<rohit::serializer::binary_integer>(value);
    verify<rohit::serializer::binary_string>(value);
    verify_unsupported_version<rohit::serializer::json>(value);
    verify_unsupported_version<rohit::serializer::binary_none>(value);
    verify_unsupported_version<rohit::serializer::binary_integer>(value);
    verify_unsupported_version<rohit::serializer::binary_string>(value);
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
