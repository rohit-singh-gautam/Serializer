import { Navigator, NavigationFiles } from '../vscode/src/navigator';
import { dependencyFiles, indexSource, spanAt } from '../vscode/src/navigation_model';
import { outputLanguage } from '../vscode/src/generated_navigation';
import { fileKey } from '../vscode/src/model';
import * as path from './path_bridge';

declare const hostRead: (file: string) => string | null;
declare const hostFiles: () => string;
declare const hostCancelled: () => boolean;

/** Discover existing outputs using the same dependency ownership and legacy names as VS Code. */
async function files(): Promise<NavigationFiles> {
  const candidates: string[] = JSON.parse(hostFiles());
  const schemas = candidates.filter(file => file.endsWith('.serializer'));
  const stems = new Set(schemas.map(file => fileKey(path.basename(file, '.serializer'))));
  const outputs = new Set<string>();
  const dependencies = new Map<string, string[]>();
  for (const candidate of candidates) {
    if (hostCancelled()) { break; }
    if (candidate.endsWith('.d')) {
      const text = hostRead(candidate);
      if (text === null) { continue; }
      const dependency = dependencyFiles(text);
      if (!dependency.schemas.length) { continue; }
      for (const output of dependency.targets.filter(outputLanguage)) {
        outputs.add(output);
        const entries = dependencies.get(fileKey(output)) ?? [];
        if (!entries.some(entry => fileKey(entry) === fileKey(dependency.schemas[0]))) { entries.push(dependency.schemas[0]); }
        dependencies.set(fileKey(output), entries);
      }
    } else if (outputLanguage(candidate) && stems.has(fileKey(path.basename(candidate, path.extname(candidate))))) {
      outputs.add(candidate);
    }
  }
  return { schemas, outputs: [...outputs], headers: [...outputs].filter(file => /\.(hpp|h|hh|hxx)$/i.test(file)), dependencies };
}

/** Expose exact schema token ranges to Visual Studio's Ctrl+click provider. */
export function reference(file: string, text: string, offset: number): string {
  if (!file.endsWith('.serializer')) { return 'null'; }
  const index = indexSource(text, true);
  return JSON.stringify(spanAt([...index.references, ...index.includes], offset) ?? null);
}

/** Reuse the tested resolver; all file reads remain passive and use captured live buffers first. */
export async function navigate(file: string, offset: number, definition: boolean): Promise<string> {
  const resolver = new Navigator({ read: async file => hostRead(file) ?? undefined,
    files, includeDirectories: [], cancelled: hostCancelled });
  return JSON.stringify(file.endsWith('.serializer') ? await resolver.schema(file, offset, definition)
    : await resolver.generatedDeclaration(file, offset));
}
