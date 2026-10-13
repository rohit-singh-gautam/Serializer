# Both include spellings must produce identical output and dependencies for every backend.
file(MAKE_DIRECTORY "${DIRECTORY}/shared")
file(WRITE "${DIRECTORY}/shared/common.serializer" [=[serializer version 1;

namespace models {
  enum state {
    ready,
    done
  }
  class account {
    public uint32 id (7);
    public state status (8);
  }
}
]=])
set(schema "${DIRECTORY}/model.serializer")
set(languages cpp java js typescript go csharp rust python swift kotlin c)
set(outputs schema.hpp Schema.java schema.mjs schema.d.mts schema.go Schema.cs
  schema.rs schema.py Schema.swift Schema.kt schema.h)
set(destinations)
foreach(language output IN ZIP_LISTS languages outputs)
  list(APPEND destinations "--${language}.output" "${DIRECTORY}/${output}")
endforeach()
list(JOIN languages "," language_list)
foreach(spelling IN ITEMS explicit shorthand mixed)
  set(includes "include shared/common.serializer;")
  if(spelling STREQUAL "shorthand")
    set(includes "include shared/common;")
  elseif(spelling STREQUAL "mixed")
    set(includes "include shared/common;\ninclude shared/./common.serializer;")
  endif()
  file(WRITE "${schema}"
    "serializer version 1;\n${includes}\n\nnamespace models {\n"
    "  class request {\n    public account owner;\n  }\n}\n")
  execute_process(COMMAND "${GENERATOR}" --input "${schema}" --language "${language_list}"
    ${destinations} --cpp.format false --depfile "${DIRECTORY}/schema.d"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "${spelling} include generation failed: ${output}\n${error}")
  endif()
  foreach(output IN ITEMS ${outputs} schema.d)
    file(SHA256 "${DIRECTORY}/${output}" digest)
    if(spelling STREQUAL "explicit")
      set("expected_${output}" "${digest}")
    elseif(NOT digest STREQUAL "${expected_${output}}")
      message(FATAL_ERROR "${spelling} includes changed ${output}")
    endif()
  endforeach()
endforeach()

# Run generation and retain complete diagnostics for option and artifact assertions.
function(succeed)
  execute_process(COMMAND "${GENERATOR}" ${ARGN}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Include generation failed: ${ARGN}\n${output}\n${error}")
  endif()
endfunction()

# Count a declaration across actual files rather than trusting only the entry header.
function(declaration_count directory declaration expected)
  file(GLOB_RECURSE headers "${directory}/*.hpp")
  set(count 0)
  foreach(header IN LISTS headers)
    file(READ "${header}" source)
    string(REGEX MATCHALL "(^|\n)[ \t]*${declaration}" declarations "${source}")
    list(LENGTH declarations header_count)
    math(EXPR count "${count} + ${header_count}")
  endforeach()
  if(NOT count EQUAL expected)
    message(FATAL_ERROR "Expected ${expected} declarations of ${declaration}, found ${count}")
  endif()
endfunction()

# Reject malformed settings without modifying root headers, child headers, or dependencies.
function(reject_preserving expected)
  set(before)
  foreach(file IN LISTS protected_files)
    file(SHA256 "${file}" digest)
    list(APPEND before "${digest}")
  endforeach()
  execute_process(COMMAND "${GENERATOR}" ${ARGN}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(result EQUAL 0 OR NOT error MATCHES "${expected}")
    message(FATAL_ERROR "Expected include rejection '${expected}': ${ARGN}\n${output}\n${error}")
  endif()
  set(after)
  foreach(file IN LISTS protected_files)
    file(SHA256 "${file}" digest)
    list(APPEND after "${digest}")
  endforeach()
  if(NOT before STREQUAL after)
    message(FATAL_ERROR "Rejected include options modified existing artifacts")
  endif()
endfunction()

# Default C++ generation keeps included declarations in their own generated headers.
file(READ "${DIRECTORY}/schema.hpp" entry_source)
if(entry_source MATCHES "class account" OR NOT entry_source MATCHES "class request" OR
    NOT entry_source MATCHES "#include \"")
  message(FATAL_ERROR "Default C++ generation did not separate included schemas")
endif()
declaration_count("${DIRECTORY}/schema.hpp.includes" "class account[ \r\n:{]" 1)
file(GLOB_RECURSE generated_headers "${DIRECTORY}/schema.hpp.includes/*.hpp")
if(NOT generated_headers)
  message(FATAL_ERROR "Included C++ headers were not generated")
endif()
list(APPEND generated_headers "${DIRECTORY}/schema.hpp")
file(READ "${DIRECTORY}/schema.d" dependency_source)
foreach(header IN LISTS generated_headers)
  get_filename_component(name "${header}" NAME)
  if(NOT dependency_source MATCHES "${name}")
    message(FATAL_ERROR "Missing generated child in depfile: ${header}")
  endif()
endforeach()

# Inline mode retains the previous single-file layout and uses the entry's selected name.
file(MAKE_DIRECTORY "${DIRECTORY}/inline" "${DIRECTORY}/configured")
set(inline "${DIRECTORY}/inline/custom_name.hpp")
succeed(--input "${schema}" --output "${inline}" --cpp.format false --include-mode inline)
file(READ "${inline}" inline_source)
file(GLOB_RECURSE inline_headers "${DIRECTORY}/inline/*.hpp")
list(LENGTH inline_headers inline_count)
if(NOT inline_count EQUAL 1 OR NOT inline_source MATCHES "class account" OR
    NOT inline_source MATCHES "class request")
  message(FATAL_ERROR "Inline mode did not retain the complete single-file layout")
endif()
file(WRITE "${DIRECTORY}/inline.ini" "[output]\ninclude_mode = inline\n[cpp]\nformat = false\n")
set(configured "${DIRECTORY}/configured/custom_name.hpp")
succeed(--input "${schema}" --output "${configured}" --config "${DIRECTORY}/inline.ini")
file(SHA256 "${inline}" inline_hash)
file(SHA256 "${configured}" configured_hash)
if(NOT inline_hash STREQUAL configured_hash)
  message(FATAL_ERROR "INI include_mode did not select the legacy inline output")
endif()
succeed(--input "${schema}" --output "${configured}" --include-mode separate
  --config "${DIRECTORY}/inline.ini")
file(READ "${configured}" configured_source)
if(configured_source MATCHES "class account" OR NOT configured_source MATCHES "#include \"")
  message(FATAL_ERROR "CLI include mode before config failed to override the INI")
endif()
file(WRITE "${DIRECTORY}/separate.ini" "[output]\ninclude_mode = separate\n[cpp]\nformat = false\n")
succeed(--input "${schema}" --output "${configured}" --config "${DIRECTORY}/separate.ini"
  --include-mode=inline)
file(SHA256 "${configured}" configured_hash)
if(NOT inline_hash STREQUAL configured_hash)
  message(FATAL_ERROR "CLI include mode after config failed to override the INI")
endif()
set(protected_files "${schema}" "${DIRECTORY}/schema.d" "${inline}" "${configured}"
  "${DIRECTORY}/inline.ini" "${DIRECTORY}/separate.ini" ${generated_headers})
reject_preserving("include.*mode.*separate.*inline|include_mode.*separate.*inline"
  --input "${schema}" --output "${configured}" --cpp.format false --include-mode invalid)
reject_preserving("Repeated argument" --input "${schema}" --output "${configured}"
  --cpp.format false --include-mode separate --include-mode inline)
file(WRITE "${DIRECTORY}/invalid.ini" "[output]\ninclude_mode = invalid\n[cpp]\nformat = false\n")
reject_preserving("include.*mode.*separate.*inline|include_mode.*separate.*inline"
  --input "${schema}" --output "${configured}" --config "${DIRECTORY}/invalid.ini")

# A diamond emits every dependency once, including two distinct source files with equal stems.
file(MAKE_DIRECTORY "${DIRECTORY}/diamond/source/shared" "${DIRECTORY}/diamond/source/left"
  "${DIRECTORY}/diamond/source/right" "${DIRECTORY}/diamond/generated")
set(diamond_schema "${DIRECTORY}/diamond/source/root.serializer")
file(WRITE "${DIRECTORY}/diamond/source/shared/base.serializer"
  "serializer version 1; namespace diamond { class base { public uint32 id (1); } }\n")
file(WRITE "${DIRECTORY}/diamond/source/left/common.serializer"
  "serializer version 1; include ../shared/base; namespace diamond { class left { public base value (2); } }\n")
file(WRITE "${DIRECTORY}/diamond/source/right/common.serializer"
  "serializer version 1; include ../shared/./base.serializer; namespace diamond { class right : public base (3) {} }\n")
file(WRITE "${diamond_schema}"
  "serializer version 1; include left/common; include right/common.serializer; include shared/base;\n"
  "namespace diamond { class request { public left first (4); public right second (5); } }\n")
succeed(--input "${diamond_schema}" --output "${DIRECTORY}/diamond/generated/custom_name.hpp"
  --cpp.format false --depfile "${DIRECTORY}/diamond/generated/schema.d")
file(GLOB_RECURSE diamond_headers "${DIRECTORY}/diamond/generated/*.hpp")
list(LENGTH diamond_headers diamond_count)
if(NOT diamond_count EQUAL 4)
  message(FATAL_ERROR "Expected the entry plus three separate diamond headers: ${diamond_headers}")
endif()
declaration_count("${DIRECTORY}/diamond/generated" "class base[ \r\n:{]" 1)
declaration_count("${DIRECTORY}/diamond/generated" "class left[ \r\n:{]" 1)
declaration_count("${DIRECTORY}/diamond/generated" "class right[ \r\n:{]" 1)
declaration_count("${DIRECTORY}/diamond/generated" "class request[ \r\n:{]" 1)
