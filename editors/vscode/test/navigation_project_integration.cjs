const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vscode = require('vscode');
const manifest = require('../package.json');

/** Check caller navigation against actual generated ledger classes and target-specific include paths. */
async function runSuite() {
  const workspace = vscode.Uri.file(process.env.SERIALIZER_TEST_WORKSPACE).fsPath;
  await vscode.extensions.getExtension(`${manifest.publisher}.${manifest.name}`).activate();
  const cmake = await vscode.extensions.getExtension('ms-vscode.cmake-tools').activate();
  await vscode.extensions.getExtension('ms-vscode.cpptools').activate();
  await vscode.commands.executeCommand('cmake.setConfigurePreset', 'navigation');
  await vscode.commands.executeCommand('cmake.setBuildPreset', 'navigation');
  const project = await cmake.getApi(1).getProject(vscode.Uri.file(workspace));
  const configured = await project.configureWithResult();
  assert.equal(configured.exitCode, 0, JSON.stringify(configured));
  const missingHeader = path.join(workspace, 'build/generated/direct/ledger.hpp');
  assert.equal(fs.existsSync(missingHeader), false, 'configure alone must not generate output');
  const missingSource = await vscode.workspace.openTextDocument(path.join(workspace, 'direct.cpp'));
  const missingEditor = await vscode.window.showTextDocument(missingSource);
  const missingPosition = missingSource.positionAt(missingSource.getText().indexOf('ledger;\nusing'));
  missingEditor.selection = new vscode.Selection(missingPosition, missingPosition);
  await vscode.commands.executeCommand('serializer.goToSchemaDeclaration');
  assert.equal(vscode.window.activeTextEditor.document.uri.fsPath, missingSource.uri.fsPath,
    'unresolved caller type must not guess either same-name schema or generate output');
  assert.equal(fs.existsSync(missingHeader), false);
  const built = await project.buildWithResult(['serializer_generated_headers']);
  assert.equal(built.exitCode, 0, JSON.stringify(built));

  /** Query the registered services without changing the current position or invoking reference fallback. */
  async function targets(document, marker, kind) {
    const offset = document.getText().indexOf(marker);
    assert.ok(offset >= 0, marker);
    const locations = await vscode.commands.executeCommand(`vscode.execute${kind}Provider`, document.uri, document.positionAt(offset));
    return (locations ?? []).map(location => ({ uri: location.uri ?? location.targetUri,
      range: location.range ?? location.targetSelectionRange ?? location.targetRange }));
  }
  let checks = 0;
  for (const file of ['direct.cpp', 'journal.cpp', 'other.cpp']) {
    const profile = file === 'other.cpp' ? 'other' : 'direct';
    const header = path.join(workspace, 'build/generated', profile, 'ledger.hpp');
    const schema = path.join(workspace, profile, 'ledger.serializer');
    assert.ok(fs.existsSync(header));
    const document = await vscode.workspace.openTextDocument(path.join(workspace, file));
    await vscode.window.showTextDocument(document);
    let definitions;
    for (let attempt = 0; attempt < 80; ++attempt) {
      definitions = await targets(document, 'ledger;\nusing', 'Definition');
      if (definitions.some(target => target.uri.fsPath === header)) { break; }
      await new Promise(resolve => setTimeout(resolve, 250));
    }
    assert.ok(definitions.some(target => target.uri.fsPath === header), `${file}: ${JSON.stringify(definitions)}`);
    for (const [marker, localLine] of [['ledger;\nusing'], ['ledger alias_value', 2],
      ['second_alias chained_value', 3], ['ledger qualified_value'], ['ledger direct_value'], ['alias_value;', 5]]) {
      for (const kind of ['Declaration', 'Definition', 'TypeDefinition']) {
        const result = await targets(document, marker, kind);
        const expected = kind === 'Declaration' && localLine === undefined ? schema
          : kind === 'TypeDefinition' || localLine === undefined ? header : document.uri.fsPath;
        assert.ok(result.some(target => target.uri.fsPath === expected), `${file} ${kind} ${marker}: ${JSON.stringify(result)}`);
        assert.ok(result.every(target => [expected, ...(kind === 'Declaration' && localLine === undefined ? [header] : [])]
          .includes(target.uri.fsPath)), 'no unrelated alias, target or schema');
        if (kind !== 'TypeDefinition' && localLine !== undefined) { assert.ok(result.every(target => target.range.start.line === localLine)); }
        if (kind === 'Declaration' && localLine === undefined) { assert.ok(result.some(target => target.uri.fsPath === schema)); }
        ++checks;
      }
      const start = document.positionAt(document.getText().indexOf(marker));
      const word = document.getWordRangeAtPosition(start);
      for (const reversed of [false, true]) {
        const editor = await vscode.window.showTextDocument(document);
        editor.selection = reversed ? new vscode.Selection(word.end, word.start) : new vscode.Selection(word.start, word.end);
        await vscode.commands.executeCommand('serializer.goToSchemaDeclaration');
        const active = vscode.window.activeTextEditor;
        assert.equal(active.document.uri.fsPath, schema, `${marker}: explicit schema command, reversed=${reversed}`);
        assert.equal(active.document.getText(active.selection), 'ledger');
        ++checks;
      }
    }
    for (const kind of ['Declaration', 'Definition', 'TypeDefinition']) {
      const local = await targets(document, 'local_type local_value', kind);
      assert.ok(local.length && local.every(target => target.uri.fsPath === document.uri.fsPath && target.range.start.line === 9));
      ++checks;
    }
    // Actual editor actions must honor native semantics instead of returning the other files' aliases.
    for (const [command, marker, destination] of [
      ['editor.action.revealDefinition', 'ledger;\nusing', header],
      ['editor.action.revealDefinition', 'ledger alias_value', document.uri.fsPath],
      ['editor.action.goToTypeDefinition', 'ledger alias_value', header]
    ]) {
      for (const reversed of [false, true]) {
        const editor = await vscode.window.showTextDocument(document);
        const start = document.positionAt(document.getText().indexOf(marker));
        const word = document.getWordRangeAtPosition(start);
        editor.selection = reversed ? new vscode.Selection(word.end, word.start) : new vscode.Selection(word.start, word.end);
        await vscode.commands.executeCommand(command);
        assert.equal(vscode.window.activeTextEditor.document.uri.fsPath, destination, `${command} ${marker}, reversed=${reversed}`);
        ++checks;
      }
    }
  }
  console.log(`CMake/C++ navigation passed: ${checks} checks for qualified classes, namespace aliases, using aliases/chains, variables, native commands, reversed selections, unrelated types and target ownership; configure-only missing output remains silent.`);
}

/** Bound language-server startup so unattended failures leave no test editor running. */
exports.run = async function run() {
  let timer;
  try {
    await Promise.race([runSuite(), new Promise((_, reject) => {
      timer = setTimeout(() => reject(new Error('CMake/C++ navigation integration timed out')), 180_000);
    })]);
  } finally { clearTimeout(timer); }
};
