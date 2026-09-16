import * as path from 'node:path';
import type { CodeModel } from 'vscode-cmake-tools';

export interface HeaderCandidate {
  path: string;
  targets: string[];
}

export interface SourceIncludes {
  target: string;
  directories: string[];
}

/** Normalize file identities without folding case on case-sensitive hosts. */
export function fileKey(file: string): string {
  const normalized = path.normalize(file);
  return process.platform === 'win32' ? normalized.toLowerCase() : normalized;
}

/** Read a literal include directive; macro includes need the C++ language service. */
export function parseInclude(line: string): { name: string; quoted: boolean } | undefined {
  const match = /^\s*#\s*include\s*(?:"([^"\r\n]+)"|<([^>\r\n]+)>)/.exec(line);
  return match ? { name: match[1] ?? match[2], quoted: match[1] !== undefined } : undefined;
}

/** Choose exactly one configuration so headers from another profile cannot leak in. */
export function activeConfiguration(model: CodeModel.Content | undefined,
  name: string | undefined): CodeModel.Configuration {
  if (!model?.configurations.length) {
    throw new Error('Configure this project with CMake Tools first, then retry.');
  }
  const exact = model.configurations.find(configuration => configuration.name === name);
  if (exact) {
    return exact;
  }
  if (model.configurations.length === 1 && !model.configurations[0].name) {
    return model.configurations[0];
  }
  throw new Error('Select a matching CMake build configuration or preset, then retry.');
}

/** Collect generated headers from CMake, retaining all target owners of each path. */
export function generatedHeaders(configuration: CodeModel.Configuration,
  name?: string): HeaderCandidate[] {
  const headers = new Map<string, HeaderCandidate>();
  for (const project of configuration.projects) {
    for (const target of project.targets) {
      for (const group of target.fileGroups ?? []) {
        if (!group.isGenerated) {
          continue;
        }
        for (const source of group.sources) {
          if (!/\.(hpp|h|hxx|hh)$/i.test(source)) {
            continue;
          }
          const file = path.resolve(target.sourceDirectory ?? project.sourceDirectory, source);
          if (name && fileKey(path.basename(file)) !== fileKey(path.basename(name))) {
            continue;
          }
          const key = fileKey(file);
          const entry = headers.get(key) ?? { path: file, targets: [] };
          if (!entry.targets.includes(target.name)) {
            entry.targets.push(target.name);
          }
          headers.set(key, entry);
        }
      }
    }
  }
  return [...headers.values()];
}

/** Read only compile groups containing this file; never merge include paths across targets. */
export function sourceIncludes(configuration: CodeModel.Configuration,
  source: string): SourceIncludes[] {
  const result: SourceIncludes[] = [];
  for (const project of configuration.projects) {
    for (const target of project.targets) {
      const base = target.sourceDirectory ?? project.sourceDirectory;
      for (const group of target.fileGroups ?? []) {
        if (!group.language || !group.sources.some(file => fileKey(path.resolve(base, file)) === fileKey(source))) {
          continue;
        }
        result.push({ target: target.name,
          directories: (group.includePath ?? []).map(include => path.resolve(base, include.path)) });
      }
    }
  }
  return result;
}

/** Preserve compiler include search order, including the local directory for quoted includes. */
export function includeCandidates(source: string, include: { name: string; quoted: boolean },
  directories: string[]): string[] {
  const roots = include.quoted ? [path.dirname(source), ...directories] : directories;
  return [...new Set(roots.map(root => path.resolve(root, include.name)))];
}
