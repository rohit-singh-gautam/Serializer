cmake_minimum_required(VERSION 3.28)

if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
project(serializer)
# find_package must be after project
find_package(GTest CONFIG REQUIRED)
endif()

add_compile_options(-Wall -Werror -Wpedantic -Wextra -Weffc++ -Wsuggest-override) # -fanalyzer

add_library(serializerlib src/parser.cpp src/cppwriter.cpp src/common.cpp)

add_executable(serializer src/serializer.cpp)

target_link_libraries(serializer serializerlib)

include_directories(include)

set_property(TARGET serializer PROPERTY CXX_STANDARD 23)
set_property(TARGET serializerlib PROPERTY CXX_STANDARD 23)

if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
    enable_testing()
    add_subdirectory(test)
endif()
