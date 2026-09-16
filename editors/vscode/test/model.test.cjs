const assert = require('node:assert/strict');
const path = require('node:path');
const { test } = require('node:test');
const { activeConfiguration, generatedHeaders, sourceIncludes, includeCandidates,
  parseInclude } = require('../out/model');

const root = path.resolve('workspace with spaces');
const configuration = { name: 'Debug', projects: [{ name: 'app', sourceDirectory: root,
  targets: ['google', 'llvm'].map(name => ({ name, sourceDirectory: path.join(root, name),
    fileGroups: [
      { sources: ['main.cpp'], language: 'CXX', isGenerated: false,
        includePath: [{ path: path.join(root, 'build', name) }] },
      { sources: [`../build/${name}/account.hpp`, '../build/stamp.rule'], isGenerated: true }
    ] })) }] };

test('literal includes support whitespace and preserve quoted search semantics', () => {
  assert.deepEqual(parseInclude(' # include "models/account.hpp" // comment'),
    { name: 'models/account.hpp', quoted: true });
  assert.deepEqual(parseInclude('#include <rohit/serializer.hpp>'),
    { name: 'rohit/serializer.hpp', quoted: false });
  assert.equal(parseInclude('// #include <account.hpp>'), undefined);
  assert.equal(parseInclude('#include HEADER'), undefined);
  assert.equal(parseInclude('#include <bad.hpp"'), undefined);
});

test('configuration selection does not guess among profiles', () => {
  const release = { name: 'Release', projects: [] };
  assert.equal(activeConfiguration({ configurations: [configuration, release] }, 'Debug'), configuration);
  assert.throws(() => activeConfiguration({ configurations: [configuration, release] }, undefined));
  assert.throws(() => activeConfiguration({ configurations: [configuration] }, 'Release'));
  assert.throws(() => activeConfiguration(undefined, 'Debug'));
  const unnamed = { name: '', projects: [] };
  assert.equal(activeConfiguration({ configurations: [unnamed] }, 'Debug'), unnamed);
});

test('same-name generated headers retain separate profile paths and target ownership', () => {
  const headers = generatedHeaders(configuration, 'account.hpp');
  assert.deepEqual(headers, ['google', 'llvm'].map(name => ({
    path: path.join(root, 'build', name, 'account.hpp'), targets: [name]
  })));
  assert.equal(generatedHeaders(configuration, 'absent.hpp').length, 0);
  assert.equal(generatedHeaders(configuration).length, 2);
});

test('shared generated paths deduplicate without losing owners', () => {
  const target = configuration.projects[0].targets[0];
  const shared = { name: 'Debug', projects: [{ name: 'app', sourceDirectory: root,
    targets: [target, { ...target, name: 'client' }] }] };
  assert.deepEqual(generatedHeaders(shared)[0].targets, ['google', 'client']);
});

test('include lookup keeps source-specific target settings and compiler order', () => {
  const source = path.join(root, 'google/main.cpp');
  const groups = sourceIncludes(configuration, source);
  assert.deepEqual(groups, [{ target: 'google', directories: [path.join(root, 'build/google')] }]);
  assert.deepEqual(sourceIncludes(configuration, path.join(root, 'unregistered.cpp')), []);
  assert.deepEqual(includeCandidates(source, { name: 'account.hpp', quoted: true }, groups[0].directories),
    [path.join(root, 'google/account.hpp'), path.join(root, 'build/google/account.hpp')]);
  assert.deepEqual(includeCandidates(source, { name: 'account.hpp', quoted: false }, groups[0].directories),
    [path.join(root, 'build/google/account.hpp')]);
});
