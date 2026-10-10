# Verify the actual installed headers, exported targets, and backend dependencies.
foreach(required IN ITEMS BUILD_DIRECTORY DIRECTORY CONFIGURATION BUILD_MANAGED GENERATOR
    WITH_zstd WITH_lz4 WITH_zlib)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "install_package.cmake requires ${required}")
  endif()
endforeach()

# Stop on failed install/configure/build commands and retain their diagnostics.
function(run_checked)
  execute_process(COMMAND ${ARGV} RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE errors)
  if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Installed-package check failed (${status}):\n${output}\n${errors}")
  endif()
endfunction()

set(prefix "${DIRECTORY}/prefix-${BUILD_MANAGED}-${WITH_zstd}-${WITH_lz4}-${WITH_zlib}")
set(source "${DIRECTORY}/consumer")
set(build "${DIRECTORY}/build")
file(MAKE_DIRECTORY "${source}")
run_checked("${CMAKE_COMMAND}" --install "${BUILD_DIRECTORY}"
  --config "${CONFIGURATION}" --prefix "${prefix}")

foreach(license IN ITEMS LICENSE LICENSE-RUNTIME LICENSE-GENERATED)
  if(NOT EXISTS "${prefix}/share/licenses/Serializer/${license}")
    message(FATAL_ERROR "Missing installed licensing terms: ${license}")
  endif()
endforeach()

set(managed_headers managed.hpp managed_delta.hpp collaboration_history.hpp collaboration_sessions.hpp
  managed_collaboration.hpp managed_local_collaboration.hpp
  managed_records.hpp collaboration_records.hpp)
foreach(header IN LISTS managed_headers)
  if(BUILD_MANAGED AND NOT EXISTS "${prefix}/include/rohit/${header}")
    message(FATAL_ERROR "Missing installed managed header: ${header}")
  elseif(NOT BUILD_MANAGED AND EXISTS "${prefix}/include/rohit/${header}")
    message(FATAL_ERROR "Disabled managed API was installed: ${header}")
  endif()
endforeach()

file(WRITE "${source}/CMakeLists.txt" "cmake_minimum_required(VERSION 3.28)\n"
  "project(installed_serializer_headers LANGUAGES CXX)\n"
  "set(CMAKE_PREFIX_PATH [=[${prefix};${DEPENDENCY_PREFIXES}]=])\n"
  "find_package(Serializer CONFIG REQUIRED)\n")
file(APPEND "${source}/CMakeLists.txt" [=[
if(NOT TARGET Serializer::runtime OR NOT TARGET Serializer::serializer_lib)
  message(FATAL_ERROR "Missing runtime or compatibility compiler-library target")
endif()
# The selected package must precede older same-name headers from dependencies.
get_target_property(runtime_is_system Serializer::runtime SYSTEM)
if(runtime_is_system)
  message(FATAL_ERROR "Installed runtime headers must use ordinary include precedence")
endif()
get_target_property(runtime_dependencies Serializer::runtime INTERFACE_LINK_LIBRARIES)
if(runtime_dependencies MATCHES "serializer_lib")
  message(FATAL_ERROR "The permissive runtime links the GPL compiler library")
endif()
]=])
if(BUILD_MANAGED)
  file(APPEND "${source}/CMakeLists.txt"
    "if(NOT TARGET Serializer::managed)\n  message(FATAL_ERROR \"Missing managed target\")\nendif()\n")
  set(runtime_target Serializer::managed)
else()
  file(APPEND "${source}/CMakeLists.txt"
    "if(TARGET Serializer::managed)\n  message(FATAL_ERROR \"Unexpected managed target\")\nendif()\n")
  set(runtime_target Serializer::runtime)
endif()

# Each translation unit must compile without another header hiding its dependencies.
file(GLOB headers RELATIVE "${prefix}/include" "${prefix}/include/rohit/*.hpp")
foreach(header IN LISTS headers)
  get_filename_component(stem "${header}" NAME_WE)
  file(WRITE "${source}/${stem}_test.cpp" "#include <${header}>\n")
  file(APPEND "${source}/CMakeLists.txt"
    "add_library(${stem}_test OBJECT ${stem}_test.cpp)\n"
    "target_link_libraries(${stem}_test PRIVATE ${runtime_target})\n")
endforeach()

file(WRITE "${source}/capabilities_test.cpp" "#include <rohit/compression.hpp>\n"
  "#include <rohit/digest.hpp>\n"
  "// Check the linked installed runtime rather than compile-time option values.\n"
  "int main() {\n  namespace compression = rohit::serializer::compression;\n"
  "  const auto digest = rohit::make_digest<rohit::digest_algorithm::sha256>(std::string_view{\"abc\"});\n"
  "  if (rohit::digest_to_hex(digest) != \"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\") { return 1; }\n")
foreach(backend IN ITEMS zstd lz4 zlib)
  if(WITH_${backend})
    set(expected true)
  else()
    set(expected false)
  endif()
  file(APPEND "${source}/capabilities_test.cpp"
    "  if (compression::available(compression::format::${backend}) != ${expected}) { return 1; }\n")
endforeach()
file(APPEND "${source}/capabilities_test.cpp" "  return 0;\n}\n")
file(APPEND "${source}/CMakeLists.txt"
  "add_executable(capabilities_test capabilities_test.cpp)\n"
  "target_link_libraries(capabilities_test PRIVATE Serializer::runtime)\n"
  "enable_testing()\nadd_test(NAME capabilities COMMAND capabilities_test)\n"
  [=[foreach(dependency_prefix IN LISTS CMAKE_PREFIX_PATH)
  set_property(TEST capabilities APPEND PROPERTY ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:${dependency_prefix}/bin")
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set_property(TEST capabilities APPEND PROPERTY ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:${dependency_prefix}/debug/bin")
  endif()
endforeach()
]=])

set(configure_arguments -S "${source}" -B "${build}" -G "${GENERATOR}"
  "-DCMAKE_BUILD_TYPE=${CONFIGURATION}")
if(PLATFORM)
  list(APPEND configure_arguments -A "${PLATFORM}")
endif()
if(GENERATOR_INSTANCE)
  list(APPEND configure_arguments "-DCMAKE_GENERATOR_INSTANCE=${GENERATOR_INSTANCE}")
endif()
if(MAKE_PROGRAM)
  list(APPEND configure_arguments "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}")
endif()
run_checked("${CMAKE_COMMAND}" ${configure_arguments})
run_checked("${CMAKE_COMMAND}" --build "${build}" --config "${CONFIGURATION}" --parallel 2)
get_filename_component(cmake_directory "${CMAKE_COMMAND}" DIRECTORY)
run_checked("${cmake_directory}/ctest" --test-dir "${build}" -C "${CONFIGURATION}"
  --output-on-failure)
message(STATUS "Verified installed Serializer headers, targets, and compression capabilities")
