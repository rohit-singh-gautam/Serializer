set(generated_headers)
foreach(fixture IN ITEMS enum person test1 array map variable assessment views)
  set(input_file "${CMAKE_CURRENT_SOURCE_DIR}/resources/${fixture}.def")
  set(output_file "${CMAKE_CURRENT_BINARY_DIR}/${fixture}.hpp")
  add_custom_command(
    OUTPUT "${output_file}"
    COMMAND serializer input "${input_file}" output "${output_file}"
      cpp.clang_format "${SERIALIZER_CLANG_FORMAT_EXECUTABLE}"
    DEPENDS "${input_file}" serializer
    VERBATIM
  )
  list(APPEND generated_headers "${output_file}")
endforeach()

add_executable(core_serializer_test
  core_serializer_test.cpp
  serializer_parser_test.cpp
  generated_test.cpp
  wire_format_test.cpp
  decoder_qualification_test.cpp
  binary_view_test.cpp
  output_options_test.cpp
  ${generated_headers}
)
target_link_libraries(core_serializer_test PRIVATE serializer_lib GTest::gtest_main)
target_include_directories(core_serializer_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(core_serializer_test PRIVATE ROHIT_SERIALIZER_ENABLE_GTEST)
if(MSVC)
  target_compile_options(core_serializer_test PRIVATE /W4 /WX)
elseif(UNIX)
  target_compile_options(core_serializer_test PRIVATE -Wall -Werror -Wpedantic -Wextra -Weffc++ -Wsuggest-override)
endif()

add_test(NAME core_serializer_test COMMAND core_serializer_test)
