// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
mod schema;
use schema::@p@ as p;
use schema::PackageGraphArchive as A;
use p::CanonicalPackage;
use std::path::Path;
use std::sync::Arc;

/// Check exact shared traversal admissions without constructing an owning payload copy.
fn boundaries(value:&schema::PackageGraphArchiveInput,source:&[u8]){
    let mut encoded=p::Budget::default();let _=A::encode(value,&mut encoded).unwrap();
    let mut decoded=p::Budget::default();let _=A::load(source,&mut decoded,p::ExtensionPolicy::preserve()).unwrap();
    for resource in [p::Resource::WorkUnits,p::Resource::DecoderAllocationBytes,p::Resource::PreservedBytes]{
        for tight in 0..2{let mut budget=p::Budget::default();budget.limits.maximum[resource as usize]=encoded.used[resource as usize]-tight;
            let result=A::encode(value,&mut budget);assert_eq!(result.is_ok(),tight==0,"encode exact/one-tight");
            if let Err(error)=result{assert!(error.downcast_ref::<p::Failure>().is_some_and(|error|error.code==p::Code::ResourceLimit));}
            let mut budget=p::Budget::default();budget.limits.maximum[resource as usize]=decoded.used[resource as usize]-tight;
            let result=A::load(source,&mut budget,p::ExtensionPolicy::preserve());assert_eq!(result.is_ok(),tight==0,"decode exact/one-tight");
            if let Err(error)=result{assert!(error.downcast_ref::<p::Failure>().is_some_and(|error|error.code==p::Code::ResourceLimit));}
        }
    }
    for decode in [false,true]{
        let failure=p::native_error(std::io::Error::other("exact cancellation object"));let polls=std::cell::Cell::new(0usize);
        let cancel=||{let count=polls.get()+1;polls.set(count);if count>100{Err(failure.clone())}else{Ok(())}};
        let mut budget=p::Budget{check_cancelled:Some(&cancel),..Default::default()};
        let result=if decode{A::load(source,&mut budget,p::ExtensionPolicy::preserve()).map(|_|())}else{A::encode(value,&mut budget).map(|_|())};
        let observed=result.err().expect("cancellation published a generation");assert!(Arc::ptr_eq(&failure,&observed));
    }
}

/// Consume independent packages and keep every scope associated with its stable physical occurrence.
fn main(){
    let arguments:Vec<_>=std::env::args().collect();let directory=Path::new(&arguments[1]);let name=&arguments[2];let decision=&arguments[3];
    let baseline=std::fs::read(directory.join("valid.opc")).unwrap();let mut value=A::load(&baseline,&mut p::Budget::default(),p::ExtensionPolicy::preserve()).unwrap();
    assert_eq!(value.copies.len(),2);assert!(value.xml_companions.get("main","").is_some());
    assert!(value.xml_companions.get("copies","/copies/a.xml").is_some());assert!(value.xml_companions.get("copies","/copies/b.xml").is_some());
    if decision=="LOAD_ERROR"{
        let model=value.main.as_ref().unwrap()as*const _;let state=value.xml_companions.get("main","").unwrap()as*const _;
        let bytes=std::fs::read(directory.join(format!("{name}.opc"))).unwrap();assert!(A::load(&bytes,&mut p::Budget::default(),p::ExtensionPolicy::preserve()).is_err());
        assert_eq!(value.main.as_ref().unwrap()as*const _,model);assert_eq!(value.xml_companions.get("main","").unwrap()as*const _,state);
        println!("{{\"decision\":\"LOAD_ERROR\"}}");return;
    }
    value.copies.swap(0,1);let model=value.main.as_ref().unwrap()as*const _;let state=value.xml_companions.get("main","").unwrap()as*const _;
    let encoded=A::encode(&value,&mut p::Budget::default()).unwrap();std::fs::write(directory.join("valid-encoded.opc"),encoded).unwrap();
    let saved=directory.join("valid-saved.opc");A::save(&saved,&value,&mut p::Budget::default(),None).unwrap();
    let canonical=A::encode_canonical(&value,&mut p::Budget::default()).unwrap();std::fs::write(directory.join("valid-canonical.opc"),canonical).unwrap();boundaries(&value,&baseline);
    let profiles=[p::PipelineOverride{role:"main".into(),codec:Some(p::PayloadCodec::Json),method:None},p::PipelineOverride{role:"copies".into(),codec:Some(p::PayloadCodec::Json),method:None}];
    let native=A::encode_profiles(&value,&mut p::Budget::default(),&profiles).unwrap();std::fs::write(directory.join("valid-native.opc"),native).unwrap();
    let callbacks=std::cell::Cell::new(0usize);
    let selected=A::load_with_codecs(&baseline,&mut p::Budget::default(),p::ExtensionPolicy::preserve(),
        |text,context|{callbacks.set(callbacks.get()+1);context.decode_document::<schema::PackageGraphPayload>(text).map(|document|document.value)},
        |text,context|{callbacks.set(callbacks.get()+1);context.decode_document::<schema::PackageGraphPayload>(text).map(|document|document.value)}).unwrap();
    assert_eq!(callbacks.get(),3);assert!(selected.xml_companions.get("main","").is_some());
    let selected_bytes=A::encode_with_codecs(&selected,&mut p::Budget::default(),
        |model,context|context.encode_document(model,selected.xml_companions.get("main","")),
        |model,context|context.encode_document(model,selected.xml_companions.get("copies",context.part))).unwrap();
    std::fs::write(directory.join("valid-selected.opc"),selected_bytes).unwrap();
    std::fs::write(&saved,b"previous generation").unwrap();let mut budget=p::Budget::default();budget.limits.maximum[p::Resource::DecoderAllocationBytes as usize]=0;
    assert!(A::save(&saved,&value,&mut budget,None).is_err());assert_eq!(std::fs::read(&saved).unwrap(),b"previous generation");A::save(&saved,&value,&mut p::Budget::default(),None).unwrap();
    assert_eq!(value.main.as_ref().unwrap()as*const _,model);assert_eq!(value.xml_companions.get("main","").unwrap()as*const _,state);assert_eq!(value.main.as_ref().unwrap().children[0].value,"p:first");
    let mut moves=A::dynamic_paths(&value);let moved=value.copies.iter_mut().find(|part|part.name=="/copies/a.xml").unwrap();moved.name="/moved/a.xml".into();
    for selected in &mut moves{if selected.original=="/copies/a.xml"{selected.original="/COPIES/A.XML".into();selected.destination="/moved/a.xml".into();}}
    let moved=A::encode_renamed_profiles(&value,&mut p::Budget::default(),&moves,&[]).unwrap();std::fs::write(directory.join("valid-moved.opc"),moved).unwrap();
    let moved_native=A::encode_renamed_profiles(&value,&mut p::Budget::default(),&moves,&profiles).unwrap();std::fs::write(directory.join("valid-moved-native.opc"),moved_native).unwrap();
    A::save_renamed_profiles(&directory.join("valid-moved-saved.opc"),&value,&mut p::Budget::default(),&moves,&[],None).unwrap();
    println!("{{\"decision\":\"OK\"}}");
}
