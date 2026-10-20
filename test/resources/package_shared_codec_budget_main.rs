// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
mod identity;
use identity::*;
use identity::@p@ as package;
use identity::@operation@ as operation;
use std::sync::Arc;

const RESOURCE_COUNT: usize = operation::Resource::References as usize + 1;
const EXPECTED_NATIVE_REFERENCES: usize = 12;
const EXPECTED_PACKAGE_REFERENCES: u64 = 15;

#[derive(Debug)]
struct Marker(&'static str);

impl std::fmt::Display for Marker {
    /// Give caller failures a stable diagnostic without changing their Arc identity.
    fn fmt(&self, output: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        output.write_str(self.0)
    }
}
impl std::error::Error for Marker {}

struct Ledger {
    used: [usize; RESOURCE_COUNT],
    maximum: [usize; RESOURCE_COUNT],
    marker: operation::Failure,
    reject: bool,
    polls: usize,
    fields: Vec<String>,
    codec_field: String,
}

impl Default for Ledger {
    /// Construct an independent caller-owned ledger with no process-wide operation state.
    fn default() -> Self {
        Self {
            used: [0; RESOURCE_COUNT], maximum: [usize::MAX; RESOURCE_COUNT],
            marker: Arc::new(Marker("original caller operation failure")), reject: false,
            polls: 0, fields: Vec::new(), codec_field: String::new(),
        }
    }
}

impl operation::Operation for Ledger {
    /// Charge before native work and retain schema/constraint names independently of XML spellings.
    fn consume(&mut self, resource: operation::Resource, count: usize, field: &str) -> operation::Result<()> {
        self.fields.push(field.to_owned());
        let index = resource as usize;
        if self.reject || count > self.maximum[index].saturating_sub(self.used[index]) {
            return Err(self.marker.clone());
        }
        self.used[index] += count;
        Ok(())
    }

    /// Preserve the caller's original error object through operation cancellation.
    fn poll(&mut self, field: &str) -> operation::Result<()> {
        self.polls += 1;
        self.fields.push(field.to_owned());
        if self.reject { Err(self.marker.clone()) } else { Ok(()) }
    }

    /// Native-local quotas use the same caller-defined failure channel and member context.
    fn limit(&mut self, _message: &'static str, field: &str) -> operation::Failure {
        self.fields.push(field.to_owned());
        self.marker.clone()
    }

    /// Preserve the exact failing semantic member while retaining ordinary native Error values.
    fn codec(&mut self, error: Error, field: &str) -> operation::Failure {
        self.codec_field = field.to_owned();
        Arc::new(error)
    }
}

/// Compose independently scoped owners while accumulating the same caller-owned native ledger.
fn decode_three(xml: &str, ledger: &mut Ledger) -> operation::Result<()> {
    for _ in 0..3 {
        let model = <IdentityXmlRegistry as operation::Codec>::decode_xml_with_operation(
            xml, operation::Options::default(), ledger)?;
        assert_eq!(model.requests.len(), 2);
        assert_eq!(model.entries.len(), 1);
        assert_eq!(model.requests[0].id, Some(7));
    }
    Ok(())
}

/// Keep the existing public limits literal, method pointers, native bytes and errors compatible.
fn native_contract(xml: &str, expected: &[usize]) -> (usize, usize) {
    let old_limits = XmlLimits {
        max_bytes: 16 * 1024 * 1024, max_text_bytes: 8 * 1024 * 1024,
        max_nodes: 100000, max_attributes: 100000, max_depth: 128,
    };
    let old_decode: fn(&str, XmlLimits, Option<&dyn XmlValidator>) -> Result<IdentityXmlRegistry, Error> =
        IdentityXmlRegistry::decode_xml_with_validation;
    let old_encode: fn(&IdentityXmlRegistry, XmlLimits, Option<&dyn XmlValidator>) -> Result<String, Error> =
        IdentityXmlRegistry::encode_xml_with_validation;
    let model = old_decode(xml, old_limits, None).unwrap();
    let old_bytes = old_encode(&model, old_limits, None).unwrap();
    let mut encoding = Ledger::default();
    let new_bytes = <IdentityXmlRegistry as operation::Codec>::encode_xml_with_operation(
        &model, operation::Options { limits: old_limits, ..Default::default() }, &mut encoding).unwrap();
    assert_eq!(old_bytes, new_bytes);
    assert_eq!(encoding.used[operation::Resource::References as usize], 4);
    let bad_xml = xml.replace("<ref id=\"7\"/>", "<ref id=\"8\"/>");
    let old_error = old_decode(&bad_xml, old_limits, None).unwrap_err().to_string();
    assert!(!old_error.is_empty());
    let mut invalid = Ledger::default();
    let new_error = <IdentityXmlRegistry as operation::Codec>::decode_xml_with_operation(
        &bad_xml, operation::Options::default(), &mut invalid).unwrap_err();
    assert_eq!(new_error.to_string(), old_error);
    assert!(new_error.downcast_ref::<Error>().is_some());

    let mut measured = Ledger::default();
    decode_three(xml, &mut measured).unwrap();
    assert_eq!(measured.used[operation::Resource::References as usize], EXPECTED_NATIVE_REFERENCES);
    assert_eq!(measured.used[operation::Resource::XmlNodes as usize], expected[0]);
    assert_eq!(measured.used[operation::Resource::XmlAttributes as usize], expected[1]);
    assert_eq!(measured.used[operation::Resource::ScalarBytes as usize], expected[2]);
    let mut passed = 8;
    for resource in [operation::Resource::References, operation::Resource::WorkUnits,
                     operation::Resource::DecoderAllocationBytes] {
        let index = resource as usize;
        for tight in [false, true] {
            let mut ledger = Ledger::default();
            ledger.maximum[index] = measured.used[index] - usize::from(tight);
            let result = decode_three(xml, &mut ledger);
            if tight {
                let error = result.unwrap_err();
                assert!(Arc::ptr_eq(&error, &ledger.marker));
                assert!(ledger.fields.last().is_some_and(|field| !field.is_empty()));
            } else { result.unwrap(); }
            passed += 1;
        }
    }
    let mut ledger = Ledger { reject: true, ..Default::default() };
    let error = decode_three(xml, &mut ledger).unwrap_err();
    assert!(Arc::ptr_eq(&error, &ledger.marker));
    ledger.reject = false;
    decode_three(xml, &mut ledger).unwrap();
    assert_eq!(ledger.used[operation::Resource::References as usize], EXPECTED_NATIVE_REFERENCES);
    passed += 2;

    let mut local = Ledger::default();
    let options = operation::Options {
        limits: XmlLimits { max_depth: 0, ..old_limits }, ..Default::default()
    };
    let error = <IdentityXmlRegistry as operation::Codec>::decode_xml_with_operation(xml, options, &mut local).unwrap_err();
    assert!(Arc::ptr_eq(&error, &local.marker));
    assert!(local.fields.iter().any(|field| !field.is_empty()));
    passed += 1;
    std::thread::scope(|scope| {
        let first = scope.spawn(|| { let mut ledger = Ledger::default(); decode_three(xml, &mut ledger).unwrap(); ledger.used });
        let second = scope.spawn(|| { let mut ledger = Ledger::default(); decode_three(xml, &mut ledger).unwrap(); ledger.used });
        assert_eq!(first.join().unwrap(), measured.used);
        assert_eq!(second.join().unwrap(), measured.used);
    });
    (passed + 2, measured.used[operation::Resource::ScalarBytes as usize])
}

/// Traverse nested, inherited and concrete generic owners without merging independent key scopes.
#[cfg(feature = "nested")]
fn nested_contract(xml: &str) -> usize {
    let model = IdentityXmlRegistry::decode_xml(xml).unwrap();
    let mut pair = NativeOperationPair { first: model.clone(), second: model.clone() };
    let mut ledger = Ledger::default();
    let bytes = <NativeOperationPair as operation::Codec>::encode_xml_with_operation(
        &pair, operation::Options::default(), &mut ledger).unwrap();
    assert_eq!(ledger.used[operation::Resource::References as usize], 8);
    ledger = Ledger::default();
    let decoded = <NativeOperationPair as operation::Codec>::decode_xml_with_operation(
        &bytes, operation::Options::default(), &mut ledger).unwrap();
    assert_eq!(decoded, pair);
    assert_eq!(ledger.used[operation::Resource::References as usize], 8);
    pair.second.requests[1].id = Some(8);
    ledger = Ledger::default();
    assert!(<NativeOperationPair as operation::Codec>::encode_xml_with_operation(
        &pair, operation::Options::default(), &mut ledger).is_err());
    assert!(ledger.codec_field.contains("requests.id"), "{}", ledger.codec_field);
    let inherited = NativeOperationInherited { base0: model.clone() };
    ledger = Ledger::default();
    <NativeOperationInherited as operation::Codec>::encode_xml_with_operation(
        &inherited, operation::Options::default(), &mut ledger).unwrap();
    assert_eq!(ledger.used[operation::Resource::References as usize], 4);
    let mut generic = NativeOperationGenericOwner::default();
    generic.wrapped.value = model;
    ledger = Ledger::default();
    <NativeOperationGenericOwner as operation::Codec>::encode_xml_with_operation(
        &generic, operation::Options::default(), &mut ledger).unwrap();
    assert_eq!(ledger.used[operation::Resource::References as usize], 4);
    7
}

/// Exercise shared physical/native ledgers and failure before aggregate or file publication.
fn package_contract(directory: &std::path::Path, bytes: &[u8]) -> usize {
    let mut budget = package::Budget::default();
    let mut value = IdentityPackageArchive::load(bytes, &mut budget, package::ExtensionPolicy::default()).unwrap();
    assert_eq!(value.registries.len(), 3);
    assert_eq!(budget.used[package::Resource::References as usize], EXPECTED_PACKAGE_REFERENCES);
    let measured = budget.used;
    let mut passed = 2;
    for resource in [package::Resource::References, package::Resource::WorkUnits,
                     package::Resource::DecoderAllocationBytes] {
        let index = resource as usize;
        for tight in [false, true] {
            let mut budget = package::Budget::default();
            budget.limits.maximum[index] = measured[index] - u64::from(tight);
            let result = IdentityPackageArchive::load(bytes, &mut budget, package::ExtensionPolicy::default());
            if tight {
                let error = result.err().unwrap();
                let failure = error.downcast_ref::<package::Failure>().unwrap();
                assert_eq!(failure.code, package::Code::ResourceLimit);
                assert!(!failure.part.is_empty() && !failure.field.is_empty() && !failure.link.is_empty(), "{failure:?}");
            } else { assert_eq!(result.unwrap().registries.len(), 3); }
            passed += 1;
        }
    }
    let original = value.registries[1].value.clone();
    for name in ["middle-invalid.opc", "middle-scalar.opc"] {
        let bad = std::fs::read(directory.join(name)).unwrap();
        let error = IdentityPackageArchive::load(&bad, &mut package::Budget::default(), package::ExtensionPolicy::default()).err().unwrap();
        let failure = error.downcast_ref::<package::Failure>().unwrap();
        assert_eq!(failure.code, package::Code::Codec);
        assert_eq!(failure.part, "/Registries/registry1.xml");
        assert_eq!(failure.link, "record1");
        assert!(!failure.field.is_empty());
        assert_eq!(value.registries[1].value, original);
        passed += 1;
    }
    let header = std::fs::read(directory.join("header-quota.opc")).unwrap();
    let header_limit = std::fs::read_to_string(directory.join("header-quota-limit.txt")).unwrap()
        .trim().parse::<usize>().unwrap();
    let mut budget = package::Budget::default(); budget.limits.max_part_bytes = header_limit;
    let mut header_calls = 0;
    let error = IdentityPackageArchive::load_with_codecs(&header, &mut budget, package::ExtensionPolicy::default(),
        |text: &str, context: &mut package::XmlOperation<'_, '_>| {
            header_calls += 1; context.decode::<IdentityXmlRegistry>(text)
        }).err().unwrap();
    let failure = error.downcast_ref::<package::Failure>().unwrap();
    assert_eq!(failure.code, package::Code::ResourceLimit, "{failure:?}");
    assert_eq!(failure.part, "/Registries/registry1.xml");
    assert_eq!(failure.field, "registries"); assert_eq!(failure.link, "record1");
    assert_eq!(header_calls, 1, "The bounded second header must fail before its selected codec");
    assert_eq!(value.registries[1].value, original);
    passed += 2;
    let namespace = "urn:".to_owned() + &"h".repeat(4096);
    let attributes = (0..9).map(|index| format!(" h:attribute{index}=\"\"")).collect::<String>();
    let mut budget = package::Budget::default();
    let mut authored_link = String::new();
    let oversized_header = format!("<registry xmlns=\"urn:serializer:identity-test\" xmlns:h=\"{namespace}\"{attributes}/>");
    budget.limits.max_part_bytes = oversized_header.len();
    let error = IdentityPackageArchive::encode_with_codecs(&value, &mut budget,
        |_model: &IdentityXmlRegistry, context: &mut package::XmlOperation<'_, '_>| {
            authored_link = context.link.to_owned(); Ok(oversized_header.clone())
        }).unwrap_err();
    let failure = error.downcast_ref::<package::Failure>().unwrap();
    assert_eq!(failure.code, package::Code::ResourceLimit, "{failure:?}");
    assert_eq!(failure.part, "/Registries/registry0.xml");
    assert_eq!(failure.field, "registries"); assert_eq!(failure.link, authored_link);
    assert!(!failure.link.is_empty()); assert_eq!(value.registries[1].value, original);
    passed += 2;
    let header_marker: package::Error = Arc::new(Marker("original header-boundary cancellation"));
    let header_phase = std::cell::Cell::new(0u8);
    let cancel_header = || match header_phase.get() {
        1 => { header_phase.set(2); Ok(()) },
        2 => Err(header_marker.clone()),
        _ => Ok(()),
    };
    let mut budget = package::Budget { check_cancelled: Some(&cancel_header), ..Default::default() };
    budget.limits.max_part_bytes = oversized_header.len() - 1;
    let error = IdentityPackageArchive::encode_with_codecs(&value, &mut budget,
        |_model: &IdentityXmlRegistry, _context: &mut package::XmlOperation<'_, '_>| {
            // The immediate callback-finish poll succeeds; cancellation becomes due at header entry.
            header_phase.set(1); Ok(oversized_header.clone())
        }).unwrap_err();
    assert!(Arc::ptr_eq(&error, &header_marker));
    assert_eq!(header_phase.get(), 2); assert_eq!(value.registries[1].value, original);
    passed += 1;
    let mut selected_calls = 0;
    let composed = IdentityPackageArchive::load_with_codecs(bytes, &mut package::Budget::default(),
        package::ExtensionPolicy::default(), |text: &str, context: &mut package::XmlOperation<'_, '_>| {
            selected_calls += 1;
            let _extra = context.decode::<IdentityXmlRegistry>(text)?;
            context.decode::<IdentityXmlRegistry>(text)
        }).unwrap();
    assert_eq!(selected_calls, 3);
    assert_eq!(composed.registries.len(), 3);
    let mut composed_budget = package::Budget::default();
    IdentityPackageArchive::load_with_codecs(bytes, &mut composed_budget, package::ExtensionPolicy::default(),
        |text: &str, context: &mut package::XmlOperation<'_, '_>| {
            let _extra = context.decode::<IdentityXmlRegistry>(text)?;
            context.decode::<IdentityXmlRegistry>(text)
        }).unwrap();
    assert_eq!(composed_budget.used[package::Resource::References as usize], 27);
    let marker: package::Error = Arc::new(Marker("original selected codec failure"));
    let mut selected_calls = 0;
    let error = IdentityPackageArchive::load_with_codecs(bytes, &mut package::Budget::default(),
        package::ExtensionPolicy::default(), |text: &str, context: &mut package::XmlOperation<'_, '_>| {
            selected_calls += 1;
            if selected_calls == 2 {
                assert_eq!(context.part, "/Registries/registry1.xml");
                assert_eq!(context.link, "record1");
                Err(marker.clone())
            } else { context.decode::<IdentityXmlRegistry>(text) }
        }).err().unwrap();
    assert!(Arc::ptr_eq(&error, &marker));
    assert_eq!(selected_calls, 2);
    assert_eq!(value.registries[1].value, original);
    let cancelled = std::cell::Cell::new(false);
    let cancel_middle = || if cancelled.get() { Err(marker.clone()) } else { Ok(()) };
    let mut budget = package::Budget { check_cancelled: Some(&cancel_middle), ..Default::default() };
    let mut selected_calls = 0;
    let error = IdentityPackageArchive::load_with_codecs(bytes, &mut budget, package::ExtensionPolicy::default(),
        |text: &str, context: &mut package::XmlOperation<'_, '_>| {
            selected_calls += 1;
            if selected_calls == 2 { cancelled.set(true); }
            context.decode::<IdentityXmlRegistry>(text)
        }).err().unwrap();
    assert!(Arc::ptr_eq(&error, &marker));
    assert_eq!(selected_calls, 2);
    let bad = std::fs::read(directory.join("middle-invalid.opc")).unwrap();
    let error = IdentityPackageArchive::load_with_codecs(&bad, &mut package::Budget::default(), package::ExtensionPolicy::default(),
        |text: &str, context: &mut package::XmlOperation<'_, '_>| {
            Ok(context.decode::<IdentityXmlRegistry>(text).unwrap_or_default())
        }).err().unwrap();
    let failure = error.downcast_ref::<package::Failure>().unwrap();
    assert_eq!(failure.part, "/Registries/registry1.xml");
    assert_eq!(failure.link, "record1");
    assert!(failure.field.contains("requests.id"));
    passed += 8;
    std::fs::write(directory.join("produced.opc"), IdentityPackageArchive::encode(&value, &mut package::Budget::default()).unwrap()).unwrap();
    value.registries[1].value.requests[1].id = Some(8);
    let destination = directory.join("atomic.opc");
    std::fs::write(&destination, b"previous destination").unwrap();
    let error = IdentityPackageArchive::save(&destination, &value, &mut package::Budget::default(), None).unwrap_err();
    assert_eq!(error.downcast_ref::<package::Failure>().unwrap().code, package::Code::Codec);
    assert_eq!(std::fs::read(&destination).unwrap(), b"previous destination");
    value.registries[1].value = original;
    let mut encoding_calls = 0;
    let error = IdentityPackageArchive::save_with_codecs(&destination, &value, &mut package::Budget::default(), None,
        |model: &IdentityXmlRegistry, context: &mut package::XmlOperation<'_, '_>| {
            encoding_calls += 1;
            if encoding_calls == 2 { Err(marker.clone()) } else { context.encode(model) }
        }, |text: &str, context: &mut package::XmlOperation<'_, '_>| context.decode::<IdentityXmlRegistry>(text)).unwrap_err();
    assert!(Arc::ptr_eq(&error, &marker));
    assert_eq!(std::fs::read(&destination).unwrap(), b"previous destination");
    let marker: package::Error = Arc::new(Marker("original package cancellation"));
    let cancel = || Err(marker.clone());
    let mut budget = package::Budget { check_cancelled: Some(&cancel), ..Default::default() };
    let error = IdentityPackageArchive::save(&destination, &value, &mut budget, None).unwrap_err();
    assert!(Arc::ptr_eq(&error, &marker));
    assert_eq!(std::fs::read(&destination).unwrap(), b"previous destination");
    passed + 5
}

/// Execute the generated Rust SDK and emit compact machine-readable acceptance counts.
fn main() {
    let directory = std::path::PathBuf::from(std::env::args_os().nth(1).unwrap());
    let xml = std::fs::read_to_string(directory.join("registry.xml")).unwrap();
    let bytes = std::fs::read(directory.join("valid.opc")).unwrap();
    let expected = std::fs::read_to_string(directory.join("inventory.txt")).unwrap().split_whitespace()
        .map(|value| value.parse::<usize>().unwrap()).collect::<Vec<_>>();
    assert_eq!(expected.len(), 3);
    let (native_checks, native_scalar_bytes) = native_contract(&xml, &expected);
    let passed = native_checks + package_contract(&directory, &bytes);
    #[cfg(feature = "nested")]
    let passed = passed + nested_contract(&xml);
    println!("{{\"passed\":{passed},\"native_references\":{EXPECTED_NATIVE_REFERENCES},\"package_references\":{EXPECTED_PACKAGE_REFERENCES},\"native_scalar_bytes\":{native_scalar_bytes}}}");
}
