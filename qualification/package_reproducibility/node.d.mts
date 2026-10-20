// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Declare only the actual Node APIs used by the strict standalone producer.
declare const process: {argv:string[];versions:{zlib:string}};
declare module 'node:fs' {
  export function readFileSync(path:string,encoding:'utf8'):string;
  export function writeFileSync(path:string,data:Uint8Array):void;
}
declare module 'node:path' {export function join(...parts:string[]):string;}
