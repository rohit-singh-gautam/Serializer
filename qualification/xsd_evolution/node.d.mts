// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
declare const process: {argv: string[]};
declare module 'node:fs' {
  export function readFileSync(path: string): Uint8Array;
  export function readFileSync(path: string, encoding: 'utf8'): string;
  export function writeFileSync(path: string, data: string | Uint8Array): void;
  export function mkdirSync(path: string, options: {recursive: true}): string | undefined;
}
declare module 'node:path' {
  export function join(...paths: string[]): string;
}
