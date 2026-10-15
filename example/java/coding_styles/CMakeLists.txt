cmake_minimum_required(VERSION 4.0)
project(serializer_java_coding_styles_example LANGUAGES NONE)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/serializer_example_dependency.cmake")
serializer_example_dependency()
enable_testing()
foreach(profile IN ITEMS serializer google oracle)
  add_subdirectory("${profile}")
endforeach()
