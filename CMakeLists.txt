cmake_minimum_required(VERSION 3.28)

project(serializer LANGUAGES CXX)

# Require at least C++20 while preserving a caller's newer language mode.
if(NOT DEFINED CMAKE_CXX_STANDARD OR CMAKE_CXX_STANDARD LESS 20)
  set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SERIALIZER_BUILD_TESTS "Build Serializer's tests" ${PROJECT_IS_TOP_LEVEL})
option(SERIALIZER_BUILD_BENCHMARKS "Build separate codec timing and allocation tools" OFF)
option(SERIALIZER_BUILD_FUZZERS "Build the bounded Clang libFuzzer target" OFF)

add_library(serializer_lib
  src/parser.cpp
  src/cpp_writer.cpp
  src/cpp_type_mapping.cpp
  include/rohit/serializer_creator.hpp
  include/rohit/serializer.hpp
  include/rohit/decode.hpp
  include/rohit/json_text.hpp
  include/rohit/stream.hpp
)
target_include_directories(serializer_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(serializer_lib PUBLIC cxx_std_20)

add_executable(serializer src/serializer.cpp)
target_link_libraries(serializer PRIVATE serializer_lib)

# Keep this project's warnings local when consumed through add_subdirectory.
foreach(target IN ITEMS serializer_lib serializer)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  elseif(UNIX)
    target_compile_options(${target} PRIVATE -Wall -Werror -Wpedantic -Wextra -Weffc++ -Wsuggest-override)
  endif()
endforeach()

if(SERIALIZER_BUILD_TESTS)
  enable_testing()
  find_package(GTest CONFIG REQUIRED)
  add_subdirectory(test)
endif()

if(SERIALIZER_BUILD_BENCHMARKS OR SERIALIZER_BUILD_FUZZERS)
  add_subdirectory(qualification)
endif()
