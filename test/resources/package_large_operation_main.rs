// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
mod identity;
use identity::IdentityPackageArchive as Archive;
use identity::serializer_package as package;
use std::cell::Cell;
use std::path::Path;
use std::sync::Arc;
use std::time::{Duration, Instant};

const MEBIBYTE: u64 = 1024 * 1024;
const SENTINEL: &[u8] = b"previous complete application file";

#[derive(Debug)]
struct Marker(&'static str);
impl std::fmt::Display for Marker {
    /// Retain a typed caller error without substituting provider/runtime failures.
    fn fmt(&self, output: &mut std::fmt::Formatter<'_>) -> std::fmt::Result { output.write_str(self.0) }
}
impl std::error::Error for Marker {}

/// Select finite large policies and actual DEFLATE without changing model or root identities.
fn limits<'a>() -> package::Budget<'a> {
    let mut value = package::Budget::default();
    value.limits.max_part_bytes = (16 * MEBIBYTE) as usize;
    value.limits.max_archive_bytes = (128 * MEBIBYTE) as usize;
    for resource in [package::Resource::PhysicalBytes, package::Resource::ExpandedBytes,
                     package::Resource::ScalarBytes] { value.limits.maximum[resource as usize] = 1024 * MEBIBYTE; }
    value.limits.maximum[package::Resource::MetadataBytes as usize] = 64 * MEBIBYTE;
    value.limits.maximum[package::Resource::DecoderAllocationBytes as usize] = 2048 * MEBIBYTE;
    value.limits.maximum[package::Resource::WorkUnits as usize] = 8192 * MEBIBYTE;
    value.limits.maximum[package::Resource::References as usize] = 4_000_000;
    value.overrides.insert("registries".into(), 8);
    value
}

/// Keep original compressed decode, identity owners, preservation and atomic Save on the same public ledger.
fn operation(bytes: &[u8], destination: &Path, budget: &mut package::Budget<'_>, count: usize) -> package::Result<()> {
    let value = Archive::load(bytes, budget, package::ExtensionPolicy::preserve())?;
    assert_eq!(value.registries.len(), 3);
    for registry in &value.registries {
        assert_eq!(registry.value.requests.len(), count);
        assert_eq!(registry.value.entries.len(), count);
    }
    assert_eq!(value.extensions.parts.len(), 1);
    assert_eq!(value.extensions.relationships.len(), 3);
    Archive::save(destination, &value, budget, None)
}

/// Exercise exact large cumulative caps and first-error/publication behavior in an actual owning Rust consumer.
fn run() -> package::Result<()> {
    let arguments: Vec<_> = std::env::args().collect();
    assert_eq!(arguments.len(), 4);
    let directory = Path::new(&arguments[1]);
    let count: usize = arguments[2].parse().unwrap();
    let expected_references: u64 = arguments[3].parse().unwrap();
    let destination = directory.join("produced.opc");
    let valid = std::fs::read(directory.join("valid.opc")).map_err(package::native_error)?;
    let invalid = std::fs::read(directory.join("invalid.opc")).map_err(package::native_error)?;
    let mut passed = 0;
    macro_rules! check { ($value:expr,$message:expr) => {{ assert!($value,"{}",$message); passed += 1; }}; }
    let polls = Cell::new(0usize);
    let observe = || { polls.set(polls.get()+1); Ok(()) };
    let mut baseline = limits(); baseline.check_cancelled = Some(&observe);
    operation(&valid, &destination, &mut baseline, count)?;
    check!(baseline.used[package::Resource::References as usize] >= expected_references*3, "one cumulative identity/relationship ledger");
    for resource in [package::Resource::References, package::Resource::WorkUnits, package::Resource::DecoderAllocationBytes] {
        for tight in [false, true] {
            std::fs::write(&destination, SENTINEL).map_err(package::native_error)?;
            let mut budget = limits();
            budget.limits.maximum[resource as usize] = baseline.used[resource as usize]-u64::from(tight);
            let result = operation(&valid, &destination, &mut budget, count);
            if tight {
                let error = result.err().expect("one-tight resource accepted");
                check!(error.downcast_ref::<package::Failure>().is_some_and(|failure|failure.code==package::Code::ResourceLimit),"resource limit category");
                check!(std::fs::read(&destination).unwrap()==SENTINEL,"one-tight preserves old file");
            } else { check!(result.is_ok(),"exact cumulative admission"); }
        }
    }
    let failed = Archive::load(&invalid, &mut limits(), package::ExtensionPolicy::preserve());
    let error = failed.err().expect("missing final-owner keyref accepted");
    check!(error.downcast_ref::<package::Failure>().is_some_and(|failure|failure.code==package::Code::Codec&&failure.part.ends_with("registry2.xml")),"late invalid identity returns no aggregate and retains owner");
    let boundaries = [1, (polls.get()/2).max(2), polls.get()];
    for clock in [false, true] {
        for boundary in boundaries {
            std::fs::write(&destination, SENTINEL).map_err(package::native_error)?;
            let observed = Cell::new(0usize);
            let deadline = Cell::new(None::<Instant>);
            let marker = package::native_error(Marker(if clock {"original host clock expiry"} else {"original callback cancellation"}));
            let callback = || {
                observed.set(observed.get()+1);
                if observed.get()==boundary {
                    if clock { deadline.set(Some(Instant::now()+Duration::from_millis(1))); std::thread::sleep(Duration::from_millis(2)); }
                    else { return Err(marker.clone()); }
                }
                if deadline.get().is_some_and(|end|Instant::now()>=end) { Err(marker.clone()) } else { Ok(()) }
            };
            let mut budget = limits(); budget.check_cancelled = Some(&callback);
            let error = operation(&valid, &destination, &mut budget, count).err().expect("large interruption accepted");
            check!(Arc::ptr_eq(&error,&marker),"original callback/host-clock Arc identity");
            check!(std::fs::read(&destination).unwrap()==SENTINEL,"large interruption preserves old file");
        }
    }
    let loaded = Archive::load(&valid,&mut limits(),package::ExtensionPolicy::preserve())?;
    for clock in [false,true] {
        std::fs::write(&destination,SENTINEL).map_err(package::native_error)?;
        let armed=Cell::new(false);let deadline=Cell::new(None::<Instant>);
        let marker=package::native_error(Marker("original last-publication policy"));
        let callback=|| {if armed.get()&&(!clock||deadline.get().is_some_and(|end|Instant::now()>=end)){Err(marker.clone())}else{Ok(())}};
        let fault=|boundary:&str| {if boundary=="before_replace" {armed.set(true);if clock{deadline.set(Some(Instant::now()+Duration::from_millis(1)));std::thread::sleep(Duration::from_millis(2));}}Ok(())};
        let mut budget=limits();budget.check_cancelled=Some(&callback);
        let error=Archive::save(&destination,&loaded,&mut budget,Some(&fault)).err().expect("last callback policy ignored");
        check!(Arc::ptr_eq(&error,&marker)&&std::fs::read(&destination).unwrap()==SENTINEL,"last callback cannot publish");
    }
    operation(&valid,&destination,&mut limits(),count)?;
    let output=std::fs::read(&destination).map_err(package::native_error)?;
    let recovered=Archive::load(&output,&mut limits(),package::ExtensionPolicy::preserve())?;
    check!(recovered.registries.len()==3,"fresh-call load/Save/reopen recovery");
    println!("{{\"checks\":{passed},\"pipeline_references\":{},\"pipeline_work\":{},\"pipeline_allocation\":{},\"pipeline_polls\":{},\"interruptions\":8,\"deadline_surface\":\"host monotonic predicate retains original expiry Arc\"}}",baseline.used[package::Resource::References as usize],baseline.used[package::Resource::WorkUnits as usize],baseline.used[package::Resource::DecoderAllocationBytes as usize],polls.get());
    Ok(())
}

/// Keep failure diagnostics visible and avoid producing a successful verification record on panic/error.
fn main() { if let Err(error)=run(){eprintln!("{error}");std::process::exit(1);} }
