#include <rohit/serializer.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {
struct allocation_probe {
  void* owned{};
  bool watching{};
  bool fail_malloc{};
  bool fail_realloc{};
  std::size_t failures{};
  std::size_t releases{};
};
thread_local allocation_probe probe{};
constexpr std::array<std::uint8_t, 4> prefix{'a', 'b', 'c', 'd'};

// Keep assertions independent of NDEBUG and run them only after the injected call has finished.
void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error{message};
  }
}

// Require the real stream allocation path to report its documented exception exactly once.
template <typename Operation>
void expect_allocation_failure(Operation&& operation) {
  bool caught{};
  try {
    operation();
  } catch (const rohit::exception::memory_allocation_exception&) {
    caught = true;
  }
  probe.fail_malloc = probe.fail_realloc = false;
  require(caught && probe.failures == 1, "Expected one injected allocation failure");
}

enum class growth_operation { reserve, byte, increment, range, aliased_append, compact_integer };

// Reach the shared realloc branch through distinct public cursor and output operations.
template <typename Stream>
void request_growth(Stream& output, growth_operation operation) {
  switch (operation) {
  case growth_operation::reserve:
    output.reserve(1);
    break;
  case growth_operation::byte:
    output.append(std::uint8_t{0xff});
    break;
  case growth_operation::increment:
    ++output;
    break;
  case growth_operation::range:
    output.get_curr_and_increase(1);
    break;
  case growth_operation::aliased_append:
    output.append(output.begin(), prefix.size());
    break;
  case growth_operation::compact_integer: {
    rohit::serializer::binary_none<rohit::serializer::serialize_type::out> encoder{output};
    encoder.serialize_out_variable(0x4000U);
    break;
  }
  }
}

// Prove failed realloc retains every byte and pointer, and recovery/move still frees exactly once.
template <typename Stream>
void check_growth(growth_operation operation) {
  probe = {};
  {
    auto* allocation = static_cast<std::uint8_t*>(std::malloc(prefix.size()));
    require(allocation != nullptr, "Unable to allocate test fixture");
    Stream output{allocation, prefix.size()};
    output.append(prefix.data(), prefix.size());
    const auto* const cursor = output.curr();
    const auto* const end = output.end();
    probe.owned = allocation;
    probe.watching = true;
    probe.fail_realloc = true;
    expect_allocation_failure([&] { request_growth(output, operation); });
    require(output.begin() == allocation && output.curr() == cursor && output.end() == end,
            "Failed growth changed stream pointers");
    require(output.capacity() == prefix.size() && output.current_offset() == prefix.size(),
            "Failed growth changed stream capacity or cursor");
    require(std::equal(prefix.begin(), prefix.end(), output.begin()),
            "Failed growth changed bytes");
    require(probe.releases == 0, "Failed growth released the owned buffer");

    output.append(std::uint8_t{0x7e});
    require(output.current_offset() == prefix.size() + 1 && output.begin()[prefix.size()] == 0x7e,
            "Stream could not recover after failed growth");
    require(std::equal(prefix.begin(), prefix.end(), output.begin()),
            "Recovery changed the prefix");
    Stream moved{std::move(output)};
    require(moved.begin() == probe.owned && output.begin() == nullptr,
            "Ownership was not transferred after recovery");
  }
  require(probe.releases == 1, "Owned buffer was not released exactly once");
  probe = {};
}

// Initial lazy realloc failure must retain the null/empty state and permit a later write.
template <typename Stream>
void check_lazy_growth() {
  probe = {};
  {
    Stream output{};
    probe.watching = true;
    probe.fail_realloc = true;
    expect_allocation_failure([&] { output.append(std::uint8_t{1}); });
    require(output.begin() == nullptr && output.curr() == nullptr && output.end() == nullptr,
            "Failed initial growth changed an empty stream");
    output.append(std::uint8_t{1});
    require(output.current_offset() == 1 && output.begin()[0] == 1,
            "Empty stream could not recover");
  }
  require(probe.releases == 1, "Recovered lazy allocation was not released exactly once");
  probe = {};
}

// Failed replacement allocation must leave the old buffer owned by its original stream.
template <typename Stream>
void check_replacement() {
  probe = {};
  {
    auto* allocation = static_cast<std::uint8_t*>(std::malloc(prefix.size()));
    require(allocation != nullptr, "Unable to allocate test fixture");
    Stream output{allocation, prefix.size()};
    output.append(prefix.data(), prefix.size());
    const auto* const cursor = output.curr();
    const auto* const end = output.end();
    probe.owned = allocation;
    probe.watching = true;
    probe.fail_malloc = true;
    expect_allocation_failure([&] {
      if constexpr (std::is_same_v<Stream, rohit::full_stream_auto_alloc>) {
        output.return_old_and_alloc(prefix.size() * 2);
      } else {
        output.return_old_and_alloc();
      }
    });
    require(output.begin() == allocation && output.curr() == cursor && output.end() == end,
            "Failed replacement changed stream pointers");
    require(std::equal(prefix.begin(), prefix.end(), output.begin()),
            "Failed replacement changed bytes");
    require(probe.releases == 0, "Failed replacement released the original allocation");
  }
  require(probe.releases == 1, "Original allocation was not released exactly once");
  probe = {};
}

// Run the same ownership and failure contract against both allocating stream policies.
template <typename Stream>
void check_stream() {
  for (const auto operation :
       {growth_operation::reserve, growth_operation::byte, growth_operation::increment,
        growth_operation::range, growth_operation::aliased_append,
        growth_operation::compact_integer}) {
    check_growth<Stream>(operation);
  }
  check_lazy_growth<Stream>();
  check_replacement<Stream>();
}
} // namespace

// GNU-compatible ELF linkers resolve these names to the actual C allocator functions.
extern "C" void* __real_malloc(std::size_t size);
extern "C" void* __real_realloc(void* pointer, std::size_t size);
extern "C" void __real_free(void* pointer);

// Fail one malloc request while leaving all unrelated allocations on their ordinary path.
extern "C" void* __wrap_malloc(std::size_t size) {
  if (std::exchange(probe.fail_malloc, false)) {
    ++probe.failures;
    return nullptr;
  }
  return __real_malloc(size);
}

// Track only the tested stream and implement realloc's null-result ownership contract.
extern "C" void* __wrap_realloc(void* pointer, std::size_t size) {
  const bool tracked = probe.watching && pointer == probe.owned;
  if (tracked && std::exchange(probe.fail_realloc, false)) {
    ++probe.failures;
    return nullptr;
  }
  auto* result = __real_realloc(pointer, size);
  if (tracked && result != nullptr) {
    probe.owned = result;
  }
  return result;
}

// Observe release of the currently owned allocation without changing allocator behavior.
extern "C" void __wrap_free(void* pointer) {
  if (probe.watching && pointer != nullptr && pointer == probe.owned) {
    ++probe.releases;
    probe.owned = nullptr;
  }
  __real_free(pointer);
}

// Keep allocator interception isolated from GoogleTest and other test processes.
int main() {
  try {
    check_stream<rohit::full_stream_auto_alloc>();
    check_stream<rohit::full_stream_auto_alloc_limits>();
    std::cout << "Passed 16 deterministic allocation-failure scenarios\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
