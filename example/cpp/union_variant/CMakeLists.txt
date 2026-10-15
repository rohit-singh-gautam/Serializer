cmake_minimum_required(VERSION 4.0)
project(serializer_cpp_union_variant_example LANGUAGES CXX)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/serializer_language_example.cmake")
serializer_add_language_example(cpp union_variant)
