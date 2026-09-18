set(schemas)
foreach(fixture IN ITEMS enum person test1 array map variable assessment views storage_reuse
    fixed_fields constant_names includes wire_compatibility)
  list(APPEND schemas "${CMAKE_CURRENT_SOURCE_DIR}/resources/${fixture}.serializer")
endforeach()

add_executable(core_serializer_test
  core_serializer_test.cpp
  stream_boundary_test.cpp
  deserialize_exact_test.cpp
  compression_test.cpp
  stream_concepts_test.cpp
  serializer_parser_test.cpp
  schema_version_test.cpp
  schema_compatibility_test.cpp
  schema_include_test.cpp
  schema_scan_test.cpp
  runtime_simd_test.cpp
  json_scan_reuse_test.cpp
  json_whitespace_scan_test.cpp
  binary_array_decode_test.cpp
  nested_storage_reuse_test.cpp
  fixed_field_batch_test.cpp
  constant_field_name_test.cpp
  generated_test.cpp
  wire_format_test.cpp
  wire_compatibility_test.cpp
  decoder_qualification_test.cpp
  binary_view_test.cpp
  binary_view_iterator_test.cpp
  output_options_test.cpp
  java_writer_test.cpp
  portable_writer_test.cpp
)
serializer_generate(TARGET core_serializer_test
  SCHEMAS ${schemas}
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(core_serializer_test PRIVATE GTest::gtest_main)
target_compile_definitions(core_serializer_test PRIVATE ROHIT_SERIALIZER_ENABLE_GTEST)
if(MSVC)
  target_compile_options(core_serializer_test PRIVATE /W4 /WX)
elseif(UNIX)
  target_compile_options(core_serializer_test PRIVATE -Wall -Werror -Wpedantic -Wextra -Weffc++ -Wsuggest-override)
endif()

add_test(NAME core_serializer_test COMMAND core_serializer_test)
add_test(NAME serializer_native_cli
  COMMAND "${CMAKE_COMMAND}" "-DGENERATOR=$<TARGET_FILE:serializer>"
    "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/native_cli"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/native_cli.cmake")
add_executable(compression_allocation_failure_test compression_allocation_failure_test.cpp)
if(MSVC)
  # MSVC debug vector constructors allocate iterator proxies inside noexcept functions.
  # Test recoverable storage failures with release iterator semantics in BOTH translation units;
  # the normal runtime and core tests keep the caller's existing iterator/ABI configuration.
  target_sources(compression_allocation_failure_test PRIVATE ../src/compression.cpp)
  target_include_directories(compression_allocation_failure_test PRIVATE
    "$<TARGET_PROPERTY:serializer_lib,INTERFACE_INCLUDE_DIRECTORIES>")
  target_compile_features(compression_allocation_failure_test PRIVATE cxx_std_20)
  target_compile_definitions(compression_allocation_failure_test PRIVATE _ITERATOR_DEBUG_LEVEL=0
    "$<TARGET_PROPERTY:serializer_lib,COMPILE_DEFINITIONS>")
  target_link_libraries(compression_allocation_failure_test PRIVATE ${serializer_compression_libraries})
else()
  target_link_libraries(compression_allocation_failure_test PRIVATE Serializer::serializer_lib)
endif()
add_test(NAME compression_allocation_failure_test COMMAND compression_allocation_failure_test)
set_tests_properties(compression_allocation_failure_test PROPERTIES TIMEOUT 60)

# Standalone interoperability is optional; frozen independent fixtures need no external tools.
if(SERIALIZER_WITH_ZSTD OR SERIALIZER_WITH_LZ4 OR SERIALIZER_WITH_ZLIB)
  add_executable(compression_interop compression_interop.cpp)
  target_link_libraries(compression_interop PRIVATE Serializer::serializer_lib)
  find_package(Python3 3.9 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    foreach(format IN ITEMS zstd lz4 gzip zlib deflate)
      set(compression_tool_arguments)
      if(format STREQUAL "zstd" AND SERIALIZER_WITH_ZSTD)
        find_program(serializer_zstd_program NAMES zstd)
        if(NOT serializer_zstd_program)
          continue()
        endif()
        list(APPEND compression_tool_arguments --tool "${serializer_zstd_program}")
      elseif(format STREQUAL "lz4" AND SERIALIZER_WITH_LZ4)
        find_program(serializer_lz4_program NAMES lz4)
        if(NOT serializer_lz4_program)
          continue()
        endif()
        list(APPEND compression_tool_arguments --tool "${serializer_lz4_program}")
      elseif(NOT (SERIALIZER_WITH_ZLIB AND format MATCHES "^(gzip|zlib|deflate)$"))
        continue()
      endif()
      add_test(NAME compression_interop_${format}
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/compression_interop_test.py"
          --helper "$<TARGET_FILE:compression_interop>" --format "${format}" ${compression_tool_arguments})
      set_tests_properties(compression_interop_${format} PROPERTIES LABELS serializer_compression)
    endforeach()
  endif()
endif()

# ELF linker wrapping injects allocator failures without modifying the production runtime.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
  add_executable(stream_allocation_failure_test stream_allocation_failure_test.cpp)
  target_link_libraries(stream_allocation_failure_test PRIVATE Serializer::serializer_lib)
  target_compile_options(stream_allocation_failure_test PRIVATE
    -Wall -Wextra -Wpedantic -Werror -fno-builtin-malloc -fno-builtin-realloc -fno-builtin-free)
  target_link_options(stream_allocation_failure_test PRIVATE
    "LINKER:--wrap=malloc" "LINKER:--wrap=realloc" "LINKER:--wrap=free")
  add_test(NAME stream_allocation_failure_test COMMAND stream_allocation_failure_test)
endif()

add_executable(protobuf_codec_test protobuf_codec_test.cpp protobuf_stream_test.cpp)
serializer_generate(TARGET protobuf_codec_test SCHEMAS resources/protobuf.serializer
  CONFIG protobuf.ini OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/protobuf")
target_link_libraries(protobuf_codec_test PRIVATE GTest::gtest_main)
add_test(NAME protobuf_codec_test COMMAND protobuf_codec_test)
if(MSVC)
  target_compile_options(protobuf_codec_test PRIVATE /W4 /WX)
else()
  target_compile_options(protobuf_codec_test PRIVATE -Wall -Wextra -Werror -Wpedantic)
endif()
if(SERIALIZER_BUILD_PROTOBUF_INTEROP_TESTS)
  find_package(protobuf CONFIG REQUIRED)
  set(protobuf_reference_directory "${CMAKE_CURRENT_BINARY_DIR}/protobuf_reference")
  file(MAKE_DIRECTORY "${protobuf_reference_directory}")
  add_custom_command(
    OUTPUT "${protobuf_reference_directory}/protobuf_reference.pb.cc"
      "${protobuf_reference_directory}/protobuf_reference.pb.h"
    COMMAND protobuf::protoc "--cpp_out=${protobuf_reference_directory}"
      "--proto_path=${CMAKE_CURRENT_SOURCE_DIR}/resources"
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/protobuf_reference.proto"
    DEPENDS resources/protobuf_reference.proto protobuf::protoc VERBATIM)
  target_sources(protobuf_codec_test PRIVATE protobuf_interop_test.cpp
    "${protobuf_reference_directory}/protobuf_reference.pb.cc")
  target_include_directories(protobuf_codec_test SYSTEM PRIVATE "${protobuf_reference_directory}")
  target_link_libraries(protobuf_codec_test PRIVATE protobuf::libprotobuf)
  if(WIN32)
    set_tests_properties(protobuf_codec_test PROPERTIES ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:protobuf::libprotobuf>")
  endif()
endif()
add_test(NAME serializer_compiler_cli
  COMMAND "${CMAKE_COMMAND}" "-DGENERATOR=$<TARGET_FILE:serializer>"
    "-DVERSION=${PROJECT_VERSION}" "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/compiler_cli"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/compiler_cli.cmake")

add_test(NAME serializer_compatibility_cli
  COMMAND "${CMAKE_COMMAND}" "-DGENERATOR=$<TARGET_FILE:serializer>"
    "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/compatibility_cli"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/schema_compatibility_cli.cmake")

find_program(serializer_test_ninja NAMES ninja HINTS "${CMAKE_MAKE_PROGRAM}/..")
if(CMAKE_GENERATOR MATCHES "Ninja")
  set(serializer_test_ninja "${CMAKE_MAKE_PROGRAM}")
endif()
if(serializer_test_ninja)
  add_test(NAME serializer_include_dependencies
    COMMAND "${CMAKE_COMMAND}" "-DGENERATOR=$<TARGET_FILE:serializer>"
      "-DREPOSITORY=${PROJECT_SOURCE_DIR}" "-DNINJA=${serializer_test_ninja}"
      "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/include_dependencies"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/schema_include_build.cmake")
endif()

add_test(NAME serializer_clang_format_discovery
  COMMAND "${CMAKE_COMMAND}" "-DFORMATTER=${SERIALIZER_CLANG_FORMAT_EXECUTABLE}"
    "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/clang_format_discovery"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/clang_format_discovery.cmake")

if(SERIALIZER_BUILD_JAVA_EXAMPLES)
  include(java_tests.cmake)
endif()

if(SERIALIZER_BUILD_INTEROP_EXAMPLES)
  include(portable_tests.cmake)
endif()

add_test(NAME serializer_portable_cli
  COMMAND "${CMAKE_COMMAND}" "-DGENERATOR=$<TARGET_FILE:serializer>"
    "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/portable_cli"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/portable_cli.cmake")
