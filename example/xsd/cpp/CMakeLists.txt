cmake_minimum_required(VERSION 4.0)
project(serializer_xsd_cpp_example LANGUAGES CXX)

find_package(LibXml2 REQUIRED)
if(NOT XSD_GENERATED_DIRECTORY OR NOT XSD_EXAMPLE_SOURCE OR
   NOT SERIALIZER_INCLUDE_DIRECTORY OR NOT SERIALIZER_RUNTIME_LIBRARY)
  message(FATAL_ERROR "Run example/xsd/run.py with a built XML-enabled Serializer runtime")
endif()
add_executable(serializer_xsd_cpp "${XSD_EXAMPLE_SOURCE}")
target_compile_features(serializer_xsd_cpp PRIVATE cxx_std_23)
target_include_directories(serializer_xsd_cpp PRIVATE
  "${XSD_GENERATED_DIRECTORY}" "${SERIALIZER_INCLUDE_DIRECTORY}")
target_link_libraries(serializer_xsd_cpp PRIVATE
  "${SERIALIZER_RUNTIME_LIBRARY}" LibXml2::LibXml2)
if(MSVC)
  target_compile_options(serializer_xsd_cpp PRIVATE /W4 /WX /utf-8)
else()
  target_compile_options(serializer_xsd_cpp PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
