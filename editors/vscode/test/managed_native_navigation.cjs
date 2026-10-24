const path = require('node:path');
const { verify } = require('./generated_navigation.cjs');

/** Qualify real managed ESM/declaration outputs through the shared schema/navigation adapter. */
async function main() {
  const repository = path.resolve(__dirname, '../../..');
  const directory = path.join(repository, 'out/extension-tests/managed-native');
  let checks = 0;
  for (const [width, encoding, protocols] of [[32, 'compact_varint', 'all'], [64, 'fixed', 'binary_none']]) {
    checks += await verify(path.join(repository, 'test/resources/javascript_managed_engine.serializer'),
      path.join(directory, `uint${width}_${encoding}_${protocols}`),
      ['--js.managed_engine', 'true', '--typescript.managed_engine', 'true',
        '--managed.id_type', `uint${width}`, '--managed.id_encoding', encoding, '--protocols', protocols],
      { js: 'model.mjs', typescript: 'model.d.mts' }, true, true);
  }
  console.log(`Fresh managed JavaScript/TypeScript navigation passed: ${checks} bidirectional type/field checks.`);
}

main().catch(error => { console.error(error.stderr?.toString() || error); process.exitCode = 1; });
