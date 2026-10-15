cmake_minimum_required(VERSION 4.0)
project(serializer_rust_conversions_example LANGUAGES NONE)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/serializer_language_example.cmake")
serializer_add_language_example(rust conversions)
