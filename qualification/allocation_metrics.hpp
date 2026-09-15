#pragma once

#include <rohit/stream.hpp>

#include <cstddef>
#include <cstdint>

namespace qualification::memory {
struct metrics {
  std::uint64_t allocations{};
  std::uint64_t requested_bytes{};
  std::uint64_t live_bytes{};
  std::uint64_t peak_bytes{};
};

// Start a separate allocation sample; preexisting storage is outside the sample's accounting.
void start() noexcept;
// Stop collecting and return requested storage metrics, excluding allocator metadata.
metrics finish() noexcept;
// Return the active sample, or zero when profiling is disabled.
std::size_t active_epoch() noexcept;
// Record malloc-backed stream storage separately from intercepted C++ new allocations.
void add_buffer(std::size_t size) noexcept;
// Release storage charged to this sample; older samples do not affect its live count.
void remove_buffer(std::size_t size, std::size_t epoch) noexcept;

class profiled_stream : public rohit::full_stream_auto_alloc {
  std::size_t tracked_capacity{};
  std::size_t epoch{};
public:
  // Count the initial allocation when it occurs inside a profiling sample.
  explicit profiled_stream(std::size_t size) : rohit::full_stream_auto_alloc{size} {
    epoch = active_epoch();
    if (epoch != 0 && capacity() != 0) {
      tracked_capacity = capacity();
      add_buffer(tracked_capacity);
    }
  }
  // Report released capacity before the owning base frees it.
  ~profiled_stream() override {
    remove_buffer(tracked_capacity, epoch);
  }
  // Preserve stream policy while counting each successful growth request.
  void reserve(std::size_t size) override {
    const auto before = capacity();
    rohit::full_stream_auto_alloc::reserve(size);
    const auto after = capacity();
    if (after != before && active_epoch() != 0) {
      // Count old plus new storage as a conservative moving-realloc peak.
      add_buffer(after);
      remove_buffer(tracked_capacity, epoch);
      tracked_capacity = after;
      epoch = active_epoch();
    }
  }
};
} // namespace qualification::memory
