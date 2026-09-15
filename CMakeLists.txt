cmake_minimum_required(VERSION 3.28)

if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
project(serializer)
# find_package must be after project
find_package(GTest CONFIG REQUIRED)
endif()

if (UNIX)
add_compile_options(-Wall -Werror -Wpedantic -Wextra -Weffc++ -Wsuggest-override) # -fanalyzer
elseif (MSVC)
add_compile_options(/W4 /WX)
endif()

add_library(serializerlib src/parser.cpp src/cpp_writer.cpp src/cpp_type_mapping.cpp include/rohit/serializer_creator.hpp include/rohit/serializer.hpp include/rohit/stream.hpp)

add_executable(serializer src/serializer.cpp)

target_link_libraries(serializer serializerlib)

include_directories(include)

target_compile_features(serializer PUBLIC cxx_std_20)
target_compile_features(serializerlib PUBLIC cxx_std_20)

if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
    enable_testing()
    add_subdirectory(test)
endif()
