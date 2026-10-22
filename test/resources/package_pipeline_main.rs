// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
mod schema;
use schema::@p@ as p;
use schema::PipelineArchive as A;
use std::path::Path;

/// Retain consumer assertions under optimized actual SDK builds.
fn check(condition:bool,message:&str,count:&mut usize){assert!(condition,"{message}");*count+=1;}

/// Consume one independent OPC fixture without using a generated codec as its wire oracle.
fn main(){
    let arguments:Vec<_>=std::env::args().collect();let directory=Path::new(&arguments[1]);let name=&arguments[2];let codec=&arguments[3];let decision=&arguments[4];let mut checks=0;
    let selected=match codec.as_str(){"xml"=>p::PayloadCodec::Xml,"json"=>p::PayloadCodec::Json,_=>p::PayloadCodec::BinaryNone};
    let profiles=[p::PipelineOverride{role:"main".into(),codec:Some(selected),method:None}];
    let source=std::fs::read(directory.join(format!("{name}.opc"))).unwrap();let mut observed=p::Budget::default();
    let mut value=match A::load_profiles(&source,&mut observed,p::ExtensionPolicy::default(),&profiles){Ok(value)=>value,Err(failure)=>{check(decision=="LOAD_ERROR",&format!("Unexpected load rejection: {failure}"),&mut checks);check(failure.downcast_ref::<p::Failure>().is_some(),"Owned failure classification",&mut checks);println!("{{\"decision\":\"{decision}\",\"checks\":{checks}}}");return;}};
    check(decision=="OK","Invalid input published",&mut checks);
    check(value.main.as_ref().is_some_and(|model|model.id==17&&model.label=="pipeline"&&model.values==[2,4,8]),"Native model changed",&mut checks);
    check(value.links.len()==1&&value.links[0].content_type.is_empty()&&value.links[0].data.is_empty(),"External role is not inert",&mut checks);
    for resource in [p::Resource::WorkUnits,p::Resource::DecoderAllocationBytes]{
        let used=observed.used[resource as usize];check(used>0,"Missing native decode admission",&mut checks);
        for tight in 0..2{let mut bounded=p::Budget::default();bounded.limits.maximum[resource as usize]=used-tight;let result=A::load_profiles(&source,&mut bounded,p::ExtensionPolicy::default(),&profiles);check(result.is_ok()==(tight==0),"Decode exact/one-tight bound",&mut checks);if let Err(failure)=result{check(failure.downcast_ref::<p::Failure>().is_some_and(|failure|failure.code==p::Code::ResourceLimit),"Decode quota classification",&mut checks);}}
    }
    let mut measured=p::Budget::default();let encoded=A::encode_profiles(&value,&mut measured,&profiles).unwrap();std::fs::write(directory.join(format!("{name}-encoded.opc")),encoded).unwrap();A::save_profiles(&directory.join(format!("{name}-saved.opc")),&value,&mut p::Budget::default(),&profiles,None).unwrap();
    for resource in [p::Resource::WorkUnits,p::Resource::DecoderAllocationBytes]{let used=measured.used[resource as usize];check(used>0,"Missing native encode admission",&mut checks);for tight in 0..2{let mut bounded=p::Budget::default();bounded.limits.maximum[resource as usize]=used-tight;let result=A::encode_profiles(&value,&mut bounded,&profiles);check(result.is_ok()==(tight==0),"Encode exact/one-tight bound",&mut checks);if let Err(failure)=result{check(failure.downcast_ref::<p::Failure>().is_some_and(|failure|failure.code==p::Code::ResourceLimit),"Encode quota classification",&mut checks);}}}
    for role in ["fixed","missing"]{check(A::select_profiles(&[p::PipelineOverride{role:role.into(),codec:Some(p::PayloadCodec::BinaryNone),method:None}],&p::Budget::default()).is_err(),"Fixed or unknown profile accepted",&mut checks);}
    let destination=directory.join(format!("{name}-atomic.opc"));std::fs::write(&destination,b"previous destination").unwrap();value.links[0].data.push(1);check(A::save_profiles(&destination,&value,&mut p::Budget::default(),&profiles,None).is_err(),"External payload published",&mut checks);check(std::fs::read(destination).unwrap()==b"previous destination","Failed Save changed destination",&mut checks);
    println!("{{\"decision\":\"{decision}\",\"checks\":{checks}}}");
}
