// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import {IdentityPackageArchive as A, IdentityXmlRegistry as Model, SerializerPackage as P,
        SerializerCompression as C} from './identity.mjs';

// Compile and execute caller callbacks against exported strict TypeScript declarations.
export async function typedLoad(bytes: Uint8Array, budget: P.Budget, provider: C.Provider) {
  return await A.load(bytes, {budget, compressionProvider: provider, codecs: {
    registries: {decode(text, limits, context) {
      const part: string = context.part;
      const id: string = context.relationshipId;
      if (!part || !id || context.budget !== budget) throw new Error('Typed callback context');
      Model.decodeXml(text, limits);
      return Model.decodeXml(text, limits);
    }}
  }});
}
