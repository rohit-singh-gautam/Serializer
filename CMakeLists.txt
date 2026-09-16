cmake_minimum_required(VERSION 3.28)

project(serializer VERSION 0.1.0 LANGUAGES CXX)
include(GNUInstallDirs)
configure_file(cmake/version.hpp.in generated/include/rohit/version.hpp @ONLY)

# Require at least C++20 while preserving a caller's newer language mode.
if(NOT DEFINED CMAKE_CXX_STANDARD OR CMAKE_CXX_STANDARD LESS 20)
  set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SERIALIZER_BUILD_TESTS "Build Serializer's tests" ${PROJECT_IS_TOP_LEVEL})
option(SERIALIZER_BUILD_BENCHMARKS "Build separate codec timing and allocation tools" OFF)
option(SERIALIZER_BUILD_FUZZERS "Build the bounded Clang libFuzzer target" OFF)
option(SERIALIZER_BUILD_STYLE_EXAMPLES "Build one example for each C++ output profile" OFF)
option(SERIALIZER_BUILD_JAVA_EXAMPLES "Generate and compile pure Java examples (JDK 17+)" OFF)
option(SERIALIZER_INSTALL "Install the library, compiler, and CMake integration" ${PROJECT_IS_TOP_LEVEL})
option(SERIALIZER_ENABLE_SIMD "Accelerate schema scanning and runtime codecs on supported CPUs" ON)

add_library(serializer_lib
  src/parser.cpp
  src/schema_scan.cpp
  src/schema_scan.hpp
  src/simd_dispatch.cpp
  src/simd_dispatch.hpp
  src/runtime_simd.cpp
  src/cpp_writer.cpp
  src/java_writer.cpp
  src/java_runtime.inc
  src/cpp_type_mapping.cpp
  src/cpp_naming.cpp
  src/cpp_naming.hpp
  src/output_options.cpp
  include/rohit/output_options.hpp
  include/rohit/serializer_creator.hpp
  include/rohit/serializer.hpp
  include/rohit/decode.hpp
  include/rohit/binary_view.hpp
  include/rohit/json_text.hpp
  include/rohit/runtime_simd.hpp
  include/rohit/stream.hpp
)
target_include_directories(serializer_lib PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated/include>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_compile_features(serializer_lib PUBLIC cxx_std_20)
target_compile_definitions(serializer_lib PRIVATE
  SERIALIZER_ENABLE_SIMD=$<BOOL:${SERIALIZER_ENABLE_SIMD}>)

# Keep AVX2 flags off the dispatcher and consumers, including in unity builds.
set(serializer_x64 FALSE)
if(MSVC)
  if(CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
    set(serializer_x64 TRUE)
  endif()
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" serializer_processor)
  if(serializer_processor MATCHES "^(x86_64|amd64|x64)$")
    set(serializer_x64 TRUE)
  endif()
  # A source-specific -mavx2 flag cannot be shared with an ARM slice of a universal binary.
  if(APPLE AND CMAKE_OSX_ARCHITECTURES AND NOT CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
    set(serializer_x64 FALSE)
  endif()
endif()
if(SERIALIZER_ENABLE_SIMD AND serializer_x64 AND
    CMAKE_CXX_COMPILER_ID MATCHES "^(MSVC|GNU|Clang|AppleClang)$")
  set(serializer_avx2_sources src/schema_scan_avx2.cpp src/runtime_simd_avx2.cpp)
  target_sources(serializer_lib PRIVATE ${serializer_avx2_sources})
  target_compile_definitions(serializer_lib PRIVATE SERIALIZER_HAS_AVX2_SCANNER=1)
  if(MSVC)
    set_source_files_properties(${serializer_avx2_sources} PROPERTIES COMPILE_OPTIONS /arch:AVX2)
  else()
    set_source_files_properties(${serializer_avx2_sources} PROPERTIES COMPILE_OPTIONS -mavx2)
  endif()
  set_source_files_properties(${serializer_avx2_sources} PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE)
endif()

add_library(Serializer::serializer_lib ALIAS serializer_lib)

add_executable(serializer src/serializer.cpp src/command_line.hpp)
target_link_libraries(serializer PRIVATE serializer_lib)
add_executable(Serializer::serializer ALIAS serializer)

# The same generation API is available from add_subdirectory and the installed package.
include(cmake/serializer_generate.cmake)
include(cmake/serializer_generate_java.cmake)

if(SERIALIZER_INSTALL)
  include(CMakePackageConfigHelpers)
  set(serializer_package_directory "${CMAKE_INSTALL_LIBDIR}/cmake/Serializer")
  configure_package_config_file(cmake/serializer_config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/SerializerConfig.cmake"
    INSTALL_DESTINATION "${serializer_package_directory}"
  )
  write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/SerializerConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMinorVersion)
  install(TARGETS serializer_lib serializer EXPORT SerializerTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  )
  install(DIRECTORY include/rohit DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.hpp")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/include/rohit/version.hpp"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/rohit")
  install(EXPORT SerializerTargets NAMESPACE Serializer::
    DESTINATION "${serializer_package_directory}")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/SerializerConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/SerializerConfigVersion.cmake"
    cmake/serializer_generate.cmake cmake/serializer_generate_java.cmake
    DESTINATION "${serializer_package_directory}")
  install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/Serializer")
endif()

# Keep this project's warnings local when consumed through add_subdirectory.
foreach(target IN ITEMS serializer_lib serializer)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  elseif(UNIX)
    target_compile_options(${target} PRIVATE -Wall -Werror -Wpedantic -Wextra -Weffc++ -Wsuggest-override)
  endif()
endforeach()

# Formatting is a generator-time dependency; runtime users do not need clang-format.
if(SERIALIZER_BUILD_TESTS OR SERIALIZER_BUILD_BENCHMARKS OR SERIALIZER_BUILD_FUZZERS
    OR SERIALIZER_BUILD_STYLE_EXAMPLES)
  find_program(SERIALIZER_CLANG_FORMAT_EXECUTABLE
    NAMES clang-format clang-format-22 clang-format-21 clang-format-20 clang-format-19 REQUIRED)
endif()

if(SERIALIZER_BUILD_TESTS)
  enable_testing()
  find_package(GTest CONFIG REQUIRED)
  add_subdirectory(test)
endif()

if(SERIALIZER_BUILD_BENCHMARKS OR SERIALIZER_BUILD_FUZZERS)
  add_subdirectory(qualification)
endif()

if(SERIALIZER_BUILD_STYLE_EXAMPLES OR SERIALIZER_BUILD_TESTS)
  add_subdirectory(example/coding_styles)
endif()

if(SERIALIZER_BUILD_JAVA_EXAMPLES)
  add_subdirectory(example/java)
endif()
