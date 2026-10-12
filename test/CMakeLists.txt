set(schemas)
foreach(fixture IN ITEMS enum person test1 array map variable assessment views storage_reuse
    fixed_fields constant_names includes wire_compatibility generics dimensions versioning magic typed_magic inferred_arrays compact_fields digests owning_variants owning_variant_cpp optional_cpp native_output_policy reuse_first reuse_second construction extended_construction overloaded_construction readonly_behavior derived_sizes conversion stateless_layout generic_constraints readonly_function_calls readonly_function_overloads numeric_values)
  list(APPEND schemas "${CMAKE_CURRENT_SOURCE_DIR}/resources/${fixture}.serializer")
endforeach()

add_executable(core_serializer_test
  core_serializer_test.cpp
  behavior_test.cpp
  numeric_test.cpp
  readonly_behavior_test.cpp
  readonly_function_calls_test.cpp
  construction_test.cpp
  extended_construction_test.cpp
  overloaded_construction_test.cpp
  auto_size_test.cpp
  conversion_test.cpp
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
target_link_libraries(core_serializer_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
target_compile_definitions(core_serializer_test PRIVATE ROHIT_SERIALIZER_ENABLE_GTEST)
if(MSVC)
  target_compile_options(core_serializer_test PRIVATE /W4 /WX)
elseif(UNIX)
  target_compile_options(core_serializer_test PRIVATE -Wall -Werror -Wpedantic -Wextra -Weffc++ -Wsuggest-override)
endif()

add_test(NAME core_serializer_test COMMAND core_serializer_test)

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
  # Exercise snapshot-local registries with both identity widths and storage representations.
  foreach(registry_profile IN ITEMS direct32 separate32 direct64 separate64)
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
    managed_local_collaboration_test.cpp managed_feature_test.cpp)
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
  add_executable(managed_direct_test managed_direct_test.cpp managed_journal_test.cpp)
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
  if(MSVC)
    target_compile_options(managed_journal_crash_helper PRIVATE /W4 /WX)
  else()
    target_compile_options(managed_journal_crash_helper PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME managed_journal_crash_test
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/managed_journal_crash_test.py"
        --helper "$<TARGET_FILE:managed_journal_crash_helper>"
        --directory "${CMAKE_CURRENT_BINARY_DIR}/journal_crash")
    set_tests_properties(managed_journal_crash_test PROPERTIES TIMEOUT 180)
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
  target_compile_features(compression_allocation_failure_test PRIVATE cxx_std_20)
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

add_executable(protobuf_codec_test protobuf_codec_test.cpp protobuf_stream_test.cpp typed_magic_protobuf_test.cpp)
serializer_generate(TARGET protobuf_codec_test SCHEMAS resources/protobuf.serializer resources/typed_magic.serializer
  CONFIG protobuf.ini OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/protobuf")
# Schema-validation tests call the compiler parser and writer in addition to generated runtime codecs.
target_link_libraries(protobuf_codec_test PRIVATE Serializer::serializer_lib GTest::gtest_main)
add_test(NAME protobuf_codec_test COMMAND protobuf_codec_test)
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

add_test(NAME serializer_clang_format_discovery
  COMMAND "${CMAKE_COMMAND}" "-DFORMATTER=${SERIALIZER_CLANG_FORMAT_EXECUTABLE}"
    "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/clang_format_discovery"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/clang_format_discovery.cmake")

if(SERIALIZER_INSTALL AND NOT CMAKE_CROSSCOMPILING)
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
      -P "${CMAKE_CURRENT_SOURCE_DIR}/install_package.cmake")
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

if(WIN32)
  get_property(serializer_test_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
  foreach(target IN LISTS serializer_test_targets)
    get_target_property(target_type "${target}" TYPE)
    if(target_type STREQUAL "EXECUTABLE")
      # Keep the DLL list in one argument; expanding it would split the -D assignment.
      add_custom_command(TARGET "${target}" POST_BUILD
        COMMAND "${CMAKE_COMMAND}" "-DDLL_FILES=$<TARGET_RUNTIME_DLLS:${target}>"
          "-DOUTPUT_DIRECTORY=$<TARGET_FILE_DIR:${target}>"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/copy_runtime_dlls.cmake"
        VERBATIM)
    endif()
  endforeach()
endif()
