#include "example_support.hpp"

#include <ios>
#include <sstream>
#include <utility>

// Transfer a binary message from an output-only memory stream to an input-only memory stream.
int main() {
  using namespace iostream_example;
  return run_example("std::ostringstream + std::istringstream / binary", [] {
    const auto original = make_archive();
    std::ostringstream output{std::ios::out | std::ios::binary};
    original.serialize_out<codec::binary_integer>(output);

    // Move the encoded string into the input stream; the adapter then borrows its unread view.
    std::istringstream input{std::move(output).str(), std::ios::in | std::ios::binary};
    const auto encoded_bytes = input.view().size();
    archive decoded{};
    decoded.serialize_in<codec::binary_integer>(input, message_limits());
    verify_round_trip(original, decoded, encoded_bytes);
    return encoded_bytes;
  });
}
