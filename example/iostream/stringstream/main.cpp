#include "example_support.hpp"

#include <ios>
#include <sstream>

// Round-trip JSON using one concrete stringstream and its borrowed unread input view.
int main() {
  using namespace iostream_example;
  return run_example("std::stringstream / JSON", [] {
    const auto original = make_archive();
    std::stringstream stream{std::ios::in | std::ios::out | std::ios::binary};
    stream.exceptions(std::ios::badbit | std::ios::failbit);
    archive::serialize<codec::json>(stream, original);
    const auto encoded_bytes = stream.view().size();

    stream.seekg(0); // The input position is independent of the output position.
    const auto decoded = archive::deserialize<codec::json>(stream, message_limits());
    verify_round_trip(original, decoded, encoded_bytes);
    return encoded_bytes;
  });
}
