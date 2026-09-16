const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { createRequire } = require('node:module');
const { test } = require('node:test');

/** Load command handlers with controlled editor/build APIs, without starting processes. */
function harness({ trusted = true, apiAvailable = true, exitCode = 0, cancelled = false,
  saveResult = true, target = 'consumer_serializer_headers' } = {}) {
  const commands = new Map();
  const messages = [];
  const builds = [];
  const calls = [];
  const folder = { uri: { fsPath: path.resolve('workspace'), toString: () => 'file:///workspace' } };
  const project = {
    codeModel: { configurations: [{ name: 'Debug', projects: [{ name: 'app',
      sourceDirectory: folder.uri.fsPath, targets: [{ name: 'consumer_serializer_headers' }] }] }] },
    getActiveBuildType: async () => 'Debug',
    getBuildDirectory: async () => path.resolve('workspace/build'),
    buildWithResult: async (targets, token) => { builds.push({ targets, token }); return { exitCode }; }
  };
  const disposable = { dispose() {} };
  const vscode = {
    workspace: {
      isTrusted: trusted, workspaceFolders: [folder],
      textDocuments: [{ isDirty: true, uri: { fsPath: path.resolve('workspace/model.serializer') },
        save: async () => { calls.push('save'); return saveResult; } }],
      getWorkspaceFolder: () => folder,
      getConfiguration: () => ({ get: () => target })
    },
    commands: { registerCommand: (id, fn) => { commands.set(id, fn); return disposable; } },
    window: {
      createOutputChannel: () => ({ ...disposable, appendLine: text => messages.push(text), show() {} }),
      showErrorMessage: async message => { messages.push(message); },
      showInformationMessage: async message => { messages.push(message); },
      withProgress: async (_options, action) => action({}, { isCancellationRequested: cancelled })
    },
    ProgressLocation: { Notification: 15 },
    CodeActionKind: { QuickFix: 'quickfix' },
    languages: { registerCodeActionsProvider: () => disposable }
  };
  const file = path.resolve(__dirname, '../out/extension.js');
  const localRequire = createRequire(file);
  const exports = {};
  const execute = vm.runInThisContext(`(function(require, exports) { ${fs.readFileSync(file, 'utf8')}\n})`, { filename: file });
  execute(name => name === 'vscode' ? vscode : name === 'vscode-cmake-tools' ? {
    Version: { v1: 1 }, getCMakeToolsApi: async () => {
      calls.push('api');
      return apiAvailable ? { getProject: async () => project } : undefined;
    }
  } : localRequire(name), exports);
  exports.activate({ subscriptions: [] });
  return { commands, messages, builds, calls };
}

test('untrusted command invocation never activates CMake Tools or saves/builds files', async () => {
  const state = harness({ trusted: false });
  for (const command of state.commands.values()) {
    assert.equal(await command(), false);
  }
  assert.deepEqual(state.calls, []);
  assert.deepEqual(state.builds, []);
});

test('missing CMake Tools yields an actionable failure without a build', async () => {
  const state = harness({ apiAvailable: false });
  assert.equal(await state.commands.get('serializer.generateHeaders')(), false);
  assert.deepEqual(state.builds, []);
  assert.ok(state.messages.some(message => message.includes('Install or enable Microsoft CMake Tools')));
});

test('generation saves inputs and builds only the configured header target', async () => {
  const state = harness();
  assert.equal(await state.commands.get('serializer.generateHeaders')(), true);
  assert.deepEqual(state.calls, ['api', 'save']);
  assert.deepEqual(state.builds[0].targets, ['consumer_serializer_headers']);
});

test('failed saves and unavailable targets never start generation', async () => {
  for (const options of [{ saveResult: false }, { target: 'absent' }]) {
    const state = harness(options);
    assert.equal(await state.commands.get('serializer.generateHeaders')(), false);
    assert.equal(state.builds.length, 0);
  }
});

test('failed and cancelled builds never announce successful generation', async () => {
  for (const options of [{ exitCode: 1 }, { cancelled: true }]) {
    const state = harness(options);
    assert.equal(await state.commands.get('serializer.generateHeaders')(), false);
    assert.ok(!state.messages.some(message => message.includes('completed.')));
  }
});
