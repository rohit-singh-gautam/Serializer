# Fail during configuration when the compiler/library pair cannot build public headers.
include(CheckCXXSourceCompiles)
include(CMakePushCheckState)
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_INCLUDES "${CMAKE_CURRENT_LIST_DIR}/../include")
check_cxx_source_compiles([=[
#include <rohit/cpp_standard.hpp>
#include <bit>
#include <cstdint>
#include <span>
static_assert(std::byteswap(std::uint32_t{0x01020304}) == 0x04030201);
// Require the context branch used by generated checked arithmetic helpers.
constexpr bool constant_context() {
  if consteval { return true; } else { return false; }
}
static_assert(constant_context());
// Exercise the library operations used by public runtime headers.
int main() {
  int values[]{1, 2};
  return std::span{values}.size() == 2 ? 0 : 1;
}
]=] SERIALIZER_HAS_CPP23_TOOLCHAIN)
cmake_pop_check_state()
if(NOT SERIALIZER_HAS_CPP23_TOOLCHAIN)
  message(FATAL_ERROR
    "Serializer requires C++23 if consteval and a matching standard library with std::byteswap and std::span")
endif()
