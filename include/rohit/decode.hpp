#pragma once

#include <rohit/stream.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>

namespace rohit::serializer {

// Limits are copied into a decoder and shared by every nested value in that decoder.
// Allocation accounting measures cumulative requested storage, not allocator-specific heap usage.
struct decode_limits {
  static constexpr std::size_t mebibyte = 1024 * 1024;
  std::size_t max_input_bytes{64 * mebibyte};
  std::size_t max_string_bytes{16 * mebibyte};
  std::size_t max_collection_elements{mebibyte};
  std::size_t max_nesting_depth{64};
  std::size_t max_allocation_bytes{64 * mebibyte};
  std::size_t max_work_units{128 * mebibyte};
  rohit::exception::diagnostic_options diagnostics{};
};

namespace exception {
class bad_input_data : public rohit::exception::base_parser {
public:
  using rohit::exception::base_parser::base_parser;
};

class resource_limit : public rohit::exception::base_parser {
public:
  using rohit::exception::base_parser::base_parser;
  // Identify a configured decoding limit failure.
  rohit::exception::parser_error_code code() const noexcept override {
    return rohit::exception::parser_error_code::resource_limit;
  }
};

class numeric_range : public rohit::exception::base_parser {
public:
  using rohit::exception::base_parser::base_parser;
  // Identify a numeric value that cannot be represented by the destination.
  rohit::exception::parser_error_code code() const noexcept override {
    return rohit::exception::parser_error_code::numeric_range;
  }
};
} // namespace exception

namespace detail {

class decoder_input {
  std::size_t input_bytes{};
  std::size_t allocation_bytes{};
  std::size_t work_units{};
  std::size_t nesting_depth{};

protected:
  const stream& in_stream;
  const decode_limits limits;

  // Reject a request before touching input or allocating destination storage.
  [[noreturn]] void fail_limit(const char* message) const {
    throw exception::resource_limit{in_stream, message, limits.diagnostics};
  }

  // Bound a local scan by both the live input range and the remaining budgets.
  std::span<const std::uint8_t> available_input() const {
    return {in_stream.curr(), std::min({in_stream.remaining_buffer(),
        limits.max_input_bytes - input_bytes, limits.max_work_units - work_units})};
  }

  // Ensure a complete range can be consumed; report budgets separately from truncation.
  void require_input(std::size_t size) const {
    if (size > limits.max_input_bytes - input_bytes ||
        size > limits.max_work_units - work_units) {
      fail_limit("Input or work limit exceeded");
    }
    if (size > in_stream.remaining_buffer()) {
      throw exception::bad_input_data{in_stream, "Truncated input", limits.diagnostics};
    }
  }

  // Consume a previously inspected span with one cursor update and charge its byte work.
  const std::uint8_t* read_bytes(std::size_t size) {
    require_input(size);
    input_bytes += size;
    work_units += size;
    return in_stream.get_curr_and_increase_unchecked(size);
  }

  // Check a batch's byte and value work without overflow or committing any charges.
  bool has_work_budget(std::size_t byte_count, std::size_t value_count) const noexcept {
    const auto remaining_work = limits.max_work_units - work_units;
    return byte_count <= remaining_work && value_count <= remaining_work - byte_count;
  }

  // Probe a fixed-field batch without changing state; the scalar fallback owns failure diagnostics.
  bool can_read_batch(std::size_t byte_count, std::size_t value_count) const {
    return byte_count <= limits.max_input_bytes - input_bytes &&
           has_work_budget(byte_count, value_count) && byte_count <= in_stream.remaining_buffer();
  }

  // Commit a batch only after can_read_batch and all value validation succeed, with no intervening reads.
  const std::uint8_t* read_batch_unchecked(std::size_t byte_count,
                                           std::size_t value_count) noexcept {
    input_bytes += byte_count;
    work_units += byte_count + value_count;
    return in_stream.get_curr_and_increase_unchecked(byte_count);
  }

  // Charge operations that may consume no bytes, such as empty positional objects.
  void charge_work(std::size_t count = 1) {
    if (count > limits.max_work_units - work_units) {
      fail_limit("Work limit exceeded");
    }
    work_units += count;
  }

  // Charge conservative requested storage even when the destination reuses its capacity.
  void charge_allocation(std::size_t count, std::size_t element_bytes = 1) {
    if (element_bytes != 0 && count > (limits.max_allocation_bytes - allocation_bytes) / element_bytes) {
      fail_limit("Allocation limit exceeded");
    }
    allocation_bytes += count * element_bytes;
  }

  // Validate a collection count before reserving or traversing its elements.
  void check_collection(std::size_t count, std::size_t element_bytes) {
    if (count > limits.max_collection_elements) {
      fail_limit("Collection element limit exceeded");
    }
    charge_work(count);
    charge_allocation(count, element_bytes);
  }

  // Validate decoded string storage before changing the destination.
  void check_string(std::size_t size, bool allocate = true) {
    if (size > limits.max_string_bytes) {
      fail_limit("String length limit exceeded");
    }
    if (allocate) {
      charge_allocation(size);
    }
  }

public:
  // Borrow a fixed input range and snapshot the limits for this decoding session.
  decoder_input(const stream& input, decode_limits input_limits = {})
      : in_stream{input}, limits{input_limits} {}

  // A decoding session owns its accounting; nested readers must share it by reference.
  decoder_input(const decoder_input&) = delete;
  // Do not fork budgets or replace the borrowed cursor of an active session.
  decoder_input& operator=(const decoder_input&) = delete;

  // Borrow the same cursor used by nested generated serializers.
  const stream& get_stream() const noexcept {
    return in_stream;
  }

  // Scope nesting with automatic restoration on both successful and failed decoding.
  class scope {
    decoder_input& input;
  public:
    // Account for one aggregate before any recursive work begins.
    explicit scope(decoder_input& input) : input{input} {
      if (input.nesting_depth >= input.limits.max_nesting_depth) {
        input.fail_limit("Nesting depth limit exceeded");
      }
      input.charge_work();
      ++input.nesting_depth;
    }
    // Each scope has exactly one owner.
    scope(const scope&) = delete;
    // Each scope has exactly one owner.
    scope& operator=(const scope&) = delete;
    // Restore depth even when an inner decoder throws.
    ~scope() {
      --input.nesting_depth;
    }
  };

  // Start an aggregate scope; the returned guard must outlive its nested values.
  [[nodiscard]] scope enter_object() {
    return scope{*this};
  }
};

struct empty_decode_scope {};

// Preserve custom protocols while enabling nesting limits in generated positional readers.
template <typename Protocol>
auto enter_decode_object(Protocol& protocol) {
  if constexpr (requires { protocol.enter_object(); }) {
    return protocol.enter_object();
  } else {
    return empty_decode_scope{};
  }
}

} // namespace detail
} // namespace rohit::serializer
