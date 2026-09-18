file(MAKE_DIRECTORY "${DIRECTORY}")
set(schema "${DIRECTORY}/model.serializer")
file(WRITE "${schema}" "serializer version 1; class message { public uint64 id { 18446744073709551615 }; }\n")
file(WRITE "${DIRECTORY}/output.ini" "[output]\nlanguage=js,typescript,go,csharp\n[go]\npackage=invalid.package\n[csharp]\nnamespace=Example.Models\n[js]\nnaming=profile\n")

# Check configuration precedence and all output extensions through the actual executable.
set(destinations --js.output "${DIRECTORY}/model.mjs" --typescript.output "${DIRECTORY}/model.d.mts"
  --go.output "${DIRECTORY}/model.go" --csharp.output "${DIRECTORY}/Model.cs")
execute_process(COMMAND "${GENERATOR}" --input "${schema}" --config "${DIRECTORY}/output.ini"
  --go.package models ${destinations} --depfile "${DIRECTORY}/models.d" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Multi-language configuration/CLI override failed")
endif()
file(READ "${DIRECTORY}/model.go" go_source)
file(READ "${DIRECTORY}/Model.cs" csharp_source)
if(NOT go_source MATCHES "package models" OR NOT csharp_source MATCHES "namespace Example.Models")
  message(FATAL_ERROR "Native module options were not applied")
endif()

# A later backend failure must preserve every previously generated destination.
foreach(file IN ITEMS model.mjs model.d.mts model.go Model.cs)
  file(SHA256 "${DIRECTORY}/${file}" "before_${file}")
endforeach()
execute_process(COMMAND "${GENERATOR}" --input "${schema}" --language js,typescript,go,csharp
  --csharp.namespace Invalid..Namespace ${destinations} RESULT_VARIABLE result ERROR_VARIABLE error)
if(result EQUAL 0)
  message(FATAL_ERROR "Invalid namespace accepted")
endif()
foreach(file IN ITEMS model.mjs model.d.mts model.go Model.cs)
  file(SHA256 "${DIRECTORY}/${file}" after)
  if(NOT after STREQUAL "${before_${file}}")
    message(FATAL_ERROR "Failed batch replaced ${file}")
  endif()
endforeach()

# Reject unsupported extensions and invalid naming selections at the compiler boundary.
foreach(language IN ITEMS js typescript go csharp)
  execute_process(COMMAND "${GENERATOR}" --input "${schema}" --language "${language}"
    --output "${DIRECTORY}/bad.txt" RESULT_VARIABLE result ERROR_QUIET)
  if(result EQUAL 0)
    message(FATAL_ERROR "Invalid ${language} output extension accepted")
  endif()
endforeach()
execute_process(COMMAND "${GENERATOR}" --input "${schema}" --language js
  --output "${DIRECTORY}/model.mjs" --js.naming invalid RESULT_VARIABLE result ERROR_QUIET)
if(result EQUAL 0)
  message(FATAL_ERROR "Invalid JS naming policy accepted")
endif()
execute_process(COMMAND "${GENERATOR}" --input "${schema}" --language csharp
  --output "${DIRECTORY}/Model.cs" --csharp.namespace= RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Clearing the optional C# namespace failed")
endif()
