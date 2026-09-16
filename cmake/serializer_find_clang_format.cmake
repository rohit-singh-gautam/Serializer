include_guard(GLOBAL)

# Reject tools that cannot run on the build host or do not support our formatting options.
function(serializer_validate_clang_format result executable)
  set(minimum_version 19)
  execute_process(COMMAND "${executable}" --version
    RESULT_VARIABLE status OUTPUT_VARIABLE version_output ERROR_QUIET TIMEOUT 5)
  if(NOT status STREQUAL "0" OR
      NOT version_output MATCHES "clang-format version ([0-9]+)\\.")
    set(${result} FALSE PARENT_SCOPE)
  elseif(CMAKE_MATCH_1 LESS minimum_version)
    set(${result} FALSE PARENT_SCOPE)
  endif()
endfunction()

# Cache a host formatter, honoring explicit choices before searching installed tools.
function(serializer_find_clang_format)
  if(SERIALIZER_CLANG_FORMAT_EXECUTABLE)
    # find_program skips validation for cached values, so check explicit/cached paths too.
    set(valid TRUE)
    serializer_validate_clang_format(valid "${SERIALIZER_CLANG_FORMAT_EXECUTABLE}")
    if(NOT valid)
      message(FATAL_ERROR
        "SERIALIZER_CLANG_FORMAT_EXECUTABLE must run clang-format 19 or newer: "
        "${SERIALIZER_CLANG_FORMAT_EXECUTABLE}")
    endif()
    return()
  endif()

  set(formatter_names clang-format clang-format-22 clang-format-21 clang-format-20 clang-format-19)
  find_program(SERIALIZER_CLANG_FORMAT_EXECUTABLE NAMES ${formatter_names}
    VALIDATOR serializer_validate_clang_format NO_CMAKE_FIND_ROOT_PATH
    DOC "Host clang-format 19+ used for generated C++ formatting")

  if(NOT SERIALIZER_CLANG_FORMAT_EXECUTABLE AND CMAKE_HOST_WIN32)
    set(formatter_directories)
    set(installer_directories)
    foreach(environment_variable IN ITEMS ProgramFiles "ProgramFiles(x86)" ProgramW6432)
      if(NOT "$ENV{${environment_variable}}" STREQUAL "")
        list(APPEND formatter_directories "$ENV{${environment_variable}}/LLVM/bin")
        list(APPEND installer_directories
          "$ENV{${environment_variable}}/Microsoft Visual Studio/Installer")
      endif()
    endforeach()

    # Query every installation: the selected Build Tools instance may lack LLVM
    # while Community, Professional, or Enterprise has a usable host formatter.
    set(visual_studio_directories "${CMAKE_GENERATOR_INSTANCE}")
    find_program(serializer_vswhere NAMES vswhere HINTS ${installer_directories}
      NO_CACHE NO_CMAKE_FIND_ROOT_PATH)
    if(serializer_vswhere)
      execute_process(COMMAND "${serializer_vswhere}" -products * -prerelease
        -property installationPath
        RESULT_VARIABLE status OUTPUT_VARIABLE installations ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE TIMEOUT 10)
      if(status STREQUAL "0")
        string(REPLACE "\r\n" "\n" installations "${installations}")
        string(REPLACE "\n" ";" installations "${installations}")
        list(APPEND visual_studio_directories ${installations})
      endif()
    endif()
    if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
      set(host_architecture ARM64)
    else()
      set(host_architecture x64)
    endif()
    foreach(installation IN LISTS visual_studio_directories)
      if(NOT installation STREQUAL "")
        list(APPEND formatter_directories
          "${installation}/VC/Tools/Llvm/${host_architecture}/bin"
          "${installation}/VC/Tools/Llvm/bin")
      endif()
    endforeach()
    list(REMOVE_DUPLICATES formatter_directories)
    find_program(SERIALIZER_CLANG_FORMAT_EXECUTABLE NAMES ${formatter_names}
      PATHS ${formatter_directories} NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH
      VALIDATOR serializer_validate_clang_format)
  endif()

  if(NOT SERIALIZER_CLANG_FORMAT_EXECUTABLE)
    message(FATAL_ERROR
      "Serializer's generated C++ tests/examples require clang-format 19 or newer. "
      "Install LLVM (or the Visual Studio C++ Clang tools), then configure again. "
      "Installed tools are detected automatically; custom locations can use "
      "-DSERIALIZER_CLANG_FORMAT_EXECUTABLE=/path/to/clang-format.")
  endif()
  message(STATUS "Serializer clang-format: ${SERIALIZER_CLANG_FORMAT_EXECUTABLE}")
endfunction()
