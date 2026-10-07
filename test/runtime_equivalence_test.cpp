#include "runtime_equivalence.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
namespace codec = rohit::serializer;
namespace models = runtime_equivalence;

// Observe the existing stream's reservation and allocation requests without modifying production code.
class observed_stream final : public rohit::full_stream_auto_alloc {
public:
  static constexpr std::size_t initial_capacity_bytes = 16;
  static constexpr std::size_t maximum_reservation_calls = 128;

  struct reservation {
    std::size_t offset_bytes{};
    std::size_t requested_bytes{};
    std::size_t capacity_before_bytes{};
    std::size_t capacity_after_bytes{};
  };

  std::array<reservation, maximum_reservation_calls> reservations{};
  std::size_t reservation_count{};
  std::size_t allocation_count{1};
  std::size_t allocation_requested_bytes{initial_capacity_bytes};

  // Begin with a small real allocation so the fixture exercises stream growth.
  observed_stream() : rohit::full_stream_auto_alloc{initial_capacity_bytes} {}

  // Record each exact request and growth before the existing serializer writes its bytes.
  void reserve(std::size_t bytes) override {
    if (reservation_count == reservations.size()) {
      throw std::runtime_error{"Runtime equivalence reservation trace exhausted"};
    }
    const auto offset = current_offset();
    const auto previous_capacity = capacity();
    rohit::full_stream_auto_alloc::reserve(bytes);
    const auto next_capacity = capacity();
    reservations[reservation_count++] = {offset, bytes, previous_capacity, next_capacity};
    if (next_capacity != previous_capacity) {
      // The production stream performs one realloc with this capacity for each growth.
      ++allocation_count;
      allocation_requested_bytes += next_capacity;
    }
  }
};

// Make the identical owning value in both independently compiled generation profiles.
models::message make_message() {
  models::message value{};
  value.code = 0x1234;
  value.enabled = true;
  value.sequence = 0x01020304;
  value.elapsed = 0x1122334455667788;
  value.label = std::string(200, 'a');
  value.samples = {0, 255, 256, 65535};
  value.state = models::phase::active;
  value.result_type = models::message::e_result::count;
  value.result.count = 0xfedcba98;
  return value;
}

// Check that the existing runtime reader consumes the complete message and preserves every field.
void verify_round_trip(const observed_stream& output, const models::message& original) {
  const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
  codec::binary_none<codec::serialize_type::in> reader{input};
  models::message decoded{};
  reader.serialize_in(decoded);
  reader.finish();
  if (decoded.code != original.code || decoded.enabled != original.enabled ||
      decoded.sequence != original.sequence || decoded.elapsed != original.elapsed ||
      decoded.label != original.label || decoded.samples != original.samples ||
      decoded.state != original.state || decoded.result_type != original.result_type ||
      decoded.result.count != original.result.count) {
    throw std::runtime_error{"Runtime equivalence round trip failed"};
  }
}
} // namespace

// Report exact layout, bytes and stream allocation behavior for the external profile comparison.
int main() {
  try {
    const auto value = make_message();
    observed_stream output{};
    value.serialize_out<codec::binary_none>(output);
    verify_round_trip(output, value);
    std::cout << "layout " << sizeof(models::base_record) << ' ' << alignof(models::base_record)
              << ' ' << sizeof(models::message) << ' ' << alignof(models::message) << '\n';
    std::cout << "storage " << output.current_offset() << ' ' << output.capacity() << ' '
              << output.allocation_count << ' ' << output.allocation_requested_bytes << '\n';
    std::cout << "reservations " << output.reservation_count << '\n';
    for (std::size_t index = 0; index < output.reservation_count; ++index) {
      const auto& request = output.reservations[index];
      std::cout << request.offset_bytes << ' ' << request.requested_bytes << ' '
                << request.capacity_before_bytes << ' ' << request.capacity_after_bytes << '\n';
    }
    std::cout << "bytes ";
    for (std::size_t index = 0; index < output.current_offset(); ++index) {
      std::cout << std::hex << std::setfill('0') << std::setw(2)
                << static_cast<unsigned>(output.begin()[index]);
    }
    std::cout << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
