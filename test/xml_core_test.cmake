# Qualify the pinned published Core schema through import, generation, compilation and execution.
set(core_fixture_directory "${CMAKE_CURRENT_SOURCE_DIR}/resources/3mf_core_1_4")
set(core_generated_directory "${CMAKE_CURRENT_BINARY_DIR}/xml_core_generated")
set(core_generated_schema "${core_generated_directory}/core.serializer")
set(core_generated_header "${core_generated_directory}/core.hpp")
add_custom_command(OUTPUT "${core_generated_schema}"
  COMMAND "${CMAKE_COMMAND}" "-DCOMPILER=$<TARGET_FILE:serializer>"
    "-DBINARY_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}"
    "-DSOURCE_XSD=${core_fixture_directory}/core.xsd"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/import_core_schema.cmake"
  DEPENDS serializer "${core_fixture_directory}/core.xsd" "${core_fixture_directory}/xml.xsd"
    "${CMAKE_CURRENT_SOURCE_DIR}/import_core_schema.cmake"
  DEPFILE "${core_generated_schema}.d"
  VERBATIM)
add_custom_command(OUTPUT "${core_generated_header}"
  COMMAND "$<TARGET_FILE:serializer>" --input "${core_generated_schema}"
    --language cpp --output "${core_generated_header}"
    --config "${core_fixture_directory}/codegen.ini" --include-mode inline
    --depfile "${core_generated_header}.d"
  DEPENDS serializer "${core_generated_schema}" "${core_fixture_directory}/core.xsd"
    "${core_fixture_directory}/xml.xsd" "${core_fixture_directory}/codegen.ini"
  DEPFILE "${core_generated_header}.d"
  VERBATIM)
add_executable(xml_core_generated_test xml_core_generated_test.cpp "${core_generated_header}")
target_include_directories(xml_core_generated_test PRIVATE "${core_generated_directory}")
target_compile_definitions(xml_core_generated_test PRIVATE
  "SERIALIZER_CORE_MODEL_FIXTURE=\"${core_fixture_directory}/minimal.model\"")
target_link_libraries(xml_core_generated_test PRIVATE Serializer::runtime GTest::gtest_main)
target_compile_features(xml_core_generated_test PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(xml_core_generated_test PRIVATE /W4 /WX)
else()
  target_compile_options(xml_core_generated_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME xml_core_generated_test COMMAND xml_core_generated_test)
