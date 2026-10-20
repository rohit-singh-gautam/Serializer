// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Declare only the Node APIs used by the maintained strict TypeScript driver.
declare const process: {
  argv: string[];
  version: string;
  hrtime: { bigint(): bigint };
};
declare module 'node:fs' {
  export function readFileSync(path: string): Uint8Array;
  export function writeFileSync(path: string, value: Uint8Array | string): void;
}
