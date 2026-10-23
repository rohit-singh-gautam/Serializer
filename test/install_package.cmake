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

# Read the generated package metadata to request this installed release exactly.
set(PACKAGE_FIND_VERSION "0.0.0")
set(PACKAGE_FIND_VERSION_MAJOR 0)
include("${BUILD_DIRECTORY}/SerializerConfigVersion.cmake")
set(installed_release "${PACKAGE_VERSION}")
string(REGEX MATCH "^[0-9]+" installed_release_major "${installed_release}")

foreach(license IN ITEMS LICENSE LICENSE-RUNTIME LICENSE-GENERATED)
  if(NOT EXISTS "${prefix}/share/licenses/Serializer/${license}")
    message(FATAL_ERROR "Missing installed licensing terms: ${license}")
  endif()
endforeach()

set(managed_headers managed.hpp managed_identity_allocation.hpp managed_editable.hpp managed_registry.hpp managed_delta.hpp collaboration_history.hpp collaboration_sessions.hpp
  managed_collaboration.hpp managed_collaboration_hooks.hpp managed_local_collaboration.hpp managed_migration.hpp
  managed_records.hpp collaboration_records.hpp)
foreach(header IN LISTS managed_headers)
  if(BUILD_MANAGED AND NOT EXISTS "${prefix}/include/rohit/${header}")
    message(FATAL_ERROR "Missing installed managed header: ${header}")
  elseif(NOT BUILD_MANAGED AND EXISTS "${prefix}/include/rohit/${header}")
    message(FATAL_ERROR "Disabled managed API was installed: ${header}")
  endif()
endforeach()

file(WRITE "${source}/CMakeLists.txt" "cmake_minimum_required(VERSION 4.0)\n"
  "project(installed_serializer_headers LANGUAGES CXX)\n"
  "set(CMAKE_CXX_EXTENSIONS OFF)\n"
  "set(CMAKE_PREFIX_PATH [=[${prefix};${DEPENDENCY_PREFIXES}]=])\n"
  "find_package(Serializer ${installed_release} EXACT CONFIG REQUIRED)\n")
if(installed_release_major GREATER 1)
  foreach(previous_release IN ITEMS 1.22.1 1.23.0)
    file(APPEND "${source}/CMakeLists.txt"
      "find_package(Serializer ${previous_release} CONFIG QUIET NO_DEFAULT_PATH PATHS [=[${prefix}]=])\n"
      "if(Serializer_FOUND)\n"
      "  message(FATAL_ERROR \"Installed Serializer ${installed_release} accepted incompatible compiler release ${previous_release}\")\n"
      "endif()\n")
  endforeach()
  file(APPEND "${source}/CMakeLists.txt"
    "find_package(Serializer ${installed_release} EXACT CONFIG REQUIRED)\n")
endif()
file(APPEND "${source}/CMakeLists.txt" [=[
if(NOT TARGET Serializer::runtime OR NOT TARGET Serializer::serializer_lib)
  message(FATAL_ERROR "Missing runtime or compatibility compiler-library target")
endif()
# The selected package must precede older same-name headers from dependencies.
get_target_property(runtime_is_system Serializer::runtime SYSTEM)
if(runtime_is_system)
  message(FATAL_ERROR "Installed runtime headers must use ordinary include precedence")
endif()
get_target_property(runtime_features Serializer::runtime INTERFACE_COMPILE_FEATURES)
if(NOT "cxx_std_23" IN_LIST runtime_features)
  message(FATAL_ERROR "Installed runtime must propagate the C++23 minimum")
endif()
get_target_property(runtime_dependencies Serializer::runtime INTERFACE_LINK_LIBRARIES)
if(runtime_dependencies MATCHES "serializer_lib")
  message(FATAL_ERROR "The permissive runtime links the GPL compiler library")
endif()
]=])
foreach(backend IN ITEMS zstd lz4 zlib)
  string(TOUPPER "${backend}" backend_name)
  if(WITH_${backend})
    set(expected ON)
  else()
    set(expected OFF)
  endif()
  file(APPEND "${source}/CMakeLists.txt"
    "get_property(runtime_capability_set TARGET Serializer::runtime PROPERTY SERIALIZER_WITH_${backend_name} SET)\n"
    "if(NOT runtime_capability_set)\n"
    "  message(FATAL_ERROR \"Missing exported ${backend} capability\")\n"
    "endif()\n"
    "get_target_property(runtime_capability Serializer::runtime SERIALIZER_WITH_${backend_name})\n"
    "if(${expected} AND NOT runtime_capability OR NOT ${expected} AND runtime_capability)\n"
    "  message(FATAL_ERROR \"Incorrect exported ${backend} capability\")\n"
    "endif()\n")
endforeach()
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

set(generator_arguments -G "${GENERATOR}")
if(PLATFORM)
  list(APPEND generator_arguments -A "${PLATFORM}")
endif()
if(GENERATOR_INSTANCE)
  list(APPEND generator_arguments "-DCMAKE_GENERATOR_INSTANCE=${GENERATOR_INSTANCE}")
endif()
if(MAKE_PROGRAM)
  list(APPEND generator_arguments "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}")
endif()
set(configure_arguments -S "${source}" -B "${build}" ${generator_arguments}
  "-DCMAKE_BUILD_TYPE=${CONFIGURATION}")
run_checked("${CMAKE_COMMAND}" ${configure_arguments})
run_checked("${CMAKE_COMMAND}" --build "${build}" --config "${CONFIGURATION}" --parallel 2)
get_filename_component(cmake_directory "${CMAKE_COMMAND}" DIRECTORY)
run_checked("${cmake_directory}/ctest" --test-dir "${build}" -C "${CONFIGURATION}"
  --output-on-failure)
message(STATUS "Verified installed Serializer headers, targets, and compression capabilities")

# SDK-only discovery must stay language-neutral and still permit a later native consumer.
get_filename_component(repository "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(component_source "${DIRECTORY}/example_components")
set(component_build "${DIRECTORY}/example_components_build")
file(MAKE_DIRECTORY "${component_source}")
file(WRITE "${component_source}/CMakeLists.txt" "cmake_minimum_required(VERSION 4.0)\n"
  "project(installed_example_components LANGUAGES NONE)\n"
  "set(CMAKE_PREFIX_PATH [=[${prefix};${DEPENDENCY_PREFIXES}]=])\n"
  "set(CMAKE_CXX_EXTENSIONS OFF)\n"
  "set(SERIALIZER_EXAMPLE_USE_INSTALLED ON)\n"
  "include([=[${repository}/example/cmake/serializer_example_dependency.cmake]=])\n"
  [=[
serializer_example_dependency()
if(NOT TARGET Serializer::serializer OR TARGET Serializer::runtime)
  message(FATAL_ERROR "SDK-only example loaded native runtime dependencies")
endif()
enable_language(CXX)
serializer_example_dependency()
if(NOT TARGET Serializer::runtime)
  message(FATAL_ERROR "Native example did not upgrade compiler-only discovery")
endif()
add_executable(component_consumer consumer.cpp)
target_link_libraries(component_consumer PRIVATE Serializer::runtime)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/consumer-path-$<CONFIG>.txt"
  CONTENT "$<TARGET_FILE:component_consumer>")
]=])
file(WRITE "${component_source}/consumer.cpp" [=[
#include <rohit/cpp_standard.hpp>
#include <rohit/version.hpp>
// Confirm that a newly enabled native consumer inherits the installed standard.
int main() { return rohit::serializer::compiler_version.empty(); }
]=])
run_checked("${CMAKE_COMMAND}" -S "${component_source}" -B "${component_build}"
  ${generator_arguments} "-DCMAKE_BUILD_TYPE=${CONFIGURATION}")
run_checked("${CMAKE_COMMAND}" --build "${component_build}" --config "${CONFIGURATION}" --parallel 2)
file(READ "${component_build}/consumer-path-${CONFIGURATION}.txt" component_executable)
run_checked("${component_executable}")
message(STATUS "Verified SDK-only example discovery and native runtime upgrade")

if(EXAMPLE_PYTHON)
  set(example_cache "${DIRECTORY}/example_cache.cmake")
  # Initial cache entries preserve semicolon-separated prefixes through command argument lists.
  file(WRITE "${example_cache}"
    "set(CMAKE_PREFIX_PATH [==[${prefix};${DEPENDENCY_PREFIXES}]==] CACHE STRING \"Installed example dependency prefixes\")\n"
    "set(Python3_EXECUTABLE [==[${EXAMPLE_PYTHON}]==] CACHE FILEPATH \"Verified Python interpreter\")\n")
  # LANGUAGES NONE consumers retain the selected toolchain's detected multiarch search suffix.
  if(LIBRARY_ARCHITECTURE)
    file(APPEND "${example_cache}"
      "set(CMAKE_LIBRARY_ARCHITECTURE [==[${LIBRARY_ARCHITECTURE}]==] CACHE STRING \"Selected toolchain library architecture\")\n")
  endif()
  if(CMAKE_HOST_WIN32)
    # Installed compiler generation also needs the selected compression runtime DLLs.
    foreach(dependency_prefix IN ITEMS "${prefix}" ${DEPENDENCY_PREFIXES})
      set(ENV{PATH} "${dependency_prefix}/bin;$ENV{PATH}")
      if(CONFIGURATION STREQUAL "Debug")
        set(ENV{PATH} "${dependency_prefix}/debug/bin;$ENV{PATH}")
      endif()
    endforeach()
  endif()
  foreach(example IN ITEMS cpp/basic c/interoperability python/basic)
    set(example_build "${DIRECTORY}/examples/${example}")
    run_checked("${CMAKE_COMMAND}" -S "${repository}/example/${example}" -B "${example_build}"
      ${generator_arguments} -C "${example_cache}" "-DCMAKE_BUILD_TYPE=${CONFIGURATION}"
      -DSERIALIZER_EXAMPLE_USE_INSTALLED=ON)
    run_checked("${CMAKE_COMMAND}" --build "${example_build}"
      --config "${CONFIGURATION}" --parallel 2)
    run_checked("${cmake_directory}/ctest" --test-dir "${example_build}"
      -C "${CONFIGURATION}" --output-on-failure --no-tests=error)
  endforeach()
  message(STATUS "Verified standalone C++, C, and Python examples against the installed package")
else()
  message(STATUS "Standalone installed examples require Python 3.10+; interpreter unavailable")
endif()
