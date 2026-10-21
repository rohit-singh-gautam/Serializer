// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
mod schema;

use schema::{EvolutionOrder, Protocol};
use std::{env, fs, path::Path};

/// Check typed storage and authored behavior independently of serialization equivalence.
fn check_authored(value: &EvolutionOrder) {
    assert_eq!(value.id, 7);
    assert_eq!(value.customer, "R&D");
    assert_eq!(value.note.as_deref(), Some("memo"));
    assert!(value.has_id(7));
    assert!(!value.has_id(8));
}

/// Exercise ordinary generated APIs and write artifacts only after decoding and validation succeed.
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let arguments: Vec<_> = env::args().collect();
    assert!(arguments.len() >= 4 && arguments[1] == "@variant@", "VARIANT emit OUTDIR | check-xml/check-native INPUT OUTPUT");
    let defaults = EvolutionOrder::default();
    assert_eq!(defaults.id, 7);
    assert_eq!(defaults.customer, "native default");
    assert_eq!(defaults.note, None);
    assert!(defaults.has_id(7));
    assert!(!defaults.has_id(8));
    if arguments[2] == "emit" {
        assert_eq!(arguments.len(), 4);
        let value = EvolutionOrder { id: 7, customer: "R&D".to_owned(), note: Some("memo".to_owned()) };
        check_authored(&value);
        let xml = value.encode_xml()?;
        let projection = EvolutionOrder::decode_xml(&xml)?;
        check_authored(&projection);
        let artifacts = [
            ("authored.xml", xml.into_bytes()),
            ("authored.bin", value.encode(Protocol::BinaryNone)?),
            ("authored-integer.bin", value.encode(Protocol::BinaryInteger)?),
            ("authored-string.bin", value.encode(Protocol::BinaryString)?),
            ("default.bin", defaults.encode(Protocol::BinaryNone)?),
            ("xml-projection.bin", projection.encode(Protocol::BinaryNone)?),
        ];
        for (name, bytes) in artifacts {
            fs::write(Path::new(&arguments[3]).join(name), bytes)?;
        }
    } else {
        assert_eq!(arguments.len(), 5);
        let bytes = fs::read(&arguments[3])?;
        let output = match arguments[2].as_str() {
            "check-xml" => {
                let value = EvolutionOrder::decode_xml_bytes(&bytes)?;
                check_authored(&value);
                value.encode(Protocol::BinaryNone)?
            }
            "check-native" => {
                let value = EvolutionOrder::decode(&bytes, Protocol::BinaryNone)?;
                check_authored(&value);
                value.encode_xml()?.into_bytes()
            }
            _ => return Err("Unknown operation".into()),
        };
        fs::write(&arguments[4], output)?;
    }
    println!("Rust authored XSD evolution passed");
    Ok(())
}
