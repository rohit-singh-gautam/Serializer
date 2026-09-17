#include "example_support.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <vector>

// Supply filebuf storage explicitly and adapt it through a generic std::iostream.
int main() {
  using namespace iostream_example;
  return run_example("std::filebuf + std::iostream / binary", [] {
    constexpr std::size_t file_buffer_bytes = 256 * 1024;
    const auto original = make_archive();
    const temporary_file file;
    // The backing storage must outlive the filebuf. pubsetbuf behavior is library-dependent.
    std::vector<char> storage(file_buffer_bytes);
    std::filebuf file_buffer;
    file_buffer.pubsetbuf(storage.data(), static_cast<std::streamsize>(storage.size()));
    if (file_buffer.open(file.name(), std::ios::in | std::ios::out | std::ios::binary |
                                          std::ios::trunc) == nullptr) {
      throw std::ios_base::failure{"Unable to open buffered example file"};
    }
    std::iostream stream{&file_buffer};
    stream.exceptions(std::ios::badbit | std::ios::failbit);
    // The static std::iostream type selects the generic 8 KiB serializer adapter.
    archive::serialize<codec::binary_integer>(stream, original);
    stream.flush();
    const auto encoded_bytes = static_cast<std::size_t>(std::filesystem::file_size(file.name()));
    stream.seekg(0);

    const auto decoded = archive::deserialize<codec::binary_integer>(stream, message_limits());
    verify_round_trip(original, decoded, encoded_bytes);
    if (file_buffer.close() == nullptr) {
      throw std::ios_base::failure{"Unable to close buffered example file"};
    }
    return encoded_bytes;
  });
}
