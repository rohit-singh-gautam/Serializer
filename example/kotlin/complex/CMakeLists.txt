cmake_minimum_required(VERSION 4.0)
project(serializer_kotlin_complex_example LANGUAGES NONE)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/serializer_language_example.cmake")
serializer_add_language_example(kotlin complex)
