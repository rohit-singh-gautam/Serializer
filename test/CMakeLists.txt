set(schemas)
foreach(fixture IN ITEMS enum person test1 array map variable assessment views storage_reuse
    fixed_fields constant_names includes wire_compatibility generics dimensions)
  list(APPEND schemas "${CMAKE_CURRENT_SOURCE_DIR}/resources/${fixture}.serializer")
endforeach()

add_executable(core_serializer_test
  core_serializer_test.cpp
  command_line_test.cpp
  stream_boundary_test.cpp
  string_stream_test.cpp
  deserialize_exact_test.cpp
  compression_test.cpp
  stream_concepts_test.cpp
  file_stream_test.cpp
  serializer_parser_test.cpp
  schema_version_test.cpp
  schema_generics_test.cpp
  dimension_templates_test.cpp
  managed_schema_test.cpp
  managed_lowering_test.cpp
  schema_compatibility_test.cpp
  schema_include_test.cpp
  schema_scan_test.cpp
  runtime_simd_test.cpp
  utf8_validation_test.cpp
  json_scan_reuse_test.cpp
  json_compatibility_test.cpp
  json_whitespace_scan_test.cpp
  binary_array_decode_test.cpp
  nested_storage_reuse_test.cpp
  fixed_field_batch_test.cpp
  constant_field_name_test.cpp
  generated_test.cpp
  wire_format_test.cpp
  binary_text_test.cpp
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
foreach(profile IN ITEMS serializer core google llvm gnu cert misra autosar qt)
  set(matrix_type matrix)
  set(point_type point)
  set(frame_type frame)
  set(message_type message)
  set(elements_field elements)
  set(response_field response)
  set(result_field result)
  set(value_field value)
  if(profile MATCHES "^(google|llvm|qt)$")
    set(matrix_type Matrix)
    set(point_type Point)
    set(frame_type Frame)
    set(message_type Message)
  endif()
  if(profile STREQUAL "google")
    set(elements_field elements_)
    set(response_field response_)
    set(result_field result_)
    set(value_field value_)
  elseif(profile STREQUAL "llvm")
    set(elements_field Elements)
    set(response_field Response)
    set(result_field Result)
    set(value_field Value)
  endif()
  set(profile_directory "${CMAKE_CURRENT_BINARY_DIR}/dimension_profile_${profile}")
  configure_file(dimension_profile_test.cpp.in "${profile_directory}/dimension_profile_test.cpp" @ONLY)
  add_executable(dimension_profile_${profile}_test "${profile_directory}/dimension_profile_test.cpp")
  set_target_properties(dimension_profile_${profile}_test PROPERTIES CXX_STANDARD 23)
  serializer_generate(TARGET dimension_profile_${profile}_test
    SCHEMAS resources/generics.serializer resources/dimensions.serializer
    CODING_STANDARD ${profile} OUTPUT_DIRECTORY "${profile_directory}")
  add_test(NAME dimension_profile_${profile}_test COMMAND dimension_profile_${profile}_test)
endforeach()
find_package(Python3 3.10 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME serializer_runtime_literals
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/runtime_literal_test.py")
  add_test(NAME serializer_generic_python
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/example/generics/run.py"
      --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/generic_python")
endif()
if(SERIALIZER_BUILD_MANAGED)
  foreach(profile IN ITEMS managed32 managed64 separate64)
    add_executable(dimension_history_${profile}_test dimension_history_test.cpp)
    set_target_properties(dimension_history_${profile}_test PROPERTIES CXX_STANDARD 23)
    serializer_generate(TARGET dimension_history_${profile}_test
      SCHEMAS resources/dimensions_managed.serializer
      CONFIG resources/dimensions_${profile}.ini
      OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dimensions_${profile}")
    target_link_libraries(dimension_history_${profile}_test PRIVATE Serializer::managed)
    if(Python3_Interpreter_FOUND)
      add_test(NAME dimension_history_${profile}_test
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/dimension_history_test.py"
          --helper "$<TARGET_FILE:dimension_history_${profile}_test>")
    endif()
  endforeach()
  add_executable(managed_collaboration_test managed_collaboration_test.cpp
    managed_collaboration_ownership_test.cpp managed_collaboration_proposal_test.cpp
    managed_collaboration_session_test.cpp managed_collaboration_history_test.cpp
    managed_local_collaboration_test.cpp managed_feature_test.cpp)
  serializer_generate(TARGET managed_collaboration_test
    SCHEMAS resources/collaboration_commands.serializer ../example/managed/ledger/ledger.serializer
    resources/collaboration_model.serializer
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/collaboration")
  target_link_libraries(managed_collaboration_test PRIVATE Serializer::managed GTest::gtest_main)
  if(MSVC)
    target_compile_options(managed_collaboration_test PRIVATE /W4 /WX)
  else()
    target_compile_options(managed_collaboration_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME managed_collaboration_test COMMAND managed_collaboration_test)
  add_executable(managed_store_test managed_store_test.cpp managed_generated_test.cpp)
  serializer_generate(TARGET managed_store_test SCHEMAS resources/managed_model.serializer resources/managed_generated.serializer
    CONFIG resources/managed_separate.ini
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/managed")
  target_link_libraries(managed_store_test PRIVATE Serializer::managed GTest::gtest_main)
  if(MSVC)
    target_compile_options(managed_store_test PRIVATE /W4 /WX)
  else()
    target_compile_options(managed_store_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME managed_store_test COMMAND managed_store_test)
  add_executable(managed_profile_test managed_profile_test.cpp)
  serializer_generate(TARGET managed_profile_test SCHEMAS resources/managed_generated.serializer
    CONFIG resources/managed_google64.ini CODING_STANDARD google
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/managed_google64")
  target_link_libraries(managed_profile_test PRIVATE Serializer::managed GTest::gtest_main)
  if(MSVC)
    target_compile_options(managed_profile_test PRIVATE /W4 /WX)
  else()
    target_compile_options(managed_profile_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME managed_profile_test COMMAND managed_profile_test)
  add_executable(managed_direct_test managed_direct_test.cpp managed_journal_test.cpp)
  serializer_generate(TARGET managed_direct_test
    SCHEMAS ../example/managed/ledger/ledger.serializer resources/managed_generated.serializer
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/managed_direct")
  target_link_libraries(managed_direct_test PRIVATE Serializer::managed GTest::gtest_main)
  if(MSVC)
    target_compile_options(managed_direct_test PRIVATE /W4 /WX)
  else()
    target_compile_options(managed_direct_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME managed_direct_test COMMAND managed_direct_test)
  add_executable(managed_journal_crash_helper managed_journal_crash_helper.cpp)
  serializer_generate(TARGET managed_journal_crash_helper
    SCHEMAS ../example/managed/ledger/ledger.serializer
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/managed_journal_crash")
  target_link_libraries(managed_journal_crash_helper PRIVATE Serializer::managed)
  if(MSVC)
    target_compile_options(managed_journal_crash_helper PRIVATE /W4 /WX)
  else()
    target_compile_options(managed_journal_crash_helper PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME managed_journal_crash_test
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/managed_journal_crash_test.py"
        --helper "$<TARGET_FILE:managed_journal_crash_helper>"
        --directory "${CMAKE_CURRENT_BINARY_DIR}/journal_crash")
    set_tests_properties(managed_journal_crash_test PROPERTIES TIMEOUT 180)
  endif()
  add_executable(managed_history_allocation_failure_test managed_history_allocation_failure_test.cpp)
  target_link_libraries(managed_history_allocation_failure_test PRIVATE Serializer::managed)
  if(MSVC)
    # Only header-local history containers are exercised; no STL values cross the runtime ABI.
    # Debug iterator proxies may allocate inside noexcept moves, so inject with release iterators.
    target_compile_definitions(managed_history_allocation_failure_test PRIVATE _ITERATOR_DEBUG_LEVEL=0)
    target_compile_options(managed_history_allocation_failure_test PRIVATE /W4 /WX)
  else()
    target_compile_options(managed_history_allocation_failure_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME managed_history_allocation_failure_test COMMAND managed_history_allocation_failure_test)
  set_tests_properties(managed_history_allocation_failure_test PROPERTIES TIMEOUT 60)
endif()
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
add_executable(protobuf_allocation_test protobuf_allocation_test.cpp)
target_link_libraries(protobuf_allocation_test PRIVATE Serializer::serializer_lib)
add_test(NAME protobuf_allocation_test COMMAND protobuf_allocation_test)
if(MSVC)
  target_compile_options(protobuf_codec_test PRIVATE /W4 /WX)
  target_compile_options(protobuf_allocation_test PRIVATE /W4 /WX)
else()
  target_compile_options(protobuf_codec_test PRIVATE -Wall -Wextra -Werror -Wpedantic)
  target_compile_options(protobuf_allocation_test PRIVATE -Wall -Wextra -Werror -Wpedantic)
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

add_test(NAME serializer_include_cli
  COMMAND "${CMAKE_COMMAND}" "-DGENERATOR=$<TARGET_FILE:serializer>"
    "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/include_cli"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/schema_include_cli.cmake")

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

if(SERIALIZER_BUILD_INTEROP_EXAMPLES OR SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES)
  include(char_map_tests.cmake)
endif()

add_test(NAME serializer_portable_cli
  COMMAND "${CMAKE_COMMAND}" "-DGENERATOR=$<TARGET_FILE:serializer>"
    "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/portable_cli"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/portable_cli.cmake")
