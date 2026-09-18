import * as path from 'node:path';
import * as vscode from 'vscode';
import type { CMakeToolsExtensionExports, CodeModel } from 'vscode-cmake-tools';
import { activeConfiguration, fileKey, generatedHeaders, sourceIncludes } from './model';
import { cppIncludeAt, dependencyFiles } from './navigation_model';
import { navigationLanguages, outputLanguage, outputPattern } from './generated_navigation';
import { NavigationFiles, NavigationTarget, Navigator } from './navigator';

const excludedDirectories = /(?:^|[\\/])(?:\.git|node_modules|\.venv|\.vscode-test)(?:[\\/]|$)/;
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
    const [schemaUris, candidates] = await Promise.all([
      vscode.workspace.findFiles(new vscode.RelativePattern(base, '**/*.serializer'), null, undefined, token),
      vscode.workspace.findFiles(new vscode.RelativePattern(base, outputPattern), null, undefined, token)
    ]);
    const schemas = [...new Set([...schemaUris, ...vscode.workspace.textDocuments.map(item => item.uri)]
      .filter(uri => uri.scheme === 'file' && uri.fsPath.endsWith('.serializer') && !excludedDirectories.test(uri.fsPath))
      .map(uri => uri.fsPath))];
    // Active configuration outputs take precedence over other build trees/profiles.
    const registered = model ? generatedHeaders(model).map(header => header.path) : [];
    for (const project of model?.projects ?? []) {
      for (const target of project.targets) {
        for (const group of target.fileGroups ?? []) {
          if (!group.isGenerated) { continue; }
          for (const source of group.sources) {
            if (outputLanguage(source)) {
              registered.push(path.resolve(target.sourceDirectory ?? project.sourceDirectory, source));
            }
          }
        }
      }
    }
    const stems = new Set(schemas.map(schema => fileKey(path.basename(schema, '.serializer'))));
    for (const include of document.getText().matchAll(/^\s*#\s*include\s*["<]([^">\r\n]+)[">]/gm)) {
      stems.add(fileKey(path.basename(include[1], path.extname(include[1]))));
    }
    const outputs = new Set<string>(registered);
    const dependencies = new Map<string, string[]>();
    for (const candidate of candidates) {
      if (token.isCancellationRequested) { return { schemas: [], headers: [] }; }
      if (excludedDirectories.test(candidate.fsPath)) { continue; }
      if (candidate.fsPath.endsWith('.d')) {
        const text = await read(candidate.fsPath);
        if (text === undefined) { continue; }
        const files = dependencyFiles(text);
        if (!files.schemas.length) { continue; }
        for (const output of files.targets.filter(outputLanguage)) {
          const key = fileKey(output);
          const entries = dependencies.get(key) ?? [];
          if (!entries.some(entry => fileKey(entry) === fileKey(files.schemas[0]))) {
            entries.push(files.schemas[0]);
          }
          dependencies.set(key, entries);
          outputs.add(output);
        }
      } else if (stems.has(fileKey(path.basename(candidate.fsPath, path.extname(candidate.fsPath))))) {
        outputs.add(candidate.fsPath);
      }
    }
    // A configured language's outputs are authoritative; unrelated language outputs remain available.
    const configuredLanguages = new Set(registered.map(outputLanguage));
    const configured = new Set(registered.map(fileKey));
    const available = [...outputs].filter(output => !configuredLanguages.has(outputLanguage(output)) || configured.has(fileKey(output)));
    return { schemas, headers: available.filter(output => /\.(hpp|h|hxx|hh)$/i.test(output)),
      outputs: available, dependencies };
  }

  /** Create a cancellable request using only available files and an optional CMake snapshot. */
  async function navigator(document: vscode.TextDocument, token: vscode.CancellationToken,
    withoutConfiguration = false): Promise<Navigator> {
    const model = withoutConfiguration ? undefined : await configuration(document.uri);
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
    return [...new Map(result.map(location => [
      `${fileKey(location.uri.fsPath)}:${location.range.start.line}:${location.range.start.character}:${location.range.end.line}:${location.range.end.character}`,
      location])).values()];
  }

  /** Resolve generated types through their language service instead of guessing from caller text. */
  async function declarations(document: vscode.TextDocument, position: vscode.Position,
    token: vscode.CancellationToken): Promise<vscode.Location[]> {
    const offset = document.offsetAt(position);
    const include = ['cpp', 'c'].includes(document.languageId) && cppIncludeAt(document.getText(), offset);
    const resolver = await navigator(document, token, !include);
    if (document.languageId === 'serializer') {
      return locations(await resolver.schema(document.uri.fsPath, offset, false), token);
    }
    if (include) {
      return locations(await resolver.cppInclude(document.uri.fsPath, offset, false), token);
    }
    // A generated declaration itself can be resolved without a language service.
    const local = await resolver.generatedDeclaration(document.uri.fsPath, offset);
    if (local.length) { return locations(local, token); }
    if (token.isCancellationRequested) { return []; }
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
        targets.push(...await resolver.generatedDeclaration(uri.fsPath, header.offsetAt(selection.start), header.offsetAt(selection.end)));
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

  const selector = navigationLanguages.map(language => ({ language, scheme: 'file' }));
  const declarationProvider = provider(declarations);
  context.subscriptions.push(vscode.languages.registerDeclarationProvider(selector, {
    provideDeclaration: declarationProvider
  }), vscode.languages.registerDefinitionProvider(
    selector.filter(item => ['serializer', 'cpp', 'c'].includes(item.language)),
    { provideDefinition: provider(definitions) }));

  /** Open one available destination, asking only when several existing outputs apply. */
  async function show(destinations: vscode.Location[]): Promise<void> {
    const selected = destinations.length === 1 ? destinations[0] : (await vscode.window.showQuickPick(
      destinations.map(location => ({ label: vscode.workspace.asRelativePath(location.uri),
        description: `${location.uri.fsPath}:${location.range.start.line + 1}`, location })),
      { placeHolder: 'Choose an available Serializer destination' }))?.location;
    if (selected) { await vscode.window.showTextDocument(selected.uri, { selection: selected.range }); }
  }

  /** Use the clicked file's URI for Explorer/tab actions, or the active schema from the palette. */
  async function openGeneratedHeader(uri?: vscode.Uri, allLanguages = false): Promise<void> {
    const schema = uri ?? vscode.window.activeTextEditor?.document.uri;
    if (!schema || schema.scheme !== 'file' || !schema.fsPath.endsWith('.serializer')) { return; }
    const cancellation = new vscode.CancellationTokenSource();
    try {
      const document = await vscode.workspace.openTextDocument(schema);
      const resolver = await navigator(document, cancellation.token);
      const targets = await locations(await (allLanguages ? resolver.implementations(schema.fsPath)
        : resolver.headers(schema.fsPath)), cancellation.token);
      if (targets.length) { await show(targets); }
    } catch { /* Opening available output never offers or invokes generation. */ }
    finally { cancellation.dispose(); }
  }

  context.subscriptions.push(vscode.commands.registerCommand('serializer.goToSchemaDeclaration', async () => {
    const editor = vscode.window.activeTextEditor;
    if (!editor || !navigationLanguages.includes(editor.document.languageId)) { return; }
    const cancellation = new vscode.CancellationTokenSource();
    try {
      const targets = await declarationProvider(editor.document, editor.selection.start ?? editor.selection.active, cancellation.token);
      if (targets.length) { await show(targets); }
    } finally { cancellation.dispose(); }
  }), vscode.commands.registerCommand('serializer.openGeneratedHeader', openGeneratedHeader),
  vscode.commands.registerCommand('serializer.goToImplementation', (uri?: vscode.Uri) => openGeneratedHeader(uri, true)),
  { dispose: () => cache.clear() });
}
