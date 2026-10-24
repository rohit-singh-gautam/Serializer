set(schemas)
foreach(fixture IN ITEMS enum person test1 array map variable assessment views storage_reuse
    fixed_fields constant_names includes wire_compatibility generics dimensions versioning magic typed_magic inferred_arrays compact_fields digests owning_variants owning_variant_cpp optional_cpp native_output_policy reuse_first reuse_second construction scalar_defaults extended_construction overloaded_construction readonly_behavior derived_sizes conversion reference_fields stateless_layout generic_constraints readonly_function_calls readonly_function_overloads numeric_values generic_calls)
  list(APPEND schemas "${CMAKE_CURRENT_SOURCE_DIR}/resources/${fixture}.serializer")
endforeach()

add_executable(core_serializer_test
  core_serializer_test.cpp
  behavior_test.cpp
  numeric_test.cpp
  readonly_behavior_test.cpp
  readonly_function_calls_test.cpp
  generic_calls_test.cpp
  construction_test.cpp
  scalar_default_test.cpp
  extended_construction_test.cpp
  overloaded_construction_test.cpp
  auto_size_test.cpp
  conversion_test.cpp
  reference_schema_test.cpp
  reference_runtime_test.cpp
  stateless_constraints_test.cpp
  generic_constraints_test.cpp
  native_binary_output_policy_test.cpp
  digest_test.cpp
  digest_schema_test.cpp
  digest_codec_test.cpp
  optional_schema_test.cpp
  optional_codec_test.cpp
  command_line_test.cpp
  stream_boundary_test.cpp
  string_stream_test.cpp
  deserialize_exact_test.cpp
  compression_test.cpp
  stream_concepts_test.cpp
  file_stream_test.cpp
  serializer_parser_test.cpp
  literal_syntax_test.cpp
  schema_version_test.cpp
  simplicity_warning_test.cpp
  versioning_test.cpp
  magic_test.cpp
  typed_magic_test.cpp
  inferred_array_test.cpp
  compact_fields_test.cpp
  schema_policy_test.cpp
  schema_generics_test.cpp
  dimension_templates_test.cpp
  managed_schema_test.cpp
  managed_lowering_test.cpp
  schema_compatibility_test.cpp
  schema_include_test.cpp
  schema_scan_test.cpp
  runtime_simd_test.cpp
  utf8_validation_test.cpp
  json_scan_reuse_test.cpp
  json_compatibility_test.cpp
  json_whitespace_scan_test.cpp
  binary_array_decode_test.cpp
  nested_storage_reuse_test.cpp
  fixed_field_batch_test.cpp
  constant_field_name_test.cpp
  generated_test.cpp
  owning_variants_test.cpp
  generated_notice_test.cpp
  wire_format_test.cpp
  binary_text_test.cpp
  wire_compatibility_test.cpp
  decoder_qualification_test.cpp
  binary_view_test.cpp
  binary_view_iterator_test.cpp
  output_options_test.cpp
  java_writer_test.cpp
  portable_writer_test.cpp
)
serializer_generate(TARGET core_serializer_test
  SCHEMAS ${schemas}
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
serializer_generate_source(TARGET include_separation_generated
  SCHEMA resources/include_separation.serializer LANGUAGE cpp
  CONFIG resources/include_separation.ini
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/include_separation.hpp")
add_dependencies(core_serializer_test include_separation_generated)
target_link_libraries(core_serializer_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
target_compile_definitions(core_serializer_test PRIVATE ROHIT_SERIALIZER_ENABLE_GTEST)
if(MSVC)
  target_compile_options(core_serializer_test PRIVATE /W4 /WX)
elseif(UNIX)
  target_compile_options(core_serializer_test PRIVATE -Wall -Werror -Wpedantic -Wextra -Weffc++ -Wsuggest-override)
endif()

add_test(NAME core_serializer_test COMMAND core_serializer_test)

if(SERIALIZER_BUILD_MANAGED)
  add_executable(cache_attributes_test cache_attributes_test.cpp)
  serializer_generate(TARGET cache_attributes_test SCHEMAS resources/cache_attributes.serializer
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/cache_attributes")
  target_link_libraries(cache_attributes_test PRIVATE Serializer::serializer_lib Serializer::managed GTest::gtest_main)
  if(MSVC)
    target_compile_options(cache_attributes_test PRIVATE /W4 /WX)
  else()
    target_compile_options(cache_attributes_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME cache_attributes_test COMMAND cache_attributes_test)
  foreach(cache_profile IN ITEMS direct64 separate32 separate64)
    add_executable(cache_attributes_${cache_profile}_test cache_attributes_test.cpp)
    serializer_generate(TARGET cache_attributes_${cache_profile}_test
      SCHEMAS resources/cache_attributes.serializer
      CONFIG "resources/cache_${cache_profile}.ini"
      OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/cache_${cache_profile}")
    target_link_libraries(cache_attributes_${cache_profile}_test PRIVATE
      Serializer::serializer_lib Serializer::managed GTest::gtest_main)
    if(MSVC)
      target_compile_options(cache_attributes_${cache_profile}_test PRIVATE /W4 /WX)
    else()
      target_compile_options(cache_attributes_${cache_profile}_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME cache_attributes_${cache_profile}_test COMMAND cache_attributes_${cache_profile}_test)
  endforeach()
endif()

add_executable(xml_event_test xml_event_test.cpp)
target_link_libraries(xml_event_test PRIVATE Serializer::runtime GTest::gtest_main)
if(MSVC)
  target_compile_options(xml_event_test PRIVATE /W4 /WX)
else()
  target_compile_options(xml_event_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME xml_event_test COMMAND xml_event_test)

add_executable(xml_cpp23_test xml_cpp23_test.cpp)
set_target_properties(xml_cpp23_test PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES)
target_link_libraries(xml_cpp23_test PRIVATE Serializer::runtime GTest::gtest_main)
if(MSVC)
  target_compile_options(xml_cpp23_test PRIVATE /W4 /WX)
else()
  target_compile_options(xml_cpp23_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME xml_cpp23_test COMMAND xml_cpp23_test)

add_executable(xml_event_reader_test xml_event_reader_test.cpp)
target_link_libraries(xml_event_reader_test PRIVATE Serializer::runtime GTest::gtest_main)
target_compile_definitions(xml_event_reader_test PRIVATE
  SERIALIZER_TEST_HAS_XML=$<BOOL:${SERIALIZER_WITH_XML}>)
if(SERIALIZER_WITH_XML)
  target_link_libraries(xml_event_reader_test PRIVATE LibXml2::LibXml2)
endif()
if(MSVC)
  target_compile_options(xml_event_reader_test PRIVATE /W4 /WX)
else()
  target_compile_options(xml_event_reader_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME xml_event_reader_test COMMAND xml_event_reader_test)

add_executable(xml_container_contract_test xml_container_contract_test.cpp)
target_link_libraries(xml_container_contract_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
if(MSVC)
  target_compile_options(xml_container_contract_test PRIVATE /W4 /WX)
else()
  target_compile_options(xml_container_contract_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME xml_container_contract_test COMMAND xml_container_contract_test)

add_executable(xml_content_contract_test xml_content_contract_test.cpp)
target_link_libraries(xml_content_contract_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
if(MSVC)
  target_compile_options(xml_content_contract_test PRIVATE /W4 /WX)
else()
  target_compile_options(xml_content_contract_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME xml_content_contract_test COMMAND xml_content_contract_test)

add_executable(xml_extension_slot_test xml_extension_slot_test.cpp)
target_link_libraries(xml_extension_slot_test PRIVATE Serializer::runtime GTest::gtest_main)
if(MSVC)
  target_compile_options(xml_extension_slot_test PRIVATE /W4 /WX)
else()
  target_compile_options(xml_extension_slot_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME xml_extension_slot_test COMMAND xml_extension_slot_test)

foreach(contract_test IN ITEMS xml_pattern_test xml_scalar_contract_test xml_model_contract_test
    document_profile_test document_profile_contract_test html_test xml_identity_test xml_identity_contract_test)
  if(contract_test MATCHES "^(document_profile_contract|xml_identity_contract)_test$" AND NOT SERIALIZER_WITH_XML)
    continue()
  endif()
  add_executable(${contract_test} "${contract_test}.cpp")
  if(contract_test MATCHES "^(xml_pattern|xml_identity|document_profile|html)_test$")
    target_link_libraries(${contract_test} PRIVATE Serializer::runtime GTest::gtest_main)
  else()
    target_link_libraries(${contract_test} PRIVATE Serializer::serializer_lib GTest::gtest_main)
    target_include_directories(${contract_test} PRIVATE "${PROJECT_SOURCE_DIR}/src")
  endif()
  if(MSVC)
    target_compile_options(${contract_test} PRIVATE /W4 /WX)
  else()
    target_compile_options(${contract_test} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME ${contract_test} COMMAND ${contract_test})
endforeach()

add_executable(xml_authoring_parser_test xml_authoring_parser_test.cpp)
target_link_libraries(xml_authoring_parser_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
if(MSVC)
  target_compile_options(xml_authoring_parser_test PRIVATE /W4 /WX)
else()
  target_compile_options(xml_authoring_parser_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME xml_authoring_parser_test COMMAND xml_authoring_parser_test)

add_executable(managed_reference_parser_test managed_reference_parser_test.cpp)
target_link_libraries(managed_reference_parser_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
if(MSVC)
  target_compile_options(managed_reference_parser_test PRIVATE /W4 /WX)
else()
  target_compile_options(managed_reference_parser_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME managed_reference_parser_test COMMAND managed_reference_parser_test)

add_executable(managed_reference_codegen_test managed_reference_codegen_test.cpp)
target_link_libraries(managed_reference_codegen_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
if(MSVC)
  target_compile_options(managed_reference_codegen_test PRIVATE /W4 /WX)
else()
  target_compile_options(managed_reference_codegen_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME managed_reference_codegen_test COMMAND managed_reference_codegen_test)

if(SERIALIZER_WITH_XML)
  include("${CMAKE_CURRENT_SOURCE_DIR}/xml_core_test.cmake")
  include("${CMAKE_CURRENT_SOURCE_DIR}/xml_xsd_import_generated.cmake")
  add_executable(xml_xsd_contract_test xml_xsd_contract_test.cpp)
  target_link_libraries(xml_xsd_contract_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
  if(MSVC)
    target_compile_options(xml_xsd_contract_test PRIVATE /W4 /WX)
  else()
    target_compile_options(xml_xsd_contract_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME xml_xsd_contract_test COMMAND xml_xsd_contract_test)
  add_executable(xml_shared_contract_test xml_shared_contract_test.cpp)
  target_link_libraries(xml_shared_contract_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
  if(MSVC)
    target_compile_options(xml_shared_contract_test PRIVATE /W4 /WX)
  else()
    target_compile_options(xml_shared_contract_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME xml_shared_contract_test COMMAND xml_shared_contract_test)
  add_executable(xsd_import_test xsd_import_test.cpp)
  target_link_libraries(xsd_import_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
  add_executable(xml_operation_test xml_operation_test.cpp xml_contract_test.cpp)
  target_link_libraries(xml_operation_test PRIVATE Serializer::runtime GTest::gtest_main)
  foreach(opc_test IN ITEMS xsd_import_test xml_operation_test)
    if(MSVC)
      target_compile_options(${opc_test} PRIVATE /W4 /WX)
    else()
      target_compile_options(${opc_test} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME ${opc_test} COMMAND ${opc_test})
  endforeach()
endif()

add_executable(reciprocal_constructors_test reciprocal_constructors_test.cpp)
serializer_generate(TARGET reciprocal_constructors_test
  SCHEMAS resources/reciprocal_constructors.serializer
  CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/reciprocal_constructors.ini"
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/reciprocal_constructors")
target_link_libraries(reciprocal_constructors_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
if(MSVC)
  target_compile_options(reciprocal_constructors_test PRIVATE /W4 /WX)
else()
  target_compile_options(reciprocal_constructors_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME reciprocal_constructors_test COMMAND reciprocal_constructors_test)

add_executable(array_exposure_test array_exposure_test.cpp)
serializer_generate(TARGET array_exposure_test
  SCHEMAS resources/array_exposure.serializer resources/array_generation.serializer
  CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/array_exposure.ini"
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/array_exposure")
target_link_libraries(array_exposure_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
if(MSVC)
  target_compile_options(array_exposure_test PRIVATE /W4 /WX)
else()
  target_compile_options(array_exposure_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME array_exposure_test COMMAND array_exposure_test)

add_executable(arithmetic_operators_test arithmetic_operators_test.cpp)
serializer_generate(TARGET arithmetic_operators_test SCHEMAS resources/arithmetic_operators.serializer
  CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/arithmetic_operators.ini"
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/arithmetic_operators")
target_link_libraries(arithmetic_operators_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
if(MSVC)
  target_compile_options(arithmetic_operators_test PRIVATE /W4 /WX)
else()
  target_compile_options(arithmetic_operators_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME arithmetic_operators_test COMMAND arithmetic_operators_test)

add_executable(constructor_initialization_test constructor_initialization_test.cpp)
serializer_generate(TARGET constructor_initialization_test
  SCHEMAS resources/generic_parameter_defaults.serializer
  CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/authoring_construction.ini"
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/constructor_initialization")
target_link_libraries(constructor_initialization_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
if(MSVC)
  target_compile_options(constructor_initialization_test PRIVATE /W4 /WX)
else()
  target_compile_options(constructor_initialization_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME constructor_initialization_test COMMAND constructor_initialization_test)

add_executable(native_callable_test native_callable_test.cpp)
serializer_generate(TARGET native_callable_test SCHEMAS resources/native_callables.serializer
  CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/authoring_construction.ini"
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/native_callables")
target_link_libraries(native_callable_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
set_target_properties(native_callable_test PROPERTIES CXX_STANDARD 23)
if(MSVC)
  target_compile_options(native_callable_test PRIVATE /W4 /WX)
else()
  target_compile_options(native_callable_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME native_callable_test COMMAND native_callable_test)

if(SERIALIZER_BUILD_MANAGED)
  add_executable(managed_identity_allocation_test managed_identity_allocation_test.cpp)
  target_link_libraries(managed_identity_allocation_test PRIVATE Serializer::managed GTest::gtest_main)
  if(MSVC)
    target_compile_options(managed_identity_allocation_test PRIVATE /W4 /WX)
  else()
    target_compile_options(managed_identity_allocation_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME managed_identity_allocation_test COMMAND managed_identity_allocation_test)
  add_executable(readonly_getters_test readonly_getters_test.cpp)
  serializer_generate(TARGET readonly_getters_test
    SCHEMAS resources/readonly_getters.serializer
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/readonly_getters.ini"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/readonly_getters")
  target_link_libraries(readonly_getters_test PRIVATE Serializer::serializer_lib Serializer::managed GTest::gtest_main)
  if(MSVC)
    target_compile_options(readonly_getters_test PRIVATE /W4 /WX)
  else()
    target_compile_options(readonly_getters_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME readonly_getters_test COMMAND readonly_getters_test)
endif()

# Preserve authored C++ spellings while qualifying native construction and readonly calls.
add_executable(authoring_construction_test
  authoring_construction_test.cpp
  hybrid_construction_test.cpp)
serializer_generate(TARGET authoring_construction_test
  SCHEMAS resources/authoring_construction.serializer resources/hybrid_construction.serializer
  CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/authoring_construction.ini"
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/authoring_construction")
target_link_libraries(authoring_construction_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
target_compile_definitions(authoring_construction_test PRIVATE ROHIT_SERIALIZER_ENABLE_GTEST)
if(MSVC)
  target_compile_options(authoring_construction_test PRIVATE /W4 /WX)
elseif(UNIX)
  target_compile_options(authoring_construction_test PRIVATE -Wall -Werror -Wpedantic -Wextra -Weffc++ -Wsuggest-override)
endif()
add_test(NAME authoring_construction_test COMMAND authoring_construction_test)

# Compile explicit native/external exception contracts through the actual schema pipeline.
add_executable(cpp_noexcept_test cpp_noexcept_test.cpp)
serializer_generate(TARGET cpp_noexcept_test
  SCHEMAS resources/cpp_noexcept.serializer
  CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/authoring_construction.ini"
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/cpp_noexcept")
target_link_libraries(cpp_noexcept_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
if(MSVC)
  target_compile_options(cpp_noexcept_test PRIVATE /W4 /WX)
else()
  target_compile_options(cpp_noexcept_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME cpp_noexcept_test COMMAND cpp_noexcept_test)

# Qualify opt-in record comparisons through the same schema and INI pipeline as user builds.
add_executable(record_comparison_test record_comparison_test.cpp)
serializer_generate(TARGET record_comparison_test
  SCHEMAS resources/record_comparison.serializer
  CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/record_comparison.ini"
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/record_comparison")
target_link_libraries(record_comparison_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
if(MSVC)
  target_compile_options(record_comparison_test PRIVATE /W4 /WX)
else()
  target_compile_options(record_comparison_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME record_comparison_test COMMAND record_comparison_test)

# Qualify nested generic record keys through the owning C++ codec pipeline.
add_executable(record_map_key_test record_map_key_test.cpp)
serializer_generate(TARGET record_map_key_test
  SCHEMAS resources/record_maps.serializer
  CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/record_maps.ini"
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/record_maps")
target_link_libraries(record_map_key_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
if(MSVC)
  target_compile_options(record_map_key_test PRIVATE /W4 /WX)
else()
  target_compile_options(record_map_key_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME record_map_key_test COMMAND record_map_key_test)

# Compare native owning variants with independently generated JavaScript and TypeScript models.
find_program(serializer_variant_node NAMES node)
if(serializer_variant_node)
  add_executable(owning_variants_portable_fixture owning_variants_portable_fixture.cpp)
  serializer_generate(TARGET owning_variants_portable_fixture
    SCHEMAS resources/owning_variants.serializer
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/owning_variants_native")
  target_link_libraries(owning_variants_portable_fixture PRIVATE Serializer::serializer_lib)
  if(MSVC)
    target_compile_options(owning_variants_portable_fixture PRIVATE /W4 /WX)
  elseif(UNIX)
    target_compile_options(owning_variants_portable_fixture PRIVATE -Wall -Werror -Wpedantic -Wextra -Weffc++)
  endif()
  set(serializer_variant_portable_directory "${CMAKE_CURRENT_BINARY_DIR}/owning_variants_portable")
  add_test(NAME serializer_owning_variants_portable
    COMMAND "${serializer_variant_node}" "${CMAKE_CURRENT_SOURCE_DIR}/owning_variant_portable_test.mjs"
      "$<TARGET_FILE:serializer>" "$<TARGET_FILE:owning_variants_portable_fixture>"
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/owning_variants.serializer"
      "${serializer_variant_portable_directory}")
  find_program(serializer_variant_tsc NAMES tsc tsc.cmd)
  if(serializer_variant_tsc)
    add_test(NAME serializer_owning_variants_typescript
      COMMAND "${serializer_variant_tsc}" --strict --noEmit --target ES2020
        --module NodeNext --moduleResolution NodeNext
        "${serializer_variant_portable_directory}/consumer.mts")
    set_tests_properties(serializer_owning_variants_typescript PROPERTIES
      DEPENDS serializer_owning_variants_portable)
  endif()
endif()

# Qualify constexpr calls through a real opt-in generated model and the existing runtime consumer.
add_executable(constant_binary_test constant_binary_test.cpp)
serializer_generate(TARGET constant_binary_test
  SCHEMAS resources/constant_evaluation.serializer
  CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/constant_evaluation.ini"
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/constant_evaluation")
target_link_libraries(constant_binary_test PRIVATE GTest::gtest_main)
if(MSVC)
  target_compile_options(constant_binary_test PRIVATE /W4 /WX)
elseif(UNIX)
  target_compile_options(constant_binary_test PRIVATE -Wall -Werror -Wpedantic -Wextra -Weffc++)
endif()
add_test(NAME constant_binary_test COMMAND constant_binary_test)

# Qualify ordinary generated native codecs with a typed borrowed constexpr stream.
add_executable(constexpr_codecs_test constexpr_codecs_test.cpp)
serializer_generate(TARGET constexpr_codecs_test
  SCHEMAS resources/constexpr_codecs.serializer
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/constexpr_codecs")
target_link_libraries(constexpr_codecs_test PRIVATE Serializer::runtime GTest::gtest_main)
if(MSVC)
  target_compile_options(constexpr_codecs_test PRIVATE /W4 /WX)
else()
  target_compile_options(constexpr_codecs_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME constexpr_codecs_test COMMAND constexpr_codecs_test)

# Derive nested size headers through the same owning and borrowed compile-time output pipeline.
add_executable(auto_size_constant_test auto_size_constant_test.cpp)
serializer_generate(TARGET auto_size_constant_test
  SCHEMAS resources/auto_size_constant.serializer
  CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/auto_size_constant.ini"
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/auto_size_constant/owning")
add_library(auto_size_constant_emission INTERFACE)
serializer_generate(TARGET auto_size_constant_emission
  SCHEMAS resources/auto_size_constant.serializer
  CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/auto_size_emission.ini"
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/auto_size_constant/emission")
target_include_directories(auto_size_constant_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(auto_size_constant_test PRIVATE auto_size_constant_emission Serializer::runtime GTest::gtest_main)
if(MSVC)
  target_compile_options(auto_size_constant_test PRIVATE /W4 /WX)
else()
  target_compile_options(auto_size_constant_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME auto_size_constant_test COMMAND auto_size_constant_test)
add_test(NAME constant_binary_negative
  COMMAND "${CMAKE_COMMAND}"
    "-DCOMPILER=${CMAKE_CXX_COMPILER}"
    "-DMSVC_LIKE=${MSVC}"
    "-DSOURCE_DIRECTORY=${PROJECT_SOURCE_DIR}"
    "-DGENERATED_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/constant_evaluation"
    "-DWORK_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/constant_evaluation_negative"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/constant_binary_negative.cmake")

# Generate coexisting borrowed emission models and their unchanged owning runtime consumer.
add_executable(emission_only_test emission_only_test.cpp)
serializer_generate(TARGET emission_only_test
  SCHEMAS resources/emission_only.serializer
  CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/emission_only.ini"
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/emission_only/emission")
add_library(emission_only_owning INTERFACE)
serializer_generate(TARGET emission_only_owning
  SCHEMAS resources/emission_only.serializer
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/emission_only/owning")
target_include_directories(emission_only_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(emission_only_test PRIVATE emission_only_owning GTest::gtest_main)
if(MSVC)
  target_compile_options(emission_only_test PRIVATE /W4 /WX)
elseif(UNIX)
  target_compile_options(emission_only_test PRIVATE -Wall -Werror -Wpedantic -Wextra -Weffc++)
endif()
add_test(NAME emission_only_test COMMAND emission_only_test)
add_test(NAME emission_only_negative
  COMMAND "${CMAKE_COMMAND}"
    "-DCOMPILER=${CMAKE_CXX_COMPILER}"
    "-DMSVC_LIKE=${MSVC}"
    "-DSOURCE_DIRECTORY=${PROJECT_SOURCE_DIR}"
    "-DGENERATED_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/emission_only/emission"
    "-DWORK_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/emission_only_negative"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/emission_only_negative.cmake")

foreach(profile IN ITEMS serializer core google llvm gnu cert misra autosar qt)
  set(matrix_type matrix)
  set(point_type point)
  set(frame_type frame)
  set(message_type message)
  set(elements_field elements)
  set(response_field response)
  set(result_field result)
  set(value_field value)
  if(profile MATCHES "^(google|llvm|qt)$")
    set(matrix_type Matrix)
    set(point_type Point)
    set(frame_type Frame)
    set(message_type Message)
  endif()
  if(profile STREQUAL "google")
    set(elements_field elements_)
    set(response_field response_)
    set(result_field result_)
    set(value_field value_)
  elseif(profile STREQUAL "llvm")
    set(elements_field Elements)
    set(response_field Response)
    set(result_field Result)
    set(value_field Value)
  endif()
  set(profile_directory "${CMAKE_CURRENT_BINARY_DIR}/dimension_profile_${profile}")
  configure_file(dimension_profile_test.cpp.in "${profile_directory}/dimension_profile_test.cpp" @ONLY)
  add_executable(dimension_profile_${profile}_test "${profile_directory}/dimension_profile_test.cpp")
  set_target_properties(dimension_profile_${profile}_test PROPERTIES CXX_STANDARD 23)
  serializer_generate(TARGET dimension_profile_${profile}_test
    SCHEMAS resources/generics.serializer resources/dimensions.serializer
    CODING_STANDARD ${profile} OUTPUT_DIRECTORY "${profile_directory}")
  add_test(NAME dimension_profile_${profile}_test COMMAND dimension_profile_${profile}_test)
endforeach()
find_package(Python3 3.10 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME portable_default_validation_test
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/portable_default_validation_test.py"
      --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_default_validation")
  add_test(NAME backend_default_diagnostics_test
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/backend_default_diagnostics_test.py"
      --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/backend_default_diagnostics")
  if(CMAKE_GENERATOR MATCHES "Ninja")
    set(serializer_publication_ninja "${CMAKE_MAKE_PROGRAM}")
  else()
    find_program(serializer_publication_ninja NAMES ninja)
  endif()
  if(serializer_publication_ninja)
    add_test(NAME serializer_generated_output_publication
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/generated_output_publication_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/generated_output_publication"
        --ninja "${serializer_publication_ninja}" --with-xml "$<BOOL:${SERIALIZER_WITH_XML}>")
  endif()
  add_test(NAME serializer_compression_python
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/python_compression_test.py"
      --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/python_compression")
  if(MSVC)
    set(serializer_compression_c_compiler "${CMAKE_CXX_COMPILER}")
  else()
    find_program(serializer_compression_c_compiler NAMES cc gcc clang)
  endif()
  if(serializer_compression_c_compiler AND SERIALIZER_WITH_ZLIB)
    add_test(NAME serializer_compression_c
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/c_compression_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/c_compression"
        --cc "${serializer_compression_c_compiler}" --zlib-include "${ZLIB_INCLUDE_DIR}"
        --zlib-library "$<TARGET_FILE:ZLIB::ZLIB>")
  endif()
  find_program(serializer_compression_go NAMES go)
  if(serializer_compression_go)
    add_test(NAME serializer_compression_go
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/go_compression_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/go_compression"
        --go "${serializer_compression_go}")
    set(serializer_go_package_native_args --native-producer "$<TARGET_FILE:package_interop>")
    if(SERIALIZER_WITH_ZLIB)
      list(APPEND serializer_go_package_native_args --cpp-zip64 "$<TARGET_FILE:package_zip64_probe>")
    endif()
    add_test(NAME serializer_package_go
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/go_package_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/go_package"
        --go "${serializer_compression_go}" ${serializer_go_package_native_args})
    set(serializer_go_opc_native_args)
    if(SERIALIZER_WITH_XML)
      list(APPEND serializer_go_opc_native_args --native-producer "$<TARGET_FILE:package_interop>")
    endif()
    add_test(NAME serializer_package_go_opc
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/go_package_opc_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/go_package_opc"
        --go "${serializer_compression_go}" ${serializer_go_opc_native_args})
  endif()
  find_program(serializer_compression_dotnet NAMES dotnet)
  set(serializer_mobile_package_native_args)
  if(SERIALIZER_WITH_XML AND SERIALIZER_WITH_ZLIB)
    list(APPEND serializer_mobile_package_native_args --cpp-example "$<TARGET_FILE:serializer_opc_example>")
  endif()
  find_program(serializer_compression_kotlinc NAMES kotlinc kotlinc.bat)
  if(serializer_compression_kotlinc)
    add_test(NAME serializer_compression_kotlin
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/kotlin_compression_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/kotlin_compression"
        --kotlinc "${serializer_compression_kotlinc}")
    add_test(NAME serializer_package_kotlin
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/kotlin_package_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/kotlin_package"
        --kotlinc "${serializer_compression_kotlinc}" ${serializer_mobile_package_native_args})
  endif()
  find_program(serializer_compression_swiftc NAMES swiftc)
  if(serializer_compression_swiftc)
    add_test(NAME serializer_compression_swift
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/swift_compression_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/swift_compression"
        --swiftc "${serializer_compression_swiftc}")
  endif()
    find_program(serializer_compression_rustc NAMES rustc)
    if(serializer_compression_rustc)
      add_test(NAME serializer_compression_rust
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/rust_compression_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/rust_compression"
          --rustc "${serializer_compression_rustc}")
    endif()
  find_program(serializer_compression_node NAMES node)
  find_file(serializer_compression_typescript_cli NAMES tsc
    PATHS "${CMAKE_SOURCE_DIR}/editors/vscode/node_modules/typescript/bin" NO_DEFAULT_PATH)
  if(serializer_compression_node AND serializer_compression_typescript_cli)
    add_test(NAME serializer_compression_javascript
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/javascript_compression_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/javascript_compression"
        --node "${serializer_compression_node}" --tsc "${serializer_compression_typescript_cli}")
  endif()
  if(serializer_compression_dotnet)
    add_test(NAME serializer_compression_csharp
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/csharp_compression_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/csharp_compression"
        --dotnet "${serializer_compression_dotnet}")
    add_test(NAME serializer_package_csharp
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/csharp_package_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/csharp_package"
        --dotnet "${serializer_compression_dotnet}" ${serializer_mobile_package_native_args})
  endif()
  find_package(Java 17 COMPONENTS Runtime Development QUIET)
  if(Java_FOUND)
    add_test(NAME serializer_java_output_buffer
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/java_output_buffer_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/java_output_buffer"
        --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}")
    add_test(NAME serializer_compression_java
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/java_compression_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/java_compression"
        --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}")
    add_test(NAME serializer_package_java
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/java_package_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/java_package"
        --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}")
  endif()
  add_test(NAME xml_fixture_manifest_test
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xml_fixture_manifest_test.py")
  add_test(NAME serializer_package_application_python
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/package_owning_contract_test.py"
      --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/package_application_python"
      --language python)
  if(serializer_compression_dotnet)
    add_test(NAME serializer_package_application_csharp
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/package_owning_contract_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/package_application_csharp"
        --language csharp --dotnet "${serializer_compression_dotnet}")
  endif()
  if(serializer_compression_kotlinc AND Java_FOUND)
    add_test(NAME serializer_package_application_kotlin
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/package_owning_contract_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/package_application_kotlin"
        --language kotlin --java "${Java_JAVA_EXECUTABLE}" --kotlinc "${serializer_compression_kotlinc}")
    add_test(NAME serializer_package_selected_codec_kotlin
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/package_selected_codec_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/package_selected_codec_kotlin"
        --language kotlin --java "${Java_JAVA_EXECUTABLE}" --kotlinc "${serializer_compression_kotlinc}")
  endif()
  if(Java_FOUND)
    foreach(package_suite IN ITEMS application_contract shared_codec_budget)
      add_test(NAME serializer_package_${package_suite}_java
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/package_${package_suite}_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/package_${package_suite}_java"
          --language java --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}")
    endforeach()
  endif()
  if(serializer_compression_go)
    foreach(package_suite IN ITEMS application_contract shared_codec_budget selected_codec)
      add_test(NAME serializer_package_${package_suite}_go
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/package_${package_suite}_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/package_${package_suite}_go"
          --language go --go "${serializer_compression_go}")
    endforeach()
  endif()
  if(serializer_compression_node AND serializer_compression_typescript_cli)
    foreach(package_suite IN ITEMS application_contract shared_codec_budget selected_codec)
      add_test(NAME serializer_package_${package_suite}_javascript
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/package_${package_suite}_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/package_${package_suite}_javascript"
          --language javascript --node "${serializer_compression_node}"
          --node-dependencies "${CMAKE_SOURCE_DIR}/editors/vscode/node_modules")
    endforeach()
    add_test(NAME serializer_package_selected_codec_typescript
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/package_selected_codec_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/package_selected_codec_typescript"
        --language typescript --node "${serializer_compression_node}"
        --typescript-cli "${serializer_compression_typescript_cli}"
        --node-dependencies "${CMAKE_SOURCE_DIR}/editors/vscode/node_modules")
  endif()
  if(serializer_compression_swiftc)
    add_test(NAME serializer_package_swift
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/swift_package_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/swift_package"
        --swiftc "${serializer_compression_swiftc}")
  endif()
  find_program(serializer_package_cargo NAMES cargo)
  if(serializer_package_cargo)
    add_test(NAME serializer_package_rust
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/rust_package_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/rust_package"
        --cargo "${serializer_package_cargo}")
    add_test(NAME serializer_package_shared_codec_budget_rust
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/package_shared_codec_budget_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/package_shared_codec_budget_rust"
        --language rust --cargo "${serializer_package_cargo}")
    add_test(NAME serializer_rust_xml_operation_policy
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/rust_xml_operation_policy_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/rust_xml_operation_policy"
        --cargo "${serializer_package_cargo}")
  endif()
  if(SERIALIZER_WITH_XML AND SERIALIZER_WITH_ZLIB)
    if(MSVC)
      set(serializer_package_cc "${CMAKE_CXX_COMPILER}")
    else()
      find_program(serializer_package_cc NAMES cc gcc clang)
    endif()
    if(serializer_package_cc)
      set(serializer_package_c_provider_args)
      if(VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
        list(APPEND serializer_package_c_provider_args
          --provider-prefix "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
      endif()
      add_test(NAME serializer_package_c
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/c_package_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/c_package"
          --cc "${serializer_package_cc}" ${serializer_package_c_provider_args})
      add_test(NAME serializer_package_shared_codec_budget_c
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/package_shared_codec_budget_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/package_shared_codec_budget_c"
          --language c --cc "${serializer_package_cc}" ${serializer_package_c_provider_args})
      add_test(NAME serializer_c_xml_operation_policy
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/c_xml_operation_policy_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/c_xml_operation_policy"
          --cc "${serializer_package_cc}" ${serializer_package_c_provider_args})
    endif()
  endif()
  # Run the same independent fixed-role preservation corpus through each available native SDK.
  function(serializer_add_package_relocation_test language)
    add_test(NAME serializer_package_relocation_${language}
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/package_relocation_test.py"
        --compiler "$<TARGET_FILE:serializer>" --language "${language}"
        --build "${CMAKE_CURRENT_BINARY_DIR}/package_relocation_${language}" ${ARGN})
  endfunction()
  serializer_add_package_relocation_test(python)
  if(Java_FOUND)
    serializer_add_package_relocation_test(java --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}")
  endif()
  if(serializer_compression_kotlinc AND Java_FOUND)
    serializer_add_package_relocation_test(kotlin --java "${Java_JAVA_EXECUTABLE}" --kotlinc "${serializer_compression_kotlinc}")
  endif()
  if(serializer_compression_node AND serializer_compression_typescript_cli)
    serializer_add_package_relocation_test(javascript --node "${serializer_compression_node}"
      --node-dependencies "${CMAKE_SOURCE_DIR}/editors/vscode/node_modules")
    serializer_add_package_relocation_test(typescript --node "${serializer_compression_node}"
      --typescript-cli "${serializer_compression_typescript_cli}"
      --node-dependencies "${CMAKE_SOURCE_DIR}/editors/vscode/node_modules")
  endif()
  if(serializer_compression_dotnet)
    serializer_add_package_relocation_test(csharp --dotnet "${serializer_compression_dotnet}")
  endif()
  if(serializer_compression_swiftc)
    serializer_add_package_relocation_test(swift --swiftc "${serializer_compression_swiftc}")
  endif()
  if(serializer_compression_go)
    serializer_add_package_relocation_test(go --go "${serializer_compression_go}")
  endif()
  if(serializer_package_cargo)
    serializer_add_package_relocation_test(rust --cargo "${serializer_package_cargo}")
  endif()
  if(SERIALIZER_WITH_XML AND SERIALIZER_WITH_ZLIB AND serializer_package_cc)
    serializer_add_package_relocation_test(c --cc "${serializer_package_cc}" ${serializer_package_c_provider_args})
  endif()
  add_executable(html_interop html_interop.cpp)
  target_link_libraries(html_interop PRIVATE Serializer::runtime)
  if(MSVC)
    target_compile_options(html_interop PRIVATE /W4 /WX)
  else()
    target_compile_options(html_interop PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME html_interop_test
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/html_interop_test.py" "$<TARGET_FILE:html_interop>")
  find_program(serializer_fixed_array_node NAMES node)
  if(serializer_fixed_array_node)
    add_executable(fixed_arrays_portable_test fixed_arrays_portable_test.cpp)
    serializer_generate(TARGET fixed_arrays_portable_test
      SCHEMAS resources/fixed_arrays_portable.serializer
      OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixed_arrays_portable")
    add_test(NAME serializer_fixed_arrays_portable
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/fixed_arrays_portable_test.py"
        --compiler "$<TARGET_FILE:serializer>" --native "$<TARGET_FILE:fixed_arrays_portable_test>"
        --node "${serializer_fixed_array_node}"
        --build "${CMAKE_CURRENT_BINARY_DIR}/fixed_arrays_portable")
  endif()
  add_test(NAME serializer_runtime_literals
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/runtime_literal_test.py")
  add_test(NAME serializer_generic_python
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/example/generics/run.py"
      --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/generic_python")
  foreach(behavior_suite IN ITEMS behavior_languages native_behavior_languages typed_behavior_languages)
    add_test(NAME "serializer_${behavior_suite}_python"
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/${behavior_suite}.py"
        --compiler "$<TARGET_FILE:serializer>" --language python
        --build "${CMAKE_CURRENT_BINARY_DIR}/${behavior_suite}_python")
  endforeach()
  add_test(NAME serializer_compact_python
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/compact_languages.py"
      --compiler "$<TARGET_FILE:serializer>" --language python
      --build "${CMAKE_CURRENT_BINARY_DIR}/compact_python")
  add_test(NAME serializer_digest_python
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/digest_languages.py"
      --compiler "$<TARGET_FILE:serializer>" --language python
      --build "${CMAKE_CURRENT_BINARY_DIR}/digest_python")
  add_test(NAME serializer_optional_python
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/optional_languages.py"
      --compiler "$<TARGET_FILE:serializer>" --language python
      --build "${CMAKE_CURRENT_BINARY_DIR}/optional_python")
  add_test(NAME serializer_extended_construction_python
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/extended_construction_languages.py"
      --compiler "$<TARGET_FILE:serializer>" --language python
      --build "${CMAKE_CURRENT_BINARY_DIR}/extended_construction_python")
  add_test(NAME serializer_numeric_python
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/numeric_languages.py"
      --compiler "$<TARGET_FILE:serializer>" --language python
      --build "${CMAKE_CURRENT_BINARY_DIR}/numeric_python")
  add_test(NAME serializer_projected_records_python
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/projected_records_languages.py"
      --compiler "$<TARGET_FILE:serializer>" --language python
      --build "${CMAKE_CURRENT_BINARY_DIR}/projected_records_python")
  add_test(NAME serializer_custom_ids_python
    COMMAND "${Python3_EXECUTABLE}" -B "${PROJECT_SOURCE_DIR}/example/custom_ids/run.py"
      --compiler "$<TARGET_FILE:serializer>" --language python
      --build "${CMAKE_CURRENT_BINARY_DIR}/custom_ids_python")
  add_test(NAME serializer_value_references_python
    COMMAND "${Python3_EXECUTABLE}" -B "${PROJECT_SOURCE_DIR}/example/references/run.py"
      --compiler "$<TARGET_FILE:serializer>" --language python
      --build "${CMAKE_CURRENT_BINARY_DIR}/value_references_python")
endif()
if(SERIALIZER_BUILD_MANAGED)
  add_executable(custom_ids_managed_example ../example/custom_ids/managed_cpp/main.cpp)
  serializer_generate(TARGET custom_ids_managed_example
    SCHEMAS ../example/custom_ids/managed_cpp/model.serializer
    CONFIG "${PROJECT_SOURCE_DIR}/example/custom_ids/managed_cpp/model.ini"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/custom_ids_managed")
  target_link_libraries(custom_ids_managed_example PRIVATE Serializer::managed)
  if(MSVC)
    target_compile_options(custom_ids_managed_example PRIVATE /W4 /WX)
  else()
    target_compile_options(custom_ids_managed_example PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME custom_ids_managed_example COMMAND custom_ids_managed_example)
  # Migrate complete saves and raw models in representative identity widths/storage layouts.
  foreach(migration_profile IN ITEMS direct32 separate64)
    add_executable(managed_migration_${migration_profile}_test managed_migration_test.cpp)
    serializer_generate(TARGET managed_migration_${migration_profile}_test
      SCHEMAS resources/managed_migration.serializer
      CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/managed_migration_${migration_profile}.ini"
      OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/managed_migration_${migration_profile}")
    target_link_libraries(managed_migration_${migration_profile}_test PRIVATE Serializer::managed GTest::gtest_main)
    if(MSVC)
      target_compile_options(managed_migration_${migration_profile}_test PRIVATE /W4 /WX)
    else()
      target_compile_options(managed_migration_${migration_profile}_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME managed_migration_${migration_profile}_test COMMAND managed_migration_${migration_profile}_test)
  endforeach()

  serializer_add_migration_tool(TARGET managed_migration_tool
    SCHEMA resources/managed_migration.serializer ROOT migration_example::document
    CONFIG resources/managed_migration_direct32.ini
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/managed_migration_tool")
  add_executable(managed_migration_fixture managed_migration_fixture.cpp)
  serializer_generate(TARGET managed_migration_fixture
    SCHEMAS resources/managed_migration.serializer CONFIG resources/managed_migration_direct32.ini
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/managed_migration_fixture")
  target_link_libraries(managed_migration_fixture PRIVATE Serializer::managed)
  foreach(migration_target IN ITEMS managed_migration_tool managed_migration_fixture)
    if(MSVC)
      target_compile_options(${migration_target} PRIVATE /W4 /WX)
    else()
      target_compile_options(${migration_target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
  endforeach()
  add_test(NAME managed_migration_cli_test
    COMMAND "${CMAKE_COMMAND}" "-DMIGRATOR=$<TARGET_FILE:managed_migration_tool>"
      "-DFIXTURE=$<TARGET_FILE:managed_migration_fixture>"
      "-DTEST_BINARY_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/managed_migration_cli_test.cmake")

  # Exercise typed tokens and ID payloads with both policies, widths and storage representations.
  foreach(registry_profile IN ITEMS direct32 separate32 direct64 separate64 fixed32 fixed64 fixed_separate32 fixed_separate64)
    add_executable(managed_entity_references_${registry_profile}_test
      managed_entity_references_test.cpp managed_identified_baseline_test.cpp
      managed_reference_journal_test.cpp managed_preview_resources_test.cpp
      managed_reference_collaboration_test.cpp)
    serializer_generate(TARGET managed_entity_references_${registry_profile}_test
      SCHEMAS resources/managed_entity_references.serializer
      CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/managed_entity_references_${registry_profile}.ini"
      OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/managed_entity_references_${registry_profile}")
    target_link_libraries(managed_entity_references_${registry_profile}_test PRIVATE Serializer::managed GTest::gtest_main)
    target_compile_definitions(managed_entity_references_${registry_profile}_test PRIVATE
      SERIALIZER_IDENTIFIED_BASELINE_TEST_DIRECTORY="${CMAKE_CURRENT_BINARY_DIR}"
      SERIALIZER_IDENTIFIED_BASELINE_TEST_PROFILE="${registry_profile}")
    if(MSVC)
      target_compile_options(managed_entity_references_${registry_profile}_test PRIVATE /W4 /WX)
    else()
      target_compile_options(managed_entity_references_${registry_profile}_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME managed_entity_references_${registry_profile}_test COMMAND managed_entity_references_${registry_profile}_test)

    add_executable(managed_reference_crash_${registry_profile} managed_reference_crash_helper.cpp)
    add_dependencies(managed_reference_crash_${registry_profile} managed_entity_references_${registry_profile}_test)
    target_include_directories(managed_reference_crash_${registry_profile} PRIVATE
      "${CMAKE_CURRENT_BINARY_DIR}/managed_entity_references_${registry_profile}")
    target_link_libraries(managed_reference_crash_${registry_profile} PRIVATE Serializer::managed)
    if(MSVC)
      target_compile_options(managed_reference_crash_${registry_profile} PRIVATE /W4 /WX)
    else()
      target_compile_options(managed_reference_crash_${registry_profile} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    if(Python3_Interpreter_FOUND)
      add_test(NAME managed_reference_crash_${registry_profile}_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/managed_reference_crash_test.py"
          --helper "$<TARGET_FILE:managed_reference_crash_${registry_profile}>"
          --directory "${CMAKE_CURRENT_BINARY_DIR}/managed_reference_crash_${registry_profile}"
          --profile "${registry_profile}")
    endif()

    add_executable(managed_editable_ownership_${registry_profile}_test managed_editable_ownership_test.cpp)
    serializer_generate(TARGET managed_editable_ownership_${registry_profile}_test
      SCHEMAS resources/managed_editable_ownership.serializer
      CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/managed_editable_ownership_${registry_profile}.ini"
      OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/managed_editable_ownership_${registry_profile}")
    target_link_libraries(managed_editable_ownership_${registry_profile}_test PRIVATE Serializer::managed GTest::gtest_main)
    if(MSVC)
      target_compile_options(managed_editable_ownership_${registry_profile}_test PRIVATE /W4 /WX)
    else()
      target_compile_options(managed_editable_ownership_${registry_profile}_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME managed_editable_ownership_${registry_profile}_test COMMAND managed_editable_ownership_${registry_profile}_test)
  endforeach()
  # Exercise snapshot-local registries with both identity widths and storage representations.
  foreach(registry_profile IN ITEMS direct32 separate32 direct64 separate64)
    add_executable(managed_merge_${registry_profile}_test managed_merge_test.cpp)
    serializer_generate(TARGET managed_merge_${registry_profile}_test
      SCHEMAS resources/branch_merge.serializer
      CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/managed_entity_references_${registry_profile}.ini"
      OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/managed_merge_${registry_profile}")
    target_link_libraries(managed_merge_${registry_profile}_test PRIVATE Serializer::managed GTest::gtest_main)
    if(MSVC)
      target_compile_options(managed_merge_${registry_profile}_test PRIVATE /W4 /WX)
    else()
      target_compile_options(managed_merge_${registry_profile}_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME managed_merge_${registry_profile}_test COMMAND managed_merge_${registry_profile}_test)

    set(registry_config CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/managed_registry_${registry_profile}.ini")
    add_executable(managed_registry_${registry_profile}_test managed_registry_test.cpp)
    serializer_generate(TARGET managed_registry_${registry_profile}_test
      SCHEMAS resources/managed_registry.serializer ${registry_config}
      OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/managed_registry_${registry_profile}")
    target_link_libraries(managed_registry_${registry_profile}_test PRIVATE Serializer::managed GTest::gtest_main)
    target_compile_definitions(managed_registry_${registry_profile}_test PRIVATE
      SERIALIZER_REGISTRY_TEST_DIRECTORY="${CMAKE_CURRENT_BINARY_DIR}"
      SERIALIZER_REGISTRY_TEST_PROFILE="${registry_profile}")
    if(MSVC)
      target_compile_options(managed_registry_${registry_profile}_test PRIVATE /W4 /WX)
    else()
      target_compile_options(managed_registry_${registry_profile}_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME managed_registry_${registry_profile}_test COMMAND managed_registry_${registry_profile}_test)
  endforeach()
  foreach(reference_profile IN ITEMS direct32 separate32 direct64 separate64 fixed32 fixed64 fixed_separate32 fixed_separate64)
    add_executable(managed_named_references_${reference_profile}_test managed_named_references_test.cpp)
    serializer_generate(TARGET managed_named_references_${reference_profile}_test
      SCHEMAS resources/managed_named_references.serializer resources/managed_named_required_references.serializer
      CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/managed_entity_references_${reference_profile}.ini"
      OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/managed_named_references_${reference_profile}")
    target_link_libraries(managed_named_references_${reference_profile}_test PRIVATE Serializer::managed GTest::gtest_main)
    if(MSVC)
      target_compile_options(managed_named_references_${reference_profile}_test PRIVATE /W4 /WX)
    else()
      target_compile_options(managed_named_references_${reference_profile}_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME managed_named_references_${reference_profile}_test COMMAND managed_named_references_${reference_profile}_test)
  endforeach()
  add_executable(managed_reuse_test managed_reuse_test.cpp)
  serializer_generate(TARGET managed_reuse_test
    SCHEMAS resources/managed_reuse_first.serializer resources/managed_reuse_second.serializer
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/managed_reuse")
  target_link_libraries(managed_reuse_test PRIVATE Serializer::managed GTest::gtest_main)
  if(MSVC)
    target_compile_options(managed_reuse_test PRIVATE /W4 /WX)
  else()
    target_compile_options(managed_reuse_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME managed_reuse_test COMMAND managed_reuse_test)
  foreach(profile IN ITEMS direct separate)
    add_executable(transaction_collection_${profile}_test transaction_collection_test.cpp)
    set(transaction_collection_config)
    if(profile STREQUAL "separate")
      set(transaction_collection_config CONFIG resources/managed_separate.ini)
    endif()
    serializer_generate(TARGET transaction_collection_${profile}_test
      SCHEMAS resources/transaction_collections.serializer ${transaction_collection_config}
      OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/transaction_collections_${profile}")
    target_link_libraries(transaction_collection_${profile}_test PRIVATE Serializer::managed GTest::gtest_main)
    if(MSVC)
      target_compile_options(transaction_collection_${profile}_test PRIVATE /W4 /WX)
    else()
      target_compile_options(transaction_collection_${profile}_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME transaction_collection_${profile}_test COMMAND transaction_collection_${profile}_test)
  endforeach()
  foreach(profile IN ITEMS direct separate)
    add_executable(managed_optional_${profile}_test managed_optional_test.cpp)
    set(optional_managed_config)
    if(profile STREQUAL "separate")
      set(optional_managed_config CONFIG resources/managed_separate.ini)
    endif()
    serializer_generate(TARGET managed_optional_${profile}_test
      SCHEMAS resources/optional_managed.serializer ${optional_managed_config}
      OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/optional_${profile}")
    target_link_libraries(managed_optional_${profile}_test PRIVATE Serializer::managed GTest::gtest_main)
    if(MSVC)
      target_compile_options(managed_optional_${profile}_test PRIVATE /W4 /WX)
    else()
      target_compile_options(managed_optional_${profile}_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME managed_optional_${profile}_test COMMAND managed_optional_${profile}_test)
  endforeach()
  add_executable(managed_optional_recovery_helper managed_optional_recovery_helper.cpp)
  serializer_generate(TARGET managed_optional_recovery_helper SCHEMAS resources/optional_managed.serializer
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/optional_recovery")
  target_link_libraries(managed_optional_recovery_helper PRIVATE Serializer::managed)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME managed_optional_recovery_test
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/managed_optional_recovery_test.py"
        --helper "$<TARGET_FILE:managed_optional_recovery_helper>"
        --directory "${CMAKE_CURRENT_BINARY_DIR}/optional_recovery_journals")
    set_tests_properties(managed_optional_recovery_test PROPERTIES TIMEOUT 120)
  endif()
  set(dimension_history_targets)
  set(dimension_history_configs)
  set(dimension_history_directories)
  foreach(profile IN ITEMS managed32 managed64 separate64)
    add_executable(dimension_history_${profile}_test dimension_history_test.cpp)
    set_target_properties(dimension_history_${profile}_test PROPERTIES CXX_STANDARD 23)
    list(APPEND dimension_history_targets dimension_history_${profile}_test)
    list(APPEND dimension_history_configs resources/dimensions_${profile}.ini)
    list(APPEND dimension_history_directories "${CMAKE_CURRENT_BINARY_DIR}/dimensions_${profile}")
    target_link_libraries(dimension_history_${profile}_test PRIVATE Serializer::managed)
    if(Python3_Interpreter_FOUND)
      add_test(NAME dimension_history_${profile}_test
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/dimension_history_test.py"
          --helper "$<TARGET_FILE:dimension_history_${profile}_test>")
    endif()
  endforeach()
  serializer_generate_variants(TARGETS ${dimension_history_targets}
    SCHEMAS resources/dimensions_managed.serializer
    CONFIGS ${dimension_history_configs}
    OUTPUT_DIRECTORIES ${dimension_history_directories})
  add_executable(managed_collaboration_test managed_collaboration_test.cpp
    managed_collaboration_ownership_test.cpp managed_collaboration_proposal_test.cpp
    managed_collaboration_session_test.cpp managed_collaboration_history_test.cpp
    managed_local_collaboration_test.cpp managed_feature_test.cpp managed_authorization_test.cpp
    managed_collaboration_fault_matrix_test.cpp)
  serializer_generate(TARGET managed_collaboration_test
    SCHEMAS resources/collaboration_commands.serializer ../example/managed/ledger/ledger.serializer
    resources/collaboration_model.serializer
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/collaboration")
  target_link_libraries(managed_collaboration_test PRIVATE Serializer::managed GTest::gtest_main)
  if(MSVC)
    target_compile_options(managed_collaboration_test PRIVATE /W4 /WX)
  else()
    target_compile_options(managed_collaboration_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME managed_collaboration_test COMMAND managed_collaboration_test)
  add_executable(managed_strict_identity_test managed_strict_identity_test.cpp)
  serializer_generate(TARGET managed_strict_identity_test
    SCHEMAS ../example/managed/ledger/ledger.serializer resources/collaboration_model.serializer
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/strict_identity")
  target_link_libraries(managed_strict_identity_test PRIVATE Serializer::managed GTest::gtest_main)
  if(MSVC)
    target_compile_options(managed_strict_identity_test PRIVATE /W4 /WX)
  else()
    target_compile_options(managed_strict_identity_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME managed_strict_identity_test COMMAND managed_strict_identity_test)
  add_executable(managed_store_test managed_store_test.cpp managed_generated_test.cpp runtime_features_test.cpp)
  serializer_generate(TARGET managed_store_test SCHEMAS resources/managed_model.serializer resources/managed_generated.serializer resources/runtime_features.serializer
    CONFIG resources/managed_separate.ini
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/managed")
  target_link_libraries(managed_store_test PRIVATE Serializer::managed GTest::gtest_main)
  if(MSVC)
    target_compile_options(managed_store_test PRIVATE /W4 /WX)
  else()
    target_compile_options(managed_store_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME managed_store_test COMMAND managed_store_test)
  add_executable(managed_document_features_test managed_document_features_test.cpp)
  serializer_generate(TARGET managed_document_features_test
    SCHEMAS resources/managed_document_features.serializer
    CONFIG ../example/managed/document_features/codegen.ini
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/managed_document_features")
  target_link_libraries(managed_document_features_test PRIVATE Serializer::managed GTest::gtest_main)
  if(MSVC)
    target_compile_options(managed_document_features_test PRIVATE /W4 /WX)
  else()
    target_compile_options(managed_document_features_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME managed_document_features_test COMMAND managed_document_features_test)
  add_executable(managed_runtime_state_example ../example/managed/runtime_state/main.cpp)
  serializer_generate(TARGET managed_runtime_state_example
    SCHEMAS ../example/managed/runtime_state/document.serializer
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/runtime_state_example")
  target_link_libraries(managed_runtime_state_example PRIVATE Serializer::managed)
  add_test(NAME managed_runtime_state_example COMMAND managed_runtime_state_example)
  add_executable(serializer_managed_document_features ../example/managed/document_features/main.cpp)
  serializer_generate(TARGET serializer_managed_document_features
    SCHEMAS ../example/managed/document_features/document.serializer
    CONFIG ../example/managed/document_features/codegen.ini
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/document_features_example")
  target_link_libraries(serializer_managed_document_features PRIVATE Serializer::managed)
  add_test(NAME serializer_managed_document_features COMMAND serializer_managed_document_features)
  foreach(example_target IN ITEMS managed_runtime_state_example serializer_managed_document_features)
    if(MSVC)
      target_compile_options(${example_target} PRIVATE /W4 /WX)
    else()
      target_compile_options(${example_target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
  endforeach()
  add_executable(managed_document_separate_test managed_document_separate_test.cpp)
  serializer_generate(TARGET managed_document_separate_test
    SCHEMAS resources/managed_document_features.serializer
    CONFIG resources/managed_document_separate.ini
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/document_features_separate")
  target_link_libraries(managed_document_separate_test PRIVATE Serializer::managed GTest::gtest_main)
  if(MSVC)
    target_compile_options(managed_document_separate_test PRIVATE /W4 /WX)
  else()
    target_compile_options(managed_document_separate_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME managed_document_separate_test COMMAND managed_document_separate_test)
  add_executable(managed_profile_test managed_profile_test.cpp)
  serializer_generate(TARGET managed_profile_test SCHEMAS resources/managed_generated.serializer
    CONFIG resources/managed_google64.ini CODING_STANDARD google
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/managed_google64")
  target_link_libraries(managed_profile_test PRIVATE Serializer::managed GTest::gtest_main)
  if(MSVC)
    target_compile_options(managed_profile_test PRIVATE /W4 /WX)
  else()
    target_compile_options(managed_profile_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME managed_profile_test COMMAND managed_profile_test)
  add_executable(managed_direct_test managed_direct_test.cpp managed_journal_test.cpp
    managed_journal_delta_test.cpp managed_journal_document_test.cpp journal_document_codec_test.cpp
    managed_document_journal_test.cpp managed_document_collaboration_test.cpp managed_sidecar_test.cpp)
  serializer_generate(TARGET managed_direct_test
    SCHEMAS ../example/managed/ledger/ledger.serializer resources/managed_generated.serializer
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/managed_direct")
  target_link_libraries(managed_direct_test PRIVATE Serializer::managed GTest::gtest_main)
  if(MSVC)
    target_compile_options(managed_direct_test PRIVATE /W4 /WX)
  else()
    target_compile_options(managed_direct_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME managed_direct_test COMMAND managed_direct_test)
  add_executable(managed_journal_crash_helper managed_journal_crash_helper.cpp)
  serializer_generate(TARGET managed_journal_crash_helper
    SCHEMAS ../example/managed/ledger/ledger.serializer
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/managed_journal_crash")
  target_link_libraries(managed_journal_crash_helper PRIVATE Serializer::managed)
    add_executable(cache_crash_helper cache_crash_helper.cpp)
    target_link_libraries(cache_crash_helper PRIVATE Serializer::runtime)
  if(MSVC)
    target_compile_options(managed_journal_crash_helper PRIVATE /W4 /WX)
  else()
    target_compile_options(managed_journal_crash_helper PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
      add_test(NAME cache_crash_test
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/cache_crash_test.py"
          --cache-helper "$<TARGET_FILE:cache_crash_helper>"
          --journal-helper "$<TARGET_FILE:managed_journal_crash_helper>"
          --directory "${CMAKE_CURRENT_BINARY_DIR}/cache_crash")
    add_test(NAME managed_journal_crash_test
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/managed_journal_crash_test.py"
        --helper "$<TARGET_FILE:managed_journal_crash_helper>"
        --directory "${CMAKE_CURRENT_BINARY_DIR}/journal_crash")
    set_tests_properties(managed_journal_crash_test PROPERTIES TIMEOUT 300)
  endif()
  add_executable(managed_history_allocation_failure_test managed_history_allocation_failure_test.cpp)
  target_link_libraries(managed_history_allocation_failure_test PRIVATE Serializer::managed)
  if(MSVC)
    # Only header-local history containers are exercised; no STL values cross the runtime ABI.
    # Debug iterator proxies may allocate inside noexcept moves, so inject with release iterators.
    target_compile_definitions(managed_history_allocation_failure_test PRIVATE _ITERATOR_DEBUG_LEVEL=0)
    target_compile_options(managed_history_allocation_failure_test PRIVATE /W4 /WX)
  else()
    target_compile_options(managed_history_allocation_failure_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME managed_history_allocation_failure_test COMMAND managed_history_allocation_failure_test)
  set_tests_properties(managed_history_allocation_failure_test PROPERTIES TIMEOUT 60)
endif()
add_test(NAME serializer_optional_c
  COMMAND "${CMAKE_COMMAND}" "-DGENERATOR=$<TARGET_FILE:serializer>"
    "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/optional_c" "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/optional_c.cmake")
add_test(NAME serializer_native_cli
  COMMAND "${CMAKE_COMMAND}" "-DGENERATOR=$<TARGET_FILE:serializer>"
    "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/native_cli"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/native_cli.cmake")
add_executable(compression_allocation_failure_test compression_allocation_failure_test.cpp)
if(MSVC)
  # MSVC debug vector constructors allocate iterator proxies inside noexcept functions.
  # Test recoverable storage failures with release iterator semantics in BOTH translation units;
  # the normal runtime and core tests keep the caller's existing iterator/ABI configuration.
  target_sources(compression_allocation_failure_test PRIVATE ../src/compression.cpp)
  target_include_directories(compression_allocation_failure_test PRIVATE
    "$<TARGET_PROPERTY:serializer_runtime,INTERFACE_INCLUDE_DIRECTORIES>")
  target_compile_features(compression_allocation_failure_test PRIVATE cxx_std_23)
  target_compile_definitions(compression_allocation_failure_test PRIVATE _ITERATOR_DEBUG_LEVEL=0
    "$<TARGET_PROPERTY:serializer_runtime,COMPILE_DEFINITIONS>")
  target_link_libraries(compression_allocation_failure_test PRIVATE ${serializer_compression_libraries})
else()
  target_link_libraries(compression_allocation_failure_test PRIVATE Serializer::runtime)
endif()
add_test(NAME compression_allocation_failure_test COMMAND compression_allocation_failure_test)
set_tests_properties(compression_allocation_failure_test PROPERTIES TIMEOUT 60)

# Standalone interoperability is optional; frozen independent fixtures need no external tools.
if(SERIALIZER_WITH_ZSTD OR SERIALIZER_WITH_LZ4 OR SERIALIZER_WITH_ZLIB)
  add_executable(compression_interop compression_interop.cpp)
  target_link_libraries(compression_interop PRIVATE Serializer::runtime)
  find_package(Python3 3.9 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    foreach(format IN ITEMS zstd lz4 gzip zlib deflate)
      set(compression_tool_arguments)
      if(format STREQUAL "zstd" AND SERIALIZER_WITH_ZSTD)
        find_program(serializer_zstd_program NAMES zstd)
        if(NOT serializer_zstd_program)
          continue()
        endif()
        list(APPEND compression_tool_arguments --tool "${serializer_zstd_program}")
      elseif(format STREQUAL "lz4" AND SERIALIZER_WITH_LZ4)
        find_program(serializer_lz4_program NAMES lz4)
        if(NOT serializer_lz4_program)
          continue()
        endif()
        list(APPEND compression_tool_arguments --tool "${serializer_lz4_program}")
      elseif(NOT (SERIALIZER_WITH_ZLIB AND format MATCHES "^(gzip|zlib|deflate)$"))
        continue()
      endif()
      add_test(NAME compression_interop_${format}
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/compression_interop_test.py"
          --helper "$<TARGET_FILE:compression_interop>" --format "${format}" ${compression_tool_arguments})
      set_tests_properties(compression_interop_${format} PROPERTIES LABELS serializer_compression)
    endforeach()
  endif()
endif()

# ELF linker wrapping injects allocator failures without modifying the production runtime.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
  add_executable(stream_allocation_failure_test stream_allocation_failure_test.cpp)
  target_link_libraries(stream_allocation_failure_test PRIVATE Serializer::runtime)
  target_compile_options(stream_allocation_failure_test PRIVATE
    -Wall -Wextra -Wpedantic -Werror -fno-builtin-malloc -fno-builtin-realloc -fno-builtin-free)
  target_link_options(stream_allocation_failure_test PRIVATE
    "LINKER:--wrap=malloc" "LINKER:--wrap=realloc" "LINKER:--wrap=free")
  add_test(NAME stream_allocation_failure_test COMMAND stream_allocation_failure_test)
endif()

add_executable(protobuf_codec_test protobuf_codec_test.cpp protobuf_stream_test.cpp typed_magic_protobuf_test.cpp
  protobuf_fixed_array_test.cpp)
serializer_generate(TARGET protobuf_codec_test SCHEMAS resources/protobuf.serializer resources/typed_magic.serializer
  resources/protobuf_arrays.serializer
  CONFIG protobuf.ini OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/protobuf")
# Schema-validation tests call the compiler parser and writer in addition to generated runtime codecs.
target_link_libraries(protobuf_codec_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
add_test(NAME protobuf_codec_test COMMAND protobuf_codec_test)
add_executable(protobuf_sdk_probe protobuf_sdk_probe.cpp)
add_dependencies(protobuf_sdk_probe protobuf_codec_test)
target_include_directories(protobuf_sdk_probe PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/protobuf")
target_link_libraries(protobuf_sdk_probe PRIVATE Serializer::runtime)
if(Python3_Interpreter_FOUND)
  execute_process(COMMAND "${Python3_EXECUTABLE}" -c "import google.protobuf"
    RESULT_VARIABLE serializer_protobuf_python_sdk OUTPUT_QUIET ERROR_QUIET)
  if(serializer_protobuf_python_sdk EQUAL 0)
    add_test(NAME protobuf_independent_sdk
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/protobuf_sdk_test.py"
        --probe "$<TARGET_FILE:protobuf_sdk_probe>" --build "${CMAKE_CURRENT_BINARY_DIR}/protobuf_sdk")
  endif()
endif()
add_executable(protobuf_allocation_test protobuf_allocation_test.cpp)
target_link_libraries(protobuf_allocation_test PRIVATE Serializer::runtime)
add_test(NAME protobuf_allocation_test COMMAND protobuf_allocation_test)
if(MSVC)
  target_compile_options(protobuf_codec_test PRIVATE /W4 /WX)
  target_compile_options(protobuf_allocation_test PRIVATE /W4 /WX)
else()
  target_compile_options(protobuf_codec_test PRIVATE -Wall -Wextra -Werror -Wpedantic)
  target_compile_options(protobuf_allocation_test PRIVATE -Wall -Wextra -Werror -Wpedantic)
endif()
if(SERIALIZER_BUILD_PROTOBUF_INTEROP_TESTS)
  find_package(protobuf CONFIG REQUIRED)
  set(protobuf_reference_directory "${CMAKE_CURRENT_BINARY_DIR}/protobuf_reference")
  file(MAKE_DIRECTORY "${protobuf_reference_directory}")
  add_custom_command(
    OUTPUT "${protobuf_reference_directory}/protobuf_reference.pb.cc"
      "${protobuf_reference_directory}/protobuf_reference.pb.h"
    COMMAND protobuf::protoc "--cpp_out=${protobuf_reference_directory}"
      "--proto_path=${CMAKE_CURRENT_SOURCE_DIR}/resources"
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/protobuf_reference.proto"
    DEPENDS resources/protobuf_reference.proto protobuf::protoc VERBATIM)
  target_sources(protobuf_codec_test PRIVATE protobuf_interop_test.cpp
    "${protobuf_reference_directory}/protobuf_reference.pb.cc")
  target_include_directories(protobuf_codec_test SYSTEM PRIVATE "${protobuf_reference_directory}")
  target_link_libraries(protobuf_codec_test PRIVATE protobuf::libprotobuf)
  if(WIN32)
    set_tests_properties(protobuf_codec_test PROPERTIES ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:protobuf::libprotobuf>")
  endif()
endif()
add_test(NAME serializer_compiler_cli
  COMMAND "${CMAKE_COMMAND}" "-DGENERATOR=$<TARGET_FILE:serializer>"
    "-DVERSION=${PROJECT_VERSION}" "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/compiler_cli"
    "-DSCHEMA_LANGUAGE_VERSION=${serializer_schema_language_version}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/compiler_cli.cmake")

add_test(NAME serializer_batch_config_cli
  COMMAND "${CMAKE_COMMAND}" "-DGENERATOR=$<TARGET_FILE:serializer>"
    "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/batch_config_cli"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/batch_config_cli.cmake")

add_test(NAME serializer_compatibility_cli
  COMMAND "${CMAKE_COMMAND}" "-DGENERATOR=$<TARGET_FILE:serializer>"
    "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/compatibility_cli"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/schema_compatibility_cli.cmake")

add_test(NAME serializer_include_cli
  COMMAND "${CMAKE_COMMAND}" "-DGENERATOR=$<TARGET_FILE:serializer>"
    "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/include_cli"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/schema_include_cli.cmake")

find_program(serializer_test_ninja NAMES ninja HINTS "${CMAKE_MAKE_PROGRAM}/..")
if(CMAKE_GENERATOR MATCHES "Ninja")
  set(serializer_test_ninja "${CMAKE_MAKE_PROGRAM}")
endif()
if(serializer_test_ninja)
  add_test(NAME serializer_batch_config_dependencies
    COMMAND "${CMAKE_COMMAND}" "-DGENERATOR=$<TARGET_FILE:serializer>"
      "-DREPOSITORY=${PROJECT_SOURCE_DIR}" "-DNINJA=${serializer_test_ninja}"
      "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/batch_config_dependencies"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/batch_config_build.cmake")
  add_test(NAME serializer_include_dependencies
    COMMAND "${CMAKE_COMMAND}" "-DGENERATOR=$<TARGET_FILE:serializer>"
      "-DREPOSITORY=${PROJECT_SOURCE_DIR}" "-DNINJA=${serializer_test_ninja}"
      "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/include_dependencies"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/schema_include_build.cmake")
endif()

add_test(NAME serializer_cpp_standard
  COMMAND "${CMAKE_COMMAND}"
    "-DCOMPILER=${CMAKE_CXX_COMPILER}" "-DMSVC_LIKE=${MSVC}"
    "-DCPP23_OPTION=${CMAKE_CXX23_STANDARD_COMPILE_OPTION}"
    "-DSOURCE_DIRECTORY=${PROJECT_SOURCE_DIR}"
    "-DWORK_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/cpp_standard"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cpp_standard.cmake")

add_test(NAME serializer_clang_format_discovery
  COMMAND "${CMAKE_COMMAND}" "-DFORMATTER=${SERIALIZER_CLANG_FORMAT_EXECUTABLE}"
    "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/clang_format_discovery"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/clang_format_discovery.cmake")

if(SERIALIZER_INSTALL AND NOT CMAKE_CROSSCOMPILING)
  find_package(Python3 3.10 COMPONENTS Interpreter QUIET)
  set(serializer_installed_example_python "")
  if(Python3_Interpreter_FOUND)
    set(serializer_installed_example_python "${Python3_EXECUTABLE}")
  endif()
  add_test(NAME serializer_package_version
    COMMAND "${CMAKE_COMMAND}"
      "-DVERSION_FILE=${PROJECT_BINARY_DIR}/SerializerConfigVersion.cmake"
      "-DVERSION=${PROJECT_VERSION}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/package_version.cmake")
  add_test(NAME serializer_installed_package
    COMMAND "${CMAKE_COMMAND}" "-DBUILD_DIRECTORY=${PROJECT_BINARY_DIR}"
      "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/installed_package/$<CONFIG>"
      "-DCONFIGURATION=$<CONFIG>" "-DBUILD_MANAGED=${SERIALIZER_BUILD_MANAGED}"
      "-DWITH_zstd=${SERIALIZER_WITH_ZSTD}" "-DWITH_lz4=${SERIALIZER_WITH_LZ4}"
      "-DWITH_zlib=${SERIALIZER_WITH_ZLIB}" "-DDEPENDENCY_PREFIXES=${CMAKE_PREFIX_PATH}"
      "-DGENERATOR=${CMAKE_GENERATOR}" "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}"
      "-DGENERATOR_INSTANCE=${CMAKE_GENERATOR_INSTANCE}" "-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
      "-DLIBRARY_ARCHITECTURE=${CMAKE_LIBRARY_ARCHITECTURE}"
      "-DEXAMPLE_PYTHON=${serializer_installed_example_python}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/install_package.cmake")
  add_test(NAME serializer_compiler_package
    COMMAND "${CMAKE_COMMAND}" "-DBUILD_DIRECTORY=${PROJECT_BINARY_DIR}"
      "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/compiler_package/$<CONFIG>"
      "-DCONFIGURATION=$<CONFIG>" "-DBUILD_MANAGED=${SERIALIZER_BUILD_MANAGED}"
      "-DVERSION=${PROJECT_VERSION}" "-DPOINTER_SIZE=${CMAKE_SIZEOF_VOID_P}"
      "-DWITH_ZSTD=${SERIALIZER_WITH_ZSTD}" "-DWITH_LZ4=${SERIALIZER_WITH_LZ4}"
      "-DWITH_ZLIB=${SERIALIZER_WITH_ZLIB}" "-DWITH_XML=${SERIALIZER_WITH_XML}"
      "-DHOST_SYSTEM_NAME=${CMAKE_HOST_SYSTEM_NAME}" "-DDEPENDENCY_PREFIXES=${CMAKE_PREFIX_PATH}"
      "-DGENERATOR=${CMAKE_GENERATOR}" "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}"
      "-DGENERATOR_INSTANCE=${CMAKE_GENERATOR_INSTANCE}" "-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
      "-DLIBRARY_ARCHITECTURE=${CMAKE_LIBRARY_ARCHITECTURE}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/compiler_package.cmake")
endif()

if(SERIALIZER_BUILD_JAVA_EXAMPLES)
  include(java_tests.cmake)
endif()

if(SERIALIZER_BUILD_INTEROP_EXAMPLES)
  include(portable_tests.cmake)
endif()

if(SERIALIZER_BUILD_INTEROP_EXAMPLES OR SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES)
  include(char_map_tests.cmake)
endif()

add_test(NAME serializer_portable_cli
  COMMAND "${CMAKE_COMMAND}" "-DGENERATOR=$<TARGET_FILE:serializer>"
    "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/portable_cli"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/portable_cli.cmake")

# Compile identical model names in separate binaries; each header comes from the actual compiler.
foreach(profile IN ITEMS disabled enabled)
  add_executable(runtime_equivalence_${profile}_test
    runtime_equivalence_test.cpp runtime_equivalence_codegen.cpp)
  serializer_generate(TARGET runtime_equivalence_${profile}_test
    SCHEMAS resources/runtime_equivalence.serializer
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/runtime_equivalence_${profile}.ini"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/runtime_equivalence_${profile}")
  if(MSVC)
    target_compile_options(runtime_equivalence_${profile}_test PRIVATE /W4 /WX)
  elseif(UNIX)
    target_compile_options(runtime_equivalence_${profile}_test PRIVATE
      -Wall -Werror -Wpedantic -Wextra -Weffc++)
  endif()
endforeach()
add_test(NAME serializer_constant_evaluation_runtime_equivalence
  COMMAND "${CMAKE_COMMAND}"
    "-DDISABLED=$<TARGET_FILE:runtime_equivalence_disabled_test>"
    "-DENABLED=$<TARGET_FILE:runtime_equivalence_enabled_test>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/runtime_equivalence.cmake")

# Keep the public fixed-array document consumer in the ordinary C++ build.
add_executable(serializer_document_collections_cpp ../example/cpp/document_collections/main.cpp)
serializer_generate(TARGET serializer_document_collections_cpp
  SCHEMAS resources/document_collections/message.serializer
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/document_collections_cpp")
target_link_libraries(serializer_document_collections_cpp PRIVATE Serializer::runtime)
if(MSVC)
  target_compile_options(serializer_document_collections_cpp PRIVATE /W4 /WX)
else()
  target_compile_options(serializer_document_collections_cpp PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME serializer_document_collections_cpp COMMAND serializer_document_collections_cpp
  "${PROJECT_SOURCE_DIR}/example/schemas/document_collections/fixture.json"
  "${CMAKE_CURRENT_BINARY_DIR}/document_collections_output.json")

# Imported vcpkg DLLs must be available when tests run from CTest or directly from the build tree.
# Derive every transitive runtime dependency from the target instead of assuming a package path.
add_executable(constructor_delegation_test constructor_delegation_test.cpp)
serializer_generate(TARGET constructor_delegation_test
  SCHEMAS resources/constructor_delegation.serializer
  CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/authoring_construction.ini"
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/constructor_delegation")
target_link_libraries(constructor_delegation_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
if(MSVC)
  target_compile_options(constructor_delegation_test PRIVATE /W4 /WX)
else()
  target_compile_options(constructor_delegation_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME constructor_delegation_test COMMAND constructor_delegation_test)

add_executable(compression_stream_test compression_stream_test.cpp)
serializer_generate(TARGET compression_stream_test SCHEMAS resources/person.serializer
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/compression_stream")
target_link_libraries(compression_stream_test PRIVATE Serializer::runtime GTest::gtest_main)
if(MSVC)
  target_compile_options(compression_stream_test PRIVATE /W4 /WX)
else()
  target_compile_options(compression_stream_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME compression_stream_test COMMAND compression_stream_test)

add_executable(package_test package_test.cpp core_properties_test.cpp pack_iri_test.cpp zip_metadata_test.cpp)
target_link_libraries(package_test PRIVATE Serializer::runtime GTest::gtest_main)
target_compile_definitions(package_test PRIVATE
  SERIALIZER_TEST_HAS_XML=$<BOOL:${SERIALIZER_WITH_XML}>
  SERIALIZER_TEST_RESOURCE_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/resources")
if(MSVC)
  target_compile_options(package_test PRIVATE /W4 /WX)
else()
  target_compile_options(package_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME package_test COMMAND package_test)

# Expose typed core-property XML to an independent standards-schema validation process.
add_executable(core_properties_probe "${PROJECT_SOURCE_DIR}/qualification/core_properties_probe.cpp")
target_link_libraries(core_properties_probe PRIVATE Serializer::runtime)
if(MSVC)
  target_compile_options(core_properties_probe PRIVATE /W4 /WX)
else()
  target_compile_options(core_properties_probe PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Exercise outer package addressing with independent RFC reference/Unicode oracles.
add_executable(pack_iri_probe "${PROJECT_SOURCE_DIR}/qualification/pack_iri_probe.cpp")
target_link_libraries(pack_iri_probe PRIVATE Serializer::runtime)
if(MSVC)
  target_compile_options(pack_iri_probe PRIVATE /W4 /WX)
else()
  target_compile_options(pack_iri_probe PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(zip_metadata_probe "${PROJECT_SOURCE_DIR}/qualification/zip_metadata_probe.cpp")
target_link_libraries(zip_metadata_probe PRIVATE Serializer::runtime)
if(MSVC)
  target_compile_options(zip_metadata_probe PRIVATE /W4 /WX)
else()
  target_compile_options(zip_metadata_probe PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Fault substitution needs native static objects, rather than calls inside a shared library.
get_target_property(serializer_runtime_test_type serializer_runtime TYPE)
if(serializer_runtime_test_type STREQUAL "STATIC_LIBRARY" AND
    (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
  add_executable(package_save_fault_test package_save_fault_test.cpp)
  target_link_libraries(package_save_fault_test PRIVATE Serializer::runtime GTest::gtest_main)
  if(MSVC)
    target_compile_options(package_save_fault_test PRIVATE /W4 /WX)
  else()
    target_compile_options(package_save_fault_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_link_options(package_save_fault_test PRIVATE
      "LINKER:--wrap=open,--wrap=write,--wrap=fsync,--wrap=rename,--wrap=close")
  endif()
  add_test(NAME package_save_fault_test COMMAND package_save_fault_test)
endif()

add_executable(package_zip64_probe package_zip64_probe.cpp)
target_link_libraries(package_zip64_probe PRIVATE Serializer::runtime)
if(MSVC)
  target_compile_options(package_zip64_probe PRIVATE /W4 /WX)
else()
  target_compile_options(package_zip64_probe PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
if(Python3_Interpreter_FOUND)
  set(serializer_zip64_python_arguments)
  if(SERIALIZER_WITH_XML)
    list(APPEND serializer_zip64_python_arguments --compiler "$<TARGET_FILE:serializer>")
  endif()
  add_test(NAME package_zip64_test COMMAND "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/package_zip64_test.py"
    --probe "$<TARGET_FILE:package_zip64_probe>"
    --build "${CMAKE_CURRENT_BINARY_DIR}/package_zip64"
    ${serializer_zip64_python_arguments})
endif()

add_executable(package_interop package_interop.cpp)
target_link_libraries(package_interop PRIVATE Serializer::runtime)
if(MSVC)
  target_compile_options(package_interop PRIVATE /W4 /WX)
else()
  target_compile_options(package_interop PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
if(Python3_Interpreter_FOUND)
  add_test(NAME package_independent_interop COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/package_interop_test.py" "$<TARGET_FILE:package_interop>")
endif()

if(SERIALIZER_WITH_XML)
  add_executable(xml_ordered_test xml_ordered_test.cpp)
  serializer_generate(TARGET xml_ordered_test SCHEMAS resources/xml_ordered.serializer
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_ordered.ini"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/xml_ordered")
  serializer_generate_source(TARGET xml_ordered_native_generated
    SCHEMA resources/xml_ordered_native.serializer LANGUAGE cpp
    OPTIONS --cpp.format false
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xml_ordered/xml_ordered_native.hpp")
  add_dependencies(xml_ordered_test xml_ordered_native_generated)
  target_link_libraries(xml_ordered_test PRIVATE Serializer::runtime GTest::gtest_main)
  if(MSVC)
    target_compile_options(xml_ordered_test PRIVATE /W4 /WX)
  else()
    target_compile_options(xml_ordered_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME xml_ordered_test COMMAND xml_ordered_test)
  add_executable(xml_ordered_xsd_test xml_ordered_xsd_test.cpp)
  serializer_generate(TARGET xml_ordered_xsd_test SCHEMAS resources/xml_ordered_xsd.serializer
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_ordered_xsd.ini"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_ordered_xsd.xsd"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/xml_ordered_xsd")
  target_compile_definitions(xml_ordered_xsd_test PRIVATE
    ORDERED_XSD_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_ordered_xsd.xsd")
  target_link_libraries(xml_ordered_xsd_test PRIVATE Serializer::runtime GTest::gtest_main)
  if(MSVC)
    target_compile_options(xml_ordered_xsd_test PRIVATE /W4 /WX)
  else()
    target_compile_options(xml_ordered_xsd_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME xml_ordered_xsd_test COMMAND xml_ordered_xsd_test)
  add_executable(xml_outer_union_test xml_outer_union_test.cpp)
  serializer_generate(TARGET xml_outer_union_test SCHEMAS resources/xml_outer_union.serializer
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_outer_union.xsd"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/xml_outer_union")
  target_compile_definitions(xml_outer_union_test PRIVATE
    OUTER_UNION_XSD_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_outer_union.xsd")
  target_link_libraries(xml_outer_union_test PRIVATE Serializer::runtime GTest::gtest_main)
  if(MSVC)
    target_compile_options(xml_outer_union_test PRIVATE /W4 /WX)
  else()
    target_compile_options(xml_outer_union_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME xml_outer_union_test COMMAND xml_outer_union_test)
  add_executable(xml_ordered_extensions_test xml_ordered_extensions_test.cpp)
  serializer_generate(TARGET xml_ordered_extensions_test SCHEMAS resources/xml_ordered_extensions.serializer
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_ordered_extensions.ini"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/xml_ordered_extensions")
  target_link_libraries(xml_ordered_extensions_test PRIVATE Serializer::runtime GTest::gtest_main)
  if(MSVC)
    target_compile_options(xml_ordered_extensions_test PRIVATE /W4 /WX)
  else()
    target_compile_options(xml_ordered_extensions_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME xml_ordered_extensions_test COMMAND xml_ordered_extensions_test)
  add_executable(xml_suffix_extensions_test xml_suffix_extensions_test.cpp)
  serializer_generate(TARGET xml_suffix_extensions_test SCHEMAS resources/xml_suffix_extensions.serializer
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_suffix_extensions.ini"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/xml_suffix_extensions")
  target_link_libraries(xml_suffix_extensions_test PRIVATE Serializer::runtime GTest::gtest_main)
  if(MSVC)
    target_compile_options(xml_suffix_extensions_test PRIVATE /W4 /WX)
  else()
    target_compile_options(xml_suffix_extensions_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME xml_suffix_extensions_test COMMAND xml_suffix_extensions_test)
  add_executable(xml_unicode_patterns_sdk xml_unicode_patterns_test.cpp)
  serializer_generate(TARGET xml_unicode_patterns_sdk SCHEMAS resources/xml_unicode_patterns.serializer
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/xml_unicode_patterns")
  target_link_libraries(xml_unicode_patterns_sdk PRIVATE Serializer::runtime)
  if(MSVC)
    target_compile_options(xml_unicode_patterns_sdk PRIVATE /W4 /WX)
  else()
    target_compile_options(xml_unicode_patterns_sdk PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  if(Python3_Interpreter_FOUND)
    add_test(NAME xml_ordered_generation_test
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xml_ordered_generation_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/xml_ordered_generation")
  endif()

  add_executable(xml_projection_test xml_projection_test.cpp)
  serializer_generate(TARGET xml_projection_test SCHEMAS resources/xml_projection.serializer
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_projection.ini"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/xml_projection")
  target_link_libraries(xml_projection_test PRIVATE Serializer::runtime GTest::gtest_main)
  if(MSVC)
    target_compile_options(xml_projection_test PRIVATE /W4 /WX)
  else()
    target_compile_options(xml_projection_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME xml_projection_test COMMAND xml_projection_test)

  add_executable(xml_rich_test xml_rich_test.cpp)
  serializer_generate(TARGET xml_rich_test
    SCHEMAS resources/xml_rich.serializer resources/xml_numeric_projection.serializer
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_rich.ini"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/xml_rich")
  target_link_libraries(xml_rich_test PRIVATE Serializer::runtime GTest::gtest_main)
  if(MSVC)
    target_compile_options(xml_rich_test PRIVATE /W4 /WX)
  else()
    target_compile_options(xml_rich_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME xml_rich_test COMMAND xml_rich_test)

  add_executable(xml_qname_list_test xml_qname_list_test.cpp)
  serializer_generate(TARGET xml_qname_list_test
    SCHEMAS resources/xml_qname_lists/model.serializer
    DEPENDS resources/xml_qname_lists/model.xsd
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/xml_qname_lists")
  target_link_libraries(xml_qname_list_test PRIVATE Serializer::runtime)
  if(MSVC)
    target_compile_options(xml_qname_list_test PRIVATE /W4 /WX)
  else()
    target_compile_options(xml_qname_list_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME xml_qname_list_test COMMAND xml_qname_list_test)

  add_executable(opc_profile_test opc_profile_test.cpp)
  serializer_generate(TARGET opc_profile_test SCHEMAS resources/opc_profile.serializer
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/opc_profile.ini"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/opc_profile")
  add_library(opc_identity_fixture INTERFACE)
  serializer_generate(TARGET opc_identity_fixture SCHEMAS resources/opc_identity.serializer
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/opc_identity.ini"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_identity_generated.serializer"
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_identity_generated.xsd"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/opc_profile")
  target_link_libraries(opc_profile_test PRIVATE Serializer::runtime GTest::gtest_main opc_identity_fixture)
  if(MSVC)
    target_compile_options(opc_profile_test PRIVATE /W4 /WX)
  else()
    target_compile_options(opc_profile_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME opc_profile_test COMMAND opc_profile_test)

  add_executable(package_application_test package_application_test.cpp)
  serializer_generate(TARGET package_application_test SCHEMAS resources/package_application_contract.serializer
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/opc_profile.ini"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/package_application")
  target_link_libraries(package_application_test PRIVATE Serializer::runtime GTest::gtest_main)
  if(MSVC)
    target_compile_options(package_application_test PRIVATE /W4 /WX)
  else()
    target_compile_options(package_application_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME package_application_test COMMAND package_application_test)

  add_executable(package_relocation_sdk package_relocation_test.cpp)
  serializer_generate(TARGET package_relocation_sdk SCHEMAS resources/package_relocation.serializer
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/opc_profile.ini"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/package_relocation")
  target_link_libraries(package_relocation_sdk PRIVATE Serializer::runtime)
  add_executable(package_pipeline_sdk package_pipeline_test.cpp)
  serializer_generate(TARGET package_pipeline_sdk SCHEMAS resources/package_pipeline.serializer
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/package_pipeline.ini"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/package_pipeline")
  target_link_libraries(package_pipeline_sdk PRIVATE Serializer::runtime)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/resources/package_pipeline.serializer" pipeline_binary_schema)
  string(REPLACE "codec(json);" "codec(xml);" pipeline_binary_schema "${pipeline_binary_schema}")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS resources/package_pipeline.serializer)
  file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/package_pipeline_binary/model.serializer"
    CONTENT "${pipeline_binary_schema}" @ONLY)
  serializer_generate_source(TARGET package_pipeline_binary_generated
    SCHEMA "${CMAKE_CURRENT_BINARY_DIR}/package_pipeline_binary/model.serializer"
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/package_pipeline_binary/package_pipeline.hpp"
    LANGUAGE cpp CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/package_pipeline.ini"
    OPTIONS --protocols binary_none --cpp.format false)
  add_executable(package_pipeline_binary_sdk package_pipeline_test.cpp)
  add_dependencies(package_pipeline_binary_sdk package_pipeline_binary_generated)
  target_include_directories(package_pipeline_binary_sdk PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/package_pipeline_binary")
  target_link_libraries(package_pipeline_binary_sdk PRIVATE Serializer::runtime)
  target_compile_definitions(package_pipeline_binary_sdk PRIVATE SERIALIZER_PIPELINE_BINARY_ONLY=1)
  if(MSVC)
    target_compile_options(package_pipeline_sdk PRIVATE /W4 /WX)
    target_compile_options(package_pipeline_binary_sdk PRIVATE /W4 /WX)
  else()
    target_compile_options(package_pipeline_sdk PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(package_pipeline_binary_sdk PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  if(SERIALIZER_WITH_ZLIB)
    add_executable(package_reproducibility_cpp "${PROJECT_SOURCE_DIR}/qualification/package_reproducibility/cpp_producer.cpp")
    serializer_generate(TARGET package_reproducibility_cpp SCHEMAS resources/package_relocation.serializer
      CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/opc_profile.ini"
      OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/package_reproducibility")
    target_link_libraries(package_reproducibility_cpp PRIVATE Serializer::runtime ZLIB::ZLIB)
    if(MSVC)
      target_compile_options(package_reproducibility_cpp PRIVATE /W4 /WX)
    else()
      target_compile_options(package_reproducibility_cpp PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
  endif()
  if(MSVC)
    target_compile_options(package_relocation_sdk PRIVATE /W4 /WX)
  else()
    target_compile_options(package_relocation_sdk PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  if(Python3_Interpreter_FOUND)
    add_test(NAME serializer_package_pipeline_cpp
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/package_pipeline_test.py"
        --compiler "$<TARGET_FILE:serializer>" --language cpp
        --consumer "$<TARGET_FILE:package_pipeline_sdk>"
        --build "${CMAKE_CURRENT_BINARY_DIR}/package_pipeline_cpp")
    add_test(NAME serializer_package_pipeline_binary_cpp
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/package_pipeline_test.py"
        --compiler "$<TARGET_FILE:serializer>" --language cpp --binary-only
        --consumer "$<TARGET_FILE:package_pipeline_binary_sdk>"
        --build "${CMAKE_CURRENT_BINARY_DIR}/package_pipeline_binary_cpp")
    add_test(NAME serializer_package_relocation_cpp
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/package_relocation_test.py"
        --compiler "$<TARGET_FILE:serializer>" --language cpp
        --consumer "$<TARGET_FILE:package_relocation_sdk>"
        --generated "${CMAKE_CURRENT_BINARY_DIR}/package_relocation/package_relocation.hpp"
        --build "${CMAKE_CURRENT_BINARY_DIR}/package_relocation_cpp")
    add_test(NAME serializer_package_reproducibility_oracle
      COMMAND "${Python3_EXECUTABLE}" -B "${PROJECT_SOURCE_DIR}/qualification/package_reproducibility/oracle.py" self-test)
    add_test(NAME serializer_package_reproducibility_guards
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/package_reproducibility_guard_test.py")
    add_test(NAME serializer_xsd_evolution_oracle
      COMMAND "${Python3_EXECUTABLE}" -B "${PROJECT_SOURCE_DIR}/qualification/xsd_evolution/oracle.py" self-test)
    add_test(NAME serializer_xsd_evolution_guards
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xsd_evolution_guard_test.py")
    add_test(NAME serializer_xsd_tooling_guards
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xsd_tooling_guard_test.py")
  endif()

  foreach(serializer_xsd_evolution_variant IN ITEMS v1 v2)
    set(serializer_xsd_evolution_target "xsd_evolution_cpp_${serializer_xsd_evolution_variant}")
    add_executable(${serializer_xsd_evolution_target}
      "${PROJECT_SOURCE_DIR}/qualification/xsd_evolution/cpp_consumer.cpp")
    serializer_generate(TARGET ${serializer_xsd_evolution_target}
      SCHEMAS "${PROJECT_SOURCE_DIR}/qualification/xsd_evolution/${serializer_xsd_evolution_variant}/order.serializer"
      OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/xsd_evolution_${serializer_xsd_evolution_variant}")
    target_link_libraries(${serializer_xsd_evolution_target} PRIVATE Serializer::runtime)
    if(MSVC)
      target_compile_options(${serializer_xsd_evolution_target} PRIVATE /W4 /WX)
    else()
      target_compile_options(${serializer_xsd_evolution_target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
  endforeach()

  if(SERIALIZER_XSD_TOOLING_SCHEMA_DIRECTORY)
    foreach(serializer_xsd_tooling_variant IN ITEMS v1 v2)
      set(serializer_xsd_tooling_schema
        "${SERIALIZER_XSD_TOOLING_SCHEMA_DIRECTORY}/${serializer_xsd_tooling_variant}/order.serializer")
      if(NOT EXISTS "${serializer_xsd_tooling_schema}")
        message(FATAL_ERROR "XSD tooling qualification requires ${serializer_xsd_tooling_schema}")
      endif()
      set(serializer_xsd_tooling_target "xsd_tooling_cpp_${serializer_xsd_tooling_variant}")
      add_executable(${serializer_xsd_tooling_target}
        "${PROJECT_SOURCE_DIR}/qualification/xsd_evolution/cpp_consumer.cpp")
      serializer_generate(TARGET ${serializer_xsd_tooling_target}
        SCHEMAS "${serializer_xsd_tooling_schema}"
        OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/xsd_tooling_${serializer_xsd_tooling_variant}")
      target_link_libraries(${serializer_xsd_tooling_target} PRIVATE Serializer::runtime)
      if(MSVC)
        target_compile_options(${serializer_xsd_tooling_target} PRIVATE /W4 /WX)
      else()
        target_compile_options(${serializer_xsd_tooling_target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
      endif()
    endforeach()
  endif()

  add_executable(xml_schema_test xml_schema_test.cpp)
  target_link_libraries(xml_schema_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
  if(MSVC)
    target_compile_options(xml_schema_test PRIVATE /W4 /WX)
  else()
    target_compile_options(xml_schema_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME xml_schema_test COMMAND xml_schema_test)
  add_executable(xsd_enum_export_test xsd_enum_export_test.cpp)
  serializer_generate(TARGET xsd_enum_export_test SCHEMAS
    "${PROJECT_SOURCE_DIR}/qualification/xsd_tooling/export_fixtures/native_enum/model.serializer"
    "${PROJECT_SOURCE_DIR}/qualification/xsd_tooling/export_fixtures/native_enum_constraints/constraints.serializer"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/xsd_enum_export")
  target_link_libraries(xsd_enum_export_test PRIVATE Serializer::serializer_lib Serializer::runtime GTest::gtest_main)
  if(MSVC)
    target_compile_options(xsd_enum_export_test PRIVATE /W4 /WX)
  else()
    target_compile_options(xsd_enum_export_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME xsd_enum_export_test COMMAND xsd_enum_export_test)
  if(Python3_Interpreter_FOUND)
    add_test(NAME python_package_test
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/python_package_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/python_package")
    add_test(NAME python_xml_ordered_test
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xml_ordered_portable_test.py"
        --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/python_xml_ordered"
        --language python)
    foreach(process strict lax)
      foreach(retention preserve skip)
        add_test(NAME python_xml_empty_inventory_${process}_${retention}_test
          COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xml_wildcard_empty_inventory_test.py"
            --compiler "$<TARGET_FILE:serializer>"
            --build "${CMAKE_CURRENT_BINARY_DIR}/python_xml_empty_inventory_${process}_${retention}"
            --language python --process ${process} --retention ${retention})
        add_test(NAME python_xml_wildcard_${process}_${retention}_test
          COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xml_wildcard_validation_test.py"
            --compiler "$<TARGET_FILE:serializer>"
            --build "${CMAKE_CURRENT_BINARY_DIR}/python_xml_wildcard_${process}_${retention}"
            --language python --process ${process} --retention ${retention})
      endforeach()
    endforeach()
    foreach(retention preserve skip)
      add_test(NAME python_xml_unknown_${retention}_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xml_unknown_policy_test.py"
          --compiler "$<TARGET_FILE:serializer>"
          --build "${CMAKE_CURRENT_BINARY_DIR}/python_xml_unknown_${retention}"
          --language python --retention ${retention} --field-count 300)
    endforeach()
    add_test(NAME python_xml_wildcard_identity_policy_test
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xml_wildcard_identity_policy_test.py"
        --compiler "$<TARGET_FILE:serializer>"
        --build "${CMAKE_CURRENT_BINARY_DIR}/python_xml_wildcard_identity_policy"
        --language python)
    if(SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES)
      add_test(NAME portable_xml_wildcard_identity_policy_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xml_wildcard_identity_policy_test.py"
          --compiler "$<TARGET_FILE:serializer>"
          --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_wildcard_identity_policy"
          --language javascript,typescript,java,csharp,go,kotlin,swift,rust,c)
      foreach(process strict lax)
        foreach(retention preserve skip)
          add_test(NAME portable_xml_empty_inventory_${process}_${retention}_test
            COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xml_wildcard_empty_inventory_test.py"
              --compiler "$<TARGET_FILE:serializer>"
              --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_empty_inventory_${process}_${retention}"
              --language javascript,typescript,java,csharp,go,kotlin,swift,rust,c
              --process ${process} --retention ${retention})
          add_test(NAME portable_xml_wildcard_${process}_${retention}_test
            COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xml_wildcard_validation_test.py"
              --compiler "$<TARGET_FILE:serializer>"
              --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_wildcard_${process}_${retention}"
              --language javascript,typescript,java,csharp,go,kotlin,swift,rust,c
              --process ${process} --retention ${retention})
        endforeach()
      endforeach()
      foreach(retention preserve skip)
        add_test(NAME portable_xml_unknown_${retention}_test
          COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xml_unknown_policy_test.py"
            --compiler "$<TARGET_FILE:serializer>"
            --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_unknown_${retention}"
            --language javascript,typescript,java,csharp,go,kotlin,swift,rust,c
            --retention ${retention} --field-count 300)
      endforeach()
      foreach(group all choice choice-explicit)
        foreach(process skip strict lax)
          add_test(NAME portable_xml_group_${group}_${process}_test
            COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xml_group_extensions_test.py"
              --compiler "$<TARGET_FILE:serializer>"
              --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_group_${group}_${process}"
              --language python,javascript,typescript,java,csharp,go,kotlin,swift,rust,c
              --group ${group} --process ${process})
        endforeach()
      endforeach()
      add_test(NAME portable_xml_ordered_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xml_ordered_portable_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_ordered"
          --language javascript,typescript,java,csharp,go,kotlin,swift,rust,c)
      add_test(NAME portable_xml_ordered_xsd_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xml_ordered_portable_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_ordered_xsd"
          --grouped-xsd)
      add_test(NAME native_xml_namespace_graph_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/native_xml_namespace_graph_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/native_xml_namespace_graphs")
      add_test(NAME native_xml_extension_graph_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/native_xml_extension_graph_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/native_xml_extension_graphs")
      add_test(NAME native_xml_qname_constraints_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/native_xml_qname_constraints_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/native_xml_qname_constraints")
        add_test(NAME native_xml_qname_defaults_test
          COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/native_xml_qname_defaults_test.py"
            --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/native_xml_qname_defaults")
        add_test(NAME native_package_pipeline_graph_test
          COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/native_package_pipeline_graph_test.py"
            --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/native_package_pipeline_graph")
        add_test(NAME native_xml_outer_qname_union_test
          COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/native_xml_outer_qname_union_test.py"
            --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/native_xml_outer_qname_union")
        add_test(NAME native_xml_restricted_list_test
          COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/native_xml_restricted_list_test.py"
            --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/native_xml_restricted_list")
        foreach(scope children attributes owner_text)
          if(scope STREQUAL "children")
            set(native_qname_scope --schema-defaults)
          elseif(scope STREQUAL "attributes")
            set(native_qname_scope --attributes)
          else()
            set(native_qname_scope --owner-text)
          endif()
          add_test(NAME native_xml_qname_defaults_${scope}_test
            COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/native_xml_outer_qname_union_test.py"
              ${native_qname_scope} --compiler "$<TARGET_FILE:serializer>"
              --build "${CMAKE_CURRENT_BINARY_DIR}/native_xml_qname_defaults_${scope}")
        endforeach()
      add_test(NAME xml_ordered_extensions_sdk_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xml_ordered_extensions_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/xml_ordered_extensions_sdk")
      add_test(NAME portable_xml_outer_union_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/portable_xml_outer_union_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_outer_union")
      add_test(NAME python_xml_choice_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/python_xml_choice_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/python_xml_choice")
      # This opt-in SDK qualification requires Python's lxml XSD provider.
      add_test(NAME python_xml_rich_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/python_xml_rich_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/python_xml_rich")
      # Keep the combined operation regression quick in CTest; the maintained
      # harness defaults to 25,000 entries per owner for full qualification.
      # Like the rich XML suite above, its independent XSD oracle requires lxml.
      add_test(NAME python_package_operation_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/package_large_operation_native_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/python_package_operation"
          --language python --entries-per-registry 10)
      add_test(NAME python_xml_token_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/python_xml_token_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/python_xml_token")
      add_test(NAME python_xml_identity_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/python_xml_identity_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/python_xml_identity")
      add_test(NAME python_xml_scalar_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/python_xml_scalar_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/python_xml_scalar")
      add_test(NAME python_xml_model_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/python_xml_model_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/python_xml_model")
      add_test(NAME python_xml_extension_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/python_xml_extension_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/python_xml_extension")
      add_test(NAME portable_xml_facets_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/portable_xml_facets_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_facets")
      add_test(NAME portable_xml_scalar_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/portable_xml_scalar_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_scalar")
      add_test(NAME portable_xml_map_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/portable_xml_map_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_map")
      add_test(NAME mobile_xml_map_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/mobile_xml_map_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/mobile_xml_map")
      add_test(NAME mobile_xml_variant_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/mobile_xml_variant_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/mobile_xml_variant")
      add_test(NAME mobile_xml_mixed_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/mobile_xml_mixed_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/mobile_xml_mixed")
      add_test(NAME mobile_xml_scalar_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/mobile_xml_scalar_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/mobile_xml_scalar")
      add_test(NAME mobile_xml_numeric_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/mobile_xml_numeric_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/mobile_xml_numeric")
      add_test(NAME mobile_html_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/mobile_html_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/mobile_html")
      add_test(NAME native_html_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/native_html_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/native_html")
      add_test(NAME native_xml_extension_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/native_xml_extension_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/native_xml_extension")
      add_test(NAME native_xml_map_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/native_xml_map_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/native_xml_map")
      add_test(NAME native_xml_variant_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/native_xml_variant_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/native_xml_variant")
      add_test(NAME native_xml_mixed_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/native_xml_mixed_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/native_xml_mixed")
      add_test(NAME native_xml_lifecycle_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/native_xml_lifecycle_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/native_xml_lifecycle")
      add_test(NAME native_xml_scalar_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/native_xml_scalar_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/native_xml_scalar")
      add_test(NAME native_identity_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/native_identity_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/native_identity")
      add_test(NAME native_xml_projection_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/native_xml_projection_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/native_xml_projection")
      add_test(NAME mobile_xml_projection_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/mobile_xml_projection_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/mobile_xml_projection")
      add_test(NAME mobile_xml_extension_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/mobile_xml_extension_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/mobile_xml_extension")
      add_test(NAME mobile_xml_lifecycle_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/mobile_xml_lifecycle_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/mobile_xml_lifecycle")
      add_test(NAME python_xml_projection_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/python_xml_projection_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/python_xml_projection")
      add_test(NAME portable_xml_variant_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/portable_xml_variant_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_variant")
      add_test(NAME portable_xml_typed_variant_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/portable_xml_typed_variant_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_typed_variant")
      add_test(NAME portable_xml_presence_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/portable_xml_presence_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_presence")
      add_test(NAME portable_xml_model_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/portable_xml_model_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_model")
      add_test(NAME portable_xml_namespace_budget_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xml_namespace_budget_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_namespace_budget"
          --language java,csharp)
      add_test(NAME mobile_xml_namespace_budget_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xml_namespace_budget_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/mobile_xml_namespace_budget"
          --language kotlin,swift)
      add_test(NAME portable_xml_lifecycle_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/portable_xml_lifecycle_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_lifecycle")
      set(portable_xml_projection_provider_args)
      if(WIN32)
        list(APPEND portable_xml_projection_provider_args --bridge "$<TARGET_FILE:xml_provider_bridge>")
      endif()
      add_test(NAME portable_xml_projection_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/portable_xml_projection_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_projection"
          ${portable_xml_projection_provider_args})
      add_test(NAME portable_xml_extension_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/portable_xml_extension_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_extension")
      add_test(NAME native_xml_model_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/native_xml_model_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/native_xml_model")
      add_test(NAME portable_xml_content_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/portable_xml_content_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_content")
      add_test(NAME portable_xml_managed_content_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/portable_xml_managed_content_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_managed_content")
      add_test(NAME portable_xml_mixed_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/portable_xml_mixed_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_mixed")
      add_test(NAME portable_xml_identity_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/portable_xml_identity_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_identity")
      add_test(NAME mobile_identity_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/mobile_identity_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/mobile_xml_identity")
      add_test(NAME portable_html_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/portable_html_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_html")
      add_test(NAME portable_xml_all_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/portable_xml_all_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_all")
      add_test(NAME portable_xml_provider_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/portable_xml_provider_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_provider")
      if(WIN32)
        add_test(NAME native_xml_provider_test
          COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/native_xml_provider_test.py"
            --compiler "$<TARGET_FILE:serializer>" --bridge "$<TARGET_FILE:xml_provider_bridge>"
            --build "${CMAKE_CURRENT_BINARY_DIR}/native_xml_provider" --language go,swift,rust,c)
      endif()
      add_test(NAME kotlin_xml_token_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/kotlin_xml_token_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/kotlin_xml_token")
      add_test(NAME swift_xml_token_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/swift_xml_token_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/swift_xml_token")
      add_test(NAME swift_xml_presence_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/swift_xml_presence_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/swift_xml_presence")
      add_test(NAME rust_xml_token_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/rust_xml_token_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/rust_xml_token")
      add_test(NAME rust_xml_presence_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/rust_xml_presence_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/rust_xml_presence")
      add_test(NAME c_xml_token_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/c_xml_token_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/c_xml_token")
      add_test(NAME c_xml_presence_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/c_xml_presence_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/c_xml_presence")
      add_test(NAME javascript_xml_token_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/javascript_xml_token_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/javascript_xml_token")
      add_test(NAME serializer_package_javascript
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/javascript_package_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/javascript_package"
          --native-producer "$<TARGET_FILE:package_interop>")
      add_test(NAME javascript_xml_content_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/javascript_xml_content_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/javascript_xml_content")
      add_test(NAME javascript_xml_map_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/javascript_xml_map_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/javascript_xml_map")
      add_test(NAME javascript_xml_variant_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/javascript_xml_variant_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/javascript_xml_variant")
      add_test(NAME javascript_xml_model_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/javascript_xml_model_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/javascript_xml_model")
      add_test(NAME javascript_xml_scalar_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/javascript_xml_scalar_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/javascript_xml_scalar")
      add_test(NAME javascript_xml_lexical_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/javascript_xml_lexical_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/javascript_xml_lexical")
      add_test(NAME javascript_xml_numeric_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/javascript_xml_numeric_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/javascript_xml_numeric")
      add_test(NAME javascript_xml_mixed_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/javascript_xml_mixed_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/javascript_xml_mixed")
      add_test(NAME javascript_xml_identity_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/javascript_xml_identity_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/javascript_xml_identity")
      add_test(NAME javascript_xml_extension_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/javascript_xml_extension_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/javascript_xml_extension")
      add_test(NAME javascript_xml_lifecycle_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/javascript_xml_lifecycle_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/javascript_xml_lifecycle")
      add_test(NAME javascript_xml_provider_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/javascript_xml_provider_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/javascript_xml_provider")
      add_test(NAME javascript_xml_projection_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/javascript_xml_projection_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/javascript_xml_projection")
      add_test(NAME javascript_html_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/javascript_html_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/javascript_html")
      add_test(NAME kotlin_xml_presence_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/kotlin_xml_presence_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/kotlin_xml_presence")
      add_test(NAME go_xml_facets_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/go_xml_facets_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/go_xml_facets")
      add_test(NAME go_xml_namespace_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/go_xml_namespace_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/go_xml_namespace")
      add_test(NAME go_xml_qname_constraints_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/go_xml_qname_constraints_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/go_xml_qname_constraints")
      add_test(NAME portable_xml_qname_constraints_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/go_xml_qname_constraints_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_qname_constraints"
          --language java,csharp)
      add_test(NAME portable_xml_namespace_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/go_xml_namespace_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/portable_xml_namespace"
          --language java,csharp)
      add_test(NAME native_xml_namespace_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/native_xml_namespace_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/native_xml_namespace")
      add_test(NAME mobile_xml_namespace_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/mobile_xml_namespace_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/mobile_xml_namespace")
      add_test(NAME mobile_xml_qname_constraints_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/mobile_xml_qname_constraints_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/mobile_xml_qname_constraints")
      add_test(NAME go_xml_pattern_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/go_xml_pattern_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/go_xml_pattern")
      add_test(NAME go_xml_model_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/go_xml_model_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/go_xml_model")
      add_test(NAME go_xml_map_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/go_xml_map_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/go_xml_map")
      add_test(NAME python_3mf_core_test
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/python_3mf_core_test.py"
          --compiler "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/python_3mf_core")
    endif()
    add_test(NAME xsd_import_cli_test
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xsd_import_cli_test.py"
        "$<TARGET_FILE:serializer>" "${CMAKE_CURRENT_BINARY_DIR}")
    add_test(NAME xml_schema_tools_test
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xml_schema_tools_test.py"
        "$<TARGET_FILE:serializer>")
    add_test(NAME xsd_bundle_cli_test
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xsd_bundle_cli_test.py"
        "$<TARGET_FILE:serializer>" --build "${CMAKE_CURRENT_BINARY_DIR}/xsd_bundle_cli")
  endif()
  add_executable(xml_test xml_test.cpp)
  add_library(xml_fixture_models INTERFACE)
  serializer_generate(TARGET xml_fixture_models SCHEMAS resources/xml_fixture.serializer
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_fixture.ini"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/xml")
  add_library(xml_xsd_fixture_models INTERFACE)
  serializer_generate(TARGET xml_xsd_fixture_models SCHEMAS resources/xml_xsd_fixture.serializer
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_xsd_fixture.ini"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_xsd_fixture.xsd"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/xml_xsd")
  target_link_libraries(xml_test PRIVATE Serializer::serializer_lib GTest::gtest_main
    xml_fixture_models xml_xsd_fixture_models)
  if(MSVC)
    target_compile_options(xml_test PRIVATE /W4 /WX)
  else()
    target_compile_options(xml_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME xml_test COMMAND xml_test)

  add_executable(xml_token_test xml_token_test.cpp)
  target_link_libraries(xml_token_test PRIVATE Serializer::runtime GTest::gtest_main
    xml_fixture_models xml_xsd_fixture_models)
  if(MSVC)
    target_compile_options(xml_token_test PRIVATE /W4 /WX)
  else()
    target_compile_options(xml_token_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME xml_token_test COMMAND xml_token_test)

  # The native executable and independent Python oracle share the maintained portable presence corpus.
  add_executable(xml_presence_test xml_presence_test.cpp)
  serializer_generate(TARGET xml_presence_test SCHEMAS resources/xml_presence_fixture.serializer
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_presence_fixture.ini"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_presence_fixture.xsd"
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_presence_all_fixture.xsd"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/xml_presence")
  target_link_libraries(xml_presence_test PRIVATE Serializer::runtime)
  if(MSVC)
    target_compile_options(xml_presence_test PRIVATE /W4 /WX)
  else()
    target_compile_options(xml_presence_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME xml_presence_test COMMAND xml_presence_test)
  add_executable(xml_scalar_test xml_scalar_test.cpp)
  serializer_generate(TARGET xml_scalar_test SCHEMAS resources/xml_scalar/xml_scalar_fixture.serializer
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_scalar/numeric.xsd"
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_scalar/pattern.xsd"
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_scalar/large.xsd"
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_scalar/scalars.xsd"
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_scalar/defaults.xsd"
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_scalar/qname_union.xsd"
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_xsd_fixture.xsd"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/xml_scalar")
  target_link_libraries(xml_scalar_test PRIVATE Serializer::runtime)
  if(MSVC)
    target_compile_options(xml_scalar_test PRIVATE /W4 /WX)
  else()
    target_compile_options(xml_scalar_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME xml_scalar_test COMMAND xml_scalar_test)
  if(Python3_Interpreter_FOUND AND SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES)
    add_test(NAME cpp_xml_scalar_test
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/cpp_xml_scalar_test.py"
        --executable "$<TARGET_FILE:xml_scalar_test>"
        --build "${CMAKE_CURRENT_BINARY_DIR}/cpp_xml_scalar")
    add_test(NAME cpp_xml_presence_test
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/cpp_xml_presence_test.py"
        --compiler "$<TARGET_FILE:serializer>" --executable "$<TARGET_FILE:xml_presence_test>"
        --build "${CMAKE_CURRENT_BINARY_DIR}/cpp_xml_presence")
  endif()

  find_package(Threads REQUIRED)
  add_library(xml_provider_bridge SHARED xml_provider_bridge.cpp xml_provider_bridge.hpp)
  target_link_libraries(xml_provider_bridge PRIVATE Serializer::runtime LibXml2::LibXml2)
  target_compile_definitions(xml_provider_bridge PRIVATE SERIALIZER_XML_PROVIDER_BRIDGE_BUILD)
  if(NOT WIN32)
    set_property(TARGET serializer_runtime PROPERTY POSITION_INDEPENDENT_CODE ON)
  endif()
  if(MSVC)
    target_compile_options(xml_provider_bridge PRIVATE /W4 /WX)
  else()
    target_compile_options(xml_provider_bridge PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_executable(xml_provider_test xml_provider_test.cpp)
  serializer_generate(TARGET xml_provider_test SCHEMAS resources/xml_provider.serializer
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_provider.ini"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/xml_provider")
  target_link_libraries(xml_provider_test PRIVATE Serializer::runtime LibXml2::LibXml2
    Threads::Threads GTest::gtest_main)
  if(MSVC)
    target_compile_options(xml_provider_test PRIVATE /W4 /WX)
  else()
    target_compile_options(xml_provider_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME xml_provider_test COMMAND xml_provider_test)

  # Compile generated libraries separately so private operation hooks cannot
  # accidentally satisfy the public cancellation API qualification.
  if(Python3_Interpreter_FOUND)
    if(serializer_compression_dotnet)
      add_test(NAME xml_public_cancellation_csharp
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xml_public_cancellation_test.py"
          --compiler "$<TARGET_FILE:serializer>" --language csharp
          --build "${CMAKE_CURRENT_BINARY_DIR}/xml_public_cancellation_csharp")
    endif()
    if(serializer_compression_kotlinc AND Java_FOUND)
      add_test(NAME xml_public_cancellation_kotlin
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xml_public_cancellation_test.py"
          --compiler "$<TARGET_FILE:serializer>" --language kotlin
          --kotlinc "${serializer_compression_kotlinc}"
          --build "${CMAKE_CURRENT_BINARY_DIR}/xml_public_cancellation_kotlin")
    endif()
    if(WIN32 AND serializer_compression_swiftc)
      add_test(NAME xml_public_cancellation_swift
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/xml_public_cancellation_test.py"
          --compiler "$<TARGET_FILE:serializer>" --language swift
          --swiftc "${serializer_compression_swiftc}" --bridge "$<TARGET_FILE:xml_provider_bridge>"
          --build "${CMAKE_CURRENT_BINARY_DIR}/xml_public_cancellation_swift")
    endif()
  endif()

  add_executable(xml_shapes_cpp xml_shapes_test.cpp)
  serializer_generate(TARGET xml_shapes_cpp SCHEMAS resources/xml_shapes/model.serializer
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/xml_shapes/options.ini"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/xml_shapes_cpp")
  target_link_libraries(xml_shapes_cpp PRIVATE Serializer::runtime)
  if(MSVC)
    target_compile_options(xml_shapes_cpp PRIVATE /W4 /WX)
  else()
    target_compile_options(xml_shapes_cpp PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME xml_shapes_cpp COMMAND xml_shapes_cpp)
endif()

if(WIN32)
  get_property(serializer_test_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
  foreach(target IN LISTS serializer_test_targets)
    get_target_property(target_type "${target}" TYPE)
    if(target_type STREQUAL "EXECUTABLE" OR target_type STREQUAL "SHARED_LIBRARY")
      # Keep the DLL list in one argument; expanding it would split the -D assignment.
      add_custom_command(TARGET "${target}" POST_BUILD
        COMMAND "${CMAKE_COMMAND}" "-DDLL_FILES=$<TARGET_RUNTIME_DLLS:${target}>"
          "-DOUTPUT_DIRECTORY=$<TARGET_FILE_DIR:${target}>"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/copy_runtime_dlls.cmake"
        VERBATIM)
    endif()
  endforeach()
endif()
