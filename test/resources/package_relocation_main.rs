// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
mod schema;
use schema::@p@ as p;
use schema::{RelocationArchive as A,RelocationArchiveValue as V};
use std::{cell::Cell,path::Path,sync::Arc};

/// Preserve every public payload, routing and opaque byte value in an independent test snapshot.
fn snapshot(value:&V)->String{format!("{:?}|{:?}|{:?}|{:?}|{:?}|{:?}",value.paths,value.reference_links,value.extensions,
    value.first.as_ref().map(|item|(item.id,&item.label)),value.second.as_ref().map(|item|(item.id,&item.label)),
    value.children.iter().map(|item|(&item.name,&item.content_type,item.source_index,item.value.id,&item.value.label)).collect::<Vec<_>>())}
/// Keep assertions active in optimized SDK consumers and count their independent acceptance decisions.
fn check(condition:bool,message:&str,count:&mut usize){assert!(condition,"{message}");*count+=1;}
/// Reject exact one-tight shared bounds while keeping the borrowed model and preservation state reusable.
fn bounds(value:&V,count:&mut usize){
    let before=snapshot(value);let mut measured=p::Budget::default();<A as p::CanonicalPackage>::encode_canonical(value,&mut measured).unwrap();
    for resource in [p::Resource::WorkUnits,p::Resource::DecoderAllocationBytes]{for tight in 0..2{
        let mut budget=p::Budget::default();budget.limits.maximum[resource as usize]=measured.used[resource as usize]-tight;
        let result=<A as p::CanonicalPackage>::encode_canonical(value,&mut budget);check(result.is_ok()==(tight==0),"exact and one-tight canonical admission",count);
        if let Err(error)=result{check(error.downcast_ref::<p::Failure>().is_some_and(|failure|failure.code==p::Code::ResourceLimit),"owned canonical resource classification",count);}
        check(snapshot(value)==before,"bounded encode changed the input",count);
    }}
}
/// Exercise the old public struct literal, explicit canonical trait and atomic publication on real fixtures.
fn main(){
    let args:Vec<_>=std::env::args().collect();assert_eq!(args.len(),5);let directory=Path::new(&args[1]);let name=&args[2];let action=&args[3];let expected=&args[4];let mut checks=0;
    let _old_layout=V{first:None,second:None,children:Vec::new(),extensions:p::Extensions{parts:Vec::new(),relationships:Vec::new()},paths:Default::default(),reference_links:Default::default()};
    let source=std::fs::read(directory.join(format!("{name}.opc"))).unwrap();
    let mut value=match A::load(&source,&mut p::Budget::default(),p::ExtensionPolicy::preserve()){
        Ok(value)=>value,Err(_)=>{check(expected=="LOAD_ERROR","unexpected legacy load rejection",&mut checks);println!("{{\"case\":\"{name}\",\"decision\":\"{expected}\",\"checks\":{checks},\"failure_stage\":\"load\"}}");return;}};
    check(expected!="LOAD_ERROR","invalid origin published",&mut checks);
    check(value.first.as_ref().is_some_and(|item|item.id==11&&item.label=="first")&&value.children.len()==1&&value.children[0].value.id==33&&value.children[0].source_index==0,"typed payload identity and child route",&mut checks);
    if action=="remove_second"{value.second=None;}
    let before=snapshot(&value);let saved=directory.join(format!("{name}-saved.opc"));let previous=b"previous destination";std::fs::write(&saved,previous).unwrap();
    if expected=="REJECT"{
        check(<A as p::CanonicalPackage>::encode_canonical(&value,&mut p::Budget::default()).is_err(),"invalid canonical encode published",&mut checks);
        check(<A as p::CanonicalPackage>::save_canonical(&saved,&value,&mut p::Budget::default(),None).is_err(),"invalid canonical Save published",&mut checks);
        check(std::fs::read(&saved).unwrap()==previous,"rejected Save replaced destination",&mut checks);
    }else{
        check(expected=="OK","unknown expected decision",&mut checks);
        let encoded=<A as p::CanonicalPackage>::encode_canonical(&value,&mut p::Budget::default()).unwrap();std::fs::write(directory.join(format!("{name}-encoded.opc")),&encoded).unwrap();
        let repeated=A::load(&encoded,&mut p::Budget::default(),p::ExtensionPolicy::preserve()).unwrap();let again=<A as p::CanonicalPackage>::encode_canonical(&repeated,&mut p::Budget::default()).unwrap();
        check(encoded==again,"canonical output is not idempotent",&mut checks);std::fs::write(directory.join(format!("{name}-repeat.opc")),again).unwrap();
        <A as p::CanonicalPackage>::save_canonical(&saved,&value,&mut p::Budget::default(),None).unwrap();
        std::fs::write(directory.join(format!("{name}-legacy.opc")),A::encode(&value,&mut p::Budget::default()).unwrap()).unwrap();
        if name=="alternate"{
            bounds(&value,&mut checks);
            let mut written=Vec::new();<A as p::CanonicalPackage>::write_canonical(&mut written,&value,&mut p::Budget::default()).unwrap();check(written==encoded,"canonical borrowed writer changed bytes",&mut checks);
            let marker:p::Error=Arc::new(std::io::Error::other("caller cancellation identity"));let polls=Cell::new(0);
            let cancel=||{polls.set(polls.get()+1);if polls.get()>=7{Err(marker.clone())}else{Ok(())}};
            let mut budget=p::Budget{check_cancelled:Some(&cancel),..Default::default()};std::fs::write(&saved,previous).unwrap();
            let error=<A as p::CanonicalPackage>::save_canonical(&saved,&value,&mut budget,None).unwrap_err();check(Arc::ptr_eq(&error,&marker),"caller cancellation identity changed",&mut checks);
            check(std::fs::read(&saved).unwrap()==previous,"cancelled canonical Save replaced destination",&mut checks);
            let fault=|point:&str|if point=="before_replace"{Err(marker.clone())}else{Ok(())};
            let error=<A as p::CanonicalPackage>::save_canonical(&saved,&value,&mut p::Budget::default(),Some(&fault)).unwrap_err();check(Arc::ptr_eq(&error,&marker)&&std::fs::read(&saved).unwrap()==previous,"faulted canonical Save replaced destination or error",&mut checks);
            let original=value.paths["first"].clone();value.paths.insert("first".into(),value.paths["second"].clone());
            check(<A as p::CanonicalPackage>::encode_canonical(&value,&mut p::Budget::default()).is_err(),"conflicting loaded origins accepted",&mut checks);value.paths.insert("first".into(),original);
            <A as p::CanonicalPackage>::save_canonical(&saved,&value,&mut p::Budget::default(),None).unwrap();
        }
    }
    check(snapshot(&value)==before,"canonical operation mutated caller model or extensions",&mut checks);
    println!("{{\"case\":\"{name}\",\"decision\":\"{expected}\",\"checks\":{checks}}}");
}
