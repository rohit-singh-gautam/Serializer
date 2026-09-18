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

test('unquoted includes highlight keywords, relative paths, comments and terminators', async () => {
  const grammar = await loadGrammar();
  for (const file of ['common', '../shared/common', './v1/shared-types', 'v1.2/common', '.common',
    'common.serializer', '../shared/common.serializer', './v1/shared-types.serializer', 'order.v2.serializer']) {
    const line = `include /* schema */ ${file}; // done`;
    const result = grammar.tokenizeLine(line, INITIAL);
    /** Find the innermost scope at the start of a directive token. */
    const scope = word => result.tokens.find(token => token.startIndex <= line.indexOf(word) &&
      token.endIndex > line.indexOf(word)).scopes.at(-1);
    assert.equal(scope('include'), 'keyword.control.import.serializer');
    assert.equal(scope(file), 'string.unquoted.path.serializer');
    assert.equal(scope(';'), 'punctuation.separator.serializer');
    assert.equal(scope('schema'), 'comment.block.serializer');
    assert.equal(scope('done'), 'comment.line.double-slash.serializer');
    assert.equal(result.ruleStack.depth, 1);
  }
  for (const file of ['"common"', '<common>', '"common.serializer"', '<common.serializer>',
    'common.hpp', 'order.v2', 'common.', '/common', 'folder/', '.', '..', 'folder/.', 'folder/..']) {
    const result = grammar.tokenizeLine(`include ${file};`, INITIAL);
    assert.ok(result.tokens.some(token => token.scopes.includes('invalid.illegal.include.serializer')));
  }
  const start = grammar.tokenizeLine('include /* dependency', INITIAL);
  const finish = grammar.tokenizeLine('*/ common; class real {}', start.ruleStack);
  assert.ok(finish.tokens.some(token => token.scopes.includes('string.unquoted.path.serializer')));
  assert.ok(finish.tokens.some(token => token.scopes.includes('entity.name.type.serializer')));
  assert.equal(finish.ruleStack.depth, 1);
  for (const line of ['// include common.serializer;', 'public string text { "include common.serializer;" };']) {
    const result = grammar.tokenizeLine(line, INITIAL);
    assert.ok(!result.tokens.some(token => token.scopes.includes('meta.include.serializer')));
  }
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
        const result = grammar.tokenizeLine(line, state);
        assert.ok(!result.tokens.some(token => token.scopes.includes('invalid.illegal.include.serializer')),
          `${file}: ${line}`);
        state = result.ruleStack;
      }
      assert.equal(state.depth, 1, file);
      ++files;
    }
  }
  assert.ok(files >= 20);
});

test('custom field, container, base and default types use theme type colors, not field-name colors', async () => {
  const grammar = await loadGrammar();
  for (const [line, types, fields] of [
    ['public array demo::order orders (3);', ['order'], ['orders']],
    ['public demo::snapshot snapshot (4);', ['demo::snapshot'], ['snapshot (']],
    ['public map(uint64) demo::customer customers (5);', ['customer'], ['customers']],
    ['public account owner;', ['account'], ['owner']],
    ['class derived : public demo::base (1) {', ['base'], []],
    ['public union(demo::order = sale, account = owner) value;', ['order', 'account'], ['sale', 'owner', 'value']],
    ['public state status { demo::state::ready };', ['state status', 'demo::state'], ['status']]
  ]) {
    const tokens = grammar.tokenizeLine(line, INITIAL).tokens;
    for (const type of types) {
      const offset = line.indexOf(type) + (type.includes('::') ? type.lastIndexOf('::') + 2 : 0);
      assert.equal(tokens.find(token => token.startIndex <= offset && token.endIndex > offset).scopes.at(-1), 'entity.name.type.serializer', line);
    }
    for (const field of fields) {
      const offset = line.indexOf(field);
      assert.notEqual(tokens.find(token => token.startIndex <= offset && token.endIndex > offset).scopes.at(-1), 'entity.name.type.serializer', line);
    }
  }
  for (const line of ['// public demo::order orders;', 'public string label { "demo::order" };']) {
    const tokens = grammar.tokenizeLine(line, INITIAL).tokens;
    const offset = line.indexOf('demo::order');
    assert.ok(!tokens.find(token => token.startIndex <= offset && token.endIndex > offset).scopes.includes('entity.name.type.serializer'));
  }
});
