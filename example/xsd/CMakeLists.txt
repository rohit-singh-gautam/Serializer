cmake_minimum_required(VERSION 4.0)
project(serializer_xsd_examples LANGUAGES CXX)

set(SERIALIZER_SOURCE_DIRECTORY "" CACHE PATH "Optional Serializer source checkout; otherwise use an installed 2.7 package")
set(SERIALIZER_EXAMPLE_COMPILER "" CACHE FILEPATH "Optional prebuilt host compiler for source-based builds")
set(XSD_WORKFLOW both CACHE STRING "Build imported, manual, or both C++ XSD workflows")
set_property(CACHE XSD_WORKFLOW PROPERTY STRINGS both imported manual)
string(TOLOWER "${XSD_WORKFLOW}" selected_workflow)
if(NOT selected_workflow MATCHES "^(both|imported|manual)$")
  message(FATAL_ERROR "XSD_WORKFLOW must be imported, manual, or both")
endif()

if(NOT TARGET Serializer::runtime OR NOT TARGET Serializer::serializer)
  if(SERIALIZER_SOURCE_DIRECTORY)
    # A source consumer builds the compiler/runtime, without the repository's test suites.
    foreach(option_name IN ITEMS SERIALIZER_BUILD_TESTS SERIALIZER_BUILD_MANAGED SERIALIZER_INSTALL
        SERIALIZER_WITH_ZSTD SERIALIZER_WITH_LZ4 SERIALIZER_WITH_ZLIB)
      if(NOT DEFINED ${option_name})
        set(${option_name} OFF)
      endif()
    endforeach()
    if(NOT DEFINED SERIALIZER_WITH_XML)
      set(SERIALIZER_WITH_XML ON)
    endif()
    add_subdirectory("${SERIALIZER_SOURCE_DIRECTORY}" "${CMAKE_CURRENT_BINARY_DIR}/serializer" EXCLUDE_FROM_ALL)
  else()
    find_package(Serializer 2.7 CONFIG REQUIRED)
  endif()
endif()
get_target_property(serializer_xml_enabled Serializer::runtime SERIALIZER_WITH_XML)
if(NOT serializer_xml_enabled)
  message(FATAL_ERROR "These examples require a Serializer compiler/runtime built with SERIALIZER_WITH_XML=ON")
endif()

if(SERIALIZER_EXAMPLE_COMPILER)
  get_filename_component(compiler "${SERIALIZER_EXAMPLE_COMPILER}" ABSOLUTE)
  if(NOT EXISTS "${compiler}" OR IS_DIRECTORY "${compiler}")
    message(FATAL_ERROR "SERIALIZER_EXAMPLE_COMPILER must name an existing host executable")
  endif()
  set(compiler_dependency "${compiler}")
else()
  set(compiler "$<TARGET_FILE:Serializer::serializer>")
  set(compiler_dependency Serializer::serializer)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/serializer_example_dependency.cmake")
enable_testing()

# Compile one explicit application client and exercise its generated XML model.
function(add_xsd_client workflow)
  set(target "serializer_xsd_${workflow}")
  set(generated_directory "${CMAKE_CURRENT_BINARY_DIR}/generated/${workflow}")
  set(header "${generated_directory}/model.hpp")
  add_executable("${target}" "${workflow}/cpp/main.cpp")
  if(workflow STREQUAL imported)
    set(schema "${generated_directory}/model.serializer")
    set_source_files_properties("${schema}" PROPERTIES GENERATED TRUE)
    add_custom_command(OUTPUT "${schema}"
      COMMAND "${CMAKE_COMMAND}"
        "-DCOMPILER=${compiler}"
        "-DBINARY_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}"
        "-DSOURCE_XSD=${CMAKE_CURRENT_SOURCE_DIR}/order.xsd"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/import_generated_schema.cmake"
      DEPENDS ${compiler_dependency} "order.xsd" "cmake/import_generated_schema.cmake"
      DEPFILE "${schema}.d"
      COMMENT "Importing the fixed XSD into a build-owned Serializer model"
      VERBATIM)
    add_custom_command(OUTPUT "${header}"
      COMMAND "${compiler}" --input "${schema}" --language cpp --output "${header}"
        --config "${CMAKE_CURRENT_SOURCE_DIR}/codegen.ini" --include-mode inline --depfile "${header}.d"
      DEPENDS "${schema}" ${compiler_dependency} "order.xsd" "codegen.ini"
      DEPFILE "${header}.d"
      COMMENT "Generating the imported C++ XML codec"
      VERBATIM)
    target_sources("${target}" PRIVATE "${header}")
    target_include_directories("${target}" PRIVATE "${generated_directory}")
    target_link_libraries("${target}" PRIVATE Serializer::runtime)
  else()
    if(SERIALIZER_EXAMPLE_COMPILER)
      set(generator_argument GENERATOR "${compiler}")
    endif()
    serializer_generate(TARGET "${target}" SCHEMAS manual/model.serializer
      CONFIG codegen.ini OUTPUT_DIRECTORY "${generated_directory}" INCLUDE_MODE inline
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/order.xsd" ${generator_argument})
  endif()
  target_compile_features("${target}" PRIVATE cxx_std_23)
  if(MSVC)
    target_compile_options("${target}" PRIVATE /W4 /WX /utf-8)
  else()
    target_compile_options("${target}" PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  serializer_example_runtime_dlls("${target}")
  add_test(NAME "xsd_${workflow}_roundtrip" COMMAND "${target}"
    "${CMAKE_CURRENT_SOURCE_DIR}/order.xml" "${CMAKE_CURRENT_BINARY_DIR}/${workflow}.xml")
  set_tests_properties("xsd_${workflow}_roundtrip" PROPERTIES FIXTURES_SETUP "xsd_${workflow}")
endfunction()

if(selected_workflow STREQUAL both OR selected_workflow STREQUAL imported)
  add_xsd_client(imported)
endif()
if(selected_workflow STREQUAL both OR selected_workflow STREQUAL manual)
  add_xsd_client(manual)
endif()
if(selected_workflow STREQUAL both)
  add_test(NAME xsd_imported_reads_manual COMMAND serializer_xsd_imported
    "${CMAKE_CURRENT_BINARY_DIR}/manual.xml" "${CMAKE_CURRENT_BINARY_DIR}/manual-to-imported.xml")
  set_tests_properties(xsd_imported_reads_manual PROPERTIES FIXTURES_REQUIRED xsd_manual)
  add_test(NAME xsd_manual_reads_imported COMMAND serializer_xsd_manual
    "${CMAKE_CURRENT_BINARY_DIR}/imported.xml" "${CMAKE_CURRENT_BINARY_DIR}/imported-to-manual.xml")
  set_tests_properties(xsd_manual_reads_imported PROPERTIES FIXTURES_REQUIRED xsd_imported)
endif()
