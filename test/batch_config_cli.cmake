# Verify one include-graph parse serves independent configuration variants atomically.
file(MAKE_DIRECTORY "${DIRECTORY}/fixtures")
set(fixtures "${DIRECTORY}/fixtures")
foreach(fixture IN ITEMS dimensions_managed.serializer dimensions.serializer
    dimensions_managed32.ini dimensions_managed64.ini dimensions_separate64.ini)
  configure_file("${CMAKE_CURRENT_LIST_DIR}/resources/${fixture}" "${fixtures}/${fixture}" COPYONLY)
endforeach()
set(schema "${fixtures}/dimensions_managed.serializer")
# A valid output extension must still reject an alias of an included schema.
set(include_alias "${fixtures}/included_alias.hpp")
file(REMOVE "${include_alias}")
file(CREATE_LINK "${fixtures}/dimensions.serializer" "${include_alias}" RESULT link_result)
if(NOT link_result STREQUAL "0")
  message(FATAL_ERROR "Could not create the included-schema alias: ${link_result}")
endif()
set(configurations)
set(destinations)
set(outputs)
set(protected_files "${schema}" "${fixtures}/dimensions.serializer" "${include_alias}")
foreach(profile IN ITEMS managed32 managed64 separate64)
  set(config "${fixtures}/dimensions_${profile}.ini")
  set(output "${DIRECTORY}/batch/${profile}/dimensions_managed.hpp")
  file(MAKE_DIRECTORY "${DIRECTORY}/batch/${profile}" "${DIRECTORY}/independent/${profile}")
  list(APPEND configurations --config "${config}")
  list(APPEND destinations --output "${output}")
  list(APPEND outputs "${output}")
  list(APPEND protected_files "${config}" "${output}")
endforeach()
set(depfile "${DIRECTORY}/batch/model.d")

# Run a valid command and retain verbose output for assertions.
function(succeed)
  execute_process(COMMAND "${GENERATOR}" ${ARGN}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Command failed: ${ARGN}\n${result}\n${output}\n${error}")
  endif()
  set(last_output "${output}" PARENT_SCOPE)
endfunction()

# Reject invalid batches while preserving all existing outputs, inputs, and dependency files.
function(reject_preserving expected)
  foreach(protected_file IN LISTS protected_files)
    file(SHA256 "${protected_file}" "before_${protected_file}")
  endforeach()
  execute_process(COMMAND "${GENERATOR}" ${ARGN}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(result EQUAL 0 OR NOT error MATCHES "${expected}")
    message(FATAL_ERROR "Expected rejection '${expected}': ${ARGN}\n${result}\n${output}\n${error}")
  endif()
  foreach(protected_file IN LISTS protected_files)
    file(SHA256 "${protected_file}" after)
    if(NOT after STREQUAL "${before_${protected_file}}")
      message(FATAL_ERROR "Rejected batch changed ${protected_file}")
    endif()
  endforeach()
endfunction()

# Compare the same output basename so language container names remain identical.
foreach(profile IN ITEMS managed32 managed64 separate64)
  succeed(--input "${schema}" --config "${fixtures}/dimensions_${profile}.ini"
    --output "${DIRECTORY}/independent/${profile}/dimensions_managed.hpp" --cpp.format false)
endforeach()
succeed(--input "${schema}" ${configurations} ${destinations} --cpp.format false
  --depfile "${depfile}" --verbose)
if(NOT last_output MATCHES "Parsed schema once for 3 configurations")
  message(FATAL_ERROR "Batch did not report one schema parse: ${last_output}")
endif()
string(REGEX MATCHALL "Parsed schema once" parse_reports "${last_output}")
list(LENGTH parse_reports parse_report_count)
if(NOT parse_report_count EQUAL 1)
  message(FATAL_ERROR "Batch reported more than one schema parse: ${last_output}")
endif()
foreach(profile IN ITEMS managed32 managed64 separate64)
  file(SHA256 "${DIRECTORY}/independent/${profile}/dimensions_managed.hpp" independent)
  file(SHA256 "${DIRECTORY}/batch/${profile}/dimensions_managed.hpp" "batch_${profile}")
  if(NOT independent STREQUAL "${batch_${profile}}")
    message(FATAL_ERROR "Batch changed the ${profile} generated model")
  endif()
endforeach()
if(batch_managed32 STREQUAL batch_managed64 OR batch_managed32 STREQUAL batch_separate64 OR
    batch_managed64 STREQUAL batch_separate64)
  message(FATAL_ERROR "Managed variants did not retain independent ID/value configuration")
endif()
file(READ "${depfile}" dependencies)
foreach(dependency IN ITEMS dimensions_managed.serializer dimensions.serializer
    dimensions_managed32.ini dimensions_managed64.ini dimensions_separate64.ini
    managed32/dimensions_managed.hpp managed64/dimensions_managed.hpp
    separate64/dimensions_managed.hpp)
  string(FIND "${dependencies}" "${dependency}" dependency_position)
  if(dependency_position EQUAL -1)
    message(FATAL_ERROR "Missing batch dependency ${dependency}: ${dependencies}")
  endif()
endforeach()
list(APPEND protected_files "${depfile}")

# CLI options override every INI even when they appear before repeated configurations.
succeed(--input "${schema}" --managed.id_type uint32 --managed.separate_values false
  --cpp.format false ${configurations} ${destinations})
foreach(output IN LISTS outputs)
  file(SHA256 "${output}" overridden)
  if(NOT overridden STREQUAL batch_managed32)
    message(FATAL_ERROR "Shared CLI overrides did not apply to ${output}")
  endif()
endforeach()
succeed(--input "${schema}" ${configurations} ${destinations} --cpp.format false
  --depfile "${depfile}")

# Cardinality and canonical path conflicts must be rejected across the whole batch.
list(GET outputs 0 first_output)
list(GET outputs 1 second_output)
list(GET outputs 2 third_output)
reject_preserving("output.*configuration|configuration.*output" --input "${schema}"
  ${configurations} --output "${first_output}" --output "${second_output}" --cpp.format false)
reject_preserving("output.*configuration|configuration.*output" --input "${schema}"
  ${configurations} ${destinations} --output "${DIRECTORY}/extra.hpp" --cpp.format false)
reject_preserving("distinct files" --input "${schema}" ${configurations}
  --output "${first_output}" --output "${second_output}" --output "${first_output}"
  --cpp.format false)
reject_preserving("distinct files" --input "${schema}" ${configurations}
  --output "${first_output}" --output "${second_output}"
  --output "${DIRECTORY}/batch/managed32/../managed32/dimensions_managed.hpp" --cpp.format false)
reject_preserving("must not overwrite" --input "${schema}" ${configurations}
  --output "${first_output}" --output "${second_output}"
  --output "${fixtures}/dimensions_separate64.ini" --cpp.format false)
reject_preserving("must not overwrite" --input "${schema}" ${configurations}
  --output "${first_output}" --output "${second_output}"
  --output "${include_alias}" --cpp.format false)
reject_preserving("must not overwrite" --input "${schema}" ${configurations}
  ${destinations} --cpp.format false --depfile "${fixtures}/dimensions_separate64.ini")
reject_preserving("must not overwrite" --input "${schema}" ${configurations}
  ${destinations} --cpp.format false --depfile "${third_output}")
reject_preserving("existing parent" --input "${schema}" ${configurations}
  --output "${first_output}" --output "${second_output}"
  --output "${DIRECTORY}/absent/model.hpp" --cpp.format false)

# Fail after earlier variants can generate; no staged output may replace a destination.
set(failure_configurations)
foreach(profile IN ITEMS managed32 managed64 separate64)
  file(READ "${fixtures}/dimensions_${profile}.ini" config_source)
  if(profile STREQUAL "separate64")
    string(APPEND config_source "\n[cpp]\nformat = true\nclang_format = ${DIRECTORY}/missing-clang-format\n")
  else()
    string(APPEND config_source "\n[cpp]\nformat = false\n")
  endif()
  set(failure_config "${fixtures}/failure_${profile}.ini")
  file(WRITE "${failure_config}" "${config_source}")
  list(APPEND failure_configurations --config "${failure_config}")
  list(APPEND protected_files "${failure_config}")
endforeach()
reject_preserving("clang-format" --input "${schema}" ${failure_configurations}
  ${destinations} --depfile "${depfile}")
set(java_failure_config "${fixtures}/java_failure.ini")
file(WRITE "${java_failure_config}" "[output]\nlanguage = java\n")
set(java_failure_output "${DIRECTORY}/batch/Model.java")
file(WRITE "${java_failure_output}" "existing Java output\n")
set(java_failure_schema "${fixtures}/java_failure.serializer")
file(WRITE "${java_failure_schema}"
  "serializer version 1; class entry managed { public uint32 value (1); }\n")
list(APPEND protected_files "${java_failure_config}" "${java_failure_output}" "${java_failure_schema}")
reject_preserving("only by the C\\+\\+ backend" --input "${java_failure_schema}"
  --config "${fixtures}/dimensions_managed32.ini" --config "${fixtures}/dimensions_managed64.ini"
  --config "${java_failure_config}" --output "${first_output}" --output "${second_output}"
  --output "${java_failure_output}" --cpp.format false --depfile "${depfile}")

# Mixed-language configurations pair each backend's output list only with selected variants.
file(MAKE_DIRECTORY "${DIRECTORY}/graph/shared")
set(graph "${DIRECTORY}/graph/model.serializer")
set(middle "${DIRECTORY}/graph/middle.serializer")
set(leaf "${DIRECTORY}/graph/shared/leaf.serializer")
file(WRITE "${graph}" "serializer version 1; include middle;\n")
file(WRITE "${middle}" "serializer version 1; include shared/leaf;\n")
file(WRITE "${leaf}" "serializer version 1; class account_record stable_ids { public uint32 id (1); }\n")
set(multi_configurations)
set(multi_destinations)
set(multi_outputs)
foreach(profile IN ITEMS first second third)
  set(config "${DIRECTORY}/graph/${profile}.ini")
  set(languages cpp,java)
  set(cpp_standard google)
  if(profile STREQUAL "second")
    set(languages java)
  elseif(profile STREQUAL "third")
    set(cpp_standard llvm)
  endif()
  file(WRITE "${config}" "[output]\nlanguage = ${languages}\n[cpp]\nformat = false\n"
    "coding_standard = ${cpp_standard}\n"
    "[java]\npackage = configured.${profile}\n")
  file(MAKE_DIRECTORY "${DIRECTORY}/multi/${profile}" "${DIRECTORY}/multi_independent/${profile}")
  list(APPEND multi_configurations --config "${config}")
  set(independent_destinations)
  if(NOT profile STREQUAL "second")
    list(APPEND multi_destinations --cpp.output "${DIRECTORY}/multi/${profile}/model.hpp")
    list(APPEND multi_outputs "${DIRECTORY}/multi/${profile}/model.hpp")
    list(APPEND independent_destinations --cpp.output "${DIRECTORY}/multi_independent/${profile}/model.hpp")
  endif()
  list(APPEND multi_destinations --java.output "${DIRECTORY}/multi/${profile}/Model.java")
  list(APPEND multi_outputs "${DIRECTORY}/multi/${profile}/Model.java")
  list(APPEND independent_destinations --java.output "${DIRECTORY}/multi_independent/${profile}/Model.java")
  succeed(--input "${graph}" --config "${config}" ${independent_destinations}
    --java.package shared.package_name)
  list(APPEND protected_files "${config}")
endforeach()
set(multi_depfile "${DIRECTORY}/multi/model.d")
succeed(--input "${graph}" --java.package shared.package_name ${multi_configurations}
  ${multi_destinations} --depfile "${multi_depfile}" --verbose)
if(NOT last_output MATCHES "Parsed schema once for 3 configurations")
  message(FATAL_ERROR "Mixed-language batch did not report one schema parse: ${last_output}")
endif()
foreach(output IN LISTS multi_outputs)
  string(REPLACE "/multi/" "/multi_independent/" independent_output "${output}")
  file(SHA256 "${output}" generated)
  file(SHA256 "${independent_output}" independent)
  if(NOT generated STREQUAL independent)
    message(FATAL_ERROR "Mixed-language batch changed ${output}")
  endif()
endforeach()
file(READ "${multi_depfile}" multi_dependencies)
# Makefile depfiles escape Windows drive colons independently of path separators.
string(REPLACE "\\:" ":" multi_dependencies "${multi_dependencies}")
foreach(input IN ITEMS model.serializer middle.serializer leaf.serializer first.ini second.ini third.ini)
  string(FIND "${multi_dependencies}" "${input}" dependency_position)
  if(dependency_position EQUAL -1)
    message(FATAL_ERROR "Missing transitive batch dependency ${input}: ${multi_dependencies}")
  endif()
endforeach()
foreach(output IN LISTS multi_outputs)
  string(FIND "${multi_dependencies}" "${output}" output_position)
  if(output_position EQUAL -1)
    message(FATAL_ERROR "Missing mixed-language output dependency ${output}: ${multi_dependencies}")
  endif()
endforeach()
list(APPEND protected_files "${graph}" "${middle}" "${leaf}" "${multi_depfile}" ${multi_outputs})
reject_preserving("multiple languages" --input "${graph}" ${multi_configurations}
  --output "${first_output}" --output "${second_output}" --output "${third_output}")
reject_preserving("output.*configuration|configuration.*output" --input "${graph}"
  ${multi_configurations} --cpp.output "${DIRECTORY}/multi/first/model.hpp"
  --java.output "${DIRECTORY}/multi/first/Model.java"
  --java.output "${DIRECTORY}/multi/second/Model.java"
  --java.output "${DIRECTORY}/multi/third/Model.java")
