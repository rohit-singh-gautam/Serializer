#include "example_support.hpp"

#include <ios>
#include <istream>
#include <ostream>
#include <sstream>

// Use an interface that exposes only standard base streams; adaptation remains automatic.
int main() {
  using namespace iostream_example;
  return run_example("std::ostream + std::istream references / binary", [] {
    const auto original = make_archive();
    std::stringstream storage{std::ios::in | std::ios::out | std::ios::binary};
    std::ostream& output = storage;
    original.serialize_out<codec::binary_integer>(output);
    const auto encoded_bytes = storage.view().size();
    storage.seekg(0);

    // Erasing the static memory-stream type selects owned staging with 8 KiB read batches.
    std::istream& input = storage;
    archive decoded{};
    decoded.serialize_in<codec::binary_integer>(input, message_limits());
    verify_round_trip(original, decoded, encoded_bytes);
    return encoded_bytes;
  });
}
