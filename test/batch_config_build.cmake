# Exercise the shared generation rule through real Ninja builds without a C++ compiler.
file(MAKE_DIRECTORY "${DIRECTORY}/source/shared")
set(source_directory "${DIRECTORY}/source")
set(build_directory "${DIRECTORY}/build")
set(leaf "${source_directory}/shared/leaf.serializer")
file(WRITE "${source_directory}/root.serializer" "serializer version 1; include shared/common;\n"
  "class ledger { public managed map(uint64) entry entries (1); }\n")
file(WRITE "${source_directory}/shared/common.serializer" "serializer version 1; include leaf;\n")
file(WRITE "${leaf}" "serializer version 1; class entry stable_ids managed { public uint32 value (1); }\n")
foreach(profile IN ITEMS managed32 managed64 separate64)
  configure_file("${CMAKE_CURRENT_LIST_DIR}/resources/dimensions_${profile}.ini"
    "${source_directory}/${profile}.ini" COPYONLY)
  file(APPEND "${source_directory}/${profile}.ini" "\n[cpp]\nformat = false\n")
endforeach()
set(source [=[
cmake_minimum_required(VERSION 3.28)
project(batch_config_dependencies LANGUAGES NONE)
add_library(Serializer::serializer_lib INTERFACE IMPORTED)
include("@REPOSITORY@/cmake/serializer_generate.cmake")
foreach(profile IN ITEMS managed32 managed64 separate64)
  add_library(${profile} INTERFACE)
endforeach()
# Caller variables must not contaminate the helper's per-target header lists.
set(headers_0 unrelated_header.hpp)
serializer_generate_variants(TARGETS managed32 managed64 separate64 SCHEMAS root.serializer
  CONFIGS managed32.ini managed64.ini separate64.ini GENERATOR "@GENERATOR@"
  OUTPUT_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}/generated/managed32"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/managed64"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/separate64")
foreach(profile IN ITEMS managed32 managed64 separate64)
  get_target_property(headers ${profile} SERIALIZER_GENERATED_HEADERS)
  get_target_property(headers_target ${profile} SERIALIZER_HEADERS_TARGET)
  get_target_property(include_directories ${profile} INTERFACE_INCLUDE_DIRECTORIES)
  set(expected_directory "${CMAKE_CURRENT_BINARY_DIR}/generated/${profile}")
  if(NOT headers STREQUAL "${expected_directory}/root.hpp" OR
      NOT headers_target STREQUAL "${profile}_serializer_headers" OR
      NOT TARGET "${headers_target}" OR
      NOT include_directories STREQUAL "$<BUILD_INTERFACE:${expected_directory}>")
    message(FATAL_ERROR "Missing variant target properties for ${profile}")
  endif()
endforeach()
]=])
string(CONFIGURE "${source}" source @ONLY)
file(WRITE "${source_directory}/CMakeLists.txt" "${source}")

# Run a build command and retain its output so invocation counts are observable.
function(run)
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Batch build failed: ${ARGN}\n${result}\n${output}\n${error}")
  endif()
  set(last_output "${output}" PARENT_SCOPE)
endfunction()

# Each schema regeneration invokes the compiler once and publishes all three headers.
function(build_batch target)
  run("${CMAKE_COMMAND}" --build "${build_directory}" --target "${target}" --verbose)
  string(REGEX MATCHALL "--input" invocations "${last_output}")
  list(LENGTH invocations invocation_count)
  string(REGEX MATCHALL "Generated " generations "${last_output}")
  list(LENGTH generations generation_count)
  if(NOT invocation_count EQUAL 1 OR NOT generation_count EQUAL 3)
    message(FATAL_ERROR "Expected one compiler invocation and three outputs: ${last_output}")
  endif()
endfunction()

run("${CMAKE_COMMAND}" -S "${source_directory}" -B "${build_directory}" -G Ninja
  "-DCMAKE_MAKE_PROGRAM=${NINJA}")
build_batch(managed32_serializer_headers)
foreach(profile IN ITEMS managed32 managed64 separate64)
  set(header "${build_directory}/generated/${profile}/root.hpp")
  if(NOT EXISTS "${header}")
    message(FATAL_ERROR "Building one variant did not generate ${profile}")
  endif()
  file(SHA256 "${header}" "before_${profile}")
endforeach()
if(before_managed32 STREQUAL before_managed64 OR before_managed64 STREQUAL before_separate64 OR
    before_managed32 STREQUAL before_separate64)
  message(FATAL_ERROR "Variant helper lost independent managed settings")
endif()

# Every per-target entry point and the aggregate target share the same completed rule.
foreach(target IN ITEMS managed32_serializer_headers managed64_serializer_headers
    separate64_serializer_headers serializer_generated_headers)
  run("${CMAKE_COMMAND}" --build "${build_directory}" --target "${target}" --verbose)
  if(NOT last_output MATCHES "no work to do" OR last_output MATCHES "--input")
    message(FATAL_ERROR "Up-to-date variant target triggered regeneration: ${last_output}")
  endif()
endforeach()

# A transitive schema dependency rebuilds the whole batch through any variant target.
file(WRITE "${leaf}" "serializer version 1; class entry stable_ids managed {\n"
  "  public uint32 value (1);\n  public uint32 added (2);\n}\n")
build_batch(managed64_serializer_headers)
foreach(profile IN ITEMS managed32 managed64 separate64)
  file(SHA256 "${build_directory}/generated/${profile}/root.hpp" "after_${profile}")
  if("${before_${profile}}" STREQUAL "${after_${profile}}")
    message(FATAL_ERROR "Editing a transitive include did not regenerate ${profile}")
  endif()
endforeach()

# Editing one INI regenerates once while unchanged profiles retain identical output.
file(WRITE "${source_directory}/managed32.ini"
  "[managed]\nid_type = uint64\nseparate_values = false\n[cpp]\nformat = false\n")
build_batch(separate64_serializer_headers)
foreach(profile IN ITEMS managed32 managed64 separate64)
  file(SHA256 "${build_directory}/generated/${profile}/root.hpp" "configured_${profile}")
endforeach()
if(configured_managed32 STREQUAL after_managed32 OR
    NOT configured_managed32 STREQUAL configured_managed64 OR
    NOT configured_managed64 STREQUAL after_managed64 OR
    NOT configured_separate64 STREQUAL after_separate64)
  message(FATAL_ERROR "A changed configuration altered unrelated variant output")
endif()

# Removing one output reruns its shared producer and recreates every expected header.
set(missing_header "${build_directory}/generated/managed64/root.hpp")
file(REMOVE "${missing_header}")
build_batch(serializer_generated_headers)
foreach(profile IN ITEMS managed32 managed64 separate64)
  file(SHA256 "${build_directory}/generated/${profile}/root.hpp" restored)
  if(NOT restored STREQUAL "${configured_${profile}}")
    message(FATAL_ERROR "Rebuilding a missing output changed ${profile}")
  endif()
endforeach()
run("${CMAKE_COMMAND}" --build "${build_directory}" --target serializer_generated_headers --verbose)
if(NOT last_output MATCHES "no work to do")
  message(FATAL_ERROR "Restored outputs did not remain up to date: ${last_output}")
endif()

# Configure-time contract errors are isolated from the working fixture and its outputs.
function(reject_configuration name expected statement)
  set(rejection_source "${DIRECTORY}/reject_${name}")
  file(MAKE_DIRECTORY "${rejection_source}")
  set(rejection [=[
cmake_minimum_required(VERSION 3.28)
project(batch_config_rejection LANGUAGES NONE)
add_library(Serializer::serializer_lib INTERFACE IMPORTED)
include("@REPOSITORY@/cmake/serializer_generate.cmake")
add_library(first INTERFACE)
add_library(second INTERFACE)
@statement@
]=])
  string(CONFIGURE "${rejection}" rejection @ONLY)
  file(WRITE "${rejection_source}/CMakeLists.txt" "${rejection}")
  execute_process(COMMAND "${CMAKE_COMMAND}" -S "${rejection_source}"
    -B "${rejection_source}/build" -G Ninja "-DCMAKE_MAKE_PROGRAM=${NINJA}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(result EQUAL 0 OR NOT error MATCHES "${expected}")
    message(FATAL_ERROR "Expected configuration rejection '${expected}': ${name}\n${output}\n${error}")
  endif()
endfunction()

set(common_arguments "SCHEMAS \"${source_directory}/root.serializer\" GENERATOR \"${GENERATOR}\"")
set(first_config "${source_directory}/managed32.ini")
set(second_config "${source_directory}/managed64.ini")
reject_configuration(config_count "one CONFIGS entry per target"
  "serializer_generate_variants(TARGETS first second ${common_arguments} CONFIGS \"${first_config}\")")
reject_configuration(directory_count "one OUTPUT_DIRECTORIES entry per"
  "serializer_generate_variants(TARGETS first second ${common_arguments} CONFIGS \"${first_config}\" \"${second_config}\" OUTPUT_DIRECTORIES shared)")
reject_configuration(duplicate_targets "distinct TARGETS"
  "serializer_generate_variants(TARGETS first first ${common_arguments} CONFIGS \"${first_config}\" \"${second_config}\")")
reject_configuration(duplicate_directories "already generated"
  "serializer_generate_variants(TARGETS first second ${common_arguments} CONFIGS \"${first_config}\" \"${second_config}\" OUTPUT_DIRECTORIES shared shared/../shared)")
reject_configuration(registered_target "once per target"
  "serializer_generate_variants(TARGETS first ${common_arguments} CONFIGS \"${first_config}\")\nserializer_generate_variants(TARGETS first second ${common_arguments} CONFIGS \"${first_config}\" \"${second_config}\")")
