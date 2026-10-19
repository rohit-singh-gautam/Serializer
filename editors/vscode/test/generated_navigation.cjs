const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { execFileSync } = require('node:child_process');
const { Navigator } = require('../out/navigator');
const { indexSource, managedNames, resolveType } = require('../out/navigation_model');
const { fileKey } = require('../out/model');
const { generatedIncludes, generatedMember, generatedSource, indexGeneratedSource, outputMemberNames } = require('../out/generated_navigation');

const repository = path.resolve(__dirname, '../../..');
const compiler = process.env.SERIALIZER_COMPILER;
const outputs = { cpp: 'model.hpp', java: 'Schema.java', js: 'schema.mjs', typescript: 'schema.d.mts',
  go: 'schema.go', csharp: 'Schema.cs', rust: 'schema.rs', python: 'schema.py', swift: 'Schema.swift',
  kotlin: 'Schema.kt', c: 'schema.h' };

/** Check both directions against freshly generated types, rather than handwritten output approximations. */
async function verify(entry, directory, options = [], selectedOutputs = outputs, typeReferences = false,
  memberDeclarations = false) {
  const args = ['--input', entry, '--language', Object.keys(selectedOutputs).join(','), '--include-mode', 'inline', '--cpp.format', 'false',
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
      if (symbol.typeParameter) { continue; }
      const definitions = await resolver.schema(schema.file, symbol.end, true);
      assert.equal(new Set(definitions.map(item => item.file)).size, files.length, `${symbol.qualified}: expected every generated language, got ${definitions.map(item => path.basename(item.file))}`);
      for (const offset of [symbol.start, symbol.end - 1]) {
        assert.deepEqual(await resolver.schema(schema.file, offset, true), definitions,
          `${symbol.qualified}: schema boundary ${offset}`);
      }
      for (const target of definitions) {
        for (let offset = target.start; offset <= target.end; ++offset) {
          const declarations = await resolver.generatedDeclaration(target.file, offset);
          assert.deepEqual(declarations.map(item => [fileKey(item.file), item.start, item.end]),
            [[fileKey(schema.file), symbol.start, symbol.end]], `${symbol.qualified} from ${target.file} at ${offset}`);
          ++checks;
        }
        if (/\.hpp$/.test(target.file)) {
          const generated = indexSource(fs.readFileSync(target.file, 'utf8'), false);
          for (const companion of generated.symbols.filter(item => managedNames(symbol.qualified).includes(item.qualified))) {
            const origins = await resolver.generatedDeclaration(target.file, companion.end);
            assert.deepEqual(origins.map(item => [fileKey(item.file), item.start, item.end]),
              [[fileKey(schema.file), symbol.start, symbol.end]], `${companion.qualified} managed origin`);
            ++checks;
          }
        }
      }
    }
    if (typeReferences) {
      const symbols = schemas.flatMap(source => source.index.symbols);
      for (const reference of schema.index.references) {
        const target = resolveType(reference, symbols).find(symbol => !symbol.typeParameter);
        if (!target) { continue; }
        const origin = schemas.find(source => source.index.symbols.includes(target));
        for (let cursor = reference.start; cursor <= reference.end; ++cursor) {
          const declarations = await resolver.schema(schema.file, cursor, false);
          assert.deepEqual(declarations.map(item => [fileKey(item.file), item.start, item.end]),
            [[fileKey(origin.file), target.start, target.end]], 'schema type reference boundary');
          const definitions = await resolver.schema(schema.file, cursor, true);
          assert.equal(new Set(definitions.map(item => item.file)).size, files.length,
            'schema type reference definitions in each generated language');
          ++checks;
        }
      }
    }
    if (memberDeclarations) {
      for (const reference of schema.index.memberReferences ?? []) {
        const declarations = await resolver.schema(schema.file, reference.start, false);
        assert.equal(declarations.length, 1, 'conversion field resolves to one authored declaration');
        for (let cursor = reference.start; cursor <= reference.end; ++cursor) {
          assert.deepEqual(await resolver.schema(schema.file, cursor, false), declarations,
            'conversion field cursor boundaries');
          const definitions = await resolver.schema(schema.file, cursor, true);
          assert.equal(new Set(definitions.map(item => item.file)).size, files.length,
            'conversion field definitions in every language');
          ++checks;
        }
      }
      for (const declaration of schema.index.members ?? []) {
        if (declaration.wireOnly) { continue; }
        const definitions = await resolver.schema(schema.file, declaration.start, true);
        assert.equal(new Set(definitions.map(item => item.file)).size, files.length,
          `${declaration.owner}::${declaration.name}: generated field definitions in every language`);
        assert.deepEqual(await resolver.schema(schema.file, declaration.end, true), definitions,
          'schema inherited field definition boundary');
        for (const definition of definitions) {
          for (const cursor of [definition.start, definition.end]) {
            const origins = await resolver.generatedDeclaration(definition.file, cursor);
            assert.deepEqual(origins.map(origin => [fileKey(origin.file), origin.start, origin.end]),
              [[fileKey(schema.file), declaration.start, declaration.end]],
              'every generated field copy retains generic and named-instantiation provenance');
            ++checks;
          }
        }
        for (const output of files) {
          const text = fs.readFileSync(output, 'utf8');
          const generated = indexGeneratedSource(text, output);
          const names = outputMemberNames(declaration.name, output, text);
          let target;
          for (const token of text.matchAll(/[A-Za-z_]\w*/g)) {
            if (!names.includes(token[0]) || !generatedMember(text, output, generated, token.index)) { continue; }
            const origins = await resolver.generatedDeclaration(output, token.index);
            if (origins.some(origin => fileKey(origin.file) === fileKey(schema.file) && origin.start === declaration.start)) {
              target = token;
              break;
            }
          }
          assert.ok(target, `${declaration.owner}::${declaration.name}: generated field navigation in ${output}`);
          for (const offset of [target.index, target.index + target[0].length]) {
            const origins = await resolver.generatedDeclaration(output, offset);
            assert.deepEqual(origins.map(origin => [fileKey(origin.file), origin.start, origin.end]),
              [[fileKey(schema.file), declaration.start, declaration.end]], 'generated inherited field boundary');
            ++checks;
          }
        }
      }
    }
  }
  return checks;
}

/** Exercise real split header ownership and every transitive type/include selection boundary. */
async function verifySeparate(entry, directory, options = [], extension = '.hpp') {
  fs.mkdirSync(directory, { recursive: true });
  const output = path.join(directory, `renamed${extension}`);
  execFileSync(compiler, ['--input', entry, '--language', 'cpp', '--output', output,
    '--cpp.format', 'false', '--depfile', path.join(directory, 'all.d'), ...options], { stdio: 'pipe' });
  const files = fs.readdirSync(directory, { recursive: true })
    .filter(name => path.extname(name) === extension).map(name => path.join(directory, name));
  const schemas = files.map(file => {
    const origin = generatedSource(fs.readFileSync(file, 'utf8'));
    assert.ok(origin, `exact source metadata for ${file}`);
    const text = fs.readFileSync(origin, 'utf8');
    return { file: origin, output: file, text, index: indexSource(text, true) };
  });
  assert.ok(schemas.length > 1, 'included schemas generate separate C++ headers by default');
  const resolver = new Navigator({ read: async file => {
    try { return fs.readFileSync(file, 'utf8'); } catch { return undefined; }
  }, files: async () => ({ schemas: schemas.map(schema => schema.file), headers: files }),
  cancelled: () => false, includeDirectories: [] });
  const symbols = schemas.flatMap(schema => schema.index.symbols);
  let checks = 0;
  for (const schema of schemas) {
    for (const symbol of schema.index.symbols.filter(symbol => !symbol.typeParameter)) {
      const definitions = await resolver.schema(schema.file, symbol.end, true);
      assert.deepEqual([...new Set(definitions.map(target => fileKey(target.file)))], [fileKey(schema.output)],
        `${symbol.qualified}: separate source owns its generated declaration`);
      for (const definition of definitions) {
        for (const cursor of [definition.start, definition.end]) {
          const origins = await resolver.generatedDeclaration(definition.file, cursor);
          assert.deepEqual(origins.map(origin => [fileKey(origin.file), origin.start, origin.end]),
            [[fileKey(schema.file), symbol.start, symbol.end]], 'split declaration origin boundaries');
          ++checks;
        }
      }
    }
    for (const reference of schema.index.references) {
      const declaration = resolveType(reference, symbols).find(symbol => !symbol.typeParameter);
      if (!declaration) { continue; }
      const owner = schemas.find(source => source.index.symbols.includes(declaration));
      for (let cursor = reference.start; cursor <= reference.end; ++cursor) {
        const definitions = await resolver.schema(schema.file, cursor, true);
        assert.deepEqual([...new Set(definitions.map(target => fileKey(target.file)))], [fileKey(owner.output)],
          'split transitive type definition boundaries');
        ++checks;
      }
    }
    const text = fs.readFileSync(schema.output, 'utf8');
    for (const include of generatedIncludes(text)) {
      const included = path.resolve(path.dirname(schema.output), include);
      const owner = generatedSource(fs.readFileSync(included, 'utf8'));
      const start = text.indexOf(`"${include}"`) + 1;
      for (const cursor of [start, start + include.length]) {
        const targets = await resolver.cppInclude(schema.output, cursor, false);
        assert.deepEqual(targets.map(target => fileKey(target.file)), [fileKey(owner)],
          'split generated include opens its own schema');
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
  const xmlAuthoringInput = path.join(directory, 'xml-authoring.serializer');
  fs.writeFileSync(xmlAuthoringInput, `serializer version 1.20.0;
namespace xml_models {
  xml(namespace("urn:serializer:editor-xml"), scalars(attribute));
  class point stable_ids { public double x (1); public double y (2); }
  class drawing stable_ids {
    public uint32 id (1);
    public array xml_models::point points (2) xml(element, name("point"), wrapper("points"));
  }
}
`);
  checks += await verify(xmlAuthoringInput, path.join(directory, 'xml-authoring'),
    ['--xml.enabled', 'true'], outputs, true, true);
  const packageInput = path.join(directory, 'package.serializer');
  fs.writeFileSync(packageInput, `serializer version 1.20.0;
namespace package_models {
  xml(namespace("urn:serializer:editor-package"), scalars(attribute));
  class model stable_ids { public uint32 id (1); }
  package document {
    part main : package_models::model {
      name("/data/model.xml"); content_type("application/xml");
      relationship("urn:serializer:main"); compression(deflate);
    }
  }
}
`);
  checks += await verify(packageInput, path.join(directory, 'package'),
    ['--xml.enabled', 'true'], { cpp: outputs.cpp }, true);
  const managedReferenceInput = path.join(directory, 'managed-reference.serializer');
  fs.writeFileSync(managedReferenceInput, `serializer version 1.20.0;
namespace managed_models {
  class first managed class_id(1001) {
    public managed_ref<managed_models::second> next (1);
    public optional<managed_ref<managed_models::first>> previous (2);
    public array managed_ref<managed_models::second> items (3);
  }
  class second managed class_id(1002) { public managed_ref<managed_models::first> previous (1); }
}
`);
  checks += await verify(managedReferenceInput, path.join(directory, 'managed-reference'),
    [], { cpp: outputs.cpp }, true, true);
  checks += await verifySeparate(path.join(repository, 'example/schemas/complex/model.serializer'),
    path.join(directory, 'complex-separate'));
  checks += await verifySeparate(path.join(repository, 'example/schemas/complex/model.serializer'),
    path.join(directory, 'complex-separate-hxx'), ['--include-mode', 'separate'], '.hxx');
  checks += await verify(path.join(repository, 'example/schemas/versioning/model.serializer'), path.join(directory, 'versioning'));
  checks += await verify(path.join(repository, 'example/generics/result.serializer'), path.join(directory, 'generics'));
  checks += await verify(path.join(repository, 'example/schemas/union_variant/model.serializer'),
    path.join(directory, 'union-variant'));
  checks += await verify(path.join(repository, 'test/resources/typed_magic_language.serializer'),
    path.join(directory, 'typed-magic'), [], outputs, true);
  checks += await verify(path.join(repository, 'test/resources/compact_language.serializer'),
    path.join(directory, 'compact-fields'));
  checks += await verify(path.join(repository, 'test/resources/digest_language.serializer'),
    path.join(directory, 'digests'));
  checks += await verify(path.join(repository, 'example/schemas/conversions/model.serializer'),
    path.join(directory, 'conversions'), [], outputs, true, true);
  checks += await verify(path.join(repository, 'test/resources/reference_fields.serializer'),
    path.join(directory, 'reference-fields'), [], outputs, true, true);
  checks += await verify(path.join(repository, 'example/schemas/derived_sizes/model.serializer'),
    path.join(directory, 'derived-sizes'), [], outputs, true, true);
  checks += await verify(path.join(repository, 'example/schemas/stateless_layout/model.serializer'),
    path.join(directory, 'stateless-layout'), [], outputs, true, true);
  checks += await verify(path.join(repository, 'test/resources/inferred_arrays.serializer'),
    path.join(directory, 'inferred-arrays'), [], { cpp: outputs.cpp }, true);
  const freshDefaultsInput = path.join(directory, 'fresh-record-defaults.serializer');
  fs.writeFileSync(freshDefaultsInput, `serializer version 1.15.0;
namespace fresh_models {
  class area stable_ids {
    public uint32 width (1) {3};
    public uint32 height (2) {4};
    public constructor(uint32 width_input, uint32 height_input) {
      width = width_input;
      height = height_input;
    }
  }
  class frame stable_ids {
    public fresh_models::area extent (1);
    public uint32 tag (2) {7};
  }
  class holder stable_ids {
    public fresh_models::area extent (1);
    public fresh_models::frame placement (2);
    public constructor(fresh_models::area area_input = {}, fresh_models::frame frame_input = {{}, 7}) {
      extent = area_input;
      placement = frame_input;
    }
    public factory sized(fresh_models::area area_input = {10, 20}, fresh_models::frame frame_input = {}) {
      extent = area_input;
      placement = frame_input;
    }
  }
}
`);
  checks += await verify(freshDefaultsInput, path.join(directory, 'fresh-record-defaults'), [], outputs, true, true);
  const nonthrowingInput = path.join(directory, 'noexcept.serializer');
  fs.writeFileSync(nonthrowingInput, `serializer version 1.13.0;
namespace nonthrowing_models {
  class sample stable_ids { public uint32 id (1); }
  class model stable_ids {
    public sample value (1);
    public function sample complete(sample input) readonly noexcept(cpp);
  }
}
`);
  checks += await verify(nonthrowingInput, path.join(directory, 'noexcept'), [], outputs, true, true);
  const optionalInput = path.join(directory, 'optional.serializer');
  fs.writeFileSync(optionalInput, `serializer version 1.7.0;
namespace optional_models {
  class sample stable_ids { public uint32 id (1); }
  class model stable_ids {
    public optional<optional_models::sample> item (1);
    public optional<optional<optional_models::sample>> nested (2);
    public optional<array<optional_models::sample>> rows (3);
    public optional<map<string, optional_models::sample>> lookup (4);
    public array optional<optional_models::sample> items (5);
  }
}
`);
  checks += await verify(optionalInput, path.join(directory, 'optional'), [], outputs, true);
  const inheritanceInput = path.join(directory, 'generic-inheritance.serializer');
  fs.writeFileSync(inheritanceInput, `serializer version 1.8.0;
namespace inherited_models {
  class base<T> stable_ids { public uint32 inherited_count (1); public T owned (2); }
  class derived<T> stable_ids : public base<T> (10, "base") { public string label (1); }
  instantiate count_derived = derived<uint32>;
  class root stable_ids : public derived<uint32> (10, "value") { public uint32 tag (1); }
}
`);
  checks += await verify(inheritanceInput, path.join(directory, 'generic-inheritance'), [], outputs, true, true);
  const suppressionInput = path.join(directory, 'warning-suppression.serializer');
  fs.writeFileSync(suppressionInput, `serializer version 1.5.0;
namespace warning_models {
  class sample stable_ids { public uint32 ignore (1); public uint32 warning (2); }
  class exposed_revision stable_ids {
    public uint32 version (1) { 1 } ignore(warning version);
  }
  class model stable_ids {
    public version uint32 (1) { 1 };
    public uint32 revision (2) { 1 } ignore(warning version);
    public array warning_models::sample samples (3) ignore(warning magic, version);
  }
}
`);
  checks += await verify(suppressionInput, path.join(directory, 'warning-suppression'), [], outputs, true);
  const input = path.join(directory, 'acronyms.serializer');
  const schema = `serializer version 1.0.0;
namespace HTTPModels { enum HTTPState { Ready } class HTTPRecord { public HTTPState StateValue; } }
namespace HTTP2Models { class ID2Record { public HTTPModels::HTTPRecord ItemValue; } }
class Model { public HTTP2Models::ID2Record ItemValue; }
namespace Names_ { class Value_Type {} }
`;
  const acronymDirectory = path.join(directory, 'acronyms');
  fs.writeFileSync(input, schema.replace('serializer version 1.0.0;', 'serializer version 1;'));
  checks += await verify(input, acronymDirectory);
  const legacyOutputs = new Map(Object.values(outputs).map(name =>
    [name, fs.readFileSync(path.join(acronymDirectory, name), 'utf8')]));
  fs.writeFileSync(input, schema);
  checks += await verify(input, path.join(directory, 'acronyms'));
  for (const [name, source] of legacyOutputs) {
    assert.equal(fs.readFileSync(path.join(acronymDirectory, name), 'utf8'), source,
      `${name}: legacy and dotted headers must produce identical codecs`);
  }
  checks += await verify(input, path.join(directory, 'preserve'),
    ['cpp', 'java', 'js', 'go', 'csharp'].flatMap(language => [`--${language}.naming`, 'preserve']));
  for (const profile of ['serializer', 'core', 'google', 'llvm', 'gnu', 'cert', 'misra', 'autosar', 'qt']) {
    checks += await verify(input, path.join(directory, `cpp-${profile}`),
      ['--cpp.coding_standard', profile], { cpp: outputs.cpp });
    checks += await verify(path.join(repository, 'test/resources/generics.serializer'),
      path.join(directory, `generics-cpp-${profile}`), ['--cpp.coding_standard', profile], { cpp: outputs.cpp });
    checks += await verify(path.join(repository, 'test/resources/dimensions.serializer'),
      path.join(directory, `dimensions-cpp-${profile}`), ['--cpp.coding_standard', profile], { cpp: outputs.cpp });
  }
  for (const profile of ['serializer', 'core', 'google', 'llvm', 'gnu', 'cert', 'misra', 'autosar', 'qt']) {
    checks += await verify(path.join(repository, 'test/resources/managed_generated.serializer'),
      path.join(directory, `managed-${profile}`), ['--cpp.coding_standard', profile], { cpp: outputs.cpp });
    checks += await verify(path.join(repository, 'test/resources/managed_generated.serializer'),
      path.join(directory, `managed-separated-${profile}`),
      ['--cpp.coding_standard', profile, '--managed.separate_values', 'true'], { cpp: outputs.cpp });
  }
  for (const profile of ['serializer', 'google', 'oracle']) {
    checks += await verify(input, path.join(directory, `java-${profile}`),
      ['--java.coding_standard', profile, '--java.package', 'example.models'], { java: outputs.java });
  }
  checks += await verify(path.join(repository, 'test/resources/projected_records.serializer'),
    path.join(directory, 'projected-records'), [], outputs, true, true);
  checks += await verify(path.join(repository, 'test/resources/record_maps.serializer'),
    path.join(directory, 'record-map-keys'), ['--config', path.join(repository, 'test/resources/record_maps.ini')],
    { cpp: outputs.cpp }, true, true);
  checks += await verify(path.join(repository, 'example/cpp/readonly_getters/model.serializer'),
    path.join(directory, 'readonly-getters'), ['--cpp.naming', 'preserve'],
    { cpp: outputs.cpp }, true, true);
  for (const name of ['hybrid_construction', 'overloaded_construction', 'authoring_construction']) {
    checks += await verify(path.join(repository, `test/resources/${name}.serializer`),
      path.join(directory, name), ['--cpp.naming', 'preserve'], { cpp: outputs.cpp }, true, true);
  }
  console.log(`Fresh compiler navigation passed: ${checks} bidirectional type/field checks across all 11 ordinary output languages; default/explicit separate C++ headers and exact source ownership with .hpp/.hxx outputs; language 1.15 fresh constructor/factory record defaults and nested positional defaults; language 1.14 C++ readonly getters and deleted temporary overloads; language 1.13 C++ noexcept signature types; C++ language 1.12 native/factory contracts, constructor overloads and generic readonly signatures; projected readonly record calls, generic record map keys, owning generic inheritance and inherited declarations, nested optionals and generic containers, unnamed revisions and warning suppression, unions and variants, digest fields, compact fields, typed magic and inferred arrays, identical codecs for legacy/dotted headers, transitive includes, acronyms, preserved names, and direct/separated managed classes across all C++ profiles.`);
}

main().catch(error => { console.error(error.stderr?.toString() || error); process.exitCode = 1; });
