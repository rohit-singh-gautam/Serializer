// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// TypeScript declarations for the shared emitted JavaScript compression runtime.
R"compressjs(
declare const SerializerCompressionErrorBase: typeof Error;
/** Owning asynchronous compression; nodeProvider() is the sole optional Node module load. */
export declare namespace SerializerCompression {
  const Format: Readonly<{None: 'none'; Gzip: 'gzip'; Zlib: 'zlib'; Deflate: 'deflate'; Zstd: 'zstd'; Lz4: 'lz4'}>;
  type Format = typeof Format[keyof typeof Format];
  const Code: Readonly<{Unavailable: 'unavailable'; InvalidOptions: 'invalid_options'; InvalidData: 'invalid_data'; ResourceLimit: 'resource_limit'}>;
  type Code = typeof Code[keyof typeof Code];
  class Error extends SerializerCompressionErrorBase { readonly code: Code; constructor(code: Code, message: string); }
  interface Provider<Name extends string = 'node:zlib'> { readonly name: Name; }
  interface ZstdProvider extends Provider<'node:zlib:zstd'> {}
  interface Lz4Provider extends Provider<'lz4js'> {}
  type AnyProvider = Provider | ZstdProvider | Lz4Provider;
  interface EncodePolicy<P extends AnyProvider = Provider> {
    readonly format?: Format; readonly maxInputBytes?: number; readonly maxOutputBytes?: number;
    readonly level?: number; readonly provider?: P | null; readonly checkCancelled?: (() => void) | null;
  }
  interface DecodePolicy<P extends AnyProvider = Provider> {
    readonly format?: Format; readonly maxInputBytes?: number; readonly maxOutputBytes?: number;
    readonly maxWindowBytes?: number; readonly provider?: P | null; readonly checkCancelled?: (() => void) | null;
  }
  class EncodeOptions<P extends AnyProvider = Provider> {
    readonly format: Format; readonly maxInputBytes: number; readonly maxOutputBytes: number;
    readonly level: number; readonly provider: P | null; readonly checkCancelled: (() => void) | null;
    constructor(policy?: EncodePolicy<P>);
  }
  class DecodeOptions<P extends AnyProvider = Provider> {
    readonly format: Format; readonly maxInputBytes: number; readonly maxOutputBytes: number;
    readonly maxWindowBytes: number; readonly provider: P | null; readonly checkCancelled: (() => void) | null;
    constructor(policy?: DecodePolicy<P>);
  }
  /** A source returns nonempty bytes within the request or null at EOF; storage remains stable until delivered. */
  interface Source { read(maxBytes: number): Uint8Array | null | globalThis.PromiseLike<Uint8Array | null>; }
  /** A sink consumes the complete independent chunk or throws/rejects; no close/flush is called. */
  interface Sink { write(bytes: Uint8Array): void | globalThis.PromiseLike<void>; }
  function available(format: Format, provider?: AnyProvider | null): boolean;
  function nodeProvider(): globalThis.Promise<Provider>;
  function zstdProvider(): globalThis.Promise<ZstdProvider>;
  interface Lz4BlockModule {
    compressBlock(input: Uint8Array, output: Uint8Array, offset: number, length: number, table: number[]): number;
    decompressBlock(input: Uint8Array, output: Uint8Array, offset: number, length: number, outputOffset: number): number;
  }
  function lz4Provider(module: Lz4BlockModule): Lz4Provider;
  function compress(input: Uint8Array, options?: EncodeOptions<AnyProvider>): globalThis.Promise<Uint8Array>;
  function decompress(input: Uint8Array, options?: DecodeOptions<AnyProvider>): globalThis.Promise<Uint8Array>;
  function encode(encoder: () => Uint8Array | globalThis.PromiseLike<Uint8Array>, options?: EncodeOptions<AnyProvider>): globalThis.Promise<Uint8Array>;
  function decode<T>(input: Uint8Array, decoder: (plain: Uint8Array) => T | globalThis.PromiseLike<T>, options?: DecodeOptions<AnyProvider>): globalThis.Promise<T>;
  function decompressFrom(source: Source, options?: DecodeOptions<AnyProvider>): globalThis.Promise<Uint8Array>;
  class Output {
    constructor(sink: Sink, options?: EncodeOptions<AnyProvider>);
    write(input: Uint8Array): globalThis.Promise<void>; finish(): globalThis.Promise<void>; flush(): globalThis.Promise<void>; close(): void;
  }
  class Input {
    constructor(source: Source, options?: DecodeOptions<AnyProvider>);
    read(maxBytes?: number): globalThis.Promise<Uint8Array | null>; finish(): globalThis.Promise<void>; flush(): globalThis.Promise<void>; close(): void;
  }
}
)compressjs"
