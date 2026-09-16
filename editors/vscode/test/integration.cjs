const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vscode = require('vscode');

/** Exercise the installed editor APIs and real Serializer CMake generation end to end. */
async function runSuite() {
  const workspace = process.env.SERIALIZER_TEST_WORKSPACE;
  const schema = vscode.Uri.file(path.join(workspace, 'account.serializer'));
  const extension = vscode.extensions.getExtension('rohit-singh-gautam.serializer-language');
  assert.ok(extension, 'development extension is available');
  await extension.activate();
  const document = await vscode.workspace.openTextDocument(schema);
  await vscode.window.showTextDocument(document);
  assert.equal(document.languageId, 'serializer');
  const commands = await vscode.commands.getCommands(true);
  for (const command of ['generateHeaders', 'diagnoseHeader', 'openGeneratedHeader', 'configureIntelliSense']) {
    assert.ok(commands.includes(`serializer.${command}`));
  }
  const cmake = await vscode.extensions.getExtension('ms-vscode.cmake-tools').activate();
  await vscode.commands.executeCommand('cmake.setConfigurePreset', 'extension-test');
  await vscode.commands.executeCommand('cmake.setBuildPreset', 'extension-test');
  const project = await cmake.getApi(1).getProject(schema);
  assert.ok(project);
  const configured = await project.configureWithResult();
  assert.equal(configured.exitCode, 0, JSON.stringify(configured));
  const header = path.join(workspace, 'build/generated/smoke/account.hpp');
  assert.equal(fs.existsSync(header), false, 'configure must not generate headers');
  const generated = await vscode.commands.executeCommand('serializer.generateHeaders', schema);
  assert.equal(generated, true, 'extension reports successful generation');
  assert.ok(fs.readFileSync(header, 'utf8').includes('class account'));
  assert.equal(fs.existsSync(path.join(workspace, 'build/Debug/smoke.exe')), false,
    'header command must not compile the application');
  assert.equal(fs.existsSync(path.join(workspace, 'build/smoke')), false);
  await vscode.commands.executeCommand('serializer.openGeneratedHeader', schema);
  assert.equal(vscode.window.activeTextEditor.document.uri.fsPath, vscode.Uri.file(header).fsPath);
  const build = await project.buildWithResult(['smoke']);
  assert.equal(build.exitCode, 0, JSON.stringify(build));

  // Failed generation must report failure and keep the last good header.
  const original = fs.readFileSync(header, 'utf8');
  fs.writeFileSync(schema.fsPath, 'serializer version 999;\nclass broken {}\n');
  assert.equal(await vscode.commands.executeCommand('serializer.generateHeaders', schema), false);
  assert.equal(fs.readFileSync(header, 'utf8'), original);
  console.log('Serializer extension host: association, commands, real generation, header opening, consumer compilation, and failure handling passed.');
}

/** Fail unattended tests instead of leaving an editor open on an unexpected prompt. */
exports.run = async function run() {
  let timer;
  try {
    await Promise.race([runSuite(), new Promise((_, reject) => {
      timer = setTimeout(() => reject(new Error('Extension integration test timed out')), 300_000);
    })]);
  } finally {
    clearTimeout(timer);
  }
};
