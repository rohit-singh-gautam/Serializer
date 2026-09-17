# Build all include examples against the same compiler and runtime as the repository.
if(SERIALIZER_BUILD_JAVA_EXAMPLES)
  find_package(Java 17 REQUIRED COMPONENTS Development Runtime)
endif()

foreach(example IN ITEMS shared_types reopened_namespaces diamond)
  set(directory "${CMAKE_CURRENT_SOURCE_DIR}/${example}")
  set(target "serializer_include_${example}")
  add_executable("${target}" "${directory}/main.cpp")
  serializer_generate(TARGET "${target}" SCHEMAS "${directory}/request.serializer")
  if(MSVC)
    target_compile_options("${target}" PRIVATE /W4 /WX)
  else()
    target_compile_options("${target}" PRIVATE -Wall -Wextra -Werror -Wpedantic)
  endif()
  if(SERIALIZER_BUILD_TESTS)
    add_test(NAME "${target}" COMMAND "${target}")
  endif()

  if(SERIALIZER_BUILD_JAVA_EXAMPLES)
    set(generated "${CMAKE_CURRENT_BINARY_DIR}/${example}/IncludeSchema.java")
    set(classes "${CMAKE_CURRENT_BINARY_DIR}/${example}/classes")
    set(stamp "${CMAKE_CURRENT_BINARY_DIR}/${example}/compiled.stamp")
    serializer_generate_java(TARGET "${target}_java_sources"
      SCHEMA "${directory}/request.serializer" OUTPUT "${generated}")
    add_custom_command(OUTPUT "${stamp}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${classes}"
      COMMAND "${Java_JAVAC_EXECUTABLE}" --release 17 -encoding UTF-8 -Xlint:all -Werror
        -d "${classes}" "${generated}" "${directory}/Main.java"
      COMMAND "${CMAKE_COMMAND}" -E touch "${stamp}"
      DEPENDS "${generated}" "${directory}/Main.java" VERBATIM)
    add_custom_target("${target}_java" ALL DEPENDS "${stamp}")
    add_dependencies("${target}_java" "${target}_java_sources")
    if(SERIALIZER_BUILD_TESTS)
      add_test(NAME "${target}_java" COMMAND "${Java_JAVA_EXECUTABLE}" -cp "${classes}" Main)
    endif()
  endif()
endforeach()
