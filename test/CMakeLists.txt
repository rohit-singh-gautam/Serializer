cmake_minimum_required(VERSION 3.28)

add_executable(CoreSerializerTest coreserializertest.cpp serializeparsertest.cpp)

include_directories(${CMAKE_SOURCE_DIR}/include)

set_property(TARGET CoreSerializerTest PROPERTY CXX_STANDARD 23)

target_link_libraries(CoreSerializerTest PRIVATE GTest::gtest_main)

target_compile_definitions(CoreSerializerTest PUBLIC ENABLE_GTEST)

add_test(CoreSerializerTest CoreSerializerTest)