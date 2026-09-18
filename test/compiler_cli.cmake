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
    NOT last_output MATCHES "Supported schema language version: 1")
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
    "serializer version 1; serializer version 1;")
  file(WRITE "${schema}" "${header}\nclass account {}\n")
  reject("Serializer:" -i "${schema}" -o "${cpp}" --cpp.format false)
endforeach()
file(WRITE "${schema}" "// comment\n/* license */ serializer /* schema */ version 1;\nclass account {}\n")
succeed(-i "${schema}" -o "${cpp}" --cpp.format=false)

# Both backends consume one include graph, and dependency output tracks the entire graph.
file(MAKE_DIRECTORY "${DIRECTORY}/shared")
file(WRITE "${DIRECTORY}/shared/common.serializer"
  "serializer version 1; namespace models { class account { public uint32 id (7); } }\n")
file(WRITE "${schema}" "serializer version 1; include shared/common.serializer;\n"
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
reject("Depfile must not overwrite" -i "${schema}" -o "${cpp}" --cpp.format false --depfile "${cpp}")
reject("Depfile must not overwrite" -i "${schema}" -o "${cpp}" --cpp.format false --depfile "${schema}")
reject("Depfile must not overwrite" -i "${schema}" -o "${cpp}" --cpp.format false
  --depfile "${DIRECTORY}/shared/common.serializer")
reject("Depfile requires" -i "${schema}" -o "${cpp}" --cpp.format false
  --depfile "${DIRECTORY}/absent/file.d")
file(SHA256 "${depfile}" dependency_before)
file(WRITE "${DIRECTORY}/shared/common.serializer" "serializer version 1; include absent.serializer;")
reject("absent.serializer" -i "${schema}" -o "${cpp}" --cpp.format false --depfile "${depfile}")
file(SHA256 "${depfile}" dependency_after)
if(NOT dependency_before STREQUAL dependency_after)
  message(FATAL_ERROR "Failed include parsing changed the dependency file")
endif()
