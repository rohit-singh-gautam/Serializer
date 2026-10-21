// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Only the actual Node host APIs used by this typed executable consumer.
declare const process: { argv: string[] };
declare module 'node:fs' {
  export function readFileSync(path: string): Uint8Array;
  export function writeFileSync(path: string, data: Uint8Array | string): void;
}
declare module 'node:path' { export function join(...parts: string[]): string; }
declare module 'node:perf_hooks' { export const performance: { now(): number }; }
