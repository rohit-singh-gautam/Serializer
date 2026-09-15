cmake_minimum_required(VERSION 3.28)

project(serializer LANGUAGES CXX)
include(GNUInstallDirs)

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
option(SERIALIZER_INSTALL "Install the library, compiler, and CMake integration" ${PROJECT_IS_TOP_LEVEL})

add_library(serializer_lib
  src/parser.cpp
  src/cpp_writer.cpp
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
  include/rohit/stream.hpp
)
target_include_directories(serializer_lib PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_compile_features(serializer_lib PUBLIC cxx_std_20)
add_library(Serializer::serializer_lib ALIAS serializer_lib)

add_executable(serializer src/serializer.cpp)
target_link_libraries(serializer PRIVATE serializer_lib)
add_executable(Serializer::serializer ALIAS serializer)

# The same generation API is available from add_subdirectory and the installed package.
include(cmake/serializer_generate.cmake)

if(SERIALIZER_INSTALL)
  include(CMakePackageConfigHelpers)
  set(serializer_package_directory "${CMAKE_INSTALL_LIBDIR}/cmake/Serializer")
  configure_package_config_file(cmake/serializer_config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/SerializerConfig.cmake"
    INSTALL_DESTINATION "${serializer_package_directory}"
  )
  install(TARGETS serializer_lib serializer EXPORT SerializerTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  )
  install(DIRECTORY include/rohit DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.hpp")
  install(EXPORT SerializerTargets NAMESPACE Serializer::
    DESTINATION "${serializer_package_directory}")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/SerializerConfig.cmake"
    cmake/serializer_generate.cmake DESTINATION "${serializer_package_directory}")
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
