cmake_minimum_required(VERSION 4.0)
project(serializer_python_basic_example LANGUAGES NONE)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/serializer_language_example.cmake")
serializer_add_language_example(python basic)
