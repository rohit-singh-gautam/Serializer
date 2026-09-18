using System;
using System.IO;
using System.Diagnostics;
using System.Collections.Generic;
using static SerializerGenerated.Schema;

internal static class Program {
  private static readonly string[] Languages = {"cpp", "java", "js", "go", "csharp"};

  // Construct the shared fixture independently using generated schema defaults.
  private static InteropMessage Fixture(int variant) {
    var value = new InteropMessage();
    value.Text = "Ada \"Lovelace\" 🚀\n" + new string('x', 64);
    value.Numbers = new List<int> {int.MinValue, -1, 0, int.MaxValue};
    value.Decimals = new List<float> {-0.0f, 1.5f, -2.25f};
    value.Flags = new List<bool> {false, true}; value.Labels = new List<string> {"", "é", "🚀"};
    var child = new InteropDetail {Code = -9, Note = "child"};
    value.Children = new List<InteropDetail> {new InteropDetail(), child};
    value.States = new List<InteropState> {InteropState.Paused, InteropState.Ready};
    value.Counts = new Dictionary<string, ulong> {{"🚀", ulong.MaxValue}, {"é", 7}, {"a", 0}};
    value.Indexed = new Dictionary<ulong, InteropDetail> {{ulong.MaxValue, child}, {0, new InteropDetail()}};
    value.Toggles = new Dictionary<bool, string> {{true, "yes"}, {false, "no"}};
    value.Enums = new Dictionary<InteropState, int> {{InteropState.Paused, -2}, {InteropState.Ready, 1}};
    value.PayloadIndex = variant; value.PayloadNumber = -1234567890123456789L;
    value.PayloadRatio = -3.5f; value.PayloadState = InteropState.Paused;
    return value;
  }

  // Check every producer's values and canonical binary representations.
  private static void Exchange(string directory, string mode) {
    Directory.CreateDirectory(directory);
    for (int variant = 0; variant < 3; ++variant) {
      var expected = Fixture(variant); var canonical = expected.Encode(Protocol.BINARY_NONE);
      foreach (Protocol protocol in Enum.GetValues<Protocol>()) {
        var encoded = expected.Encode(protocol);
        if (mode == "emit") File.WriteAllBytes(Path.Combine(directory, $"csharp_{protocol}_{variant}.bin"), encoded);
        else foreach (var language in Languages) {
          var input = File.ReadAllBytes(Path.Combine(directory, $"{language}_{protocol}_{variant}.bin"));
          var actual = InteropMessage.Decode(input, protocol);
          if (!actual.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(canonical)) throw new Exception($"{language} -> csharp {protocol}/{variant} value mismatch");
          if (protocol != Protocol.JSON && !input.AsSpan().SequenceEqual(encoded)) throw new Exception("Canonical binary mismatch");
        }
      }
    }
  }

  // Report local throughput after warmup, with no cross-runtime performance claim.
  private static void Benchmark() {
    var value = Fixture(0); const int iterations = 10000;
    foreach (Protocol protocol in Enum.GetValues<Protocol>()) {
      var bytes = value.Encode(protocol);
      for (int i = 0; i < 1000; ++i) InteropMessage.Decode(value.Encode(protocol), protocol);
      var timer = Stopwatch.StartNew();
      for (int i = 0; i < iterations; ++i) value.Encode(protocol);
      double encodeSeconds = timer.Elapsed.TotalSeconds; timer.Restart();
      for (int i = 0; i < iterations; ++i) InteropMessage.Decode(bytes, protocol);
      Console.WriteLine($"csharp {protocol}: {bytes.Length} bytes; encode {iterations / encodeSeconds:F0}/s; decode {iterations / timer.Elapsed.TotalSeconds:F0}/s");
    }
  }

  // Run one consumer or producer in the shared interoperability matrix.
  private static int Main(string[] args) {
    try {
      if (args.Length != 2) throw new ArgumentException("Usage: Interop <directory> emit|verify|benchmark");
      if (args[1] == "benchmark") Benchmark();
      else if (args[1] == "emit" || args[1] == "verify") Exchange(args[0], args[1]);
      else throw new ArgumentException("Unknown mode");
      Console.WriteLine($"csharp {args[1]} passed"); return 0;
    } catch (Exception error) { Console.Error.WriteLine(error); return 1; }
  }
}
