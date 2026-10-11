const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { Registry, INITIAL, parseRawGrammar } = require('../../vscode/node_modules/vscode-textmate');
const onig = require('../../vscode/node_modules/vscode-oniguruma');

const serializerGrammar = path.resolve(__dirname, '../../serializer.tmLanguage.json');
const nativeLanguages = ['cpp', 'csharp', 'java', 'javascript', 'typescript', 'go',
  'rust', 'python', 'swift', 'kotlin', 'c'];

/** Read installed Visual Studio grammar assets without modifying the installation. */
function installedGrammars(directory) {
  const grammars = new Map();
  const pending = [directory];
  while (pending.length !== 0) {
    const current = pending.pop();
    for (const entry of fs.readdirSync(current, { withFileTypes: true })) {
      const file = path.join(current, entry.name);
      if (entry.isDirectory()) { pending.push(file); continue; }
      if (!entry.isFile() || !/\.(?:json|plist|tmlanguage)$/i.test(entry.name)) { continue; }
      // Themes, snippets and language configuration files also use these suffixes.
      try {
        const grammar = parseRawGrammar(fs.readFileSync(file, 'utf8').replace(/^\uFEFF/, ''), file);
        if (grammar.scopeName && grammar.patterns) { grammars.set(grammar.scopeName, grammar); }
      } catch (error) {
        if (/[/\\](?:syntaxes|grammars)[/\\]/i.test(file) && !/configuration/i.test(file)) {
          throw new Error(`Cannot parse installed grammar ${file}: ${error.message}`);
        }
      }
    }
  }
  return grammars;
}

/** Load the canonical Serializer grammar with either real host grammars or none. */
async function loadGrammar(nativeGrammars) {
  const registry = new Registry({
    onigLib: Promise.resolve({
      createOnigScanner: patterns => new onig.OnigScanner(patterns),
      createOnigString: text => new onig.OnigString(text),
    }),
    loadGrammar: async scope => scope === 'source.serializer'
      ? parseRawGrammar(fs.readFileSync(serializerGrammar, 'utf8'), serializerGrammar)
      : nativeGrammars.get(scope) || null,
  });
  return registry.loadGrammar('source.serializer');
}

/** Tokenize each line while retaining multiline native strings and block state. */
function tokenize(grammar, lines) {
  let state = INITIAL;
  const tokens = lines.map(line => {
    const result = grammar.tokenizeLine(line, state);
    state = result.ruleStack;
    return result.tokens;
  });
  return { tokens, state };
}

/** Read the token scopes containing a checked word on a specified line. */
function scopesAt(lines, result, lineIndex, word) {
  const offset = lines[lineIndex].indexOf(word);
  assert.ok(offset >= 0, `Missing fixture word ${word}`);
  const token = result.tokens[lineIndex].find(item =>
    item.startIndex <= offset && item.endIndex > offset);
  assert.ok(token, `Missing token at ${word}`);
  return token.scopes;
}

/** Ensure every tag uses native syntax while returning to schema syntax at its boundary. */
function checkLanguages(grammar, label, requireNativeProviders = false) {
  let checks = 0;
  for (const language of nativeLanguages) {
    // C and Go allow file preambles, but their generators reject class hooks.
    const contexts = language === 'c' || language === 'go' ? ['preamble'] : ['preamble', 'class'];
    for (const context of contexts) {
      const prefix = context === 'preamble' ? `${language} preamble {` : `class outer { public ${language} {`;
      const suffix = context === 'preamble' ? '} class outer { public uint32 after_native (1); }'
        : '} public uint32 after_native (1); }';
      const lines = [
        prefix,
        '  if (true) { return "a } escaped \\" brace"; }',
        `  ${language === 'python' ? '#' : '//'} } native_comment {`,
        suffix,
        'class next_schema {}',
      ];
      const result = tokenize(grammar, lines);
      const keyword = scopesAt(lines, result, 1, 'return');
      const fixture = `${label}/${language}/${context}`;
      assert.ok(keyword.includes(`meta.embedded.block.${language}`),
        `${fixture}: missing embedded language scope`);
      assert.ok(keyword.some(scope => /^keyword\./.test(scope)),
        `${fixture}: return must use native keyword coloring (${keyword.join(' ')})`);
      if (requireNativeProviders && (language === 'cpp' || language === 'java')) {
        assert.ok(keyword.some(scope => /^keyword\./.test(scope) && scope.endsWith(`.${language}`)),
          `${fixture}: installed native grammar was not used (${keyword.join(' ')})`);
        ++checks;
      }
      const string = scopesAt(lines, result, 1, 'a } escaped');
      assert.ok(string.some(scope => /^string\./.test(scope)), `${fixture}: string coloring`);
      const comment = scopesAt(lines, result, 2, 'native_comment');
      assert.ok(comment.some(scope => /^comment\./.test(scope)), `${fixture}: comment coloring`);
      assert.ok(scopesAt(lines, result, 3, 'uint32').includes('support.type.serializer'),
        `${fixture}: native body consumed the following schema field`);
      assert.ok(scopesAt(lines, result, 4, 'next_schema').includes('entity.name.type.serializer'),
        `${fixture}: schema declaration state was lost`);
      assert.equal(result.state.depth, 1, `${fixture}: unclosed grammar rule`);
      checks += 6;
    }
  }
  return checks;
}

/** Keep language-specific raw and multiline literals from terminating the schema wrapper. */
function checkLiterals(grammar, label) {
  const fixtures = [
    ['cpp', ['  auto text = R"tag(} " { //)tag"; return 1;'], 'tag(}'],
    ['java', ['  String text = """', '    } native_text {', '    """; return 1;'], 'native_text'],
    ['csharp', ['  var text = @"} "" native_text {"; return 1;'], 'native_text'],
    ['javascript', ['  const text = `} native_text ${ { value: "}" } }`; return 1;'], 'native_text'],
    ['typescript', ['  const text = `} native_text ${ { value: "}" } }`; return 1;'], 'native_text'],
    ['rust', ['  let text = r##"} " native_text {"##; return 1;'], 'native_text'],
    ['python', ['  text = """', '    } native_text {', '    """', '  return 1'], 'native_text'],
    ['kotlin', ['  val text = """', '    } native_text {', '    """; return 1'], 'native_text'],
  ];
  let checks = 0;
  for (const [language, body, word] of fixtures) {
    const lines = [`${language} preamble {`, ...body,
      '} class outer { public uint32 after_native (1); }', 'class next_schema {}'];
    const result = tokenize(grammar, lines);
    const literalLine = lines.findIndex(line => line.includes(word));
    assert.ok(scopesAt(lines, result, literalLine, word).some(scope => /^string\./.test(scope)),
      `${label}/${language}: native literal coloring`);
    assert.ok(scopesAt(lines, result, lines.length - 2, 'uint32').includes('support.type.serializer'),
      `${label}/${language}: raw/multiline literal lost the native closing brace`);
    assert.ok(scopesAt(lines, result, lines.length - 1, 'next_schema').includes('entity.name.type.serializer'),
      `${label}/${language}: raw/multiline literal consumed the following declaration`);
    assert.equal(result.state.depth, 1, `${label}/${language}: literal grammar did not close`);
    checks += 4;
  }
  return checks;
}

/** Keep new construction contracts around native blocks in both host and fallback grammars. */
function checkConstructionContracts(grammar, label) {
  const lines = [
    'class holder<T, Frame: factory(identity)> {',
    'public cpp storage(none) { constexpr int native_value() const { return 1; } }',
    'public constructor(T input, Frame placement = default(identity)) { value = input;',
    'validate expression(input > 0); validate cpp { check_value(); }; initialize cpp { if (true) { register_value(); } }; }',
    'public function common_type<T, Input> scaled<Input: integral || floating_point>(Input factor) readonly expression(checked<common_type<T, Input>>(value) * factor);',
    'public function T native_value() readonly constexpr(cpp) noexcept(cpp) cpp { return value; };',
    'public function bool complete() readonly noexcept(cpp);',
    'public function T reversed() readonly noexcept(cpp) constexpr(cpp) cpp { return value; };',
    'public uint32 after_native; }',
  ];
  const result = tokenize(grammar, lines);
  const expected = [[0, 'factory(', 'storage.modifier.serializer'],
    [1, 'storage(', 'storage.modifier.serializer'],
    [1, 'return', 'meta.embedded.block.cpp'],
    [3, 'validate', 'keyword.control.serializer'],
    [3, 'initialize', 'keyword.control.serializer'],
    [3, 'check_value', 'meta.embedded.block.cpp'],
    [3, 'register_value', 'meta.embedded.block.cpp'],
    [4, 'common_type<', 'support.type.serializer'],
    [4, 'scaled<', 'entity.name.function.serializer'],
    [5, 'constexpr(', 'storage.modifier.serializer'],
    [5, 'noexcept(', 'storage.modifier.serializer'],
    [5, 'return', 'meta.embedded.block.cpp'],
    [6, 'noexcept(', 'storage.modifier.serializer'],
    [6, 'cpp)', 'storage.modifier.language.serializer'],
    [7, 'noexcept(', 'storage.modifier.serializer'],
    [7, 'constexpr(', 'storage.modifier.serializer'],
    [7, 'return', 'meta.embedded.block.cpp'],
    [8, 'uint32', 'support.type.serializer']];
  for (const [line, word, scope] of expected) {
    assert.ok(scopesAt(lines, result, line, word).includes(scope), `${label}: ${word} lacks ${scope}`);
  }
  assert.equal(result.state.depth, 1, `${label}: construction contracts left an open grammar rule`);
  return expected.length + 1;
}

/** Exercise real Visual Studio grammar assets and missing-provider fallback separately. */
async function main() {
  const directory = process.argv[2];
  assert.ok(directory && fs.statSync(directory).isDirectory(), 'Pass the Visual Studio TextMate directory.');
  const grammars = installedGrammars(directory);
  assert.ok(grammars.has('source.cpp'), 'Visual Studio C++ grammar is missing.');
  assert.ok(grammars.has('source.java'), 'Visual Studio Java grammar is missing.');
  const wasmPath = require.resolve('../../vscode/node_modules/vscode-oniguruma/release/onig.wasm');
  await onig.loadWASM(fs.readFileSync(wasmPath));
  const host = await loadGrammar(grammars);
  const fallback = await loadGrammar(new Map());
  const checks = checkLanguages(host, 'installed host grammar', true) +
    checkLanguages(fallback, 'missing grammar fallback') +
    checkLiterals(host, 'installed host grammar') +
    checkLiterals(fallback, 'missing grammar fallback') +
    checkConstructionContracts(host, 'installed host grammar') +
    checkConstructionContracts(fallback, 'missing grammar fallback');
  console.log(`Installed grammar assets passed ${checks} embedded syntax/boundary checks across all ${nativeLanguages.length} tags.`);
  console.log(`Loaded ${grammars.size} installed grammar scopes; this validates assets with the TextMate tokenizer, not an editor-host UI run.`);
}

main().catch(error => { console.error(error); process.exitCode = 1; });
