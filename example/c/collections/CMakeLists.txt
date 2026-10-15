cmake_minimum_required(VERSION 4.0)
project(serializer_c_collections_example LANGUAGES C CXX)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/serializer_language_example.cmake")
serializer_add_language_example(c collections)
