# Verify compiler-only discovery against the actual relocatable installation.
foreach(required IN ITEMS BUILD_DIRECTORY DIRECTORY CONFIGURATION BUILD_MANAGED GENERATOR
    VERSION POINTER_SIZE HOST_SYSTEM_NAME)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "compiler_package.cmake requires ${required}")
  endif()
endforeach()

# Stop on command failure and retain enough output to diagnose the consumer.
function(run_checked)
  execute_process(COMMAND ${ARGV} RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE errors)
  if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Compiler-package check failed (${status}):\n${output}\n${errors}")
  endif()
  set(last_output "${output}" PARENT_SCOPE)
endfunction()

# Rejected consumers must fail for the intended contract, not an unrelated error.
function(run_rejected expected)
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE errors)
  if(status STREQUAL "0" OR NOT "${output}\n${errors}" MATCHES "${expected}")
    message(FATAL_ERROR "Expected package rejection '${expected}' (${status}):\n${output}\n${errors}")
  endif()
endfunction()

# Keep every installation and relocation inside this test's build subtree.
cmake_path(ABSOLUTE_PATH BUILD_DIRECTORY NORMALIZE OUTPUT_VARIABLE build_root)
cmake_path(ABSOLUTE_PATH DIRECTORY NORMALIZE OUTPUT_VARIABLE test_root)
cmake_path(IS_PREFIX build_root "${test_root}" NORMALIZE inside_build)
if(NOT inside_build OR test_root STREQUAL build_root)
  message(FATAL_ERROR "Compiler-package test directory must be below its build directory")
endif()
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef run_id)
set(directory "${test_root}/run-${run_id}")
set(original_prefix "${directory}/initial install")
set(prefix "${directory}/relocated prefix with spaces")
file(MAKE_DIRECTORY "${directory}")
run_checked("${CMAKE_COMMAND}" --install "${BUILD_DIRECTORY}"
  --config "${CONFIGURATION}" --prefix "${original_prefix}")
file(RENAME "${original_prefix}" "${prefix}")
# Force prefix-only discovery through the language-neutral package location.
# Disabling the library copy also catches accidental reliance on a detected lib suffix.
file(GLOB_RECURSE package_configs "${prefix}/*/SerializerConfig.cmake")
foreach(package_config IN LISTS package_configs)
  if(NOT package_config MATCHES "/share/cmake/Serializer/SerializerConfig[.]cmake$")
    file(RENAME "${package_config}" "${package_config}.disabled")
  endif()
endforeach()

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

# Poison both config and module discovery so a system provider cannot hide a lookup.
set(poison "${directory}/forbidden providers")
file(MAKE_DIRECTORY "${poison}")
foreach(provider IN ITEMS zstd lz4 ZLIB LibXml2)
  string(TOLOWER "${provider}" provider_lower)
  foreach(filename IN ITEMS "${provider}Config.cmake" "${provider_lower}-config.cmake" "Find${provider}.cmake")
    file(WRITE "${poison}/${filename}"
      "message(FATAL_ERROR \"Runtime provider discovery is forbidden: ${provider}\")\n")
  endforeach()
endforeach()

# Generate consumers without requiring any language SDK or C/C++ compiler.
function(write_consumer name body)
  set(source "${directory}/${name}")
  file(MAKE_DIRECTORY "${source}")
  set(header [=[cmake_minimum_required(VERSION 4.0)
project(serializer_package_consumer LANGUAGES NONE)
set(CMAKE_PREFIX_PATH [==[@prefix@;@DEPENDENCY_PREFIXES@]==])
set(CMAKE_LIBRARY_ARCHITECTURE [==[@LIBRARY_ARCHITECTURE@]==])
set(CMAKE_SIZEOF_VOID_P @POINTER_SIZE@)
]=])
  string(CONFIGURE "${header}${body}" contents @ONLY)
  file(WRITE "${source}/CMakeLists.txt" "${contents}")
endfunction()

# Configure one generated consumer with the parent build's native build tool.
function(configure_consumer name)
  run_checked("${CMAKE_COMMAND}" -S "${directory}/${name}" -B "${directory}/${name}-build"
    ${generator_arguments} "-DCMAKE_BUILD_TYPE=${CONFIGURATION}" ${ARGN})
endfunction()

if(POINTER_SIZE EQUAL 8)
  set(other_pointer_size 4)
else()
  set(other_pointer_size 8)
endif()
string(REPLACE "." ";" release_components "${VERSION}")
list(GET release_components 0 release_major)
list(GET release_components 1 release_minor)
math(EXPR next_minor "${release_minor} + 1")

write_consumer(compiler [=[
# Explicitly selecting a system makes this a cross build even on the same host OS.
if(NOT CMAKE_CROSSCOMPILING)
  message(FATAL_ERROR "Expected a mocked cross-compiling consumer")
endif()
set(CMAKE_SIZEOF_VOID_P @other_pointer_size@)
unset(CMAKE_LIBRARY_ARCHITECTURE)
set(CMAKE_FIND_ROOT_PATH "${CMAKE_CURRENT_BINARY_DIR}/target sysroot")
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
find_package(Serializer @VERSION@ EXACT CONFIG QUIET COMPONENTS compiler
  NO_DEFAULT_PATH PATHS [==[@prefix@]==])
if(Serializer_FOUND)
  message(FATAL_ERROR "Host package unexpectedly resolved inside the empty target sysroot")
endif()
set(CMAKE_MODULE_PATH [==[@poison@]==])
list(PREPEND CMAKE_PREFIX_PATH [==[@poison@]==])
foreach(provider IN ITEMS zstd lz4 ZLIB LibXml2)
  set(${provider}_DIR [==[@poison@]==])
endforeach()
find_package(Serializer @VERSION@ EXACT CONFIG REQUIRED COMPONENTS compiler
  OPTIONAL_COMPONENTS unsupported_optional NO_CMAKE_FIND_ROOT_PATH)
if(NOT Serializer_compiler_FOUND OR Serializer_unsupported_optional_FOUND)
  message(FATAL_ERROR "Incorrect compiler or optional component result")
endif()
if(NOT TARGET Serializer::serializer OR TARGET Serializer::runtime OR
    TARGET Serializer::serializer_lib OR TARGET Serializer::managed)
  message(FATAL_ERROR "Compiler-only discovery must expose only the executable")
endif()
get_target_property(compiler_type Serializer::serializer TYPE)
get_target_property(compiler_imported Serializer::serializer IMPORTED)
if(NOT compiler_type STREQUAL "EXECUTABLE" OR NOT compiler_imported)
  message(FATAL_ERROR "Compiler target is not an imported host executable")
endif()
foreach(helper IN ITEMS serializer_generate serializer_generate_source serializer_generate_java)
  if(NOT COMMAND ${helper})
    message(FATAL_ERROR "Missing installed generation helper: ${helper}")
  endif()
endforeach()
find_package(Serializer @release_major@.0.0 CONFIG REQUIRED COMPONENTS compiler)
find_package(Serializer @release_major@.@next_minor@.0 CONFIG QUIET COMPONENTS compiler
  NO_DEFAULT_PATH PATHS [==[@prefix@]==])
if(Serializer_FOUND)
  message(FATAL_ERROR "Compiler component accepted an unsupported future release")
endif()
find_package(Serializer @VERSION@ EXACT CONFIG REQUIRED COMPONENTS compiler
  NO_CMAKE_FIND_ROOT_PATH)
file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/model.serializer"
  "serializer version 1;\nclass account_record stable_ids { public uint32 account_id (1); }\n")
serializer_generate_source(TARGET generated_python SCHEMA model.serializer LANGUAGE python
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated output/model.py")
serializer_generate_java(TARGET generated_java SCHEMA model.serializer
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated output/Model.java")
add_custom_target(generate_all ALL)
add_dependencies(generate_all generated_python generated_java)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/compiler-path.txt"
  CONTENT "$<TARGET_FILE:Serializer::serializer>")
]=])
configure_consumer(compiler "-DCMAKE_SYSTEM_NAME=${HOST_SYSTEM_NAME}")

# Runtime DLL availability remains a host installation prerequisite on Windows.
if(CMAKE_HOST_WIN32)
  foreach(dependency_prefix IN ITEMS "${prefix}" ${DEPENDENCY_PREFIXES})
    set(ENV{PATH} "${dependency_prefix}/bin;$ENV{PATH}")
    if(CONFIGURATION STREQUAL "Debug")
      set(ENV{PATH} "${dependency_prefix}/debug/bin;$ENV{PATH}")
    endif()
  endforeach()
endif()
run_checked("${CMAKE_COMMAND}" --build "${directory}/compiler-build"
  --config "${CONFIGURATION}" --parallel 2)
file(READ "${directory}/compiler-build/compiler-path.txt" compiler)
cmake_path(IS_PREFIX prefix "${compiler}" NORMALIZE compiler_relocated)
if(NOT compiler_relocated OR NOT EXISTS "${compiler}")
  message(FATAL_ERROR "Imported compiler did not relocate to ${prefix}: ${compiler}")
endif()
run_checked("${compiler}" --version)
string(REPLACE "." "[.]" version_pattern "${VERSION}")
if(NOT last_output MATCHES "Serializer compiler ${version_pattern}[\r\n]")
  message(FATAL_ERROR "Imported executable reported the wrong release: ${last_output}")
endif()
foreach(output IN ITEMS model.py Model.java)
  set(generated "${directory}/compiler-build/generated output/${output}")
  if(NOT EXISTS "${generated}" OR NOT EXISTS "${generated}.d")
    message(FATAL_ERROR "Installed helper did not generate ${output} and its dependency file")
  endif()
  file(READ "${generated}" generated_source)
  if(NOT generated_source MATCHES "[Aa]ccount" OR NOT generated_source MATCHES "[Ii]d")
    message(FATAL_ERROR "Installed helper generated no account schema in ${output}")
  endif()
endforeach()

write_consumer(repeated [=[
find_package(Serializer @VERSION@ EXACT CONFIG REQUIRED COMPONENTS compiler)
if(TARGET Serializer::runtime OR TARGET Serializer::serializer_lib OR TARGET Serializer::managed)
  message(FATAL_ERROR "First compiler lookup loaded runtime exports")
endif()
find_package(Serializer @VERSION@ EXACT CONFIG REQUIRED COMPONENTS runtime)
if(NOT Serializer_runtime_FOUND OR NOT TARGET Serializer::runtime OR
    NOT TARGET Serializer::serializer_lib OR NOT TARGET Serializer::serializer)
  message(FATAL_ERROR "Runtime lookup after compiler lookup did not complete exports")
endif()
if(@BUILD_MANAGED@ AND NOT TARGET Serializer::managed)
  message(FATAL_ERROR "Runtime component omitted the enabled managed target")
endif()
find_package(Serializer @VERSION@ EXACT CONFIG REQUIRED COMPONENTS compiler runtime)
find_package(Serializer @VERSION@ EXACT CONFIG REQUIRED COMPONENTS compiler)
if(NOT TARGET Serializer::runtime OR NOT TARGET Serializer::serializer_lib)
  message(FATAL_ERROR "Repeated compiler lookup lost existing runtime exports")
endif()
]=])
configure_consumer(repeated)

write_consumer(default [=[
find_package(Serializer @VERSION@ EXACT CONFIG REQUIRED)
if(NOT TARGET Serializer::runtime OR NOT TARGET Serializer::serializer_lib OR
    NOT TARGET Serializer::serializer)
  message(FATAL_ERROR "Default package lookup no longer exposes the complete package")
endif()
if(@BUILD_MANAGED@ AND NOT TARGET Serializer::managed)
  message(FATAL_ERROR "Default lookup omitted the enabled managed target")
endif()
]=])
configure_consumer(default)

write_consumer(unknown-optional [=[
find_package(Serializer @VERSION@ EXACT CONFIG REQUIRED OPTIONAL_COMPONENTS unsupported_optional)
if(Serializer_unsupported_optional_FOUND OR NOT Serializer_runtime_FOUND OR
    NOT TARGET Serializer::runtime OR NOT TARGET Serializer::serializer_lib OR
    NOT TARGET Serializer::serializer)
  message(FATAL_ERROR "Ignored optional names changed default package discovery")
endif()
]=])
configure_consumer(unknown-optional)

foreach(selection IN ITEMS compiler-runtime runtime-only)
  if(selection STREQUAL "compiler-runtime")
    set(components "COMPONENTS compiler OPTIONAL_COMPONENTS runtime")
  else()
    set(components "OPTIONAL_COMPONENTS runtime")
  endif()
  write_consumer("optional-architecture-${selection}" [=[
set(CMAKE_SIZEOF_VOID_P @other_pointer_size@)
set(CMAKE_MODULE_PATH [==[@poison@]==])
list(PREPEND CMAKE_PREFIX_PATH [==[@poison@]==])
foreach(provider IN ITEMS zstd lz4 ZLIB LibXml2)
  set(${provider}_DIR [==[@poison@]==])
endforeach()
find_package(Serializer @VERSION@ EXACT CONFIG REQUIRED @components@)
if(NOT Serializer_compiler_FOUND OR Serializer_runtime_FOUND OR
    NOT TARGET Serializer::serializer OR TARGET Serializer::runtime OR
    TARGET Serializer::serializer_lib OR TARGET Serializer::managed)
  message(FATAL_ERROR "ABI-incompatible optional runtime blocked or polluted compiler discovery")
endif()
]=])
  configure_consumer("optional-architecture-${selection}")
endforeach()

if(WITH_ZSTD OR WITH_LZ4 OR WITH_ZLIB OR WITH_XML)
  write_consumer(optional-runtime [=[
foreach(provider IN ITEMS zstd lz4 ZLIB LibXml2)
  set(CMAKE_DISABLE_FIND_PACKAGE_${provider} TRUE)
endforeach()
find_package(Serializer @VERSION@ EXACT CONFIG REQUIRED COMPONENTS compiler
  OPTIONAL_COMPONENTS runtime)
if(NOT Serializer_compiler_FOUND OR Serializer_runtime_FOUND OR
    NOT TARGET Serializer::serializer OR TARGET Serializer::runtime OR
    TARGET Serializer::serializer_lib OR TARGET Serializer::managed)
  message(FATAL_ERROR "Unavailable optional runtime prevented compiler-only discovery")
endif()
]=])
  configure_consumer(optional-runtime)
endif()

write_consumer(unknown [=[
find_package(Serializer @VERSION@ EXACT CONFIG REQUIRED COMPONENTS compiler unsupported_required)
]=])
run_rejected("unsupported_required" "${CMAKE_COMMAND}" -S "${directory}/unknown"
  -B "${directory}/unknown-build" ${generator_arguments})

write_consumer(runtime-helper [=[
find_package(Serializer @VERSION@ EXACT CONFIG REQUIRED COMPONENTS compiler)
add_library(model INTERFACE)
serializer_generate(TARGET model SCHEMAS [==[@directory@/compiler/model.serializer]==])
]=])
run_rejected("requires Serializer::runtime" "${CMAKE_COMMAND}" -S "${directory}/runtime-helper"
  -B "${directory}/runtime-helper-build" ${generator_arguments})

foreach(selection IN ITEMS default runtime unknown-optional)
  if(selection STREQUAL "runtime")
    set(components "COMPONENTS runtime")
  elseif(selection STREQUAL "unknown-optional")
    set(components "OPTIONAL_COMPONENTS unsupported_optional")
  else()
    set(components "")
  endif()
  set(body [=[
set(CMAKE_SIZEOF_VOID_P @other_pointer_size@)
find_package(Serializer @VERSION@ EXACT CONFIG QUIET @components@
  NO_DEFAULT_PATH PATHS [==[@prefix@]==])
if(Serializer_FOUND)
  message(FATAL_ERROR "Runtime package accepted an incompatible pointer width")
endif()
]=])
  write_consumer("architecture-${selection}" "${body}")
  configure_consumer("architecture-${selection}")
endforeach()
message(STATUS "Verified relocated compiler-only package, host generation, components and runtime architecture checks")
