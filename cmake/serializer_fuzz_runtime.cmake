# Keep coverage and sanitizers out of the normal library, generator, and installed package.
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
    CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  message(FATAL_ERROR "Fuzzing requires Clang's GNU-style driver with libFuzzer, ASan, and UBSan")
endif()
include(CMakePushCheckState)
include(CheckCXXSourceCompiles)
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_FLAGS "-fsanitize=fuzzer,address,undefined")
check_cxx_source_compiles("
  #include <cstddef>
  #include <cstdint>
  extern \"C\" int LLVMFuzzerTestOneInput(const std::uint8_t*, std::size_t) { return 0; }
" SERIALIZER_HAS_FUZZ_SANITIZERS)
cmake_pop_check_state()
if(NOT SERIALIZER_HAS_FUZZ_SANITIZERS)
  message(FATAL_ERROR "The selected Clang toolchain cannot link libFuzzer, ASan, and UBSan")
endif()

add_library(serializer_fuzz_instrumentation INTERFACE)
target_compile_options(serializer_fuzz_instrumentation INTERFACE
  -fsanitize=fuzzer-no-link,address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
target_link_options(serializer_fuzz_instrumentation INTERFACE -fsanitize=address,undefined)

# Clone the actual source list in the root directory so source-specific AVX2 options also apply.
# Mirroring target settings keeps future compiled helpers instrumented without a second source list.
get_target_property(serializer_fuzz_sources serializer_lib SOURCES)
add_library(serializer_fuzz_runtime STATIC ${serializer_fuzz_sources})
target_include_directories(serializer_fuzz_runtime PUBLIC
  "$<TARGET_PROPERTY:serializer_lib,INTERFACE_INCLUDE_DIRECTORIES>")
target_compile_features(serializer_fuzz_runtime PUBLIC
  "$<TARGET_PROPERTY:serializer_lib,INTERFACE_COMPILE_FEATURES>")
target_compile_definitions(serializer_fuzz_runtime PRIVATE
  "$<TARGET_PROPERTY:serializer_lib,COMPILE_DEFINITIONS>")
target_compile_options(serializer_fuzz_runtime PRIVATE
  "$<TARGET_PROPERTY:serializer_lib,COMPILE_OPTIONS>")
target_link_libraries(serializer_fuzz_runtime PUBLIC serializer_fuzz_instrumentation)
target_link_libraries(serializer_fuzz_runtime PRIVATE ${serializer_compression_libraries})
