const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { test } = require('node:test');
const { Registry, INITIAL } = require('vscode-textmate');
const onig = require('vscode-oniguruma');

const nativeLanguages = [
  ['cpp', 'cpp'], ['c', 'c'], ['csharp', 'cs'], ['java', 'java'],
  ['javascript', 'js'], ['typescript', 'ts'], ['go', 'go'], ['rust', 'rust'],
  ['python', 'python'], ['swift', 'swift'], ['kotlin', 'kotlin']
];

/** Supply independent native grammar rules to verify dispatch and editor language identity. */
async function loadGrammar(importNativeGrammars = true, unavailableNativeScopes = []) {
  const wasm = fs.readFileSync(require.resolve('vscode-oniguruma/release/onig.wasm'));
  await onig.loadWASM(wasm.buffer.slice(wasm.byteOffset, wasm.byteOffset + wasm.byteLength));
  const grammar = JSON.parse(fs.readFileSync(path.join(__dirname, '../../serializer.tmLanguage.json'), 'utf8'));
  const manifest = JSON.parse(fs.readFileSync(path.join(__dirname, '../package.json'), 'utf8'));
  const contributions = manifest.contributes.grammars.find(item => item.scopeName === grammar.scopeName);
  const embeddedLanguages = Object.fromEntries(nativeLanguages.map(([tag], index) => {
    assert.equal(contributions.embeddedLanguages[`meta.embedded.block.${tag}`], tag);
    return [`meta.embedded.block.${tag}`, index + 2];
  }));
  const registry = new Registry({
    onigLib: Promise.resolve({ createOnigScanner: patterns => new onig.OnigScanner(patterns),
      createOnigString: value => new onig.OnigString(value) }),
    loadGrammar: async scope => {
      if (scope === grammar.scopeName) { return grammar; }
      const native = nativeLanguages.find(([, suffix]) => scope === `source.${suffix}`);
      if (!importNativeGrammars || !native || unavailableNativeScopes.includes(scope)) { return null; }
      return { scopeName: scope, patterns: [
        { name: `keyword.control.imported.${native[1]}`, match: '\\breturn\\b' },
        { name: `entity.name.type.imported.${native[1]}`, match: '\\bNativeOnly\\b' },
        // Deliberately expose punctuation without owning its nesting: Serializer must bound the body.
        { name: `punctuation.section.block.imported.${native[1]}`, match: '[{}]' }
      ] };
    }
  });
  return registry.loadGrammarWithEmbeddedLanguages(grammar.scopeName, 1, embeddedLanguages);
}

/** Find the scope stack for a source spelling in one tokenized line. */
function scopesAt(result, line, spelling, fromIndex = 0) {
  const offset = line.indexOf(spelling, fromIndex);
  assert.ok(offset >= 0, spelling);
  return result.tokens.find(token => token.startIndex <= offset && token.endIndex > offset).scopes;
}

/** Decode the language ID from TextMate's binary token metadata at a source offset. */
function languageAt(result, offset) {
  for (let index = result.tokens.length - 2; index >= 0; index -= 2) {
    if (result.tokens[index] <= offset) { return result.tokens[index + 1] & 0xff; }
  }
  assert.fail(`No encoded token at ${offset}`);
}

test('all native target bodies import their language grammar and embedded language identity', async () => {
  const grammar = await loadGrammar();
  for (const [index, [tag, suffix]] of nativeLanguages.entries()) {
    const line = `public function int32 calculate() readonly ${tag} { if (true) { return 17; } }; public uint32 after (1);`;
    const result = grammar.tokenizeLine(line, INITIAL);
    assert.equal(scopesAt(result, line, 'return').at(-1), `keyword.control.imported.${suffix}`);
    assert.ok(scopesAt(result, line, 'return').includes(`meta.embedded.block.${tag}`));
    assert.ok(scopesAt(result, line, 'return').includes(`source.${suffix}`));
    assert.equal(scopesAt(result, line, 'uint32').at(-1), 'support.type.serializer');
    assert.equal(result.ruleStack.depth, 1, tag);
    const encoded = grammar.tokenizeLine2(line, INITIAL);
    assert.equal(languageAt(encoded, line.indexOf(tag)), 1, `${tag} header`);
    assert.equal(languageAt(encoded, line.indexOf('return')), index + 2, `${tag} body`);
    assert.equal(languageAt(encoded, line.indexOf('uint32')), 1, `${tag} following schema`);
  }
});

test('native file and class hooks preserve header scopes and resume schema after nested bodies', async () => {
  const grammar = await loadGrammar();
  for (const [tag, suffix] of nativeLanguages) {
    const prefixes = tag === 'c' || tag === 'go' ? [`${tag} preamble`] :
      [`${tag} preamble`, `public ${tag}`, `private ${tag}`, `protected ${tag}`];
    for (const prefix of prefixes) {
      const line = `${prefix} { class NativeOnly { /* } { */ return 1; } } class following {}`;
      const result = grammar.tokenizeLine(line, INITIAL);
      assert.equal(scopesAt(result, line, tag, prefix.lastIndexOf(tag)).at(-1),
        'storage.modifier.language.serializer');
      assert.equal(scopesAt(result, line, 'NativeOnly').at(-1), `entity.name.type.imported.${suffix}`);
      assert.equal(scopesAt(result, line, 'following').at(-1), 'entity.name.type.serializer');
      assert.equal(result.ruleStack.depth, 1, prefix);
    }
    let state = grammar.tokenizeLine(`${tag} /* native header */ preamble`, INITIAL).ruleStack;
    const opened = grammar.tokenizeLine('{ return 2;', state);
    assert.equal(scopesAt(opened, '{ return 2;', 'return').at(-1), `keyword.control.imported.${suffix}`);
    const closed = grammar.tokenizeLine('} public uint32 after (1);', opened.ruleStack);
    assert.equal(scopesAt(closed, '} public uint32 after (1);', 'uint32').at(-1), 'support.type.serializer');
    assert.equal(closed.ruleStack.depth, 1, tag);
  }
});

test('missing language providers retain fallback colors and isolate strings, comments and braces', async () => {
  const grammar = await loadGrammar(false);
  for (const [tag] of nativeLanguages) {
    const line = `${tag} preamble { if (true) { return "} { \\\" literal"; /* } { */ } } public uint32 after (1);`;
    const result = grammar.tokenizeLine(line, INITIAL);
    assert.equal(scopesAt(result, line, 'return').at(-1), 'keyword.control.native.serializer');
    assert.ok(scopesAt(result, line, 'literal').some(scope => scope.startsWith('string.')));
    assert.equal(scopesAt(result, line, 'uint32').at(-1), 'support.type.serializer');
    assert.equal(result.ruleStack.depth, 1, tag);
  }
  const line = 'cpp preamble { // } { return\n';
  const start = grammar.tokenizeLine(line.trimEnd(), INITIAL);
  assert.ok(scopesAt(start, line.trimEnd(), 'return').some(scope => scope.startsWith('comment.')));
  const finish = grammar.tokenizeLine('return 9; } public uint32 after (1);', start.ruleStack);
  assert.equal(scopesAt(finish, 'return 9; } public uint32 after (1);', 'uint32').at(-1), 'support.type.serializer');
  assert.equal(finish.ruleStack.depth, 1);
});

test('fallback raw and multiline strings preserve native block boundaries', async () => {
  const grammar = await loadGrammar(false);
  for (const [tag, literal] of [
    ['cpp', 'R"delim(} " { literal)delim"'],
    ['rust', 'r##"} " { literal"##'],
    ['csharp', '@"} "" { literal"'],
    ['csharp', '"""} " { literal"""'],
    ['java', '"""} " { literal"""'],
    ['python', '"""} " { literal"""'],
    ['python', "''' } ' { literal '''"],
    ['swift', '"""} " { literal"""'],
    ['kotlin', '"""} " { literal"""'],
    ['javascript', '`} " { literal ${value}`'],
    ['typescript', '`} " { literal ${value}`'],
    ['go', '`} " { literal`']
  ]) {
    const line = `${tag} preamble { return ${literal}; } public uint32 after (1);`;
    const result = grammar.tokenizeLine(line, INITIAL);
    assert.ok(scopesAt(result, line, 'literal').some(scope => scope.startsWith('string.')), tag);
    assert.equal(scopesAt(result, line, 'uint32').at(-1), 'support.type.serializer', tag);
    assert.equal(result.ruleStack.depth, 1, tag);
  }
  const start = grammar.tokenizeLine('python preamble { # } { ignored', INITIAL);
  assert.ok(scopesAt(start, 'python preamble { # } { ignored', 'ignored').includes('comment.line.number-sign.python'));
  const finish = grammar.tokenizeLine('return 3; } class following {}', start.ruleStack);
  assert.equal(scopesAt(finish, 'return 3; } class following {}', 'following').at(-1), 'entity.name.type.serializer');
  assert.equal(finish.ruleStack.depth, 1);
});

test('native-looking comments, strings and default-valued field names remain Serializer tokens', async () => {
  const grammar = await loadGrammar();
  for (const line of ['// cpp { return 1; }', 'public string text { "java { return 1; }" };',
    'public uint32 cpp (1); public uint32 java (2);',
    ...nativeLanguages.flatMap(([tag]) => [`public uint32 ${tag} {3};`,
      `public string ${tag} {"body"};`])]) {
    const result = grammar.tokenizeLine(line, INITIAL);
    assert.ok(!result.tokens.some(token => token.scopes.some(scope =>
      nativeLanguages.some(([tag]) => scope === `meta.embedded.block.${tag}`))));
    assert.equal(result.ruleStack.depth, 1);
  }
  for (const [tag] of nativeLanguages) {
    const first = grammar.tokenizeLine(`public uint32 ${tag}`, INITIAL);
    const last = grammar.tokenizeLine('{3}; class following {}', first.ruleStack);
    assert.equal(scopesAt(last, '{3}; class following {}', 'following').at(-1), 'entity.name.type.serializer');
    assert.ok(!last.tokens.some(token => token.scopes.includes(`meta.embedded.block.${tag}`)));
    assert.equal(last.ruleStack.depth, 1, tag);
  }
});

test('recognized native headers preserve multiline comments before their opening brace', async () => {
  const grammar = await loadGrammar();
  for (const prefix of ['public cpp', 'cpp preamble', 'public function int32 value() readonly cpp']) {
    for (const comment of [['/* opening header', 'comment */'], ['// opening header']]) {
      let state = INITIAL;
      for (const line of [`${prefix} ${comment[0]}`, ...comment.slice(1)]) {
        state = grammar.tokenizeLine(line, state).ruleStack;
      }
      const body = grammar.tokenizeLine('{ return 1; }; public uint32 following (1);', state);
      assert.equal(scopesAt(body, '{ return 1; }; public uint32 following (1);', 'return').at(-1),
        'keyword.control.imported.cpp', prefix);
      assert.equal(scopesAt(body, '{ return 1; }; public uint32 following (1);', 'uint32').at(-1),
        'support.type.serializer');
      assert.equal(body.ruleStack.depth, 1, prefix);
    }
  }
  const first = grammar.tokenizeLine('public function int32 value() readonly /* opening header', INITIAL);
  const second = grammar.tokenizeLine('comment */ cpp { return 1; } java { return 2; }; class following {}', first.ruleStack);
  assert.equal(scopesAt(second, 'comment */ cpp { return 1; } java { return 2; }; class following {}', 'following').at(-1),
    'entity.name.type.serializer');
  assert.equal(second.ruleStack.depth, 1);
});

test('JavaScript can reuse an installed TypeScript grammar when its own provider is unavailable', async () => {
  const grammar = await loadGrammar(true, ['source.js']);
  const line = 'javascript preamble { return 7; } class following {}';
  const result = grammar.tokenizeLine(line, INITIAL);
  assert.equal(scopesAt(result, line, 'return').at(-1), 'keyword.control.imported.ts');
  assert.ok(scopesAt(result, line, 'return').includes('meta.embedded.block.javascript'));
  assert.equal(scopesAt(result, line, 'following').at(-1), 'entity.name.type.serializer');
  assert.equal(result.ruleStack.depth, 1);
});

test('fallback Rust lifetime spellings do not consume following schema declarations', async () => {
  const grammar = await loadGrammar(false);
  // This is a tokenizer robustness fixture; the compiler's native scanner does not support lifetimes.
  const line = "rust preamble { fn value<'a>(input: &'a str) -> &'a str { input } } class following {}";
  const result = grammar.tokenizeLine(line, INITIAL);
  assert.equal(scopesAt(result, line, "'a").at(-1), 'storage.modifier.lifetime.rust');
  assert.equal(scopesAt(result, line, 'following').at(-1), 'entity.name.type.serializer');
  assert.equal(result.ruleStack.depth, 1);
});

test('fallback C# interpolated strings retain nested strings and expression braces', async () => {
  const grammar = await loadGrammar(false);
  for (const literal of ['$"{ "}" }"', '$@"{ "}" }"', '@$"{ "}" }"',
    '$"{ new { Value = "}" } }"', '$"{ $"{ "}" }" }"',
    '$"escaped {{ }} { "}" }"', '$"""} " native_text {"""']) {
    const line = `csharp preamble { public static class Native { public static string Label() => ${literal}; } ` +
      'public static class Second {} } class following {}';
    const result = grammar.tokenizeLine(line, INITIAL);
    assert.ok(scopesAt(result, line, 'Second').includes('meta.embedded.block.csharp'), literal);
    assert.notEqual(scopesAt(result, line, 'Second').at(-1), 'entity.name.type.serializer', literal);
    assert.equal(scopesAt(result, line, 'following').at(-1), 'entity.name.type.serializer', literal);
    assert.equal(result.ruleStack.depth, 1, literal);
  }
});

test('split access and file prefixes retain native bodies while split fields retain defaults', async () => {
  const grammar = await loadGrammar();
  for (const [tag, suffix] of nativeLanguages) {
    const headers = [[tag, 'preamble'], [`${tag} /* header`, 'comment */ preamble']];
    if (tag !== 'c' && tag !== 'go') {
      headers.push(['public', tag], ['public /* header', `comment */ ${tag}`],
        ['public // header', tag]);
    }
    for (const header of headers) {
      let state = INITIAL;
      for (const line of header) { state = grammar.tokenizeLine(line, state).ruleStack; }
      const line = '{ return 4; } class following {}';
      const result = grammar.tokenizeLine(line, state);
      assert.equal(scopesAt(result, line, 'return').at(-1), `keyword.control.imported.${suffix}`, header.join('\n'));
      assert.equal(scopesAt(result, line, 'following').at(-1), 'entity.name.type.serializer');
      assert.equal(result.ruleStack.depth, 1);
    }
    for (const header of [['public uint32', tag], ['public', 'uint32', tag],
      ['public uint32 /* field', `comment */ ${tag}`], [`public optional<lib::value>`, tag]]) {
      let state = INITIAL;
      for (const line of header) { state = grammar.tokenizeLine(line, state).ruleStack; }
      const result = grammar.tokenizeLine('{3}; class following {}', state);
      assert.ok(!result.tokens.some(token => token.scopes.includes(`meta.embedded.block.${tag}`)), header.join('\n'));
      assert.equal(scopesAt(result, '{3}; class following {}', 'following').at(-1), 'entity.name.type.serializer');
      assert.equal(result.ruleStack.depth, 1);
    }
  }
});
