const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { execFileSync } = require('node:child_process');
const { Navigator } = require('../out/navigator');
const { indexSource } = require('../out/navigation_model');
const { fileKey } = require('../out/model');

const repository = path.resolve(__dirname, '../../..');
const compiler = process.env.SERIALIZER_COMPILER;
const outputs = { cpp: 'model.hpp', java: 'Schema.java', js: 'schema.mjs', typescript: 'schema.d.mts',
  go: 'schema.go', csharp: 'Schema.cs', rust: 'schema.rs', python: 'schema.py', swift: 'Schema.swift',
  kotlin: 'Schema.kt', c: 'schema.h' };

/** Check both directions against freshly generated types, rather than handwritten output approximations. */
async function verify(entry, directory, options = [], selectedOutputs = outputs) {
  const args = ['--input', entry, '--language', Object.keys(selectedOutputs).join(','), '--cpp.format', 'false',
    '--depfile', path.join(directory, 'all.d'), ...options];
  fs.mkdirSync(directory, { recursive: true });
  for (const [language, name] of Object.entries(selectedOutputs)) {
    args.push(`--${language}.output`, path.join(directory, name));
  }
  execFileSync(compiler, args, { stdio: 'pipe' });
  const schemas = [];
  const pending = [entry];
  const seen = new Set();
  while (pending.length) {
    const file = pending.pop();
    if (seen.has(fileKey(file))) { continue; }
    seen.add(fileKey(file));
    const text = fs.readFileSync(file, 'utf8');
    const index = indexSource(text, true);
    schemas.push({ file, text, index });
    pending.push(...index.includes.map(include => path.resolve(path.dirname(file), include.name)));
  }
  const files = Object.values(selectedOutputs).map(name => path.join(directory, name));
  const dependencies = new Map(files.map(file => [fileKey(file), [entry]]));
  const resolver = new Navigator({ read: async file => {
    try { return fs.readFileSync(file, 'utf8'); } catch { return undefined; }
  }, files: async () => ({ schemas: schemas.map(schema => schema.file), headers: [], outputs: files, dependencies }),
  cancelled: () => false, includeDirectories: [] });
  let checks = 0;
  for (const schema of schemas) {
    for (const symbol of schema.index.symbols) {
      const definitions = await resolver.schema(schema.file, symbol.end, true);
      assert.equal(definitions.length, files.length, `${symbol.qualified}: expected every generated language, got ${definitions.map(item => path.basename(item.file))}`);
      for (const target of definitions) {
        const declarations = await resolver.generatedDeclaration(target.file, target.end);
        assert.deepEqual(declarations.map(item => [fileKey(item.file), item.start, item.end]),
          [[fileKey(schema.file), symbol.start, symbol.end]], `${symbol.qualified} from ${target.file}`);
        ++checks;
      }
    }
  }
  return checks;
}

/** Generate into an isolated directory; no language SDK, build or modification of existing output is needed. */
async function main() {
  assert.ok(compiler, 'Set SERIALIZER_COMPILER to the current built Serializer executable.');
  const parent = path.join(repository, 'out/extension-tests');
  fs.mkdirSync(parent, { recursive: true });
  const directory = fs.mkdtempSync(path.join(parent, 'generated-navigation-'));
  let checks = await verify(path.join(repository, 'example/schemas/complex/model.serializer'), path.join(directory, 'complex'));
  const input = path.join(directory, 'acronyms.serializer');
  fs.writeFileSync(input, `serializer version 1;
namespace HTTPModels { enum HTTPState { Ready } class HTTPRecord { public HTTPState StateValue; } }
namespace HTTP2Models { class ID2Record { public HTTPModels::HTTPRecord ItemValue; } }
class Model { public HTTP2Models::ID2Record ItemValue; }
namespace Names_ { class Value_Type {} }
`);
  checks += await verify(input, path.join(directory, 'acronyms'));
  checks += await verify(input, path.join(directory, 'preserve'),
    ['cpp', 'java', 'js', 'go', 'csharp'].flatMap(language => [`--${language}.naming`, 'preserve']));
  for (const profile of ['serializer', 'core', 'google', 'llvm', 'gnu', 'cert', 'misra', 'autosar', 'qt']) {
    checks += await verify(input, path.join(directory, `cpp-${profile}`),
      ['--cpp.coding_standard', profile], { cpp: outputs.cpp });
  }
  for (const profile of ['serializer', 'google', 'oracle']) {
    checks += await verify(input, path.join(directory, `java-${profile}`),
      ['--java.coding_standard', profile, '--java.package', 'example.models'], { java: outputs.java });
  }
  console.log(`Fresh compiler navigation passed: ${checks} bidirectional type checks across all 11 output languages, transitive includes, acronyms, and preserved names.`);
}

main().catch(error => { console.error(error.stderr?.toString() || error); process.exitCode = 1; });
