include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/serializer_example_dependency.cmake")
enable_testing()


# Compile a native consumer with the selected CMake toolchain and generated entry filename.
function(serializer_special_native_consumer target language schema source)
  if(language STREQUAL "cpp")
    set(output "${CMAKE_CURRENT_BINARY_DIR}/generated/${target}/message.hpp")
    add_executable("${target}" "${source}/main.cpp")
    serializer_example_runtime_dlls("${target}")
    target_link_libraries("${target}" PRIVATE Serializer::runtime)
  else()
    set(output "${CMAKE_CURRENT_BINARY_DIR}/generated/${target}/schema.h")
    add_executable("${target}" "${source}/main.c")
    serializer_example_runtime_dlls("${target}")
    set_target_properties("${target}" PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    if(MSVC)
      target_compile_definitions("${target}" PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
      target_link_libraries("${target}" PRIVATE m)
    endif()
  endif()
  serializer_generate_source(TARGET "${target}_schema" SCHEMA "${schema}"
    OUTPUT "${output}" LANGUAGE "${language}" OPTIONS --cpp.format=false)
  add_dependencies("${target}" "${target}_schema")
  get_filename_component(generated_directory "${output}" DIRECTORY)
  target_include_directories("${target}" PRIVATE "${generated_directory}")
  if(MSVC)
    target_compile_options("${target}" PRIVATE /W4 /WX /utf-8)
  else()
    target_compile_options("${target}" PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endfunction()

# Build one custom-ID registry consumer using its maintained schema and SDK checks.
function(serializer_add_custom_id_example language)
  serializer_example_dependency()
  enable_testing()
  get_filename_component(directory "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../custom_ids" ABSOLUTE)
  set(target "serializer_custom_ids_${language}")
  if(language STREQUAL "cpp" OR language STREQUAL "c")
    serializer_special_native_consumer("${target}" "${language}"
      "${directory}/model.serializer" "${directory}/${language}")
    add_test(NAME "${target}" COMMAND "${target}")
  else()
    serializer_example_sdk("${language}")
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    set(command "${Python3_EXECUTABLE}" "${directory}/run.py"
      --compiler "$<TARGET_FILE:Serializer::serializer>"
      --build "${CMAKE_CURRENT_BINARY_DIR}/verification" --language "${language}"
      "--wsl-languages=${SERIALIZER_EXAMPLE_WSL_LANGUAGES}")
    add_custom_target("${target}" ALL COMMAND ${command}
      DEPENDS Serializer::serializer VERBATIM)
    add_test(NAME "${target}" COMMAND ${command})
  endif()
endfunction()

# Generate one managed wire fixture and exercise it through a selected language codec.
function(serializer_add_managed_wire_example feature language)
  serializer_example_dependency()
  enable_testing()
  serializer_example_sdk("${language}")
  find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
  get_filename_component(directory "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../managed/multilanguage" ABSOLUTE)
  set(target "serializer_managed_wire_${feature}_${language}")
  set(command "${Python3_EXECUTABLE}" "${directory}/run.py"
    --compiler "$<TARGET_FILE:Serializer::serializer>"
    --build "${CMAKE_CURRENT_BINARY_DIR}/verification/${language}"
    --language "${language}" --feature "${feature}")
  if(language STREQUAL "cpp" OR language STREQUAL "c")
    serializer_special_native_consumer("${target}" "${language}"
      "${directory}/${feature}/model.serializer" "${directory}/clients/${language}")
    list(APPEND command --executable "$<TARGET_FILE:${target}>")
  else()
    list(APPEND command "--wsl-languages=${SERIALIZER_EXAMPLE_WSL_LANGUAGES}")
    add_custom_target("${target}" ALL COMMAND ${command}
      DEPENDS Serializer::serializer VERBATIM)
  endif()
  add_test(NAME "${target}" COMMAND ${command})
endfunction()
