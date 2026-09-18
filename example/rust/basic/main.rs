mod schema;
use schema::{ExampleModel, Protocol};
use std::{env, fs};

/// Edit an owned model and check complete values after every protocol round trip.
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = env::args().collect();
    let mut value = ExampleModel::decode(&fs::read(&args[1])?, Protocol::Json)?;
    value.revision += 1;
    let canonical = value.encode(Protocol::BinaryNone)?;
    for protocol in [Protocol::Json, Protocol::BinaryNone, Protocol::BinaryInteger, Protocol::BinaryString] {
        let copy = ExampleModel::decode(&value.encode(protocol)?, protocol)?;
        assert_eq!(copy.encode(Protocol::BinaryNone)?, canonical);
    }
    fs::write(&args[2], value.encode(Protocol::Json)?)?;
    println!("Four protocols passed");
    Ok(())
}
