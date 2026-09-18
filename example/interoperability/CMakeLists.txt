# Build the C++ schema compiler first; target-language tools only compile generated examples.
find_package(Java 17 REQUIRED COMPONENTS Development Runtime)
find_program(serializer_node NAMES node REQUIRED)
find_program(serializer_go NAMES go REQUIRED)
find_program(serializer_dotnet NAMES dotnet REQUIRED)
set(generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${generated}/cpp" "${generated}/java" "${generated}/javascript"
  "${generated}/go" "${generated}/csharp")

set(outputs "${generated}/cpp/message.hpp" "${generated}/java/Schema.java"
  "${generated}/javascript/schema.mjs" "${generated}/javascript/schema.d.mts"
  "${generated}/go/schema.go" "${generated}/csharp/Schema.cs")
add_custom_command(OUTPUT ${outputs}
  COMMAND "$<TARGET_FILE:serializer>" --input "${CMAKE_CURRENT_SOURCE_DIR}/message.serializer"
    --language cpp,java,js,typescript,go,csharp
    --cpp.output "${generated}/cpp/message.hpp" --cpp.format false
    --java.output "${generated}/java/Schema.java"
    --js.output "${generated}/javascript/schema.mjs"
    --typescript.output "${generated}/javascript/schema.d.mts"
    --go.output "${generated}/go/schema.go" --go.package main
    --csharp.output "${generated}/csharp/Schema.cs"
    --depfile "${generated}/schema.d"
  DEPENDS serializer message.serializer common.serializer
  DEPFILE "${generated}/schema.d" VERBATIM)
add_custom_target(serializer_interop_sources DEPENDS ${outputs})

add_executable(serializer_interop_cpp ../cpp/interoperability/main.cpp "${generated}/cpp/message.hpp")
target_include_directories(serializer_interop_cpp PRIVATE "${generated}/cpp")
target_link_libraries(serializer_interop_cpp PRIVATE Serializer::serializer_lib)
add_dependencies(serializer_interop_cpp serializer_interop_sources)
if(MSVC)
  target_compile_options(serializer_interop_cpp PRIVATE /W4 /WX)
else()
  target_compile_options(serializer_interop_cpp PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

configure_file(../javascript/interoperability/browser.html "${generated}/javascript/browser.html" COPYONLY)
configure_file(../javascript/interoperability/main.mjs "${generated}/javascript/main.mjs" COPYONLY)
configure_file(../go/interoperability/main.go "${generated}/go/main.go" COPYONLY)
configure_file(../go/interoperability/go.mod "${generated}/go/go.mod" COPYONLY)
configure_file(../csharp/interoperability/Program.cs "${generated}/csharp/Program.cs" COPYONLY)
configure_file(../csharp/interoperability/Interop.csproj "${generated}/csharp/Interop.csproj" COPYONLY)
add_custom_command(OUTPUT "${generated}/java/classes/Main.class"
  COMMAND "${Java_JAVAC_EXECUTABLE}" --release 17 -encoding UTF-8 -Xlint:all -Werror
    -d "${generated}/java/classes" "${generated}/java/Schema.java" "${CMAKE_CURRENT_SOURCE_DIR}/../java/interoperability/Main.java"
  DEPENDS "${generated}/java/Schema.java" ../java/interoperability/Main.java VERBATIM)
set(go_executable "${generated}/go/interop_go${CMAKE_EXECUTABLE_SUFFIX}")
add_custom_command(OUTPUT "${go_executable}"
  COMMAND "${serializer_go}" build -trimpath -o "${go_executable}" .
  WORKING_DIRECTORY "${generated}/go"
  DEPENDS "${generated}/go/schema.go" "${generated}/go/main.go" "${generated}/go/go.mod" VERBATIM)
set(csharp_dll "${generated}/csharp/bin/Release/net8.0/Interop.dll")
add_custom_command(OUTPUT "${csharp_dll}"
  COMMAND "${serializer_dotnet}" build "${generated}/csharp/Interop.csproj" --configuration Release -p:Platform=AnyCPU --nologo -v:q
  DEPENDS "${generated}/csharp/Schema.cs" "${generated}/csharp/Program.cs" "${generated}/csharp/Interop.csproj" VERBATIM)
add_custom_target(serializer_interop_examples ALL DEPENDS "${generated}/java/classes/Main.class"
  "${go_executable}" "${csharp_dll}" "${generated}/javascript/main.mjs")
add_dependencies(serializer_interop_examples serializer_interop_sources serializer_interop_cpp)

add_test(NAME serializer_all_language_interoperability
  COMMAND "${CMAKE_COMMAND}" "-DCPP=$<TARGET_FILE:serializer_interop_cpp>"
    "-DJAVA=${Java_JAVA_EXECUTABLE}" "-DCLASSES=${generated}/java/classes"
    "-DNODE=${serializer_node}" "-DJS=${generated}/javascript/main.mjs"
    "-DGO=${go_executable}" "-DDOTNET=${serializer_dotnet}" "-DCSHARP=${csharp_dll}"
    "-DFIXTURES=${CMAKE_CURRENT_BINARY_DIR}/fixtures"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/../../test/all_language_interop.cmake")
set_tests_properties(serializer_all_language_interoperability PROPERTIES TIMEOUT 120)

# A browser is optional; the generated module itself has no Node-only dependencies.
find_package(Python3 3.9 COMPONENTS Interpreter QUIET)
find_program(serializer_browser NAMES chromium chromium-browser google-chrome chrome msedge
  HINTS "$ENV{ProgramFiles}/Google/Chrome/Application"
    "$ENV{ProgramFiles}/Microsoft/Edge/Application" "C:/Program Files (x86)/Microsoft/Edge/Application")
if(Python3_Interpreter_FOUND AND serializer_browser)
  set_tests_properties(serializer_all_language_interoperability PROPERTIES FIXTURES_SETUP serializer_wire_fixtures)
  add_test(NAME serializer_browser_interoperability
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/test/browser_interop_test.py"
      --browser "${serializer_browser}" --directory "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(serializer_browser_interoperability PROPERTIES
    FIXTURES_REQUIRED serializer_wire_fixtures TIMEOUT 60)
endif()
