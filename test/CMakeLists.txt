set(schemas)
foreach(fixture IN ITEMS enum person test1 array map variable assessment views)
  list(APPEND schemas "${CMAKE_CURRENT_SOURCE_DIR}/resources/${fixture}.serializer")
endforeach()

add_executable(core_serializer_test
  core_serializer_test.cpp
  serializer_parser_test.cpp
  schema_version_test.cpp
  schema_scan_test.cpp
  runtime_simd_test.cpp
  binary_array_decode_test.cpp
  generated_test.cpp
  wire_format_test.cpp
  decoder_qualification_test.cpp
  binary_view_test.cpp
  output_options_test.cpp
  java_writer_test.cpp
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
add_test(NAME serializer_compiler_cli
  COMMAND "${CMAKE_COMMAND}" "-DGENERATOR=$<TARGET_FILE:serializer>"
    "-DVERSION=${PROJECT_VERSION}" "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/compiler_cli"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/compiler_cli.cmake")

if(SERIALIZER_BUILD_JAVA_EXAMPLES)
  include(java_tests.cmake)
endif()
