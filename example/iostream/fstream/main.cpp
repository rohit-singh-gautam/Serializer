#include "example_support.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>

// Switch a single bidirectional binary file stream from writing to reading.
int main() {
  using namespace iostream_example;
  return run_example("std::fstream / binary", [] {
    const auto original = make_archive();
    const temporary_file file;
    std::fstream stream;
    stream.exceptions(std::ios::badbit | std::ios::failbit);
    stream.open(file.name(), std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
    archive::serialize<codec::binary_integer>(stream, original);
    stream.flush();
    const auto encoded_bytes = static_cast<std::size_t>(std::filesystem::file_size(file.name()));
    stream.seekg(0); // Explicitly reposition when switching from output to input.

    const auto decoded = archive::deserialize<codec::binary_integer>(stream, message_limits());
    verify_round_trip(original, decoded, encoded_bytes);
    stream.clear(); // A successful EOF-delimited read leaves EOF/fail state set.
    stream.close();
    return encoded_bytes;
  });
}
