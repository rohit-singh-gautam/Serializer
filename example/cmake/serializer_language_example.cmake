include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/serializer_example_dependency.cmake")
enable_testing()

# Build one maintained language consumer and register its existing fixture checks with CTest.
function(serializer_add_language_example language example)
  if(language STREQUAL "go")
    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/serializer_go_example.cmake")
    serializer_add_go_example("serializer_go_${example}_example" "${CMAKE_CURRENT_SOURCE_DIR}")
    return()
  endif()
  serializer_example_dependency()
  find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
  get_filename_component(example_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
  set(target "serializer_${language}_${example}_example")
  set(verification_directory "${CMAKE_CURRENT_BINARY_DIR}/verification")
  if(example STREQUAL "generics")
    set(runner "${example_root}/generics/run.py")
    set(selection)
  else()
    set(runner "${example_root}/run.py")
    set(selection --example "${example}")
  endif()
  set(verify_command "${Python3_EXECUTABLE}" "${runner}"
    --compiler "$<TARGET_FILE:Serializer::serializer>" --build "${verification_directory}"
    --language "${language}" ${selection})

  if(language STREQUAL "cpp")
    add_executable("${target}" main.cpp)
    serializer_generate(TARGET "${target}" SCHEMAS model.serializer
      CONFIG "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/codegen.ini"
      OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
    # Handwritten consumers share the runner's stable message.hpp include name.
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/message.hpp"
      CONTENT "#pragma once\n#include \"model.hpp\"\n")
  elseif(language STREQUAL "c")
    add_executable("${target}" main.c)
    serializer_generate_source(TARGET "${target}_schema" SCHEMA model.serializer
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/schema.h" LANGUAGE c)
    add_dependencies("${target}" "${target}_schema")
    target_include_directories("${target}" PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set_target_properties("${target}" PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    if(MSVC)
      target_compile_definitions("${target}" PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
      target_link_libraries("${target}" PRIVATE m)
    endif()
  else()
    serializer_example_sdk("${language}")
    # Keep each real SDK's strict build and independent fixture validation in the shared runner.
    list(APPEND verify_command "--wsl-languages=${SERIALIZER_EXAMPLE_WSL_LANGUAGES}")
    add_custom_target("${target}" ALL
      COMMAND ${verify_command}
      DEPENDS Serializer::serializer
      COMMENT "Building and checking ${language}/${example}"
      VERBATIM)
  endif()

  if(language STREQUAL "cpp" OR language STREQUAL "c")
    serializer_example_runtime_dlls("${target}")
    if(MSVC)
      target_compile_options("${target}" PRIVATE /W4 /WX /utf-8)
    else()
      target_compile_options("${target}" PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    list(APPEND verify_command --executable "$<TARGET_FILE:${target}>")
  endif()
  add_test(NAME "${target}" COMMAND ${verify_command})
endfunction()
