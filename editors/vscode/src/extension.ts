import * as vscode from 'vscode';
import * as path from 'node:path';
import { getCMakeToolsApi, Version } from 'vscode-cmake-tools';
import type { Project, CodeModel } from 'vscode-cmake-tools';
import { activeConfiguration, generatedHeaders, includeCandidates, parseInclude,
  sourceIncludes } from './model';

interface ProjectContext {
  folder: vscode.WorkspaceFolder;
  project: Project;
  configuration: CodeModel.Configuration;
}

/** Register commands lazily; opening a file never configures or builds a project. */
export function activate(context: vscode.ExtensionContext): void {
  const output = vscode.window.createOutputChannel('Serializer');
  const running = new Set<string>();
  context.subscriptions.push(output);

  /** Report failures without claiming generation succeeded or hiding compiler output. */
  function command(name: string, action: (uri?: vscode.Uri) => Promise<unknown>): void {
    context.subscriptions.push(vscode.commands.registerCommand(name, async (uri?: vscode.Uri) => {
      try {
        requireTrust();
        return await action(uri);
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        output.appendLine(message);
        void vscode.window.showErrorMessage(`Serializer: ${message}`);
        return false;
      }
    }));
  }

  /** Resolve the document's project, including multi-root workspaces and nested CMake projects. */
  async function projectContext(uri?: vscode.Uri): Promise<ProjectContext> {
    const documentUri = uri ?? vscode.window.activeTextEditor?.document.uri;
    let folder = documentUri ? vscode.workspace.getWorkspaceFolder(documentUri) : undefined;
    if (!folder) {
      const folders = vscode.workspace.workspaceFolders ?? [];
      folder = folders.length === 1 ? folders[0] : await vscode.window.showWorkspaceFolderPick();
    }
    if (!folder) {
      throw new Error('Open a local or remote CMake workspace folder first.');
    }
    const api = await getCMakeToolsApi(Version.v1);
    if (!api) {
      throw new Error('Install or enable Microsoft CMake Tools (ms-vscode.cmake-tools), then configure your project. Highlighting works without it.');
    }
    const project = await api.getProject(documentUri && vscode.workspace.getWorkspaceFolder(documentUri) === folder
      ? documentUri : folder.uri);
    if (!project) {
      throw new Error('CMake Tools has no project for this file. Open its CMake source folder and configure it.');
    }
    const configuration = activeConfiguration(project.codeModel, await project.getActiveBuildType());
    return { folder, project, configuration };
  }

  /** Run the registered target through CMake Tools, preserving its preset and build environment. */
  async function generate(uri?: vscode.Uri): Promise<boolean> {
    const selected = await projectContext(uri);
    const key = selected.folder.uri.toString();
    if (running.has(key)) {
      throw new Error('Header generation is already running for this workspace folder.');
    }
    const target = vscode.workspace.getConfiguration('serializer', selected.folder.uri)
      .get<string>('headersTarget', 'serializer_generated_headers').trim();
    const targets = selected.configuration.projects.flatMap(project => project.targets);
    if (!target || !targets.some(candidate => candidate.name === target)) {
      throw new Error(`CMake target "${target}" is unavailable. Add serializer_generate(TARGET ... SCHEMAS ...), reconfigure, and check serializer.headersTarget.`);
    }
    if (typeof selected.project.buildWithResult !== 'function') {
      throw new Error('Update Microsoft CMake Tools to a version providing buildWithResult.');
    }
    running.add(key);
    try {
      // CMake reads saved inputs. Do not silently build an older version of the edited schema.
      for (const document of vscode.workspace.textDocuments) {
        if (document.isDirty && vscode.workspace.getWorkspaceFolder(document.uri) === selected.folder
          && /\.(serializer|ini|cmake)$|[\\/]CMakeLists\.txt$/i.test(document.uri.fsPath)) {
          if (!await document.save()) {
            throw new Error(`Could not save ${document.uri.fsPath}; generation was not started.`);
          }
        }
      }
      requireTrust();
      return await vscode.window.withProgress({ location: vscode.ProgressLocation.Notification,
        title: `Serializer: generating ${target}`, cancellable: true }, async (_progress, token) => {
        output.appendLine(`Building ${target} in ${await selected.project.getBuildDirectory()} (${selected.configuration.name || 'default'})`);
        const result = await selected.project.buildWithResult([target], token);
        if (result.stdout) { output.appendLine(result.stdout); }
        if (result.stderr) { output.appendLine(result.stderr); }
        if (token.isCancellationRequested) {
          output.appendLine('Header generation cancelled.');
          return false;
        }
        if (result.exitCode !== 0) {
          output.show(true);
          throw new Error(`Header generation failed (exit ${result.exitCode}). See Serializer and CMake build output; check schema errors and clang-format 19+.`);
        }
        void vscode.window.showInformationMessage(`Serializer: ${target} completed.`);
        return true;
      });
    } finally {
      running.delete(key);
    }
  }

  /** Find and open a schema's generated header, asking when several profiles share its filename. */
  async function openGenerated(uri?: vscode.Uri): Promise<void> {
    const schema = uri ?? vscode.window.activeTextEditor?.document.uri;
    if (!schema || path.extname(schema.fsPath) !== '.serializer') {
      throw new Error('Open a .serializer schema first.');
    }
    const selected = await projectContext(schema);
    const name = `${path.basename(schema.fsPath, '.serializer')}.hpp`;
    const headers = generatedHeaders(selected.configuration, name);
    if (!headers.length) {
      throw new Error(`No generated ${name} is registered in this CMake configuration. Add this schema to serializer_generate and reconfigure.`);
    }
    const items = headers.map(header => ({ label: header.targets.join(', '),
      description: header.path, header }));
    const item = items.length === 1 ? items[0] : await vscode.window.showQuickPick(items, {
      placeHolder: `Choose the target/profile for ${name}`
    });
    if (!item) { return; }
    const headerUri = vscode.Uri.file(item.header.path);
    if (!await isFile(headerUri)) {
      const choice = await vscode.window.showInformationMessage(`${name} has not been generated.`, 'Generate Headers');
      if (choice !== 'Generate Headers' || !await generate(schema)) { return; }
      if (!await isFile(headerUri)) {
        throw new Error(`The selected target did not produce ${headerUri.fsPath}. Check serializer.headersTarget and the active profile.`);
      }
    }
    await vscode.window.showTextDocument(headerUri);
  }

  /** Explain actual target include paths and missing outputs without editing CMake files. */
  async function diagnose(uri?: vscode.Uri): Promise<void> {
    const editor = vscode.window.activeTextEditor;
    const source = uri ?? editor?.document.uri;
    if (!source) { throw new Error('Open a C++ source file and place the cursor on an #include.'); }
    const selected = await projectContext(source);
    const currentInclude = editor?.document.uri.toString() === source.toString()
      ? parseInclude(editor.document.lineAt(editor.selection.active.line).text) : undefined;
    const requested = currentInclude?.name ?? await vscode.window.showInputBox({
      prompt: 'Header include path to diagnose (for example account.hpp or rohit/serializer.hpp)',
      ignoreFocusOut: true
    });
    if (!requested?.trim()) { return; }
    const include = currentInclude ?? { name: requested.trim(), quoted: false };
    output.clear();
    output.appendLine(`Header: ${include.name}\nSource: ${source.fsPath}\nConfiguration: ${selected.configuration.name || 'default'}`);
    const groups = sourceIncludes(selected.configuration, source.fsPath);
    if (!groups.length) {
      output.appendLine('This file has no compile group in the active CMake model. Run this command from a .cpp file that includes it, or add the source to its CMake target and reconfigure.');
    }
    for (const group of groups) {
      output.appendLine(`\nTarget: ${group.target}`);
      let found = false;
      for (const candidate of includeCandidates(source.fsPath, include, group.directories)) {
        const exists = await isFile(vscode.Uri.file(candidate));
        output.appendLine(`  ${exists ? 'exists' : 'missing'}: ${candidate}`);
        if (exists) {
          found = true;
          // Match the compiler's first result instead of suggesting a different profile's header.
          break;
        }
      }
      output.appendLine(found
        ? 'The header exists on this target\'s search path. If IntelliSense still reports it missing, select CMake Tools as its provider and check the active configuration.'
        : 'No file found in the include paths reported by CMake for this source. Compiler implicit system paths are not inspected.');
    }
    const registered = generatedHeaders(selected.configuration, include.name);
    for (const header of registered) {
      output.appendLine(`\nRegistered generated header (${header.targets.join(', ')}): ${header.path}`);
      output.appendLine(await isFile(vscode.Uri.file(header.path))
        ? 'File exists. If absent from the consuming target\'s search paths, link the shared schema target or attach serializer_generate to that consumer.'
        : 'File is missing. Run Serializer: Generate Headers. CMake configuration alone does not create it.');
    }
    if (/^rohit[\\/]serializer\.hpp$/.test(include.name)) {
      output.appendLine('\nRuntime header: add_subdirectory(...) or find_package(Serializer CONFIG REQUIRED), then link Serializer::serializer_lib. serializer_generate supplies that linkage automatically.');
    } else if (!registered.length) {
      output.appendLine('\nNo matching generated header is registered. For a Serializer schema, use serializer_generate(TARGET my_app SCHEMAS schemas/name.serializer), then reconfigure.');
    }
    output.show(true);
    const action = await vscode.window.showInformationMessage('Serializer header report is available in Output.',
      ...(registered.length ? ['Generate Headers'] : []), 'Configure IntelliSense');
    if (action === 'Generate Headers') { await generate(source); }
    if (action === 'Configure IntelliSense') { await configureIntelliSense(source); }
  }

  /** Set only the explicitly requested folder's C/C++ provider; preserve include paths and profiles. */
  async function configureIntelliSense(uri?: vscode.Uri): Promise<void> {
    const selected = await projectContext(uri);
    if (!vscode.extensions.getExtension('ms-vscode.cpptools')) {
      throw new Error('Install Microsoft C/C++ to use its CMake Tools configuration provider. For clangd, configure its compile_commands.json instead.');
    }
    requireTrust();
    await vscode.workspace.getConfiguration('C_Cpp', selected.folder.uri).update(
      'default.configurationProvider', 'ms-vscode.cmake-tools', vscode.ConfigurationTarget.WorkspaceFolder);
    void vscode.window.showInformationMessage('Serializer: C/C++ IntelliSense now uses CMake Tools for this folder.');
  }

  command('serializer.generateHeaders', generate);
  command('serializer.openGeneratedHeader', openGenerated);
  command('serializer.diagnoseHeader', diagnose);
  command('serializer.configureIntelliSense', configureIntelliSense);

  context.subscriptions.push(vscode.languages.registerCodeActionsProvider(
    [{ language: 'cpp', scheme: 'file' }, { language: 'c', scheme: 'file' }], {
      /** Offer assistance only for missing runtime headers or headers with a matching schema. */
      async provideCodeActions(document, range, actionContext, token) {
        if (!vscode.workspace.isTrusted || !actionContext.diagnostics.some(diagnostic =>
          diagnostic.severity === vscode.DiagnosticSeverity.Error &&
          /cannot open|not found|file not found|no such file/i.test(diagnostic.message))) {
          return [];
        }
        const include = parseInclude(document.lineAt(range.start.line).text);
        const folder = vscode.workspace.getWorkspaceFolder(document.uri);
        if (!include || !folder) { return []; }
        const runtime = /^rohit[\\/]serializer\.hpp$/.test(include.name);
        if (!runtime) {
          if (!/\.hpp$/.test(include.name)) { return []; }
          const stem = path.basename(include.name, '.hpp');
          // A literal schema stem cannot change the glob's meaning.
          if (!/^[A-Za-z_0-9.-]+$/.test(stem)) { return []; }
          const schemas = await vscode.workspace.findFiles(new vscode.RelativePattern(folder, `**/${stem}.serializer`),
            '**/{node_modules,.git,out,build}/**', 1, token);
          if (!schemas.length || token.isCancellationRequested) { return []; }
        }
        const fix = new vscode.CodeAction('Serializer: Diagnose Missing Header', vscode.CodeActionKind.QuickFix);
        fix.command = { command: 'serializer.diagnoseHeader', title: fix.title, arguments: [document.uri] };
        fix.diagnostics = [...actionContext.diagnostics];
        const actions = [fix];
        if (!runtime) {
          const generateFix = new vscode.CodeAction('Serializer: Generate Headers', vscode.CodeActionKind.QuickFix);
          generateFix.command = { command: 'serializer.generateHeaders', title: generateFix.title, arguments: [document.uri] };
          actions.push(generateFix);
        }
        return actions;
      }
    }, { providedCodeActionKinds: [vscode.CodeActionKind.QuickFix] }));
}

/** Prevent command/API invocation from executing workspace build logic in Restricted Mode. */
function requireTrust(): void {
  if (!vscode.workspace.isTrusted) {
    throw new Error('Trust this workspace before using Serializer build assistance. Highlighting and snippets remain available.');
  }
}

/** Test file existence through VS Code so the same code works in a remote extension host. */
async function isFile(uri: vscode.Uri): Promise<boolean> {
  try {
    return ((await vscode.workspace.fs.stat(uri)).type & vscode.FileType.File) !== 0;
  } catch {
    return false;
  }
}
