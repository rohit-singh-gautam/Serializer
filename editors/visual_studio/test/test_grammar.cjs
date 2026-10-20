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

/** Qualify new field/API contracts with both the installed C++ grammar and native fallback. */
function checkAuthoringContracts(grammar, label) {
  const lines = [
    'class holder<T> arithmetic(unary(-) -> expression(lib::value<T>(-number))) {',
    'private lib::value<T> stored expose readonly(public) as read_value;',
    'public cpp { constexpr inline } constructor(lib::value<T> input);',
    'public constructor() = default;',
    'public cpp { constexpr } function lib::value<T> result() readonly;',
    'arithmetic(binary(+, lib::value<T> rhs) -> expression(lib::value<T>(rhs.number)));',
    'public lib::frame nested_default {{0, 0, 1}, 7}; public uint32 after; }',
  ];
  const result = tokenize(grammar, lines);
  const expected = [[0, 'arithmetic', 'storage.modifier.serializer'],
    [0, 'expression', 'keyword.control.serializer'],
    [1, 'expose', 'storage.modifier.serializer'],
    [1, 'read_value', 'entity.name.function.serializer'],
    [2, 'constexpr', 'meta.embedded.block.cpp'],
    [2, 'constructor', 'storage.type.serializer'],
    [3, 'default', 'keyword.control.serializer'],
    [4, 'result', 'entity.name.function.serializer'],
    [5, 'binary', 'storage.modifier.serializer'],
    [5, 'value<T>', 'entity.name.type.serializer'],
    [6, 'uint32', 'support.type.serializer']];
  for (const [line, word, scope] of expected) {
    assert.ok(scopesAt(lines, result, line, word).includes(scope), `${label}: ${word} lacks ${scope}`);
  }
  assert.equal(result.state.depth, 1, `${label}: authoring contracts consumed the following declaration`);
  return expected.length + 1;
}

/** Qualify empty constructor/factory record defaults against installed and fallback grammars. */
function checkFreshRecordDefaults(grammar, label) {
  const lines = [
    'serializer version 1.15.0; class holder {',
    'public constructor(lib::area area = {},',
    '  lib::frame frame = {{}, 7}, lib::area tail = {10, 20}) { value = area; }',
    'public factory make(lib::area area = { /* fresh record */ }, lib::frame frame = {{}, 7}) { value = area; }',
    'public lib::area after; }',
  ];
  const result = tokenize(grammar, lines);
  const expected = [[0, '1.15.0', 'constant.numeric.serializer'],
    [1, 'constructor', 'storage.type.serializer'],
    [1, 'area area', 'entity.name.type.serializer'],
    [2, 'frame frame', 'entity.name.type.serializer'],
    [2, 'area tail', 'entity.name.type.serializer'],
    [3, 'make', 'entity.name.function.serializer'],
    [3, 'area area', 'entity.name.type.serializer'],
    [3, 'frame frame', 'entity.name.type.serializer'],
    [4, 'area after', 'entity.name.type.serializer']];
  for (const [line, word, scope] of expected) {
    assert.ok(scopesAt(lines, result, line, word).includes(scope), `${label}: ${word} lacks ${scope}`);
  }
  assert.equal(result.state.depth, 1, `${label}: fresh defaults consumed the following declaration`);
  return expected.length + 1;
}

/** Exercise real Visual Studio grammar assets and missing-provider fallback separately. */
/** Qualify array exposure and callable metadata against native and fallback tokenizers. */
function checkArrayCallableApis(grammar, label) {
  const lines = [
    'serializer version 1.17.0; class grid<uint64 Rows, uint64 Cols, T> {',
    'private array[Rows * Cols] T stored expose array(public, const_value) dimensions(Rows, Cols);',
    'private array[3] uint32 indices expose array(readonly, public);',
    'public function static expected<lib::value, lib::error> make(array[Rows] lib::value input const_ref) readonly cpp { return {}; };',
    'public constructor<Input>(Input input const_ref) explicit delegates(input.radius, input.height);',
    'public factory identity() when(Rows == Cols && same_type(T, double)) { stored = generate(row, column => checked<T>(row == column ? 1 : 0)); }',
    'public function array[3] lib::value copy() readonly cpp { return {}; };',
    'public function Vector apply_vector<Vector>(Vector input const_ref) readonly;',
    'public lib::value after; }',
  ];
  const result = tokenize(grammar, lines);
  const checks = [[1, 'expose', 'storage.modifier.serializer'],
    [1, 'array(public', 'storage.modifier.serializer'], [1, 'const_value', 'storage.modifier.serializer'],
    [1, 'dimensions', 'storage.modifier.serializer'], [1, 'Cols);', 'entity.name.type.serializer'],
    [2, 'readonly', 'storage.modifier.serializer'], [3, 'static', 'storage.modifier.serializer'],
    [3, 'expected', 'support.type.serializer'], [3, 'const_ref', 'storage.modifier.serializer'],
    [4, 'delegates', 'keyword.control.serializer'], [5, 'when', 'keyword.control.serializer'],
    [5, 'same_type', 'support.function.serializer'], [5, 'generate', 'keyword.control.serializer'],
    [6, 'array', 'storage.type.serializer'], [7, 'apply_vector', 'entity.name.function.serializer'],
    [7, 'const_ref', 'storage.modifier.serializer'], [8, 'value', 'entity.name.type.serializer']];
  for (const [line, word, expected] of checks) {
    assert.equal(scopesAt(lines, result, line, word).at(-1), expected, `${label}: ${word}`);
  }
  return checks.length;
}

/** Keep durable references and translator declarations bounded in every grammar host. */
function checkReferenceContracts(grammar, label) {
  const lines = ['serializer version 1.18.0;',
    'reference label_ref = borrowed(lib::owner.item.label);',
    'reference shared_ref = shared(lib::owner.item);',
    'reference weak_ref = weak(lib::owner.item.label);',
    'class holder { public label_ref d (1);',
    'public translator lib::target as to_target { name = source.item.label; };',
    'public lib::owner after (2); }'];
  const result = tokenize(grammar, lines);
  const expected = [[1, 'reference', 'storage.type.serializer'],
    [1, 'label_ref', 'entity.name.type.serializer'],
    [1, 'borrowed', 'storage.modifier.serializer'], [1, 'owner', 'entity.name.type.serializer'],
    [2, 'shared(', 'storage.modifier.serializer'], [2, 'owner', 'entity.name.type.serializer'],
    [3, 'weak(', 'storage.modifier.serializer'], [3, 'owner', 'entity.name.type.serializer'],
    [4, 'label_ref', 'entity.name.type.serializer'], [5, 'translator', 'storage.type.serializer'],
    [5, 'target', 'entity.name.type.serializer'], [5, 'to_target', 'entity.name.function.serializer'],
    [6, 'owner', 'entity.name.type.serializer']];
  for (const [line, word, scope] of expected) {
    assert.equal(scopesAt(lines, result, line, word).at(-1), scope, `${label}: ${word}`);
  }
  assert.equal(result.state.depth, 1, `${label}: reference contracts consumed later declarations`);
  return expected.length + 1;
}

/** Highlight inline value ownership without claiming its selected fields as types. */
function checkValueReferenceContracts(grammar, label) {
  const lines = ['serializer version 1.19.0; class export_view {',
    'public borrowed(lib::owner.item.label) first (1);',
    'public shared(lib::owner.item) second (2);',
    'public weak(lib::owner.item.label) third (3);',
    'public shared(lib::payload) value (4);',
    'public lib::owner after (5); }'];
  const result = tokenize(grammar, lines);
  for (const [line, mode] of [[1, 'borrowed'], [2, 'shared'], [3, 'weak']]) {
    assert.equal(scopesAt(lines, result, line, mode).at(-1), 'storage.modifier.serializer', label);
    assert.equal(scopesAt(lines, result, line, 'owner').at(-1), 'entity.name.type.serializer', label);
    assert.notEqual(scopesAt(lines, result, line, 'item').at(-1), 'entity.name.type.serializer', label);
  }
  assert.equal(scopesAt(lines, result, 4, 'shared').at(-1), 'storage.modifier.serializer', label);
  assert.equal(scopesAt(lines, result, 4, 'payload').at(-1), 'entity.name.type.serializer', label);
  assert.equal(scopesAt(lines, result, 5, 'owner').at(-1), 'entity.name.type.serializer', label);
  assert.equal(result.state.depth, 1, label);
  return 13;
}

/** Keep namespace/default/package metadata distinct from its referenced model type. */
function checkXmlAuthoring(grammar, label) {
  const lines = ['serializer version 1.20.0; namespace sample {',
    'using schema "schemas/core.xsd";',
    'xml(namespace("urn:core"), scalars(attribute), unknown(skip));',
    'class model { public double x (1) xml(attribute, name("axis")); }',
    'package archive { part main : sample::model {',
    'name("/model.xml"); content_type("application/xml"); relationship("urn:main"); compression(deflate);',
    '} part extras : bytes { name(dynamic); content_type(dynamic); relationship("urn:extra"); source(main); occurs(0,unbounded); } }',
    'class later { public sample::model value (1); } }'];
  const result = tokenize(grammar, lines);
  const expected = [[1, 'using', 'keyword.control.serializer'], [1, 'schema ', 'storage.type.serializer'],
    [1, 'schemas/core.xsd', 'string.quoted.double.serializer'], [2, 'xml', 'storage.modifier.serializer'],
    [2, 'namespace', 'storage.modifier.serializer'], [2, 'attribute', 'constant.other.xml.serializer'],
    [4, 'package', 'storage.type.serializer'], [4, 'part ', 'storage.type.serializer'],
    [4, 'model', 'entity.name.type.serializer'], [5, 'compression', 'storage.modifier.serializer'],
    [5, 'deflate', 'constant.other.package.serializer'], [6, 'bytes', 'support.type.serializer'],
    [7, 'model', 'entity.name.type.serializer']];
  for (const [line, word, scope] of expected) {
    assert.equal(scopesAt(lines, result, line, word).at(-1), scope, `${label}: ${word}`);
  }
  assert.equal(result.state.depth, 1, label);
  return expected.length + 1;
}

/** Keep managed target operands as types and stable identity metadata numeric. */
function checkManagedTokens(grammar, label) {
  const lines = ['serializer version 1.20.0; class owner managed class_id(18446744073709551615) {',
    'public managed_ref<lib::target> direct (1);',
    'public optional<managed_ref<lib::target>> maybe (2);',
    'public array managed_ref<lib::target> items (3); }'];
  const result = tokenize(grammar, lines);
  assert.equal(scopesAt(lines, result, 0, 'class_id').at(-1), 'storage.modifier.serializer', label);
  assert.equal(scopesAt(lines, result, 0, '18446744073709551615').at(-1), 'constant.numeric.serializer', label);
  for (const line of [1, 2, 3]) {
    assert.equal(scopesAt(lines, result, line, 'target').at(-1), 'entity.name.type.serializer', label);
    assert.equal(scopesAt(lines, result, line, 'managed_ref').at(-1), 'support.type.serializer', label);
  }
  assert.equal(result.state.depth, 1, label);
  return 9;
}

/** Keep new package envelope/reference metadata distinct from ordinary schema type positions. */
function checkPackageReferences(grammar, label) {
  const lines = ['serializer version 1.21.0; package archive { part main : model {',
    'policy(fixed); key("identities", id); reference("identities", target_id, targets);',
    'policy(defaults); } } class later { public model value; }'];
  const result = tokenize(grammar, lines);
  for (const word of ['policy', 'key', 'reference']) {
    assert.equal(scopesAt(lines, result, 1, word).at(-1), 'storage.modifier.serializer', label);
  }
  for (const [line, word] of [[1, 'fixed'], [2, 'defaults']]) {
    assert.equal(scopesAt(lines, result, line, word).at(-1), 'constant.other.package.serializer', label);
  }
  for (const word of ['id)', 'target_id', 'targets']) {
    assert.equal(scopesAt(lines, result, 1, word).at(-1), 'variable.other.member.serializer', label);
  }
  assert.equal(scopesAt(lines, result, 1, 'identities').at(-1), 'string.quoted.double.serializer', label);
  assert.equal(scopesAt(lines, result, 2, 'model').at(-1), 'entity.name.type.serializer', label);
  assert.equal(result.state.depth, 1, label);
  return 11;
}

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
    checkConstructionContracts(fallback, 'missing grammar fallback') +
    checkAuthoringContracts(host, 'installed host grammar') +
    checkAuthoringContracts(fallback, 'missing grammar fallback') +
    checkFreshRecordDefaults(host, 'installed host grammar') +
    checkFreshRecordDefaults(fallback, 'missing grammar fallback') +
    checkArrayCallableApis(host, 'installed host grammar') +
    checkArrayCallableApis(fallback, 'missing grammar fallback') +
    checkValueReferenceContracts(host, 'installed host grammar') +
    checkValueReferenceContracts(fallback, 'missing grammar fallback') +
    checkXmlAuthoring(host, 'installed host grammar') +
    checkXmlAuthoring(fallback, 'missing grammar fallback') +
    checkManagedTokens(host, 'installed host grammar') +
    checkManagedTokens(fallback, 'missing grammar fallback') +
    checkReferenceContracts(host, 'installed host grammar') +
    checkReferenceContracts(fallback, 'missing grammar fallback') +
    checkPackageReferences(host, 'installed host grammar') +
    checkPackageReferences(fallback, 'missing grammar fallback');
  console.log(`Installed grammar assets passed ${checks} embedded syntax/boundary checks across all ${nativeLanguages.length} tags.`);
  console.log(`Loaded ${grammars.size} installed grammar scopes; this validates assets with the TextMate tokenizer, not an editor-host UI run.`);
}

main().catch(error => { console.error(error); process.exitCode = 1; });
