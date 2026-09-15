cmake_minimum_required(VERSION 3.28)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/test/enum.hpp
    COMMAND serializer input ${CMAKE_SOURCE_DIR}/test/resources/enum.def output ${CMAKE_BINARY_DIR}/test/enum.hpp
    DEPENDS ${CMAKE_SOURCE_DIR}/test/resources/enum.def serializer
    VERBATIM
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/test/person.hpp
    COMMAND serializer input ${CMAKE_SOURCE_DIR}/test/resources/person.def output ${CMAKE_BINARY_DIR}/test/person.hpp
    DEPENDS ${CMAKE_SOURCE_DIR}/test/resources/person.def serializer
    VERBATIM
)


add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/test/test1.hpp
    COMMAND serializer input ${CMAKE_SOURCE_DIR}/test/resources/test1.def output ${CMAKE_BINARY_DIR}/test/test1.hpp
    DEPENDS ${CMAKE_SOURCE_DIR}/test/resources/test1.def serializer
    VERBATIM
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/test/array.hpp
    COMMAND serializer input ${CMAKE_SOURCE_DIR}/test/resources/array.def output ${CMAKE_BINARY_DIR}/test/array.hpp
    DEPENDS ${CMAKE_SOURCE_DIR}/test/resources/array.def serializer
    VERBATIM
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/test/map.hpp
    COMMAND serializer input ${CMAKE_SOURCE_DIR}/test/resources/map.def output ${CMAKE_BINARY_DIR}/test/map.hpp
    DEPENDS ${CMAKE_SOURCE_DIR}/test/resources/map.def serializer
    VERBATIM
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/test/variable.hpp
    COMMAND serializer input ${CMAKE_SOURCE_DIR}/test/resources/variable.def output ${CMAKE_BINARY_DIR}/test/variable.hpp
    DEPENDS ${CMAKE_SOURCE_DIR}/test/resources/variable.def serializer
    VERBATIM
)

add_executable(CoreSerializerTest
    core_serializer_test.cpp
    serializer_parser_test.cpp
    ${CMAKE_BINARY_DIR}/test/person.hpp
    ${CMAKE_BINARY_DIR}/test/test1.hpp
    ${CMAKE_BINARY_DIR}/test/array.hpp
    ${CMAKE_BINARY_DIR}/test/map.hpp
    ${CMAKE_BINARY_DIR}/test/variable.hpp
    ${CMAKE_BINARY_DIR}/test/enum.hpp
    generated_test.cpp)

target_compile_definitions(CoreSerializerTest PUBLIC cxx_std_20)

include_directories(${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/test)

target_link_libraries(CoreSerializerTest PRIVATE serializerlib GTest::gtest_main)

target_compile_definitions(CoreSerializerTest PUBLIC ENABLE_GTEST)

add_dependencies(CoreSerializerTest serializer)

add_test(CoreSerializerTest CoreSerializerTest)
