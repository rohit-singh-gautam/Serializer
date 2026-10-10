const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { test } = require('node:test');
const { Registry, INITIAL } = require('vscode-textmate');
const onig = require('vscode-oniguruma');

test('warning suppression highlights scoped rules and preserves ordinary keyword spellings', async () => {
  const grammar = await loadGrammar();
  for (const suffix of ['ignore(warning version)', 'ignore(warning magic, version)',
    'ignore /* intent */ ( /* rule */ warning magic, version)']) {
    const line = `public uint32 version (1) { 1 } ${suffix}; public uint32 ignore (2); public uint32 warning (3);`;
    const tokens = grammar.tokenizeLine(line, INITIAL).tokens;
    /** Read the innermost scope at a source offset. */
    const scope = offset => tokens.find(token => token.startIndex <= offset && token.endIndex > offset).scopes.at(-1);
    const start = line.indexOf(suffix);
    assert.equal(scope(start), 'keyword.control.serializer');
    assert.equal(scope(line.indexOf('warning', start)), 'keyword.control.serializer');
    for (const match of suffix.matchAll(/magic|version/g)) {
      assert.equal(scope(start + match.index), 'constant.other.warning.serializer');
    }
    for (const name of ['ignore (2)', 'warning (3)']) {
      assert.equal(scope(line.indexOf(name)), 'source.serializer', name);
    }
    assert.equal(grammar.tokenizeLine(line, INITIAL).ruleStack.depth, 1);
  }
  const start = grammar.tokenizeLine('public uint32 version (1) { 1 } ignore(', INITIAL);
  const end = grammar.tokenizeLine('  warning magic, version); public uint32 warning (2);', start.ruleStack);
  assert.ok(end.tokens.some(token => token.scopes.includes('constant.other.warning.serializer')));
  assert.equal(end.ruleStack.depth, 1);
  for (const line of ['public ignore value;', 'public warning value;']) {
    const tokens = grammar.tokenizeLine(line, INITIAL).tokens;
    const offset = line.indexOf(' ', 'public'.length) + 1;
    assert.equal(tokens.find(token => token.startIndex <= offset && token.endIndex > offset).scopes.at(-1),
      'entity.name.type.serializer');
  }
});

test('digest algorithms and byte extents highlight without claiming field names', async () => {
  const grammar = await loadGrammar();
  for (const algorithm of ['md5', 'sha1', 'sha224', 'sha256', 'sha384', 'sha512',
    'sha512_224', 'sha512_256', 'sha3_224', 'sha3_256', 'sha3_384', 'sha3_512']) {
    const line = `serializer version 1.3.0; class item { public digest(${algorithm}) content_hash (1); public digest[32] manual (2); public digest opaque; public uint32 digest (4); }`;
    const result = grammar.tokenizeLine(line, INITIAL);
    /** Read the innermost TextMate scope at a source word. */
    const scope = word => result.tokens.find(token => token.startIndex <= line.indexOf(word) &&
      token.endIndex > line.indexOf(word)).scopes.at(-1);
    assert.equal(scope(algorithm), 'constant.other.algorithm.serializer', algorithm);
    assert.equal(scope(`digest(${algorithm})`), 'support.type.serializer');
    assert.equal(scope('32]'), 'constant.numeric.serializer');
    for (const name of ['content_hash', 'manual', 'opaque', 'digest (4)']) {
      assert.ok(!['entity.name.type.serializer', 'support.type.serializer'].includes(scope(name)), name);
    }
    assert.equal(result.ruleStack.depth, 1);
  }
  const nested = 'class box<T = digest(sha3_256)> {} class item { public array digest(sha256) values; ' +
    'public box<digest[32], digest(sha512)> pair; public union(digest(sha224)=hash, uint8=empty) choice; }';
  const result = grammar.tokenizeLine(nested, INITIAL);
  for (const word of ['sha3_256', 'sha256', 'sha512', 'sha224']) {
    const offset = nested.indexOf(word);
    assert.equal(result.tokens.find(token => token.startIndex <= offset && token.endIndex > offset).scopes.at(-1),
      'constant.other.algorithm.serializer', word);
  }
  const alias = nested.indexOf('hash,');
  assert.equal(result.tokens.find(token => token.startIndex <= alias && token.endIndex > alias).scopes.at(-1),
    'variable.other.member.serializer');
  assert.equal(result.ruleStack.depth, 1);
  const legacy = 'serializer version 1.2.0; class digest {} class old { public digest value; }';
  const offset = legacy.indexOf('digest value');
  const tokens = grammar.tokenizeLine(legacy, INITIAL).tokens;
  assert.equal(tokens.find(token => token.startIndex <= offset && token.endIndex > offset).scopes.at(-1),
    'entity.name.type.serializer');
});

test('compact keyword spellings remain type operands in older ordinary fields', async () => {
  const grammar = await loadGrammar();
  const line = 'serializer version 1.0.0; class record { public compact_prefix prefix omit(json); public compact_varint varint omit /* format */ (binary_string); public strict checked; public lenient relaxed; }';
  const tokens = grammar.tokenizeLine(line, INITIAL).tokens;
  for (const word of ['compact_prefix', 'compact_varint', 'strict', 'lenient']) {
    const offset = line.indexOf(word);
    assert.equal(tokens.find(token => token.startIndex <= offset && token.endIndex > offset).scopes.at(-1),
      'entity.name.type.serializer', word);
  }
});

test('legacy declared variant types keep their type scope', async () => {
  const grammar = await loadGrammar();
  const line = 'serializer version 1; class variant {} class model { public variant value; }';
  const offset = line.indexOf('variant value');
  const tokens = grammar.tokenizeLine(line, INITIAL).tokens;
  assert.equal(tokens.find(token => token.startIndex <= offset && token.endIndex > offset).scopes.at(-1),
    'entity.name.type.serializer');
});

test('compact modifiers highlight unsigned and generic operands without claiming field names', async () => {
  const grammar = await loadGrammar();
  const line = 'serializer version 1.2.0; class box<T> { public compact_prefix strict uint32 value (3) {32}; public compact_varint lenient T other; }';
  const tokens = grammar.tokenizeLine(line, INITIAL).tokens;
  for (const word of ['public', 'compact_prefix', 'strict', 'compact_varint', 'lenient']) {
    const offset = line.indexOf(word);
    assert.equal(tokens.find(token => token.startIndex <= offset && token.endIndex > offset).scopes.at(-1),
      'storage.modifier.serializer', word);
  }
  const integer = line.indexOf('uint32');
  assert.equal(tokens.find(token => token.startIndex <= integer && token.endIndex > integer).scopes.at(-1),
    'support.type.serializer');
  const generic = line.indexOf('T other');
  assert.equal(tokens.find(token => token.startIndex <= generic && token.endIndex > generic).scopes.at(-1),
    'entity.name.type.serializer');
  assert.equal(grammar.tokenizeLine(line, INITIAL).ruleStack.depth, 1);
});

test('generic declarations, nested arguments and concrete field uses retain type scopes', async () => {
  const grammar = await loadGrammar();
  const line = 'class box<T> { public array box<lib::person> values; } class root { public box<box<uint32>> value; }';
  const tokens = grammar.tokenizeLine(line, INITIAL).tokens;
  for (const word of ['box', 'T', 'person', 'root']) {
    const offset = line.indexOf(word);
    assert.equal(tokens.find(token => token.startIndex <= offset && token.endIndex > offset).scopes.at(-1),
      'entity.name.type.serializer', word);
  }
  const offset = line.indexOf('uint32');
  assert.equal(tokens.find(token => token.startIndex <= offset && token.endIndex > offset).scopes.at(-1),
    'support.type.serializer');
  assert.equal(grammar.tokenizeLine(line, INITIAL).ruleStack.depth, 1);
});

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

test('schema language versions highlight all components without affecting following declarations', async () => {
  const grammar = await loadGrammar();
  for (const version of ['1', '1.0.0', '1.10.20']) {
    const line = `serializer /* language */ version ${version}; class account { public double amount { 1.25 }; }`;
    const result = grammar.tokenizeLine(line, INITIAL);
    const start = line.indexOf(version);
    const token = result.tokens.find(item => item.startIndex === start);
    assert.equal(token.endIndex, start + version.length);
    assert.equal(token.scopes.at(-1), 'constant.numeric.serializer');
    for (const [word, scope] of [['account', 'entity.name.type.serializer'],
      ['1.25', 'constant.numeric.serializer']]) {
      const offset = line.indexOf(word);
      assert.equal(result.tokens.find(item => item.startIndex <= offset && item.endIndex > offset).scopes.at(-1), scope);
    }
    assert.equal(result.ruleStack.depth, 1);
  }
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
    ['public variant(demo::order = sale, account = owner) value;', ['order', 'account'], ['sale', 'owner', 'value']],
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

test('managed declarations highlight the modifier before direct and collection types', async () => {
  const grammar = await loadGrammar();
  for (const line of ['class task stable_ids managed {', 'public managed task child (1);',
    'public managed map(uint64) task tasks (2);', 'public managed array task tasks (3);']) {
    const tokens = grammar.tokenizeLine(line, INITIAL).tokens;
    const at = line.indexOf('managed');
    assert.equal(tokens.find(token => token.startIndex <= at && token.endIndex > at).scopes.at(-1),
      'storage.modifier.serializer');
    const typeAt = line.lastIndexOf(' task ');
    if (typeAt >= 0) {
      assert.equal(tokens.find(token => token.startIndex <= typeAt + 1 && token.endIndex > typeAt + 1).scopes.at(-1),
        'entity.name.type.serializer');
    }
  }
});

test('dimension defaults and fixed extents highlight numbers, operators and parameters', async () => {
  const grammar = await loadGrammar();
  const line = 'class matrix<uint64 Rows, uint64 Cols = Rows, T = double> { public array[Rows * Cols + 1] T elements; } instantiate square = matrix<3>;';
  const tokens = grammar.tokenizeLine(line, INITIAL).tokens;
  for (const [word, scope] of [['uint64', 'support.type.serializer'], ['Rows', 'entity.name.type.serializer'], ['=', 'keyword.operator.serializer'], ['*', 'keyword.operator.serializer'], ['1', 'constant.numeric.serializer'], ['3', 'constant.numeric.serializer']]) {
    const offset = line.indexOf(word);
    assert.equal(tokens.find(token => token.startIndex <= offset && token.endIndex > offset).scopes.at(-1), scope, word);
  }
  assert.equal(grammar.tokenizeLine(line, INITIAL).ruleStack.depth, 1);
});


test('magic literals and generic exclusions preserve metadata scopes', async () => {
  const grammar = await loadGrammar();
  const line = "private magic (7) {'SRLFILE\\0'} omit(json, binary_none); public lib::record value (2) omit(binary_integer);";
  const tokens = grammar.tokenizeLine(line, INITIAL).tokens;
  for (const word of ['magic', 'omit']) {
    const offset = line.indexOf(word);
    assert.equal(tokens.find(token => token.startIndex <= offset && token.endIndex > offset).scopes.at(-1),
      'keyword.control.serializer');
  }
  const bytes = line.indexOf('SRLFILE');
  assert.ok(tokens.find(token => token.startIndex <= bytes && token.endIndex > bytes).scopes.includes('string.quoted.single.serializer'));
  const type = line.indexOf('record');
  assert.equal(tokens.find(token => token.startIndex <= type && token.endIndex > type).scopes.at(-1),
    'entity.name.type.serializer');
});


test('version lifecycles and component types use the canonical shared grammar', async () => {
  const grammar = await loadGrammar();
  const line = 'public version version3 ver (6) { "1.10.0" } compatibility { "1.2.0" }; obsolete(3) public uint64 id (2); created(3) replaced(id) public float identity (4); reserve id {7} variable {retired} display {"old"};';
  const tokens = grammar.tokenizeLine(line, INITIAL).tokens;
  for (const keyword of ['version ', 'compatibility', 'obsolete', 'created', 'replaced', 'reserve']) {
    const offset = line.indexOf(keyword);
    assert.equal(tokens.find(t => t.startIndex <= offset && t.endIndex > offset).scopes.at(-1), 'keyword.control.serializer', keyword);
  }
  const offset = line.indexOf('version3');
  assert.equal(tokens.find(t => t.startIndex <= offset && t.endIndex > offset).scopes.at(-1), 'support.type.serializer');
});

test('release catalogs and nested acceptance policies use the shared grammar', async () => {
  const grammar = await loadGrammar();
  const line = 'public version { 10 } releases { 8 { "2024-01-01" }; 10 { "2026-01-01" }; } policy { all { max_age { 2 years }; any { keep_last { 3 }; compatibility { 8 }; released_since { "2025-01-01" }; expires_on { "2027-01-01" }; }; }; };';
  const tokens = grammar.tokenizeLine(line, INITIAL).tokens;
  for (const keyword of ['releases', 'policy', 'all', 'any', 'max_age', 'keep_last', 'compatibility', 'released_since', 'expires_on']) {
    const offset = line.indexOf(keyword);
    assert.equal(tokens.find(t => t.startIndex <= offset && t.endIndex > offset).scopes.at(-1), 'keyword.control.serializer', keyword);
  }
  assert.equal(grammar.tokenizeLine(line, INITIAL).ruleStack.depth, 1);
});

test('inferred arrays and typed magic preserve element and enum scopes', async () => {
  const grammar = await loadGrammar();
  const line = "public array[] lib::status values (1) {lib::status::ready, lib::status::done}; private magic lib::status (99) {lib::status::ready}; protected magic uint32 {42}; public array[] float numbers {1'000.25, 2'000.5};";
  const tokens = grammar.tokenizeLine(line, INITIAL).tokens;
  for (const match of line.matchAll(/status/g)) {
    const expected = 'entity.name.type.serializer';
    assert.equal(tokens.find(token => token.startIndex <= match.index && token.endIndex > match.index).scopes.at(-1), expected);
  }
  for (const [word, scope] of [['magic', 'keyword.control.serializer'], ['uint32', 'support.type.serializer'], ['42', 'constant.numeric.serializer']]) {
    const offset = line.indexOf(word);
    assert.equal(tokens.find(token => token.startIndex <= offset && token.endIndex > offset).scopes.at(-1), scope);
  }
  assert.equal(grammar.tokenizeLine(line, INITIAL).ruleStack.depth, 1);
});
