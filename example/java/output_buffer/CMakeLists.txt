cmake_minimum_required(VERSION 4.0)
project(serializer_java_output_buffer_example LANGUAGES NONE)

# Require the host compiler feature when this SDK-only example uses an installed package.
if(SERIALIZER_EXAMPLE_USE_INSTALLED)
  find_package(Serializer 2.33 CONFIG REQUIRED COMPONENTS compiler)
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/serializer_java_account_example.cmake")
serializer_add_java_account_example(serializer_java_output_buffer)
