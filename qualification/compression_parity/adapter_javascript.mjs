// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {join} from 'node:path';
import {createRequire} from 'node:module';
import {SerializerCompression as C,AccountRecord,Protocol} from './schema.mjs';
const [mode,directory,lz4Module,language='javascript']=process.argv.slice(2);
const node=await C.nodeProvider(),zstd=await C.zstdProvider(),lz4=C.lz4Provider(createRequire(import.meta.url)(lz4Module));
const formats={NONE:C.Format.None,GZIP:C.Format.Gzip,ZLIB:C.Format.Zlib,DEFLATE:C.Format.Deflate,ZSTD:C.Format.Zstd,LZ4:C.Format.Lz4};
const protocols={JSON:Protocol.JSON,BINARY_NONE:Protocol.BINARY_NONE,BINARY_INTEGER:Protocol.BINARY_INTEGER,BINARY_STRING:Protocol.BINARY_STRING};
// Admit exactly the explicit provider associated with the requested standard format.
function provider(format){return format===C.Format.Zstd?zstd:format===C.Format.Lz4?lz4:node;}
// Keep policies identical across shared fixtures and model exchanges.
function decoding(format,input=8388608,output=8388608,window=8388608){return new C.DecodeOptions({format,provider:provider(format),maxInputBytes:input,maxOutputBytes:output,maxWindowBytes:window});}
if(mode==='produce'){
  const value=new AccountRecord();value.accountId=42;value.label='shared \u03bb \u{1f642}';
  for(const [name,protocol] of Object.entries(protocols))for(const [kind,format] of Object.entries(formats)){
    const stem=language+'-'+name+'-'+kind;
    const frame=await C.encode(()=>value.encode(protocol),new C.EncodeOptions({format,provider:provider(format),level:format===C.Format.None||format===C.Format.Lz4?0:3}));
    const plain=value.encode(protocol),level=format===C.Format.None||format===C.Format.Lz4?0:3;
    const policy=(input,output)=>new C.EncodeOptions({format,provider:provider(format),level,maxInputBytes:input,maxOutputBytes:output});
    assert.deepEqual(Buffer.from(await C.encode(()=>value.encode(protocol),policy(plain.length,frame.length))),Buffer.from(frame));
    for(const [input,output] of [[plain.length-1,frame.length],[plain.length,frame.length-1]]){
      await assert.rejects(()=>C.encode(()=>value.encode(protocol),policy(input,output)),failure=>failure instanceof C.Error&&failure.code===C.Code.ResourceLimit);
    }
    writeFileSync(join(directory,stem+'.frame'),frame);writeFileSync(join(directory,stem+'.plain'),plain);
  }
  console.log(JSON.stringify({produced:24,encode_bounds:72}));
}else{
  let decisions=0,exchanges=0;
  for(const line of readFileSync(join(directory,'cases.tsv'),'utf8').trim().split('\n')){
    const [id,name,source,oracle,wanted,input,output,window]=line.trimEnd().split('\t');let observed='ok';
    try{const plain=await C.decompress(readFileSync(join(directory,source)),decoding(formats[name],+input,+output,+window));if(wanted==='ok')assert.deepEqual(Buffer.from(plain),readFileSync(join(directory,oracle)),id);}
    catch(failure){if(!(failure instanceof C.Error))throw failure;observed=failure.code===C.Code.InvalidData?'invalid_data':failure.code===C.Code.ResourceLimit?'resource_limit':String(failure.code);}
    assert.equal(observed,wanted,id);++decisions;
  }
  for(const line of readFileSync(join(directory,'exchange.tsv'),'utf8').trim().split('\n')){
    const [id,protocol,format,source,oracle]=line.trimEnd().split('\t');const frame=readFileSync(join(directory,source)),policy=decoding(formats[format]);
    const value=await C.decode(frame,bytes=>AccountRecord.decode(bytes,protocols[protocol]),policy);
    assert.equal(value.accountId,42,id);assert.equal(value.label,'shared \u03bb \u{1f642}',id);
    assert.deepEqual(Buffer.from(await C.decompress(frame,policy)),readFileSync(join(directory,oracle)),id);++exchanges;
  }
  console.log(JSON.stringify({decisions,exchanges}));
}
