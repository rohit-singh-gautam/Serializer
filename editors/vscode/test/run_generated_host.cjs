const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { execFileSync } = require('node:child_process');
const { runTests } = require('@vscode/test-electron');

/** Generate every owning output before launching a disposable real editor host. */
async function main() {
  const extension = path.resolve(__dirname, '..');
  const repository = path.resolve(extension, '../..');
  assert.ok(process.env.SERIALIZER_COMPILER, 'Set SERIALIZER_COMPILER to the current compiler');
  const parent = path.join(repository, 'out/extension-tests');
  fs.mkdirSync(parent, { recursive: true });
  const directory = fs.mkdtempSync(path.join(parent, 'generated-host-'));
  const workspace = path.join(directory, 'workspace with spaces');
  const schemaDirectory = path.join(workspace, 'types');
  fs.mkdirSync(schemaDirectory, { recursive: true });
  fs.writeFileSync(path.join(workspace, 'model.serializer'),
    'serializer version 1;\ninclude types/item;\n' +
    'namespace host_test { class envelope { public host_test::item owner (1); } }\n');
  fs.writeFileSync(path.join(schemaDirectory, 'item.serializer'),
    'serializer version 1;\nnamespace host_test { class item { public int32 value (1); } }\n');
  const outputs = { cpp: 'model.hpp', java: 'Model.java', js: 'model.mjs', typescript: 'model.d.mts',
    go: 'model.go', csharp: 'Model.cs', rust: 'model.rs', python: 'model.py',
    swift: 'Model.swift', kotlin: 'Model.kt', c: 'model.h' };
  for (const [language, file] of Object.entries(outputs)) {
    const output = path.join(workspace, 'build', language, file);
    fs.mkdirSync(path.dirname(output), { recursive: true });
    execFileSync(process.env.SERIALIZER_COMPILER, ['--input', path.join(workspace, 'model.serializer'),
      '--language', language, `--${language}.output`, output, '--include-mode', 'inline',
      '--cpp.format', 'false', '--depfile', `${output}.d`], { stdio: 'pipe' });
  }
  fs.writeFileSync(path.join(workspace, 'outputs.json'), JSON.stringify(outputs));
  fs.mkdirSync(path.join(workspace, '.vscode'));
  fs.writeFileSync(path.join(workspace, '.vscode/settings.json'), JSON.stringify({
    'files.exclude': { '**/build': true }, 'search.exclude': { '**/build': true }
  }));
  delete process.env.ELECTRON_RUN_AS_NODE;
  await runTests({ vscodeExecutablePath: process.env.VSCODE_EXECUTABLE_PATH,
    extensionDevelopmentPath: extension,
    extensionTestsPath: path.join(__dirname, 'generated_host_integration.cjs'),
    extensionTestsEnv: { SERIALIZER_TEST_WORKSPACE: workspace },
    launchArgs: [workspace, '--user-data-dir', path.join(directory, 'user-data'),
      '--extensions-dir', path.join(directory, 'extensions'), '--disable-workspace-trust',
      '--skip-welcome', '--skip-release-notes', '--disable-updates'] });
}

main().catch(error => { console.error(error); process.exitCode = 1; });
