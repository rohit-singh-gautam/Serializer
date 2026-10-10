mod schema;
use schema::{ExampleModel, Protocol};
use std::{env, fs};

/// Check a complete versioned model after every protocol round trip.
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = env::args().collect();
    let value = ExampleModel::decode(&fs::read(&args[1])?, Protocol::Json)?;
    let canonical = value.encode(Protocol::BinaryNone)?;
    for protocol in [Protocol::Json, Protocol::BinaryNone, Protocol::BinaryInteger, Protocol::BinaryString] {
        let copy = ExampleModel::decode(&value.encode(protocol)?, protocol)?;
        assert_eq!(copy.encode(Protocol::BinaryNone)?, canonical);
    }
    fs::write(&args[2], value.encode(Protocol::Json)?)?;
    println!("Four protocols passed");
    Ok(())
}
