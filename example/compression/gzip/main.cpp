#include "example_support.hpp"

#include <ios>
#include <sstream>

// Pass a standard memory stream directly to member APIs for one gzip member.
int main() {
  using namespace compression_example;
  return run_example("gzip", [] {
    const auto original = make_document();
    std::stringstream stream{std::ios::in | std::ios::out | std::ios::binary};
    original.serialize_out<codec::binary_integer>(stream, compression::gzip_options{.level = 6},
                                                  output_limits());
    const auto transmitted_bytes = stream.view().size();
    stream.seekg(0);

    document decoded{};
    decoded.serialize_in<codec::binary_integer>(stream, message_limits(),
                                                input_options(compression::format::gzip));
    return verify_round_trip(original, decoded, transmitted_bytes);
  });
}
