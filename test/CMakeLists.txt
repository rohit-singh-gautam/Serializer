cmake_minimum_required(VERSION 3.28)

project(SerializerTest)

add_executable(CoreSerializerTest src/coreserializertest.cpp)

include_directories(${CMAKE_SOURCE_DIR}/include)

set_property(TARGET CoreSerializerTest PROPERTY CXX_STANDARD 23)

target_link_libraries(CoreSerializerTest PRIVATE GTest::gtest_main)

add_test(CoreSerializerTest CoreSerializerTest)