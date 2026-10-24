const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vscode = require('vscode');
const manifest = require('../package.json');

/** Normalize editor locations without weakening exact selection checks. */
function location(target) {
  return { uri: target.uri ?? target.targetUri,
    range: target.range ?? target.targetSelectionRange ?? target.targetRange };
}

/** Qualify actual provider registration and live-buffer dispatch for all eleven outputs. */
exports.run = async function run() {
  const workspace = process.env.SERIALIZER_TEST_WORKSPACE;
  const extension = vscode.extensions.getExtension(`${manifest.publisher}.${manifest.name}`);
  await extension.activate();
  const outputs = JSON.parse(fs.readFileSync(path.join(workspace, 'outputs.json'), 'utf8'));
  assert.equal(Object.keys(outputs).length, 11);
  const schema = await vscode.workspace.openTextDocument(path.join(workspace, 'model.serializer'));
  const included = await vscode.workspace.openTextDocument(path.join(workspace, 'types/item.serializer'));
  const edit = new vscode.WorkspaceEdit();
  for (const document of [schema, included]) {
    edit.insert(document.uri, new vscode.Position(0, 0), '// unsaved \u{1f642} host qualification\n');
  }
  assert.ok(await vscode.workspace.applyEdit(edit));
  let checks = 0;
  for (const [token, source] of [['envelope', schema], ['host_test::item', included]]) {
    const offset = schema.getText().indexOf(token);
    for (const endpoint of [offset, offset + token.length]) {
      const declarations = (await vscode.commands.executeCommand('vscode.executeDeclarationProvider',
        schema.uri, schema.positionAt(endpoint))).map(location);
      assert.equal(declarations.length, 1);
      assert.equal(declarations[0].uri.fsPath, source.uri.fsPath);
      assert.equal(source.getText(declarations[0].range), token.split('::').at(-1));
      const definitions = (await vscode.commands.executeCommand('vscode.executeDefinitionProvider',
        schema.uri, schema.positionAt(endpoint))).map(location);
      const expected = Object.entries(outputs).map(([language, file]) =>
        vscode.Uri.file(path.join(workspace, 'build', language, file)).fsPath).sort();
      assert.deepEqual(definitions.map(target => target.uri.fsPath).sort(), expected);
      ++checks;
      for (const target of definitions) {
        const generated = await vscode.workspace.openTextDocument(target.uri);
        assert.match(generated.getText(target.range), new RegExp(token.split('::').at(-1), 'i'));
        for (const reversed of [false, true]) {
          const editor = await vscode.window.showTextDocument(generated);
          editor.selection = reversed ? new vscode.Selection(target.range.end, target.range.start)
            : new vscode.Selection(target.range.start, target.range.end);
          await vscode.commands.executeCommand('editor.action.revealDeclaration');
          const active = vscode.window.activeTextEditor;
          assert.equal(active.document.uri.fsPath, source.uri.fsPath);
          const word = active.document.getWordRangeAtPosition(active.selection.active);
          assert.ok(word, `native declaration cursor in ${active.document.uri.fsPath}`);
          assert.equal(active.document.getText(word), token.split('::').at(-1));
          assert.equal(active.document.offsetAt(word.start),
            source.getText().indexOf(`class ${token.split('::').at(-1)}`) + 'class '.length);
          ++checks;
        }
      }
    }
  }
  for (const document of [schema, included]) {
    assert.ok(document.isDirty);
    assert.ok(!fs.readFileSync(document.uri.fsPath, 'utf8').includes('unsaved'));
  }
  console.log(`PASS: ${checks} real-host checks, eleven generated languages, qualified includes, ` +
    'cursor endpoints, reversed selections, excluded build outputs and live unsaved source offsets.');
};
