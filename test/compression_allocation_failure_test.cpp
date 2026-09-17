#include <rohit/compression.hpp>

#include <array>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <vector>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace {
struct allocation_probe {
  bool active{};
  std::size_t fail_at{};
  std::size_t attempts{};
  std::array<void*, 128> owned{};
} probe;

// Allocate normally outside the tested operation; track C++ scratch/output allocations inside it.
void* allocate(std::size_t size) {
  if (probe.active && probe.attempts++ == probe.fail_at) {
    throw std::bad_alloc{};
  }
  auto* pointer = std::malloc(size == 0 ? 1 : size);
  if (pointer == nullptr) {
    throw std::bad_alloc{};
  }
  if (probe.active) {
    for (auto& slot : probe.owned) {
      if (slot == nullptr) {
        slot = pointer;
        return pointer;
      }
    }
    std::abort();
  }
  return pointer;
}

// Track releases even during exception unwinding and after injection has been disarmed.
void release(void* pointer) noexcept {
  if (pointer != nullptr) {
    for (auto& slot : probe.owned) {
      if (slot == pointer) {
        slot = nullptr;
        break;
      }
    }
    std::free(pointer);
  }
}

// Fail each C++ allocation position until one complete operation succeeds; every attempt must clean up.
template <typename Operation>
std::size_t check_failures(Operation operation) {
  constexpr std::size_t maximum_attempts = 128;
  for (std::size_t index = 0; index < maximum_attempts; ++index) {
    probe.fail_at = index;
    probe.attempts = 0;
    probe.active = true;
    bool completed{};
    try {
      operation();
      completed = true;
    } catch (const std::bad_alloc&) {
    } catch (...) {
      probe.active = false;
      throw;
    }
    probe.active = false;
    for (const auto* pointer : probe.owned) {
      if (pointer != nullptr) {
        throw std::runtime_error{"Compression allocation leaked"};
      }
    }
    if (completed) {
      return index;
    }
  }
  throw std::runtime_error{"Compression did not recover from allocation failures"};
}
} // namespace

// Route standard C++ allocation families through the isolated test probe.
void* operator new(std::size_t size) {
  return allocate(size);
}
// Route array storage through the same probe.
void* operator new[](std::size_t size) {
  return allocate(size);
}
// Release ordinary object storage.
void operator delete(void* pointer) noexcept {
  release(pointer);
}
// Release array storage.
void operator delete[](void* pointer) noexcept {
  release(pointer);
}
// Support compilers that emit sized deallocation.
void operator delete(void* pointer, std::size_t) noexcept {
  release(pointer);
}
// Support sized array deallocation.
void operator delete[](void* pointer, std::size_t) noexcept {
  release(pointer);
}

// Verify adapter-owned C++ storage cleanup; backend C allocations are covered by sanitizer leak checks.
int main() {
  namespace compression = rohit::serializer::compression;
#ifdef _MSC_VER
  // Report debug-CRT failures to CTest instead of opening an interactive assertion dialog.
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
  try {
    std::vector<std::uint8_t> original(200000);
    std::uint32_t state = 12345;
    for (auto& byte : original) {
      state ^= state << 13;
      state ^= state >> 17;
      state ^= state << 5;
      byte = static_cast<std::uint8_t>(state);
    }
    const std::array formats{compression::format::none, compression::format::zstd,
                             compression::format::lz4,  compression::format::gzip,
                             compression::format::zlib, compression::format::deflate};
    const std::array<compression::encode_options, 6> options{
        compression::none_options{}, compression::zstd_options{}, compression::lz4_options{},
        compression::gzip_options{}, compression::zlib_options{}, compression::deflate_options{}};
    std::size_t failures{};
    for (std::size_t index = 0; index < formats.size(); ++index) {
      if (!compression::available(formats[index])) {
        continue;
      }
      const auto encoded = compression::compress(original, options[index]);
      failures += check_failures([&] {
        const auto result = compression::compress(original, options[index]);
        if (result != encoded) {
          throw std::runtime_error{"Compression recovery changed output"};
        }
      });
      failures += check_failures([&] {
        const auto result = compression::decompress(encoded, {.format = formats[index]});
        if (result != original) {
          throw std::runtime_error{"Decompression recovery changed bytes"};
        }
      });
    }
    std::cout << "Passed " << failures << " injected C++ allocation failures\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
