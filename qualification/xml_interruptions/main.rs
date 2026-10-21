// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
mod schema;
use schema::{XmlExampleOrder,Protocol};
use schema::serializer_xml_operation as operation;
use operation::{Codec,Options,Resource};
use std::{env,fs,sync::Arc,time::{Duration,Instant}};

#[derive(Debug)]
struct Interrupted;
impl std::fmt::Display for Interrupted {
  /// Retain one caller-owned failure independent of native diagnostics.
  fn fmt(&self, output:&mut std::fmt::Formatter<'_>)->std::fmt::Result { output.write_str("Caller XML interruption") }
}
impl std::error::Error for Interrupted {}
struct Context { polls:usize, threshold:usize, deadline:Instant, marker:operation::Failure }
impl operation::Operation for Context {
  /// Poll every logical admission before retaining more native state.
  fn consume(&mut self,_resource:Resource,_count:usize,field:&str)->operation::Result<()> { self.poll(field) }
  /// Preserve the exact Arc for cancellation and an already expired host monotonic deadline.
  fn poll(&mut self,_field:&str)->operation::Result<()> {
    self.polls+=1;
    if self.threshold!=0 && self.polls>=self.threshold || self.threshold==0 && Instant::now()>=self.deadline { Err(self.marker.clone()) } else { Ok(()) }
  }
  /// Keep local quota failures distinct from the selected caller interruption.
  fn limit(&mut self,message:&'static str,_field:&str)->operation::Failure { Arc::new(schema::Error(message)) }
}

/// Qualify public owning operation callbacks and fresh recovery on both large fixtures.
fn main()->Result<(),Box<dyn std::error::Error>> {
  let args:Vec<String>=env::args().collect();
  for (index,path) in args[2..].iter().enumerate() {
    let input=fs::read_to_string(path)?;
    let value=XmlExampleOrder::decode_xml(&input)?;
    let canonical=value.encode(Protocol::BinaryNone)?;
    let mut verified=0;
    for writing in [false,true] { for threshold in [1,1000,0] {
      let mut context=Context { polls:0,threshold,deadline:Instant::now()-Duration::from_nanos(1),marker:Arc::new(Interrupted) };
      let failure=if writing { value.encode_xml_with_operation(Options::default(),&mut context).err() }
          else { XmlExampleOrder::decode_xml_with_operation(&input,Options::default(),&mut context).err() };
      assert!(failure.is_some_and(|failure|Arc::ptr_eq(&failure,&context.marker)),"Original failure lost or candidate returned");
      assert!(threshold==0 || context.polls==threshold);
      assert_eq!(canonical,value.encode(Protocol::BinaryNone)?);
      assert_eq!(canonical,XmlExampleOrder::decode_xml(&value.encode_xml()?)?.encode(Protocol::BinaryNone)?);
      verified+=1;
    }}
    fs::write(std::path::Path::new(&args[1]).join(format!("{}.wire",index+1)),canonical)?;
    println!("{{\"fixture\":{},\"verified_interruptions\":{},\"recovered\":true}}",index+1,verified);
  }
  Ok(())
}
