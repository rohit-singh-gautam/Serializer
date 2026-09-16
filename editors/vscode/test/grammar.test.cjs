const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { test } = require('node:test');
const { Registry, INITIAL } = require('vscode-textmate');
const onig = require('vscode-oniguruma');

/** Tokenize real TextMate rules with the same regex engine used by VS Code. */
async function loadGrammar() {
  const wasm = fs.readFileSync(require.resolve('vscode-oniguruma/release/onig.wasm'));
  await onig.loadWASM(wasm.buffer.slice(wasm.byteOffset, wasm.byteOffset + wasm.byteLength));
  const registry = new Registry({
    onigLib: Promise.resolve({ createOnigScanner: patterns => new onig.OnigScanner(patterns),
      createOnigString: value => new onig.OnigString(value) }),
    loadGrammar: async () => JSON.parse(fs.readFileSync(path.join(__dirname, '../../serializer.tmLanguage.json'), 'utf8'))
  });
  return registry.loadGrammar('source.serializer');
}

test('grammar recognizes version, metadata, types, attributes and escaped strings', async () => {
  const grammar = await loadGrammar();
  const line = 'serializer version 1; class account stable_ids view readonly { public uint32 id ("wire_id", 3); }';
  const tokens = grammar.tokenizeLine(line, INITIAL).tokens;
  /** Resolve the scope at a token's first character. */
  function scope(word) {
    const offset = line.indexOf(word);
    return tokens.find(token => token.startIndex <= offset && token.endIndex > offset).scopes.at(-1);
  }
  assert.equal(scope('serializer'), 'keyword.control.serializer');
  assert.equal(scope('version'), 'keyword.control.serializer');
  assert.equal(scope('account'), 'entity.name.type.serializer');
  assert.equal(scope('stable_ids'), 'storage.modifier.serializer');
  assert.equal(scope('readonly'), 'storage.modifier.serializer');
  assert.equal(scope('uint32'), 'support.type.serializer');
  assert.equal(scope('wire_id'), 'string.quoted.double.serializer');
  assert.equal(scope('3);'), 'constant.numeric.serializer');
  const escaped = grammar.tokenizeLine('public string text { "a\\\"//still string" };', INITIAL);
  assert.ok(escaped.tokens.some(token => token.scopes.includes('constant.character.escape.serializer')));
  assert.ok(!escaped.tokens.some(token => token.scopes.includes('comment.line.double-slash.serializer')));
});

test('block comment state spans lines without highlighting keywords inside it', async () => {
  const grammar = await loadGrammar();
  const start = grammar.tokenizeLine('/* class fake {', INITIAL);
  const next = grammar.tokenizeLine('uint32 fake; */ class real {}', start.ruleStack);
  assert.equal(next.tokens[0].scopes.at(-1), 'comment.block.serializer');
  assert.ok(next.tokens.some(token => token.scopes.includes('entity.name.type.serializer')));
});

test('all repository schemas tokenize without losing the outer grammar state', async () => {
  const grammar = await loadGrammar();
  const root = path.resolve(__dirname, '../../..');
  let files = 0;
  /** Walk maintained schema directories only, excluding generated output. */
  function* schemas(directory) {
    for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
      const file = path.join(directory, entry.name);
      if (entry.isDirectory()) { yield* schemas(file); }
      else if (entry.name.endsWith('.serializer')) { yield file; }
    }
  }
  for (const directory of ['example', 'test', 'qualification']) {
    for (const file of schemas(path.join(root, directory))) {
      let state = INITIAL;
      for (const line of fs.readFileSync(file, 'utf8').split(/\r?\n/)) {
        state = grammar.tokenizeLine(line, state).ruleStack;
      }
      assert.equal(state.depth, 1, file);
      ++files;
    }
  }
  assert.ok(files >= 20);
});
