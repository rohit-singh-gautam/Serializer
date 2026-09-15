set(record_header "${CMAKE_CURRENT_BINARY_DIR}/records.hpp")
add_custom_command(
  OUTPUT "${record_header}"
  COMMAND serializer input "${CMAKE_CURRENT_SOURCE_DIR}/records.def" output "${record_header}"
    cpp.clang_format "${SERIALIZER_CLANG_FORMAT_EXECUTABLE}"
  DEPENDS serializer "${CMAKE_CURRENT_SOURCE_DIR}/records.def"
  VERBATIM
)
add_custom_target(serializer_qualification_headers DEPENDS "${record_header}")

if(SERIALIZER_BUILD_BENCHMARKS)
  foreach(target IN ITEMS codec_benchmark codec_allocation_profile)
    add_executable(${target} codec_benchmark.cpp benchmark_sink.cpp)
    add_dependencies(${target} serializer_qualification_headers)
    target_link_libraries(${target} PRIVATE serializer_lib)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_compile_definitions(${target} PRIVATE
      SERIALIZER_BENCHMARK_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
      SERIALIZER_BENCHMARK_CONFIGURATION="$<CONFIG>"
    )
    # Keep the observation function opaque to the optimizer during codec timing.
    set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
    if(MSVC)
      target_compile_options(${target} PRIVATE /GL- /W4 /WX)
    else()
      target_compile_options(${target} PRIVATE -fno-lto -Wall -Wextra -Wpedantic -Werror)
      target_link_options(${target} PRIVATE -fno-lto)
    endif()
  endforeach()
  target_sources(codec_allocation_profile PRIVATE allocation_metrics.cpp)
  target_compile_definitions(codec_allocation_profile PRIVATE SERIALIZER_ALLOCATION_PROFILE)
endif()

if(SERIALIZER_BUILD_FUZZERS)
  if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR MSVC)
    message(FATAL_ERROR "The fuzz target requires Clang with libFuzzer and address/undefined sanitizers")
  endif()
  add_executable(codec_fuzz codec_fuzz.cpp)
  add_dependencies(codec_fuzz serializer_qualification_headers)
  target_link_libraries(codec_fuzz PRIVATE serializer_lib)
  target_include_directories(codec_fuzz PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
  target_compile_options(codec_fuzz PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
  target_link_options(codec_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
endif()
