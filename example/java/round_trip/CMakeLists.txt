cmake_minimum_required(VERSION 4.0)
project(serializer_java_round_trip_example LANGUAGES NONE)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/serializer_java_account_example.cmake")
serializer_add_java_account_example(serializer_java_round_trip)
