# Verify real Ninja depfile integration for both CMake generation helpers without a C++ compiler.
file(MAKE_DIRECTORY "${DIRECTORY}/source/shared")
file(WRITE "${DIRECTORY}/source/shared/common.serializer"
  "serializer version 1; class account { public uint32 id (7); }\n")
file(WRITE "${DIRECTORY}/source/root.serializer"
  "serializer version 1; include shared/common.serializer; class request { public account owner; }\n")
file(WRITE "${DIRECTORY}/source/output.ini" "[cpp]\nformat = false\n")
set(source [=[
cmake_minimum_required(VERSION 3.28)
project(include_dependencies LANGUAGES NONE)
add_library(Serializer::serializer_lib INTERFACE IMPORTED)
include("@REPOSITORY@/cmake/serializer_generate.cmake")
include("@REPOSITORY@/cmake/serializer_generate_java.cmake")
add_library(consumer INTERFACE)
serializer_generate(TARGET consumer SCHEMAS root.serializer CONFIG output.ini
  GENERATOR "@GENERATOR@" OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated space #hash $dollar")
serializer_generate_java(TARGET java_sources SCHEMA root.serializer
  GENERATOR "@GENERATOR@" OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/java space #hash $dollar/Schema.java")
]=])
string(CONFIGURE "${source}" source @ONLY)
file(WRITE "${DIRECTORY}/source/CMakeLists.txt" "${source}")

# Run CMake or Ninja with complete failure output; no source code is compiled by this fixture.
function(run)
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Dependency build failed: ${ARGN}\n${output}\n${error}")
  endif()
endfunction()

run("${CMAKE_COMMAND}" -S "${DIRECTORY}/source" -B "${DIRECTORY}/build" -G Ninja
  "-DCMAKE_MAKE_PROGRAM=${NINJA}")
run("${CMAKE_COMMAND}" --build "${DIRECTORY}/build" --target serializer_generated_headers serializer_generated_java)
set(cpp "${DIRECTORY}/build/generated space #hash $dollar/root.hpp")
set(java "${DIRECTORY}/build/java space #hash $dollar/Schema.java")
file(SHA256 "${cpp}" cpp_before)
file(SHA256 "${java}" java_before)

# Changing a dependency adds a new transitive dependency; the next build must discover it.
file(WRITE "${DIRECTORY}/source/shared/leaf.serializer"
  "serializer version 1; enum state { pending, active }\n")
file(WRITE "${DIRECTORY}/source/shared/common.serializer"
  "serializer version 1; include leaf.serializer; class account { public uint32 id (7); public state status (8); }\n")
run("${CMAKE_COMMAND}" --build "${DIRECTORY}/build" --target serializer_generated_headers serializer_generated_java)
file(SHA256 "${cpp}" cpp_after)
file(SHA256 "${java}" java_after)
if(cpp_before STREQUAL cpp_after OR java_before STREQUAL java_after)
  message(FATAL_ERROR "Editing a direct include did not regenerate both languages")
endif()
file(WRITE "${DIRECTORY}/source/shared/leaf.serializer"
  "serializer version 1; enum state { pending, active, suspended }\n")
run("${CMAKE_COMMAND}" --build "${DIRECTORY}/build" --target serializer_generated_headers serializer_generated_java)
file(READ "${cpp}" cpp_source)
file(READ "${java}" java_source)
if(NOT cpp_source MATCHES "suspended" OR NOT java_source MATCHES "SUSPENDED")
  message(FATAL_ERROR "Editing a newly discovered transitive include did not regenerate both languages")
endif()

# An unchanged graph must remain up to date instead of regenerating on every invocation.
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${DIRECTORY}/build"
  --target serializer_generated_headers serializer_generated_java
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT output MATCHES "no work to do")
  message(FATAL_ERROR "Unchanged includes triggered a rebuild: ${output}\n${error}")
endif()
