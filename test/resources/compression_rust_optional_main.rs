// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
use std::{error::Error, fs, path::Path};
mod schema;
mod controls;
use schema::__COMPRESSION_NAME__ as c;
serializer_compression_providers::zstd_provider!(zstd_adapter,crate::schema::__COMPRESSION_NAME__);
serializer_compression_providers::lz4_provider!(lz4_adapter,crate::schema::__COMPRESSION_NAME__);
mod adapters { pub use crate::{zstd_adapter::Zstd,lz4_adapter::Lz4}; }
type Result<T> = std::result::Result<T, Box<dyn Error>>;

// Exercise every independent success/error/resource decision through staged generated framing.
fn provider_corpus(directory: &Path) -> Result<()> {
    for (name,format,provider) in [("ZSTD",c::Format::Zstd,&adapters::Zstd as &dyn c::Provider),
        ("LZ4",c::Format::Lz4,&adapters::Lz4 as &dyn c::Provider)] {
        let mut count = 0;
        for row in fs::read_to_string(directory.join(name.to_string()+".tsv"))?.lines() {
            let cells: Vec<_> = row.split('\t').collect();
            let packed = fs::read(directory.join(cells[0]))?;
            let options = c::DecodeOptions {format,max_input_bytes:cells[3].parse()?,max_output_bytes:cells[4].parse()?,max_window_bytes:cells[5].parse()?,check_cancelled:None};
            let result = c::decompress_with_provider(&packed,options,provider);
            if cells[2] == "ok" {
                let actual = result.map_err(|error| error.to_string())?;
                let expected = fs::read(directory.join(cells[1]))?;
                assert_eq!(actual,expected,"{}",cells[0]);
                let mut source = std::io::Cursor::new(&packed);
                assert_eq!(c::decompress_from_with_provider(&mut source,options,provider).map_err(|error| error.to_string())?,expected);
            } else {
                let error = result.expect_err(cells[0]);
                let code = error.downcast_ref::<c::Failure>().ok_or_else(|| format!("Missing compression category for {}: {error:?}",cells[0]))?.code;
                let expected = match cells[2] {"INVALID_DATA" => c::Code::InvalidData,"RESOURCE_LIMIT" => c::Code::ResourceLimit,_ => return Err("Unknown fixture category".into())};
                assert_eq!(code,expected,"{}",cells[0]);
            }
            count += 1;
        }
        println!("PASS actual generated {name} decisions {count}");
        let level = if format == c::Format::Lz4 {0} else {3};
        let plain = vec![b'A';65537];
        let options = c::EncodeOptions {format,level,..Default::default()};
        let frame = c::compress_with_provider(&plain,options,provider).map_err(|error| error.to_string())?;
        fs::write(Path::new(env!("CARGO_MANIFEST_DIR")).join("rust-output-".to_owned()+name+".bin"),&frame)?;
        let mut sink = Vec::new();
        { let mut output = c::Output::with_provider(&mut sink,options,provider).map_err(|error| error.to_string())?;
          output.write(&plain).map_err(|error| error.to_string())?; output.finish().map_err(|error| error.to_string())?; }
        assert_eq!(c::decompress_with_provider(&sink,c::DecodeOptions {format,..Default::default()},provider).map_err(|error| error.to_string())?,plain);
    }
    Ok(())
}

// Replay independent frames and real native model/lifecycle controls with explicitly borrowed providers.
fn main() -> Result<()> {
    let directory = std::env::args().nth(1).ok_or("Expected independent fixture directory")?;
    provider_corpus(Path::new(&directory))?;
    controls::run(Path::new(env!("CARGO_MANIFEST_DIR")));
    Ok(())
}
