cmake_minimum_required(VERSION 3.28)

if(WIN32)
set(SERIALIZER ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/serializer.exe)
elseif(UNIX)
set(SERIALIZER ${CMAKE_BINARY_DIR}/serializer)
endif()

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/test/enum.h
    COMMAND ${SERIALIZER} input ${CMAKE_SOURCE_DIR}/test/resources/enum.def output ${CMAKE_BINARY_DIR}/test/enum.h
    DEPENDS ${CMAKE_SOURCE_DIR}/test/resources/enum.def ${CMAKE_SOURCE_DIR}/include/rohit/serializercreator.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/test/person.h
    COMMAND ${SERIALIZER} input ${CMAKE_SOURCE_DIR}/test/resources/person.def output ${CMAKE_BINARY_DIR}/test/person.h
    DEPENDS ${CMAKE_SOURCE_DIR}/test/resources/person.def ${CMAKE_SOURCE_DIR}/include/rohit/serializercreator.h
    VERBATIM
)


add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/test/test1.h
    COMMAND ${SERIALIZER} input ${CMAKE_SOURCE_DIR}/test/resources/test1.def output ${CMAKE_BINARY_DIR}/test/test1.h
    DEPENDS ${CMAKE_SOURCE_DIR}/test/resources/test1.def ${CMAKE_SOURCE_DIR}/include/rohit/serializercreator.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/test/array.h
    COMMAND ${SERIALIZER} input ${CMAKE_SOURCE_DIR}/test/resources/array.def output ${CMAKE_BINARY_DIR}/test/array.h
    DEPENDS ${CMAKE_SOURCE_DIR}/test/resources/array.def ${CMAKE_SOURCE_DIR}/include/rohit/serializercreator.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/test/map.h
    COMMAND ${SERIALIZER} input ${CMAKE_SOURCE_DIR}/test/resources/map.def output ${CMAKE_BINARY_DIR}/test/map.h
    DEPENDS ${CMAKE_SOURCE_DIR}/test/resources/map.def ${CMAKE_SOURCE_DIR}/include/rohit/serializercreator.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/test/variable.h
    COMMAND ${SERIALIZER} input ${CMAKE_SOURCE_DIR}/test/resources/variable.def output ${CMAKE_BINARY_DIR}/test/variable.h
    DEPENDS ${CMAKE_SOURCE_DIR}/test/resources/variable.def ${CMAKE_SOURCE_DIR}/include/rohit/serializercreator.h
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

include_directories(${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/test)

set_property(TARGET CoreSerializerTest PROPERTY CXX_STANDARD 23)

target_link_libraries(CoreSerializerTest PRIVATE serializerlib GTest::gtest_main)

target_compile_definitions(CoreSerializerTest PUBLIC ENABLE_GTEST)

add_dependencies(CoreSerializerTest serializer)

add_test(CoreSerializerTest CoreSerializerTest)
