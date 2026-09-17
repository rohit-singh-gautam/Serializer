#pragma once

#include <telemetry_archive.hpp>

#include <rohit/serializer.hpp>

#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <utility>

namespace iostream_example {
namespace codec = rohit::serializer;
using archive = iostream_model::telemetry_archive;
inline constexpr std::size_t minimum_payload_bytes = 256 * 1024;

// Construct deterministic nested data large enough to cross the 8 KiB and 64 KiB batches.
archive make_archive();

// Bound one complete message, its decoded allocations, and its parsing work.
codec::decode_limits message_limits();

// Compare every serialized field through native buffers; throw on mismatch or a small fixture.
void verify_round_trip(const archive& expected, const archive& actual, std::size_t encoded_bytes);

// Own a newly created temporary directory and one file; streams must close before destruction.
class temporary_file {
  std::filesystem::path directory;
  std::filesystem::path path;

public:
  // Atomically claim an unused temporary directory without replacing an existing file.
  temporary_file();
  // Remove only this example's file and directory; cleanup does not throw.
  ~temporary_file();
  temporary_file(const temporary_file&) = delete;
  temporary_file& operator=(const temporary_file&) = delete;
  temporary_file(temporary_file&&) = delete;
  temporary_file& operator=(temporary_file&&) = delete;

  // Borrow the path until this owner is destroyed.
  const std::filesystem::path& name() const;
};

// Report one verified round trip and translate exceptions into a failing executable status.
template <typename Operation>
int run_example(std::string_view name, Operation&& operation) {
  try {
    const auto encoded_bytes = std::forward<Operation>(operation)();
    std::cout << name << ": verified " << encoded_bytes << " encoded bytes\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << name << ": " << error.what() << '\n';
    return 1;
  }
}
} // namespace iostream_example
