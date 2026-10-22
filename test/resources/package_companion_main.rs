// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
mod schema;
use schema::*;
use schema::@package_name@ as p;
/// Preserve XML-only QName scopes outside unchanged native payloads through package replay.
fn main(){let args:Vec<_>=std::env::args().skip(1).collect();assert_eq!(args.len(),2);let source=std::fs::read(&args[0]).unwrap();let value=CompanionGraph::load(&source,&mut p::Budget::default(),p::ExtensionPolicy::default()).unwrap();assert!(value.xml_companions.get("main","").is_some());let encoded=CompanionGraph::encode(&value,&mut p::Budget::default()).unwrap();let copy=CompanionGraph::load(&encoded,&mut p::Budget::default(),p::ExtensionPolicy::default()).unwrap();for protocol in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString]{assert_eq!(value.main.as_ref().unwrap().encode(protocol).unwrap(),copy.main.as_ref().unwrap().encode(protocol).unwrap());}std::fs::write(&args[1],encoded).unwrap();}
