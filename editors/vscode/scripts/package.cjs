// Bundle the extension and copy canonical assets into the installable package.
const fs = require('node:fs');
const path = require('node:path');
const esbuild = require('esbuild');
const root = path.resolve(__dirname, '..');
fs.mkdirSync(path.join(root, 'syntaxes'), { recursive: true });
fs.mkdirSync(path.join(root, '../../out/extensions'), { recursive: true });
fs.copyFileSync(path.join(root, '../serializer.tmLanguage.json'),
  path.join(root, 'syntaxes/serializer.tmLanguage.json'));
fs.copyFileSync(path.join(root, '../../LICENSE'), path.join(root, 'LICENSE'));
// Keep the MIT notice for the CMake Tools API helper bundled into extension.js.
esbuild.buildSync({
  entryPoints: [path.join(root, 'src/extension.ts')],
  outfile: path.join(root, 'dist/extension.js'),
  bundle: true,
  platform: 'node',
  format: 'cjs',
  target: 'node20',
  external: ['vscode'],
  sourcemap: true
});
fs.copyFileSync(path.join(root, 'node_modules/vscode-cmake-tools/LICENSE'),
  path.join(root, 'dist/CMAKE_TOOLS_LICENSE'));
fs.copyFileSync(path.join(root, '../../logo/serializer_logo_128x128.png'),
  path.join(root, 'dist/serializer_logo.png'));
fs.copyFileSync(path.join(root, '../../logo/serializer_icon_32x32.png'),
  path.join(root, 'dist/serializer_icon_32x32.png'));
