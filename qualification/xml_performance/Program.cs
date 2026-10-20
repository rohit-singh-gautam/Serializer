// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
using System;
using System.Diagnostics;
using System.IO;
using System.Text.Json;
using static SerializerGenerated.Schema;

// Measure actual owning codecs with setup and result validation outside timings.
internal static class Program {
  private static object? retained;

  // Retain five warmed batches and one independent managed-allocation sample.
  private static void Main(string[] args) {
    byte[] input = File.ReadAllBytes(args[0]);
    string mode = args[1];
    int repetitions = int.Parse(args[3], System.Globalization.CultureInfo.InvariantCulture);
    // BENCHMARK_SETUP
    byte[] canonical = value.Encode(Protocol.BINARY_NONE);
    Func<object> operation = mode switch {
      // BENCHMARK_XML_OPERATIONS
      "native-encode" => () => value.Encode(Protocol.BINARY_NONE),
      "native-decode" => () => XmlExampleOrder.Decode(canonical, Protocol.BINARY_NONE),
      _ => throw new ArgumentException("Unknown benchmark operation")
    };
    for (int warmup = 0; warmup < 3; warmup++) {
      for (int iteration = 0; iteration < repetitions; iteration++) retained = operation();
    }
    long[] samples = new long[5];
    for (int sample = 0; sample < samples.Length; sample++) {
      long start = Stopwatch.GetTimestamp();
      for (int iteration = 0; iteration < repetitions; iteration++) retained = operation();
      samples[sample] = checked((long)((Stopwatch.GetTimestamp() - start) * (1e9 / Stopwatch.Frequency)));
    }
    long before = GC.GetAllocatedBytesForCurrentThread();
    retained = operation();
    long allocated = GC.GetAllocatedBytesForCurrentThread() - before;
    XmlExampleOrder result;
    if (retained is XmlExampleOrder owning) result = owning;
    else {
      byte[] encoded = (byte[])retained;
      // BENCHMARK_RESULT
    }
    if (!result.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(canonical)) throw new Exception("Measured result changed values");
    File.WriteAllBytes(args[2], canonical);
    // BENCHMARK_XML_OUTPUT
    Console.WriteLine(JsonSerializer.Serialize(new {
      wall_ns = samples, repetitions, allocation_bytes = allocated,
      allocation_method = "GC.GetAllocatedBytesForCurrentThread delta for one separate call",
      sdk = System.Runtime.InteropServices.RuntimeInformation.FrameworkDescription
    }));
  }
}
