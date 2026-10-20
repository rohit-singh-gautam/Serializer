// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
mod schema;
use schema::*;
use schema::@package_name@ as p;
use std::collections::BTreeMap;
use std::io::{Read,Write};

/// Construct a complete authoring candidate with independently ordered attachment resources.
fn authored(reverse:bool)->ProfileArchiveInput {
    let model=ProfileModel{name:"checked & generated".into(),points:vec![ProfilePoint{x:1.0,y:2.0,z:3.0}]};
    let mut attachments=vec![p::BinaryPart{name:"/Textures/a.bin".into(),content_type:"application/octet-stream".into(),data:vec![0,1,254,255],..Default::default()},p::BinaryPart{name:"/Textures/b.bin".into(),content_type:"application/octet-stream".into(),data:b"bounded opaque".to_vec(),..Default::default()}];
    if reverse {attachments.reverse();}
    ProfileArchiveInput{main:Some(model),attachments,..Default::default()}
}

/// Preserve concrete I/O failure identity across the package boundary.
#[derive(Debug)]
struct Marker;
impl std::fmt::Display for Marker {
    /// Give the injected cause a recognizable concrete type.
    fn fmt(&self,f:&mut std::fmt::Formatter<'_>)->std::fmt::Result {f.write_str("injected caller failure")}
}
impl std::error::Error for Marker {}

/// Prove legal nonseekable short reads are consumed without taking ownership.
struct ShortSource<'a> {bytes:&'a [u8],position:usize}
impl Read for ShortSource<'_> {
    /// Return at most three bytes and ordinary zero-at-EOF semantics.
    fn read(&mut self,out:&mut [u8])->std::io::Result<usize>{let count=3.min(out.len()).min(self.bytes.len()-self.position);out[..count].copy_from_slice(&self.bytes[self.position..self.position+count]);self.position+=count;Ok(count)}
}
/// Prove a borrowed sink can legally complete partial writes.
struct ShortSink(Vec<u8>);
impl Write for ShortSink {
    /// Accept at most five bytes per call.
    fn write(&mut self,bytes:&[u8])->std::io::Result<usize>{let count=5.min(bytes.len());self.0.extend_from_slice(&bytes[..count]);Ok(count)}
    /// Package output does not require ownership of the caller's flush policy.
    fn flush(&mut self)->std::io::Result<()>{Ok(())}
}

/// Create a forward-reference graph using exact native string and uint64 key values.
fn graph()->SelectedGraphInput {
    SelectedGraphInput{
        nodes:vec![p::ModelPart{name:"/nodes/n.xml".into(),content_type:"application/vnd.selected-node+xml".into(),source_index:0,value:SelectedNode{key:"name".into(),id:u64::MAX}}],
        links:vec![p::ModelPart{name:"/links/l.xml".into(),content_type:"application/vnd.selected-link+xml".into(),source_index:0,value:SelectedLink{target:"name".into(),target_id:u64::MAX}}],
        ..Default::default()
    }
}

/// Exercise real ZIP32/ZIP64 count transitions and retain bytes for an independent directory reader.
fn count_output(directory:&std::path::Path,count:usize){let parts:Vec<_>=(0..count).map(|index|p::BinaryPart{name:format!("{index:08x}.bin"),..Default::default()}).collect();let bytes=p::zip_write(&parts,&mut p::Budget::default()).unwrap();std::fs::write(directory.join(format!("count-output-{count}.zip")),bytes).unwrap();}

/// Qualify maintained independent application fixtures and retained origin context at exact boundaries.
fn application_contract(directory:&std::path::Path){
    let application=directory.join("application");let mut cases=0;
    for row in std::fs::read_to_string(application.join("cases.tsv")).unwrap().lines(){let(label,expected)=row.split_once('\t').unwrap();let bytes=std::fs::read(application.join(format!("{label}.opc"))).unwrap();let result=ContractArchive::load(&bytes,&mut p::Budget::default(),p::ExtensionPolicy::default());assert_eq!(result.is_ok(),expected=="OK","Application fixture {label}");if expected=="REFERENCE"{let error=result.err().unwrap();let failure=error.downcast_ref::<p::Failure>().unwrap();assert_eq!(failure.code,p::Code::Semantic);assert!(!failure.field.is_empty());assert!(!failure.link.is_empty());if label=="missing"{assert_eq!(failure.part,"/custom/requests.xml");assert_eq!(failure.link,"authored-request");}if label=="duplicate"{assert_eq!(failure.part,"/t2.xml");assert_eq!(failure.link,"duplicate-key");}}cases+=1;}
    let bytes=std::fs::read(application.join("forward-repeated.opc")).unwrap();let mut measured=p::Budget::default();ContractArchive::load(&bytes,&mut measured,p::ExtensionPolicy::default()).unwrap();assert_eq!(measured.used[p::Resource::References as usize],6);
    for resource in [p::Resource::References,p::Resource::Relationships]{let mut budget=p::Budget::default();budget.limits.maximum[resource as usize]=0;let error=ContractArchive::load(&bytes,&mut budget,p::ExtensionPolicy::default()).err().unwrap();let failure=error.downcast_ref::<p::Failure>().unwrap();assert_eq!(failure.code,p::Code::ResourceLimit);assert_eq!(failure.part,"/");assert_eq!(failure.field,"relationships");assert_eq!(failure.link,"r0");}
    for resource in [p::Resource::References,p::Resource::WorkUnits]{let used=measured.used[resource as usize];let mut budget=p::Budget::default();budget.limits.maximum[resource as usize]=used;ContractArchive::load(&bytes,&mut budget,p::ExtensionPolicy::default()).unwrap();let mut budget=p::Budget::default();budget.limits.maximum[resource as usize]=used-1;let error=ContractArchive::load(&bytes,&mut budget,p::ExtensionPolicy::default()).err().unwrap();let failure=error.downcast_ref::<p::Failure>().unwrap();assert_eq!(failure.code,p::Code::ResourceLimit);assert_eq!(failure.part,"/s2.xml");assert_eq!(failure.field,"target_id");assert_eq!(failure.link,"r1");}
    let mut loaded=ContractArchive::load(&std::fs::read(application.join("relocated-singleton.opc")).unwrap(),&mut p::Budget::default(),p::ExtensionPolicy::default()).unwrap();assert_eq!(loaded.paths["fixed"],"/custom/fixed.xml");loaded.fixed.as_mut().unwrap().id=7;let error=ContractArchive::encode(&loaded,&mut p::Budget::default()).unwrap_err();let failure=error.downcast_ref::<p::Failure>().unwrap();assert_eq!(failure.part,"/custom/fixed.xml");assert_eq!(failure.field,"id");assert_eq!(failure.link,"r2");
    println!("PASS Rust {cases} shared application fixtures, exact six-reference/work boundaries and loaded origin diagnostics");
}

/// Run actual generated schemas, independent ZIP/XML fixtures and atomic publication failures.
fn main() {
    let directory=std::path::PathBuf::from(std::env::args().nth(1).expect("fixture directory"));
    application_contract(&directory);
    let spool_temp=directory.join("spool-temp");assert!(std::fs::read_dir(&spool_temp).unwrap().next().is_none());
    let candidate=authored(false);
    let encoded=ProfileArchive::encode(&candidate,&mut p::Budget::default()).unwrap();
    let decoded=ProfileArchive::load(&encoded,&mut p::Budget::default(),p::ExtensionPolicy::default()).unwrap();
    assert_eq!(decoded.main.unwrap().name,"checked & generated");assert_eq!(decoded.attachments.len(),2);
    std::fs::write(directory.join("generated.opc"),&encoded).unwrap();
    let mut budget=p::Budget{force_zip64:true,..Default::default()};
    let wide=ProfileArchive::encode(&candidate,&mut budget).unwrap();
    assert!(ProfileArchive::load(&wide,&mut p::Budget::default(),p::ExtensionPolicy::default()).is_ok());
    std::fs::write(directory.join("generated64.opc"),wide).unwrap();
    let mut source=ShortSource{bytes:&encoded,position:0};
    assert_eq!(ProfileArchive::read(&mut source,&mut p::Budget::default(),p::ExtensionPolicy::default()).unwrap().attachments.len(),2);
    assert_eq!(source.position,encoded.len());
    assert!(std::fs::read_dir(&spool_temp).unwrap().next().is_none());
    let mut sink=ShortSink(Vec::new());ProfileArchive::write(&mut sink,&candidate,&mut p::Budget::default()).unwrap();assert_eq!(sink.0,encoded);
    let mut count=0;
    for line in std::fs::read_to_string(directory.join("fixtures/cases.tsv")).unwrap().lines(){
        let fields:Vec<_>=line.split('\t').collect();let bytes=std::fs::read(directory.join("fixtures").join(fields[0])).unwrap();let expected=fields[2]=="1";
        let accepted=match fields[1]{
            "zip"=>(||{let mut budget=p::Budget::default();let archive=p::ZipReader::new(&bytes,&mut budget)?;if let Some(count)=fields[0].strip_prefix("count-").and_then(|value|value.strip_suffix(".zip")){assert_eq!(archive.entries.len(),count.parse::<usize>().unwrap());}for entry in &archive.entries{archive.read(&entry.name,&mut budget)?;}Ok::<(),p::Error>(())})().is_ok(),
            "collection"=>ProfileCollectionArchive::load(&bytes,&mut p::Budget::default(),p::ExtensionPolicy::default()).is_ok(),
            "graph"=>SelectedGraph::load(&bytes,&mut p::Budget::default(),p::ExtensionPolicy::default()).is_ok(),
            "parameters"=>SelectedParameters::load(&bytes,&mut p::Budget::default(),p::ExtensionPolicy::default()).is_ok(),
            "preserve"=>ProfileArchive::load(&bytes,&mut p::Budget::default(),p::ExtensionPolicy::preserve()).is_ok(),
            _=>ProfileArchive::load(&bytes,&mut p::Budget::default(),p::ExtensionPolicy::default()).is_ok()
        };
        assert_eq!(accepted,expected,"Independent case {}",fields[0]);count+=1;
    }
    // Loaded singleton paths and source-relative opaque links survive edit/save without retargeting.
    let extensions=std::fs::read(directory.join("fixtures/extensions.opc")).unwrap();
    assert!(ProfileArchive::load(&extensions,&mut p::Budget::default(),p::ExtensionPolicy::default()).is_err());
    let preserved=ProfileArchive::load(&extensions,&mut p::Budget::default(),p::ExtensionPolicy::preserve()).unwrap();
    assert_eq!(preserved.paths["main"],"/Alternate/root.xml");
    assert_eq!(preserved.extensions.parts.len(),1);assert_eq!(preserved.extensions.relationships.len(),1);
    let preserved_bytes=ProfileArchive::encode(&preserved,&mut p::Budget::default()).unwrap();
    std::fs::write(directory.join("preserved.opc"),&preserved_bytes).unwrap();
    assert!(ProfileArchive::load(&preserved_bytes,&mut p::Budget::default(),p::ExtensionPolicy::preserve()).is_ok());
    let mut scoped=ProfileArchive::load(&std::fs::read(directory.join("fixtures/scoped-extensions.opc")).unwrap(),&mut p::Budget::default(),p::ExtensionPolicy::preserve()).unwrap();assert_eq!(scoped.extensions.relationships.len(),3);scoped.main.as_mut().unwrap().name="edited".into();let scoped_bytes=ProfileArchive::encode(&scoped,&mut p::Budget::default()).unwrap();assert!(ProfileArchive::load(&scoped_bytes,&mut p::Budget::default(),p::ExtensionPolicy::preserve()).is_ok());std::fs::write(directory.join("scoped-preserved.opc"),scoped_bytes).unwrap();
    let mut broken=preserved;broken.extensions.relationships[0].target="missing.xml".into();
    assert!(ProfileArchive::encode(&broken,&mut p::Budget::default()).is_err());
    let mut required=p::ExtensionPolicy{optional:p::Unknown::Skip,..Default::default()};required.required_relationships.insert("urn:vendor:metadata".into());
    assert!(ProfileArchive::load(&extensions,&mut p::Budget::default(),required).is_err());
    let skip=p::ExtensionPolicy{optional:p::Unknown::Skip,..Default::default()};assert_eq!(ProfileArchive::load(&extensions,&mut p::Budget::default(),skip).unwrap().extensions.parts.len(),0);
    // Shared counters cross part boundaries, with exact field/link diagnostics on exhaustion.
    let mut measured=p::Budget::default();ProfileArchive::load(&encoded,&mut measured,p::ExtensionPolicy::default()).unwrap();
    for resource in [p::Resource::PhysicalBytes,p::Resource::ExpandedBytes,p::Resource::MetadataBytes,p::Resource::Entries,p::Resource::Relationships,p::Resource::References,p::Resource::WorkUnits,p::Resource::XmlNodes,p::Resource::XmlAttributes,p::Resource::ScalarBytes,p::Resource::DecoderAllocationBytes]{let index=resource as usize;assert!(measured.used[index]>0);let mut budget=p::Budget::default();budget.limits.maximum[index]=measured.used[index]-1;assert!(ProfileArchive::load(&encoded,&mut budget,p::ExtensionPolicy::default()).is_err(),"Resource {:?}",resource);}
    let mut budget=p::Budget::default();budget.limits.maximum[p::Resource::XmlNodes as usize]=measured.used[p::Resource::XmlNodes as usize]-1;let error=ProfileArchive::load(&encoded,&mut budget,p::ExtensionPolicy::default()).err().unwrap();let failure=error.downcast_ref::<p::Failure>().unwrap();assert_eq!(failure.code,p::Code::ResourceLimit);assert_eq!(failure.part,"/3D/model.xml");assert_eq!(failure.field,"main");assert!(!failure.link.is_empty());assert!(failure.cause.is_some());
    let mut budget=p::Budget::default();budget.limits.maximum[p::Resource::PreservedBytes as usize]=0;let error=ProfileArchive::load(&extensions,&mut budget,p::ExtensionPolicy::preserve()).err().unwrap();let failure=error.downcast_ref::<p::Failure>().unwrap();assert_eq!(failure.code,p::Code::ResourceLimit);assert_eq!(failure.part,"/Extensions/vendor.xml");
    let mut budget=p::Budget::default();budget.limits.max_archive_bytes=encoded.len()-1;assert!(ProfileArchive::read(&mut &encoded[..],&mut budget,p::ExtensionPolicy::default()).is_err());
    let marker:p::Error=std::sync::Arc::new(Marker);let cancel=||Err(marker.clone());let mut budget=p::Budget{check_cancelled:Some(&cancel),..Default::default()};
    let error=ProfileArchive::encode(&candidate,&mut budget).unwrap_err();assert!(std::sync::Arc::ptr_eq(&error,&marker));
    let polls=std::cell::Cell::new(0);let cancel_spool=||{polls.set(polls.get()+1);if polls.get()>=3{Err(marker.clone())}else{Ok(())}};let mut source=ShortSource{bytes:&encoded,position:0};let mut budget=p::Budget{check_cancelled:Some(&cancel_spool),..Default::default()};let error=ProfileArchive::read(&mut source,&mut budget,p::ExtensionPolicy::default()).err().unwrap();assert!(std::sync::Arc::ptr_eq(&error,&marker));assert!(source.position<encoded.len());
    assert!(std::fs::read_dir(&spool_temp).unwrap().next().is_none());
    // Fixed/default profiles reject unsupported changes before model-codec work.
    let fixed=SelectedFixedInput{main:Some(SelectedNode{key:"fixed".into(),id:9}),..Default::default()};
    let mut budget=p::Budget::default();budget.overrides.insert("main".into(),0);
    let error=SelectedFixed::encode(&fixed,&mut budget).unwrap_err();assert_eq!(error.downcast_ref::<p::Failure>().unwrap().code,p::Code::InvalidOptions);
    let defaults=SelectedDefaultsInput{main:Some(SelectedNode{key:"default".into(),id:8}),..Default::default()};
    let mut budget=p::Budget::default();budget.overrides.insert("main".into(),0);let bytes=SelectedDefaults::encode(&defaults,&mut budget).unwrap();let archive=p::ZipReader::new(&bytes,&mut p::Budget::default()).unwrap();assert_eq!(archive.entries.iter().find(|entry|entry.name=="node.xml").unwrap().method,0);
    assert!(SelectedFixed::load(&bytes,&mut p::Budget::default(),p::ExtensionPolicy::default()).is_err());
    let mut budget=p::Budget::default();budget.overrides.insert("unknown".into(),8);assert!(SelectedDefaults::encode(&defaults,&mut budget).is_err());
    assert_eq!(SelectedFixed::profiles()[0].root,Some(("urn:serializer:selected","node")));
    // Generated key/reference selectors validate forward edges and exact uint64 keys atomically.
    let graph_value=graph();let packed=SelectedGraph::encode(&graph_value,&mut p::Budget::default()).unwrap();
    assert_eq!(SelectedGraph::load(&packed,&mut p::Budget::default(),p::ExtensionPolicy::default()).unwrap().nodes[0].value.id,u64::MAX);
    let mut aliased=graph();aliased.alias_nodes=std::mem::take(&mut aliased.nodes);let alias_bytes=SelectedGraph::encode(&aliased,&mut p::Budget::default()).unwrap();assert_eq!(SelectedGraph::load(&alias_bytes,&mut p::Budget::default(),p::ExtensionPolicy::default()).unwrap().alias_nodes.len(),1);
    let mut missing=graph();missing.nodes.clear();assert!(SelectedGraph::encode(&missing,&mut p::Budget::default()).is_err());
    let mut wrong=graph();wrong.nodes.clear();wrong.others.push(p::ModelPart{name:"/other.xml".into(),content_type:"application/vnd.selected-other+xml".into(),source_index:0,value:SelectedOther{key:"name".into()}});assert!(SelectedGraph::encode(&wrong,&mut p::Budget::default()).is_err());
    let mut duplicate=graph();duplicate.nodes.push(duplicate.nodes[0].clone());assert!(SelectedGraph::encode(&duplicate,&mut p::Budget::default()).is_err());
    let mut wrong_key=graph();wrong_key.links[0].value.target_id=u64::MAX-1;assert!(SelectedGraph::encode(&wrong_key,&mut p::Budget::default()).is_err());
    let mut budget=p::Budget::default();budget.limits.maximum[p::Resource::References as usize]=3;let error=SelectedGraph::encode(&graph_value,&mut budget).unwrap_err();let failure=error.downcast_ref::<p::Failure>().unwrap();assert!(!failure.part.is_empty());assert!(!failure.field.is_empty());
    // Every pre-replacement fault keeps the old file and removes the private create-new stage.
    let destination=directory.join("atomic.opc");
    for point in ["before_create","after_write","before_replace"]{std::fs::write(&destination,b"old").unwrap();let fault=|stage:&str|if stage==point{Err(marker.clone())}else{Ok(())};let error=ProfileArchive::save(&destination,&candidate,&mut p::Budget::default(),Some(&fault)).unwrap_err();assert!(std::sync::Arc::ptr_eq(&error,&marker));assert_eq!(std::fs::read(&destination).unwrap(),b"old");}
    ProfileArchive::save(&destination,&candidate,&mut p::Budget::default(),None).unwrap();assert!(ProfileArchive::load(&std::fs::read(&destination).unwrap(),&mut p::Budget::default(),p::ExtensionPolicy::default()).is_ok());
    std::fs::write(&destination,b"old").unwrap();let fault=|stage:&str|if stage=="after_replace"{Err(marker.clone())}else{Ok(())};assert!(ProfileArchive::save(&destination,&candidate,&mut p::Budget::default(),Some(&fault)).is_err());assert!(ProfileArchive::load(&std::fs::read(&destination).unwrap(),&mut p::Budget::default(),p::ExtensionPolicy::default()).is_ok());
    std::fs::write(&destination,b"old").unwrap();let cancelled=std::cell::Cell::new(false);let check=||if cancelled.get(){Err(marker.clone())}else{Ok(())};let fault=|stage:&str|{if stage=="before_replace"{cancelled.set(true);}Ok(())};let mut budget=p::Budget{check_cancelled:Some(&check),..Default::default()};assert!(ProfileArchive::save(&destination,&candidate,&mut budget,Some(&fault)).is_err());assert_eq!(std::fs::read(&destination).unwrap(),b"old");
    // Stable relationship IDs derive from part identities, so input permutations are byte-identical.
    std::fs::write(directory.join("canonical-a.opc"),ProfileArchive::encode(&authored(false),&mut p::Budget::default()).unwrap()).unwrap();
    std::fs::write(directory.join("canonical-b.opc"),ProfileArchive::encode(&authored(true),&mut p::Budget::default()).unwrap()).unwrap();
    assert!(p::target("/a/source.xml","../../escape.xml").is_err());
    for name in ["/a/../b","/a/%2f/b","/a/%41","/trailing.","relative"]{assert!(p::part_name(name).is_err());}
    for count in [65534usize,65535,65536]{count_output(&directory,count);}
    let stored=p::BinaryPart{name:"stored.bin".into(),data:b"independent stored bytes".to_vec(),..Default::default()};for(wide,name)in[(false,"stored.zip"),(true,"stored64.zip")]{let bytes=p::zip_write(std::slice::from_ref(&stored),&mut p::Budget{force_zip64:wide,..Default::default()}).unwrap();std::fs::write(directory.join(name),bytes).unwrap();}
    let _=BTreeMap::<String,String>::new();
    println!("PASS Rust {} independent fixtures, generated roles/profiles/references, shared bounds and atomic Save",count);
}
