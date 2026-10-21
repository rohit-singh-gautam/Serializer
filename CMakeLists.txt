cmake_minimum_required(VERSION 4.0)

project(serializer VERSION 2.19.2 LANGUAGES CXX)
include(GNUInstallDirs)
# The language contract evolves independently of the compiler/runtime release.
set(serializer_schema_language_version 1.21.0)
string(REPLACE "." ";" serializer_schema_language_version_components
  "${serializer_schema_language_version}")
list(GET serializer_schema_language_version_components 0 serializer_schema_language_version_major)
list(GET serializer_schema_language_version_components 1 serializer_schema_language_version_minor)
list(GET serializer_schema_language_version_components 2 serializer_schema_language_version_patch)
configure_file(cmake/version.hpp.in generated/include/rohit/version.hpp @ONLY)

# Require at least C++20 while preserving a caller's newer language mode.
if(NOT DEFINED CMAKE_CXX_STANDARD OR CMAKE_CXX_STANDARD LESS 20)
  set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SERIALIZER_BUILD_TESTS "Build Serializer's tests" ${PROJECT_IS_TOP_LEVEL})
option(SERIALIZER_BUILD_MANAGED "Build the C++ managed snapshot runtime records" ON)
option(SERIALIZER_BUILD_MANAGED_EXAMPLES "Build C++ managed runtime examples without the test suite" OFF)
option(SERIALIZER_BUILD_PROTOBUF_INTEROP_TESTS "Verify codecs against the official Protobuf runtime" OFF)
option(SERIALIZER_BUILD_BENCHMARKS "Build separate codec timing and allocation tools" OFF)
option(SERIALIZER_BUILD_FUZZERS "Build instrumented Clang libFuzzer targets and seed corpus tools" OFF)
option(SERIALIZER_BUILD_STYLE_EXAMPLES "Build one example for each C++ output profile" OFF)
option(SERIALIZER_BUILD_IOSTREAM_EXAMPLES "Build standard and custom iostream examples" OFF)
option(SERIALIZER_BUILD_COMPRESSION_EXAMPLES "Build examples for enabled message compression formats" OFF)
option(SERIALIZER_BUILD_XML_EXAMPLES "Build the XML, compression and package example" OFF)
option(SERIALIZER_BUILD_JAVA_EXAMPLES "Generate and compile pure Java examples (JDK 17+)" OFF)
option(SERIALIZER_BUILD_INTEROP_EXAMPLES "Build five-language examples and interoperability tests" OFF)
option(SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES "Register all language examples and native codec qualification with CTest" OFF)
set(SERIALIZER_EXAMPLE_WSL_LANGUAGES "" CACHE STRING "Comma-separated example SDKs to run under WSL on Windows")
option(SERIALIZER_EXAMPLE_SANITIZERS "Use address/undefined sanitizers for native C examples and tests" OFF)
option(SERIALIZER_EXAMPLE_BIG_ENDIAN "Include QEMU s390x C/C++ in all-language interoperability" OFF)
option(SERIALIZER_INSTALL "Install the library, compiler, and CMake integration" ${PROJECT_IS_TOP_LEVEL})
option(SERIALIZER_ENABLE_SIMD "Accelerate schema scanning and runtime codecs on supported CPUs" ON)
option(SERIALIZER_WITH_ZSTD "Enable standard Zstandard frame compression" ON)
option(SERIALIZER_WITH_LZ4 "Enable standard LZ4 frame compression" ON)
option(SERIALIZER_WITH_ZLIB "Enable gzip, zlib, and raw DEFLATE compression" ON)
option(SERIALIZER_WITH_XML "Enable XML parsing and trusted XSD validation with LibXml2" ON)
include(cmake/serializer_compression.cmake)
if(SERIALIZER_WITH_XML)
  find_package(LibXml2 REQUIRED)
endif()

# Application support is permissively licensed and contains no schema compiler code.
add_library(serializer_runtime
  src/digest.cpp
  include/rohit/digest.hpp
  src/file_stream.cpp
  include/rohit/file_stream.hpp
  src/managed_file_journal.cpp
  src/managed_journal_delta.cpp
  src/managed_journal_document.cpp
  include/rohit/managed_journal_document.hpp
  include/rohit/managed_file_journal.hpp
  include/rohit/managed_journal_stream.hpp
  src/managed_journal.cpp
  include/rohit/managed_journal.hpp
  src/compression.cpp
  src/compression_deflate.hpp
  include/rohit/compression.hpp
  include/rohit/compression_io.hpp
  src/compression_stream.cpp
  include/rohit/compression_stream.hpp
  src/xml.cpp
  include/rohit/xml.hpp
  include/rohit/xml_tokens.hpp
  include/rohit/xml_name.hpp
  src/xml_events.cpp
  include/rohit/xml_events.hpp
  src/xml_event_reader.cpp
  include/rohit/xml_event_reader.hpp
  src/xml_extensions.cpp
  include/rohit/xml_extensions.hpp
  src/xml_pattern.cpp
  include/rohit/xml_pattern.hpp
  src/xml_identity.cpp
  include/rohit/xml_identity.hpp
  src/document_profile.cpp
  include/rohit/document_profile.hpp
  src/html.cpp
  include/rohit/html.hpp
  include/rohit/xml_lexical_contract.hpp
  include/rohit/xml_choice_contract.hpp
  src/package.cpp
  src/package_opc.cpp
  include/rohit/package.hpp
  include/rohit/package_profile.hpp
  include/rohit/opc_generated.hpp
  src/simd_dispatch.cpp
  src/simd_dispatch.hpp
  src/runtime_simd.cpp
  include/rohit/serializer.hpp
  include/rohit/constant_binary.hpp
  include/rohit/constant_binary_output.hpp
  include/rohit/binary_output_support.hpp
  include/rohit/binary_count_stream.hpp
  include/rohit/binary_scalar.hpp
  include/rohit/utf8.hpp
  include/rohit/protobuf.hpp
  include/rohit/decode.hpp
  include/rohit/binary_view.hpp
  include/rohit/json_text.hpp
  include/rohit/runtime_simd.hpp
  include/rohit/numeric.hpp
  include/rohit/stream.hpp
  include/rohit/stream_concepts.hpp
  include/rohit/stream_io.hpp
)
# Keep installed headers ahead of older copies supplied by dependency include paths.
set_target_properties(serializer_runtime PROPERTIES EXPORT_NAME runtime EXPORT_NO_SYSTEM TRUE)
add_library(Serializer::runtime ALIAS serializer_runtime)

# Retain the original compiler-library API and target for existing compiler clients.
add_library(serializer_lib
  src/parser.cpp
  src/construction_parser.hpp
  src/construction_writer.hpp
  src/construction_defaults.hpp
  src/construction_defaults_parser.hpp
  src/callable_prefix_parser.hpp
  src/arithmetic_parser.hpp
  src/arithmetic_writer.hpp
  src/construction_contract.hpp
  src/construction_qualification.hpp
  src/numeric_expression_writer.hpp
  src/auto_size_writer.hpp
  src/schema_formats.hpp
  src/xml_schema.cpp
  src/xml_container_contract.cpp
  src/xml_container_contract.hpp
  src/xml_content_contract.cpp
  src/xml_content_contract.hpp
  src/xml_scalar_contract.cpp
  src/xml_scalar_contract.hpp
  src/xml_identity_contract.cpp
  src/xml_identity_contract.hpp
  src/xml_particle_contract.cpp
  src/xml_particle_contract.hpp
  src/xml_model_contract.cpp
  src/xml_model_contract.hpp
  src/document_profile_contract.cpp
  src/document_profile_contract.hpp
  src/xsd_import.cpp
  src/xsd_import.hpp
  src/xsd_bundle_output.hpp
  src/xml_authoring_parser.hpp
  src/package_writer.hpp
  src/native_package_contract.hpp
  src/xml_schema.hpp
  src/xml_portable_writer.cpp
  src/xml_portable_writer.hpp
  src/xml_portable_projection_writer.hpp
  src/xml_native_writer.cpp
  src/xml_native_choice_writer.hpp
  src/xml_native_model_writer.hpp
  src/xml_native_extension_writer.hpp
  src/xml_c_extension_runtime.inc
  src/xml_rust_extension_runtime.inc
  src/xml_native_identity_writer.hpp
  src/xml_native_extension_frames.hpp
  src/xml_c_identity_runtime.inc
  src/xml_rust_identity_runtime.inc
  src/xml_native_scalar_writer.hpp
  src/xml_mobile_map_writer.hpp
  src/xml_mobile_variant_writer.hpp
  src/xml_mobile_mixed_writer.hpp
  src/xml_mobile_scalar_writer.hpp
  src/xml_mobile_identity_writer.hpp
  src/xml_mobile_extension_writer.hpp
  src/xml_portable_extension_writer.hpp
  src/xml_java_extension_runtime.inc
  src/xml_csharp_extension_runtime.inc
  src/xml_go_extension_runtime.inc
  src/xml_native_writer.hpp
  src/xml_js_writer.hpp
  src/xml_js_token_runtime.inc
  src/xml_js_container_writer.hpp
  src/xml_js_container_runtime.inc
  src/xml_js_variant_writer.hpp
  src/xml_js_variant_runtime.inc
  src/xml_js_model_writer.hpp
  src/xml_js_scalar_writer.hpp
  src/xml_js_scalar_runtime.inc
  src/xml_js_namespace_runtime.inc
  src/xml_js_mixed_runtime.inc
  src/xml_js_mixed_writer.hpp
  src/xml_js_identity_writer.hpp
  src/xml_js_identity_runtime.inc
  src/xml_js_extension_writer.hpp
  src/xml_js_extension_runtime.inc
  src/xml_js_html_runtime.inc
  src/xml_js_lifecycle_writer.hpp
  src/xml_js_lifecycle_runtime.inc
  src/xml_js_provider_runtime.inc
  src/xml_js_projection_types.hpp
  src/xml_js_projection_writer.hpp
  src/xml_js_projection_runtime.inc
  src/xml_rust_writer.hpp
  src/xml_rust_container_writer.hpp
  src/xml_rust_map_runtime.inc
  src/xml_rust_variant_writer.hpp
  src/xml_rust_variant_runtime.inc
  src/xml_rust_mixed_writer.hpp
  src/xml_rust_mixed_runtime.inc
  src/xml_rust_model_runtime.inc
  src/xml_rust_scalar_runtime.inc
  src/xml_rust_html_runtime.inc
  src/xml_rust_projection_writer.hpp
  src/xml_rust_tokens_runtime.inc
  src/xml_csharp_writer.hpp
  src/xml_go_runtime.inc
  src/xml_go_map_runtime.inc
  src/xml_go_scalar_runtime.inc
  src/xml_go_namespace_runtime.inc
  src/xml_java_namespace_runtime.inc
  src/xml_csharp_namespace_runtime.inc
  src/xml_mobile_namespace.hpp
  src/xml_kotlin_namespace_runtime.inc
  src/xml_swift_namespace_runtime.inc
  src/xml_go_mixed_runtime.inc
  src/xml_go_identity_runtime.inc
  src/xml_go_html_runtime.inc
  src/xml_c_runtime.inc
  src/xml_c_tokens_runtime.inc
  src/xml_c_container_writer.hpp
  src/xml_c_map_runtime.inc
  src/xml_c_variant_writer.hpp
  src/xml_c_variant_runtime.inc
  src/xml_c_mixed_writer.hpp
  src/xml_c_mixed_runtime.inc
  src/xml_c_model_runtime.inc
  src/xml_c_scalar_runtime.inc
  src/xml_c_html_runtime.inc
  src/xml_csharp_runtime.inc
  src/xml_csharp_scalar_runtime.inc
  src/xml_csharp_mixed_runtime.inc
  src/xml_csharp_identity_runtime.inc
  src/xml_csharp_html_runtime.inc
  src/xml_csharp_model_runtime.inc
  src/xml_java_model_runtime.inc
  src/xml_go_model_runtime.inc
  src/xml_kotlin_runtime.inc
  src/xml_kotlin_output_runtime.inc
  src/xml_kotlin_map_runtime.inc
  src/xml_kotlin_variant_runtime.inc
  src/xml_kotlin_mixed_runtime.inc
  src/xml_kotlin_html_runtime.inc
  src/xml_kotlin_scalar_runtime.inc
  src/xml_kotlin_model_runtime.inc
  src/xml_kotlin_identity_runtime.inc
  src/xml_kotlin_extension_runtime.inc
  src/xml_kotlin_tokens_runtime.inc
  src/xml_swift_runtime.inc
  src/xml_swift_output_runtime.inc
  src/xml_swift_map_runtime.inc
  src/xml_swift_variant_runtime.inc
  src/xml_swift_mixed_runtime.inc
  src/xml_swift_html_runtime.inc
  src/xml_swift_scalar_runtime.inc
  src/xml_swift_model_runtime.inc
  src/xml_swift_identity_runtime.inc
  src/xml_swift_extension_runtime.inc
  src/xml_swift_extension_tokens_runtime.inc
  src/xml_swift_tokens_runtime.inc
  src/xml_swift_attribute_runtime.inc
  src/java_xml_runtime.inc
  src/xml_java_identity_runtime.inc
  src/xml_java_html_runtime.inc
  src/xml_java_scalar_runtime.inc
  src/xml_java_mixed_runtime.inc
  src/python_xml_runtime.inc
  src/python_xml_rich_runtime.inc
  src/python_xml_writer.hpp
  src/python_xml_identity.hpp
  src/python_xml_identity_runtime.inc
  src/python_xml_scalar_runtime.inc
  src/python_xml_model_runtime.inc
  src/python_xml_projection_runtime.inc
  src/python_xml_extension_runtime.inc
  src/python_html_runtime.inc
  src/python_package_runtime.inc
  src/python_package_mce_runtime.inc
  src/python_package_writer.hpp
  src/conversion_parser.hpp
  src/conversion_contract.hpp
  src/conversion_writer.hpp
  src/reference_parser.hpp
  src/reference_writer.hpp
  src/reference_portable_writer.hpp
  src/reference_validation_writer.hpp
  src/translator_writer.hpp
  src/stateless_constraint.hpp
  src/predicate_helpers.hpp
  src/typed_expression_writer.hpp
  src/statement_behavior_writer.hpp
  src/comparison_writer.hpp
  src/schema_policy.cpp
  src/schema_policy.hpp
  src/schema_scan.cpp
  src/schema_scan.hpp
  src/cpp_writer.cpp
  src/managed_schema.hpp
  src/managed_lowering.hpp
  src/native_schema.cpp
  src/c_writer.cpp
  src/c_compression_runtime.inc
  src/c_compression_validation_runtime.inc
  src/c_zstd_compression_runtime.inc
  src/c_lz4_compression_runtime.inc
  src/c_compression_stream_runtime.inc
  src/c_runtime.inc
  src/mobile_writer.cpp
  src/swift_runtime.inc
  src/swift_compression_runtime.inc
  src/kotlin_runtime.inc
  src/kotlin_compression_runtime.inc
  src/kotlin_zstd_compression_runtime.inc
  src/kotlin_lz4_compression_runtime.inc
  src/rust_writer.cpp
  src/rust_runtime.inc
  src/rust_compression_runtime.inc
  src/rust_compression_provider_runtime.inc
  src/python_writer.cpp
  src/python_runtime.inc
  src/python_compression_runtime.inc
  src/portable_writer.cpp
  src/js_runtime.inc
  src/js_compression_runtime.inc
  src/js_compression_declarations.inc
  src/javascript_package_runtime.inc
  src/javascript_package_writer_runtime.inc
  src/javascript_package_host_runtime.inc
  src/javascript_package_opc_runtime.inc
  src/javascript_package_mce_runtime.inc
  src/javascript_package_profile_runtime.inc
  src/javascript_package_declarations.inc
  src/javascript_package_writer.hpp
  src/go_runtime.inc
  src/go_compression_runtime.inc
  src/go_compression_provider_runtime.inc
  src/go_package_runtime.inc
  src/go_package_zip_reader_runtime.inc
  src/go_package_zip_writer_runtime.inc
  src/go_package_file_runtime.inc
  src/go_package_opc_runtime.inc
  src/go_package_metadata_runtime.inc
  src/go_package_mce_runtime.inc
  src/go_package_catalog_runtime.inc
  src/go_package_opc_writer_runtime.inc
  src/go_package_loader_runtime.inc
  src/go_package_composer_runtime.inc
  src/go_package_native_runtime.inc
  src/go_package_writer.hpp
  src/csharp_runtime.inc
  src/csharp_compression_runtime.inc
  src/csharp_optional_compression_runtime.inc
  src/csharp_zstd_compression_runtime.inc
  src/csharp_lz4_compression_runtime.inc
  src/csharp_package_writer.hpp
  src/c_package_writer.hpp
  src/c_package_runtime.inc
  src/c_package_file_runtime.inc
  src/c_package_opc_runtime.inc
  src/c_package_profile_runtime.inc
  src/csharp_package_contract_runtime.inc
  src/csharp_package_runtime.inc
  src/csharp_package_writer_runtime.inc
  src/csharp_package_opc_runtime.inc
  src/csharp_package_profile_runtime.inc
  src/java_writer.cpp
  src/java_runtime.inc
  src/java_package_writer.hpp
  src/java_package_runtime.inc
  src/java_package_writer_runtime.inc
  src/java_package_opc_runtime.inc
  src/java_package_xml_runtime.inc
  src/java_package_profile_runtime.inc
  src/kotlin_package_writer.hpp
  src/kotlin_package_contract_runtime.inc
  src/rust_package_writer.hpp
  src/rust_package_runtime.inc
  src/swift_package_writer.hpp
  src/swift_package_runtime.inc
  src/swift_package_opc_runtime.inc
  src/swift_package_profile_runtime.inc
  src/kotlin_package_runtime.inc
  src/kotlin_package_zip_runtime.inc
  src/kotlin_package_writer_runtime.inc
  src/kotlin_package_opc_runtime.inc
  src/kotlin_package_xml_runtime.inc
  src/kotlin_package_catalog_runtime.inc
  src/kotlin_package_profile_runtime.inc
  src/java_compression_runtime.inc
  src/java_deflate_validation_runtime.inc
  src/cpp_type_mapping.cpp
  src/cpp_naming.cpp
  src/cpp_naming.hpp
  src/output_options.cpp
  src/protobuf_schema.cpp
  src/protobuf_schema.hpp
  src/schema_compatibility.cpp
  include/rohit/schema_compatibility.hpp
  include/rohit/output_options.hpp
  include/rohit/serializer_creator.hpp
)
target_include_directories(serializer_runtime PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated/include>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_compile_features(serializer_runtime PUBLIC cxx_std_20)
target_compile_definitions(serializer_runtime PRIVATE
  SERIALIZER_ENABLE_SIMD=$<BOOL:${SERIALIZER_ENABLE_SIMD}>
  SERIALIZER_WITH_ZSTD=$<BOOL:${SERIALIZER_WITH_ZSTD}>
  SERIALIZER_WITH_LZ4=$<BOOL:${SERIALIZER_WITH_LZ4}>
  SERIALIZER_WITH_ZLIB=$<BOOL:${SERIALIZER_WITH_ZLIB}>
  SERIALIZER_HAS_XML=$<BOOL:${SERIALIZER_WITH_XML}>)
# Installed shared runtimes must expose capabilities without their private dependency links.
foreach(backend IN ITEMS ZSTD LZ4 ZLIB XML)
  set_property(TARGET serializer_runtime PROPERTY "SERIALIZER_WITH_${backend}"
    "${SERIALIZER_WITH_${backend}}")
  set_property(TARGET serializer_runtime APPEND PROPERTY EXPORT_PROPERTIES
    "SERIALIZER_WITH_${backend}")
endforeach()
target_compile_definitions(serializer_lib PRIVATE
  SERIALIZER_ENABLE_SIMD=$<BOOL:${SERIALIZER_ENABLE_SIMD}>
  SERIALIZER_HAS_XML=$<BOOL:${SERIALIZER_WITH_XML}>)
target_link_libraries(serializer_runtime PRIVATE ${serializer_compression_libraries})
if(SERIALIZER_WITH_XML)
  target_link_libraries(serializer_runtime PRIVATE LibXml2::LibXml2)
endif()
target_link_libraries(serializer_lib PUBLIC Serializer::runtime)

# Keep AVX2 flags off the dispatcher and consumers, including in unity builds.
set(serializer_x64 FALSE)
if(MSVC)
  if(CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
    set(serializer_x64 TRUE)
  endif()
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" serializer_processor)
  if(serializer_processor MATCHES "^(x86_64|amd64|x64)$")
    set(serializer_x64 TRUE)
  endif()
  # A source-specific -mavx2 flag cannot be shared with an ARM slice of a universal binary.
  if(APPLE AND CMAKE_OSX_ARCHITECTURES AND NOT CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
    set(serializer_x64 FALSE)
  endif()
endif()
if(SERIALIZER_ENABLE_SIMD AND serializer_x64 AND
    CMAKE_CXX_COMPILER_ID MATCHES "^(MSVC|GNU|Clang|AppleClang)$")
  set(serializer_avx2_sources src/schema_scan_avx2.cpp src/runtime_simd_avx2.cpp)
  target_sources(serializer_lib PRIVATE src/schema_scan_avx2.cpp)
  target_sources(serializer_runtime PRIVATE src/runtime_simd_avx2.cpp)
  target_compile_definitions(serializer_lib PRIVATE SERIALIZER_HAS_AVX2_SCANNER=1)
  target_compile_definitions(serializer_runtime PRIVATE SERIALIZER_HAS_AVX2_SCANNER=1)
  if(MSVC)
    set_source_files_properties(${serializer_avx2_sources} PROPERTIES COMPILE_OPTIONS /arch:AVX2)
  else()
    set_source_files_properties(${serializer_avx2_sources} PROPERTIES COMPILE_OPTIONS -mavx2)
  endif()
  set_source_files_properties(${serializer_avx2_sources} PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE)
endif()

set_target_properties(serializer_lib PROPERTIES EXPORT_NO_SYSTEM TRUE)
add_library(Serializer::serializer_lib ALIAS serializer_lib)

add_executable(serializer src/serializer.cpp src/command_line.hpp src/compiler_artifact_output.hpp)
if(WIN32)
  # Source-based example generation must find enabled shared compression runtimes.
  add_custom_command(TARGET serializer POST_BUILD
    COMMAND "${CMAKE_COMMAND}" "-DDLL_FILES=$<TARGET_RUNTIME_DLLS:serializer>"
      "-DOUTPUT_DIRECTORY=$<TARGET_FILE_DIR:serializer>"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_runtime_dlls.cmake"
    VERBATIM)
endif()
target_link_libraries(serializer PRIVATE serializer_lib)
add_executable(Serializer::serializer ALIAS serializer)

# The same generation API is available from add_subdirectory and the installed package.
include(cmake/serializer_generate.cmake)
include(cmake/serializer_generate_java.cmake)
include(cmake/serializer_generate_source.cmake)

# Generate runtime records after the compiler, avoiding a compiler/library dependency cycle.
if(SERIALIZER_BUILD_MANAGED)
  add_library(serializer_managed INTERFACE)
  set_target_properties(serializer_managed PROPERTIES EXPORT_NAME managed EXPORT_NO_SYSTEM TRUE)
  add_library(Serializer::managed ALIAS serializer_managed)
  serializer_generate(TARGET serializer_managed SCHEMAS schemas/managed_records.serializer
    schemas/collaboration_records.serializer
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/schemas/runtime_records.ini"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/managed/include/rohit"
    CODING_STANDARD serializer)
  # Generate installed records even when no examples or consumers link the interface target.
  add_custom_target(serializer_managed_records ALL)
  add_dependencies(serializer_managed_records serializer_managed_serializer_headers)
  target_include_directories(serializer_managed INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/managed/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
endif()

if(SERIALIZER_INSTALL)
  include(CMakePackageConfigHelpers)
  set(serializer_package_directory "${CMAKE_INSTALL_LIBDIR}/cmake/Serializer")
  # LANGUAGES NONE projects cannot infer multiarch or lib64 search suffixes.
  set(serializer_neutral_package_directory "${CMAKE_INSTALL_DATADIR}/cmake/Serializer")
  configure_package_config_file(cmake/serializer_config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/SerializerConfig.cmake"
    INSTALL_DESTINATION "${serializer_package_directory}"
  )
  configure_package_config_file(cmake/serializer_config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/neutral/SerializerConfig.cmake"
    INSTALL_DESTINATION "${serializer_neutral_package_directory}"
  )
  # Host-tool discovery must not compare the host executable's ABI with the target ABI.
  configure_file(cmake/serializer_config_version.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/SerializerConfigVersion.cmake" @ONLY)
  write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/SerializerRuntimeConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMajorVersion)
  write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/SerializerCompilerConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMajorVersion ARCH_INDEPENDENT)
  install(TARGETS serializer_runtime serializer_lib EXPORT SerializerTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  )
  install(TARGETS serializer EXPORT SerializerCompilerTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
  if(SERIALIZER_BUILD_MANAGED)
    install(TARGETS serializer_managed EXPORT SerializerTargets)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/managed/include/rohit/managed_records.hpp"
      "${CMAKE_CURRENT_BINARY_DIR}/managed/include/rohit/collaboration_records.hpp"
      DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/rohit")
    install(FILES schemas/managed_records.serializer schemas/collaboration_records.serializer
      DESTINATION "${CMAKE_INSTALL_DATADIR}/Serializer/schemas")
  endif()
  # Install record-dependent APIs only when their generated dependencies are available.
  set(serializer_disabled_header_patterns)
  if(NOT SERIALIZER_BUILD_MANAGED)
    foreach(header IN ITEMS managed.hpp managed_identity_allocation.hpp managed_editable.hpp managed_registry.hpp managed_delta.hpp collaboration_history.hpp collaboration_sessions.hpp
        managed_collaboration.hpp managed_collaboration_hooks.hpp managed_local_collaboration.hpp managed_migration.hpp)
      list(APPEND serializer_disabled_header_patterns PATTERN "${header}" EXCLUDE)
    endforeach()
  endif()
  install(DIRECTORY include/rohit DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.hpp" ${serializer_disabled_header_patterns})
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/include/rohit/version.hpp"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/rohit")
  set(serializer_package_directories "${serializer_package_directory}"
    "${serializer_neutral_package_directory}")
  list(REMOVE_DUPLICATES serializer_package_directories)
  foreach(package_directory IN LISTS serializer_package_directories)
    if(package_directory STREQUAL serializer_package_directory)
      set(package_config "${CMAKE_CURRENT_BINARY_DIR}/SerializerConfig.cmake")
    else()
      set(package_config "${CMAKE_CURRENT_BINARY_DIR}/neutral/SerializerConfig.cmake")
    endif()
    install(EXPORT SerializerTargets NAMESPACE Serializer::
      DESTINATION "${package_directory}")
    install(EXPORT SerializerCompilerTargets NAMESPACE Serializer::
      DESTINATION "${package_directory}")
    install(FILES "${package_config}"
      "${CMAKE_CURRENT_BINARY_DIR}/SerializerConfigVersion.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/SerializerRuntimeConfigVersion.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/SerializerCompilerConfigVersion.cmake"
      cmake/serializer_generate.cmake cmake/serializer_generate_java.cmake
      cmake/serializer_generate_source.cmake
      cmake/serializer_include_outputs.cmake
      cmake/serializer_compression.cmake
      DESTINATION "${package_directory}")
  endforeach()
  install(FILES LICENSE LICENSE-RUNTIME LICENSE-GENERATED
    DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/Serializer")
endif()

# Keep this project's warnings local when consumed through add_subdirectory.
foreach(target IN ITEMS serializer_runtime serializer_lib serializer)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  elseif(UNIX)
    target_compile_options(${target} PRIVATE -Wall -Werror -Wpedantic -Wextra -Weffc++ -Wsuggest-override)
  endif()
endforeach()

if(SERIALIZER_BUILD_FUZZERS)
  include(cmake/serializer_fuzz_runtime.cmake)
endif()

# Formatting is a generator-time dependency; runtime users do not need clang-format.
if(SERIALIZER_BUILD_TESTS OR SERIALIZER_BUILD_BENCHMARKS OR SERIALIZER_BUILD_FUZZERS
    OR SERIALIZER_BUILD_STYLE_EXAMPLES OR SERIALIZER_BUILD_IOSTREAM_EXAMPLES
    OR (SERIALIZER_BUILD_MANAGED AND SERIALIZER_BUILD_MANAGED_EXAMPLES)
    OR SERIALIZER_BUILD_COMPRESSION_EXAMPLES)
  include(cmake/serializer_find_clang_format.cmake)
  serializer_find_clang_format()
endif()

if(SERIALIZER_BUILD_TESTS OR SERIALIZER_BUILD_FUZZERS OR SERIALIZER_BUILD_IOSTREAM_EXAMPLES
    OR SERIALIZER_BUILD_COMPRESSION_EXAMPLES OR SERIALIZER_BUILD_INTEROP_EXAMPLES
    OR SERIALIZER_BUILD_XML_EXAMPLES OR SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES)
  enable_testing()
endif()
if(SERIALIZER_BUILD_TESTS)
  find_package(GTest 1.18.0 CONFIG REQUIRED)
  add_subdirectory(test)
endif()
if(SERIALIZER_BUILD_MANAGED AND (SERIALIZER_BUILD_TESTS OR SERIALIZER_BUILD_MANAGED_EXAMPLES))
  add_subdirectory(example/managed)
endif()

if(SERIALIZER_BUILD_BENCHMARKS OR SERIALIZER_BUILD_FUZZERS)
  add_subdirectory(qualification)
endif()

if(SERIALIZER_BUILD_STYLE_EXAMPLES OR SERIALIZER_BUILD_TESTS)
  add_subdirectory(example/coding_styles)
endif()

if(SERIALIZER_BUILD_IOSTREAM_EXAMPLES OR SERIALIZER_BUILD_TESTS)
  add_subdirectory(example/iostream)
endif()

if(SERIALIZER_BUILD_COMPRESSION_EXAMPLES OR SERIALIZER_BUILD_TESTS)
  add_subdirectory(example/compression)
endif()
if(SERIALIZER_BUILD_XML_EXAMPLES)
  if(NOT SERIALIZER_WITH_XML OR NOT SERIALIZER_WITH_ZLIB)
    message(FATAL_ERROR "XML examples require SERIALIZER_WITH_XML and SERIALIZER_WITH_ZLIB")
  endif()
endif()
if(SERIALIZER_WITH_XML AND SERIALIZER_WITH_ZLIB AND
    (SERIALIZER_BUILD_XML_EXAMPLES OR SERIALIZER_BUILD_TESTS))
  add_subdirectory(example/cpp/xml)
  add_subdirectory(example/opc)
endif()
if(SERIALIZER_WITH_XML AND (SERIALIZER_BUILD_XML_EXAMPLES OR SERIALIZER_BUILD_TESTS
    OR SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES))
  add_subdirectory(example/xml/cpp)
endif()

if(SERIALIZER_BUILD_JAVA_EXAMPLES)
  add_subdirectory(example/java)
endif()

if(SERIALIZER_BUILD_TESTS OR SERIALIZER_BUILD_STYLE_EXAMPLES OR SERIALIZER_BUILD_JAVA_EXAMPLES)
  add_subdirectory(example/includes)
endif()

if(SERIALIZER_BUILD_TESTS OR SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES)
  add_subdirectory(example/generics)
  add_subdirectory(example/cpp/expressions)
  add_subdirectory(example/cpp/custom_code)
  add_subdirectory(example/cpp/readonly_getters)
  add_subdirectory(example/cpp/arithmetic_operators)
  add_subdirectory(example/cpp/reciprocal_quantities)
  add_subdirectory(example/cpp/construction_defaults)
  add_subdirectory(example/references/cpp)
  add_subdirectory(example/address_references/cpp)
endif()

if(SERIALIZER_BUILD_INTEROP_EXAMPLES)
  add_subdirectory(example/interoperability)
endif()

if(SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES)
  find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
  if(SERIALIZER_WITH_XML)
    foreach(profile IN ITEMS configured xsd)
      if(profile STREQUAL "configured")
        set(xml_example_target serializer_xml_cpp)
        set(xml_example_config codegen.ini)
      else()
        set(xml_example_target serializer_xml_cpp_xsd)
        set(xml_example_config xsd.ini)
      endif()
      add_test(NAME serializer_xml_languages_${profile}
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/example/xml/run.py"
          --compiler "$<TARGET_FILE:serializer>"
          --cpp-executable "$<TARGET_FILE:${xml_example_target}>"
          --config "${PROJECT_SOURCE_DIR}/example/xml/${xml_example_config}"
          --build "${PROJECT_BINARY_DIR}/xml_languages_${profile}"
          "--wsl-languages=${SERIALIZER_EXAMPLE_WSL_LANGUAGES}")
      set_tests_properties(serializer_xml_languages_${profile} PROPERTIES
        TIMEOUT 1800 LABELS serializer_xml)
    endforeach()
    string(REPLACE "," ";" xml_wsl_languages "${SERIALIZER_EXAMPLE_WSL_LANGUAGES}")
    set(xml_kotlin_private_options)
    if("kotlin" IN_LIST xml_wsl_languages)
      list(APPEND xml_kotlin_private_options --wsl-languages=kotlin)
    endif()
    add_test(NAME serializer_xml_kotlin_private
      COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/test/xml_kotlin_private_test.py"
        --compiler "$<TARGET_FILE:serializer>"
        --build "${PROJECT_BINARY_DIR}/xml_kotlin_private" ${xml_kotlin_private_options})
    set_tests_properties(serializer_xml_kotlin_private PROPERTIES TIMEOUT 1800 LABELS serializer_xml)
    if(TARGET xml_shapes_cpp)
      add_test(NAME serializer_xml_shapes_languages
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/test/xml_shapes_test.py"
          --compiler "$<TARGET_FILE:serializer>" --cpp-executable "$<TARGET_FILE:xml_shapes_cpp>"
          --build "${PROJECT_BINARY_DIR}/xml_shapes_languages"
          "--wsl-languages=${SERIALIZER_EXAMPLE_WSL_LANGUAGES}")
      set_tests_properties(serializer_xml_shapes_languages PROPERTIES
        TIMEOUT 1800 LABELS serializer_xml)
    endif()
  endif()
  set(native_example_options "--wsl-languages=${SERIALIZER_EXAMPLE_WSL_LANGUAGES}")
  if(SERIALIZER_EXAMPLE_SANITIZERS)
    list(APPEND native_example_options --sanitize)
  endif()
  set(big_endian_example_options)
  if(SERIALIZER_EXAMPLE_BIG_ENDIAN)
    list(APPEND big_endian_example_options --big-endian)
  endif()
  add_test(NAME serializer_language_examples
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/example/run.py"
      --compiler "$<TARGET_FILE:serializer>" --cpp-library "$<TARGET_FILE:serializer_runtime>"
      --build "${PROJECT_BINARY_DIR}/language_examples" ${native_example_options}
      ${big_endian_example_options})
  add_test(NAME serializer_native_codecs
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/test/native/run.py"
      --compiler "$<TARGET_FILE:serializer>" --build "${PROJECT_BINARY_DIR}/native_codecs"
      ${native_example_options})
  add_test(NAME serializer_magic_languages
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/test/magic_languages.py"
      --compiler "$<TARGET_FILE:serializer>" --cpp-library "$<TARGET_FILE:serializer_runtime>"
      --build "${PROJECT_BINARY_DIR}/magic_languages"
      "--wsl-languages=${SERIALIZER_EXAMPLE_WSL_LANGUAGES}")
  add_test(NAME serializer_typed_magic_languages
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/test/typed_magic_languages.py"
      --compiler "$<TARGET_FILE:serializer>" --cpp-library "$<TARGET_FILE:serializer_runtime>"
      --build "${PROJECT_BINARY_DIR}/typed_magic_languages"
      "--wsl-languages=${SERIALIZER_EXAMPLE_WSL_LANGUAGES}")
  add_test(NAME serializer_generic_languages
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/example/generics/run.py"
      --compiler "$<TARGET_FILE:serializer>" --cpp-library "$<TARGET_FILE:serializer_runtime>"
      --build "${PROJECT_BINARY_DIR}/generic_languages" --language all
      "--wsl-languages=${SERIALIZER_EXAMPLE_WSL_LANGUAGES}")
  foreach(behavior_suite IN ITEMS behavior_languages native_behavior_languages optional_languages construction_languages record_comparison_languages auto_size_languages conversion_languages reference_languages translator_languages stateless_languages readonly_function_languages)
    add_test(NAME "serializer_${behavior_suite}"
      COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/test/${behavior_suite}.py"
        --compiler "$<TARGET_FILE:serializer>" --cpp-library "$<TARGET_FILE:serializer_runtime>"
        --build "${PROJECT_BINARY_DIR}/${behavior_suite}"
        "--wsl-languages=${SERIALIZER_EXAMPLE_WSL_LANGUAGES}")
    set_tests_properties("serializer_${behavior_suite}" PROPERTIES TIMEOUT 1800 LABELS serializer_native)
  endforeach()
  add_test(NAME serializer_construction_c_ownership
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/test/construction_c_ownership.py"
      --compiler "$<TARGET_FILE:serializer>"
      --build "${PROJECT_BINARY_DIR}/construction_c_ownership"
      "--wsl-languages=${SERIALIZER_EXAMPLE_WSL_LANGUAGES}")
  set_tests_properties(serializer_construction_c_ownership PROPERTIES TIMEOUT 1800 LABELS serializer_native)
  set_tests_properties(serializer_generic_languages PROPERTIES TIMEOUT 1800 LABELS serializer_native)
  add_test(NAME serializer_optional_variant_languages
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/test/optional_languages.py"
      --compiler "$<TARGET_FILE:serializer>" --cpp-library "$<TARGET_FILE:serializer_runtime>"
      --build "${PROJECT_BINARY_DIR}/optional_variant_languages" --variant-only
      "--wsl-languages=${SERIALIZER_EXAMPLE_WSL_LANGUAGES}")
  set_tests_properties(serializer_optional_variant_languages PROPERTIES TIMEOUT 1800 LABELS serializer_native)
  set_tests_properties(serializer_language_examples serializer_native_codecs PROPERTIES
    TIMEOUT 1800 LABELS serializer_native)
  foreach(feature IN ITEMS history journal collaboration authentication)
    add_test(NAME "serializer_managed_wire_${feature}"
      COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/example/managed/multilanguage/run.py"
        --compiler "$<TARGET_FILE:serializer>" --cpp-library "$<TARGET_FILE:serializer_runtime>"
        --build "${PROJECT_BINARY_DIR}/managed_wire" --feature "${feature}"
        ${native_example_options})
    set_tests_properties("serializer_managed_wire_${feature}" PROPERTIES
      TIMEOUT 1800 LABELS "serializer_native;serializer_managed_wire")
  endforeach()
endif()
