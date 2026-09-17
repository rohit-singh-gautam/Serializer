# Share one generated schema and its build dependency across qualification consumers.
add_library(serializer_qualification_models INTERFACE)
serializer_generate(TARGET serializer_qualification_models
  SCHEMAS records.serializer
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

if(SERIALIZER_BUILD_BENCHMARKS)
  foreach(target IN ITEMS codec_benchmark codec_allocation_profile)
    add_executable(${target} codec_benchmark.cpp benchmark_sink.cpp)
    target_link_libraries(${target} PRIVATE serializer_qualification_models)
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
  add_library(serializer_fuzz_models INTERFACE)
  serializer_generate(TARGET serializer_fuzz_models SCHEMAS fuzz_models.serializer
    CONFIG fuzz.ini OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fuzz")
  # Replace the generation helper's production runtime link for these consumers only.
  # The generation executable remains an ordinary host tool.
  set_property(TARGET serializer_fuzz_models PROPERTY INTERFACE_LINK_LIBRARIES serializer_fuzz_runtime)

  set(serializer_fuzz_targets codec_fuzz view_fuzz protobuf_fuzz runtime_simd_fuzz)
  if(SERIALIZER_WITH_ZSTD OR SERIALIZER_WITH_LZ4 OR SERIALIZER_WITH_ZLIB)
    list(APPEND serializer_fuzz_targets compression_fuzz)
  endif()
  foreach(target IN LISTS serializer_fuzz_targets)
    add_executable(${target} ${target}.cpp)
    target_link_libraries(${target} PRIVATE serializer_fuzz_models)
    target_link_options(${target} PRIVATE -fsanitize=fuzzer)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endforeach()
  add_executable(fuzz_corpus_generator fuzz_corpus_generator.cpp)
  target_link_libraries(fuzz_corpus_generator PRIVATE serializer_fuzz_models)
  target_compile_options(fuzz_corpus_generator PRIVATE -Wall -Wextra -Wpedantic -Werror)

  set(serializer_fuzz_seed_directory "${CMAKE_CURRENT_BINARY_DIR}/corpus/seeds")
  add_custom_target(serializer_fuzz_corpus
    COMMAND fuzz_corpus_generator "${serializer_fuzz_seed_directory}"
    DEPENDS fuzz_corpus_generator VERBATIM)
  add_test(NAME serializer_fuzz_seed_corpus
    COMMAND fuzz_corpus_generator "${serializer_fuzz_seed_directory}")
  set_tests_properties(serializer_fuzz_seed_corpus PROPERTIES
    FIXTURES_SETUP serializer_fuzz_seeds LABELS serializer_fuzz TIMEOUT 60)
  foreach(target IN LISTS serializer_fuzz_targets)
    add_test(NAME ${target}_seeds COMMAND ${target} -runs=0 -seed=12345 -max_len=4099
      "${serializer_fuzz_seed_directory}/${target}")
    set_tests_properties(${target}_seeds PROPERTIES FIXTURES_REQUIRED serializer_fuzz_seeds
      LABELS serializer_fuzz TIMEOUT 60)
  endforeach()
endif()
