// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
mod schema;
use schema::*;
use schema::@operation@ as operation;
use operation::{Codec, Options, Resource};
use std::sync::Arc;

const RESOURCE_COUNT: usize = Resource::References as usize + 1;

#[derive(Debug)]
struct Marker;
impl std::fmt::Display for Marker {
    /// Retain one recognizable caller failure independently of native diagnostic messages.
    fn fmt(&self, output: &mut std::fmt::Formatter<'_>) -> std::fmt::Result { output.write_str("caller marker") }
}
impl std::error::Error for Marker {}

struct Ledger {
    used: [usize; RESOURCE_COUNT], maximum: [usize; RESOURCE_COUNT],
    marker: operation::Failure, local: usize, field: String, deny_default: bool,
}
impl Default for Ledger {
    /// Give every invocation independent counters while preserving one exact caller-owned Arc.
    fn default() -> Self {
        Self { used: [0; RESOURCE_COUNT], maximum: [usize::MAX; RESOURCE_COUNT],
            marker: Arc::new(Marker), local: 0, field: String::new(), deny_default: false }
    }
}
impl operation::Operation for Ledger {
    /// Admit before storage or traversal and deliberately reject known native default construction.
    fn consume(&mut self, resource: Resource, count: usize, field: &str) -> operation::Result<()> {
        self.field = field.to_owned();
        if self.deny_default && resource == Resource::DecoderAllocationBytes &&
            field == "operation_policy::child.wireValue" && count == "known default storage".len() {
            return Err(self.marker.clone());
        }
        let index = resource as usize;
        let Some(total) = self.used[index].checked_add(count).filter(|total| *total <= self.maximum[index]) else {
            return Err(self.marker.clone());
        };
        self.used[index] = total; Ok(())
    }
    /// Keep cancellation independent of native policy and schema classification.
    fn poll(&mut self, _field: &str) -> operation::Result<()> { Ok(()) }
    /// Mark only explicit configured native resource failures.
    fn limit(&mut self, _message: &'static str, field: &str) -> operation::Failure {
        self.local += 1; self.field = field.to_owned(); self.marker.clone()
    }
    /// Preserve ordinary native semantic failures and the canonical schema projection.
    fn codec(&mut self, error: Error, field: &str) -> operation::Failure {
        self.field = field.to_owned(); Arc::new(error)
    }
}

/// Compare complete ordinary bytes and exercise exact/one-tight XML accounting on both directions.
fn exercise<T: Codec + std::fmt::Debug>(text: &str, path: &std::path::Path,
    decode: fn(&str) -> Result<T, Error>, encode: fn(&T) -> Result<String, Error>) -> usize {
    let mut input = Ledger::default();
    let value = T::decode_xml_with_operation(text, Options::default(), &mut input).unwrap();
    let ordinary = decode(text).unwrap();
    assert_eq!(encode(&ordinary).unwrap(), encode(&value).unwrap());
    let mut output = Ledger::default();
    let bytes = value.encode_xml_with_operation(Options::default(), &mut output).unwrap();
    assert_eq!(bytes, encode(&ordinary).unwrap());
    std::fs::write(path, &bytes).unwrap();
    let mut checks = 4;
    for resource in [Resource::XmlNodes, Resource::XmlAttributes, Resource::ScalarBytes,
                     Resource::WorkUnits, Resource::DecoderAllocationBytes] {
        let index = resource as usize;
        for writing in [false, true] {
            let measured = if writing {output.used[index]} else {input.used[index]};
            for tight in [false, true] {
                if measured == 0 && tight { continue; }
                let mut ledger = Ledger::default(); ledger.maximum[index] = measured - usize::from(tight);
                let result = if writing {value.encode_xml_with_operation(Options::default(), &mut ledger).map(|_| ())}
                    else {T::decode_xml_with_operation(text, Options::default(), &mut ledger).map(|_| ())};
                if tight {
                    let error = result.unwrap_err(); assert!(Arc::ptr_eq(&error, &ledger.marker));
                    assert!(!ledger.field.is_empty());
                } else { result.unwrap(); }
                checks += 1;
            }
        }
    }
    println!("{{\"input\":{:?},\"output\":{:?}}}", input.used, output.used);
    checks
}

/// Require a configured native cap to retain the exact local resource failure object.
fn local<T: Codec + std::fmt::Debug>(text: &str, options: Options<'_>) {
    let output_options = Options { limits: options.limits, map_limits: options.map_limits,
        mixed_limits: options.mixed_limits, digest_limits: options.digest_limits,
        identity_limits: options.identity_limits, validator: options.validator };
    let value = T::decode_xml_with_operation(text, Options::default(), &mut Ledger::default()).unwrap();
    let mut ledger = Ledger::default();
    let error = T::decode_xml_with_operation(text, options, &mut ledger).unwrap_err();
    assert!(Arc::ptr_eq(&error, &ledger.marker)); assert_eq!(ledger.local, 1); assert!(!ledger.field.is_empty());
    let mut ledger = Ledger::default();
    let error = value.encode_xml_with_operation(output_options, &mut ledger).unwrap_err();
    assert!(Arc::ptr_eq(&error, &ledger.marker)); assert_eq!(ledger.local, 1); assert!(!ledger.field.is_empty());
}

/// Require schema cardinality and lexical failures to keep the ordinary native Error type.
fn semantic<T: Codec + std::fmt::Debug>(text: &str, expected: &str) {
    let mut ledger = Ledger::default();
    let error = T::decode_xml_with_operation(text, Options::default(), &mut ledger).unwrap_err();
    assert!(error.downcast_ref::<Error>().is_some()); assert_eq!(ledger.local, 0);
    assert!(ledger.field.contains(expected), "{}", ledger.field);
}

/// Keep the canonical native text member when parser lookahead exhausts its final scalar admission.
fn scalar_location<T: Codec + std::fmt::Debug>(text: &str, expected: &str) {
    let mut measured = Ledger::default();
    T::decode_xml_with_operation(text, Options::default(), &mut measured).unwrap();
    let mut ledger = Ledger::default();
    ledger.maximum[Resource::ScalarBytes as usize] = measured.used[Resource::ScalarBytes as usize] - 1;
    let error = T::decode_xml_with_operation(text, Options::default(), &mut ledger).unwrap_err();
    assert!(Arc::ptr_eq(&error, &ledger.marker)); assert_eq!(ledger.field, expected);
}

/// Run a selected policy against actual emitted native code; no provider or package replay substitutes it.
fn main() {
    let arguments: Vec<String> = std::env::args().collect();
    let text = std::fs::read_to_string(&arguments[2]).unwrap();
    let path = std::path::Path::new(&arguments[3]);
    let assertions = match arguments[1].as_str() {
        "scalar" => {
            let count = exercise::<OperationPolicyScalar>(&text, path, OperationPolicyScalar::decode_xml, OperationPolicyScalar::encode_xml);
            scalar_location::<OperationPolicyScalar>(&text, "operation_policy::scalar.value");
            local::<OperationPolicyScalar>(&text, Options { limits: XmlLimits { max_nodes: 1, ..Default::default() }, ..Default::default() });
            semantic::<OperationPolicyScalar>("<scalar>1</scalar>", "operation_policy::scalar.value"); count + 4
        },
        "map" => {
            let count = exercise::<OperationPolicyMaps>(&text, path, OperationPolicyMaps::decode_xml, OperationPolicyMaps::encode_xml);
            scalar_location::<OperationPolicyMaps>(&text, "operation_policy::maps.entries");
            local::<OperationPolicyMaps>(&text, Options { map_limits: XmlMapLimits { max_entries: 0, max_key_bytes: 100 }, ..Default::default() });
            local::<OperationPolicyMaps>(&text, Options { map_limits: XmlMapLimits { max_entries: 2, max_key_bytes: 0 }, ..Default::default() });
            semantic::<OperationPolicyMaps>(&text.replace("id=\"2\"", "id=\"1\""), "entries"); count + 6
        },
        "mixed" => {
            let count = exercise::<OperationPolicyMixed>(&text, path, OperationPolicyMixed::decode_xml, OperationPolicyMixed::encode_xml);
            scalar_location::<OperationPolicyMixed>(&text, "operation_policy::mixed.content");
            local::<OperationPolicyMixed>(&text, Options { mixed_limits: XmlMixedLimits { max_fragments: 0, max_text_bytes: 100 }, ..Default::default() });
            local::<OperationPolicyMixed>(&text, Options { mixed_limits: XmlMixedLimits { max_fragments: 10, max_text_bytes: 0 }, ..Default::default() });
            semantic::<OperationPolicyMixed>(&text.replace("head", "<unknown/>"), "operation_policy::mixed"); count + 6
        },
        "digest" => {
            let count = exercise::<OperationPolicyDigests>(&text, path, OperationPolicyDigests::decode_xml, OperationPolicyDigests::encode_xml);
            local::<OperationPolicyDigests>(&text, Options { digest_limits: XmlDigestLimits { max_bytes: 1 }, ..Default::default() });
            semantic::<OperationPolicyDigests>("<digests exact=\"000000\"/>", "operation_policy::digests.exact");
            let mut value = OperationPolicyDigests::decode_xml(&text).unwrap(); value.exact.push(0);
            let mut ledger = Ledger::default();
            let error = value.encode_xml_with_operation(Options::default(), &mut ledger).unwrap_err();
            assert!(error.downcast_ref::<Error>().is_some()); assert_eq!(ledger.local, 0);
            assert_eq!(ledger.field, "operation_policy::digests.exact"); count + 4
        },
        "defaults" => {
            let count = exercise::<OperationPolicyDefaults>(&text, path, OperationPolicyDefaults::decode_xml, OperationPolicyDefaults::encode_xml);
            let mut ledger = Ledger { deny_default: true, ..Default::default() };
            let error = OperationPolicyDefaults::decode_xml_with_operation(&text, Options::default(), &mut ledger).unwrap_err();
            assert!(Arc::ptr_eq(&error, &ledger.marker)); assert_eq!(ledger.field, "operation_policy::child.wireValue");
            ledger.deny_default = false;
            OperationPolicyDefaults::decode_xml_with_operation(&text, Options::default(), &mut ledger).unwrap();
            semantic::<OperationPolicyChild>("<child bytes=\"000000\"><wireValue/></child><extra/>", "operation_policy::child"); count + 3
        },
        _ => panic!("Unknown policy"),
    };
    println!("{{\"assertions\":{assertions}}}");
}
