# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify actual generated async JS compression and strict matching TypeScript declarations."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import zlib

from java_compression_test import fixtures as common_fixtures
from csharp_compression_test import extra_fixtures


SCHEMA = 'serializer version 1; class account_record stable_ids { public uint32 account_id (1); public string label (2); }'

JAVASCRIPT = r'''import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {join} from 'node:path';
import {SourceTextModule,createContext} from 'node:vm';
import * as Model from './schema.mjs';
const C = Model.__COMPRESSION_NAME__, directory = process.argv[2];
const plain = readFileSync(join(directory,'plain.bin'));
const formats = [C.Format.None,C.Format.Gzip,C.Format.Zlib,C.Format.Deflate];
const provider = await C.nodeProvider();
const enc = (format, extra={}) => new C.EncodeOptions({format,provider,...extra});
const dec = (format, extra={}) => new C.DecodeOptions({format,provider,...extra});
// Keep exact compression categories distinct from transport/native/cancellation exceptions.
async function reject(code, action) { await assert.rejects(action, error => error instanceof C.Error && error.code === code); }
// Preserve application exception object identity across asynchronous provider cleanup.
async function same(error, action) { await assert.rejects(action, actual => actual === error); }
// Borrowed sources return short chunks and a distinct null EOF without owning lifecycle calls.
function source(bytes, batch=7) {
  return {position:0,calls:0,maximum:0,closes:0,flushes:0,read(maximum) {
    ++this.calls; this.maximum=Math.max(maximum,this.maximum);
    if(this.failure) throw this.failure;
    if(this.position===bytes.length) {this.atEof?.(); return null;}
    const result=bytes.subarray(this.position,this.position+Math.min(maximum,batch)); this.position+=result.length; return result;
  },close(){++this.closes;},flush(){++this.flushes;}};
}
// Every published chunk is independent and is consumed wholly or the callback rejects.
function sink() {
  return {chunks:[],calls:0,maximum:0,closes:0,flushes:0,async write(bytes) {
    ++this.calls; this.maximum=Math.max(bytes.length,this.maximum);
    if(this.failure) throw this.failure;
    this.chunks.push(bytes); await this.afterWrite?.();
  },close(){++this.closes;},flush(){++this.flushes;}};
}
// Exercise the independent framing corpus at exact and one-tight logical bounds.
let decisions=0;
for(const row of readFileSync(join(directory,'fixtures.tsv'),'utf8').trim().split('\n')) {
  const [name,status,packedName,plainName]=row.trim().split('\t');
  const format={GZIP:C.Format.Gzip,ZLIB:C.Format.Zlib,DEFLATE:C.Format.Deflate}[name];
  const packed=readFileSync(join(directory,packedName));
  if(status==='invalid') await reject(C.Code.InvalidData,()=>C.decompress(packed,dec(format)));
  else {
    const value=readFileSync(join(directory,plainName));
    assert.deepEqual(Buffer.from(await C.decompress(packed,dec(format,{maxInputBytes:packed.length,maxOutputBytes:value.length}))),value);
    if(value.length) await reject(C.Code.ResourceLimit,()=>C.decompress(packed,dec(format,{maxOutputBytes:value.length-1})));
    if(packed.length) await reject(C.Code.ResourceLimit,()=>C.decompress(packed,dec(format,{maxInputBytes:packed.length-1})));
  }
  ++decisions;
}
// Existing native callbacks and all four protocols retain their complete uncompressed byte representation.
const value=new Model.AccountRecord(); value.accountId=42; value.label='owning async bytes';
for(const [name,protocol] of Object.entries(Model.Protocol)) {
  const native=value.encode(protocol); writeFileSync(join(directory,'native-'+name+'.bin'),native);
  for(const format of formats) {
    const packed=await C.encode(()=>value.encode(protocol),enc(format));
    const candidate=await C.decode(packed,bytes=>Model.AccountRecord.decode(bytes,protocol),dec(format));
    assert.deepEqual(candidate.encode(protocol),native); writeFileSync(join(directory,'output-'+format+'-'+name+'.bin'),packed);
    await assert.rejects(()=>C.decode(awaitableTail(native),bytes=>Model.AccountRecord.decode(bytes,protocol)),error=>!(error instanceof C.Error));
  }
}
// A native suffix is an inner-decoder error, not an accepted compressed-message merge.
function awaitableTail(bytes) { const result=new Uint8Array(bytes.length+1);result.set(bytes);return result; }
const cancelled=new Error('original cancellation'), transport=new Error('original transport');
for(const format of formats) {
  const packed=await C.compress(plain,enc(format));
  assert.deepEqual(Buffer.from(await C.decompress(packed,dec(format))),plain);
  assert.deepEqual(await C.compress(new Uint8Array(),enc(format)),await C.compress(new Uint8Array(),enc(format)));
  assert.equal((await C.decompress(await C.compress(new Uint8Array(),enc(format)),dec(format,{maxOutputBytes:0}))).length,0);
  await reject(C.Code.ResourceLimit,()=>C.compress(plain,enc(format,{maxInputBytes:plain.length-1})));
  await reject(C.Code.ResourceLimit,()=>C.compress(plain,enc(format,{maxOutputBytes:packed.length-1})));
  await same(cancelled,()=>C.compress(plain,enc(format,{checkCancelled(){throw cancelled;}})));
  await same(cancelled,()=>C.decompress(packed,dec(format,{checkCancelled(){throw cancelled;}})));
  // Mutable complete inputs are copied before the first asynchronous yield.
  const mutable=new Uint8Array(plain), expected=mutable.slice(), pending=C.compress(mutable,enc(format)); mutable.fill(0);
  assert.deepEqual(Buffer.from(await C.decompress(await pending,dec(format))),Buffer.from(expected));
  const packedMutable=packed.slice(), expanded=C.decompress(packedMutable,dec(format));packedMutable.fill(0);
  assert.deepEqual(Buffer.from(await expanded),plain);
  const borrowed=source(packed), input=new C.Input(borrowed,dec(format));
  assert.equal(borrowed.calls,0); await input.flush(); assert.equal(borrowed.calls,0);
  const pieces=[];let item;while((item=await input.read(17))!==null)pieces.push(item);
  await input.finish(); await assert.rejects(()=>input.read()); input.close();
  assert.deepEqual(Buffer.concat(pieces),plain);assert.equal(borrowed.closes,0);assert.equal(borrowed.flushes,0);assert(borrowed.maximum<=32768);
  const target=sink(), output=new C.Output(target,enc(format));
  const mutableWrite=new Uint8Array(plain);const written=output.write(mutableWrite);mutableWrite.fill(0);await written;
  await output.flush();assert.equal(target.calls,0);await output.finish();output.close();
  assert.deepEqual(Buffer.from(await C.decompress(Buffer.concat(target.chunks),dec(format))),plain);
  assert(target.maximum<=32768);assert.equal(target.closes,0);assert.equal(target.flushes,0);
  await assert.rejects(()=>output.write(plain));await assert.rejects(()=>output.finish());
}
// Admission failures precede cancellation, codec, source and sink callbacks, even with invalid frame bytes.
for(const format of [C.Format.Zstd,C.Format.Lz4,C.Format.Gzip,C.Format.Zlib,C.Format.Deflate]) {
  let calls=0;const policy=new C.EncodeOptions({format,checkCancelled(){++calls;}}), readPolicy=new C.DecodeOptions({format,checkCancelled(){++calls;}});
  assert.equal(C.available(format),false);
  await reject(C.Code.Unavailable,()=>C.encode(()=>{++calls;return plain;},policy));
  await reject(C.Code.Unavailable,()=>C.decode(plain,()=>{++calls;},readPolicy));
  await reject(C.Code.Unavailable,()=>C.decompressFrom({read(){++calls;return null;}},readPolicy));
  assert.throws(()=>new C.Output({write(){++calls;}},policy),error=>error.code===C.Code.Unavailable);
  assert.throws(()=>new C.Input({read(){++calls;return null;}},readPolicy),error=>error.code===C.Code.Unavailable);assert.equal(calls,0);
}
assert.throws(()=>new C.EncodeOptions({level:10}),error=>error.code===C.Code.InvalidOptions);
assert.throws(()=>new C.EncodeOptions(null),error=>error.code===C.Code.InvalidOptions);
assert.throws(()=>new C.DecodeOptions(null),error=>error.code===C.Code.InvalidOptions);
assert.throws(()=>new C.DecodeOptions({maxInputBytes:-1}),error=>error.code===C.Code.InvalidOptions);
await reject(C.Code.InvalidOptions,()=>C.compress(plain,{}));
let windowCalls=0;await reject(C.Code.ResourceLimit,()=>C.decompressFrom({read(){++windowCalls;return null;}},dec(C.Format.Gzip,{maxWindowBytes:32767,checkCancelled(){++windowCalls;}})));assert.equal(windowCalls,0);
for(const level of [0,1,6,9])assert.deepEqual(Buffer.from(await C.decompress(await C.compress(plain,enc(C.Format.Deflate,{level})),dec(C.Format.Deflate))),plain);
// Failed wrappers never retry callbacks or emit a finish trailer during close.
let target=sink(), output=new C.Output(target,enc(C.Format.Gzip));await output.write(plain);output.close();assert.equal(target.calls,0);await assert.rejects(()=>output.finish());
target=sink();target.failure=transport;output=new C.Output(target);await output.write(plain);await same(transport,()=>output.finish());const failedCalls=target.calls;await assert.rejects(()=>output.finish());output.close();assert.equal(target.calls,failedCalls);
let borrowed=source(plain);borrowed.failure=transport;let input=new C.Input(borrowed);await same(transport,()=>input.read());const failedReads=borrowed.calls;await assert.rejects(()=>input.read());input.close();assert.equal(borrowed.calls,failedReads);
await assert.rejects(()=>C.decompressFrom({read(){return new Uint8Array();}}),TypeError);
borrowed=source(plain,plain.length);await reject(C.Code.ResourceLimit,()=>C.decompressFrom(borrowed,new C.DecodeOptions({maxInputBytes:10})));assert.equal(borrowed.maximum,11);
input=new C.Input(source(plain));await input.read(1);await reject(C.Code.InvalidData,()=>input.finish());await assert.rejects(()=>input.read());input.close();
// Final source/sink/model boundaries observe cancellation and preserve the first exception object.
let armed=false;borrowed=source(plain,plain.length);borrowed.atEof=()=>{armed=true;};await same(cancelled,()=>C.decompressFrom(borrowed,new C.DecodeOptions({checkCancelled(){if(armed)throw cancelled;}})));
armed=false;input=new C.Input(source(plain),new C.DecodeOptions({checkCancelled(){if(armed)throw cancelled;}}));await input.read(1);armed=true;await same(cancelled,()=>input.read());await assert.rejects(()=>input.finish());
armed=false;target=sink();target.afterWrite=()=>{armed=true;};output=new C.Output(target,new C.EncodeOptions({checkCancelled(){if(armed)throw cancelled;}}));await output.write(Uint8Array.of(1));await same(cancelled,()=>output.finish());assert.equal(target.calls,1);await assert.rejects(()=>output.finish());
armed=false;await same(cancelled,()=>C.decode(Uint8Array.of(1),()=>{armed=true;return {};},new C.DecodeOptions({checkCancelled(){if(armed)throw cancelled;}})));
const nativeFailure=new TypeError('native callback original');await same(nativeFailure,()=>C.encode(()=>{throw nativeFailure;}));await same(nativeFailure,()=>C.decode(Uint8Array.of(1),()=>{throw nativeFailure;}));
// Cancellation raised inside a provider data event must survive stream destruction unchanged.
let polls=0;await same(cancelled,()=>C.compress(plain,enc(C.Format.Deflate,{checkCancelled(){if(++polls===14)throw cancelled;}})));
// A cancellation object may resemble a provider error, and JavaScript can throw falsy values.
const lookalike=Object.assign(new Error('application zlib-like error'),{code:'Z_DATA_ERROR'});
polls=0;await same(lookalike,()=>C.compress(plain,enc(C.Format.Deflate,{checkCancelled(){if(++polls===14)throw lookalike;}})));
for(const cause of [null,undefined,0,false]) {
  let caught=false;
  polls=0;try {await C.compress(plain,enc(C.Format.Deflate,{checkCancelled(){if(++polls===14)throw cause;}}));}catch(actual){caught=true;assert.equal(actual,cause);}
  assert(caught);
  const failed=new C.Output({write(){throw cause;}});await failed.write(Uint8Array.of(1));caught=false;
  try {await failed.finish();}catch(actual){caught=true;assert.equal(actual,cause);}assert(caught);await assert.rejects(()=>failed.finish());
}
// Overlap and close fence active asynchronous operations before a second borrowed call.
let release;target=sink();target.afterWrite=()=>new Promise(resolve=>{release=resolve;});output=new C.Output(target);await output.write(plain);
const finishing=output.finish();while(!release)await new Promise(resolve=>setImmediate(resolve));
const activeFailure=assert.rejects(finishing);await assert.rejects(()=>output.finish());release();await activeFailure;assert.equal(target.calls,1);
let deliver;borrowed={calls:0,read(){++this.calls;return new Promise(resolve=>{deliver=resolve;});}};input=new C.Input(borrowed);const reading=input.read();const readFailure=assert.rejects(reading);input.close();deliver(Uint8Array.of(1));await readFailure;assert.equal(borrowed.calls,1);
// Every user cancellation callback is a reentrancy boundary, including zero-byte/finish paths.
async function callbackFences() {
  const scenarios=[
    async hook=>{const t=sink(),w=new C.Output(t,new C.EncodeOptions({checkCancelled:hook}));return{w,t,operation:()=>w.write(Uint8Array.of(1))};},
    async hook=>{const t=sink(),w=new C.Output(t,new C.EncodeOptions({checkCancelled:hook}));await w.write(Uint8Array.of(1));return{w,t,operation:()=>w.finish()};},
    async hook=>{const t=sink(),w=new C.Output(t,enc(C.Format.Gzip,{checkCancelled:hook}));await w.write(Uint8Array.of(1));return{w,t,operation:()=>w.finish()};},
    async hook=>{const t=sink(),w=new C.Output(t,new C.EncodeOptions({checkCancelled:hook}));return{w,t,operation:()=>w.flush()};},
    async hook=>{const t=source(Uint8Array.of(1)),w=new C.Input(t,new C.DecodeOptions({checkCancelled:hook}));return{w,t,operation:()=>w.read()};},
    async hook=>{const t=source(await C.compress(Uint8Array.of(1),enc(C.Format.Gzip))),w=new C.Input(t,dec(C.Format.Gzip,{checkCancelled:hook}));return{w,t,operation:()=>w.read()};},
    async hook=>{const t=source(new Uint8Array()),w=new C.Input(t,new C.DecodeOptions({checkCancelled:hook}));await w.read();return{w,t,operation:()=>w.read()};},
    async hook=>{const t=source(new Uint8Array()),w=new C.Input(t,new C.DecodeOptions({checkCancelled:hook}));await w.read();return{w,t,operation:()=>w.finish()};},
    async hook=>{const t=source(new Uint8Array()),w=new C.Input(t,new C.DecodeOptions({checkCancelled:hook}));return{w,t,operation:()=>w.flush()};}
  ];
  let boundaries=0;
  for(const build of scenarios) {
    let armed=false,polls=0;const baseline=await build(()=>{if(armed)++polls;});armed=true;await baseline.operation();baseline.w.close();
    const count=polls;assert(count>0);
    for(let at=1;at<=count;++at) {
      let active=false,seen=0,closed=false,callsAtClose=0,subject;
      subject=await build(()=>{if(active&&++seen===at){closed=true;callsAtClose=subject.t.calls;subject.w.close();}});
      active=true;await assert.rejects(subject.operation);assert(closed);assert.equal(subject.t.calls,callsAtClose,'Borrowed I/O after callback close');
      await assert.rejects(()=>subject.w.finish());assert.equal(subject.t.calls,callsAtClose);++boundaries;
    }
  }
  // Reentering an operation fences the original pending operation with the same first failure.
  for(const kind of ['output','input']) {
    let armed=false,nested,nestedFailure,w;const hook=()=>{if(armed){armed=false;nested=w.finish().catch(error=>{nestedFailure=error;});}};
    w=kind==='output'?new C.Output(sink(),new C.EncodeOptions({checkCancelled:hook})):new C.Input(source(new Uint8Array()),new C.DecodeOptions({checkCancelled:hook}));
    if(kind==='output')await w.write(Uint8Array.of(1));else await w.read();
    armed=true;let original;try{await w.finish();}catch(error){original=error;}await nested;assert(original);assert.equal(original,nestedFailure);w.close();
  }
  // A hook which closes and throws keeps its own failure instead of replacing it with a lifecycle error.
  let armed=false,w;w=new C.Output(sink(),new C.EncodeOptions({checkCancelled(){if(armed){w.close();throw cancelled;}}}));armed=true;await same(cancelled,()=>w.write(Uint8Array.of(1)));
  console.log('PASS '+boundaries+' callback-close boundaries and first-failure reentrancy controls');
}
await callbackFences();
// Browser-like execution imports no Node modules and has identity functionality without any provider.
let imports=0;const context=createContext({TextEncoder,TextDecoder});
const browser=new SourceTextModule(readFileSync(new URL('./schema.mjs',import.meta.url),'utf8'),{context,importModuleDynamically(){++imports;throw new Error('Node forbidden');}});
await browser.link(()=>{throw new Error('Unconditional dependency');});await browser.evaluate();
const consumer=new SourceTextModule(`import {__COMPRESSION_NAME__ as C,AccountRecord,Protocol} from 'schema';
const a=new AccountRecord();a.accountId=7;const wire=await C.encode(()=>a.encode(Protocol.BINARY_NONE));
if((await C.decode(wire,b=>AccountRecord.decode(b,Protocol.BINARY_NONE))).accountId!==7)throw Error('Identity changed');
if(C.available(C.Format.Gzip))throw Error('Implicit provider');
try{await C.compress(wire,new C.EncodeOptions({format:C.Format.Gzip}));throw Error('Unavailable accepted');}catch(e){if(e.code!==C.Code.Unavailable)throw e;}`,{context});
await consumer.link(()=>browser);await consumer.evaluate();assert.equal(imports,0);
await assert.rejects(()=>browser.namespace.__COMPRESSION_NAME__.nodeProvider(),error=>error.code===C.Code.Unavailable);assert.equal(imports,1);
const asyncWire=await C.encode(async()=>value.encode(Model.Protocol.BINARY_NONE),enc(C.Format.Gzip));
assert.equal((await C.decode(asyncWire,async bytes=>Model.AccountRecord.decode(bytes,Model.Protocol.BINARY_NONE),dec(C.Format.Gzip))).accountId,42);
console.log('PASS '+decisions+' independent framing decisions, native bytes, async ownership, browser admission, bounds and lifecycle');
console.log(JSON.stringify(process.versions));
'''

TYPESCRIPT = '''import {SerializerCompression as C, AccountRecord, Protocol} from './schema.mjs';
const provider: C.Provider = await C.nodeProvider();
const options = new C.EncodeOptions({format:C.Format.Gzip, provider, level:6});
const value = new AccountRecord(); value.accountId = 42;
const wire: Uint8Array = await C.encode(() => value.encode(Protocol.BINARY_INTEGER), options);
const candidate: AccountRecord = await C.decode(wire, async bytes => AccountRecord.decode(bytes,Protocol.BINARY_INTEGER), new C.DecodeOptions({format:C.Format.Gzip,provider}));
const chunks: Uint8Array[] = [];
const output = new C.Output({async write(bytes) {chunks.push(bytes);}}, options);
await output.write(wire); await output.finish(); output.close();
const input = new C.Input({async read(maximum) {return chunks.shift() ?? null;}});
const bytes: Uint8Array | null = await input.read(17); await input.finish(); input.close();
const plain: Uint8Array = await C.decompressFrom({read() {return null;}});
try { await C.decompress(plain); } catch(error) { if(error instanceof C.Error) { const code: C.Code = error.code; console.log(code); } }
// @ts-expect-error policies are immutable
options.maxOutputBytes = 7;
// @ts-expect-error formats use an explicit closed selector family
new C.EncodeOptions({format:'automatic'});
// @ts-expect-error complete owning encoders return bytes
C.encode(() => 'text');
console.log(candidate, bytes);
'''


def run(command, directory):
    """Run real SDK/compiler commands with bounded time and full failure diagnostics."""
    print('RUN', subprocess.list2cmdline([str(value) for value in command]), flush=True)
    subprocess.run([str(value) for value in command], cwd=directory, check=True, timeout=300)


def generate(compiler, directory, schema=SCHEMA, extra=()):
    """Emit JS and its matching declarations through the real compiler and discover the helper name."""
    source = directory / 'model.serializer'
    source.write_text(schema, encoding='utf-8')
    run([compiler, '--input', source, '--language', 'js,typescript', '--js.output', directory / 'schema.mjs',
         '--typescript.output', directory / 'schema.d.mts', *extra], directory)
    match = re.search(r'Generated JS/TS compression helper name: (\w+)', (directory / 'schema.mjs').read_text(encoding='utf-8'))
    if not match:
        raise AssertionError('Actual generated JS compression API is absent')
    if match[1] not in (directory / 'schema.d.mts').read_text(encoding='utf-8'):
        raise AssertionError('Declaration/runtime helper names diverged')
    return match[1]


def main():
    """Qualify one shared JS/TS runtime, independent compression decisions and real strict type consumers."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--node', default=shutil.which('node'))
    parser.add_argument('--tsc', type=Path)
    args = parser.parse_args()
    root, build = Path(__file__).resolve().parent.parent, args.build.resolve()
    if not args.node:
        parser.error('Actual Node SDK is required')
    tsc = args.tsc or root / 'editors/vscode/node_modules/typescript/bin/tsc'
    if not tsc.is_file():
        parser.error('Actual TypeScript compiler is required (--tsc path/to/typescript/bin/tsc)')
    if build == root or root in build.parents and 'out' not in build.relative_to(root).parts:
        parser.error('In-repository scratch must remain below ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    sizes = {}
    with tempfile.TemporaryDirectory(prefix='javascript-compression-', dir=build) as temporary:
        directory = Path(temporary)
        count = common_fixtures(directory) + extra_fixtures(directory)
        family = generate(args.compiler.resolve(), directory)
        (directory / 'test.mjs').write_text(JAVASCRIPT.replace('__COMPRESSION_NAME__', family), encoding='utf-8')
        (directory / 'consumer.mts').write_text(TYPESCRIPT.replace('SerializerCompression as', family + ' as'), encoding='utf-8')
        run([args.node, '--experimental-vm-modules', directory / 'test.mjs', directory], directory)
        run([args.node, tsc.resolve(), '--strict', '--noEmit', '--target', 'ES2022', '--module', 'NodeNext',
             '--moduleResolution', 'NodeNext', directory / 'consumer.mts'], directory)
        sizes['normal'] = (directory / 'schema.mjs').stat().st_size
        for protocol in ('JSON', 'BINARY_NONE', 'BINARY_INTEGER', 'BINARY_STRING'):
            native = (directory / ('native-' + protocol + '.bin')).read_bytes()
            for name, bits in (('none', None), ('gzip', 31), ('zlib', 15), ('deflate', -15)):
                packed = (directory / ('output-' + name + '-' + protocol + '.bin')).read_bytes()
                if bits is None:
                    actual = packed
                else:
                    decoder = zlib.decompressobj(bits)
                    actual = decoder.decompress(packed) + decoder.flush()
                    if not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
                        raise AssertionError('JS output did not contain exactly one independently accepted frame')
                if actual != native:
                    raise AssertionError('Compression changed native model bytes')
        if json.loads((directory / 'native-JSON.bin').read_bytes())['account_id'] != 42:
            raise AssertionError('Native JSON field identity changed')
        for name, schema, extra in (
                ('collision', 'serializer version 1; class SerializerCompression {} class SerializerCompression_2 {} class Promise {} class PromiseLike {} class globalThis {} class AccountRecord { public uint32 accountId (1); }', ('--js.naming', 'preserve')),
                ('declaration-collision', 'serializer version 1; class SerializerCompressionErrorBase {} class AccountRecord { public uint32 accountId (1); }', ('--js.naming', 'preserve')),
                ('binary', SCHEMA, ('--protocols', 'binary_none'))):
            case = directory / name
            case.mkdir()
            prefix = generate(args.compiler.resolve(), case, schema, extra)
            if name == 'collision' and prefix != 'SerializerCompression_3':
                raise AssertionError('Existing helper-like schema names were not preserved')
            if name == 'declaration-collision' and prefix != 'SerializerCompression_2':
                raise AssertionError('Private declaration names displaced a schema model')
            text = "import {" + prefix + " as C,AccountRecord,Protocol} from './schema.mjs';\n" \
                "const p=await C.nodeProvider();const a=new AccountRecord();a.accountId=7;\n" \
                "const w=await C.encode(()=>a.encode(Protocol.BINARY_NONE),new C.EncodeOptions({format:C.Format.Gzip,provider:p}));\n" \
                "const b=await C.decode(w,x=>AccountRecord.decode(x,Protocol.BINARY_NONE),new C.DecodeOptions({format:C.Format.Gzip,provider:p}));\n" \
                "if(b.accountId!==7)throw Error('Native value changed');\n"
            if name == 'binary':
                text += "// @ts-expect-error binary-only declarations reject disabled native protocols\ntry{a.encode(Protocol.JSON);throw Error('Disabled protocol accepted');}catch(e){if(!(e instanceof RangeError))throw e;}\n"
            (case / 'consumer.mts').write_text(text, encoding='utf-8')
            (case / 'smoke.mjs').write_text(text, encoding='utf-8')
            run([args.node, case / 'smoke.mjs'], case)
            run([args.node, tsc.resolve(), '--strict', '--noEmit', '--target', 'ES2022', '--module', 'NodeNext', '--moduleResolution', 'NodeNext', case / 'consumer.mts'], case)
            sizes[name] = (case / 'schema.mjs').stat().st_size
    report = {'node': subprocess.check_output([args.node, '--version'], text=True).strip(),
              'typescript': subprocess.check_output([args.node, tsc, '--version'], text=True).strip(),
              'independent_input_decisions': count, 'independent_output_decisions': 16,
              'generated_bytes': sizes, 'compiler_sha256': hashlib.sha256(args.compiler.read_bytes()).hexdigest()}
    (build / 'verification.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2), flush=True)
    print('PASS: actual generated JS/TS compression, independent framing, browser admission and strict types', flush=True)


if __name__ == '__main__':
    main()
