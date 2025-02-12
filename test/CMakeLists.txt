cmake_minimum_required(VERSION 3.28)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/test/enum.h
    COMMAND serializer input ${CMAKE_SOURCE_DIR}/test/resources/enum.def output ${CMAKE_BINARY_DIR}/test/enum.h
    DEPENDS ${CMAKE_SOURCE_DIR}/test/resources/enum.def serializer
    VERBATIM
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/test/person.h
    COMMAND serializer input ${CMAKE_SOURCE_DIR}/test/resources/person.def output ${CMAKE_BINARY_DIR}/test/person.h
    DEPENDS ${CMAKE_SOURCE_DIR}/test/resources/person.def serializer
    VERBATIM
)


add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/test/test1.h
    COMMAND serializer input ${CMAKE_SOURCE_DIR}/test/resources/test1.def output ${CMAKE_BINARY_DIR}/test/test1.h
    DEPENDS ${CMAKE_SOURCE_DIR}/test/resources/test1.def serializer
    VERBATIM
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/test/array.h
    COMMAND serializer input ${CMAKE_SOURCE_DIR}/test/resources/array.def output ${CMAKE_BINARY_DIR}/test/array.h
    DEPENDS ${CMAKE_SOURCE_DIR}/test/resources/array.def serializer
    VERBATIM
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/test/map.h
    COMMAND serializer input ${CMAKE_SOURCE_DIR}/test/resources/map.def output ${CMAKE_BINARY_DIR}/test/map.h
    DEPENDS ${CMAKE_SOURCE_DIR}/test/resources/map.def serializer
    VERBATIM
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/test/variable.h
    COMMAND serializer input ${CMAKE_SOURCE_DIR}/test/resources/variable.def output ${CMAKE_BINARY_DIR}/test/variable.h
    DEPENDS ${CMAKE_SOURCE_DIR}/test/resources/variable.def serializer
    VERBATIM
)

add_executable(CoreSerializerTest
    coreserializertest.cpp
    serializeparsertest.cpp
    ${CMAKE_BINARY_DIR}/test/person.h
    ${CMAKE_BINARY_DIR}/test/test1.h
    ${CMAKE_BINARY_DIR}/test/array.h
    ${CMAKE_BINARY_DIR}/test/map.h
    ${CMAKE_BINARY_DIR}/test/variable.h
    ${CMAKE_BINARY_DIR}/test/enum.h
    generatedtest.cpp)

target_compile_definitions(CoreSerializerTest PUBLIC cxx_std_20)

include_directories(${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/test)

target_link_libraries(CoreSerializerTest PRIVATE serializerlib GTest::gtest_main)

target_compile_definitions(CoreSerializerTest PUBLIC ENABLE_GTEST)

add_dependencies(CoreSerializerTest serializer)

add_test(CoreSerializerTest CoreSerializerTest)
