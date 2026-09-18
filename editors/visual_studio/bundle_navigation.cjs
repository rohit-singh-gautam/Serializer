const fs = require('node:fs');
const path = require('node:path');
const esbuild = require('../vscode/node_modules/esbuild');

// Bundle the shared resolver with a small native-path adapter, without a Node.js runtime dependency.
fs.mkdirSync(path.join(__dirname, 'obj'), { recursive: true });
esbuild.buildSync({ entryPoints: [path.join(__dirname, 'navigation_bridge.ts')], bundle: true,
  outfile: path.join(__dirname, 'obj/navigation.js'), platform: 'neutral', format: 'iife',
  globalName: 'serializerNavigation', target: 'es2020', define: { 'process.platform': '"win32"' },
  alias: { 'node:path': path.join(__dirname, 'path_bridge.ts') } });
