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
