// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
mod schema;

use schema::{Error, Protocol, XmlLimits, XmlProviderFixtureOrder, XmlValidator};
use std::{env, ffi::c_void, fs, ptr::NonNull, sync::{Arc, RwLock, atomic::{AtomicUsize,Ordering}}, thread};

type Prepare=unsafe extern "C" fn(*const u8,u64)->*mut c_void;
type Validate=unsafe extern "C" fn(*const c_void,*const u8,u64,u64,u64,u64,u64,u64)->i32;
type Release=unsafe extern "C" fn(*mut c_void);
type Message=unsafe extern "C" fn(*mut u8,u64)->u64;

#[link(name="kernel32")]
extern "system" {
    fn LoadLibraryExW(name:*const u16,file:*mut c_void,flags:u32)->*mut c_void;
    fn GetProcAddress(module:*mut c_void,name:*const u8)->*mut c_void;
    fn FreeLibrary(module:*mut c_void)->i32;
}

/// Keep a caller-selected local fixture module alive through every owning prepared schema.
struct Bridge {module:NonNull<c_void>,prepare:Prepare,validate:Validate,release:Release,diagnostic:Message,version:Message}
// The loaded immutable exports are thread-safe; the final Arc owner unloads only after every schema/call ends.
unsafe impl Send for Bridge {}
// ABI contract explicitly gives each validation its own provider context and thread-local diagnostics.
unsafe impl Sync for Bridge {}
impl Bridge {
    /// Resolve fixed fixture symbols without changing process-wide DLL search policy.
    fn load(path:&str)->Result<Arc<Self>,Box<dyn std::error::Error>> {
        let wide:Vec<u16>=path.encode_utf16().chain(Some(0)).collect();
        let module=NonNull::new(unsafe{LoadLibraryExW(wide.as_ptr(),std::ptr::null_mut(),0x0000_0100|0x0000_1000)}).ok_or("Cannot load local provider bridge")?;
        let symbol=|name:&[u8]|->Result<*mut c_void,&'static str>{let value=unsafe{GetProcAddress(module.as_ptr(),name.as_ptr())};if value.is_null(){Err("Missing provider bridge symbol")}else{Ok(value)}};
        // Fixed signatures match xml_provider_bridge.hpp exactly; no C++ exception crosses these C ABI functions.
        let result=(||unsafe{Ok::<Self,&'static str>(Self{module,
            prepare:std::mem::transmute::<*mut c_void,Prepare>(symbol(b"serializer_xml_provider_prepare\0")?),
            validate:std::mem::transmute::<*mut c_void,Validate>(symbol(b"serializer_xml_provider_validate\0")?),
            release:std::mem::transmute::<*mut c_void,Release>(symbol(b"serializer_xml_provider_release\0")?),
            diagnostic:std::mem::transmute::<*mut c_void,Message>(symbol(b"serializer_xml_provider_error\0")?),
            version:std::mem::transmute::<*mut c_void,Message>(symbol(b"serializer_xml_provider_version\0")?),})})();
        match result {Ok(value)=>Ok(Arc::new(value)),Err(error)=>{unsafe{FreeLibrary(module.as_ptr());}Err(error.into())}}
    }
    /// Copy the same synchronous OS thread's bounded provider diagnostic before a scheduler can migrate work.
    fn message(&self,function:Message)->String {
        let mut bytes=[0u8;1024];let length=unsafe{function(bytes.as_mut_ptr(),bytes.len() as u64)};
        String::from_utf8_lossy(&bytes[..(length as usize).min(bytes.len()-1)]).into_owned()
    }
}
impl Drop for Bridge {
    /// Unload after the final prepared schema retaining this module has released its unique native handle.
    fn drop(&mut self){unsafe{FreeLibrary(self.module.as_ptr());}}
}

/// A native handle is shared immutably under a read lease and released once under its exclusive lifetime lock.
struct Handle(NonNull<c_void>);
// xml_provider_bridge.hpp permits immutable handles to be borrowed on multiple native threads.
unsafe impl Send for Handle {}
// Concurrent validation uses independent native provider contexts; release requires an exclusive lock.
unsafe impl Sync for Handle {}
struct Prepared {bridge:Arc<Bridge>,handle:RwLock<Option<Handle>>,calls:AtomicUsize}
const BOUNDS:[u64;5]=[16777216,128,100000,100000,8388608];
const DOCUMENT:&str="<order xmlns='urn:serializer:xsd' id='7'><customer>R&amp;D</customer><value>3</value></order>";
impl Prepared {
    /// Prepare synchronously from borrowed bytes, retaining no caller source slice after returning.
    fn new(bridge:Arc<Bridge>,schema:&[u8])->Result<Self,String>{
        let raw=unsafe{(bridge.prepare)(schema.as_ptr(),schema.len() as u64)};
        let handle=NonNull::new(raw).ok_or_else(||bridge.message(bridge.diagnostic))?;
        Ok(Self{bridge,handle:RwLock::new(Some(Handle(handle))),calls:AtomicUsize::new(0)})
    }
    /// Borrow an immutable handle through the complete provider call; retrieve its diagnostic on the same thread.
    fn bounded(&self,document:&[u8],bounds:[u64;5])->Result<(),String>{
        let lease=self.handle.read().map_err(|_|"Poisoned provider lifetime lock")?;
        let handle=lease.as_ref().ok_or("Prepared provider is closed")?;self.calls.fetch_add(1,Ordering::Relaxed);
        let status=unsafe{(self.bridge.validate)(handle.0.as_ptr(),document.as_ptr(),document.len() as u64,bounds[0],bounds[1],bounds[2],bounds[3],bounds[4])};
        if status==0 {Ok(())}else{let message=self.bridge.message(self.bridge.diagnostic);assert!(!message.is_empty());Err(message)}
    }
    /// Release exactly once, after every validation holding a read lease completes.
    fn close(&self){let previous=self.handle.write().expect("Provider lifetime lock").take();if let Some(handle)=previous{unsafe{(self.bridge.release)(handle.0.as_ptr());}}}
}
impl Drop for Prepared {
    /// Make explicit close and failure-path RAII cleanup idempotent.
    fn drop(&mut self){self.close();}
}
impl XmlValidator for Prepared {
    /// Adapt the generated static-message Error API while retaining full dynamic diagnostics inside bounded().
    fn validate(&self,document:&str)->Result<(),Error>{self.bounded(document.as_bytes(),BOUNDS).map_err(|_|Error("Prepared libxml2 validation failure"))}
}

/// Check real compiled schemas, generated owning transactions, provider bounds, no-fetch and concurrent reuse.
fn main()->Result<(),Box<dyn std::error::Error>>{
    let arguments:Vec<String>=env::args().collect();assert_eq!(arguments.len(),3);
    let bridge=Bridge::load(&arguments[1])?;let source=fs::read(&arguments[2])?;
    let provider=Arc::new(Prepared::new(bridge.clone(),&source)?);
    let value=XmlProviderFixtureOrder::decode_xml_with_validation(DOCUMENT,XmlLimits::default(),Some(provider.as_ref()))?;
    assert_eq!(value.id,7);assert_eq!(value.customer,"R&D");assert_eq!(value.values,vec![3]);
    let output=value.encode_xml_with_validation(XmlLimits::default(),Some(provider.as_ref()))?;provider.bounded(output.as_bytes(),BOUNDS)?;
    let copy=XmlProviderFixtureOrder::decode_xml_with_validation(&output,XmlLimits::default(),Some(provider.as_ref()))?;
    assert_eq!(value.encode(Protocol::BinaryNone)?,copy.encode(Protocol::BinaryNone)?);assert!(provider.calls.load(Ordering::Relaxed)>=4);
    for bad in ["<wrong xmlns='urn:serializer:xsd'/>".to_owned(),DOCUMENT.replace("urn:serializer:xsd","urn:wrong"),format!("{}<extra/>",DOCUMENT),DOCUMENT.replace("<value>3</value>",""),format!("<!DOCTYPE order SYSTEM 'file:///must-not-open'>{}",DOCUMENT),format!("<?xml version='1.1'?>{}",DOCUMENT)]{
        assert!(provider.bounded(bad.as_bytes(),BOUNDS).is_err());
    }
    assert!(provider.bounded(&[0xc3,0x28],BOUNDS).is_err());
    let mismatch=Prepared::new(bridge.clone(),b"<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema' targetNamespace='urn:serializer:xsd'><xs:element name='order' type='xs:string'/></xs:schema>")?;
    assert_eq!(value.encode_xml_with_validation(XmlLimits::default(),Some(&mismatch)),Err(Error("Prepared libxml2 validation failure")));
    assert!(XmlProviderFixtureOrder::decode_xml_with_validation(DOCUMENT,XmlLimits::default(),Some(&mismatch)).is_err());
    for directive in ["include","import","redefine","override"]{for location in ["file:///must-not-open.xsd","https://invalid.example/must-not-open.xsd"]{
        let source=format!("<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:{} schemaLocation='{}'/></xs:schema>",directive,location);
        assert!(Prepared::new(bridge.clone(),source.as_bytes()).is_err());
    }}
    let hinted=DOCUMENT.replacen(" id="," xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance' xsi:schemaLocation='urn:serializer:xsd https://invalid.example/must-not-open.xsd' id=",1);
    provider.bounded(hinted.as_bytes(),BOUNDS)?;
    for index in 0..5{let mut bounds=BOUNDS;bounds[index]=if index==0{DOCUMENT.len() as u64-1}else if index==3{0}else{1};assert!(provider.bounded(DOCUMENT.as_bytes(),bounds).is_err());}
    let workers:Vec<_>=(0..8).map(|_|{let shared=provider.clone();thread::spawn(move||{for _ in 0..16{shared.bounded(DOCUMENT.as_bytes(),BOUNDS).unwrap();assert!(shared.bounded(b"<wrong/>",BOUNDS).is_err());shared.bounded(DOCUMENT.as_bytes(),BOUNDS).unwrap();}})}).collect();
    for worker in workers{worker.join().expect("Concurrent prepared schema validation");}
    assert!(!bridge.message(bridge.version).is_empty());provider.close();assert!(provider.bounded(DOCUMENT.as_bytes(),BOUNDS).is_err());
    println!("Rust prepared libxml2 provider: owning transactions, five bounds, no-fetch, concurrent reuse and closed-state checks pass");Ok(())
}
