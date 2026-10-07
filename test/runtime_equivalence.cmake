foreach(profile IN ITEMS DISABLED ENABLED)
  if(NOT DEFINED ${profile})
    message(FATAL_ERROR "Missing runtime equivalence executable: ${profile}")
  endif()
  execute_process(COMMAND "${${profile}}" RESULT_VARIABLE status
    OUTPUT_VARIABLE report ERROR_VARIABLE error)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "${profile} runtime equivalence probe failed: ${error}")
  endif()
  set(report_${profile} "${report}")
endforeach()
if(NOT report_DISABLED STREQUAL report_ENABLED)
  message(FATAL_ERROR
    "Constant-evaluation activation changed runtime layout, bytes or allocation requests.\n"
    "Disabled:\n${report_DISABLED}Enabled:\n${report_ENABLED}")
endif()
