# Verify actual compiler/library admission and deliberate C++20 rejection outside CMake.
foreach(required IN ITEMS COMPILER SOURCE_DIRECTORY WORK_DIRECTORY CPP23_OPTION)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "cpp_standard.cmake requires ${required}")
  endif()
endforeach()
file(MAKE_DIRECTORY "${WORK_DIRECTORY}")
set(input "${WORK_DIRECTORY}/consumer.cpp")
file(WRITE "${input}" [=[
#include <rohit/stream.hpp>
#include <cstdint>
static_assert(rohit::byteswap(std::uint32_t{0x01020304}) == 0x04030201);
]=])

# Keep a successful control so a missing SDK or include directory cannot mask rejection.
function(compile_mode option reject)
  if(MSVC_LIKE)
    set(arguments /nologo /EHsc /c "${option}" "${input}"
      "/I${SOURCE_DIRECTORY}/include" "/Fo${WORK_DIRECTORY}/consumer.obj")
  else()
    set(arguments -c "${option}" "${input}"
      "-I${SOURCE_DIRECTORY}/include" -o "${WORK_DIRECTORY}/consumer.o")
  endif()
  execute_process(COMMAND "${COMPILER}" ${arguments}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
  if(reject)
    if(result EQUAL 0 OR NOT "${output}${errors}" MATCHES "Serializer requires C\\+\\+23")
      message(FATAL_ERROR "Missing deliberate C++20 diagnostic: ${output}${errors}")
    endif()
  elseif(NOT result EQUAL 0)
    message(FATAL_ERROR "C++23 control failed: ${output}${errors}")
  endif()
endfunction()

compile_mode("${CPP23_OPTION}" FALSE)
if(MSVC_LIKE)
  compile_mode(/std:c++20 TRUE)
else()
  compile_mode(-std=c++20 TRUE)
endif()

# CMake's C++98 spelling is numerically greater than 23 but remains unsupported.
foreach(standard IN ITEMS 98 11 14 17 20)
  execute_process(COMMAND "${CMAKE_COMMAND}" -S "${SOURCE_DIRECTORY}"
      -B "${WORK_DIRECTORY}/rejected_${standard}" "-DCMAKE_CXX_STANDARD=${standard}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
  if(result EQUAL 0 OR NOT "${output}${errors}" MATCHES "Serializer requires C\\+\\+23")
    message(FATAL_ERROR "Missing C++${standard} configure diagnostic: ${output}${errors}")
  endif()
endforeach()
message(STATUS "Verified C++23 control, C++20 header rejection and five older CMake modes")
