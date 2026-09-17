file(MAKE_DIRECTORY "${DIRECTORY}")
file(WRITE "${DIRECTORY}/old.serializer"
  "serializer version 1; include common.serializer; class record stable_ids { public child value (1); public string retired (2); }")
file(WRITE "${DIRECTORY}/common.serializer"
  "serializer version 1; class child stable_ids { public uint32 code (1); }")
file(WRITE "${DIRECTORY}/new.serializer"
  "serializer version 1; include common.serializer; class record stable_ids { public child value (1); }")
file(WRITE "${DIRECTORY}/reserved.json"
  [[{"version":1,"reserved_fields":[{"type":"record","ids":[2],"names":["retired"]}]}]])

# Check exit codes and diagnostic text while retaining all inputs unchanged.
function(check expected pattern)
  execute_process(COMMAND "${GENERATOR}" ${ARGN}
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT status EQUAL expected OR NOT "${output}${error}" MATCHES "${pattern}")
    message(FATAL_ERROR "Expected ${expected}/${pattern}; got ${status}: ${output}${error}")
  endif()
endfunction()

set(inputs --input "${DIRECTORY}/new.serializer" --check-against "${DIRECTORY}/old.serializer")
set(policy --compatibility-policy "${DIRECTORY}/reserved.json")
check(0 "No incompatibilities" ${inputs} --compatibility-protocol protobuf_binary ${policy})
check(2 "requires its ID and wire name" ${inputs} --compatibility-protocol protobuf_binary)
check(2 "New native reader" ${inputs} --compatibility-protocol binary_integer ${policy})
check(0 "No incompatibilities" ${inputs} --compatibility-protocol binary_integer ${policy}
  --compatibility-direction forward)
check(2 "New native reader" ${inputs} --compatibility-protocol binary_integer ${policy}
  --compatibility-direction backward)
check(1 "requires --compatibility-protocol" ${inputs})
check(1 "Unsupported compatibility protocol" ${inputs} --compatibility-protocol protojson)
check(1 "direction must be" ${inputs} --compatibility-protocol json --compatibility-direction invalid)
check(1 "require --check-against" --input "${DIRECTORY}/new.serializer" --compatibility-policy missing.json)
file(WRITE "${DIRECTORY}/output.hpp" "keep this output")
check(1 "cannot be combined" ${inputs} --compatibility-protocol json --output "${DIRECTORY}/output.hpp")
check(1 "cannot be combined" ${inputs} --compatibility-protocol json --config missing.ini)
file(READ "${DIRECTORY}/output.hpp" preserved)
if(NOT preserved STREQUAL "keep this output")
  message(FATAL_ERROR "Compatibility checking modified an output file")
endif()

foreach(invalid IN ITEMS
    [[{"version":2}]]
    [[{"version":1,"version":1}]]
    [[{"version":1,"unknown":[]}]]
    [[{"version":1,"reserved_fields":[{"ids":[2]}]}]]
    [[{"version":1,"reserved_fields":[{"type":"record","unknown":[]}]}]]
    [[{"version":1,"reserved_fields":[{"type":"record","type":"record"}]}]]
    [[{"version":1,"reserved_fields":[{"type":"::record","ids":[2]}]}]]
    [[{"version":1,"reserved_fields":[{"type":"record::","ids":[2]}]}]]
    [[{"version":1,"reserved_fields":[{"type":"record","ids":[0]}]}]]
    [[{"version":1,"reserved_fields":[{"type":"record","ids":[2,2]}]}]]
    [[{"version":1,"reserved_fields":[{"type":"record","names":[""]}]}]]
    [[{"version":1,"reserved_fields":[{"type":"record","names":["a","a"]}]}]]
    [[{"version":1,"reserved_fields":[{"type":"record"},{"type":"record"}]}]]
    [[{"version":1} trailing]])
  file(WRITE "${DIRECTORY}/invalid.json" "${invalid}")
  check(1 "Serializer:" ${inputs} --compatibility-protocol protobuf_binary
    --compatibility-policy "${DIRECTORY}/invalid.json")
endforeach()

# A policy is bounded before JSON decoding, and unknown nested properties cannot be ignored.
string(REPEAT " " 1048577 oversized)
file(WRITE "${DIRECTORY}/oversized.json" "${oversized}")
check(1 "Serializer:" ${inputs} --compatibility-protocol protobuf_binary
  --compatibility-policy "${DIRECTORY}/oversized.json")
