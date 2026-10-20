"""Qualify real generated Go compression with independent standard-frame and native-codec oracles."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import re
import shutil
import struct
import subprocess
import tempfile
import zlib


SCHEMA = """serializer version 1;
class account_record stable_ids {
  public uint32 account_id (1);
  public string label (2);
}
"""

GO_TEST = r'''package main
import (
  "bytes"
  "encoding/hex"
  "errors"
  "io"
  "os"
  "path/filepath"
  "strings"
  "testing"
)
var formats = map[string]SerializerCompressionFormat{"NONE":SerializerCompressionNone,
  "GZIP":SerializerCompressionGzip,"ZLIB":SerializerCompressionZlib,"DEFLATE":SerializerCompressionDeflate}
var cancelled = errors.New("application cancellation sentinel")

// requireCode checks error classification and prevents partial owned output from escaping.
func requireCode(t *testing.T, code SerializerCompressionCode, data []byte, err error) {
  t.Helper()
  var failure *SerializerCompressionError
  if data != nil || !errors.As(err, &failure) || failure.Code != code { t.Fatalf("Expected %s and no output; got %d bytes, %v", code, len(data), err) }
}
// fixturePath selects only the runner's private directory.
func fixturePath(name string) string { return filepath.Join(os.Getenv("SRL_COMPRESSION_FIXTURES"), name) }

// TestIndependentFrames consumes independent framing decisions at exact and one-tight byte boundaries.
func TestIndependentFrames(t *testing.T) {
  contents, err := os.ReadFile(fixturePath("fixtures.tsv")); if err != nil { t.Fatal(err) }
  for _, line := range strings.Split(strings.TrimSuffix(string(contents), "\n"), "\n") {
    fields := strings.Split(line, "\t"); if len(fields) != 5 { t.Fatal("Malformed fixture manifest") }
    t.Run(fields[0], func(t *testing.T) {
      format := formats[fields[1]]
      plain, err := hex.DecodeString(fields[3]); if err != nil { t.Fatal(err) }
      packed, err := hex.DecodeString(fields[4]); if err != nil { t.Fatal(err) }
      options := SerializerCompressionDefaultDecodeOptions(format)
      if fields[2] != "ok" {
        actual, err := SerializerCompressionDecompress(packed, options)
        requireCode(t, SerializerCompressionInvalidData, actual, err); return
      }
      options.MaxInputBytes = int64(len(packed)); options.MaxOutputBytes = int64(len(plain))
      actual, err := SerializerCompressionDecompress(packed, options)
      if err != nil || !bytes.Equal(actual, plain) { t.Fatalf("Independent plaintext mismatch: %v", err) }
      if len(plain) != 0 {
        options.MaxOutputBytes--
        actual, err = SerializerCompressionDecompress(packed, options)
        requireCode(t, SerializerCompressionResourceLimit, actual, err); options.MaxOutputBytes++
      }
      if len(packed) != 0 {
        options.MaxInputBytes--
        actual, err = SerializerCompressionDecompress(packed, options)
        requireCode(t, SerializerCompressionResourceLimit, actual, err)
      }
    })
  }
}

// TestSmallWindowPreflightCancellation preserves exact callback errors during bounded grammar traversal.
func TestSmallWindowPreflightCancellation(t *testing.T) {
  contents,err:=os.ReadFile(fixturePath("fixtures.tsv"));if err!=nil{t.Fatal(err)};var packed []byte
  for _,line:=range strings.Split(string(contents),"\n") {
    fields:=strings.Split(line,"\t");if len(fields)==5&&fields[0]=="ZLIB-window-6-0"{packed,err=hex.DecodeString(fields[4]);if err!=nil{t.Fatal(err)}}
  }
  if len(packed)==0{t.Fatal("Missing independently authored small-window control")}
  for _,stop:=range []int{2,5,20}{
    calls:=0;options:=SerializerCompressionDefaultDecodeOptions(SerializerCompressionZlib)
    options.CheckCancelled=func()error{calls++;if calls==stop{return cancelled};return nil}
    data,err:=SerializerCompressionDecompress(packed,options)
    if data!=nil||err!=cancelled||calls!=stop{t.Fatal("Small-window traversal lost cancellation or resumed",stop,calls,err)}
  }
}

// TestNativeModels retains every ordinary wire format and produces outputs for independent Python decoding.
func TestNativeModels(t *testing.T) {
  value := NewAccountRecord(); value.AccountId = 42; value.Label = "native + snowman \u2603 / \U00010437 \x00"
  for protocol := JSON; protocol <= BINARY_STRING; protocol++ {
    native, err := value.Encode(protocol); if err != nil { t.Fatal(err) }
    protocolName := []string{"JSON","BINARY_NONE","BINARY_INTEGER","BINARY_STRING"}[int(protocol)]
    if err = os.WriteFile(fixturePath("native-"+protocolName+".bin"), native, 0600); err != nil { t.Fatal(err) }
    for name, format := range formats {
      t.Run(name+"-"+protocolName, func(t *testing.T) {
        encode := SerializerCompressionDefaultEncodeOptions(format)
        encode.MaxInputBytes = int64(len(native))
        packed, err := SerializerCompressionEncode(value, protocol, encode); if err != nil { t.Fatal(err) }
        if format == SerializerCompressionNone && !bytes.Equal(packed, native) { t.Fatal("Identity changed native bytes") }
        decode := SerializerCompressionDefaultDecodeOptions(format)
        candidate, err := SerializerCompressionDecode(packed, protocol, DecodeAccountRecord, decode)
        if err != nil || candidate == nil || candidate.AccountId != value.AccountId || candidate.Label != value.Label { t.Fatalf("Native candidate mismatch: %v", err) }
        again, err := candidate.Encode(protocol); if err != nil || !bytes.Equal(again, native) { t.Fatal("Native byte identity changed", err) }
        if err = os.WriteFile(fixturePath("output-"+name+"-"+protocolName+".bin"), packed, 0600); err != nil { t.Fatal(err) }
        decode.Limits.MaxBytes = len(native)-1
        failed, err := SerializerCompressionDecode(packed, protocol, DecodeAccountRecord, decode)
        if failed != nil || err == nil { t.Fatal("Native byte policy published a candidate") }
        if format != SerializerCompressionNone {
          decode = SerializerCompressionDefaultDecodeOptions(format); decode.MaxWindowBytes = 32767
          actual, err := SerializerCompressionDecompress(packed, decode)
          requireCode(t, SerializerCompressionResourceLimit, actual, err)
          decode.MaxWindowBytes = 32768
          if _, err = SerializerCompressionDecompress(packed, decode); err != nil { t.Fatal("Exact window rejected", err) }
        }
      })
    }
  }
}

// TestEncodeBudgets covers empty, one-byte and chunk-boundary inputs without publishing bounded failures.
func TestEncodeBudgets(t *testing.T) {
  for _,format:=range formats {
    for _,plain:=range [][]byte{nil,{7},bytes.Repeat([]byte("x"),65537)} {
      options:=SerializerCompressionDefaultEncodeOptions(format)
      packed,err:=SerializerCompressionCompress(plain,options);if err!=nil{t.Fatal(err)}
      options.MaxInputBytes=int64(len(plain));options.MaxOutputBytes=int64(len(packed))
      actual,err:=SerializerCompressionCompress(plain,options)
      if err!=nil||!bytes.Equal(actual,packed){t.Fatal("Exact encode bounds changed bytes",err)}
      if len(plain)!=0 {
        options.MaxInputBytes--;actual,err=SerializerCompressionCompress(plain,options)
        requireCode(t,SerializerCompressionResourceLimit,actual,err);options.MaxInputBytes++
      }
      if len(packed)!=0 {
        options.MaxOutputBytes--;actual,err=SerializerCompressionCompress(plain,options)
        requireCode(t,SerializerCompressionResourceLimit,actual,err)
      }
    }
  }
  source:=[]byte{1,2,3}
  packed,err:=SerializerCompressionCompress(source,SerializerCompressionDefaultEncodeOptions(SerializerCompressionNone));if err!=nil{t.Fatal(err)}
  source[0]=9;if packed[0]!=1{t.Fatal("Identity output borrowed mutable input")}
  plain,err:=SerializerCompressionDecompress(packed,SerializerCompressionDefaultDecodeOptions(SerializerCompressionNone));if err!=nil{t.Fatal(err)}
  packed[0]=8;if plain[0]!=1{t.Fatal("Identity plaintext borrowed compressed input")}
}

// countingSource records provider/option/protocol rejection before any underlying input.
type countingSource struct { calls int; data []byte; failure error; empty bool; finalEOF bool; after func() }
// Read can deliberately return short bytes with EOF, no progress, an application error, or late cancellation.
func (source *countingSource) Read(destination []byte) (int,error) {
  source.calls++
  if source.failure != nil { return 0,source.failure }
  if source.empty { return 0,nil }
  if len(source.data)==0 { if source.after!=nil { source.after() }; return 0,io.EOF }
  count:=len(destination); if count>3 { count=3 }; if count>len(source.data) { count=len(source.data) }
  copy(destination,source.data[:count]); source.data=source.data[count:]
  if source.after!=nil { source.after() }
  if source.finalEOF && len(source.data)==0 { return count,io.EOF }; return count,nil
}
// countingSink exposes short/invalid progress and final-call cancellation without closing an application object.
type countingSink struct { calls int; data []byte; short bool; invalid bool; failure error; after func() }
// Write deliberately follows the selected fault while retaining externally visible prefix bytes.
func (sink *countingSink) Write(data []byte) (int,error) {
  sink.calls++
  if sink.invalid { return len(data)+1,nil }
  if sink.failure!=nil { return 0,sink.failure }
  if sink.short { return 0,nil }
  sink.data=append(sink.data,data...); if sink.after!=nil { sink.after() }; return len(data),nil
}
// trackedEncoder exposes early validation and cancellation after a native serialization callback.
type trackedEncoder struct { calls int; after func(); failure error }
// Encode supplies valid empty native bytes only after recording its invocation.
func (value *trackedEncoder) Encode(protocol Protocol) ([]byte,error) {
  value.calls++; if value.after!=nil { value.after() }; return []byte{},value.failure
}

// TestPoliciesAndAbsence validates all options before source I/O and preserves explicit provider absence.
func TestPoliciesAndAbsence(t *testing.T) {
  for _, format:=range []SerializerCompressionFormat{SerializerCompressionZstd,SerializerCompressionLz4} {
    if SerializerCompressionAvailable(format) { t.Fatal("Unavailable provider reported present") }
    source:=&countingSource{}; sink:=&countingSink{}; value:=&trackedEncoder{}
    _,err:=SerializerCompressionNewInput(source,SerializerCompressionDefaultDecodeOptions(format))
    requireCode(t,SerializerCompressionUnavailable,nil,err)
    _,err=SerializerCompressionNewOutput(sink,SerializerCompressionDefaultEncodeOptions(format))
    requireCode(t,SerializerCompressionUnavailable,nil,err)
    actual,err:=SerializerCompressionEncode(value,JSON,SerializerCompressionDefaultEncodeOptions(format))
    requireCode(t,SerializerCompressionUnavailable,actual,err)
    if source.calls!=0 || sink.calls!=0 || value.calls!=0 { t.Fatal("Unavailable provider performed I/O or native work") }
  }
  for _, format:=range formats {
    source:=&countingSource{}; sink:=&countingSink{}
    options:=SerializerCompressionDefaultEncodeOptions(format); options.Level=10
    _,err:=SerializerCompressionNewOutput(sink,options); requireCode(t,SerializerCompressionInvalidOptions,nil,err)
    options=SerializerCompressionDefaultEncodeOptions(format); options.MaxInputBytes=-1
    _,err=SerializerCompressionNewOutput(sink,options); requireCode(t,SerializerCompressionInvalidOptions,nil,err)
    decode:=SerializerCompressionDefaultDecodeOptions(format); decode.Limits.ReadPolicy=ReadPolicy(255)
    _,err=SerializerCompressionNewInput(source,decode); requireCode(t,SerializerCompressionInvalidOptions,nil,err)
    decode=SerializerCompressionDefaultDecodeOptions(format); decode.MaxOutputBytes=-1
    _,err=SerializerCompressionNewInput(source,decode); requireCode(t,SerializerCompressionInvalidOptions,nil,err)
    input,err:=SerializerCompressionNewInput(source,SerializerCompressionDefaultDecodeOptions(format)); if err!=nil { t.Fatal(err) }
    candidate,err:=SerializerCompressionDecodeFrom(input,Protocol(255),DecodeAccountRecord)
    if candidate!=nil { t.Fatal("Invalid protocol returned a candidate") }; requireCode(t,SerializerCompressionInvalidOptions,nil,err)
    if source.calls!=0 || sink.calls!=0 { t.Fatal("Invalid policy or protocol performed I/O") }
  }
  actual,err:=SerializerCompressionCompress(nil,SerializerCompressionDefaultEncodeOptions(SerializerCompressionFormat(255)))
  requireCode(t,SerializerCompressionInvalidOptions,actual,err)
}

// TestBorrowedWrappers covers exact EOF, short input, source errors, sticky failures and explicit Finish.
func TestBorrowedWrappers(t *testing.T) {
  plain:=bytes.Repeat([]byte("bounded payload"),10000)
  for _,format:=range formats {
    sink:=&countingSink{}
    output,err:=SerializerCompressionNewOutput(sink,SerializerCompressionDefaultEncodeOptions(format)); if err!=nil { t.Fatal(err) }
    if _,err=output.Write(plain); err!=nil { t.Fatal(err) }
    if sink.calls!=0 { t.Fatal("Bytes published before Finish") }
    if err=output.Finish(); err!=nil { t.Fatal(err) }
    calls:=sink.calls; if err=output.Finish(); err!=nil || sink.calls!=calls { t.Fatal("Successful Finish repeated I/O") }
    source:=&countingSource{data:sink.data,finalEOF:true}
    input,err:=SerializerCompressionNewInput(source,SerializerCompressionDefaultDecodeOptions(format)); if err!=nil { t.Fatal(err) }
    data,err:=io.ReadAll(input); if err!=nil || !bytes.Equal(data,plain) { t.Fatal("Short source mismatch",err) }
    if err=input.Finish(); err!=nil { t.Fatal(err) }
    source=&countingSource{data:sink.data}
    input,err=SerializerCompressionNewInput(source,SerializerCompressionDefaultDecodeOptions(format)); if err!=nil { t.Fatal(err) }
    first:=make([]byte,1); if _,err=input.Read(first); err!=nil { t.Fatal(err) }
    err=input.Finish(); requireCode(t,SerializerCompressionInvalidData,nil,err)
    if _,again:=input.Read(first); again!=err { t.Fatal("Unread plaintext failure was not sticky") }
  }
  failure:=errors.New("source sentinel")
  source:=&countingSource{failure:failure}
  input,err:=SerializerCompressionNewInput(source,SerializerCompressionDefaultDecodeOptions(SerializerCompressionNone)); if err!=nil { t.Fatal(err) }
  if _,err=io.ReadAll(input); err!=failure { t.Fatal("Source error identity changed",err) }
  calls:=source.calls; if err=input.Finish(); err!=failure || source.calls!=calls { t.Fatal("Failed input resumed") }
  input,err=SerializerCompressionNewInput(&countingSource{empty:true},SerializerCompressionDefaultDecodeOptions(SerializerCompressionNone)); if err!=nil { t.Fatal(err) }
  if _,err=io.ReadAll(input); err!=io.ErrNoProgress { t.Fatal("Zero-progress source did not fail",err) }
  for _,sink:=range []*countingSink{{short:true},{invalid:true},{failure:failure}} {
    output,err:=SerializerCompressionNewOutput(sink,SerializerCompressionDefaultEncodeOptions(SerializerCompressionNone)); if err!=nil { t.Fatal(err) }
    output.Write([]byte("x")); err=output.Finish(); if err==nil { t.Fatal("Invalid sink succeeded") }
    if sink.failure!=nil && err!=failure { t.Fatal("Sink error identity changed") }
    if sink.short && err!=io.ErrShortWrite { t.Fatal("Short io.Writer contract changed") }
    calls:=sink.calls; if again:=output.Finish(); again!=err || sink.calls!=calls { t.Fatal("Failed output resumed") }
  }
}

// TestNativeBorrowedWrappers uses fresh typed models and bounds native parsing before any candidate publication.
func TestNativeBorrowedWrappers(t *testing.T) {
  for _,format:=range formats {
    sink:=&countingSink{}
    output,err:=SerializerCompressionNewOutput(sink,SerializerCompressionDefaultEncodeOptions(format));if err!=nil{t.Fatal(err)}
    value:=NewAccountRecord();value.AccountId=9;value.Label="text"
    if err=output.Encode(value,BINARY_NONE);err!=nil{t.Fatal(err)}
    if err=output.Finish();err!=nil{t.Fatal(err)}
    input,err:=SerializerCompressionNewInput(bytes.NewReader(sink.data),SerializerCompressionDefaultDecodeOptions(format));if err!=nil{t.Fatal(err)}
    candidate,err:=SerializerCompressionDecodeFrom(input,BINARY_NONE,DecodeAccountRecord)
    if err!=nil||candidate==nil||candidate.AccountId!=9||candidate.Label!="text"{t.Fatal("Fresh typed stream decode failed",err)}
    decode:=SerializerCompressionDefaultDecodeOptions(format);decode.Limits.MaxStringBytes=0
    candidate,err=SerializerCompressionDecode(sink.data,BINARY_NONE,DecodeAccountRecord,decode)
    if err==nil||candidate!=nil{t.Fatal("Native string limit published a candidate")}
    failure:=errors.New("native callback sentinel")
    callback:=func(data[]byte,protocol Protocol,limits...Limits)(*AccountRecord,error){return NewAccountRecord(),failure}
    candidate,err=SerializerCompressionDecode(sink.data,BINARY_NONE,callback,SerializerCompressionDefaultDecodeOptions(format))
    if err!=failure||candidate!=nil{t.Fatal("Failed native callback published its value or changed error")}
  }
}

// TestCancellationAndFreshPublication covers final callbacks, later reads, exact EOF and native parser failures.
func TestCancellationAndFreshPublication(t *testing.T) {
  stopped:=false
  check:=func()error{if stopped{return cancelled};return nil}
  options:=SerializerCompressionDefaultEncodeOptions(SerializerCompressionGzip);options.CheckCancelled=check
  value:=&trackedEncoder{after:func(){stopped=true}}
  actual,err:=SerializerCompressionEncode(value,JSON,options)
  if actual!=nil || err!=cancelled { t.Fatal("Post-encode cancellation published output",err) }
  stopped=false
  sink:=&countingSink{after:func(){stopped=true}}
  output,err:=SerializerCompressionNewOutput(sink,options);if err!=nil{t.Fatal(err)}
  output.Write([]byte("x")); if err=output.Finish();err!=cancelled{t.Fatal("Final sink cancellation ignored",err)}
  calls:=sink.calls;if err=output.Finish();err!=cancelled||sink.calls!=calls{t.Fatal("Cancelled output resumed")}
  for _,format:=range formats{
    stopped=false
    packed,err:=SerializerCompressionCompress([]byte("hello"),SerializerCompressionDefaultEncodeOptions(format));if err!=nil{t.Fatal(err)}
    decode:=SerializerCompressionDefaultDecodeOptions(format);decode.CheckCancelled=check
    input,err:=SerializerCompressionNewInput(bytes.NewReader(packed),decode);if err!=nil{t.Fatal(err)}
    first:=make([]byte,1);input.Read(first);stopped=true
    if _,err=input.Read(first);err!=cancelled{t.Fatal("Later plaintext read ignored cancellation",err)}
    stopped=false
    input,err=SerializerCompressionNewInput(bytes.NewReader(packed),decode);if err!=nil{t.Fatal(err)}
    io.ReadAll(input);stopped=true;if err=input.Finish();err!=cancelled{t.Fatal("Late Finish cancellation ignored",err)}
    stopped=false
    native:=NewAccountRecord();native.AccountId=42
    packed,err=SerializerCompressionEncode(native,JSON,SerializerCompressionDefaultEncodeOptions(format));if err!=nil{t.Fatal(err)}
    callback:=func(data[]byte,protocol Protocol,limits...Limits)(*AccountRecord,error){
      candidate,err:=DecodeAccountRecord(data,protocol,limits...);stopped=true;return candidate,err
    }
    candidate,err:=SerializerCompressionDecode(packed,JSON,callback,decode)
    if candidate!=nil||err!=cancelled{t.Fatal("Post-decode cancellation published a candidate",err)}
    stopped=false
    input,err=SerializerCompressionNewInput(bytes.NewReader(packed),decode);if err!=nil{t.Fatal(err)}
    candidate,err=SerializerCompressionDecodeFrom(input,JSON,callback)
    if candidate!=nil||err!=cancelled{t.Fatal("Stream post-decode cancellation published a candidate",err)}
    stopped=false
    packed,err=SerializerCompressionCompress([]byte("{}x"),SerializerCompressionDefaultEncodeOptions(format));if err!=nil{t.Fatal(err)}
    candidate,err=SerializerCompressionDecode(packed,JSON,DecodeAccountRecord,decode)
    if candidate!=nil||err==nil{t.Fatal("Invalid native message published a candidate")}
    stopped=false
    source:=&countingSource{data:packed,finalEOF:true}
    source.after=func(){if len(source.data)==0{stopped=true}}
    input,err=SerializerCompressionNewInput(source,decode);if err!=nil{t.Fatal(err)}
    if _,err=io.ReadAll(input);err!=cancelled{t.Fatal("Final source callback cancellation ignored",err)}
  }
}
'''

SMOKE = r'''package main
// main compiles the collision-selected API or the binary-only native protocol guard.
func main() {
  value:=NewAccountRecord();value.AccountId=7
  options:=SerializerCompressionDefaultEncodeOptions(SerializerCompressionGzip)
  packed,err:=SerializerCompressionEncode(value,BINARY_NONE,options);if err!=nil{panic(err)}
  decode:=SerializerCompressionDefaultDecodeOptions(SerializerCompressionGzip)
  candidate,err:=SerializerCompressionDecode(packed,BINARY_NONE,DecodeAccountRecord,decode)
  if err!=nil||candidate==nil||candidate.AccountId!=7{panic("Owning compressed candidate mismatch")}
}
'''


def pack(data, bits):
    """Create one independent zlib-family frame without relying on the generated implementation."""
    encoder = zlib.compressobj(6, zlib.DEFLATED, bits)
    return encoder.compress(data) + encoder.flush()


def small_window_rows(directory):
    """Adapt the shared wire corpus; the ordinary Go loop already checks exact/one-tight limits."""
    from compression_window_fixtures import small_zlib_windows
    fixtures = directory / 'small-windows'
    fixtures.mkdir(exist_ok=True)
    rows = []
    for source, plain, status, *_ in small_zlib_windows(fixtures):
        if status == 'resource_limit':
            continue
        rows.append(('ZLIB-' + Path(source).stem, 'ZLIB', status,
                     (fixtures / plain).read_bytes().hex() if status == 'ok' else '',
                     (fixtures / source).read_bytes().hex()))
    return rows


def write_fixtures(directory):
    """Build accepted frames and exact malformed/truncation decisions independently."""
    rows = []
    payload = random.Random(1847).randbytes(262181)
    for name, bits in (('NONE', None), ('GZIP', 31), ('ZLIB', 15), ('DEFLATE', -15)):
        for label, plain in (('empty', b''), ('one', b'x'), ('small', b'abc\x00' * 23), ('large', payload)):
            packed = plain if bits is None else pack(plain, bits)
            rows.append((name + '-' + label, name, 'ok', plain.hex(), packed.hex()))
        if bits is None:
            continue
        packed = pack(b'abc\x00' * 23, bits)
        for length in range(len(packed)):
            rows.append((f'{name}-truncated-{length}', name, 'invalid_data', '', packed[:length].hex()))
        for label, malformed in (('trailing', packed + b'\0'), ('concatenated', packed + packed)):
            rows.append((name + '-' + label, name, 'invalid_data', '', malformed.hex()))
        if name in ('GZIP', 'ZLIB'):
            damaged = bytearray(packed)
            damaged[-1] ^= 1
            rows.append((name + '-checksum', name, 'invalid_data', '', damaged.hex()))
    dictionary = zlib.compressobj(zdict=b'shared dictionary')
    rows.append(('ZLIB-dictionary', 'ZLIB', 'invalid_data', '',
                 (dictionary.compress(b'shared dictionary payload') + dictionary.flush()).hex()))
    plain = b'optional gzip header'
    header = b'\x1f\x8b\x08\x1e' + b'\0' * 4 + b'\0\xff'
    extra = b'extra\0bytes'
    header += struct.pack('<H', len(extra)) + extra + b'n' * 700 + b'\0' + b'c' * 900 + b'\0'
    header += struct.pack('<H', zlib.crc32(header) & 0xffff)
    framed = header + pack(plain, -15) + struct.pack('<II', zlib.crc32(plain), len(plain))
    rows.append(('GZIP-long-optional-header', 'GZIP', 'ok', plain.hex(), framed.hex()))
    for label, offset in (('header-crc', len(header) - 1), ('payload-crc', len(framed) - 8),
                          ('size', len(framed) - 4), ('reserved-flags', 3), ('method', 2)):
        damaged = bytearray(framed)
        damaged[offset] ^= 0x80 if label == 'reserved-flags' else 1
        rows.append(('GZIP-' + label, 'GZIP', 'invalid_data', '', damaged.hex()))
    rows.extend(small_window_rows(directory))
    (directory / 'fixtures.tsv').write_text('\n'.join('\t'.join(row) for row in rows) + '\n',
                                          encoding='utf-8', newline='\n')
    return len(rows)


def generate(compiler, directory, schema, extra=()):
    """Emit through the actual compiler and discover the selected collision-safe public family."""
    source = directory / 'model.serializer'
    source.write_text(schema, encoding='utf-8')
    destination = directory / 'schema.go'
    subprocess.run([str(compiler), '--input', str(source), '--language', 'go',
                    '--go.output', str(destination), '--go.package', 'main', *extra], check=True)
    generated = destination.read_text(encoding='utf-8')
    match = re.search(r'Generated compression helper prefix: ([A-Za-z_][A-Za-z_0-9]*)', generated)
    if not match:
        raise AssertionError('Actual generated Go compression API is missing')
    return match[1], len(destination.read_bytes())


def main():
    """Compile/run the real generated module offline under the documented Go 1.22 language baseline."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--go', default=shutil.which('go'))
    args = parser.parse_args()
    if not args.go:
        parser.error('An actual Go 1.22+ SDK is required')
    root = Path(__file__).resolve().parent.parent
    build = args.build.resolve()
    if build == root or root in build.parents and 'out' not in build.relative_to(root).parts:
        parser.error('In-repository scratch must remain beneath ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment.update(GOTOOLCHAIN='local', GOWORK='off', GOPROXY='off', GOSUMDB='off',
                       GOENV='off', GOFLAGS='', CGO_ENABLED='0', GOMAXPROCS='4',
                       GOCACHE=str(build / 'go-cache'),
                       GOTMPDIR=str(build / 'go-temp'))
    Path(environment['GOTMPDIR']).mkdir(exist_ok=True)
    version = subprocess.check_output([args.go, 'version'], env=environment, text=True).strip()
    sizes = {}
    with tempfile.TemporaryDirectory(prefix='go-compression-', dir=build) as temporary:
        directory = Path(temporary)
        count = write_fixtures(directory)
        prefix, sizes['ordinary'] = generate(args.compiler.resolve(), directory, SCHEMA)
        (directory / 'go.mod').write_text('module serializer-go-compression\n\ngo 1.22\n', encoding='utf-8')
        (directory / 'compression_test.go').write_text(GO_TEST.replace('SerializerCompression', prefix),
                                                   encoding='utf-8')
        (directory / 'reentry_test.go').write_text(
            (root / 'test/resources/compression_reentry_go_test.go').read_text(encoding='utf-8').replace(
                'SerializerCompression', prefix), encoding='utf-8')
        environment['SRL_COMPRESSION_FIXTURES'] = str(directory)
        subprocess.run([args.go, 'test', '-count=1', '-v', '.'], cwd=directory, env=environment, check=True)
        for protocol in ('JSON', 'BINARY_NONE', 'BINARY_INTEGER', 'BINARY_STRING'):
            native = (directory / f'native-{protocol}.bin').read_bytes()
            for name, bits in (('NONE', None), ('GZIP', 31), ('ZLIB', 15), ('DEFLATE', -15)):
                packed = (directory / f'output-{name}-{protocol}.bin').read_bytes()
                if bits is None:
                    actual = packed
                else:
                    decoder = zlib.decompressobj(bits)
                    actual = decoder.decompress(packed) + decoder.flush()
                    if not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
                        raise AssertionError(f'Non-exact independent Go output: {name}/{protocol}')
                if actual != native:
                    raise AssertionError('Compressed output changed native bytes')
            if protocol == 'JSON' and json.loads(native)['account_id'] != 42:
                raise AssertionError('Ordinary native JSON contract changed')
        (directory / 'compression_test.go').unlink()
        (directory / 'reentry_test.go').unlink()
        for name, schema, extra in (
                ('collision', 'serializer version 1; class SerializerCompression {} '
                 'class SerializerCompression_2Format {} class AccountRecord { public uint32 AccountId (1); }',
                 ('--go.naming', 'preserve')),
                ('binary', SCHEMA, ('--protocols', 'binary_none')),
                ('xml-independent', SCHEMA, ('--xml.enabled', 'true'))):
            case = directory / name
            case.mkdir()
            family, sizes[name] = generate(args.compiler.resolve(), case, schema, extra)
            if name == 'collision' and family != 'SerializerCompression_3':
                raise AssertionError('The complete existing-name family was not preserved')
            (case / 'go.mod').write_text('module serializer-go-compression-case\n\ngo 1.22\n',
                                       encoding='utf-8')
            smoke = SMOKE.replace('SerializerCompression', family)
            if name == 'binary':
                smoke = smoke.replace('\n}', '\n  if data,err:=' + family +
                    'Encode(value,JSON,options);data!=nil||err==nil{panic("Disabled protocol accepted")}\n}')
            (case / 'main.go').write_text(smoke, encoding='utf-8')
            subprocess.run([args.go, 'run', '.'], cwd=case, env=environment, check=True)
    report = {'sdk': version, 'language_baseline': 'go 1.22', 'independent_input_decisions': count,
              'independent_output_decisions': 16, 'generated_bytes': sizes,
              'compiler_sha256': hashlib.sha256(args.compiler.read_bytes()).hexdigest()}
    (build / 'verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))
    print('PASS: generated Go owning compression, independent framing/native bytes, policies and lifecycle')


if __name__ == '__main__':
    main()
