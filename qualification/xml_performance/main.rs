// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
mod schema;
use schema::{Protocol, XmlExampleOrder};
use std::alloc::{GlobalAlloc, Layout, System};
use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use std::{env, fs, hint::black_box, time::Instant};

struct CountingAllocator;
static COUNTING: AtomicBool = AtomicBool::new(false);
static ALLOCATED: AtomicUsize = AtomicUsize::new(0);
#[global_allocator]
static ALLOCATOR: CountingAllocator = CountingAllocator;

// Count successful Rust allocation requests only during the separate allocation sample.
unsafe impl GlobalAlloc for CountingAllocator {
    // Delegate ordinary allocation and count its successful requested extent.
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        let pointer = System.alloc(layout);
        if !pointer.is_null() && COUNTING.load(Ordering::Relaxed) {
            ALLOCATED.fetch_add(layout.size(), Ordering::Relaxed);
        }
        pointer
    }
    // Include zeroed allocation requests under the same independent sampling fence.
    unsafe fn alloc_zeroed(&self, layout: Layout) -> *mut u8 {
        let pointer = System.alloc_zeroed(layout);
        if !pointer.is_null() && COUNTING.load(Ordering::Relaxed) {
            ALLOCATED.fetch_add(layout.size(), Ordering::Relaxed);
        }
        pointer
    }
    // Preserve ordinary ownership cleanup without subtracting cumulative allocation requests.
    unsafe fn dealloc(&self, pointer: *mut u8, layout: Layout) {
        System.dealloc(pointer, layout);
    }
    // Count the full successful realloc request, rather than presenting it as net heap growth.
    unsafe fn realloc(&self, pointer: *mut u8, layout: Layout, size: usize) -> *mut u8 {
        let replacement = System.realloc(pointer, layout, size);
        if !replacement.is_null() && COUNTING.load(Ordering::Relaxed) {
            ALLOCATED.fetch_add(size, Ordering::Relaxed);
        }
        replacement
    }
}

enum Retained { Bytes(Vec<u8>), Model(XmlExampleOrder) }

// Keep complete public codec results observable while timing their owning replacement lifecycle.
#[inline(never)]
fn operation(mode: &str, value: &XmlExampleOrder, _input: &[u8], canonical: &[u8]) -> Retained {
    match mode {
        "native-encode" => Retained::Bytes(value.encode(Protocol::BinaryNone).expect("Native encode")),
        "native-decode" => Retained::Model(XmlExampleOrder::decode(canonical, Protocol::BinaryNone).expect("Native decode")),
        // BENCHMARK_XML_OPERATIONS
        _ => panic!("Unknown benchmark operation"),
    }
}

// Measure isolated owning operations; canonical checks and disk I/O remain outside wall samples.
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = env::args().collect();
    if args.len() != 5 { return Err("Usage: benchmark INPUT MODE OUTPUT REPETITIONS".into()); }
    let input = fs::read(&args[1])?;
    let mode = &args[2];
    let repetitions: usize = args[4].parse()?;
    assert!(repetitions > 0);
    // BENCHMARK_SETUP
    let canonical = value.encode(Protocol::BinaryNone)?;
    let mut retained = Retained::Bytes(Vec::new());
    for _ in 0..3 {
        for _ in 0..repetitions { retained = black_box(operation(mode, &value, &input, &canonical)); }
    }
    let mut samples = [0u128; 5];
    for sample in &mut samples {
        let start = Instant::now();
        for _ in 0..repetitions { retained = black_box(operation(mode, &value, &input, &canonical)); }
        *sample = start.elapsed().as_nanos();
    }
    // Keep the final timed result alive through the separate allocation sample.
    black_box(&retained);
    ALLOCATED.store(0, Ordering::Relaxed);
    COUNTING.store(true, Ordering::Relaxed);
    retained = black_box(operation(mode, &value, &input, &canonical));
    COUNTING.store(false, Ordering::Relaxed);
    let allocated = ALLOCATED.load(Ordering::Relaxed);
    let result = match retained {
        Retained::Model(model) => model,
        Retained::Bytes(encoded) => {
            // BENCHMARK_RESULT
        }
    };
    assert_eq!(result.encode(Protocol::BinaryNone)?, canonical, "Measured result changed native values");
    fs::write(&args[3], &canonical)?;
    // BENCHMARK_XML_OUTPUT
    println!("{{\"wall_ns\":{:?},\"repetitions\":{},\"allocation_bytes\":{},\"allocation_method\":\"Successful Rust global allocator allocation/reallocation requested bytes for one separate call; excludes allocator overhead\"}}", samples, repetitions, allocated);
    Ok(())
}
