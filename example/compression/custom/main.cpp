#include "example_support.hpp"

#include <rohit/stream.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace {
namespace compression = rohit::serializer::compression;

// Delegate to a built-in format to demonstrate the extension interface without inventing a format.
// This stateless adapter is safe for concurrent calls with independently owned input/output.
class gzip_backend final : public compression::backend {
public:
  // A real external adapter can feed library-produced chunks directly into this bounded sink.
  void encode(std::span<const std::uint8_t> input,
              compression::output_sink& output) const override {
    const auto bytes = compression::compress(input, compression::gzip_options{.level = 6},
                                             compression_example::output_limits());
    output.append(bytes);
  }

  // Validate the complete gzip member and retain the caller's window bound before emitting bytes.
  void decode(std::span<const std::uint8_t> input, compression::output_sink& output,
              std::size_t max_window_bytes) const override {
    auto options = compression_example::input_options(compression::format::gzip);
    options.max_window_bytes = max_window_bytes;
    const auto bytes = compression::decompress(input, options);
    output.append(bytes);
  }
};
} // namespace

// Borrow an application-owned backend for each call; it must outlive both operations.
int main() {
  using namespace compression_example;
  return run_example("custom gzip backend", [] {
    const gzip_backend backend{};
    const auto original = make_document();
    rohit::full_stream_auto_alloc output;
    codec::serialize_to<codec::binary_integer>(
        output, original, compression::custom_options{&backend}, output_limits());

    auto options = input_options(compression::format::custom);
    options.custom_backend = &backend;
    const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
    const auto decoded =
        codec::deserialize_exact<document, codec::binary_integer>(input, message_limits(), options);
    return verify_round_trip(original, decoded, output.current_offset());
  });
}
