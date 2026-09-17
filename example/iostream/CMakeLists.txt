# Generate the large shared model once; every executable retains its concrete stream types.
add_library(serializer_iostream_models INTERFACE)
serializer_generate(TARGET serializer_iostream_models SCHEMAS shared/telemetry_archive.serializer)

add_library(serializer_iostream_support STATIC shared/example_support.cpp)
target_include_directories(serializer_iostream_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/shared")
target_link_libraries(serializer_iostream_support PUBLIC serializer_iostream_models)

set(iostream_targets serializer_iostream_support)
add_custom_target(serializer_iostream_examples)
foreach(example IN ITEMS stringstream string_streams file_streams fstream buffered_file base_streams
    custom_streambuf)
  set(target "serializer_iostream_${example}")
  add_executable("${target}" "${example}/main.cpp")
  target_link_libraries("${target}" PRIVATE serializer_iostream_support)
  add_dependencies(serializer_iostream_examples "${target}")
  list(APPEND iostream_targets "${target}")
  add_test(NAME "${target}" COMMAND "${target}")
  set_tests_properties("${target}" PROPERTIES LABELS serializer_iostream TIMEOUT 60)
endforeach()

foreach(target IN LISTS iostream_targets)
  if(MSVC)
    target_compile_options("${target}" PRIVATE /W4 /WX)
  else()
    target_compile_options("${target}" PRIVATE -Wall -Wextra -Werror -Wpedantic)
  endif()
endforeach()
