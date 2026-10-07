# Verify the executable contract without requiring a formatter or Java toolchain.
file(MAKE_DIRECTORY "${DIRECTORY}")
set(schema "${DIRECTORY}/model.serializer")
set(cpp "${DIRECTORY}/model.hpp")
set(java "${DIRECTORY}/Model.java")
file(WRITE "${schema}" "serializer version 1;\nclass account_record stable_ids { public uint32 account_id (1); }\n")
file(WRITE "${DIRECTORY}/output.ini" "[output]\nlanguage = java\n[cpp]\nformat = false\ncoding_standard = google\n[java]\ncoding_standard = google\npackage = configured.package_name\n")

# Run a valid command and include its diagnostics if it fails.
function(succeed)
  execute_process(COMMAND "${GENERATOR}" ${ARGN}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Command failed: ${ARGN}\n${output}\n${error}")
  endif()
  set(last_output "${output}" PARENT_SCOPE)
endfunction()

# A rejected invocation must report a useful error and preserve every existing output.
function(reject expected)
  file(SHA256 "${cpp}" cpp_before)
  file(SHA256 "${java}" java_before)
  execute_process(COMMAND "${GENERATOR}" ${ARGN}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(result EQUAL 0 OR NOT error MATCHES "${expected}")
    message(FATAL_ERROR "Expected rejection '${expected}': ${ARGN}\n${result}\n${output}\n${error}")
  endif()
  file(SHA256 "${cpp}" cpp_after)
  file(SHA256 "${java}" java_after)
  if(NOT cpp_before STREQUAL cpp_after OR NOT java_before STREQUAL java_after)
    message(FATAL_ERROR "Rejected invocation changed an existing output")
  endif()
endfunction()

succeed(--version)
if(NOT last_output MATCHES "Serializer compiler ${VERSION}" OR
    NOT last_output MATCHES "Supported schema language version: 1[.]2[.]0[\r\n]")
  message(FATAL_ERROR "Incorrect compiler version: ${last_output}")
endif()
succeed(-v)
succeed(-h)
if(NOT last_output MATCHES "--java.output" OR NOT last_output MATCHES "--cpp.coding_standard")
  message(FATAL_ERROR "Help is missing backend options")
endif()
succeed(-i "${schema}" -c "${DIRECTORY}/output.ini" -l cpp --language=java
  "--cpp.output=${cpp}" --java.output "${java}"
  --cpp.coding_standard serializer --java.coding_standard oracle --java.package direct.package_name)
file(READ "${cpp}" cpp_source)
file(READ "${java}" java_source)
if(NOT cpp_source MATCHES "class account_record" OR
    NOT java_source MATCHES "package direct.package_name;" OR
    NOT java_source MATCHES "    public static final class AccountRecord")
  message(FATAL_ERROR "Language-specific overrides did not win over config values")
endif()
# Override a config-selected language using the comma form, then clear the Java package.
succeed(--input "${schema}" --config "${DIRECTORY}/output.ini" --language cpp,java
  --cpp.output "${cpp}" --java.output "${java}" --java.package=)
file(READ "${java}" java_source)
if(java_source MATCHES "package configured")
  message(FATAL_ERROR "Empty Java package override did not clear the config")
endif()
file(WRITE "${DIRECTORY}/multi.ini" "[output]\nlanguage = cpp, java\n[cpp]\nformat = false\n")
succeed(-i "${schema}" -c "${DIRECTORY}/multi.ini" --cpp.output "${cpp}" --java.output "${java}")
succeed(-i "${schema}" -o "${java}" -l java)
succeed(-i "${schema}" -o "${cpp}" --cpp.format false --cpp.protobuf true)
file(READ "${cpp}" protobuf_source)
if(NOT protobuf_source MATCHES "serializer_protobuf_write" OR
    NOT protobuf_source MATCHES "rohit/protobuf.hpp")
  message(FATAL_ERROR "Protobuf option did not generate direct protocol support")
endif()
reject("cpp.protobuf must be true or false" -i "${schema}" -o "${cpp}" --cpp.protobuf maybe)
succeed(-i "${schema}" -o "${cpp}" --cpp.format false --cpp.protobuf false)
file(READ "${cpp}" plain_source)
if(plain_source MATCHES "serializer_protobuf_write")
  message(FATAL_ERROR "Disabled Protobuf output retained generated protocol methods")
endif()
reject("Unknown argument" input "${schema}")
reject("Missing value" --input)
reject("Missing value" --input --output "${cpp}")
reject("Empty value" --input=)
reject("Repeated argument" -i "${schema}" --input "${schema}")
reject("Flag does not accept" --version=true)
reject("Unknown argument" -input "${schema}")
reject("Unsupported output language" -i "${schema}" -l cpp,unknown_backend)
reject("Unsupported output language" -i "${schema}" -l cpp,)
reject("Repeated output language" -i "${schema}" -l cpp -l cpp)
reject("multiple languages" -i "${schema}" -l cpp,java -o "${cpp}")
reject("Missing output path" -i "${schema}" -l cpp,java --cpp.output "${cpp}")
reject("unselected language" -i "${schema}" -o "${cpp}" --java.output "${java}")
reject("only one" -i "${schema}" -o "${cpp}" --cpp.output "${cpp}")
reject("distinct files" -i "${schema}" -l cpp,java --cpp.output "${cpp}" --java.output "${cpp}")
reject("must not overwrite" -i "${schema}" -o "${schema}")
reject("existing parent" -i "${schema}" -l cpp,java --cpp.output "${cpp}"
  --java.output "${DIRECTORY}/absent/Model.java")
reject("Java identifier" -i "${schema}" -l cpp,java --cpp.output "${cpp}" --java.output "${java}"
  --cpp.format false --java.package not-valid)
reject(".java extension" -i "${schema}" -l java --output "${cpp}")
reject("must not overwrite" -i "${schema}" --config "${DIRECTORY}/output.ini"
  --output "${DIRECTORY}/output.ini")
# A failure in the second backend must also preserve the first backend's file.
reject("clang-format" -i "${schema}" -l java,cpp --cpp.output "${cpp}" --java.output "${java}"
  --cpp.clang_format "${DIRECTORY}/missing-clang-format")
# Clearing an inherited format file allows formatting to be explicitly disabled.
file(WRITE "${DIRECTORY}/format.ini" "[cpp]\nformat_file = absent.clang-format\n")
succeed(-i "${schema}" -o "${cpp}" -c "${DIRECTORY}/format.ini"
  --cpp.format_file= --cpp.format false)
file(WRITE "${DIRECTORY}/legacy.def" "class account {}")
reject("Input must use .serializer" -i "${DIRECTORY}/legacy.def" -o "${cpp}")
foreach(header IN ITEMS "" "serializer version 0;" "serializer version 2;"
    "serializer version 999999999999999999999;" "serializer version -1;"
    "serializer version 1" "serializer version 1.0;" "serializer version1;"
    "serializer version 1..0;" "serializer version 1.0.0.0;"
    "serializer version 1.2.1;" "serializer version 1.3.0;" "serializer version 2.0.0;"
    "serializer version 1.999999999999999999999.0;"
    "serializer version 1.0.999999999999999999999;"
    "serializer version 1; serializer version 1;")
  file(WRITE "${schema}" "${header}\nclass account {}\n")
  reject("Serializer:" -i "${schema}" -o "${cpp}" --cpp.format false)
endforeach()
file(WRITE "${schema}" "// comment\n/* license */ serializer /* schema */ version 1;\nclass account {}\n")
succeed(-i "${schema}" -o "${cpp}" --cpp.format=false)

# The legacy alias and full language version must produce identical C++ and Java codecs.
foreach(language_version IN ITEMS 1 1.0.0 1.0.1 1.1.0 1.1.1 1.2.0)
  file(WRITE "${schema}" "serializer version ${language_version};\n"
    "class account stable_ids { public uint32 id (7); }\n")
  succeed(-i "${schema}" -l cpp,java --cpp.output "${cpp}" --java.output "${java}"
    --cpp.format false)
  file(SHA256 "${cpp}" cpp_hash)
  file(SHA256 "${java}" java_hash)
  if(language_version STREQUAL "1")
    set(legacy_cpp_hash "${cpp_hash}")
    set(legacy_java_hash "${java_hash}")
  elseif(NOT cpp_hash STREQUAL legacy_cpp_hash OR NOT java_hash STREQUAL legacy_java_hash)
    message(FATAL_ERROR "Schema language alias changed generated codecs")
  endif()
endforeach()

# Both backends consume one include graph, and dependency output tracks the entire graph.
file(MAKE_DIRECTORY "${DIRECTORY}/shared")
file(WRITE "${DIRECTORY}/shared/common.serializer"
  "serializer version 1; namespace models { class account { public uint32 id (7); } }\n")
file(WRITE "${schema}" "serializer version 1.0.0; include shared/common;\n"
  "include shared/./common.serializer; namespace models { class request { public account owner; } }\n")
set(depfile "${DIRECTORY}/model.d")
succeed(-i "${schema}" -l cpp,java --cpp.output "${cpp}" --java.output "${java}"
  --cpp.format false --depfile "${depfile}")
file(READ "${depfile}" dependencies)
if(NOT dependencies MATCHES "common.serializer" OR NOT dependencies MATCHES "model.serializer" OR
    NOT dependencies MATCHES "model.hpp" OR NOT dependencies MATCHES "Model.java")
  message(FATAL_ERROR "Missing schema or output in dependency file: ${dependencies}")
endif()
file(READ "${java}" java_source)
string(REGEX MATCHALL "class Models" containers "${java_source}")
list(LENGTH containers container_count)
if(NOT container_count EQUAL 1)
  message(FATAL_ERROR "Reopened namespace must produce one Java container")
endif()
# Each dependency validates its own version, independently of the entry's header.
file(WRITE "${DIRECTORY}/shared/common.serializer"
  "serializer version 1.0.0; namespace models { class account { public uint32 id (7); } }\n")
file(WRITE "${schema}" "serializer version 1; include shared/common;\n"
  "namespace models { class request { public account owner; } }\n")
succeed(-i "${schema}" -l cpp,java --cpp.output "${cpp}" --java.output "${java}"
  --cpp.format false --depfile "${depfile}")
file(WRITE "${DIRECTORY}/shared/common.serializer" "serializer version 1.2.1; class account {}")
reject("Unsupported schema language version" -i "${schema}" -o "${cpp}" --cpp.format false)
file(WRITE "${DIRECTORY}/shared/common.serializer"
  "serializer version 1.0.0; namespace models { class account { public uint32 id (7); } }\n")
reject("Depfile must not overwrite" -i "${schema}" -o "${cpp}" --cpp.format false --depfile "${cpp}")
reject("Depfile must not overwrite" -i "${schema}" -o "${cpp}" --cpp.format false --depfile "${schema}")
reject("Depfile must not overwrite" -i "${schema}" -o "${cpp}" --cpp.format false
  --depfile "${DIRECTORY}/shared/common.serializer")
reject("Depfile requires" -i "${schema}" -o "${cpp}" --cpp.format false
  --depfile "${DIRECTORY}/absent/file.d")
file(SHA256 "${depfile}" dependency_before)
file(WRITE "${DIRECTORY}/shared/common.serializer" "serializer version 1; include absent;")
reject("absent.serializer" -i "${schema}" -o "${cpp}" --cpp.format false --depfile "${depfile}")
file(SHA256 "${depfile}" dependency_after)
if(NOT dependency_before STREQUAL dependency_after)
  message(FATAL_ERROR "Failed include parsing changed the dependency file")
endif()
# Managed configuration participates in ordinary CLI precedence and atomic output validation.
file(WRITE "${schema}" "serializer version 1; class entry managed { public int32 amount (1); } class ledger { public managed map(uint64) entry entries (1); }")
file(WRITE "${DIRECTORY}/managed.ini" "[managed]\nid_type = uint64\n[cpp]\nformat = false\n")
succeed(-i "${schema}" -o "${cpp}" --config "${DIRECTORY}/managed.ini")
file(READ "${cpp}" managed_source)
if(NOT managed_source MATCHES "uint64_t persistent_id")
  message(FATAL_ERROR "Managed configuration did not select uint64 IDs")
endif()
succeed(-i "${schema}" -o "${cpp}" --config "${DIRECTORY}/managed.ini" --managed.id_type uint32)
file(READ "${cpp}" managed_source)
if(NOT managed_source MATCHES "uint32_t persistent_id" OR NOT managed_source MATCHES "class ledger_editor")
  message(FATAL_ERROR "Managed CLI override or generated editor is missing")
endif()
reject("managed.id_type must be" -i "${schema}" -o "${cpp}" --managed.id_type uint16 --cpp.format false)
succeed(-i "${schema}" -o "${cpp}" --config "${DIRECTORY}/managed.ini" --managed.separate_values true)
file(READ "${cpp}" managed_source)
if(NOT managed_source MATCHES "class managed_ledger_storage")
  message(FATAL_ERROR "Opt-in managed value separation did not generate storage companions")
endif()
file(WRITE "${DIRECTORY}/managed.ini" "[managed]\nseparate_values = true\n[cpp]\nformat = false\n")
succeed(-i "${schema}" -o "${cpp}" --config "${DIRECTORY}/managed.ini" --managed.separate_values false)
file(READ "${cpp}" managed_source)
if(managed_source MATCHES "class managed_ledger_storage" OR NOT managed_source MATCHES "managed.direct.v1:")
  message(FATAL_ERROR "CLI override did not restore direct managed identity")
endif()
reject("managed.separate_values must be" -i "${schema}" -o "${cpp}" --managed.separate_values maybe --cpp.format false)
reject("only by the C\\+\\+ backend" -i "${schema}" --language cpp,java --cpp.output "${cpp}" --java.output "${java}" --cpp.format false)
file(WRITE "${schema}" "serializer version 1; class entry {} class ledger { public managed entry item (1); }")
reject("managed members require" -i "${schema}" -o "${cpp}" --cpp.format false)

# Release policies fold during generation; pinned dates and promoted warnings preserve atomic output.
file(WRITE "${schema}" "serializer version 1; class record { public version { 10 } releases { 8 { \"2024-10-01\" }; 9 { \"2025-04-01\" }; 10 { \"2026-10-01\" }; } policy { max_age { 2 years }; }; public uint32 value (2); }")
succeed(-i "${schema}" -o "${cpp}" --cpp.format false --version-policy-as-of 2026-10-06 --verbose)
if(NOT last_output MATCHES "version policy as of 2026-10-06, minimum 9")
  message(FATAL_ERROR "Pinned release policy was not folded: ${last_output}")
endif()
succeed(-i "${schema}" -o "${cpp}" --cpp.format false --version-policy-as-of 2027-10-06 --verbose)
if(NOT last_output MATCHES "minimum 10")
  message(FATAL_ERROR "Changing the pinned date did not advance compatibility")
endif()
file(READ "${cpp}" policy_source)
if(policy_source MATCHES "2026-10-01|releases|max_age|system_clock")
  message(FATAL_ERROR "Generated runtime contains release policy metadata")
endif()
succeed(-i "${schema}" -o "${cpp}" --cpp.format false --verbose)
if(NOT last_output MATCHES "version policy as of [0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]")
  message(FATAL_ERROR "Default reference date was not captured")
endif()
reject("Invalid release policy calendar date" -i "${schema}" -o "${cpp}" --cpp.format false --version-policy-as-of 2026-02-29)
reject("YYYY-MM-DD" -i "${schema}" -o "${cpp}" --cpp.format false --version-policy-as-of 2026-2-01)
reject("warning treated as error" -i "${schema}" -o "${cpp}" --cpp.format false --version-policy-as-of 2030-10-06 --version-policy-warnings-as-errors)
execute_process(COMMAND "${GENERATOR}" -i "${schema}" -o "${cpp}" --cpp.format false --version-policy-as-of 2030-10-06
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT error MATCHES "current version remains readable")
  message(FATAL_ERROR "Expired current version must warn without failing: ${output}\n${error}")
endif()
succeed(-i "${schema}" --check-against "${schema}" --compatibility-protocol binary_none --version-policy-as-of 2026-10-06 --verbose)

# Compact fields require their declared contract in every included file.
foreach(declaration IN ITEMS
    "public compact_prefix strict uint32 value (3) { 32 };"
    "public compact_varint uint64 value (3) { 32 };")
  foreach(language_version IN ITEMS 1 1.0.0 1.1.0 1.1.999)
    file(WRITE "${schema}" "serializer version ${language_version}; class account { ${declaration} }")
    reject("requires serializer version 1.2.0" -i "${schema}" -o "${cpp}" --cpp.format false)
  endforeach()
  file(WRITE "${schema}" "serializer version 1.2.0; class account { ${declaration} }")
  succeed(-i "${schema}" -o "${cpp}" --cpp.format false)
endforeach()
file(WRITE "${DIRECTORY}/shared/common.serializer"
  "serializer version 1.1.0; class account { public compact_varint uint32 value; }")
file(WRITE "${schema}" "serializer version 1.2.0; include shared/common; class request {}")
reject("requires serializer version 1.2.0" -i "${schema}" -o "${cpp}" --cpp.format false)
file(WRITE "${DIRECTORY}/shared/common.serializer"
  "serializer version 1.2.0; class account { public compact_varint uint32 value; }")
file(WRITE "${schema}" "serializer version 1.1.0; include shared/common; class request {}")
succeed(-i "${schema}" -o "${cpp}" --cpp.format false)
file(WRITE "${schema}" "serializer version 1.1.0; include shared/common; "
  "class request { public compact_prefix uint32 value; }")
reject("requires serializer version 1.2.0" -i "${schema}" -o "${cpp}" --cpp.format false)

# The current language contract accepts the additions and older contracts reject them transactionally.
foreach(declaration IN ITEMS
    "private magic uint32 (99) { 0x534552 };"
    "public array[] char signature (1) { 'SRLFILE' };")
  foreach(language_version IN ITEMS 1 1.0.0 1.0.1)
    file(WRITE "${schema}" "serializer version ${language_version}; class account { ${declaration} }")
    reject("requires serializer version 1.1.0" -i "${schema}" -o "${cpp}" --cpp.format false)
  endforeach()
  file(WRITE "${schema}" "serializer version 1.1.0; class account { ${declaration} }")
  succeed(-i "${schema}" -o "${cpp}" --cpp.format false)
endforeach()

# Included files select their own syntax contract; a child cannot upgrade its parent.
file(WRITE "${DIRECTORY}/shared/common.serializer"
  "serializer version 1.0.0; class account { private magic uint32 { 42 }; }")
file(WRITE "${schema}" "serializer version 1.1.0; include shared/common; class request {}")
reject("requires serializer version 1.1.0" -i "${schema}" -o "${cpp}" --cpp.format false)
file(WRITE "${DIRECTORY}/shared/common.serializer"
  "serializer version 1.1.0; class account { private magic uint32 { 42 }; }")
file(WRITE "${schema}" "serializer version 1.0.0; include shared/common; class request {}")
succeed(-i "${schema}" -o "${cpp}" --cpp.format false)
file(WRITE "${schema}"
  "serializer version 1.0.0; include shared/common; class request { public array[] char signature { 'SRL' }; }")
reject("requires serializer version 1.1.0" -i "${schema}" -o "${cpp}" --cpp.format false)

# An inferred declaration in an older dependency fails even when its includer uses the current version.
file(WRITE "${DIRECTORY}/shared/common.serializer"
  "serializer version 1.0.1; class account { public array[] char signature { 'SRL' }; }")
file(WRITE "${schema}" "serializer version 1.1.0; include shared/common; class request {}")
reject("requires serializer version 1.1.0" -i "${schema}" -o "${cpp}" --cpp.format false)
