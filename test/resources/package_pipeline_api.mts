// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import {PipelineArchive as A,SerializerPackage as P,SerializerCompression as C} from './schema.mjs';

// Compile each additive native envelope selector against the generated public declaration surface.
export async function verify(source: Uint8Array, codec: 'xml'|'json'|'binary_none', provider: C.Provider): Promise<Uint8Array> {
  const profiles: P.Options['profiles']={main:{codec,compression:'stored'}};
  const loaded=await A.load(source,{profiles,compressionProvider:provider});
  const input=new A.Input({main:loaded.value.main,fixed:loaded.value.fixed,links:loaded.value.links,extensions:loaded.extensions});
  return A.encode(input,{profiles,compressionProvider:provider});
}
