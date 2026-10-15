include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/serializer_example_dependency.cmake")
enable_testing()

# Run the module's native Go workflow with an available host Serializer executable.
function(serializer_add_go_example target directory)
  set(SERIALIZER_EXAMPLE_COMPILER "" CACHE FILEPATH "Host serializer executable for native Go examples")
  set(compiler_dependency)
  if(SERIALIZER_EXAMPLE_COMPILER)
    get_filename_component(compiler "${SERIALIZER_EXAMPLE_COMPILER}" ABSOLUTE)
    if(NOT EXISTS "${compiler}" OR IS_DIRECTORY "${compiler}")
      message(FATAL_ERROR "SERIALIZER_EXAMPLE_COMPILER must name an existing serializer executable")
    endif()
    get_filename_component(compiler_name "${compiler}" NAME)
    if(WIN32)
      string(TOLOWER "${compiler_name}" compiler_name)
    endif()
    if(NOT compiler_name STREQUAL "serializer" AND NOT compiler_name STREQUAL "serializer.exe")
      message(FATAL_ERROR "The Go generation directive requires an executable named serializer or serializer.exe")
    endif()
    get_filename_component(compiler_directory "${compiler}" DIRECTORY)
  else()
    if(NOT TARGET Serializer::serializer AND NOT SERIALIZER_EXAMPLE_USE_INSTALLED)
      unset(serializer_go_compiler CACHE)
      find_program(serializer_go_compiler NAMES serializer)
    endif()
    if(NOT TARGET Serializer::serializer AND serializer_go_compiler AND NOT SERIALIZER_EXAMPLE_USE_INSTALLED)
      get_filename_component(compiler_directory "${serializer_go_compiler}" DIRECTORY)
    else()
      serializer_example_dependency()
      set(compiler_dependency Serializer::serializer)
      set(compiler_directory "$<TARGET_FILE_DIR:Serializer::serializer>")
    endif()
  endif()

  unset(serializer_go_sdk CACHE)
  if(DEFINED ENV{SERIALIZER_GO} AND NOT "$ENV{SERIALIZER_GO}" STREQUAL "")
    find_program(serializer_go_sdk NAMES "$ENV{SERIALIZER_GO}" REQUIRED)
  else()
    find_program(serializer_go_sdk NAMES go REQUIRED)
  endif()
  if(WIN32)
    set(executable "${CMAKE_CURRENT_BINARY_DIR}/${target}.exe")
  else()
    set(executable "${CMAKE_CURRENT_BINARY_DIR}/${target}")
  endif()
  add_custom_target("${target}" ALL
    COMMAND "${CMAKE_COMMAND}" -E env --modify "PATH=path_list_prepend:${compiler_directory}"
      -- "${serializer_go_sdk}" generate .
    COMMAND "${serializer_go_sdk}" build -p 1 -trimpath -o "${executable}" .
    WORKING_DIRECTORY "${directory}"
    BYPRODUCTS "${directory}/internal/model/model_gen.go"
      "${directory}/internal/model/model_gen.go.d" "${executable}"
    DEPENDS ${compiler_dependency}
    COMMENT "Generating and building ${target} with Go"
    VERBATIM)
  add_test(NAME "${target}" COMMAND "${serializer_go_sdk}" test -p 1 ./...)
  set_tests_properties("${target}" PROPERTIES WORKING_DIRECTORY "${directory}")
endfunction()
