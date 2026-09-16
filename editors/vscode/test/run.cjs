const fs = require('node:fs');
const path = require('node:path');
const { runTests } = require('@vscode/test-electron');

/** Launch an isolated extension host with a disposable real CMake consumer. */
async function main() {
  const cmakeTools = process.env.SERIALIZER_CMAKE_TOOLS_PATH;
  if (!cmakeTools || !fs.existsSync(path.join(cmakeTools, 'package.json'))) {
    throw new Error('Set SERIALIZER_CMAKE_TOOLS_PATH to an installed ms-vscode.cmake-tools extension directory.');
  }
  const extension = path.resolve(__dirname, '..');
  const repository = path.resolve(extension, '../..');
  const parent = path.join(repository, 'out/extension-tests');
  fs.mkdirSync(parent, { recursive: true });
  const directory = fs.mkdtempSync(path.join(parent, 'host-'));
  const workspace = path.join(directory, 'consumer with spaces');
  fs.cpSync(path.join(__dirname, 'fixture'), workspace, { recursive: true });
  const configurePreset = { name: 'extension-test', generator: process.env.SERIALIZER_TEST_CMAKE_GENERATOR || 'Ninja',
    binaryDir: '${sourceDir}/build', cacheVariables: {
      CMAKE_BUILD_TYPE: 'Debug', SERIALIZER_SOURCE_DIR: repository.replaceAll('\\', '/')
    } };
  fs.writeFileSync(path.join(workspace, 'CMakePresets.json'), JSON.stringify({ version: 3,
    configurePresets: [configurePreset],
    buildPresets: [{ name: 'extension-test', configurePreset: 'extension-test', configuration: 'Debug' }]
  }, null, 2));
  fs.mkdirSync(path.join(workspace, '.vscode'));
  fs.writeFileSync(path.join(workspace, '.vscode/settings.json'), JSON.stringify({
    'cmake.configureOnOpen': false, 'cmake.useCMakePresets': 'always',
    'cmake.showOptionsMovedNotification': false
  }));
  // Terminal hosts may set this for their own CLI; the test process needs Electron's UI mode.
  delete process.env.ELECTRON_RUN_AS_NODE;
  await runTests({
    vscodeExecutablePath: process.env.VSCODE_EXECUTABLE_PATH,
    extensionDevelopmentPath: [extension, cmakeTools],
    extensionTestsPath: path.join(__dirname, 'integration.cjs'),
    extensionTestsEnv: { SERIALIZER_TEST_WORKSPACE: workspace },
    launchArgs: [workspace, '--user-data-dir', path.join(directory, 'user-data'),
      '--extensions-dir', path.join(directory, 'extensions'), '--disable-workspace-trust',
      '--skip-welcome', '--skip-release-notes', '--disable-updates'],
  });
}

main().catch(error => { console.error(error); process.exitCode = 1; });
