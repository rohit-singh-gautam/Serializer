cmake_minimum_required(VERSION 4.0)
project(serializer_typescript_generics_example LANGUAGES NONE)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/serializer_language_example.cmake")
serializer_add_language_example(typescript generics)
