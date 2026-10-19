// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
mod schema;
use schema::{Protocol, XsdExampleOrder, XmlLimits};
use std::{env, fs};

/// Run generated native Rust XML methods against the shared independent XML fixture.
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = env::args().collect();
    if args.len() != 3 { return Err("Usage: serializer-xml-example INPUT OUTPUT".into()); }
    let input = fs::read_to_string(&args[1])?;
    let value = XsdExampleOrder::decode_xml(&input)?;
    assert_eq!(value.id, 7);
    assert_eq!(value.customer, "R&D");
    assert_eq!(value.count, 3);
    assert_eq!(value.tag, vec!["A".to_owned(), "B".to_owned()]);
    let canonical = value.encode(Protocol::BinaryNone)?;
    let output = value.encode_xml()?;
    assert_eq!(XsdExampleOrder::decode_xml(&output)?.encode(Protocol::BinaryNone)?, canonical);
    assert!(XsdExampleOrder::decode_xml("<wrong/>").is_err());
    assert!(XsdExampleOrder::decode_xml("<!DOCTYPE order><order/>").is_err());
    assert!(XsdExampleOrder::decode_xml_with_validation(&input, XmlLimits { max_bytes:1,
        ..XmlLimits::default() }, None).is_err());
    assert!(XsdExampleOrder::decode_xml(&input.replace("id=\"7\"", "id=\"4294967296\"")).is_err());
    let body = input.trim_start_matches('\u{feff}');
    let body = if body.starts_with("<?xml") { body.split_once("?>").ok_or("Missing declaration end")?.1 } else { body };
    assert!(XsdExampleOrder::decode_xml(&format!("<?xml version=\"1.0\" encoding=\"UTF-16\"?>{}", body)).is_err());
    assert!(XsdExampleOrder::decode_xml(&format!("<?xml version=\"1.1\"?>{}", body)).is_err());
    fs::write(&args[2], output)?;
    println!("Generated Rust XML codec passed");
    Ok(())
}
