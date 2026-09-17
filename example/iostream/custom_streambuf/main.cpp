#include "example_support.hpp"

#include "chunked_streambuf.hpp"

#include <ios>
#include <istream>
#include <ostream>

// Adapt a custom non-seekable transport through ordinary istream and ostream objects.
int main() {
  using namespace iostream_example;
  return run_example("custom std::streambuf / binary", [] {
    const auto original = make_archive();
    chunked_streambuf buffer;
    std::ostream output{&buffer};
    output.exceptions(std::ios::badbit | std::ios::failbit);
    archive::serialize<codec::binary_integer>(output, original);
    output.flush();

    // The first read begins at the start of the stored message; no seeking is supported.
    std::istream input{&buffer};
    input.exceptions(std::ios::badbit | std::ios::failbit);
    const auto decoded = archive::deserialize<codec::binary_integer>(input, message_limits());
    verify_round_trip(original, decoded, buffer.size());
    return buffer.size();
  });
}
