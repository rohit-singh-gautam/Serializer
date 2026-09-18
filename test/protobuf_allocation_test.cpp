#include <rohit/protobuf.hpp>

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace codec = rohit::serializer;
constexpr std::size_t maximum_test_allocation_bytes = 4096;
bool watch_allocations{};

// Permit small runtime diagnostics, but reject payload-sized allocations during bounded decoding.
void* allocate(std::size_t size) {
  if (watch_allocations && size > maximum_test_allocation_bytes) {
    throw std::bad_alloc{};
  }
  auto* result = std::malloc(size == 0 ? 1 : size);
  if (result == nullptr) {
    throw std::bad_alloc{};
  }
  return result;
}

// A minimal message isolates TextProto storage accounting from generated container initialization.
struct text_message {
  std::string text{};

  // Restore the standard Protobuf string default for replacement decoding.
  void serializer_protobuf_reset() {
    text.clear();
  }

  // Dispatch the only field through the same runtime operations as a generated message.
  template <typename Protocol>
  void serializer_protobuf_read(Protocol& input) {
    bool seen{};
    while (input.next_field()) {
      if (input.template match<1>("s", "s")) {
        input.template occurrence<false>(seen);
        input.field(text);
      } else {
        input.unknown();
      }
    }
  }
};

// The budget must fail before large allocation attempts, retaining the caller's destination.
void check_preallocation_limit(std::string_view wire, codec::decode_limits limits) {
  const auto stream = rohit::make_constant_stream(wire.data(), wire.size());
  codec::textproto<codec::serialize_type::in> input{stream, limits};
  text_message value{"unchanged"};
  bool limited{};
  watch_allocations = true;
  try {
    input.serialize_in(value);
  } catch (const codec::exception::resource_limit&) {
    limited = true;
  } catch (...) {
    watch_allocations = false;
    throw;
  }
  watch_allocations = false;
  if (!limited || value.text != "unchanged") {
    throw std::runtime_error{"TextProto did not reject storage growth transactionally"};
  }
}
} // namespace

// Route ordinary object allocations through the isolated payload-allocation guard.
void* operator new(std::size_t size) {
  return allocate(size);
}
// Apply the same guard to array allocations.
void* operator new[](std::size_t size) {
  return allocate(size);
}
// Release ordinary storage through the matching allocator.
void operator delete(void* pointer) noexcept {
  std::free(pointer);
}
// Release array storage through the matching allocator.
void operator delete[](void* pointer) noexcept {
  std::free(pointer);
}
// Support compilers that emit sized object deallocation.
void operator delete(void* pointer, std::size_t) noexcept {
  std::free(pointer);
}
// Support compilers that emit sized array deallocation.
void operator delete[](void* pointer, std::size_t) noexcept {
  std::free(pointer);
}

// Exercise ordinary text, concatenated fragments, and Unicode escapes under a tiny storage budget.
int main() {
  try {
    constexpr std::size_t payload_bytes = 64 * 1024;
    const std::string payload(payload_bytes, 'a');
    codec::decode_limits limits{};
    limits.max_allocation_bytes = 0;
    check_preallocation_limit("s:'" + payload + "'", limits);
    limits.max_allocation_bytes = 2;
    check_preallocation_limit("s:'ab' '" + payload + "'", limits);
    check_preallocation_limit("s:'ab\\U0001f600" + payload + "'", limits);
    limits.max_allocation_bytes = 4;
    check_preallocation_limit("s:'\\U0001f600" + payload + "'", limits);
    std::cout << "TextProto rejected storage growth before large allocations\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
