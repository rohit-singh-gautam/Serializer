#include <cstddef>
#include <cstdint>

namespace qualification {
namespace {
const void* volatile observed_address{};
volatile std::size_t observed_size{};
}

// Keep the complete caller-visible buffer or object observable across a separate translation unit.
void observe(const void* data, std::size_t size) noexcept {
  observed_address = data;
  observed_size = size;
}
} // namespace qualification
