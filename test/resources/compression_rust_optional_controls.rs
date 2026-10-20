// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
use crate::{adapters,schema::{self,__COMPRESSION_NAME__ as c}};
use std::{cell::Cell,io::{Cursor,Write},path::Path,sync::Arc};

/// Select immutable explicit capability handles; default generated APIs never consult this adapter.
fn provider(format: c::Format) -> &'static dyn c::Provider {
    static ZSTD: adapters::Zstd = adapters::Zstd; static LZ4: adapters::Lz4 = adapters::Lz4;
    match format {c::Format::Zstd => &ZSTD,c::Format::Lz4 => &LZ4,_ => panic!("Expected optional format")}
}
/// Select the actual provider level, preserving the public options struct's exact five fields.
fn options(format: c::Format) -> c::EncodeOptions<'static> {
    c::EncodeOptions {format,max_input_bytes:64*1024*1024,max_output_bytes:64*1024*1024,
        level:if format == c::Format::Zstd {3} else {0},check_cancelled:None}
}
/// Require exact shared error identity across owning callbacks and later fenced causes.
fn same<T>(expected: &c::Error, result: c::Result<T>) {
    match result {Err(error) => assert!(Arc::ptr_eq(expected,&error),"Original error identity changed: {error:?}"),Ok(_) => panic!("Expected original error")}
}
/// Require the generated failure category without accepting unrelated provider errors.
fn reject<T>(code: c::Code, result: c::Result<T>) -> c::Error {
    match result {Err(error) => {assert_eq!(error.downcast_ref::<c::Failure>().unwrap().code,code);error},Ok(_) => panic!("Expected compression category {code:?}")}
}
/// Preserve all four native model protocols and leave independent consumer artifacts.
fn models(directory: &Path) {
    let mut value = schema::AccountRecord::default(); value.account_id = 42; value.label = "native \u{1f600}".to_owned();
    for (name,protocol) in [("JSON",schema::Protocol::Json),("BINARY_NONE",schema::Protocol::BinaryNone),
        ("BINARY_INTEGER",schema::Protocol::BinaryInteger),("BINARY_STRING",schema::Protocol::BinaryString)] {
        let plain = value.encode(protocol).unwrap(); std::fs::write(directory.join(format!("native-{name}.bin")),&plain).unwrap();
        for (format_name,format) in [("ZSTD",c::Format::Zstd),("LZ4",c::Format::Lz4)] {
            let selected = provider(format);
            let packed = c::encode_with_provider(||value.encode(protocol).map_err(c::into_error),options(format),selected).unwrap();
            let result = c::decode_with_provider(&packed,|bytes|schema::AccountRecord::decode(bytes,protocol).map_err(c::into_error),c::DecodeOptions {format,..Default::default()},selected).unwrap();
            assert_eq!(result.account_id,value.account_id); assert_eq!(result.label,value.label);
            std::fs::write(directory.join(format!("output-{format_name}-{name}.bin")),&packed).unwrap();
            let mut suffix = plain.clone(); suffix.push(0);
            let invalid = c::compress_with_provider(&suffix,options(format),selected).unwrap();
            assert!(c::decode_with_provider(&invalid,|bytes|schema::AccountRecord::decode(bytes,protocol).map_err(c::into_error),c::DecodeOptions {format,..Default::default()},selected).is_err());
        }
    }
}
/// Retain independent standard-frame outputs at the actual supported provider levels.
fn levels(directory:&Path) {
    for (name,format,levels) in [("ZSTD",c::Format::Zstd,&[-131072,-1,3,22][..]),("LZ4",c::Format::Lz4,&[0][..])] {
        for &level in levels {for sample in 0..=1 {
            let plain = if sample == 0 {Vec::new()} else {vec![b'A';65537]};
            let packed = c::compress_with_provider(&plain,c::EncodeOptions {level,..options(format)},provider(format)).unwrap();
            std::fs::write(directory.join(format!("level-{name}-{level}-{sample}.bin")),packed).unwrap();
        }}
    }
}
/// Cancel at each observed generated/provider boundary, preserving the exact error and no publication.
fn cancellation() {
    let plain = vec![b'A';65537];
    for format in [c::Format::Zstd,c::Format::Lz4] {
        let selected = provider(format); let original: c::Error = c::into_error(std::io::Error::other("original cancellation"));
        let calls = Cell::new(0); let counting = || {calls.set(calls.get()+1);Ok(())};
        let packed = c::compress_with_provider(&plain,c::EncodeOptions {check_cancelled:Some(&counting),..options(format)},selected).unwrap();
        let encode_calls = calls.get();
        for point in 1..=encode_calls {
            calls.set(0); let check = || {calls.set(calls.get()+1);if calls.get() == point {Err(original.clone())} else {Ok(())}};
            same(&original,c::compress_with_provider(&plain,c::EncodeOptions {check_cancelled:Some(&check),..options(format)},selected));
        }
        calls.set(0);
        assert_eq!(c::decompress_with_provider(&packed,c::DecodeOptions {format,check_cancelled:Some(&counting),..Default::default()},selected).unwrap(),plain);
        let decode_calls = calls.get();
        for point in 1..=decode_calls {
            calls.set(0); let check = || {calls.set(calls.get()+1);if calls.get() == point {Err(original.clone())} else {Ok(())}};
            same(&original,c::decompress_with_provider(&packed,c::DecodeOptions {format,check_cancelled:Some(&check),..Default::default()},selected));
        }
        let cancelled = Cell::new(false); let check = || if cancelled.get() {Err(original.clone())} else {Ok(())};
        same(&original,c::decode_with_provider(&packed,|_| {cancelled.set(true);Ok(42)},c::DecodeOptions {format,check_cancelled:Some(&check),..Default::default()},selected));
        let mut source = Cursor::new(&packed);
        cancelled.set(false);
        let mut input = c::Input::with_provider(&mut source,c::DecodeOptions {format,check_cancelled:Some(&check),..Default::default()},selected).unwrap();
        let mut expanded = [0u8;32768]; while input.read(&mut expanded).unwrap() != 0 {}
        cancelled.set(true); same(&original,input.finish());
        let inactive = reject(c::Code::InvalidOptions,input.read(&mut expanded));
        assert!(Arc::ptr_eq(inactive.downcast_ref::<c::Failure>().unwrap().cause.as_ref().unwrap(),&original));
        println!("PASS {:?} cancellation boundaries encode={} decode={}",format,encode_calls,decode_calls);
    }
}
/// A borrowed sink can request cancellation after the final write without owning the wrapper.
struct CancellingSink<'a> { cancelled: &'a Cell<bool>, bytes: Vec<u8> }
impl Write for CancellingSink<'_> {
    /// Accept one short complete output and request cancellation before outer publication.
    fn write(&mut self, bytes: &[u8]) -> std::io::Result<usize> { self.bytes.extend_from_slice(bytes);self.cancelled.set(true);Ok(bytes.len()) }
    /// Native flush must never be forwarded by generated staging.
    fn flush(&mut self) -> std::io::Result<()> { panic!("Borrowed flush was forwarded") }
}
/// Exercise exact first-failure propagation and final-write cancellation through borrowed wrappers.
fn lifecycle() {
    for format in [c::Format::Zstd,c::Format::Lz4] {
        let original: c::Error = c::into_error(std::io::Error::other("final sink cancellation"));
        let cancelled = Cell::new(false); let check = || if cancelled.get() {Err(original.clone())} else {Ok(())};
        let mut sink = CancellingSink {cancelled:&cancelled,bytes:Vec::new()};
        let mut output = c::Output::with_provider(&mut sink,c::EncodeOptions {check_cancelled:Some(&check),..options(format)},provider(format)).unwrap();
        output.write(b"message").unwrap(); output.flush().unwrap(); same(&original,output.finish());
        let inactive = reject(c::Code::InvalidOptions,output.write(b"late"));
        assert!(Arc::ptr_eq(inactive.downcast_ref::<c::Failure>().unwrap().cause.as_ref().unwrap(),&original));
        output.close(); assert!(!sink.bytes.is_empty());
    }
}
/// Missing provider admission is explicit rather than a fallback to linked optional crates.
struct Missing;
impl c::Provider for Missing {
    /// Advertise the format whose capabilities are intentionally absent.
    fn format(&self) -> c::Format {c::Format::Zstd}
    /// Fail before cancellation or model callbacks.
    fn validate_encode(&self,_level:i32) -> c::Result<()> {Err(Arc::new(c::Failure {code:c::Code::Unavailable,message:"Missing test provider",cause:None}))}
    /// Fail before cancellation or source reads.
    fn validate_decode(&self,_maximum:usize) -> c::Result<()> {self.validate_encode(0)}
    /// Inaccessible codec construction is itself a maintained assertion.
    fn encoder(&self,_level:i32,_bytes:usize) -> c::Result<Box<dyn c::Encoder + '_>> {panic!("Unadmitted encoder")}
    /// Inaccessible codec construction is itself a maintained assertion.
    fn decoder(&self,_maximum:usize) -> c::Result<Box<dyn c::Decoder + '_>> {panic!("Unadmitted decoder")}
}
/// Prove original defaults, exact option fields, absence and level/mismatch pre-admission.
fn admission() {
    let count = Cell::new(0); let check = || {count.set(count.get()+1);Ok(())};
    assert!(!c::available(c::Format::Zstd));assert!(!c::available(c::Format::Lz4));
    assert!(!c::available_with_provider(c::Format::Zstd,None));
    let encode = c::EncodeOptions {format:c::Format::Zstd,max_input_bytes:1,max_output_bytes:1,level:3,check_cancelled:Some(&check)};
    let decode = c::DecodeOptions {format:c::Format::Zstd,max_input_bytes:1,max_output_bytes:1,max_window_bytes:1024,check_cancelled:Some(&check)};
    reject(c::Code::Unavailable,c::encode(|| {count.set(count.get()+1);Ok(Vec::new())},encode));
    reject(c::Code::Unavailable,c::encode_with_provider(|| {count.set(count.get()+1);Ok(Vec::new())},encode,&Missing));
    assert!(c::Output::with_provider(Vec::new(),encode,&Missing).is_err());
    assert!(c::Input::with_provider(Cursor::new(&[]),decode,&Missing).is_err());
    reject(c::Code::InvalidOptions,c::encode_with_provider(|| {count.set(count.get()+1);Ok(Vec::new())},c::EncodeOptions {format:c::Format::Lz4,check_cancelled:Some(&check),..options(c::Format::Lz4)},&adapters::Zstd));
    for level in [i32::MIN,-1,1,6,i32::MAX] {
        reject(c::Code::InvalidOptions,c::encode_with_provider(|| {count.set(count.get()+1);Ok(Vec::new())},c::EncodeOptions {level,check_cancelled:Some(&check),..options(c::Format::Lz4)},&adapters::Lz4));
    }
    for level in [i32::MIN,-131073,23,i32::MAX] {
        reject(c::Code::InvalidOptions,c::encode_with_provider(|| {count.set(count.get()+1);Ok(Vec::new())},c::EncodeOptions {level,check_cancelled:Some(&check),..options(c::Format::Zstd)},&adapters::Zstd));
    }
    assert_eq!(count.get(),0);
    for level in [-131072,-1,0,3,22] {c::compress_with_provider(b"message",c::EncodeOptions {level,..options(c::Format::Zstd)},&adapters::Zstd).unwrap();}
    let frame = c::compress_with_provider(b"message",options(c::Format::Zstd),&adapters::Zstd).unwrap();
    assert_eq!(c::decompress_with_provider(&frame,c::DecodeOptions {format:c::Format::Zstd,max_window_bytes:usize::MAX,..Default::default()},&adapters::Zstd).unwrap(),b"message");
}
/// Provider contract violations remain bounded errors rather than offset arithmetic or retry loops.
struct BadProgress(u8);
impl c::Provider for BadProgress {
    /// Exercise the stream path with preflighted Zstandard frames.
    fn format(&self)->c::Format {c::Format::Zstd}
    /// Admit the diagnostic capability without callbacks.
    fn validate_encode(&self,_level:i32)->c::Result<()> {Ok(())}
    /// Admit the diagnostic capability without source reads.
    fn validate_decode(&self,_maximum:usize)->c::Result<()> {Ok(())}
    /// Create independent faulty context state for one encode.
    fn encoder(&self,_level:i32,_bytes:usize)->c::Result<Box<dyn c::Encoder+'_>> {Ok(Box::new(BadProgress(self.0)))}
    /// Create independent faulty context state for one decode.
    fn decoder(&self,_maximum:usize)->c::Result<Box<dyn c::Decoder+'_>> {Ok(Box::new(BadProgress(self.0)))}
}
impl c::Encoder for BadProgress {
    /// Return no progress or an explicit out-of-range count for generated rejection.
    fn process(&mut self,input:&[u8],output:&mut[u8],_finish:bool)->c::Result<c::Progress> {
        Ok(c::Progress {bytes_read:if self.0 == 1 {input.len()+1} else {0},
            bytes_written:if self.0 == 2 {output.len()+1} else {0},finished:false})
    }
}
impl c::Decoder for BadProgress {
    /// Use identical diagnostics on the separately bounded decode path.
    fn process(&mut self,input:&[u8],output:&mut[u8],_history:&[u8])->c::Result<c::Progress> {
        c::Encoder::process(self,input,output,false)
    }
}
/// Context-construction failures retain their exact Arc identity through wrapper failure fencing.
struct ContextFailure(c::Error);
impl c::Provider for ContextFailure {
    /// Select the stream path for a late private allocation/provider failure.
    fn format(&self)->c::Format {c::Format::Zstd}
    /// Capability admission succeeds; the later explicit failure is not called unavailable.
    fn validate_encode(&self,_level:i32)->c::Result<()> {Ok(())}
    /// Capability admission succeeds without application callbacks.
    fn validate_decode(&self,_maximum:usize)->c::Result<()> {Ok(())}
    /// Preserve the original context failure object.
    fn encoder(&self,_level:i32,_bytes:usize)->c::Result<Box<dyn c::Encoder+'_>> {Err(self.0.clone())}
    /// Preserve the original context failure object.
    fn decoder(&self,_maximum:usize)->c::Result<Box<dyn c::Decoder+'_>> {Err(self.0.clone())}
}
/// Check provider progress, factory provenance and inactive causes on real wrapper calls.
fn negative_providers() {
    let frame = c::compress_with_provider(b"message",options(c::Format::Zstd),&adapters::Zstd).unwrap();
    for kind in 0..=2 {
        let provider = BadProgress(kind);
        reject(c::Code::InvalidData,c::compress_with_provider(b"message",options(c::Format::Zstd),&provider));
        reject(c::Code::InvalidData,c::decompress_with_provider(&frame,c::DecodeOptions {format:c::Format::Zstd,..Default::default()},&provider));
        let mut sink = Vec::new(); let mut output = c::Output::with_provider(&mut sink,options(c::Format::Zstd),&provider).unwrap();
        output.write(b"message").unwrap();let first = reject(c::Code::InvalidData,output.finish());
        let later = reject(c::Code::InvalidOptions,output.finish());
        assert!(Arc::ptr_eq(&first,later.downcast_ref::<c::Failure>().unwrap().cause.as_ref().unwrap()));
        output.close();assert!(sink.is_empty());
    }
    let original = c::into_error(std::io::Error::other("unexpected provider context error"));let provider = ContextFailure(original.clone());
    same(&original,c::compress_with_provider(b"message",options(c::Format::Zstd),&provider));
    same(&original,c::decompress_with_provider(&frame,c::DecodeOptions {format:c::Format::Zstd,..Default::default()},&provider));
    let mut input = c::Input::with_provider(Cursor::new(frame),c::DecodeOptions {format:c::Format::Zstd,..Default::default()},&provider).unwrap();
    same(&original,input.read(&mut[0u8;8]));
    let later = reject(c::Code::InvalidOptions,input.finish());
    assert!(Arc::ptr_eq(&original,later.downcast_ref::<c::Failure>().unwrap().cause.as_ref().unwrap()));
}
/// Run independent owning/lifecycle controls in addition to the shared malformed frame corpus.
pub fn run(directory:&Path) {admission();models(directory);levels(directory);cancellation();lifecycle();negative_providers();println!("PASS provider native/lifecycle/admission controls");}
