import * as path from 'node:path';
import * as vscode from 'vscode';
import type { CMakeToolsExtensionExports, CodeModel } from 'vscode-cmake-tools';
import { activeConfiguration, fileKey, generatedHeaders, sourceIncludes } from './model';
import { cppIncludeAt } from './navigation_model';
import { NavigationFiles, NavigationTarget, Navigator } from './navigator';

const excludedDirectories = /(?:^|[\\/])(?:\.git|node_modules|\.venv)(?:[\\/]|$)/;
const maximumCachedFiles = 96;
const maximumCachedCharacters = 16 * 1024 * 1024;
const maximumFileBytes = 16 * 1024 * 1024;
interface CachedFile { stamp: string; text: string }

/** Register passive navigation; no code path activates CMake Tools, configures, saves or builds. */
export function registerNavigation(context: vscode.ExtensionContext): void {
  const cache = new Map<string, CachedFile>();

  /** Prefer live buffers and validate disk caches so edits/deletions are visible immediately. */
  async function read(file: string): Promise<string | undefined> {
    const uri = vscode.Uri.file(file);
    const key = fileKey(file);
    const open = vscode.workspace.textDocuments.find(document =>
      document.uri.scheme === 'file' && fileKey(document.uri.fsPath) === key);
    if (open) { return open.getText(); }
    try {
      const stat = await vscode.workspace.fs.stat(uri);
      if (!(stat.type & vscode.FileType.File) || stat.size > maximumFileBytes) { return undefined; }
      const stamp = `${stat.mtime}:${stat.ctime}:${stat.size}`;
      const cached = cache.get(key);
      if (cached?.stamp === stamp) { return cached.text; }
      const text = Buffer.from(await vscode.workspace.fs.readFile(uri)).toString('utf8');
      cache.delete(key);
      cache.set(key, { stamp, text });
      let characters = [...cache.values()].reduce((sum, entry) => sum + entry.text.length, 0);
      while (cache.size > maximumCachedFiles || characters > maximumCachedCharacters) {
        const oldest = cache.keys().next().value!;
        characters -= cache.get(oldest)!.text.length;
        cache.delete(oldest);
      }
      return text;
    } catch {
      cache.delete(key);
      return undefined;
    }
  }

  /** Consult only an already active, configured CMake extension; absence is not an error. */
  async function configuration(uri: vscode.Uri): Promise<CodeModel.Configuration | undefined> {
    if (!vscode.workspace.isTrusted) { return undefined; }
    const extension = vscode.extensions.getExtension<CMakeToolsExtensionExports>('ms-vscode.cmake-tools');
    if (!extension?.isActive) { return undefined; }
    try {
      const project = await extension.exports.getApi(1).getProject(uri);
      return project ? activeConfiguration(project.codeModel, await project.getActiveBuildType()) : undefined;
    } catch { return undefined; }
  }

  /** Discover existing workspace outputs, including ignored build directories and legacy headers. */
  async function discover(document: vscode.TextDocument, model: CodeModel.Configuration | undefined,
    token: vscode.CancellationToken): Promise<NavigationFiles> {
    const folder = vscode.workspace.getWorkspaceFolder(document.uri);
    const base = folder ?? vscode.Uri.file(path.dirname(document.uri.fsPath));
    // Only null disables files.exclude as well as search.exclude; a custom exclusion
    // glob still inherits files.exclude in VS Code and can hide every build output.
    const schemas = (await vscode.workspace.findFiles(new vscode.RelativePattern(base, '**/*.serializer'),
      null, undefined, token)).filter(uri => !excludedDirectories.test(uri.fsPath)).map(uri => uri.fsPath);
    // Active configuration outputs take precedence over other build trees/profiles.
    const registered = model ? generatedHeaders(model).map(header => header.path) : [];
    if (registered.length) { return { schemas, headers: registered }; }
    const candidates = await vscode.workspace.findFiles(
      new vscode.RelativePattern(base, '**/*.{hpp,h,hxx,hh,hpp.d,h.d,hxx.d,hh.d}'),
      null, undefined, token);
    const stems = new Set(schemas.map(schema => fileKey(path.basename(schema, '.serializer'))));
    for (const include of document.getText().matchAll(/^\s*#\s*include\s*["<]([^">\r\n]+)[">]/gm)) {
      stems.add(fileKey(path.basename(include[1], path.extname(include[1]))));
    }
    const headers = new Set<string>();
    for (const candidate of candidates) {
      if (excludedDirectories.test(candidate.fsPath)) { continue; }
      if (candidate.fsPath.endsWith('.d')) {
        headers.add(candidate.fsPath.slice(0, -'.d'.length));
      } else if (stems.has(fileKey(path.basename(candidate.fsPath, path.extname(candidate.fsPath))))) {
        headers.add(candidate.fsPath);
      }
    }
    return { schemas, headers: [...headers] };
  }

  /** Create a cancellable request using only available files and an optional CMake snapshot. */
  async function navigator(document: vscode.TextDocument, token: vscode.CancellationToken): Promise<Navigator> {
    const model = await configuration(document.uri);
    return new Navigator({ read, files: () => discover(document, model, token),
      includeDirectories: model ? sourceIncludes(model, document.uri.fsPath).map(group => group.directories) : [],
      cancelled: () => token.isCancellationRequested });
  }

  /** Convert offsets to editor locations using the same live contents used during resolution. */
  async function locations(targets: NavigationTarget[], token: vscode.CancellationToken): Promise<vscode.Location[]> {
    const result: vscode.Location[] = [];
    for (const target of targets) {
      if (token.isCancellationRequested) { return []; }
      try {
        const document = await vscode.workspace.openTextDocument(vscode.Uri.file(target.file));
        result.push(new vscode.Location(document.uri,
          new vscode.Range(document.positionAt(target.start), document.positionAt(target.end))));
      } catch { /* A deleted or inaccessible destination is a navigation miss. */ }
    }
    return result;
  }

  /** Resolve C++ types through their language service instead of guessing from identifier text. */
  async function declarations(document: vscode.TextDocument, position: vscode.Position,
    token: vscode.CancellationToken): Promise<vscode.Location[]> {
    const resolver = await navigator(document, token);
    const offset = document.offsetAt(position);
    if (document.languageId === 'serializer') {
      return locations(await resolver.schema(document.uri.fsPath, offset, false), token);
    }
    if (cppIncludeAt(document.getText(), offset)) {
      return locations(await resolver.cppInclude(document.uri.fsPath, offset, false), token);
    }
    // A generated class declaration itself can be resolved without a C++ language service.
    const local = await resolver.cppDeclaration(document.uri.fsPath, offset);
    if (local.length) { return locations(local, token); }
    const resolved = await vscode.commands.executeCommand<Array<vscode.Location | vscode.LocationLink>>(
      'vscode.executeDefinitionProvider', document.uri, position) ?? [];
    const targets: NavigationTarget[] = [];
    for (const target of resolved) {
      if (token.isCancellationRequested) { return []; }
      const uri = 'targetUri' in target ? target.targetUri : target.uri;
      if (uri.scheme !== 'file') { continue; }
      try {
        const header = await vscode.workspace.openTextDocument(uri);
        const selection = 'targetUri' in target ? target.targetSelectionRange ?? target.targetRange : target.range;
        targets.push(...await resolver.cppDeclaration(uri.fsPath, header.offsetAt(selection.start)));
      } catch { /* Other providers may return destinations that no longer exist. */ }
    }
    return locations(targets, token);
  }

  /** Resolve schema definitions and C++ includes; C++ class definitions remain with its language service. */
  async function definitions(document: vscode.TextDocument, position: vscode.Position,
    token: vscode.CancellationToken): Promise<vscode.Location[]> {
    if (document.languageId !== 'serializer' && !cppIncludeAt(document.getText(), document.offsetAt(position))) {
      return [];
    }
    const resolver = await navigator(document, token);
    const targets = document.languageId === 'serializer'
      ? await resolver.schema(document.uri.fsPath, document.offsetAt(position), true)
      : await resolver.cppInclude(document.uri.fsPath, document.offsetAt(position), true);
    return locations(targets, token);
  }

  /** Keep missing providers/projects/files silent in standard navigation actions. */
  function provider(action: typeof declarations): typeof declarations {
    return async (document, position, token) => {
      try { return await action(document, position, token); }
      catch (error) { console.warn('Serializer navigation failed:', error); return []; }
    };
  }

  const selector = [{ language: 'serializer', scheme: 'file' }, { language: 'cpp', scheme: 'file' }];
  const declarationProvider = provider(declarations);
  context.subscriptions.push(vscode.languages.registerDeclarationProvider(selector, {
    provideDeclaration: declarationProvider
  }), vscode.languages.registerDefinitionProvider(selector, { provideDefinition: provider(definitions) }));

  /** Open one available destination, asking only when several existing outputs apply. */
  async function show(destinations: vscode.Location[]): Promise<void> {
    const selected = destinations.length === 1 ? destinations[0] : (await vscode.window.showQuickPick(
      destinations.map(location => ({ label: vscode.workspace.asRelativePath(location.uri),
        description: `${location.uri.fsPath}:${location.range.start.line + 1}`, location })),
      { placeHolder: 'Choose an available Serializer destination' }))?.location;
    if (selected) { await vscode.window.showTextDocument(selected.uri, { selection: selected.range }); }
  }

  context.subscriptions.push(vscode.commands.registerCommand('serializer.goToSchemaDeclaration', async () => {
    const editor = vscode.window.activeTextEditor;
    if (!editor || !['serializer', 'cpp'].includes(editor.document.languageId)) { return; }
    const cancellation = new vscode.CancellationTokenSource();
    try {
      const targets = await declarationProvider(editor.document, editor.selection.active, cancellation.token);
      if (targets.length) { await show(targets); }
    } finally { cancellation.dispose(); }
  }), vscode.commands.registerCommand('serializer.openGeneratedHeader', async (uri?: vscode.Uri) => {
    const schema = uri ?? vscode.window.activeTextEditor?.document.uri;
    if (!schema || schema.scheme !== 'file' || !schema.fsPath.endsWith('.serializer')) { return; }
    const cancellation = new vscode.CancellationTokenSource();
    try {
      const document = await vscode.workspace.openTextDocument(schema);
      const resolver = await navigator(document, cancellation.token);
      const targets = await locations(await resolver.headers(schema.fsPath), cancellation.token);
      if (targets.length) { await show(targets); }
    } catch { /* Opening available output never offers or invokes generation. */ }
    finally { cancellation.dispose(); }
  }), { dispose: () => cache.clear() });
}
