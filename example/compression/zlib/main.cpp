#include "example_support.hpp"

#include <ios>
#include <sstream>
#include <utility>

// Transfer a zlib-wrapped DEFLATE stream through independent standard output/input streams.
int main() {
  using namespace compression_example;
  return run_example("zlib", [] {
    const auto original = make_document();
    std::ostringstream output{std::ios::out | std::ios::binary};
    document::serialize<codec::binary_integer>(
        output, original, compression::zlib_options{.level = 6}, output_limits());
    std::istringstream input{std::move(output).str(), std::ios::in | std::ios::binary};
    const auto transmitted_bytes = input.view().size();

    const auto decoded = document::deserialize<codec::binary_integer>(
        input, message_limits(), input_options(compression::format::zlib));
    return verify_round_trip(original, decoded, transmitted_bytes);
  });
}
