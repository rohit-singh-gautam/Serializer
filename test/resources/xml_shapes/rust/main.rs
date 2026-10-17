// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
mod schema;
use schema::{Error, Protocol, ShapesChild, ShapesRecord, ShapesState, ShapesCatalog, XmlLimits, XmlValidator};

/// A caller-prepared validator that proves both generated hook directions are enforced.
struct RejectSchema;
impl XmlValidator for RejectSchema {
    /// Reject a complete document as a prepared XSD provider would report an invalid instance.
    fn validate(&self, _document: &str) -> Result<(), Error> { Err(Error("Prepared XSD rejection")) }
}

/// Qualify generated nested, optional, fixed enum, exact numeric and scalar text mappings.
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let catalog_xml = "<catalog xmlns='urn:serializer:vocabulary'><prices><price currency='USD'>12.5</price></prices><labels><label>A&amp;B</label><label/></labels></catalog>";
    let catalog = ShapesCatalog::decode_xml(catalog_xml)?;
    assert_eq!(catalog.prices[0].currency, "USD"); assert_eq!(catalog.prices[0].value, 12.5);
    assert_eq!(catalog.labels, vec!["A&B", ""]);
    assert_eq!(catalog.encode(Protocol::BinaryNone)?, ShapesCatalog::decode_xml(&catalog.encode_xml()?)?.encode(Protocol::BinaryNone)?);
    assert!(ShapesCatalog::decode_xml(&ShapesCatalog::default().encode_xml()?)?.prices.is_empty());
    assert!(ShapesCatalog::decode_xml(&catalog_xml.replace("<prices>", "<prices extra='1'>")).is_err());
    let mut value = ShapesRecord::default();
    value.big = u64::MAX;
    value.label = "\tX\r\n\u{1f600}".to_owned();
    value.flag = Some(true);
    value.nested.value = i16::MIN;
    value.children = vec![ShapesChild { value: i16::MAX }];
    value.states = vec![ShapesState::Zero, ShapesState::One];
    value.amount = f64::INFINITY;
    value.small = 1.25;
    let encoded = value.encode_xml()?;
    assert_eq!(ShapesRecord::decode_xml(&encoded)?.encode(Protocol::BinaryNone)?, value.encode(Protocol::BinaryNone)?);
    value.flag = None;
    assert_eq!(ShapesRecord::decode_xml(&value.encode_xml()?)?.flag, None);
    assert_eq!(ShapesRecord::decode_xml(&encoded.replace("big=\"18446744073709551615\"", "big=\"-000\""))?.big, 0);
    assert!(ShapesRecord::decode_xml(&encoded.replace("big=\"18446744073709551615\"", "big=\"-\"")).is_err());
    assert!(ShapesRecord::decode_xml(&encoded.replace("<value>-32768</value>", "<value>\u{a0}-32768</value>")).is_err());
    assert!(ShapesRecord::decode_xml(&encoded.replace("<states>one</states>", "")).is_err());
    assert!(ShapesRecord::decode_xml(&encoded.replace("<states>one</states>", "<states>unknown</states>")).is_err());
    assert!(value.encode_xml_with_validation(XmlLimits { max_depth:1, ..XmlLimits::default() }, None).is_err());
    value.small = -0.0;
    assert_eq!(ShapesRecord::decode_xml(&value.encode_xml()?)?.encode(Protocol::BinaryNone)?, value.encode(Protocol::BinaryNone)?);
    assert!(ShapesRecord::decode_xml_with_validation(&encoded, XmlLimits::default(), Some(&RejectSchema)).is_err());
    assert!(value.encode_xml_with_validation(XmlLimits::default(), Some(&RejectSchema)).is_err());
    println!("Rust generated typed XML and prepared-validator hooks passed");
    Ok(())
}
