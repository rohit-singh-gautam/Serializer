cmake_minimum_required(VERSION 4.0)
project(serializer_opc_example LANGUAGES CXX)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/serializer_example_dependency.cmake")
if(NOT DEFINED SERIALIZER_WITH_XML)
  set(SERIALIZER_WITH_XML ON)
endif()
if(NOT DEFINED SERIALIZER_WITH_ZLIB)
  set(SERIALIZER_WITH_ZLIB ON)
endif()
serializer_example_dependency()
enable_testing()

add_executable(serializer_opc_example main.cpp)
serializer_generate(TARGET serializer_opc_example SCHEMAS model.serializer
  CONFIG "${CMAKE_CURRENT_LIST_DIR}/codegen.ini"
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(serializer_opc_example PRIVATE Serializer::runtime)
serializer_example_runtime_dlls(serializer_opc_example)
if(MSVC)
  target_compile_options(serializer_opc_example PRIVATE /W4 /WX /utf-8)
else()
  target_compile_options(serializer_opc_example PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME serializer_opc_example COMMAND serializer_opc_example
  "${CMAKE_CURRENT_BINARY_DIR}/catalog.opc")

find_package(Python3 3.10 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME serializer_opc_exchange
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_LIST_DIR}/run.py"
      --compiler "$<TARGET_FILE:Serializer::serializer>" --cpp "$<TARGET_FILE:serializer_opc_example>"
      --build "${CMAKE_CURRENT_BINARY_DIR}/exchange")
endif()
