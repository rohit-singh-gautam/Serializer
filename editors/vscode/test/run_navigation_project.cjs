const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { runTests } = require('@vscode/test-electron');

/** Test real generated managed types with CMake Tools and Microsoft C/C++ in an isolated project. */
async function main() {
  const extension = path.resolve(__dirname, '..');
  const repository = path.resolve(extension, '../..');
  const required = ['SERIALIZER_COMPILER', 'SERIALIZER_CMAKE_TOOLS_PATH', 'SERIALIZER_CPP_TOOLS_PATH'];
  for (const name of required) { assert.ok(process.env[name] && fs.existsSync(process.env[name]), `Set ${name}`); }
  const parent = path.join(repository, 'out/extension-tests');
  fs.mkdirSync(parent, { recursive: true });
  const directory = fs.mkdtempSync(path.join(parent, 'navigation-project-'));
  const workspace = path.join(directory, 'workspace with spaces');
  fs.mkdirSync(workspace);
  for (const name of ['direct', 'other']) {
    fs.mkdirSync(path.join(workspace, name));
    fs.copyFileSync(path.join(repository, 'example/managed/ledger/ledger.serializer'), path.join(workspace, name, 'ledger.serializer'));
  }
  const source = '#include <ledger.hpp>\nnamespace {\nusing ledger = ledger_example::ledger;\n' +
    'using second_alias = ledger;\nnamespace model = ledger_example;\n' +
    'ledger alias_value;\nsecond_alias chained_value;\nmodel::ledger qualified_value;\n' +
    'ledger_example::ledger direct_value;\nstruct local_type {};\nlocal_type local_value;\n}\n';
  for (const file of ['direct.cpp', 'journal.cpp', 'other.cpp']) { fs.writeFileSync(path.join(workspace, file), source); }
  fs.writeFileSync(path.join(workspace, 'output.ini'), '[cpp]\nformat = false\n');
  fs.writeFileSync(path.join(workspace, 'CMakeLists.txt'), `cmake_minimum_required(VERSION 3.28)
project(serializer_navigation LANGUAGES CXX)
include("\${SERIALIZER_SOURCE_DIR}/cmake/serializer_generate.cmake")
add_library(serializer_runtime INTERFACE)
add_library(Serializer::runtime ALIAS serializer_runtime)
target_include_directories(serializer_runtime INTERFACE "\${SERIALIZER_SOURCE_DIR}/include" "\${CMAKE_BINARY_DIR}/runtime")
target_compile_features(serializer_runtime INTERFACE cxx_std_23)
add_library(records INTERFACE)
serializer_generate(TARGET records SCHEMAS "\${SERIALIZER_SOURCE_DIR}/schemas/managed_records.serializer"
  OUTPUT_DIRECTORY "\${CMAKE_BINARY_DIR}/runtime/rohit" GENERATOR "\${SERIALIZER_COMPILER}" CONFIG output.ini)
foreach(name direct other)
  add_library(\${name} OBJECT \${name}.cpp)
  target_link_libraries(\${name} PRIVATE records)
  serializer_generate(TARGET \${name} SCHEMAS \${name}/ledger.serializer
    GENERATOR "\${SERIALIZER_COMPILER}" CONFIG output.ini)
endforeach()
target_sources(direct PRIVATE journal.cpp)
`);
  const preset = { name: 'navigation', generator: process.env.SERIALIZER_TEST_CMAKE_GENERATOR || 'Ninja',
    binaryDir: '${sourceDir}/build', cacheVariables: { CMAKE_BUILD_TYPE: 'Debug',
      SERIALIZER_SOURCE_DIR: repository.replaceAll('\\', '/'), SERIALIZER_COMPILER: process.env.SERIALIZER_COMPILER.replaceAll('\\', '/') } };
  fs.writeFileSync(path.join(workspace, 'CMakePresets.json'), JSON.stringify({ version: 3, configurePresets: [preset],
    buildPresets: [{ name: 'navigation', configurePreset: 'navigation', configuration: 'Debug' }] }, null, 2));
  fs.mkdirSync(path.join(workspace, '.vscode'));
  fs.writeFileSync(path.join(workspace, '.vscode/settings.json'), JSON.stringify({
    'cmake.configureOnOpen': false, 'cmake.useCMakePresets': 'always', 'cmake.showOptionsMovedNotification': false,
    'C_Cpp.default.configurationProvider': 'ms-vscode.cmake-tools', 'cmake.loggingLevel': 'error',
    'files.exclude': { '**/build': true }, 'search.exclude': { '**/build': true }
  }));
  delete process.env.ELECTRON_RUN_AS_NODE;
  await runTests({ vscodeExecutablePath: process.env.VSCODE_EXECUTABLE_PATH,
    extensionDevelopmentPath: [extension, process.env.SERIALIZER_CMAKE_TOOLS_PATH, process.env.SERIALIZER_CPP_TOOLS_PATH],
    extensionTestsPath: path.join(__dirname, 'navigation_project_integration.cjs'),
    extensionTestsEnv: { SERIALIZER_TEST_WORKSPACE: workspace },
    launchArgs: [workspace, '--user-data-dir', path.join(directory, 'user-data'),
      '--extensions-dir', path.join(directory, 'extensions'), '--disable-workspace-trust',
      '--skip-welcome', '--skip-release-notes', '--disable-updates'] });
}

main().catch(error => { console.error(error); process.exitCode = 1; });
