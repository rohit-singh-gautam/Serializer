# Java qualification is opt-in alongside the runnable Java examples.
find_package(Java 17 REQUIRED COMPONENTS Development Runtime)
set(java_test_directory "${CMAKE_CURRENT_BINARY_DIR}/java_tests")
serializer_generate_java(TARGET serializer_java_test_schema
  SCHEMA java/all_types.def OUTPUT "${java_test_directory}/TestSchema.java")
serializer_generate_java(TARGET serializer_java_test_account
  SCHEMA "${PROJECT_SOURCE_DIR}/example/java/round_trip/account.def"
  OUTPUT "${java_test_directory}/AccountSchema.java"
  CONFIG "${PROJECT_SOURCE_DIR}/example/java/round_trip/java.ini")
add_custom_command(OUTPUT "${java_test_directory}/compiled.stamp"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${java_test_directory}/classes"
  COMMAND "${Java_JAVAC_EXECUTABLE}" --release 17 -encoding UTF-8 -Xlint:all -Werror
    -d "${java_test_directory}/classes"
    "${java_test_directory}/TestSchema.java" "${java_test_directory}/AccountSchema.java"
    "${CMAKE_CURRENT_SOURCE_DIR}/java/CodecTest.java"
  COMMAND "${CMAKE_COMMAND}" -E touch "${java_test_directory}/compiled.stamp"
  DEPENDS "${java_test_directory}/TestSchema.java" "${java_test_directory}/AccountSchema.java"
    "${CMAKE_CURRENT_SOURCE_DIR}/java/CodecTest.java"
  VERBATIM)
add_custom_target(serializer_java_codec_tests ALL DEPENDS "${java_test_directory}/compiled.stamp")
add_dependencies(serializer_java_codec_tests serializer_java_test_schema serializer_java_test_account)
add_executable(serializer_java_interop java_interop_test.cpp)
serializer_generate(TARGET serializer_java_interop
  SCHEMAS "${PROJECT_SOURCE_DIR}/example/java/round_trip/account.def"
  OUTPUT_DIRECTORY "${java_test_directory}/cpp")
add_test(NAME serializer_java_interoperability
  COMMAND "${CMAKE_COMMAND}" "-DCPP=$<TARGET_FILE:serializer_java_interop>"
    "-DJAVA=${Java_JAVA_EXECUTABLE}" "-DCLASSES=${java_test_directory}/classes"
    "-DFIXTURES=${java_test_directory}/fixtures"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/java_interop.cmake")
add_test(NAME serializer_java_cli
  COMMAND "${CMAKE_COMMAND}" "-DGENERATOR=$<TARGET_FILE:serializer>"
    "-DJAVAC=${Java_JAVAC_EXECUTABLE}"
    "-DSCHEMA=${PROJECT_SOURCE_DIR}/example/java/round_trip/account.def"
    "-DDIRECTORY=${java_test_directory}/cli"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/java_cli.cmake")
