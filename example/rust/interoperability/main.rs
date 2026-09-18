mod schema;
use schema::{InteropDetail, InteropMessage, InteropState, Protocol};
use std::{env, fs, path::Path, time::Instant};

const PROTOCOLS: [(&str, Protocol); 4] = [
    ("JSON", Protocol::Json), ("BINARY_NONE", Protocol::BinaryNone),
    ("BINARY_INTEGER", Protocol::BinaryInteger), ("BINARY_STRING", Protocol::BinaryString),
];

/// Construct an independent expected value, preserving integer extrema and negative zero.
fn fixture(variant: usize) -> InteropMessage {
    let child = InteropDetail { code: -9, note: "child".into() };
    InteropMessage {
        text: format!("Ada \"Lovelace\" 🚀\n{}", "x".repeat(64)),
        numbers: vec![i32::MIN, -1, 0, i32::MAX],
        decimals: vec![-0.0, 1.5, -2.25], flags: vec![false, true],
        labels: vec!["".into(), "é".into(), "🚀".into()],
        children: vec![InteropDetail::default(), child.clone()],
        states: vec![InteropState::Paused, InteropState::Ready],
        counts: [("🚀".into(), u64::MAX), ("é".into(), 7), ("a".into(), 0)].into(),
        indexed: [(u64::MAX, child), (0, InteropDetail::default())].into(),
        toggles: [(true, "yes".into()), (false, "no".into())].into(),
        enums: [(InteropState::Paused, -2), (InteropState::Ready, 1)].into(),
        payload_index: variant, payload_number: -1234567890123456789,
        payload_ratio: -3.5, payload_state: InteropState::Paused,
        ..InteropMessage::default()
    }
}

/// Exchange complete messages with every producer listed by the test orchestrator.
fn exchange(directory: &Path, mode: &str) -> Result<(), Box<dyn std::error::Error>> {
    fs::create_dir_all(directory)?;
    let producers = fs::read_to_string(directory.join("producers.txt"))?;
    for variant in 0..3 {
        let expected = fixture(variant);
        let canonical = expected.encode(Protocol::BinaryNone)?;
        for (name, protocol) in PROTOCOLS {
            let encoded = expected.encode(protocol)?;
            if mode == "emit" {
                fs::write(directory.join(format!("rust_{name}_{variant}.bin")), &encoded)?;
            } else {
                for producer in producers.split_whitespace() {
                    let data = fs::read(directory.join(format!("{producer}_{name}_{variant}.bin")))?;
                    let actual = InteropMessage::decode(&data, protocol)?;
                    assert_eq!(actual.encode(Protocol::BinaryNone)?, canonical, "{producer} -> rust {name}/{variant}");
                    if protocol != Protocol::Json { assert_eq!(data, encoded, "Canonical binary mismatch"); }
                }
            }
        }
    }
    println!("rust {mode} passed"); Ok(())
}

/// Report warmed local timing samples including allocations and output construction.
fn benchmark() -> Result<(), Box<dyn std::error::Error>> {
    let value = fixture(0); let iterations = 10000;
    for (name, protocol) in PROTOCOLS {
        let bytes = value.encode(protocol)?;
        for _ in 0..1000 { std::hint::black_box(InteropMessage::decode(&value.encode(protocol)?, protocol)?); }
        let start = Instant::now();
        for _ in 0..iterations { std::hint::black_box(value.encode(protocol)?); }
        let encode = start.elapsed().as_secs_f64(); let start = Instant::now();
        for _ in 0..iterations { std::hint::black_box(InteropMessage::decode(&bytes, protocol)?); }
        println!("rust {name}: {} bytes; encode {:.0}/s; decode {:.0}/s", bytes.len(), iterations as f64/encode, iterations as f64/start.elapsed().as_secs_f64());
    }
    Ok(())
}

/// Run a standalone producer, consumer, or local benchmark using generated Rust only.
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<_> = env::args().collect();
    if args.len() != 3 { return Err("Usage: main <fixtures-directory> emit|verify|benchmark".into()); }
    match args[2].as_str() {
        "benchmark" => benchmark(), "emit" | "verify" => exchange(Path::new(&args[1]), &args[2]),
        _ => Err("Unknown mode".into()),
    }
}
