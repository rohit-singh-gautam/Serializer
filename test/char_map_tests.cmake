# Keep high-byte map interoperability independent of native char signedness and wire ordering.
find_package(Java 17 REQUIRED COMPONENTS Development Runtime)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
set(char_map_directory "${CMAKE_CURRENT_BINARY_DIR}/char_map")
serializer_generate_java(TARGET serializer_char_map_java_schema
  SCHEMA char_map/message.serializer OUTPUT "${char_map_directory}/Schema.java")
serializer_generate_source(TARGET serializer_char_map_python_schema
  SCHEMA char_map/message.serializer OUTPUT "${char_map_directory}/schema.py" LANGUAGE python)
add_library(serializer_char_map_model INTERFACE)
serializer_generate(TARGET serializer_char_map_model SCHEMAS char_map/message.serializer
  OUTPUT_DIRECTORY "${char_map_directory}")
foreach(char_mode IN ITEMS signed unsigned)
  set(target "serializer_char_map_${char_mode}")
  add_executable(${target} char_map/char_map_test.cpp)
  target_link_libraries(${target} PRIVATE serializer_char_map_model)
  if(char_mode STREQUAL "unsigned")
    target_compile_definitions(${target} PRIVATE SERIALIZER_TEST_CHAR_UNSIGNED=1)
  else()
    target_compile_definitions(${target} PRIVATE SERIALIZER_TEST_CHAR_UNSIGNED=0)
  endif()
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
    if(char_mode STREQUAL "unsigned")
      target_compile_options(${target} PRIVATE /J)
    endif()
    # MSVC has no /J-; the signed target asserts its default char mode explicitly.
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror "-f${char_mode}-char")
  endif()
endforeach()
add_custom_command(OUTPUT "${char_map_directory}/classes/Main.class"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${char_map_directory}/classes"
  COMMAND "${Java_JAVAC_EXECUTABLE}" --release 17 -encoding UTF-8 -Xlint:all -Werror
    -d "${char_map_directory}/classes" "${char_map_directory}/Schema.java"
    "${CMAKE_CURRENT_SOURCE_DIR}/char_map/Main.java"
  DEPENDS "${char_map_directory}/Schema.java" char_map/Main.java VERBATIM)
add_custom_target(serializer_char_map_consumers ALL
  DEPENDS "${char_map_directory}/classes/Main.class")
add_dependencies(serializer_char_map_consumers serializer_char_map_java_schema
  serializer_char_map_python_schema serializer_char_map_signed serializer_char_map_unsigned)
add_test(NAME serializer_char_map_interoperability
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/char_map/run.py"
    --cpp-signed "$<TARGET_FILE:serializer_char_map_signed>"
    --cpp-unsigned "$<TARGET_FILE:serializer_char_map_unsigned>"
    --java "${Java_JAVA_EXECUTABLE}" --classes "${char_map_directory}/classes"
    --schema "${char_map_directory}/schema.py" --fixtures "${char_map_directory}/fixtures")
set_tests_properties(serializer_char_map_interoperability PROPERTIES
  TIMEOUT 60 LABELS serializer_interop)
