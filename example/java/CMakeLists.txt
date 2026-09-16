find_package(Java 17 REQUIRED COMPONENTS Development Runtime)

# Generate and compile each example in its own directory to isolate alternative APIs.
function(serializer_java_example target folder config)
  set(generated "${CMAKE_CURRENT_BINARY_DIR}/${target}/AccountSchema.java")
  set(classes "${CMAKE_CURRENT_BINARY_DIR}/${target}/classes")
  serializer_generate_java(TARGET "${target}_sources"
    SCHEMA "${folder}/account.def" OUTPUT "${generated}" CONFIG "${folder}/${config}")
  set(stamp "${CMAKE_CURRENT_BINARY_DIR}/${target}/compiled.stamp")
  add_custom_command(OUTPUT "${stamp}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${classes}"
    COMMAND "${Java_JAVAC_EXECUTABLE}" --release 17 -encoding UTF-8 -Xlint:all -Werror -d "${classes}"
      "${generated}" "${CMAKE_CURRENT_SOURCE_DIR}/${folder}/Main.java"
    COMMAND "${CMAKE_COMMAND}" -E touch "${stamp}"
    DEPENDS "${generated}" "${CMAKE_CURRENT_SOURCE_DIR}/${folder}/Main.java"
    VERBATIM)
  add_custom_target("${target}" ALL DEPENDS "${stamp}")
  add_dependencies("${target}" "${target}_sources")
  if(SERIALIZER_BUILD_TESTS)
    add_test(NAME "${target}" COMMAND "${Java_JAVA_EXECUTABLE}" -cp "${classes}" serializer.example.Main)
  endif()
endfunction()

serializer_java_example(serializer_java_round_trip round_trip java.ini)
foreach(profile IN ITEMS serializer google oracle)
  serializer_java_example("serializer_java_style_${profile}" "coding_styles/${profile}" java.ini)
endforeach()
