// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import Foundation
import Dispatch
import SerializerBenchmarkSink

// Retain each owning result so optimized codec work remains observable across all measured batches.
enum BenchmarkResult {
    case bytes([UInt8])
    case model(XmlExampleOrder)
}

let arguments = CommandLine.arguments
guard arguments.count == 5, let repetitions = Int(arguments[4]), repetitions > 0 else {
    throw SerializerError.invalid("Usage: benchmark INPUT MODE OUTPUT REPETITIONS")
}
let input = Array(try Data(contentsOf: URL(fileURLWithPath: arguments[1])))
let mode = arguments[2]
// BENCHMARK_SETUP
let canonical = try value.encode(.BINARY_NONE)
let operation: () throws -> BenchmarkResult
switch mode {
case "native-encode": operation = { .bytes(try value.encode(.BINARY_NONE)) }
case "native-decode": operation = { .model(try XmlExampleOrder.decode(canonical, .BINARY_NONE)) }
// BENCHMARK_XML_OPERATIONS
default: throw SerializerError.invalid("Unknown benchmark operation")
}
var retained: BenchmarkResult = .bytes([])
for _ in 0..<3 {
    for _ in 0..<repetitions {
        retained = try operation()
        withUnsafePointer(to: &retained) { serializer_benchmark_observe(UnsafeRawPointer($0)) }
    }
}
var samples = [UInt64](repeating: 0, count: 5)
for index in samples.indices {
    let start = DispatchTime.now().uptimeNanoseconds
    for _ in 0..<repetitions {
        retained = try operation()
        withUnsafePointer(to: &retained) { serializer_benchmark_observe(UnsafeRawPointer($0)) }
    }
    samples[index] = DispatchTime.now().uptimeNanoseconds - start
}
let result: XmlExampleOrder
switch retained {
case .model(let model): result = model
case .bytes(let encoded):
    // BENCHMARK_RESULT
}
guard try result.encode(.BINARY_NONE) == canonical else {
    throw SerializerError.invalid("Measured result changed native values")
}
try Data(canonical).write(to: URL(fileURLWithPath: arguments[3]))
// BENCHMARK_XML_OUTPUT
let report: [String: Any] = ["wall_ns": samples, "repetitions": repetitions,
    "allocation_bytes": NSNull(),
    "allocation_method": "Not instrumented; independent process high-water includes Swift/Foundation/provider storage"]
let json = try JSONSerialization.data(withJSONObject: report, options: [.sortedKeys])
print(String(decoding: json, as: UTF8.self))
