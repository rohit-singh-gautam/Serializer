// Narrow declarations for the Node APIs used by these dependency-free examples.
// Applications can instead use the complete @types/node package.
declare const process: { argv: string[] };
declare module 'node:fs' {
  export function readFileSync(path: string): Uint8Array;
  export function readFileSync(path: string, encoding: 'utf8'): string;
  export function writeFileSync(path: string, data: Uint8Array | string): void;
  export function mkdirSync(path: string, options: {recursive: true}): string | undefined;
}
declare module 'node:path' {
  export function join(...parts: string[]): string;
}
declare module 'node:assert/strict' {
  const assert: { deepEqual(actual: unknown, expected: unknown, message?: string): void };
  export default assert;
}
declare module 'node:perf_hooks' {
  export const performance: { now(): number };
}
