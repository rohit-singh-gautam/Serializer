# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify explicitly selected Zstandard/LZ4 against independent frames and actual JS/TS SDKs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

import javascript_compression_test as core
from java_compression_optional_test import fixtures
from go_compression_optional_test import native_fixtures

ROOT = Path(__file__).resolve().parents[1]

EXTRA = r'''
// Replay the common independent exact-frame, output and history boundary decisions.
let decisions = 0;
for (const format of formats) for (const row of readFileSync(join(directory, format.toUpperCase()+'.tsv'), 'utf8').trim().split('\n')) {
  const [packedName, plainName, status, input, output, window] = row.trim().split('\t');
  const bytes = readFileSync(join(directory, packedName));
  const options = dec(format, {maxInputBytes:+input, maxOutputBytes:+output, maxWindowBytes:+window});
  if (status === 'ok') assert.deepEqual(Buffer.from(await C.decompress(bytes, options)), readFileSync(join(directory, plainName)));
  else await reject({INVALID_DATA:C.Code.InvalidData, RESOURCE_LIMIT:C.Code.ResourceLimit}[status], () => C.decompress(bytes, options));
  ++decisions;
}
// The original Node DEFLATE provider grants no implicit optional-format capability.
const ordinary = await C.nodeProvider();
assert(C.available(C.Format.Zstd, provider)); assert(!C.available(C.Format.Zstd, ordinary));
for (const absent of [null, ordinary, Object.freeze({name:'node:zlib:zstd'})]) {
  let calls=0; const checkCancelled=()=>{++calls;};
  const encode=new C.EncodeOptions({format:C.Format.Zstd,provider:absent,checkCancelled});
  const decode=new C.DecodeOptions({format:C.Format.Zstd,provider:absent,checkCancelled});
  await reject(C.Code.Unavailable,()=>C.encode(()=>{++calls;return plain;},encode));
  await reject(C.Code.Unavailable,()=>C.decode(plain,()=>{++calls;},decode));
  await reject(C.Code.Unavailable,()=>C.decompressFrom({read(){++calls;return null;}},decode));
  assert.throws(()=>new C.Output({write(){++calls;}},encode),error=>error.code===C.Code.Unavailable);
  assert.throws(()=>new C.Input({read(){++calls;return null;}},decode),error=>error.code===C.Code.Unavailable);
  assert.equal(calls,0);
}
// Levels and conservative minimum history reject before cancellation, native callbacks or source reads.
for (const level of [-131073,23,1.5,Infinity]) assert.throws(()=>enc(C.Format.Zstd,{level}),error=>error.code===C.Code.InvalidOptions);
for (const level of [-131072,0,22]) {
  const frame=await C.compress(Uint8Array.of(1),enc(C.Format.Zstd,{level}));
  assert.deepEqual(Buffer.from(await C.decompress(frame,dec(C.Format.Zstd))),Buffer.from([1]));
}
let admissionCalls=0;
await reject(C.Code.ResourceLimit,()=>C.decompressFrom({read(){++admissionCalls;return null;}},dec(C.Format.Zstd,{maxWindowBytes:1023,checkCancelled(){++admissionCalls;}})));
assert.equal(admissionCalls,0);
// LZ4 fast mode has an explicit zero-only level policy and a non-forgeable per-operation handle.
assert.throws(()=>C.lz4Provider(null),error=>error.code===C.Code.Unavailable);
assert.throws(()=>C.lz4Provider({}),error=>error.code===C.Code.Unavailable);
for(const absent of [null,ordinary,provider,Object.freeze({name:'lz4js'}),Object.create(Object.getPrototypeOf(lz4))]) {
  let calls=0;const checkCancelled=()=>{++calls;};
  const encode=new C.EncodeOptions({format:C.Format.Lz4,provider:absent,checkCancelled});
  const decode=new C.DecodeOptions({format:C.Format.Lz4,provider:absent,checkCancelled});
  assert(!C.available(C.Format.Lz4,absent));
  await reject(C.Code.Unavailable,()=>C.encode(()=>{++calls;return plain;},encode));
  await reject(C.Code.Unavailable,()=>C.decompressFrom({read(){++calls;return null;}},decode));
  assert.throws(()=>new C.Output({write(){++calls;}},encode),error=>error.code===C.Code.Unavailable);
  assert.throws(()=>new C.Input({read(){++calls;return null;}},decode),error=>error.code===C.Code.Unavailable);
  assert.equal(calls,0);
}
let lz4Calls=0;
for(const level of [1,6,9]) await reject(C.Code.InvalidOptions,()=>C.encode(()=>{++lz4Calls;return plain;},enc(C.Format.Lz4,{level,checkCancelled(){++lz4Calls;}})));
await reject(C.Code.ResourceLimit,()=>C.decompressFrom({read(){++lz4Calls;return null;}},dec(C.Format.Lz4,{maxWindowBytes:65535,checkCancelled(){++lz4Calls;}})));
assert.equal(lz4Calls,0);
// Optional module failures retain their exact values, including falsy JavaScript throws.
const repeated=new Uint8Array(100).fill(65), checkedLz4=await C.compress(repeated,enc(C.Format.Lz4));
for(const original of [new Error('original LZ4 provider failure'),null,undefined,0,false]) {
  const broken=C.lz4Provider({compressBlock(){throw original;},decompressBlock(){throw original;}});
  for(const encoding of [true,false]) {
    let caught=false;
    try { if(encoding)await C.compress(repeated,enc(C.Format.Lz4,{provider:broken}));else await C.decompress(checkedLz4,dec(C.Format.Lz4,{provider:broken})); }
    catch(actual){caught=true;assert.equal(actual,original);}
    assert(caught);
  }
}
// Invalid provider progress is observed before the next policy callback can replace it.
for(const encoding of [true,false]) {
  let returned=false;
  const broken=C.lz4Provider({compressBlock(){returned=true;return NaN;},decompressBlock(){returned=true;return -1;}});
  const checkCancelled=()=>{if(returned)throw new Error('later callback must not mask invalid progress');};
  await reject(C.Code.InvalidData,()=>encoding ? C.compress(repeated,enc(C.Format.Lz4,{provider:broken,checkCancelled})) : C.decompress(checkedLz4,dec(C.Format.Lz4,{provider:broken,checkCancelled})));
}
// An accidentally asynchronous block module rejects without an unhandled provider rejection.
let unhandled=0;const onUnhandled=()=>{++unhandled;};process.on('unhandledRejection',onUnhandled);
try {
  const broken=C.lz4Provider({compressBlock(){return Promise.reject(new Error('asynchronous block call'));},decompressBlock(){return Promise.reject(new Error('asynchronous block call'));}});
  await reject(C.Code.InvalidData,()=>C.compress(repeated,enc(C.Format.Lz4,{provider:broken})));
  await reject(C.Code.InvalidData,()=>C.decompress(checkedLz4,dec(C.Format.Lz4,{provider:broken})));
  await new Promise(resolve=>setImmediate(resolve));assert.equal(unhandled,0);
} finally { process.removeListener('unhandledRejection',onUnhandled); }
// Every observed asynchronous provider cancellation boundary retains the first original exception.
let cancellationBoundaries=0;
for (const format of formats) for (const encoding of [false,true]) {
  const packed=await C.compress(plain,enc(format));
  let observed=0;
  const operation=checkCancelled=>encoding ? C.compress(plain,enc(format,{checkCancelled})) : C.decompress(packed,dec(format,{checkCancelled}));
  await operation(()=>{++observed;});
  for(let boundary=1;boundary<=observed;++boundary) {
    let count=0; const original=new Error('original optional cancellation');
    await same(original,()=>operation(()=>{if(++count===boundary)throw original;})); ++cancellationBoundaries;
  }
}
// Retain independent empty/multichunk outputs at selected provider levels.
for (const format of formats) for (const level of format===C.Format.Zstd ? [0,1,6,9] : [0]) for (const sample of [0,1]) {
  const bytes=sample ? new Uint8Array(65537).fill(65) : new Uint8Array();
  writeFileSync(join(directory,`level-${format}-${level}-${sample}.bin`),await C.compress(bytes,enc(format,{level})));
}
// Browser imports and identity never load Node; explicit optional factory failure stays unavailable.
let imports=0; const context=createContext({TextEncoder,TextDecoder,lz4Module,plainBytes:Array.from(plain)});
const browser=new SourceTextModule(readFileSync(new URL('./schema.mjs',import.meta.url),'utf8'),{context,importModuleDynamically(){++imports;throw new Error('Node forbidden');}});
await browser.link(()=>{throw new Error('Unconditional dependency');}); await browser.evaluate();
assert.equal(imports,0); assert(!browser.namespace.__COMPRESSION_NAME__.available(C.Format.Zstd));
const browserConsumer=new SourceTextModule(`import {__COMPRESSION_NAME__ as C} from 'schema';
const provider=C.lz4Provider(lz4Module),plain=Uint8Array.from(plainBytes);
const packed=await C.compress(plain,new C.EncodeOptions({format:C.Format.Lz4,provider}));
const back=await C.decompress(packed,new C.DecodeOptions({format:C.Format.Lz4,provider}));
if(back.length!==plain.length||back.some((byte,index)=>byte!==plain[index]))throw Error('Browser LZ4 differs');`,{context});
await browserConsumer.link(()=>browser);await browserConsumer.evaluate();assert.equal(imports,0);
await assert.rejects(()=>browser.namespace.__COMPRESSION_NAME__.zstdProvider(),error=>error.code===C.Code.Unavailable); assert.equal(imports,1);
// A Node module with only the older DEFLATE capability stays usable while Zstandard rejects.
const olderContext=createContext({TextEncoder,TextDecoder}); let olderImports=0, forbiddenCalls=0;
const olderModule=new SyntheticModule(['createDeflateRaw','createInflateRaw','constants'],function() {
  this.setExport('createDeflateRaw',()=>{++forbiddenCalls;throw new Error('Forbidden native call');});
  this.setExport('createInflateRaw',()=>{++forbiddenCalls;throw new Error('Forbidden native call');});
  this.setExport('constants',Object.freeze({}));
},{context:olderContext});
await olderModule.link(()=>{}); await olderModule.evaluate();
const older=new SourceTextModule(readFileSync(new URL('./schema.mjs',import.meta.url),'utf8'),{context:olderContext,
  importModuleDynamically(){++olderImports;return olderModule;}});
await older.link(()=>{throw new Error('Unconditional dependency');}); await older.evaluate();
const olderAPI=older.namespace.__COMPRESSION_NAME__;
await assert.rejects(()=>olderAPI.zstdProvider(),error=>error.code===C.Code.Unavailable);
const olderProvider=await olderAPI.nodeProvider();
assert(olderAPI.available(C.Format.Gzip,olderProvider)); assert(!olderAPI.available(C.Format.Zstd,olderProvider));
assert.equal(forbiddenCalls,0); assert.equal(olderImports,1);
console.log(JSON.stringify({input_decisions:decisions,independent_outputs:18,cancellation_boundaries:cancellationBoundaries,versions:process.versions}));
'''


def consumer(helper):
    """Reuse actual native/lifecycle checks while replacing core-only corpus and admission assertions."""
    source = core.JAVASCRIPT
    first = source.index('// Exercise the independent framing corpus')
    last = source.index('// Existing native callbacks')
    source = source[:first] + source[last:]
    first = source.index('// Admission failures precede cancellation')
    last = source.index('// Browser-like execution imports no Node')
    # Keep the established bounds/lifecycle/callback-close suite after the core admission-only block.
    admission_end = source.index("assert.throws(()=>new C.DecodeOptions(null)", first)
    admission_end = source.index('\n', admission_end) + 1
    source = source[:first] + source[admission_end:last]
    source = source.replace('const formats = [C.Format.None,C.Format.Gzip,C.Format.Zlib,C.Format.Deflate];',
                            'const formats = [C.Format.Zstd,C.Format.Lz4];')
    source = source.replace('const provider = await C.nodeProvider();',
        'const provider = await C.zstdProvider();\nconst importedLz4=await import(process.argv[3]),lz4Module=importedLz4.default??importedLz4,lz4=C.lz4Provider(lz4Module);')
    source = source.replace('{format,provider,...extra}', '{format,provider:format===C.Format.Lz4?lz4:provider,...extra}')
    source = source.replace('C.Format.Gzip', 'C.Format.Zstd').replace('C.Format.Deflate', 'C.Format.Zstd')
    source = source.replace('maxWindowBytes:32767', 'maxWindowBytes:1023')
    source = source.replace('{SourceTextModule,createContext}', '{SourceTextModule,SyntheticModule,createContext}')
    source = source.replace('  let boundaries=0;', '''  scenarios.push(
    async hook=>{const t=sink(),w=new C.Output(t,enc(C.Format.Lz4,{checkCancelled:hook}));await w.write(new Uint8Array(100).fill(1));return{w,t,operation:()=>w.finish()};},
    async hook=>{const t=source(await C.compress(new Uint8Array(100).fill(1),enc(C.Format.Lz4))),w=new C.Input(t,dec(C.Format.Lz4,{checkCancelled:hook}));return{w,t,operation:()=>w.read()};}
  );
  let boundaries=0;''')
    return (source + EXTRA).replace('__COMPRESSION_NAME__', helper)


def run(command, directory):
    """Require explicit SDK/provider commands to finish and retain their complete diagnostics."""
    print('RUN', subprocess.list2cmdline(list(map(str, command))), flush=True)
    result = subprocess.run(list(map(str, command)), cwd=directory, capture_output=True, text=True, timeout=300)
    print(result.stdout+result.stderr, end='', flush=True)
    result.check_returncode()
    return result.stdout


def main():
    """Generate real matching modules and qualify Node frames plus strict normal/collision consumers."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--node', default=shutil.which('node'))
    parser.add_argument('--tsc', type=Path, default=ROOT/'editors/vscode/node_modules/typescript/bin/tsc')
    parser.add_argument('--provider-path', type=Path)
    parser.add_argument('--cpp-helper', type=Path, required=True)
    parser.add_argument('--cpp-linked-helper', type=Path, required=True)
    parser.add_argument('--lz4-module', type=Path, required=True)
    args = parser.parse_args()
    if args.provider_path:
        sys.path.insert(0, str(args.provider_path.resolve()))
    from compression import zstd
    import lz4.frame
    build=args.build.resolve()
    if build==ROOT or ROOT in build.parents and build.relative_to(ROOT).parts[0]!='out':
        parser.error('In-repository output must remain beneath ignored out/')
    build.mkdir(parents=True,exist_ok=True)
    compiler=args.compiler.resolve(); helper=args.cpp_helper.resolve(); tsc=args.tsc.resolve()
    counts=fixtures(build,('ZSTD','LZ4'))
    # Actual native producer bytes extend the independent common fixture inventory.
    args.cpp_helper=helper;args.cpp_linked_helper=args.cpp_linked_helper.resolve()
    native=native_fixtures(args,build,os.environ.copy(),counts)
    family=core.generate(compiler,build)
    (build/'test.mjs').write_text(consumer(family),encoding='utf-8')
    output=run([args.node,'--experimental-vm-modules',build/'test.mjs',build,args.lz4_module.resolve().as_uri()],build)
    report=json.loads(output.strip().splitlines()[-1])
    assert report['input_decisions']==sum(counts.values())
    outputs=[]
    for format in ('zstd','lz4'):
        for protocol in ('JSON','BINARY_NONE','BINARY_INTEGER','BINARY_STRING'):
            outputs.append((format,build/f'output-{format}-{protocol}.bin',(build/f'native-{protocol}.bin').read_bytes()))
        for level in ((0,1,6,9) if format=='zstd' else (0,)):
            for sample in (0,1):
                outputs.append((format,build/f'level-{format}-{level}-{sample}.bin',b'A'*65537 if sample else b''))
    for format,path,expected in outputs:
        decoder=zstd.ZstdDecompressor() if format=='zstd' else lz4.frame.LZ4FrameDecompressor(); frame=path.read_bytes()
        if decoder.decompress(frame)!=expected or not decoder.eof or decoder.unused_data or not frame[4]&4:
            raise AssertionError('Independent exact checksummed frame failed: '+path.name)
        for mode in ('decode','stream_decode'):
            destination=build/(path.stem+'-'+mode+'.plain')
            run([helper,mode,format,path,destination],build)
            if destination.read_bytes()!=expected:
                raise AssertionError('Native C++ consumer differs: '+path.name)
    profiles={}
    for name,schema,flags in (
            ('normal',core.SCHEMA,()),
            ('collision','serializer version 1; class SerializerCompression {} class SerializerCompression_2 {} class Promise {} class PromiseLike {} class globalThis {} class AccountRecord { public uint32 accountId (1); }',('--js.naming','preserve')),
            ('declaration-collision','serializer version 1; class SerializerCompressionErrorBase {} class AccountRecord { public uint32 accountId (1); }',('--js.naming','preserve')),
            ('binary',core.SCHEMA,('--protocols','binary_none'))):
        case=build/name;case.mkdir(exist_ok=True)
        prefix=core.generate(compiler,case,schema,flags)
        text="import {"+prefix+" as C,AccountRecord,Protocol} from './schema.mjs';\n" \
            "const provider:C.ZstdProvider=await C.zstdProvider();const value=new AccountRecord();value.accountId=7;\n" \
            "const encoded:Uint8Array=await C.encode(()=>value.encode(Protocol.BINARY_NONE),new C.EncodeOptions({format:C.Format.Zstd,provider}));\n" \
            "const decoded:AccountRecord=await C.decode(encoded,x=>AccountRecord.decode(x,Protocol.BINARY_NONE),new C.DecodeOptions({format:C.Format.Zstd,provider}));\n" \
            "if(decoded.accountId!==7)throw Error('Native value changed');\n"
        if name=='binary':
            text+="// @ts-expect-error disabled native protocol stays unavailable\ntry{value.encode(Protocol.JSON);throw Error('Disabled protocol accepted');}catch(e){if(!(e instanceof RangeError))throw e;}\n"
        typed_lz4="const block:C.Lz4BlockModule={compressBlock(){return 0;},decompressBlock(_a,_b,_c,_d,begin){return begin;}};\n" \
            "const lz4Provider:C.Lz4Provider=C.lz4Provider(block);const lz4Options=new C.EncodeOptions({format:C.Format.Lz4,provider:lz4Provider,level:0});\n" \
            "const legacyProvider:C.Provider=await C.nodeProvider();const legacyName:'node:zlib'=legacyProvider.name;\n" \
            "const legacyPolicy:C.EncodePolicy={provider:legacyProvider};const legacy:C.EncodeOptions=new C.EncodeOptions(legacyPolicy);\n" \
            "const legacyProperty:C.Provider|null=legacy.provider;const inferred=new C.EncodeOptions({provider:legacyProvider});const inferredName:'node:zlib'|undefined=inferred.provider?.name;\n" \
            "// @ts-expect-error block modules require the decoder as well\nC.lz4Provider({compressBlock(){return 0;}});\n" \
            "// @ts-expect-error provider metadata is immutable\nlz4Provider.name='node:zlib';\n"
        (case/'consumer.mts').write_text(text+typed_lz4,encoding='utf-8')
        smoke=text.replace(':C.ZstdProvider','').replace(':Uint8Array','').replace(':AccountRecord','')
        smoke+="const lz4Module=(await import("+json.dumps(args.lz4_module.resolve().as_uri())+")).default;\n" \
            "const lz4Provider=C.lz4Provider(lz4Module);const lz4Wire=await C.encode(()=>value.encode(Protocol.BINARY_NONE),new C.EncodeOptions({format:C.Format.Lz4,provider:lz4Provider}));\n" \
            "const lz4Value=await C.decode(lz4Wire,x=>AccountRecord.decode(x,Protocol.BINARY_NONE),new C.DecodeOptions({format:C.Format.Lz4,provider:lz4Provider}));\n" \
            "if(lz4Value.accountId!==7)throw Error('LZ4 native value changed');\n"
        (case/'smoke.mjs').write_text(smoke,encoding='utf-8')
        run([args.node,case/'smoke.mjs'],case)
        run([args.node,tsc,'--strict','--noEmit','--target','ES2022','--module','NodeNext','--moduleResolution','NodeNext',case/'consumer.mts'],case)
        profiles[name]={'generated_bytes':(case/'schema.mjs').stat().st_size,
                        'generated_sha256':hashlib.sha256((case/'schema.mjs').read_bytes()).hexdigest()}
    report.update(native_cpp_producers=native,native_cpp_consumers=36,profiles=profiles,
                  provider_module_sha256=hashlib.sha256(args.lz4_module.read_bytes()).hexdigest(),
                  compiler_sha256=hashlib.sha256(compiler.read_bytes()).hexdigest(),
                  test_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    (build/'verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report,indent=2))


if __name__=='__main__':
    main()
