// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
R"jsEngineTypes(
export declare class ManagedError extends Error { constructor(message: string, cause?: unknown); }
export declare class ManagedIndeterminate extends ManagedError {}
export declare class ManagedDocument { private readonly _native: void; readonly high: bigint; readonly low: bigint; constructor(high: bigint, low: bigint); }
export type ManagedId = number | bigint;
export type ManagedConstructor<T> = new () => T;
export interface ManagedLimits {
  maxHistoryEntries?: number; maxHistoryBytes?: number; maxReceipts?: number;
  maxReceiptBytes?: number; maxFrameBytes?: number; maxJournalBytes?: number; maxJournalRecords?: number;
}
export interface ManagedSchemaLimits { maxSnapshotBytes?: number; maxEntities?: number; }
export declare class ManagedOptions implements Required<ManagedLimits> {
  private readonly _native: void;
  readonly maxHistoryEntries: number; readonly maxHistoryBytes: number; readonly maxReceipts: number;
  readonly maxReceiptBytes: number; readonly maxFrameBytes: number; readonly maxJournalBytes: number;
  readonly maxJournalRecords: number; constructor(limits?: ManagedLimits);
}
export declare class ManagedToken<T, Id extends ManagedId = ManagedId> {
  private readonly _target: (value: T) => T;
  readonly document: ManagedDocument; readonly classId: bigint; readonly objectId: Id;
  constructor(document: ManagedDocument, classId: bigint, objectId: Id);
}
export declare class ManagedSchema<T, Id extends ManagedId = ManagedId> {
  private constructor(); private readonly _target: (value: T) => T; private readonly _id: Id;
  readonly idBytes: 4 | 8; readonly schemaId: string;
  importSnapshot(document: ManagedDocument, epoch: bigint, revision: bigint, data: Uint8Array): ManagedSnapshot<T, Id>;
}
export declare class ManagedSnapshot<T, Id extends ManagedId = ManagedId> {
  private constructor(); private readonly _target: (value: T) => T;
  readonly document: ManagedDocument; readonly epoch: bigint; readonly revision: bigint;
  copyValue(): T; bytes(): Uint8Array;
  resolve<U>(token: ManagedToken<U, Id>, target: ManagedConstructor<U>): U;
}
export declare class ManagedEdit<T, Id extends ManagedId = ManagedId> {
  private readonly _native: void;
  private constructor(); readonly root: T;
  token<U extends object>(target: U): ManagedToken<U, Id>;
}
export declare class ManagedStore<T, Id extends ManagedId = ManagedId> {
  private readonly _target: (value: T) => T;
  constructor(schema: ManagedSchema<T, Id>, initial: T, document: ManagedDocument, options?: ManagedOptions,
    initialize?: (edit: ManagedEdit<T, Id>) => undefined);
  read(): ManagedSnapshot<T, Id>; allocatedId(): Id;
  transact(callback: (edit: ManagedEdit<T, Id>) => undefined,
    initialize?: (edit: ManagedEdit<T, Id>) => undefined): ManagedSnapshot<T, Id>;
  undo(): ManagedSnapshot<T, Id>; redo(): ManagedSnapshot<T, Id>;
  createJournal(path: string): Promise<void>; close(): Promise<void>;
  static openJournal<T, Id extends ManagedId>(schema: ManagedSchema<T, Id>, path: string,
    options?: ManagedOptions): Promise<ManagedStore<T, Id>>;
}
export declare class ManagedProposal<T, Id extends ManagedId = ManagedId> {
  private readonly _target: (value: T) => T; private readonly _id: Id;
  readonly clientId: string; readonly requestId: string; readonly document: ManagedDocument;
  readonly schemaId: string; readonly baseEpoch: bigint;
  constructor(clientId: string, requestId: string, document: ManagedDocument, schemaId: string,
    baseEpoch: bigint, bytes: Uint8Array); bytes(): Uint8Array;
}
export declare class ManagedReceipt<T, Id extends ManagedId = ManagedId> {
  private readonly _native: void;
  readonly proposal: ManagedProposal<T, Id>; readonly status: 'accepted' | 'conflict';
  readonly snapshot: ManagedSnapshot<T, Id>;
  constructor(proposal: ManagedProposal<T, Id>, status: 'accepted' | 'conflict', snapshot: ManagedSnapshot<T, Id>);
}
export declare class ManagedAuthority<T, Id extends ManagedId = ManagedId> {
  private readonly _native: void;
  constructor(store: ManagedStore<T, Id>); readonly store: ManagedStore<T, Id>;
  accept(proposal: ManagedProposal<T, Id>): ManagedReceipt<T, Id>;
}
export declare class ManagedClient<T, Id extends ManagedId = ManagedId> {
  private readonly _native: void;
  constructor(schema: ManagedSchema<T, Id>, baseline: ManagedSnapshot<T, Id>, clientId: string);
  read(): ManagedSnapshot<T, Id>;
  propose(requestId: string, callback: (value: T) => undefined): ManagedProposal<T, Id>;
  retry(): ManagedProposal<T, Id>; acknowledge(receipt: ManagedReceipt<T, Id>): void;
}
export declare function managedReference<T, Id extends ManagedId>(token: ManagedToken<T, Id>): ManagedReference<T, Id>;
export declare function managedToken<T, Id extends ManagedId>(reference: ManagedReference<T, Id>): ManagedToken<T, Id>;
)jsEngineTypes"
