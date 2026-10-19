cmake_minimum_required(VERSION 4.0)
project(serializer_migration_example LANGUAGES CXX)

set(SERIALIZER_SOURCE_DIRECTORY "" CACHE PATH "Optional Serializer checkout; otherwise use an installed package")
if(NOT TARGET Serializer::managed)
  if(SERIALIZER_SOURCE_DIRECTORY)
    set(SERIALIZER_BUILD_MANAGED ON)
    set(SERIALIZER_BUILD_TESTS OFF)
    set(SERIALIZER_INSTALL OFF)
    add_subdirectory("${SERIALIZER_SOURCE_DIRECTORY}" "${CMAKE_CURRENT_BINARY_DIR}/serializer" EXCLUDE_FROM_ALL)
  else()
    find_package(Serializer 2.7 CONFIG REQUIRED)
  endif()
endif()

serializer_add_migration_tool(TARGET model_migrate
  SCHEMA model.serializer ROOT migration_example::document CONFIG model.ini)
