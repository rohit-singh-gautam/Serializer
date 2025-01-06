cmake_minimum_required(VERSION 3.28)

project(serializer)
# find_package must be after project
find_package(GTest CONFIG REQUIRED)

add_compile_options(-Wall -Werror -Wpedantic -Wextra -Weffc++ -Wsuggest-override) # -fanalyzer

add_executable(serializer src/serializer.cpp)

include_directories(include)

set_property(TARGET serializer PROPERTY CXX_STANDARD 23)

if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
    enable_testing()
    add_subdirectory(test)
endif()
