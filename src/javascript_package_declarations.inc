// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagejs(
declare const SerializerPackageErrorBase: typeof Error;
/** Bounded asynchronous ZIP/OPC; Node imports occur only through explicit provider/host factories. */
export declare namespace SerializerPackage {
  type Code = 'invalid_options' | 'invalid_archive' | 'integrity' | 'unsupported_feature' | 'unavailable' | 'resource_limit' | 'invalid_metadata' | 'invalid_part_name' | 'invalid_relationship' | 'missing_part' | 'content_type_mismatch' | 'occurrence' | 'unknown_part' | 'unknown_relationship' | 'publication_uncertain';
  const Code: {readonly InvalidOptions:'invalid_options';readonly InvalidArchive:'invalid_archive';readonly Integrity:'integrity';readonly Unsupported:'unsupported_feature';readonly Unavailable:'unavailable';readonly ResourceLimit:'resource_limit';readonly InvalidMetadata:'invalid_metadata';readonly InvalidPartName:'invalid_part_name';readonly InvalidRelationship:'invalid_relationship';readonly MissingPart:'missing_part';readonly ContentType:'content_type_mismatch';readonly Occurrence:'occurrence';readonly UnknownPart:'unknown_part';readonly UnknownRelationship:'unknown_relationship';readonly PublicationUncertain:'publication_uncertain'};
  class Error extends SerializerPackageErrorBase { readonly code: Code; readonly part: string; readonly relationshipId: string; readonly fieldPath?: string; constructor(code: Code, message: string, part?: string, relationshipId?: string); }
  interface LimitOptions {
    maxArchiveBytes?: number; maxIoBytes?: number; maxMetadataBytes?: number; maxPartBytes?: number;
    maxTotalExpandedBytes?: number; maxBufferBytes?: number; maxEntries?: number; maxRelationships?: number;
    maxDecoderAllocationBytes?: number; maxWorkUnits?: number; maxReferences?: number; maxPreservedBytes?: number;
    checkCancelled?: (() => void) | null; clock?: (() => number) | null; deadline?: number | null;
  }
  class Limits {
    readonly maxArchiveBytes: number; readonly maxIoBytes: number; readonly maxMetadataBytes: number; readonly maxPartBytes: number;
    readonly maxTotalExpandedBytes: number; readonly maxBufferBytes: number; readonly maxEntries: number; readonly maxRelationships: number;
    readonly maxDecoderAllocationBytes: number; readonly maxWorkUnits: number; readonly maxReferences: number; readonly maxPreservedBytes: number;
    readonly checkCancelled: (() => void) | null; readonly clock: (() => number) | null; readonly deadline: number | null;
    constructor(options?: LimitOptions);
  }
  type Resource = 'ioBytes' | 'metadataBytes' | 'expandedBytes' | 'entries' | 'relationships' | 'decoderAllocationBytes' | 'workUnits' | 'references' | 'preservedBytes';
  class Budget { readonly limits: Limits; readonly used: {readonly [K in Resource]: number}; readonly bufferBytes: number; constructor(limits?: Limits); check(): void; consume(resource: Resource, count: number, part?: string): void; remaining(resource: Resource): number; reserve(count: number, part?: string): () => void; }
  interface RandomSource { readonly size: number; readAt(offsetBytes: number, maxBytes: number): Uint8Array | globalThis.PromiseLike<Uint8Array>; }
  interface SequentialSource { read(maxBytes: number): Uint8Array | null | globalThis.PromiseLike<Uint8Array | null>; }
  interface Sink { write(bytes: Uint8Array): void | globalThis.PromiseLike<void>; }
  type Source = Uint8Array | RandomSource | SequentialSource | string;
  interface Host { readonly open: (source: string | SequentialSource, options: Options, budget: Budget) => globalThis.Promise<RandomSource>; readonly save: (path: string, bytes: Uint8Array, budget: Budget, fault?: ((stage: string) => void) | null) => globalThis.Promise<void>; }
  interface Options { profiles?: Readonly<Record<string,'stored'|'deflate'>>; limits?: Limits | LimitOptions; budget?: Budget; compressionProvider?: SerializerCompression.Provider | null; host?: Host; spoolDirectory?: string; maxSpoolBytes?: number; forceZip64?: boolean; descriptor?: 'none' | 'signed' | 'unsigned'; }
  interface LoadOptions<T> extends Options { policy?: ExtensionPolicy; validate?: (candidate: T, budget: Budget) => void | globalThis.PromiseLike<void>; }
  interface SaveOptions<T> extends LoadOptions<T> { host: Host; fault?: (stage: string) => void; }
  interface ZipEntry { readonly name: string; readonly method: 'stored' | 'deflate'; readonly flags: number; readonly compressedBytes: number; readonly expandedBytes: number; readonly crc32: number; readonly headerOffsetBytes: number; readonly dataOffsetBytes: number; readonly zip64Sizes: boolean; readonly creatorSystem: number; readonly externalAttributes: number; }
  interface ZipInput { name: string; data: Uint8Array; compression?: 'stored' | 'deflate'; level?: number; forceZip64?: boolean; }
  class ZipReader { readonly entries: readonly ZipEntry[]; static open(source: Source, options?: Options): globalThis.Promise<ZipReader>; read(name: string): globalThis.Promise<Uint8Array>; close(): globalThis.Promise<void>; }
  class ZipWriter { constructor(options?: Options); add(entry: ZipInput): globalThis.Promise<void>; finish(): globalThis.Promise<Uint8Array>; writeTo(sink: Sink): globalThis.Promise<void>; close(): void; }
  class BinaryPart { data: Uint8Array; name: string; contentType: string; sourceIndex: number; compression: 'stored' | 'deflate'; constructor(data: Uint8Array, name: string, contentType: string, sourceIndex?: number, compression?: 'stored' | 'deflate'); }
  class ModelPart<T> { value: T; name: string; contentType: string; sourceIndex: number; constructor(value: T, name: string, contentType: string, sourceIndex?: number); }
  class Relationship { source: string; id: string; type: string; target: string; mode: 'internal' | 'external'; constructor(source: string, id: string, type: string, target: string, mode?: 'internal' | 'external'); }
  class Extensions { parts: readonly BinaryPart[]; relationships: readonly Relationship[]; constructor(parts?: readonly BinaryPart[], relationships?: readonly Relationship[]); }
  interface PartMetadata { readonly name: string; readonly contentType: string; readonly entry: ZipEntry; }
  interface ExtensionOptions { parts?: 'reject' | 'skip' | 'preserve'; relationships?: 'reject' | 'skip' | 'preserve'; isRequiredPart?: ((part: PartMetadata) => boolean) | null; isRequiredRelationship?: ((link: Relationship) => boolean) | null; }
  class ExtensionPolicy { readonly parts: 'reject' | 'skip' | 'preserve'; readonly relationships: 'reject' | 'skip' | 'preserve'; readonly isRequiredPart: ((part: PartMetadata) => boolean) | null; readonly isRequiredRelationship: ((link: Relationship) => boolean) | null; constructor(options?: ExtensionOptions); }
  class Result<T> { value: T; extensions: Extensions; constructor(value: T, extensions: Extensions); }
  function nodeHost(): globalThis.Promise<Host>;
  function encode(parts: readonly BinaryPart[], relationships: readonly Relationship[], options?: Options): globalThis.Promise<Uint8Array>;
}
)packagejs"
