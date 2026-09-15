#include "allocation_metrics.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <new>

namespace qualification::memory {
namespace {
thread_local metrics current{};
thread_local std::size_t epoch{};
thread_local bool enabled{};

struct alignas(std::max_align_t) allocation_header {
  void* allocation;
  std::size_t size;
  std::size_t sample;
};
}

// Record requested payload bytes, independently of allocator bookkeeping and size classes.
void add_buffer(std::size_t size) noexcept {
  if (!enabled) { return; }
  ++current.allocations;
  current.requested_bytes += size;
  current.live_bytes += size;
  current.peak_bytes = std::max(current.peak_bytes, current.live_bytes);
}

// Retire only storage that was allocated during the current profiling sample.
void remove_buffer(std::size_t size, std::size_t sample) noexcept {
  if (sample != 0 && sample == epoch) { current.live_bytes -= size; }
}

// Begin an independent sample without allocating memory in the profiler itself.
void start() noexcept {
  ++epoch;
  current = {};
  enabled = true;
}

// Snapshot before any reporting code allocates strings or stream buffers.
metrics finish() noexcept {
  enabled = false;
  return current;
}

// Zero means that this allocation belongs to setup or reporting, not a measured sample.
std::size_t active_epoch() noexcept {
  return enabled ? epoch : 0;
}

// Store allocation metadata outside an alignment-correct payload; used only by the profile executable.
void* allocate(std::size_t size, std::size_t alignment) {
  alignment = std::max(alignment, alignof(allocation_header));
  size = std::max(size, std::size_t{1});
  if (alignment - 1 > std::numeric_limits<std::size_t>::max() - sizeof(allocation_header) ||
      size > std::numeric_limits<std::size_t>::max() - sizeof(allocation_header) - (alignment - 1)) {
    throw std::bad_alloc{};
  }
  void* allocation = std::malloc(size + sizeof(allocation_header) + alignment - 1);
  if (!allocation) { throw std::bad_alloc{}; }
  const auto address = reinterpret_cast<std::uintptr_t>(allocation) + sizeof(allocation_header);
  const auto aligned = (address + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1);
  auto* header = reinterpret_cast<allocation_header*>(aligned) - 1;
  ::new (static_cast<void*>(header)) allocation_header{allocation, size, active_epoch()};
  add_buffer(size);
  return reinterpret_cast<void*>(aligned);
}

// Recover the original malloc pointer and remove its payload from the current sample if applicable.
void release(void* pointer) noexcept {
  if (!pointer) { return; }
  auto* header = reinterpret_cast<allocation_header*>(pointer) - 1;
  remove_buffer(header->size, header->sample);
  std::free(header->allocation);
}
} // namespace qualification::memory

// Instrument all standard C++ allocation forms in the separate profiling executable.
void* operator new(std::size_t size) { return qualification::memory::allocate(size, alignof(std::max_align_t)); }
// Instrument array allocation without relying on implementation-specific array cookies.
void* operator new[](std::size_t size) { return ::operator new(size); }
// Release an ordinary tracked allocation.
void operator delete(void* pointer) noexcept { qualification::memory::release(pointer); }
// Release a tracked array allocation.
void operator delete[](void* pointer) noexcept { ::operator delete(pointer); }
// Sized deletion uses the recorded payload size.
void operator delete(void* pointer, std::size_t) noexcept { ::operator delete(pointer); }
// Sized array deletion uses the recorded payload size.
void operator delete[](void* pointer, std::size_t) noexcept { ::operator delete(pointer); }
// Preserve extended alignment in tracked allocations.
void* operator new(std::size_t size, std::align_val_t alignment) { return qualification::memory::allocate(size, static_cast<std::size_t>(alignment)); }
// Preserve extended alignment for arrays.
void* operator new[](std::size_t size, std::align_val_t alignment) { return ::operator new(size, alignment); }
// Release an aligned allocation through its recorded original pointer.
void operator delete(void* pointer, std::align_val_t) noexcept { ::operator delete(pointer); }
// Release an aligned array allocation.
void operator delete[](void* pointer, std::align_val_t) noexcept { ::operator delete(pointer); }
// Release a sized aligned allocation.
void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept { ::operator delete(pointer); }
// Release a sized aligned array allocation.
void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept { ::operator delete(pointer); }
