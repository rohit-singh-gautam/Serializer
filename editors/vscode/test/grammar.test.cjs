const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { test } = require('node:test');
const { Registry, INITIAL } = require('vscode-textmate');
const onig = require('vscode-oniguruma');

test('language 1.15 fresh record defaults preserve constructor and factory boundaries', async () => {
  const grammar = await loadGrammar();
  const lines = [
    'serializer version 1.15.0; class holder {',
    'public constructor(lib::area area = {},',
    '  lib::frame frame = {{}, 7}, lib::area tail = {10, 20}) { value = area; }',
    'public factory make(lib::area area = { /* fresh record */ }, lib::frame frame = {{}, 7}) { value = area; }',
    'public lib::area after; }',
  ];
  let state = INITIAL;
  const tokens = lines.map(line => {
    const result = grammar.tokenizeLine(line, state);
    state = result.ruleStack;
    return result.tokens;
  });
  /** Inspect a defaulted signature token without consuming the following declaration. */
  const scopes = (line, word) => {
    const offset = lines[line].indexOf(word);
    return tokens[line].find(token => token.startIndex <= offset && token.endIndex > offset).scopes;
  };
  assert.equal(scopes(0, '1.15.0').at(-1), 'constant.numeric.serializer');
  assert.equal(scopes(1, 'constructor').at(-1), 'storage.type.serializer');
  assert.equal(scopes(3, 'make').at(-1), 'entity.name.function.serializer');
  for (const line of [1, 2, 3, 4]) {
    for (const match of lines[line].matchAll(/(?:area|frame)(?= (?:area|frame|tail|after))/g)) {
      assert.equal(tokens[line].find(token => token.startIndex <= match.index && token.endIndex > match.index)
        .scopes.at(-1), 'entity.name.type.serializer');
    }
  }
  assert.equal(scopes(2, '7').at(-1), 'constant.numeric.serializer');
  assert.equal(scopes(2, '10').at(-1), 'constant.numeric.serializer');
  assert.ok(scopes(3, 'fresh record').some(scope => /^comment\./.test(scope)));
  assert.equal(state.depth, 1, 'empty and nested defaults close before the next class declaration');
  const snippets = JSON.parse(fs.readFileSync(path.join(__dirname, '../snippets/serializer.json'), 'utf8'));
  assert.equal(snippets.Schema.body[0], 'serializer version 1.15.0;');
});

test('language 1.14 exposure, operator mappings and callable additions preserve schema boundaries', async () => {
  const grammar = await loadGrammar();
  const lines = [
    'serializer version 1.14.0; class holder<T> {',
    'private lib::value<T> stored (1) expose readonly(public) as read_value;',
    'public cpp { constexpr inline } constructor(lib::value<T> input);',
    'public constructor() = default;',
    'public cpp { constexpr } function lib::value<T> result() readonly;',
    'arithmetic(unary(-) -> negated);',
    'arithmetic(binary(+, lib::value<T> rhs) -> expression(lib::value<T>(checked<T>(rhs.number))));',
    'public lib::frame placement (2) {{0, 0, 1}, 7};',
    'public uint32 expose; public uint32 arithmetic (3); public uint32 default; }',
  ];
  let state = INITIAL;
  const tokens = lines.map(line => {
    const result = grammar.tokenizeLine(line, state);
    state = result.ruleStack;
    return result.tokens;
  });
  /** Inspect a new contract token at its exact source position. */
  const scopes = (line, word) => {
    const offset = lines[line].indexOf(word);
    return tokens[line].find(token => token.startIndex <= offset && token.endIndex > offset).scopes;
  };
  assert.equal(scopes(1, 'expose').at(-1), 'storage.modifier.serializer');
  assert.equal(scopes(1, 'read_value').at(-1), 'entity.name.function.serializer');
  assert.equal(scopes(1, 'readonly').at(-1), 'storage.modifier.serializer');
  assert.ok(scopes(2, 'constexpr').includes('meta.embedded.block.cpp'));
  assert.equal(scopes(2, 'constructor').at(-1), 'storage.type.serializer');
  assert.equal(scopes(2, 'value<T>').at(-1), 'entity.name.type.serializer');
  assert.equal(scopes(3, 'default').at(-1), 'keyword.control.serializer');
  assert.equal(scopes(4, 'result').at(-1), 'entity.name.function.serializer');
  assert.equal(scopes(5, 'arithmetic').at(-1), 'storage.modifier.serializer');
  assert.equal(scopes(5, 'unary').at(-1), 'storage.modifier.serializer');
  assert.equal(scopes(5, 'negated').at(-1), 'entity.name.function.serializer');
  assert.equal(scopes(6, 'binary').at(-1), 'storage.modifier.serializer');
  assert.equal(scopes(6, 'expression').at(-1), 'keyword.control.serializer');
  for (const match of lines[6].matchAll(/value/g)) {
    assert.equal(tokens[6].find(token => token.startIndex <= match.index && token.endIndex > match.index).scopes.at(-1),
      'entity.name.type.serializer');
  }
  assert.equal(state.depth, 1);
  for (const word of ['expose;', 'arithmetic (', 'default;']) {
    assert.equal(scopes(8, word).at(-1), 'source.serializer');
  }
  const header = 'class holder<T> arithmetic(unary(-) -> expression(lib::value<T>(-number))) { public uint32 after; }';
  const result = grammar.tokenizeLine(header, INITIAL);
  assert.equal(result.ruleStack.depth, 1, 'header operator expression closes before class body');
  assert.equal(result.tokens.find(token => token.startIndex <= header.indexOf('uint32') &&
    token.endIndex > header.indexOf('uint32')).scopes.at(-1), 'support.type.serializer');
  const split = [
    'private lib::value<T> stored expose /* declaration */',
    'readonly(public)',
    'as /* explicit name */',
    'read_value;',
    'public uint32 after;',
  ];
  let splitState = INITIAL;
  const splitTokens = split.map(line => {
    const parsed = grammar.tokenizeLine(line, splitState);
    splitState = parsed.ruleStack;
    return parsed.tokens;
  });
  assert.equal(splitTokens[3].find(token => token.startIndex === 0).scopes.at(-1),
    'entity.name.function.serializer');
  assert.equal(splitTokens[4].find(token => token.startIndex <= split[4].indexOf('uint32') &&
    token.endIndex > split[4].indexOf('uint32')).scopes.at(-1), 'support.type.serializer');
  assert.equal(splitState.depth, 1);
});

test('language 1.13 C++ exception contracts preserve qualifier and native body boundaries', async () => {
  const grammar = await loadGrammar();
  for (const suffix of ['noexcept(cpp);', 'noexcept ( cpp );',
    'constexpr(cpp) noexcept(cpp) cpp { return 1; };',
    'noexcept(cpp) constexpr(cpp) cpp { return 1; };']) {
    const line = `class holder { public function bool complete() readonly ${suffix} public uint32 after; }`;
    const result = grammar.tokenizeLine(line, INITIAL);
    /** Inspect schema contract tokens without treating the qualifier as a native C++ body. */
    const scopes = word => {
      const offset = line.indexOf(word);
      return result.tokens.find(token => token.startIndex <= offset && token.endIndex > offset).scopes;
    };
    assert.equal(scopes('complete').at(-1), 'entity.name.function.serializer');
    assert.equal(scopes('readonly').at(-1), 'storage.modifier.serializer');
    assert.equal(scopes('noexcept').at(-1), 'storage.modifier.serializer');
    assert.equal(scopes('cpp').at(-1), 'storage.modifier.language.serializer');
    assert.ok(!scopes('noexcept').includes('meta.embedded.block.cpp'));
    if (line.includes('constexpr')) {
      assert.equal(scopes('constexpr').at(-1), 'storage.modifier.serializer');
      assert.ok(scopes('return').includes('meta.embedded.block.cpp'));
    }
    assert.equal(scopes('uint32').at(-1), 'support.type.serializer');
    assert.equal(result.ruleStack.depth, 1, suffix);
  }
  const field = 'class holder { public uint32 noexcept; }';
  const result = grammar.tokenizeLine(field, INITIAL);
  const offset = field.indexOf('noexcept');
  assert.equal(result.tokens.find(token => token.startIndex <= offset && token.endIndex > offset).scopes.at(-1),
    'source.serializer');
  const snippets = JSON.parse(fs.readFileSync(path.join(__dirname, '../snippets/serializer.json'), 'utf8'));
  assert.equal(snippets['Nonthrowing readonly C++ declaration'].prefix, 'readonly-noexcept-cpp');
  assert.match(snippets['Nonthrowing readonly C++ declaration'].body, /readonly noexcept\(cpp\);/);
});

test('language 1.12 construction contracts and generic readonly methods retain scopes', async () => {
  const grammar = await loadGrammar();
  const lines = [
    'serializer version 1.12.0; class holder<T, Frame: factory(identity) = lib::frame> {',
    'public cpp storage(none) { constexpr int native_value() const { return 1; } }',
    'public constructor(T input, Frame placement = default(identity)) { value = input;',
    '  validate expression(input > 0); validate cpp { check_value(); }; initialize cpp { if (true) { register_value(); } }; }',
    'public constructor<Input: integral || floating_point>(lib::number<Input> input) explicit { value = checked<T>(input.value); }',
    'public function common_type<T, Input> scaled<Input: integral || floating_point>(Input factor) readonly expression(checked<common_type<T, Input>>(value) * factor);',
    'public function T native_value() readonly constexpr(cpp) cpp { return value; };',
    'public uint32 validate; public uint32 initialize; public uint32 storage; }',
  ];
  let state = INITIAL;
  const tokens = lines.map(line => {
    const result = grammar.tokenizeLine(line, state);
    state = result.ruleStack;
    return result.tokens;
  });
  /** Inspect a contract token without accepting an earlier same-spelling native token. */
  const scopes = (line, word) => {
    const offset = lines[line].indexOf(word);
    return tokens[line].find(token => token.startIndex <= offset && token.endIndex > offset).scopes;
  };
  assert.equal(scopes(0, 'factory(').at(-1), 'storage.modifier.serializer');
  assert.equal(scopes(0, 'identity)').at(-1), 'entity.name.function.serializer');
  assert.equal(scopes(1, 'storage(').at(-1), 'storage.modifier.serializer');
  assert.equal(scopes(1, 'none)').at(-1), 'constant.other.layout.serializer');
  assert.ok(scopes(1, 'return').includes('meta.embedded.block.cpp'));
  assert.equal(scopes(2, 'default(').at(-1), 'keyword.control.serializer');
  assert.equal(scopes(3, 'validate').at(-1), 'keyword.control.serializer');
  assert.equal(scopes(3, 'initialize').at(-1), 'keyword.control.serializer');
  assert.ok(scopes(3, 'check_value').includes('meta.embedded.block.cpp'));
  assert.ok(scopes(3, 'register_value').includes('meta.embedded.block.cpp'));
  assert.equal(scopes(4, 'constructor').at(-1), 'storage.type.serializer');
  assert.equal(scopes(5, 'common_type<').at(-1), 'support.type.serializer');
  assert.equal(scopes(5, 'scaled<').at(-1), 'entity.name.function.serializer');
  assert.equal(scopes(5, 'integral').at(-1), 'storage.modifier.serializer');
  assert.equal(scopes(6, 'constexpr(').at(-1), 'storage.modifier.serializer');
  assert.ok(scopes(6, 'return').includes('meta.embedded.block.cpp'));
  for (const word of ['validate;', 'initialize;', 'storage;']) {
    assert.equal(scopes(7, word).at(-1), 'source.serializer');
  }
  assert.equal(state.depth, 1);
});

test('language 1.11 checked construction and record selection retain exact type scopes', async () => {
  const grammar = await loadGrammar();
  const line = 'serializer version 1.11.0; class values<T> { ' +
    'public constructor<Input: (integral || floating_point) && !bool>(Input input, uint32 count = 3) explicit { value = checked<T>(input); } ' +
    'public map(lib::key<T>) lib::record entries; ' +
    'public function lib::record selected() readonly expression(first.valid() ? first : second); ' +
    'public function uint64 converted() readonly expression(checked<uint64>(value)); }';
  const result = grammar.tokenizeLine(line, INITIAL);
  const scope = offset => result.tokens.find(token => token.startIndex <= offset && token.endIndex > offset).scopes.at(-1);
  for (const match of line.matchAll(/checked/g)) { assert.equal(scope(match.index), 'support.function.serializer'); }
  assert.equal(scope(line.indexOf('constructor')), 'storage.type.serializer');
  assert.equal(scope(line.indexOf('explicit')), 'storage.modifier.serializer');
  assert.equal(scope(line.indexOf('key<T>')), 'entity.name.type.serializer');
  assert.equal(scope(line.indexOf('lib::record entries') + 5), 'entity.name.type.serializer');
  assert.equal(scope(line.indexOf('valid()')), 'entity.name.function.serializer');
  assert.equal(scope(line.indexOf('checked<T>') + 'checked<'.length), 'entity.name.type.serializer');
  assert.equal(result.ruleStack.depth, 1);
});

test('language 1.10 namespace calls and Boolean type constraints have distinct scopes', async () => {
  const grammar = await loadGrammar();
  const line = 'serializer version 1.10.0; namespace math_values { ' +
    'function double total(double left, double right) readonly expression(left + right); ' +
    'class number<T: (integral || floating_point) && !bool> { public T value; ' +
    'public function double added(double other) readonly expression(math_values::total(value, other)); } }';
  const result = grammar.tokenizeLine(line, INITIAL);
  const scope = word => result.tokens.find(token => token.startIndex <= line.indexOf(word) &&
    token.endIndex > line.indexOf(word)).scopes.at(-1);
  assert.equal(scope('total('), 'entity.name.function.serializer');
  assert.equal(scope('math_values::total'), 'entity.name.function.serializer');
  for (const word of ['integral', 'floating_point']) { assert.equal(scope(word), 'storage.modifier.serializer'); }
  for (const word of ['||', '&&', '!bool']) { assert.equal(scope(word), 'keyword.operator.serializer'); }
  assert.equal(scope('bool>'), 'entity.name.type.serializer');
  assert.equal(result.ruleStack.depth, 1);
});

test('optional and nested container generics highlight types without consuming fields or method names', async () => {
  const grammar = await loadGrammar();
  const line = 'serializer version 1.7.0; class box<T = optional<lib::person>> { ' +
    'public optional<optional<lib::person>> value; public optional<array<T>> rows; ' +
    'public optional<map<string, lib::person>> lookup; public array optional<T> items; ' +
    'public function optional<lib::person> find(optional<lib::person> input) cpp { return input; }; }';
  const result = grammar.tokenizeLine(line, INITIAL);
  /** Read a token scope at its source offset. */
  const scope = offset => result.tokens.find(token => token.startIndex <= offset && token.endIndex > offset).scopes.at(-1);
  for (const match of line.matchAll(/person/g)) {
    assert.equal(scope(match.index), 'entity.name.type.serializer');
  }
  for (const match of line.matchAll(/optional/g)) {
    assert.equal(scope(match.index), 'entity.name.type.serializer');
  }
  assert.equal(scope(line.indexOf('find(')), 'entity.name.function.serializer');
  for (const word of ['value;', 'rows;', 'lookup;', 'items;', 'input)']) {
    assert.equal(scope(line.indexOf(word)), 'source.serializer', word);
  }
  assert.equal(result.ruleStack.depth, 1);
  const snippets = JSON.parse(fs.readFileSync(path.join(__dirname, '../snippets/serializer.json'), 'utf8'));
  assert.equal(snippets.Schema.body[0], 'serializer version 1.15.0;');
  assert.equal(snippets['Optional field'].prefix, 'optional');
  assert.match(snippets['Optional field'].body, /optional</);
});

test('language 1.8 construction, layout, generic bases and checked math retain distinct scopes', async () => {
  const grammar = await loadGrammar();
  const line = 'class derived<T> : public lib::base<T> { ' +
    'public constructor(lib::point input) { value = input; } ' +
    'public factory origin() { value = default(zero); } ' +
    'public transient layout(empty) lib::empty reference; ' +
    'public function bool valid(double x) readonly expression(x >= 0.0 && math.sqrt(x) / 2.0 < math.pi ? true : false); }';
  const result = grammar.tokenizeLine(line, INITIAL);
  const scope = word => result.tokens.find(token => token.startIndex <= line.indexOf(word) &&
    token.endIndex > line.indexOf(word)).scopes.at(-1);
  assert.equal(scope('constructor'), 'storage.type.serializer');
  assert.equal(scope('factory'), 'storage.type.serializer');
  assert.equal(scope('origin'), 'entity.name.function.serializer');
  assert.equal(scope('zero'), 'entity.name.function.serializer');
  assert.equal(scope('empty)'), 'constant.other.layout.serializer');
  assert.equal(scope('sqrt'), 'support.function.serializer');
  for (const word of ['>=', '&&', '/', '< math', '?']) { assert.equal(scope(word), 'keyword.operator.serializer'); }
  for (const word of ['base', 'point', 'empty reference']) { assert.equal(scope(word), 'entity.name.type.serializer'); }
  assert.equal(result.ruleStack.depth, 1);
});

test('new metadata keywords preserve ordinary field IDs and qualified stateless defaults', async () => {
  const grammar = await loadGrammar();
  const line = 'class sample<T = lib::stateless> { public uint32 auto_size(1); ' +
    'public uint32 all_except(2); public uint32 size_scope(3); }';
  const result = grammar.tokenizeLine(line, INITIAL);
  const scope = word => result.tokens.find(token => token.startIndex <= line.indexOf(word) &&
    token.endIndex > line.indexOf(word)).scopes.at(-1);
  for (const word of ['auto_size', 'all_except', 'size_scope']) {
    assert.equal(scope(word), 'source.serializer');
  }
  assert.equal(scope('stateless'), 'entity.name.type.serializer');
  assert.equal(result.ruleStack.depth, 1);
});

test('language 1.9 constraints, size metadata and conversions have contextual scopes', async () => {
  const grammar = await loadGrammar();
  const line = 'class passive<T: stateless = lib::empty> { public transient layout(empty) T reference; ' +
    'public wire_only uint32 encoded_size(1) auto_size(binary_none, binary_integer) ' +
    'size_scope(payload) omit(all_except(binary_none, binary_integer)); ' +
    'public conversion lib::view as to_view { address = contact.street; }; ' +
    'public uint32 stateless; public uint32 all_except; }';
  const result = grammar.tokenizeLine(line, INITIAL);
  const scope = word => result.tokens.find(token => token.startIndex <= line.indexOf(word) &&
    token.endIndex > line.indexOf(word)).scopes.at(-1);
  assert.equal(scope('stateless ='), 'storage.modifier.serializer');
  assert.equal(scope('wire_only'), 'storage.modifier.serializer');
  assert.equal(scope('auto_size'), 'storage.modifier.serializer');
  assert.equal(scope('binary_none,'), 'constant.other.format.serializer');
  assert.equal(scope('payload)'), 'constant.other.layout.serializer');
  assert.equal(scope('all_except('), 'keyword.control.serializer');
  assert.equal(scope('conversion'), 'storage.type.serializer');
  assert.equal(scope('view as'), 'entity.name.type.serializer');
  assert.equal(scope('to_view'), 'entity.name.function.serializer');
  assert.equal(scope('stateless;'), 'source.serializer');
  assert.equal(scope('all_except;'), 'source.serializer');
  assert.equal(result.ruleStack.depth, 1);
});

test('digest returns and bounded typed predicates retain declaration and operator scopes', async () => {
  const grammar = await loadGrammar();
  const line = 'class reading { public function digest(sha256) fingerprint() readonly cpp { return {}; }; ' +
    'public function bool usable() readonly predicate(size(samples) <= 8 && ' +
    'all(samples, 8, item => math.is_finite(item.value)) && state == lib::phase::ready && samples[0].enabled); }';
  const result = grammar.tokenizeLine(line, INITIAL);
  const scope = word => result.tokens.find(token => token.startIndex <= line.indexOf(word) &&
    token.endIndex > line.indexOf(word)).scopes.at(-1);
  assert.equal(scope('digest('), 'support.type.serializer');
  assert.equal(scope('sha256'), 'constant.other.algorithm.serializer');
  assert.equal(scope('fingerprint'), 'entity.name.function.serializer');
  assert.equal(scope('predicate'), 'keyword.control.serializer');
  for (const word of ['size', 'all(samples', 'is_finite']) { assert.equal(scope(word), 'support.function.serializer'); }
  assert.equal(scope('=>'), 'keyword.operator.serializer');
  assert.equal(scope('[0]'), 'punctuation.section.group.serializer');
  assert.equal(scope('lib::phase'), 'entity.name.type.serializer');
  assert.equal(result.ruleStack.depth, 1);
});

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
    loadGrammar: async scope => scope === 'source.serializer' ?
      JSON.parse(fs.readFileSync(path.join(__dirname, '../../serializer.tmLanguage.json'), 'utf8')) : null
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

// Native declarations must not inherit Serializer's schema type classifications.
test('language 1.6 methods and native braces retain the outer schema state', async () => {
  const grammar = await loadGrammar();
  const line = 'class document { public function double estimate(double rate) readonly expression(rate * 2); public transient uint64 cache {0}; public cpp { if (true) { class NativeOnly {}; } } public uint32 pages (1); }';
  const result = grammar.tokenizeLine(line, INITIAL);
  const scope = word => result.tokens.find(token => token.startIndex <= line.indexOf(word) &&
    token.endIndex > line.indexOf(word)).scopes.at(-1);
  assert.equal(scope('estimate'), 'entity.name.function.serializer');
  assert.equal(scope('transient'), 'storage.modifier.serializer');
  assert.equal(scope('cpp'), 'storage.modifier.language.serializer');
  assert.notEqual(scope('NativeOnly'), 'entity.name.type.serializer');
  assert.equal(result.ruleStack.depth, 1);
});
