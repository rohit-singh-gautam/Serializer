// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
using System;
using System.IO;
using System.Linq;
using System.Diagnostics;
using System.Collections.Generic;
using P = SerializerGenerated.Schema.SerializerPackage;
using A = SerializerGenerated.Schema.IdentityPackageArchive;

// Qualify large generated owning operations without replacing codecs or validators.
internal static class Program {
  private static readonly P.ExtensionPolicy Preserve = new(P.Unknown.Preserve, P.Unknown.Preserve);
  private static readonly P.Resource[] Resources = { P.Resource.References, P.Resource.WorkUnits, P.Resource.DecoderAllocationBytes, P.Resource.PreservedBytes };
  private static int checks;
  // Fail independently of generated error handling.
  private static void Require(bool value, string message) { if (!value) throw new Exception(message); }
  // Keep all policies finite while admitting the complete repeated logical pipeline.
  private static P.PackageLimits Limits(P.Resource? resource = null, long ceiling = 0, Func<bool>? poll = null) => new() {
    MaxReferences = resource == P.Resource.References ? ceiling : 4_000_000,
    MaxWorkUnits = resource == P.Resource.WorkUnits ? ceiling : 4L << 30,
    MaxDecoderAllocationBytes = resource == P.Resource.DecoderAllocationBytes ? ceiling : 2L << 30,
    MaxPreservedBytes = resource == P.Resource.PreservedBytes ? ceiling : 64L << 20,
    IsCancelled = poll
  };
  // Capture failures outside the operation to prevent false success.
  private static Exception Failure(Action action) { try { action(); } catch (Exception error) { return error; } throw new Exception("Unexpected operation success"); }
  // Record completed decisions only.
  private static void Pass(string name) { ++checks; Console.WriteLine("PASS " + name); }
  // Invoke genuine compiler-emitted native XML bindings through the owning facade.
  private static P.Result<A.Value> Load(byte[] bytes, P.Budget budget) => A.Load(bytes, budget, Preserve);
  // Require complete native values and preserved whole-part/source-scoped metadata.
  private static void Shape(A.Value value, int count) {
    Require(value.Registries.Count == 3 && value.Extensions.Parts.Count == 1 && value.Extensions.Relationships.Count == 3, "Aggregate/preservation shape");
    foreach (var item in value.Registries) Require(item.Value.Requests.Count == count && item.Value.Entries.Count == count, "Incomplete model");
  }
  // Compose load, explicit DEFLATE output, reload and atomic Save on one public budget.
  private static void Pipeline(byte[] bytes, string path, P.Budget budget, int count) {
    long previous = budget.Used(Resources[0]); var value = Load(bytes, budget).Value; Shape(value, count);
    Require(budget.Used(Resources[0]) > previous, "Load bypassed references"); previous = budget.Used(Resources[0]);
    value.ProfileOverrides = new Dictionary<string, string> { ["registries"] = "deflate" };
    byte[] encoded = A.Encode(value, budget); Require(budget.Used(Resources[0]) > previous, "Encode reset references"); previous = budget.Used(Resources[0]);
    Shape(Load(encoded, budget).Value, count); Require(budget.Used(Resources[0]) > previous, "Reload reset references");
    A.Save(path, value, budget);
  }
  // Serialize public ledger counters for independent reporting.
  private static string Metrics(P.Budget budget) => "{\"references\":" + budget.Used(Resources[0]) + ",\"work\":" + budget.Used(Resources[1]) + ",\"allocation\":" + budget.Used(Resources[2]) + ",\"preserved\":" + budget.Used(Resources[3]) + "}";
  // Execute large quota, interruption, error precedence and publication controls.
  private static void Main(string[] arguments) {
    string directory = arguments[0]; int count = int.Parse(arguments[1], System.Globalization.CultureInfo.InvariantCulture);
    byte[] valid = File.ReadAllBytes(Path.Combine(directory, "valid.opc")), invalid = File.ReadAllBytes(Path.Combine(directory, "invalid.opc"));
    var baseline = new P.Budget(Limits()); var value = Load(valid, baseline).Value; Shape(value, count);
    Require(baseline.Used(Resources[0]) >= 9L * count, "Identity tuples bypassed ledger"); Pass("large generated identity load and preserved opaque/link metadata");
    bool validated = false; object sentinel = new(); object published = sentinel;
    var bad = Failure(() => published = A.Load(invalid, new P.Budget(Limits()), Preserve, (candidate, budget) => validated = true));
    Require(bad is P.PackageError && !validated && ReferenceEquals(published, sentinel), "Invalid final-owner keyref published"); Pass("invalid final owner has no aggregate publication");
    foreach (var resource in Resources) foreach (bool tight in new[] { false, true }) {
      var budget = new P.Budget(Limits(resource, baseline.Used(resource) - (tight ? 1 : 0)));
      if (tight) Require(Failure(() => Load(valid, budget)) is P.PackageError error && error.Code == P.Code.ResourceLimit, "Wrong tight quota error");
      else Shape(Load(valid, budget).Value, count);
      Pass(resource + (tight ? " one tight" : " exact"));
    }
    foreach (string mode in new[] { "middle", "late", "expiry", "original" }) {
      var marker = new InvalidOperationException("original " + mode); var cancellation = new InvalidOperationException("secondary cancellation");
      bool armed = false; long expiry = long.MaxValue; P.Budget? selected = null;
      selected = new P.Budget(Limits(poll: () => {
        if (selected == null) return false;
        if ((mode == "middle" || mode == "expiry") && selected.Used(Resources[0]) >= 9L * count / 2) {
          if (mode == "middle") throw marker;
          if (expiry == long.MaxValue) expiry = Stopwatch.GetTimestamp();
          if (Stopwatch.GetTimestamp() >= expiry) throw marker;
        }
        if (armed) throw mode == "original" ? cancellation : marker;
        return false;
      }));
      published = sentinel;
      var error = Failure(() => published = A.Load(valid, selected, Preserve, (candidate, budget) => {
        Require(ReferenceEquals(budget, selected), "Aggregate validator received a fresh budget"); Shape(candidate, count); armed = true;
        if (mode == "original") throw marker;
      }));
      Require(ReferenceEquals(error, marker) && ReferenceEquals(published, sentinel), "Cancellation identity/publication changed: " + mode);
      if (mode == "expiry") Require(expiry != long.MaxValue, "Host monotonic deadline not reached");
      Pass("large load " + mode);
    }
    var encodeMarker = new InvalidOperationException("original encode cancellation"); P.Budget? encodeBudget = null;
    encodeBudget = new P.Budget(Limits(poll: () => { if (encodeBudget != null && encodeBudget.Used(Resources[0]) >= 9L * count / 2) throw encodeMarker; return false; }));
    using var sink = new MemoryStream(); Require(ReferenceEquals(Failure(() => A.Write(sink, value, encodeBudget)), encodeMarker) && sink.Length == 0, "Incomplete XML reached sink"); Pass("middle encode cancellation publishes no sink bytes");
    byte[] old = System.Text.Encoding.UTF8.GetBytes("existing complete sentinel"); string guarded = Path.Combine(directory, "guarded.opc"); File.WriteAllBytes(guarded, old);
    bool saveArmed = false; var saveBudget = new P.Budget(Limits(poll: () => { if (saveArmed) throw encodeMarker; return false; }));
    Require(ReferenceEquals(Failure(() => A.Save(guarded, value, saveBudget, fault: stage => { if (stage == "before_publish") saveArmed = true; })), encodeMarker), "Save changed original error");
    Require(saveArmed && File.ReadAllBytes(guarded).SequenceEqual(old), "Late Save replaced sentinel"); Pass("late atomic Save preserves previous file");
    var combined = new P.Budget(Limits()); Pipeline(valid, Path.Combine(directory, "output.opc"), combined, count); Pass("shared load DEFLATE reload Save pipeline");
    foreach (bool tight in new[] { false, true }) {
      var budget = new P.Budget(Limits(Resources[0], combined.Used(Resources[0]) - (tight ? 1 : 0))); string path = Path.Combine(directory, tight ? "tight.opc" : "exact.opc"); File.WriteAllBytes(path, old);
      if (tight) Require(Failure(() => Pipeline(valid, path, budget, count)) is P.PackageError error && error.Code == P.Code.ResourceLimit && File.ReadAllBytes(path).SequenceEqual(old), "Combined quota published Save");
      else Pipeline(valid, path, budget, count);
      Pass(tight ? "combined reference one tight" : "combined reference exact");
    }
    Shape(Load(File.ReadAllBytes(Path.Combine(directory, "output.opc")), new P.Budget(Limits())).Value, count); Pass("fresh load recovery");
    Console.WriteLine("LEDGER\t{\"load\":" + Metrics(baseline) + ",\"combined\":" + Metrics(combined) + ",\"checks\":" + checks + "}");
  }
}
