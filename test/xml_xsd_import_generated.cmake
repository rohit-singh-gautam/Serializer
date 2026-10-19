# Qualify scalar/attribute/text and nested include import through native generation and roundtrip.
set(shared_xsd_fixture_directory "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_shared_import")
set(shared_xsd_generated_directory "${CMAKE_CURRENT_BINARY_DIR}/xml_shared_import_generated")
set(shared_xsd_generated_schema "${shared_xsd_generated_directory}/shared_import.serializer")
set(shared_xsd_generated_header "${shared_xsd_generated_directory}/shared_import.hpp")
add_custom_command(OUTPUT "${shared_xsd_generated_schema}"
  COMMAND "${CMAKE_COMMAND}" "-DCOMPILER=$<TARGET_FILE:serializer>"
    "-DBINARY_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}"
    "-DSOURCE_XSD=${shared_xsd_fixture_directory}/root.xsd"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/import_shared_xsd_schema.cmake"
  DEPENDS serializer "${shared_xsd_fixture_directory}/root.xsd" "${shared_xsd_fixture_directory}/types.xsd"
    "${CMAKE_CURRENT_SOURCE_DIR}/import_shared_xsd_schema.cmake"
  DEPFILE "${shared_xsd_generated_schema}.d"
  VERBATIM)
add_custom_command(OUTPUT "${shared_xsd_generated_header}"
  COMMAND "$<TARGET_FILE:serializer>" --input "${shared_xsd_generated_schema}"
    --language cpp --output "${shared_xsd_generated_header}" --cpp.format false --include-mode inline
    --depfile "${shared_xsd_generated_header}.d"
  DEPENDS serializer "${shared_xsd_generated_schema}" "${shared_xsd_fixture_directory}/root.xsd"
    "${shared_xsd_fixture_directory}/types.xsd"
  DEPFILE "${shared_xsd_generated_header}.d"
  VERBATIM)
add_executable(xml_xsd_import_generated_test xml_xsd_import_generated_test.cpp "${shared_xsd_generated_header}")
target_include_directories(xml_xsd_import_generated_test PRIVATE "${shared_xsd_generated_directory}")
target_link_libraries(xml_xsd_import_generated_test PRIVATE Serializer::runtime GTest::gtest_main)
target_compile_features(xml_xsd_import_generated_test PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(xml_xsd_import_generated_test PRIVATE /W4 /WX)
else()
  target_compile_options(xml_xsd_import_generated_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME xml_xsd_import_generated_test COMMAND xml_xsd_import_generated_test)
