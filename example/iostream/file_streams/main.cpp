#include "example_support.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>

// Write and reopen a binary file while retaining the concrete file types for 64 KiB batching.
int main() {
  using namespace iostream_example;
  return run_example("std::ofstream + std::ifstream / binary", [] {
    const auto original = make_archive();
    const temporary_file file;
    {
      std::ofstream output;
      output.exceptions(std::ios::badbit | std::ios::failbit);
      output.open(file.name(), std::ios::binary | std::ios::trunc);
      original.serialize_out<codec::binary_integer>(output);
      output.close(); // Closing is the caller's responsibility and reports pending write failures.
    }
    const auto encoded_bytes = static_cast<std::size_t>(std::filesystem::file_size(file.name()));
    std::ifstream input;
    input.exceptions(std::ios::badbit | std::ios::failbit);
    input.open(file.name(), std::ios::binary);
    archive decoded{};
    decoded.serialize_in<codec::binary_integer>(input, message_limits());
    verify_round_trip(original, decoded, encoded_bytes);
    return encoded_bytes;
  });
}
