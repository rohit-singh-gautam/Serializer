mod schema;
use schema::*;
use std::{env, fs};

/// Edit an owned model and check complete values after every protocol round trip.
fn main() -> Result<(), Box<dyn std::error::Error>> {
    {
        let typed = Uint16Version::default();
        assert_eq!(typed.encode(Protocol::BinaryNone)?, vec![44, 1]);
        for protocol in [Protocol::Json, Protocol::BinaryNone, Protocol::BinaryInteger, Protocol::BinaryString] {
            let copy = Uint16Version::decode(&typed.encode(protocol)?, protocol)?;
            assert_eq!(copy.encode(Protocol::BinaryNone)?, typed.encode(Protocol::BinaryNone)?);
        }
    }
    {
        let typed = Uint32Version::default();
        assert_eq!(typed.encode(Protocol::BinaryNone)?, vec![112, 17, 1, 0]);
        for protocol in [Protocol::Json, Protocol::BinaryNone, Protocol::BinaryInteger, Protocol::BinaryString] {
            let copy = Uint32Version::decode(&typed.encode(protocol)?, protocol)?;
            assert_eq!(copy.encode(Protocol::BinaryNone)?, typed.encode(Protocol::BinaryNone)?);
        }
    }
    {
        let typed = Uint64Version::default();
        assert_eq!(typed.encode(Protocol::BinaryNone)?, vec![255, 255, 255, 255, 255, 255, 255, 255]);
        for protocol in [Protocol::Json, Protocol::BinaryNone, Protocol::BinaryInteger, Protocol::BinaryString] {
            let copy = Uint64Version::decode(&typed.encode(protocol)?, protocol)?;
            assert_eq!(copy.encode(Protocol::BinaryNone)?, typed.encode(Protocol::BinaryNone)?);
        }
    }
    {
        let typed = FloatVersion::default();
        assert_eq!(typed.encode(Protocol::BinaryNone)?, vec![205, 204, 204, 61]);
        for protocol in [Protocol::Json, Protocol::BinaryNone, Protocol::BinaryInteger, Protocol::BinaryString] {
            let copy = FloatVersion::decode(&typed.encode(protocol)?, protocol)?;
            assert_eq!(copy.encode(Protocol::BinaryNone)?, typed.encode(Protocol::BinaryNone)?);
        }
    }
    {
        let typed = DoubleVersion::default();
        assert_eq!(typed.encode(Protocol::BinaryNone)?, vec![0, 0, 0, 0, 0, 0, 4, 64]);
        for protocol in [Protocol::Json, Protocol::BinaryNone, Protocol::BinaryInteger, Protocol::BinaryString] {
            let copy = DoubleVersion::decode(&typed.encode(protocol)?, protocol)?;
            assert_eq!(copy.encode(Protocol::BinaryNone)?, typed.encode(Protocol::BinaryNone)?);
        }
    }
    {
        let typed = Dotted2Version::default();
        assert_eq!(typed.encode(Protocol::BinaryNone)?, vec![1, 0, 10, 0]);
        for protocol in [Protocol::Json, Protocol::BinaryNone, Protocol::BinaryInteger, Protocol::BinaryString] {
            let copy = Dotted2Version::decode(&typed.encode(protocol)?, protocol)?;
            assert_eq!(copy.encode(Protocol::BinaryNone)?, typed.encode(Protocol::BinaryNone)?);
        }
    }
    {
        let typed = Dotted3Version::default();
        assert_eq!(typed.encode(Protocol::BinaryNone)?, vec![1, 0, 10, 0, 0, 0]);
        for protocol in [Protocol::Json, Protocol::BinaryNone, Protocol::BinaryInteger, Protocol::BinaryString] {
            let copy = Dotted3Version::decode(&typed.encode(protocol)?, protocol)?;
            assert_eq!(copy.encode(Protocol::BinaryNone)?, typed.encode(Protocol::BinaryNone)?);
        }
    }
    {
        let typed = Dotted4Version::default();
        assert_eq!(typed.encode(Protocol::BinaryNone)?, vec![1, 0, 10, 0, 0, 0, 4, 0]);
        for protocol in [Protocol::Json, Protocol::BinaryNone, Protocol::BinaryInteger, Protocol::BinaryString] {
            let copy = Dotted4Version::decode(&typed.encode(protocol)?, protocol)?;
            assert_eq!(copy.encode(Protocol::BinaryNone)?, typed.encode(Protocol::BinaryNone)?);
        }
    }
    let args: Vec<String> = env::args().collect();
    let mut value = ExampleModel::decode_with_limits(&fs::read(&args[1])?, Protocol::Json, Limits { read_policy: ReadPolicy::Compatible, ..Limits::default() })?;
    let old = ExampleModel::decode_with_limits(&value.encode(Protocol::BinaryNone)?, Protocol::BinaryNone, Limits { read_policy: ReadPolicy::Compatible, ..Limits::default() })?;
    assert_eq!(old.old_name, "Ada");
    value.name = value.old_name.clone();
    value.version = 10;
    let canonical = value.encode(Protocol::BinaryNone)?;
    for protocol in [Protocol::Json, Protocol::BinaryNone, Protocol::BinaryInteger, Protocol::BinaryString] {
        let copy = ExampleModel::decode(&value.encode(protocol)?, protocol)?;
        assert_eq!(copy.encode(Protocol::BinaryNone)?, canonical);
    }
    fs::write(&args[2], value.encode(Protocol::Json)?)?;
    println!("Version migration and four protocols passed");
    Ok(())
}
