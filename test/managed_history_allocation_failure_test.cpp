#include <rohit/managed.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
bool fail_allocations = false;
std::size_t allocations_before_failure = 0;

// Inject allocation failure only inside append, after preparing the input and expected history.
void* allocate(std::size_t size) {
  if (fail_allocations && allocations_before_failure-- == 0) {
    throw std::bad_alloc{};
  }
  auto* pointer = std::malloc(size == 0 ? 1 : size);
  if (pointer == nullptr) {
    throw std::bad_alloc{};
  }
  return pointer;
}

// Compare all retained state, including the cursor and cached byte accounting, after failure.
template <rohit::managed::history_labels Labels>
bool equal(const rohit::managed::detail::linear_history<Labels>& left,
           const rohit::managed::detail::linear_history<Labels>& right) {
  if (left.cursor != right.cursor || left.bytes != right.bytes ||
      left.entries.size() != right.entries.size()) {
    return false;
  }
  for (std::size_t index = 0; index < left.entries.size(); ++index) {
    const auto& a = left.entries[index];
    const auto& b = right.entries[index];
    if constexpr (Labels == rohit::managed::history_labels::enabled) {
      if (a.label != b.label) {
        return false;
      }
    }
    if (a.snapshot != b.snapshot) {
      return false;
    }
  }
  return true;
}

// Exercise deque block growth with both oldest-state eviction and redo removal pending.
template <rohit::managed::history_labels Labels>
void check_append_failures(bool with_redo) {
  namespace managed = rohit::managed;
  managed::store_options options;
  constexpr std::size_t maximum_states = 2048;
  options.max_revisions = maximum_states;
  managed::detail::linear_history<Labels> history;
  std::size_t failures = 0;
  std::size_t maximum_failures_per_append = 0;
  for (std::size_t count = 1; count <= maximum_states; ++count) {
    managed::detail::history_entry<Labels> old;
    old.snapshot = {1, 2};
    if constexpr (Labels == managed::history_labels::enabled) {
      old.label = "old";
    }
    history.append(std::move(old), options);
    if (with_redo && count > 1) {
      history.cursor = count - 2;
    }
    const auto before = history;
    auto limits = options;
    limits.max_revisions = 1;
    for (std::size_t fail_at = 0;; ++fail_at) {
      // Replay normal growth for each failure position. Deque copy assignment can retain
      // shifted storage and skip allocation boundaries; a deque copy can reserve its map.
      managed::detail::linear_history<Labels> attempted;
      for (const auto& entry : before.entries) {
        attempted.append(entry, options);
      }
      attempted.cursor = before.cursor;
      managed::detail::history_entry<Labels> candidate;
      candidate.snapshot = {3, 4};
      if constexpr (Labels == managed::history_labels::enabled) {
        candidate.label = "new";
      }
      const auto expected_bytes = candidate.bytes();
      bool failed = false;
      allocations_before_failure = fail_at;
      fail_allocations = true;
      try {
        attempted.append(std::move(candidate), limits);
      } catch (const std::bad_alloc&) {
        failed = true;
      } catch (...) {
        fail_allocations = false;
        throw;
      }
      fail_allocations = false;
      if (failed) {
        ++failures;
        if (!equal(attempted, before)) {
          throw std::runtime_error{"Failed append changed retained history"};
        }
        // Retry, permitting one more allocation; also cover failure after internal map growth.
        continue;
      }
      maximum_failures_per_append = std::max(maximum_failures_per_append, fail_at);
      if (attempted.entries.size() != 1 || attempted.cursor != 0 ||
          attempted.bytes != expected_bytes ||
          attempted.entries.front().snapshot != std::vector<std::uint8_t>{3, 4}) {
        throw std::runtime_error{"Append did not complete eviction correctly"};
      }
      if constexpr (Labels == managed::history_labels::enabled) {
        if (attempted.entries.front().label != "new") {
          throw std::runtime_error{"Append lost its label"};
        }
      }
      break;
    }
    history.cursor = history.entries.size() - 1;
  }
  if (failures == 0) {
    throw std::runtime_error{"Deque allocation failure was not exercised"};
  }
  // Map growth followed by block allocation must also preserve history when the second fails.
  constexpr std::size_t minimum_failures_per_append = 2;
  if (maximum_failures_per_append < minimum_failures_per_append) {
    throw std::runtime_error{"Deque map and block allocation failures were not exercised"};
  }
}
} // namespace

// Route object allocations through this executable's isolated failure probe.
void* operator new(std::size_t size) { return allocate(size); }
// Route array allocations through the same probe.
void* operator new[](std::size_t size) { return allocate(size); }
// Release object storage allocated with malloc.
void operator delete(void* pointer) noexcept { std::free(pointer); }
// Release array storage allocated with malloc.
void operator delete[](void* pointer) noexcept { std::free(pointer); }
// Support sized object deallocation.
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
// Support sized array deallocation.
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

// Keep injected allocation failures isolated from the test framework and other runtime tests.
int main() {
  try {
    using rohit::managed::history_labels;
    check_append_failures<history_labels::disabled>(false);
    check_append_failures<history_labels::disabled>(true);
    check_append_failures<history_labels::enabled>(false);
    check_append_failures<history_labels::enabled>(true);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
