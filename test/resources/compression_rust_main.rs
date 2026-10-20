// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
mod schema;
use schema::*;
use schema::__COMPRESSION_NAME__ as c;
use std::cell::Cell;
use std::io::{Read, Write};
use std::path::Path;
use std::rc::Rc;
use std::sync::Arc;

const FORMATS: [(&str,c::Format);4] = [("NONE",c::Format::None),("GZIP",c::Format::Gzip),
    ("ZLIB",c::Format::Zlib),("DEFLATE",c::Format::Deflate)];

/// Keep independent application errors concrete and observable across owning and borrowed calls.
#[derive(Debug)]
struct Marker(&'static str);
impl std::fmt::Display for Marker {
    /// Format only the bounded independent marker.
    fn fmt(&self, output: &mut std::fmt::Formatter<'_>) -> std::fmt::Result { output.write_str(self.0) }
}
impl std::error::Error for Marker {}

/// Require a compression category rather than an unrelated transport/native error.
fn reject<T>(code: c::Code, action: impl FnOnce() -> c::Result<T>) -> c::Error {
    match action() {
        Ok(_) => panic!("Accepted expected category {:?}",code),
        Err(error) => { assert_eq!(error.downcast_ref::<c::Failure>().expect("Compression category").code,code); error }
    }
}

/// Compare the shared identity of the original application error, not only its string value.
fn same<T>(expected: &c::Error, action: impl FnOnce() -> c::Result<T>) {
    match action() { Err(actual) => assert!(Arc::ptr_eq(expected,&actual)), Ok(_) => panic!("Expected original error") }
}

/// Load independently authored framing bytes without using the generated provider as an oracle.
fn bytes(directory: &Path, name: &str) -> Vec<u8> { std::fs::read(directory.join(name)).unwrap() }

/// Keep actual emitted output for a separate standard-library consumer.
fn save(directory: &Path, name: &str, value: &[u8]) { std::fs::write(directory.join(name),value).unwrap(); }

/// Replay malformed/dynamic/history fixtures at exact and one-tight input/output bounds.
fn fixtures(directory: &Path) -> usize {
    let rows = std::fs::read_to_string(directory.join("fixtures.tsv")).unwrap();
    for row in rows.lines() {
        let cells: Vec<_> = row.split('\t').collect();
        let format = FORMATS.iter().find(|entry| entry.0 == cells[0]).unwrap().1;
        let packed = bytes(directory,cells[2]);
        if cells[1] == "invalid" { reject(c::Code::InvalidData,|| c::decompress(&packed,c::DecodeOptions {format,..Default::default()})); }
        else {
            let plain = bytes(directory,cells[3]);
            assert_eq!(c::decompress(&packed,c::DecodeOptions {format,max_input_bytes:packed.len(),max_output_bytes:plain.len(),..Default::default()}).unwrap(),plain,"{}",cells[2]);
            if !packed.is_empty() { reject(c::Code::ResourceLimit,|| c::decompress(&packed,c::DecodeOptions {format,max_input_bytes:packed.len()-1,..Default::default()})); }
            if !plain.is_empty() { reject(c::Code::ResourceLimit,|| c::decompress(&packed,c::DecodeOptions {format,max_output_bytes:plain.len()-1,..Default::default()})); }
        }
    }
    rows.lines().count()
}

/// Exercise actual generated native methods, exact native consumption and post-callback cancellation.
fn models(directory: &Path) {
    let mut value = AccountRecord::default(); value.account_id = 42; value.label = "owning 😀".to_owned();
    for (name,protocol) in [("JSON",Protocol::Json),("BINARY_NONE",Protocol::BinaryNone),
        ("BINARY_INTEGER",Protocol::BinaryInteger),("BINARY_STRING",Protocol::BinaryString)] {
        let native = value.encode(protocol).unwrap(); save(directory,&format!("native-{name}.bin"),&native);
        for (label,format) in FORMATS {
            let packed = c::encode(|| value.encode(protocol).map_err(c::into_error),c::EncodeOptions {format,..Default::default()}).unwrap();
            let candidate = c::decode(&packed,|data| AccountRecord::decode_with_limits(data,protocol,Limits::default()).map_err(c::into_error),
                c::DecodeOptions {format,..Default::default()}).unwrap();
            assert_eq!(candidate.encode(protocol).unwrap(),native);
            save(directory,&format!("output-{label}-{name}.bin"),&packed);
            let mut source=std::io::Cursor::new(packed.clone());
            let mut input=c::Input::new(&mut source,c::DecodeOptions {format,..Default::default()}).unwrap();
            let candidate=input.decode(|data| AccountRecord::decode(data,protocol).map_err(c::into_error)).unwrap();
            assert_eq!(candidate.encode(protocol).unwrap(),native);
            reject(c::Code::InvalidOptions,|| input.finish()); input.close(); drop(input);
            assert_eq!(source.position(),packed.len() as u64);
            let mut suffix = native.clone(); suffix.push(0);
            let bad = c::compress(&suffix,c::EncodeOptions {format,..Default::default()}).unwrap();
            let error = c::decode(&bad,|data| AccountRecord::decode(data,protocol).map_err(c::into_error),
                c::DecodeOptions {format,..Default::default()}).unwrap_err();
            assert!(error.is::<schema::Error>(),"Native error type changed");
            let marker: c::Error = Arc::new(Marker("late native candidate")); let armed = Cell::new(false);
            let hook = || if armed.get() {Err(marker.clone())} else {Ok(())};
            same(&marker,|| c::decode(&packed,|data| { let candidate = AccountRecord::decode(data,protocol).map_err(c::into_error)?;
                armed.set(true); Ok(candidate) },c::DecodeOptions {format,check_cancelled:Some(&hook),..Default::default()}));
            armed.set(false);
            let mut source=std::io::Cursor::new(packed.clone());
            let mut input=c::Input::new(&mut source,c::DecodeOptions {format,check_cancelled:Some(&hook),..Default::default()}).unwrap();
            same(&marker,|| input.decode(|data| {let candidate=AccountRecord::decode(data,protocol).map_err(c::into_error)?;
                armed.set(true); Ok(candidate)}));
            cause(&marker,input.finish().unwrap_err());
        }
    }
}

/// Check option/provider admission before either a native or cancellation callback.
fn admission() {
    let touched = Cell::new(false); let hook = || {touched.set(true); Ok(())};
    for format in [c::Format::Zstd,c::Format::Lz4] {
        assert!(!c::available(format));
        reject(c::Code::Unavailable,|| c::encode(|| {touched.set(true); Ok(Vec::new())},c::EncodeOptions {format,check_cancelled:Some(&hook),..Default::default()}));
        reject(c::Code::Unavailable,|| c::decode(&[],|_| {touched.set(true); Ok(())},c::DecodeOptions {format,check_cancelled:Some(&hook),..Default::default()}));
        let mut source=Source::new(Vec::new()); let reads=source.calls.clone();
        reject(c::Code::Unavailable,|| c::Input::new(&mut source,c::DecodeOptions {format,check_cancelled:Some(&hook),..Default::default()}));
        let mut sink=Sink::new(); let writes=sink.calls.clone(); let flushes=sink.flushes.clone();
        reject(c::Code::Unavailable,|| c::Output::new(&mut sink,c::EncodeOptions {format,check_cancelled:Some(&hook),..Default::default()}));
        assert_eq!((reads.get(),writes.get(),flushes.get()),(0,0,0));
    }
    for level in [-1,10] { reject(c::Code::InvalidOptions,|| c::encode(|| {touched.set(true); Ok(Vec::new())},c::EncodeOptions {level,check_cancelled:Some(&hook),..Default::default()})); }
    reject(c::Code::ResourceLimit,|| c::decompress(&[],c::DecodeOptions {format:c::Format::Gzip,max_window_bytes:32767,check_cancelled:Some(&hook),..Default::default()}));
    assert!(!touched.get());
}

/// Check empty/exact bounds and emit stored/fixed/history output for an independent oracle.
fn owning(directory: &Path, payload: &[u8]) {
    for (name,format) in FORMATS {
        let packed = c::compress(payload,c::EncodeOptions {format,..Default::default()}).unwrap();
        assert_eq!(c::decompress(&packed,c::DecodeOptions {format,max_input_bytes:packed.len(),max_output_bytes:payload.len(),..Default::default()}).unwrap(),payload);
        assert_eq!(c::compress(payload,c::EncodeOptions {format,max_input_bytes:payload.len(),max_output_bytes:packed.len(),..Default::default()}).unwrap(),packed);
        if !packed.is_empty() { reject(c::Code::ResourceLimit,|| c::compress(payload,c::EncodeOptions {format,max_output_bytes:packed.len()-1,..Default::default()})); }
        let empty = c::compress(&[],c::EncodeOptions {format,max_input_bytes:0,max_output_bytes:64,..Default::default()}).unwrap();
        assert!(c::decompress(&empty,c::DecodeOptions {format,max_output_bytes:0,..Default::default()}).unwrap().is_empty());
        let sample = &payload[..70001.min(payload.len())];
        for level in [0,1,6,9] {
            let output = c::compress(sample,c::EncodeOptions {format,level,..Default::default()}).unwrap();
            assert_eq!(c::decompress(&output,c::DecodeOptions {format,..Default::default()}).unwrap(),sample);
            save(directory,&format!("level-{name}-{level}.bin"),&output);
        }
        let history = &payload[..32768];
        for (sample,input) in [("repeat",vec![65;262145]),("history",[history,history,&history[..257]].concat())] {
            for level in [0,1,6,9] {
                let output = c::compress(&input,c::EncodeOptions {format,level,..Default::default()}).unwrap();
                assert_eq!(c::decompress(&output,c::DecodeOptions {format,..Default::default()}).unwrap(),input);
                save(directory,&format!("sample-{name}-{sample}-{level}.bin"),&output);
            }
        }
        let polls = Cell::new(0); let counter = || {polls.set(polls.get()+1); Ok(())};
        c::decompress(&packed,c::DecodeOptions {format,check_cancelled:Some(&counter),..Default::default()}).unwrap();
        let marker: c::Error = Arc::new(Marker("decoder cancellation"));
        for stop in [1,1.max(polls.get()/2),polls.get()] {
            let seen = Cell::new(0); let hook = || {seen.set(seen.get()+1); if seen.get()==stop {Err(marker.clone())} else {Ok(())}};
            same(&marker,|| c::decompress(&packed,c::DecodeOptions {format,check_cancelled:Some(&hook),..Default::default()}));
        }
    }
}

/// Observe legal short source reads and original transport failures through standard Read.
struct Source { bytes: Vec<u8>, position: usize, calls: Rc<Cell<usize>>, maximum: Rc<Cell<usize>>, mode: u8 }
impl Source {
    /// Keep independent input bytes and external counters without aliasing the wrapper itself.
    fn new(bytes: Vec<u8>) -> Self { Self {bytes,position:0,calls:Rc::new(Cell::new(0)),maximum:Rc::new(Cell::new(0)),mode:0} }
}
impl Read for Source {
    /// Return only requested bytes, standard EOF, or one independently selected transport failure.
    fn read(&mut self, output: &mut [u8]) -> std::io::Result<usize> {
        self.calls.set(self.calls.get()+1); self.maximum.set(self.maximum.get().max(output.len()));
        if self.mode == 1 {return Err(std::io::Error::other("original input failure"));}
        if self.mode == 2 {return Ok(output.len()+1);}
        if self.mode == 3 {panic!("input panic tripwire");}
        let count = 7.min(output.len()).min(self.bytes.len()-self.position);
        output[..count].copy_from_slice(&self.bytes[self.position..self.position+count]); self.position += count; Ok(count)
    }
}

/// Observe short writes, zero progress, contract violations, transport errors and unwinding panics.
struct Sink { bytes: Vec<u8>, calls: Rc<Cell<usize>>, flushes: Rc<Cell<usize>>, mode: u8 }
impl Sink {
    /// Own only test evidence; the production wrapper receives a mutable borrow.
    fn new() -> Self {Self {bytes:Vec::new(),calls:Rc::new(Cell::new(0)),flushes:Rc::new(Cell::new(0)),mode:0}}
}
impl Write for Sink {
    /// Retain a complete accepted prefix or return the selected independent failure.
    fn write(&mut self, input: &[u8]) -> std::io::Result<usize> {
        self.calls.set(self.calls.get()+1);
        if self.mode == 1 {return Ok(0);}
        if self.mode == 2 {return Ok(input.len()+1);}
        if self.mode == 3 {return Err(std::io::Error::other("original output failure"));}
        if self.mode == 4 {panic!("output panic tripwire");}
        let count = 11.min(input.len()); self.bytes.extend_from_slice(&input[..count]); Ok(count)
    }
    /// Trip if the production wrapper forwards implicit flushes to caller-owned transport.
    fn flush(&mut self) -> std::io::Result<()> {self.flushes.set(self.flushes.get()+1); Ok(())}
}

/// Require later inactive operations to retain the first original error as their shared cause.
fn cause(expected: &c::Error, later: c::Error) {
    let failure = later.downcast_ref::<c::Failure>().unwrap(); assert_eq!(failure.code,c::Code::InvalidOptions);
    assert!(Arc::ptr_eq(expected,failure.cause.as_ref().unwrap()));
}

/// Verify exact EOF/finish, short I/O, failure/panic fencing and caller-owned transport lifetime.
fn lifecycle(payload: &[u8]) {
    for (_,format) in FORMATS {
        let packed = c::compress(payload,c::EncodeOptions {format,..Default::default()}).unwrap();
        let mut source = Source::new(packed.clone()); let calls = source.calls.clone(); let maximum = source.maximum.clone();
        {
            let mut input = c::Input::new(&mut source,c::DecodeOptions {format,..Default::default()}).unwrap();
            assert_eq!(input.read(&mut []).unwrap(),0); assert_eq!(calls.get(),0);
            let mut plain = Vec::new(); let mut buffer = [0u8;13];
            loop {let count=input.read(&mut buffer).unwrap(); if count==0 {break;} plain.extend_from_slice(&buffer[..count]);}
            assert_eq!(plain,payload); assert!(maximum.get()<=32768); input.finish().unwrap();
            reject(c::Code::InvalidOptions,|| input.read(&mut buffer)); input.close();
        }
        assert_eq!(source.position,packed.len());
        let mut unread=std::io::Cursor::new(packed.clone());
        let mut input=c::Input::new(&mut unread,c::DecodeOptions {format,..Default::default()}).unwrap();
        let first=reject(c::Code::InvalidData,|| input.finish()); cause(&first,input.read(&mut [0u8;1]).unwrap_err());
        let empty=c::compress(&[],c::EncodeOptions {format,..Default::default()}).unwrap();
        let mut empty_source=std::io::Cursor::new(empty);
        let mut input=c::Input::new(&mut empty_source,c::DecodeOptions {format,max_output_bytes:0,..Default::default()}).unwrap();
        input.finish().unwrap(); reject(c::Code::InvalidOptions,|| input.read(&mut [0u8;1]));
        let mut sink = Sink::new(); let calls = sink.calls.clone(); let flushes = sink.flushes.clone();
        {
            let mut output = c::Output::new(&mut sink,c::EncodeOptions {format,..Default::default()}).unwrap();
            output.write(payload).unwrap(); output.flush().unwrap(); assert_eq!(calls.get(),0);
            output.finish().unwrap(); reject(c::Code::InvalidOptions,|| output.finish()); output.close();
        }
        assert_eq!(flushes.get(),0); assert_eq!(c::decompress(&sink.bytes,c::DecodeOptions {format,..Default::default()}).unwrap(),payload);
        let marker: c::Error = Arc::new(Marker("native output failure")); let mut sink = Sink::new();
        let mut output = c::Output::new(&mut sink,c::EncodeOptions {format,..Default::default()}).unwrap();
        same(&marker,|| output.encode(|| Err(marker.clone()))); cause(&marker,output.finish().unwrap_err());
        for mode in [1,2,3,4] {
            let mut sink = Sink::new(); sink.mode=mode; let calls=sink.calls.clone();
            let mut output=c::Output::new(&mut sink,c::EncodeOptions {format,..Default::default()}).unwrap(); output.write(&[1]).unwrap();
            if mode==4 {assert!(std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| output.finish())).is_err());}
            else {let first=output.finish().unwrap_err(); if mode==2 {assert_eq!(first.downcast_ref::<c::Failure>().unwrap().code,c::Code::InvalidData);} else {assert!(first.is::<std::io::Error>());} cause(&first,output.flush().unwrap_err());}
            reject(c::Code::InvalidOptions,|| output.finish()); assert_eq!(calls.get(),1); output.close();
        }
        for mode in [1,2,3] {
            let mut source=Source::new(Vec::new()); source.mode=mode; let calls=source.calls.clone();
            let mut input=c::Input::new(&mut source,c::DecodeOptions {format,..Default::default()}).unwrap(); let mut target=[0xa5;8];
            if mode==3 {assert!(std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| input.read(&mut target))).is_err());}
            else {let first=input.read(&mut target).unwrap_err(); if mode==2 {assert_eq!(first.downcast_ref::<c::Failure>().unwrap().code,c::Code::InvalidData);} else {assert!(first.is::<std::io::Error>());} cause(&first,input.finish().unwrap_err());}
            reject(c::Code::InvalidOptions,|| input.read(&mut target)); assert_eq!(calls.get(),1); assert_eq!(target,[0xa5;8]); input.close();
        }
        let armed=Cell::new(false); let marker:c::Error=Arc::new(Marker("staged cancellation"));
        let hook=|| if armed.get() {Err(marker.clone())} else {Ok(())};
        let mut source=Source::new(c::compress(&[1],c::EncodeOptions {format,..Default::default()}).unwrap());
        let mut input=c::Input::new(&mut source,c::DecodeOptions {format,check_cancelled:Some(&hook),..Default::default()}).unwrap();
        assert_eq!(input.read(&mut [0u8;1]).unwrap(),1); armed.set(true); same(&marker,|| input.finish());
        cause(&marker,input.read(&mut [0u8;1]).unwrap_err());
    }
    let mut source=Source::new(vec![0;11]); reject(c::Code::ResourceLimit,|| c::decompress_from(&mut source,c::DecodeOptions {max_input_bytes:10,..Default::default()}));
    assert!(source.maximum.get()<=11);
    let mut sink=Sink::new(); let calls=sink.calls.clone(); let mut output=c::Output::new(&mut sink,Default::default()).unwrap();
    output.write(&[1]).unwrap(); output.close(); assert_eq!(calls.get(),0);
}

/// Run actual generated codecs and preserve independent outputs even in optimized SDK builds.
fn main() {
    let directory=std::path::PathBuf::from(std::env::args().nth(1).expect("fixture directory"));
    let decisions=fixtures(&directory); let payload=bytes(&directory,"plain.bin");
    admission(); models(&directory); owning(&directory,&payload); lifecycle(&payload);
    println!("PASS Rust compression: {decisions} independent decisions, native bytes, bounds and borrowed lifecycle");
}
