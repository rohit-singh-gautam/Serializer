// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import {RelocationArchive as A,SerializerPackage as P} from './schema.mjs';

/** Compile and execute public owning operations through a separate strict TypeScript consumer. */
export async function verify(input:A.Input,options:A.Options,path:string,host:P.Host):Promise<void>{
  const bytes:Uint8Array=await A.encode(input,options);
  const loaded:P.Result<A.Value>=await A.load(bytes,{...options,policy:new P.ExtensionPolicy({parts:'preserve',relationships:'preserve'})});
  if(loaded.value.first?.id!==11)throw Error('typed canonical payload changed');
  await A.save(path,input,{...options,host});
}
