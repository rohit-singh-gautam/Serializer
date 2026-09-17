#pragma once

#include <fuzz_models.hpp>
#include <rohit/serializer.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <stdexcept>

namespace qualification {
inline constexpr std::size_t fuzz_payload_bytes = 4096;
inline constexpr std::size_t fuzz_control_bytes = 3;
inline constexpr std::size_t fuzz_alignment_offsets = 32;
inline constexpr std::size_t fuzz_policy_count = 7;
inline constexpr std::uint8_t native_protocol_count = 11;
inline constexpr std::uint8_t protobuf_protocol_count = 3;
using owning_record = fuzz_models::record<rohit::serializer::storage_mode::owning>;

// Give each attempt fresh budgets, with one restrictive dimension selected by input.
inline rohit::serializer::decode_limits fuzz_limits(std::uint8_t policy) {
  rohit::serializer::decode_limits limits{};
  limits.max_input_bytes = fuzz_payload_bytes;
  limits.max_string_bytes = 1024;
  limits.max_collection_elements = 64;
  limits.max_nesting_depth = 8;
  limits.max_allocation_bytes = 16384;
  limits.max_work_units = 8192;
  switch (policy % fuzz_policy_count) {
  case 1:
    limits.max_input_bytes = 16;
    break;
  case 2:
    limits.max_string_bytes = 16;
    break;
  case 3:
    limits.max_collection_elements = 2;
    break;
  case 4:
    limits.max_nesting_depth = 1;
    break;
  case 5:
    limits.max_allocation_bytes = 32;
    break;
  case 6:
    limits.max_work_units = 32;
    break;
  default:
    break;
  }
  return limits;
}

// Own an exact allocation ending at the payload boundary, including odd starting alignments.
// The three control bytes select a protocol, a budget policy, and the starting alignment.
struct fuzz_input {
  std::unique_ptr<std::uint8_t[]> allocation;
  std::span<std::uint8_t> bytes;
  rohit::serializer::decode_limits limits;
  std::uint8_t selector;

  // Require fuzz_control_bytes <= size <= fuzz_control_bytes + fuzz_payload_bytes.
  fuzz_input(const std::uint8_t* data, std::size_t size)
      : allocation{std::make_unique<std::uint8_t[]>(size - fuzz_control_bytes +
                                                    data[2] % fuzz_alignment_offsets)},
        bytes{allocation.get() + data[2] % fuzz_alignment_offsets, size - fuzz_control_bytes},
        limits{fuzz_limits(data[1])}, selector{data[0]} {
    if (!bytes.empty()) {
      std::memcpy(bytes.data(), data + fuzz_control_bytes, bytes.size());
    }
  }
};

// Ignore documented input rejection; unexpected exceptions and sanitizer failures escape.
template <typename Operation>
void reject_invalid_input(Operation&& operation) {
  try {
    operation();
  } catch (const rohit::exception::base_parser&) {
  } catch (const std::invalid_argument&) {
    // Generated enum-name conversion also uses invalid_argument for malformed alternatives.
  }
}

// Exercise exact-message decoding with fresh, shared per-message accounting.
template <typename Protocol, typename Value = owning_record>
void decode_input(const fuzz_input& input) {
  reject_invalid_input([&] {
    const auto stream = rohit::make_constant_full_stream(input.bytes.data(), input.bytes.size());
    Protocol decoder{stream, input.limits};
    Value value{};
    decoder.serialize_in(value);
    decoder.finish();
  });
}
} // namespace qualification
