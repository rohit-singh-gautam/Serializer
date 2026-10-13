# Verify real Ninja depfile integration and policy forwarding without a C++ compiler.
file(MAKE_DIRECTORY "${DIRECTORY}/source/shared")
file(WRITE "${DIRECTORY}/source/shared/common.serializer"
  "serializer version 1; class account { public uint32 id (7); }\n")
file(WRITE "${DIRECTORY}/source/root.serializer"
  "serializer /* language */ version 1; include/* relative */\nshared/common/* once */;\n"
  "include./shared/./common.serializer;\n"
  "class request { public account owner; }\n")
file(WRITE "${DIRECTORY}/source/output.ini" "[cpp]\nformat = false\n")
file(WRITE "${DIRECTORY}/source/policy.serializer"
  "serializer version 1; class record { public version { 10 } releases { 8 { \"2024-10-01\" }; 9 { \"2025-04-01\" }; 10 { \"2026-10-01\" }; } policy { max_age { 2 years }; }; }\n")
set(source [=[
cmake_minimum_required(VERSION 3.28)
project(include_dependencies LANGUAGES NONE)
add_library(Serializer::runtime INTERFACE IMPORTED)
include("@REPOSITORY@/cmake/serializer_generate.cmake")
include("@REPOSITORY@/cmake/serializer_generate_java.cmake")
include("@REPOSITORY@/cmake/serializer_generate_source.cmake")
add_library(consumer INTERFACE)
serializer_generate(TARGET consumer SCHEMAS root.serializer policy.serializer CONFIG output.ini
  VERSION_POLICY_AS_OF 2026-10-06 VERSION_POLICY_WARNINGS_AS_ERRORS
  GENERATOR "@GENERATOR@" OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated space #hash $dollar")
add_library(inline_consumer INTERFACE)
serializer_generate(TARGET inline_consumer SCHEMAS root.serializer CONFIG output.ini
  INCLUDE_MODE inline GENERATOR "@GENERATOR@"
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated_inline")
serializer_generate_java(TARGET java_sources SCHEMA root.serializer
  GENERATOR "@GENERATOR@" OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/java space #hash $dollar/Schema.java")
serializer_generate_java(TARGET policy_java SCHEMA policy.serializer
  VERSION_POLICY_AS_OF 2026-10-06 VERSION_POLICY_WARNINGS_AS_ERRORS
  GENERATOR "@GENERATOR@" OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/policy/Schema.java")
serializer_generate_source(TARGET policy_python SCHEMA policy.serializer LANGUAGE python
  VERSION_POLICY_AS_OF 2026-10-06 VERSION_POLICY_WARNINGS_AS_ERRORS
  GENERATOR "@GENERATOR@" OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/policy/schema.py")
serializer_generate_source(TARGET cpp_source SCHEMA root.serializer LANGUAGE cpp CONFIG output.ini
  GENERATOR "@GENERATOR@" OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated_source.hpp")
serializer_generate_source(TARGET inline_cpp_source SCHEMA root.serializer LANGUAGE cpp CONFIG output.ini
  INCLUDE_MODE inline GENERATOR "@GENERATOR@" OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/inline_source.hpp")
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
run("${CMAKE_COMMAND}" --build "${DIRECTORY}/build" --target serializer_generated_headers
  serializer_generated_java policy_python cpp_source inline_cpp_source)
file(READ "${DIRECTORY}/build/generated space #hash $dollar/policy.hpp" policy_cpp_source)
file(READ "${DIRECTORY}/build/policy/Schema.java" policy_java_source)
file(READ "${DIRECTORY}/build/policy/schema.py" policy_python_source)
if(NOT policy_cpp_source MATCHES "serializer_minimum_version = static_cast<std::uint8_t>\\(9ULL\\)" OR
    NOT policy_java_source MATCHES "parseUnsignedLong\\(\"9\"\\)" OR
    NOT policy_python_source MATCHES "result.version < 9")
  message(FATAL_ERROR "CMake helpers did not forward release-policy options")
endif()
set(cpp "${DIRECTORY}/build/generated space #hash $dollar/root.hpp")
set(common "${cpp}.includes/shared/common.hpp")
set(leaf "${cpp}.includes/shared/leaf.hpp")
set(source_leaf "${DIRECTORY}/build/generated_source.hpp.includes/shared/leaf.hpp")
set(java "${DIRECTORY}/build/java space #hash $dollar/Schema.java")
file(READ "${cpp}" cpp_source)
file(READ "${DIRECTORY}/build/generated_inline/root.hpp" inline_source)
file(READ "${DIRECTORY}/build/inline_source.hpp" inline_unit_source)
if(cpp_source MATCHES "class account" OR NOT EXISTS "${common}" OR
    NOT inline_source MATCHES "class account" OR NOT inline_unit_source MATCHES "class account")
  message(FATAL_ERROR "CMake helpers did not preserve separate defaults and explicit inline mode")
endif()
file(SHA256 "${common}" cpp_before)
file(SHA256 "${java}" java_before)

# Changing a dependency adds a new transitive dependency; the next build must discover it.
file(WRITE "${DIRECTORY}/source/shared/leaf.serializer"
  "serializer version 1; enum state { pending, active }\n")
file(WRITE "${DIRECTORY}/source/shared/common.serializer"
  "serializer version 1; include leaf; class account { public uint32 id (7); public state status (8); }\n")
run("${CMAKE_COMMAND}" --build "${DIRECTORY}/build" --target serializer_generated_headers
  serializer_generated_java cpp_source inline_cpp_source)
file(SHA256 "${common}" cpp_after)
file(SHA256 "${java}" java_after)
if(cpp_before STREQUAL cpp_after OR java_before STREQUAL java_after)
  message(FATAL_ERROR "Editing a direct include did not regenerate both languages")
endif()
file(WRITE "${DIRECTORY}/source/shared/leaf.serializer"
  "serializer version 1; enum state { pending, active, suspended }\n")
run("${CMAKE_COMMAND}" --build "${DIRECTORY}/build" --target serializer_generated_headers
  serializer_generated_java cpp_source inline_cpp_source)
file(READ "${leaf}" cpp_source)
file(READ "${java}" java_source)
if(NOT cpp_source MATCHES "suspended" OR NOT java_source MATCHES "SUSPENDED")
  message(FATAL_ERROR "Editing a newly discovered transitive include did not regenerate both languages")
endif()

# Static child byproducts must recover a deleted transitive header with the entry still present.
file(SHA256 "${leaf}" leaf_before)
file(REMOVE "${leaf}" "${source_leaf}")
run("${CMAKE_COMMAND}" --build "${DIRECTORY}/build" --target serializer_generated_headers cpp_source)
if(NOT EXISTS "${leaf}" OR NOT EXISTS "${source_leaf}")
  message(FATAL_ERROR "Deleting an included header did not rerun its producer")
endif()
file(SHA256 "${leaf}" leaf_after)
if(NOT leaf_before STREQUAL leaf_after)
  message(FATAL_ERROR "Recreating a missing included header changed its generated contents")
endif()

# An unchanged graph must remain up to date instead of regenerating on every invocation.
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${DIRECTORY}/build"
  --target serializer_generated_headers serializer_generated_java cpp_source inline_cpp_source
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT output MATCHES "no work to do")
  message(FATAL_ERROR "Unchanged includes triggered a rebuild: ${output}\n${error}")
endif()

# An INI layout change must update CMake's artifact declarations during automatic reconfiguration.
file(WRITE "${DIRECTORY}/source/output.ini"
  "[output]\ninclude_mode = inline\n[cpp]\nformat = false\n")
run("${CMAKE_COMMAND}" --build "${DIRECTORY}/build" --target serializer_generated_headers cpp_source)
file(READ "${cpp}" configured_source)
file(READ "${DIRECTORY}/build/generated_source.hpp" configured_unit_source)
if(NOT configured_source MATCHES "class account" OR NOT configured_source MATCHES "suspended" OR
    NOT configured_unit_source MATCHES "class account")
  message(FATAL_ERROR "Changing output.include_mode did not reconfigure the CMake helper layouts")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${DIRECTORY}/build"
  --target serializer_generated_headers cpp_source
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT output MATCHES "no work to do")
  message(FATAL_ERROR "Inline-configured helpers kept expecting separate byproducts: ${output}\n${error}")
endif()
