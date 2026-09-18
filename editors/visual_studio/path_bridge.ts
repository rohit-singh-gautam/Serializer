declare const hostResolve: (directory: string, name: string) => string;
export const sep = '\\';

/** Normalize Windows paths without requiring Node.js in the Visual Studio process. */
export function normalize(value: string): string {
  const input = value.replace(/\//g, '\\');
  const prefix = /^(?:[A-Za-z]:\\|\\\\[^\\]+\\[^\\]+\\?|\\)/.exec(input)?.[0] ?? '';
  const parts: string[] = [];
  for (const part of input.slice(prefix.length).split('\\')) {
    if (!part || part === '.') { continue; }
    if (part === '..' && parts.length && parts[parts.length - 1] !== '..') { parts.pop(); }
    else if (part !== '..' || !prefix) { parts.push(part); }
  }
  return prefix + parts.join('\\') || '.';
}

/** Join an absolute owner directory and a relative include through the native filesystem API. */
export function resolve(directory: string, name: string): string { return hostResolve(directory, name); }

/** Return a Windows path's parent, preserving drive and UNC roots. */
export function dirname(value: string): string {
  const normalized = normalize(value);
  const index = normalized.lastIndexOf('\\');
  return index < 0 ? '.' : normalized.slice(0, index + (/^[A-Za-z]:\\/.test(normalized) && index === 2 ? 1 : 0));
}

/** Extract a leaf with an optional exact extension suffix removed. */
export function basename(value: string, suffix = ''): string {
  const name = value.replace(/\\/g, '/').split('/').pop()!;
  return suffix && name.endsWith(suffix) ? name.slice(0, -suffix.length) : name;
}

/** Match Node's extension behavior for dotfiles and schema include validation. */
export function extname(value: string): string {
  const name = basename(value);
  const index = name.lastIndexOf('.');
  return index > 0 && name !== '..' ? name.slice(index) : '';
}

/** Recognize rooted Windows and UNC dependency paths. */
export function isAbsolute(value: string): boolean { return /^(?:[A-Za-z]:[\\/]|[\\/])/.test(value); }
export const posix = { basename, extname };
