include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/serializer_example_dependency.cmake")
enable_testing()

# Generate and strictly compile one account consumer with its own Java naming profile.
function(serializer_add_java_account_example target)
  serializer_example_dependency()
  find_package(Java 17 REQUIRED COMPONENTS Development Runtime)
  set(generated "${CMAKE_CURRENT_BINARY_DIR}/generated/AccountSchema.java")
  set(classes "${CMAKE_CURRENT_BINARY_DIR}/classes")
  serializer_generate_java(TARGET "${target}_sources" SCHEMA account.serializer
    OUTPUT "${generated}" CONFIG java.ini)
  set(stamp "${CMAKE_CURRENT_BINARY_DIR}/compiled.stamp")
  add_custom_command(OUTPUT "${stamp}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${classes}"
    COMMAND "${Java_JAVAC_EXECUTABLE}" --release 17 -encoding UTF-8 -Xlint:all -Werror
      -d "${classes}" "${generated}" "${CMAKE_CURRENT_SOURCE_DIR}/Main.java"
    COMMAND "${CMAKE_COMMAND}" -E touch "${stamp}"
    DEPENDS "${generated}" "${CMAKE_CURRENT_SOURCE_DIR}/Main.java"
    VERBATIM)
  add_custom_target("${target}" ALL DEPENDS "${stamp}")
  add_dependencies("${target}" "${target}_sources")
  add_test(NAME "${target}"
    COMMAND "${Java_JAVA_EXECUTABLE}" -cp "${classes}" serializer.example.Main)
endfunction()
