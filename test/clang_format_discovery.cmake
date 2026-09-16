# Exercise host discovery and override failures in separate CMake scopes/caches.
cmake_minimum_required(VERSION 3.28)
file(MAKE_DIRECTORY "${DIRECTORY}")
set(helper "${CMAKE_CURRENT_LIST_DIR}/../cmake/serializer_find_clang_format.cmake")

# Run one isolated discovery scenario and check either its result or its diagnostic.
function(check_discovery name setup expected_error)
  set(script "${DIRECTORY}/${name}.cmake")
  file(WRITE "${script}" "cmake_minimum_required(VERSION 3.28)\n"
    "include([==[${helper}]==])\n${setup}\nserializer_find_clang_format()\n")
  if(expected_error STREQUAL "")
    file(APPEND "${script}"
      "if(NOT SERIALIZER_CLANG_FORMAT_EXECUTABLE STREQUAL [==[${FORMATTER}]==])\n"
      "  message(FATAL_ERROR \"Unexpected formatter selected\")\nendif()\n")
  endif()
  execute_process(COMMAND "${CMAKE_COMMAND}" -P "${script}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(expected_error STREQUAL "")
    if(NOT status STREQUAL "0")
      message(FATAL_ERROR "${name}: ${output}\n${error}")
    endif()
  elseif(status STREQUAL "0" OR NOT error MATCHES "${expected_error}")
    message(FATAL_ERROR "${name}: expected '${expected_error}', got ${status}: ${output}\n${error}")
  endif()
endfunction()

check_discovery(explicit
  "set(SERIALIZER_CLANG_FORMAT_EXECUTABLE [==[${FORMATTER}]==])" "")
check_discovery(cached
  "set(SERIALIZER_CLANG_FORMAT_EXECUTABLE [==[${FORMATTER}]==] CACHE FILEPATH \"\")" "")
check_discovery(missing_override
  "set(SERIALIZER_CLANG_FORMAT_EXECUTABLE [==[${DIRECTORY}/absent-formatter]==])"
  "must run clang-format 19 or newer")
check_discovery(wrong_program
  "set(SERIALIZER_CLANG_FORMAT_EXECUTABLE [==[${CMAKE_COMMAND}]==])"
  "must run clang-format 19 or newer")

# A runnable older tool must be rejected, including when its path is cached.
if(CMAKE_HOST_WIN32)
  set(old_formatter "${DIRECTORY}/old-formatter.cmd")
  file(WRITE "${old_formatter}" "@echo off\r\necho clang-format version 18.1.0\r\n")
else()
  set(old_formatter "${DIRECTORY}/old-formatter")
  file(WRITE "${old_formatter}" "#!/bin/sh\necho 'clang-format version 18.1.0'\n")
  file(CHMOD "${old_formatter}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
endif()
check_discovery(old_override
  "set(SERIALIZER_CLANG_FORMAT_EXECUTABLE [==[${old_formatter}]==] CACHE FILEPATH \"\")"
  "must run clang-format 19 or newer")

# Restrict the search to test absence and CMake's portable program-path discovery.
set(isolated_search "set(CMAKE_HOST_WIN32 FALSE)\n"
  "set(CMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH FALSE)\n"
  "set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH FALSE)\n"
  "set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH FALSE)\n"
  "set(CMAKE_FIND_USE_INSTALL_PREFIX FALSE)\n")
string(JOIN "" isolated_search ${isolated_search})
get_filename_component(formatter_directory "${FORMATTER}" DIRECTORY)
check_discovery(program_path
  "${isolated_search}set(CMAKE_PROGRAM_PATH [==[${formatter_directory}]==])" "")
check_discovery(absent "${isolated_search}"
  "require clang-format 19 or newer")
