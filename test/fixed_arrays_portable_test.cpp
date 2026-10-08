#include "fixed_arrays_portable.hpp"
#include <rohit/serializer.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
namespace schema = rohit::serializer;

// Retain native C++ wire bytes after verifying an exact generated-codec round trip.
template <template <schema::serialize_type> class Protocol>
void write_fixture(const fixture& value, const std::filesystem::path& path) {
  rohit::full_stream_auto_alloc output{};
  value.template serialize_out<Protocol>(output);
  const auto input = rohit::make_constant_stream(output.begin(), output.current_offset());
  const auto decoded = schema::deserialize_exact<fixture, Protocol>(input);
  if (decoded.short_hash.bytes != value.short_hash.bytes ||
      decoded.long_hash.bytes != value.long_hash.bytes || decoded.commit_type != value.commit_type) {
    throw std::runtime_error("Fixed-array native round trip failed");
  }
  std::ofstream destination(path, std::ios::binary);
  destination.write(reinterpret_cast<const char*>(output.begin()),
                    static_cast<std::streamsize>(output.current_offset()));
  if (!destination) {
    throw std::runtime_error("Cannot write native fixture");
  }
}
} // namespace

// Produce all union alternatives through real generated C++ codecs for cross-language checks.
int main(int argument_count, char** arguments) {
  if (argument_count != 2) {
    return 1;
  }
  const std::filesystem::path directory(arguments[1]);
  std::filesystem::create_directories(directory);
  fixture value{};
  for (std::size_t index = 0; index < value.short_hash.bytes.size(); ++index) {
    value.short_hash.bytes[index] = static_cast<std::uint8_t>(index + 1);
  }
  for (std::size_t index = 0; index < value.long_hash.bytes.size(); ++index) {
    value.long_hash.bytes[index] = static_cast<std::uint8_t>(index + 21);
  }
  value.nested[0].value = 41;
  value.nested[1].value = 42;
  for (unsigned alternative = 0; alternative < 3; ++alternative) {
    value.commit_type = static_cast<fixture::e_commit>(alternative);
    // Class alternatives require an explicit lifetime start in the generated union storage.
    if (value.commit_type == fixture::e_commit::sha1) {
      std::construct_at(&value.commit.sha1, value.short_hash);
    } else if (value.commit_type == fixture::e_commit::sha256) {
      std::construct_at(&value.commit.sha256, value.long_hash);
    } else {
      value.commit.absent = 0;
    }
    const auto prefix = std::to_string(alternative) + "-";
    write_fixture<schema::json>(value, directory / (prefix + "0.bin"));
    write_fixture<schema::binary_none>(value, directory / (prefix + "1.bin"));
    write_fixture<schema::binary_integer>(value, directory / (prefix + "2.bin"));
    write_fixture<schema::binary_string>(value, directory / (prefix + "3.bin"));
  }
}
