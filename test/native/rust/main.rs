mod schema;
use schema::*;
use std::{env, fs, path::Path};

/// Dispatch typed owning codecs; errors never expose partially decoded models.
fn decode(kind: usize, bytes: &[u8], protocol: Protocol, limits: Limits) -> Result<Vec<u8>, Error> {
    match kind {
        0 => InteropMessage::decode_with_limits(bytes, protocol, limits)?.encode(Protocol::Json),
        1 => CheckTextValue::decode_with_limits(bytes, protocol, limits)?.encode(Protocol::Json),
        2 => CheckIntegerValue::decode_with_limits(bytes, protocol, limits)?.encode(Protocol::Json),
        3 => CheckBoolValue::decode_with_limits(bytes, protocol, limits)?.encode(Protocol::Json),
        4 => CheckFloatValue::decode_with_limits(bytes, protocol, limits)?.encode(Protocol::Json),
        5 => CheckBytesValue::decode_with_limits(bytes, protocol, limits)?.encode(Protocol::Json),
        6 => CheckEmpty::decode_with_limits(bytes, protocol, limits)?.encode(Protocol::Json),
        7 => CheckRecursive::decode_with_limits(bytes, protocol, limits)?.encode(Protocol::Json),
        8 => CheckVisibility::decode_with_limits(bytes, protocol, limits)?.encode(Protocol::Json),
        9 => CheckDefaultValue::decode_with_limits(bytes, protocol, limits)?.encode(Protocol::Json),
        _ => panic!("Unknown test type"),
    }
}

/// Run all corpus cases in one process so rejection tests measure codecs, not SDK startup.
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = env::args().collect();
    let directory = Path::new(&args[1]); let output = Path::new(&args[2]);
    let mut statuses = String::new();
    for line in fs::read_to_string(directory.join("manifest.tsv"))?.lines() {
        let fields: Vec<usize> = line.split_whitespace().map(|x| x.parse().unwrap()).collect();
        let bytes = fs::read(directory.join(format!("{}.bin", fields[0])))?;
        let protocol = [Protocol::Json, Protocol::BinaryNone, Protocol::BinaryInteger, Protocol::BinaryString][fields[2]];
        let limits = Limits { max_bytes: fields[3], max_string_bytes: fields[4], max_elements: fields[5], max_depth: fields[6] };
        match decode(fields[1], &bytes, protocol, limits) {
            Ok(json) => { fs::write(output.join(format!("{}.json", fields[0])), json)?; statuses.push_str("OK\n"); }
            Err(_) => statuses.push_str("ERR\n"),
        }
    }
    fs::write(output.join("results.txt"), statuses)?;
    Ok(())
}
