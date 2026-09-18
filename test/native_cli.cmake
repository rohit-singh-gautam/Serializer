# All backends must participate in the same C++ CLI validation/publication transaction.
file(MAKE_DIRECTORY "${DIRECTORY}")
set(schema "${DIRECTORY}/model.serializer")
file(WRITE "${schema}" "serializer version 1; class message { public uint64 id { 18446744073709551615 }; }\n")
file(WRITE "${DIRECTORY}/output.ini" "[output]\nlanguage=rust,python,swift,kotlin,c\n[kotlin]\npackage=bad..package\n")
set(destinations --rust.output "${DIRECTORY}/schema.rs" --python.output "${DIRECTORY}/schema.py"
  --swift.output "${DIRECTORY}/Schema.swift" --kotlin.output "${DIRECTORY}/Schema.kt"
  --c.output "${DIRECTORY}/schema.h")
execute_process(COMMAND "${GENERATOR}" --input "${schema}" --config "${DIRECTORY}/output.ini"
  --kotlin.package example.models ${destinations} --depfile "${DIRECTORY}/schema.d" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Native multi-language generation/precedence failed")
endif()
file(READ "${DIRECTORY}/Schema.kt" kotlin_source)
if(NOT kotlin_source MATCHES "package example.models")
  message(FATAL_ERROR "Kotlin package override missing")
endif()
foreach(file IN ITEMS schema.rs schema.py Schema.swift Schema.kt schema.h)
  file(SHA256 "${DIRECTORY}/${file}" "before_${file}")
endforeach()
execute_process(COMMAND "${GENERATOR}" --input "${schema}" --config "${DIRECTORY}/output.ini"
  ${destinations} RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
if(result EQUAL 0)
  message(FATAL_ERROR "Invalid Kotlin package was accepted")
endif()
foreach(file IN ITEMS schema.rs schema.py Schema.swift Schema.kt schema.h)
  file(SHA256 "${DIRECTORY}/${file}" after)
  if(NOT after STREQUAL "${before_${file}}")
    message(FATAL_ERROR "Rejected generation modified ${file}")
  endif()
endforeach()
# Repeated language options retain every value through scalar-option helper changes.
execute_process(COMMAND "${GENERATOR}" --input "${schema}" --language rust --language python
  --rust.output "${DIRECTORY}/schema.rs" --python.output "${DIRECTORY}/schema.py"
  RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Repeatable language options lost values")
endif()
