const fs = require('node:fs');
const path = require('node:path');
const { runTests } = require('@vscode/test-electron');

/** Launch navigation tests with existing files and no CMake extension or build tools. */
async function main() {
  const extension = path.resolve(__dirname, '..');
  const parent = path.resolve(extension, '../../out/extension-tests');
  fs.mkdirSync(parent, { recursive: true });
  const directory = fs.mkdtempSync(path.join(parent, 'navigation-'));
  const workspace = path.join(directory, 'workspace with spaces');
  fs.mkdirSync(workspace);
  fs.mkdirSync(path.join(workspace, '.vscode'));
  fs.writeFileSync(path.join(workspace, '.vscode/settings.json'), JSON.stringify({
    'files.exclude': { '**/build': true }, 'search.exclude': { '**/build': true },
    'C_Cpp.default.compilerPath': '', 'C_Cpp.default.cppStandard': 'c++20'
  }));
  delete process.env.ELECTRON_RUN_AS_NODE;
  await runTests({ vscodeExecutablePath: process.env.VSCODE_EXECUTABLE_PATH,
    extensionDevelopmentPath: [extension, ...[process.env.SERIALIZER_CPP_TOOLS_PATH].filter(Boolean)],
    extensionTestsPath: path.join(__dirname, 'navigation_integration.cjs'),
    extensionTestsEnv: { SERIALIZER_TEST_WORKSPACE: workspace },
    launchArgs: [workspace, '--user-data-dir', path.join(directory, 'user-data'),
      '--extensions-dir', path.join(directory, 'extensions'), '--disable-workspace-trust',
      '--skip-welcome', '--skip-release-notes', '--disable-updates'] });
}

main().catch(error => { console.error(error); process.exitCode = 1; });
