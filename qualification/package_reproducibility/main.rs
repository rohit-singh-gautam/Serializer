// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
mod schema;

use schema::@p@ as package;
use std::{env, fs, path::Path};

const FIXTURE_LIMIT_BYTES: u64 = 16 * 1024 * 1024;
const PERMUTATION_COUNT: usize = 6;

/// Retain exact UTF-8/LF field spelling, including an empty final hexadecimal column.
fn rows(directory: &Path, kind: &str, name: &str, columns: usize) -> Vec<Vec<String>> {
    let path = directory.join(kind).join(name);
    assert!(fs::metadata(&path).expect("fixture metadata").len() <= FIXTURE_LIMIT_BYTES);
    let text = fs::read_to_string(path).expect("UTF-8 fixture TSV");
    assert!(!text.contains(['\r', '\0']), "fixtures require UTF-8/LF");
    text.split_terminator('\n')
        .map(|line| {
            let values: Vec<_> = line.split('\t').map(str::to_owned).collect();
            assert_eq!(values.len(), columns, "fixture column count");
            values
        })
        .collect()
}

/// Decode binary payloads directly rather than interpreting them as native strings.
fn decode_hex(value: &str) -> Vec<u8> {
    assert_eq!(value.len() % 2, 0, "hexadecimal extent");
    value.as_bytes().chunks_exact(2)
        .map(|pair| {
            let digit = |value: u8| match value {
                b'0'..=b'9' => value - b'0',
                b'a'..=b'f' => value - b'a' + 10,
                b'A'..=b'F' => value - b'A' + 10,
                _ => panic!("invalid hexadecimal fixture payload"),
            };
            digit(pair[0]) * 16 + digit(pair[1])
        })
        .collect()
}

/// Reorder only caller inputs; the generated writer retains its legacy canonical sort.
fn permuted<T>(values: Vec<T>, selected: usize) -> Vec<T> {
    assert!(selected < PERMUTATION_COUNT);
    let count = values.len();
    let order: Vec<_> = match selected {
        0 => (0..count).collect(),
        1 => (0..count).rev().collect(),
        2 => (0..count).map(|index| (index + 1) % count).collect(),
        3 => (0..count).map(|index| (index + count / 2) % count).collect(),
        4 => (0..count).step_by(2).chain((1..count).step_by(2)).collect(),
        5 => (0..count).rev().filter(|index| index % 2 == 1)
            .chain((0..count).rev().filter(|index| index % 2 == 0)).collect(),
        _ => unreachable!(),
    };
    let mut slots: Vec<_> = values.into_iter().map(Some).collect();
    order.into_iter().map(|index| slots[index].take().expect("unique permutation index")).collect()
}

/// Fingerprint the exact dependency-free generated provider and unchanged raw-DEFLATE defaults.
fn provider() -> &'static str {
    concat!("{\"implementation\":\"Serializer generated fixed-Huffman LZ77\",",
        "\"version\":\"sha256:@provider_sha256@\",",
        "\"options\":{\"level\":6,\"window_bytes\":32768,\"wrapper\":\"raw\",",
        "\"match_attempts\":32,\"xml_provider\":\"Serializer generated XML metadata\"},",
        "\"metadata_method\":8}")
}

/// Produce one archive through public generated ZIP/Composer APIs in a fresh process.
fn main() {
    let arguments: Vec<_> = env::args().collect();
    if arguments.len() == 2 && arguments[1] == "--provider" {
        println!("{}", provider());
        return;
    }
    assert!(matches!(arguments.len(), 5 | 6), "fixture-dir profile permutation output [force64 0|1]");
    let profile = arguments[2].as_str();
    let opc = matches!(profile, "opc-stored" | "opc-deflate");
    assert!(opc || matches!(profile, "zip-stored" | "zip-deflate"), "unknown profile");
    let method = if matches!(profile, "zip-deflate" | "opc-deflate") { 8 } else { 0 };
    let selected: usize = arguments[3].parse().expect("permutation number");
    assert!(selected < PERMUTATION_COUNT);
    let force = arguments.get(5).is_some_and(|value| value == "1");
    assert!(arguments.get(5).is_none_or(|value| value == "0" || value == "1"));
    let directory = Path::new(&arguments[1]);
    let mut parts = Vec::new();
    for row in rows(directory, if opc { "opc" } else { "zip" }, "parts.tsv", 4) {
        assert!(matches!(row[2].as_str(), "stored" | "deflate"), "unknown TSV storage");
        parts.push(package::BinaryPart {
            name: row[0].clone(), content_type: row[1].clone(), data: decode_hex(&row[3]),
            source_index: 0, method,
        });
    }
    let part_count = parts.len();
    parts = permuted(parts, selected);
    let mut links = Vec::new();
    if opc {
        for row in rows(directory, "opc", "links.tsv", 5) {
            assert!(matches!(row[4].as_str(), "internal" | "external"), "unknown target mode");
            links.push(package::Relationship {
                source: row[0].clone(), id: row[1].clone(), kind: row[2].clone(),
                target: row[3].clone(), external: row[4] == "external",
            });
        }
    }
    let link_count = links.len();
    links = permuted(links, selected);
    let mut budget = package::Budget { force_zip64: force, ..Default::default() };
    let output = if opc {
        package::Composer::new(&mut budget, &Default::default())
            .finish(&package::Extensions { parts, relationships: links })
    } else {
        package::zip_write(&parts, &mut budget)
    }.expect("generated package producer");
    fs::write(&arguments[4], output).expect("publish producer output");
    println!("{{\"profile\":\"{profile}\",\"permutation\":{selected},\"parts\":{part_count},\"relationships\":{link_count},\"force_zip64\":{force},\"provider\":{}}}", provider());
}
