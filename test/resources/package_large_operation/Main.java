// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import java.io.ByteArrayOutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.Map;

/** Execute large public OPC/XML operations with one observable bounded ledger. */
public final class Main {
  private static final long GIB = 1L << 30;
  private static final Schema.SerializerPackage.ExtensionPolicy PRESERVE = new Schema.SerializerPackage.ExtensionPolicy(
      Schema.SerializerPackage.Unknown.PRESERVE, Schema.SerializerPackage.Unknown.PRESERVE, null, null);
  private static final Schema.SerializerPackage.Resource[] RESOURCES = {
      Schema.SerializerPackage.Resource.REFERENCES, Schema.SerializerPackage.Resource.WORK_UNITS,
      Schema.SerializerPackage.Resource.DECODER_ALLOCATION_BYTES, Schema.SerializerPackage.Resource.PRESERVED_BYTES};
  private static int checks;

  /** Reject a false decision independently of the generated exception paths. */
  private static void require(boolean value, String message) { if (!value) throw new AssertionError(message); }
  /** Grant finite logical capacity for the complete repeated operation, independently of process memory. */
  private static Schema.SerializerPackage.Limits limits() {
    var value = new Schema.SerializerPackage.Limits();
    value.maxReferences = 4_000_000; value.maxDecoderAllocationBytes = 2 * GIB; value.maxWorkUnits = 4 * GIB;
    return value;
  }
  /** Read a public ceiling without inspecting private counters. */
  private static long maximum(Schema.SerializerPackage.Limits value, Schema.SerializerPackage.Resource resource) {
    return switch (resource) {
      case REFERENCES -> value.maxReferences; case WORK_UNITS -> value.maxWorkUnits;
      case DECODER_ALLOCATION_BYTES -> value.maxDecoderAllocationBytes; case PRESERVED_BYTES -> value.maxPreservedBytes;
      default -> throw new AssertionError("Unexpected resource");
    };
  }
  /** Change only the selected exact/one-tight policy. */
  private static void ceiling(Schema.SerializerPackage.Limits value, Schema.SerializerPackage.Resource resource, long count) {
    switch (resource) {
      case REFERENCES -> value.maxReferences = count; case WORK_UNITS -> value.maxWorkUnits = count;
      case DECODER_ALLOCATION_BYTES -> value.maxDecoderAllocationBytes = count; case PRESERVED_BYTES -> value.maxPreservedBytes = count;
      default -> throw new AssertionError("Unexpected resource");
    }
  }
  /** Derive consumed capacity through the public immutable policy snapshot and remaining allowance. */
  private static long used(Schema.SerializerPackage.Budget budget, Schema.SerializerPackage.Resource resource) {
    return maximum(budget.configured(), resource) - budget.remaining(resource);
  }
  /** Keep expected failure capture outside the operation under test. */
  private static Throwable failure(Action action) throws Exception {
    try { action.run(); } catch (Exception | AssertionError error) { return error; }
    throw new AssertionError("Unexpected operation success");
  }
  @FunctionalInterface private interface Action { void run() throws Exception; }
  /** Record only fully completed decisions. */
  private static void pass(String name) { ++checks; System.out.println("PASS " + name); }
  /** Load all owners and extensions without substituting any typed codec or validator. */
  private static Schema.SerializerPackage.Result<Schema.IdentityPackage.Archive.Value> load(byte[] bytes, Schema.SerializerPackage.Budget budget) throws Exception {
    return Schema.IdentityPackage.Archive.load(bytes, budget, PRESERVE, null);
  }
  /** Check large native aggregate shape before any output is accepted. */
  private static void shape(Schema.IdentityPackage.Archive.Value value, int count) {
    require(value.registries.size() == 3 && value.extensions.parts.size() == 1 && value.extensions.relationships.size() == 3, "Aggregate/preservation shape");
    for (var item : value.registries) require(item.value.requests.length == count && item.value.entries.length == count, "Incomplete large model");
  }
  /** Exercise load, explicit DEFLATE encode, reload and Save with the identical caller-owned ledger. */
  private static void pipeline(byte[] bytes, Path path, Schema.SerializerPackage.Budget budget, int count) throws Exception {
    long previous = used(budget, RESOURCES[0]); var value = load(bytes, budget).value; shape(value, count);
    require(used(budget, RESOURCES[0]) > previous, "Load reset references"); previous = used(budget, RESOURCES[0]);
    byte[] encoded = Schema.IdentityPackage.Archive.encode(value, budget, Map.of("registries", "deflate"));
    require(used(budget, RESOURCES[0]) > previous, "Encode bypassed references"); previous = used(budget, RESOURCES[0]);
    shape(load(encoded, budget).value, count); require(used(budget, RESOURCES[0]) > previous, "Reload reset references");
    Schema.IdentityPackage.Archive.save(path, value, budget, null, null);
  }
  /** Serialize measured logical capacities for the independent report. */
  private static String metrics(Schema.SerializerPackage.Budget budget) {
    return "{\"references\":" + used(budget, RESOURCES[0]) + ",\"work\":" + used(budget, RESOURCES[1]) +
        ",\"allocation\":" + used(budget, RESOURCES[2]) + ",\"preserved\":" + used(budget, RESOURCES[3]) + "}";
  }
  /** Run independent invalid, cumulative admission, interruption and publication decisions. */
  public static void main(String[] arguments) throws Exception {
    Path directory = Path.of(arguments[0]); int count = Integer.parseInt(arguments[1]);
    byte[] valid = Files.readAllBytes(directory.resolve("valid.opc")), invalid = Files.readAllBytes(directory.resolve("invalid.opc"));
    var baseline = new Schema.SerializerPackage.Budget(limits()); var value = load(valid, baseline).value; shape(value, count);
    require(used(baseline, RESOURCES[0]) >= 9L * count, "Identity tuples bypassed ledger"); pass("large generated identity load and opaque/link preservation");
    boolean[] validated = {false}; Object sentinel = new Object(); Object[] published = {sentinel};
    Throwable bad = failure(() -> published[0] = Schema.IdentityPackage.Archive.load(invalid, new Schema.SerializerPackage.Budget(limits()), PRESERVE, (candidate, budget) -> validated[0] = true));
    require(bad instanceof Schema.SerializerPackage.PackageError && !validated[0] && published[0] == sentinel, "Invalid final-owner keyref published"); pass("invalid final owner has no aggregate publication");
    for (var resource : RESOURCES) for (boolean tight : new boolean[]{false, true}) {
      var policy = limits(); ceiling(policy, resource, used(baseline, resource) - (tight ? 1 : 0)); var budget = new Schema.SerializerPackage.Budget(policy);
      if (tight) { Throwable error = failure(() -> load(valid, budget)); require(error instanceof Schema.SerializerPackage.PackageError p && p.code == Schema.SerializerPackage.Code.RESOURCE_LIMIT, "Wrong tight quota failure"); }
      else shape(load(valid, budget).value, count);
      pass(resource + (tight ? " one tight" : " exact"));
    }
    for (String mode : new String[]{"middle", "late", "expiry", "original"}) {
      var marker = new IllegalStateException("original " + mode); var cancellation = new IllegalStateException("secondary cancellation");
      boolean[] armed = {false}; long[] expired = {Long.MAX_VALUE}; Schema.SerializerPackage.Budget[] selected = {null}; var policy = limits();
      policy.checkCancelled = () -> {
        if (selected[0] == null) return;
        if ((mode.equals("middle") || mode.equals("expiry")) && used(selected[0], RESOURCES[0]) >= 9L * count / 2) {
          if (mode.equals("middle")) throw marker;
          if (expired[0] == Long.MAX_VALUE) expired[0] = System.nanoTime();
          if (System.nanoTime() - expired[0] >= 0) throw marker;
        }
        if (armed[0]) throw mode.equals("original") ? cancellation : marker;
      };
      selected[0] = new Schema.SerializerPackage.Budget(policy); published[0] = sentinel;
      Throwable error = failure(() -> published[0] = Schema.IdentityPackage.Archive.load(valid, selected[0], PRESERVE, (candidate, budget) -> {
        require(budget == selected[0], "Aggregate validator received a fresh ledger"); shape(candidate, count); armed[0] = true;
        if (mode.equals("original")) throw marker;
      }));
      require(error == marker && published[0] == sentinel, "Cancellation/error identity or publication changed: " + mode);
      if (mode.equals("expiry")) require(expired[0] != Long.MAX_VALUE, "Host monotonic expiry not reached");
      pass("large load " + mode);
    }
    var marker = new IllegalStateException("original encode cancellation"); Schema.SerializerPackage.Budget[] selected = {null}; var policy = limits();
    policy.checkCancelled = () -> { if (selected[0] != null && used(selected[0], RESOURCES[0]) >= 9L * count / 2) throw marker; };
    selected[0] = new Schema.SerializerPackage.Budget(policy); var sink = new ByteArrayOutputStream();
    require(failure(() -> Schema.IdentityPackage.Archive.write(sink, value, selected[0])) == marker && sink.size() == 0, "Incomplete staged XML reached borrowed sink"); pass("middle encode cancellation publishes no sink bytes");
    byte[] old = "existing complete sentinel".getBytes(java.nio.charset.StandardCharsets.UTF_8); Path guarded = directory.resolve("guarded.opc"); Files.write(guarded, old);
    boolean[] armed = {false}; policy = limits(); policy.checkCancelled = () -> { if (armed[0]) throw marker; };
    var saveBudget = new Schema.SerializerPackage.Budget(policy);
    require(failure(() -> Schema.IdentityPackage.Archive.save(guarded, value, saveBudget, null, point -> { if (point.equals("before_replace")) armed[0] = true; })) == marker, "Late Save error changed");
    require(armed[0] && Arrays.equals(Files.readAllBytes(guarded), old), "Late Save replaced sentinel"); pass("late atomic Save preserves previous file");
    var combined = new Schema.SerializerPackage.Budget(limits()); pipeline(valid, directory.resolve("output.opc"), combined, count); pass("shared load DEFLATE reload Save pipeline");
    for (boolean tight : new boolean[]{false, true}) {
      policy = limits(); policy.maxReferences = used(combined, RESOURCES[0]) - (tight ? 1 : 0); var budget = new Schema.SerializerPackage.Budget(policy); Path path = directory.resolve(tight ? "tight.opc" : "exact.opc"); Files.write(path, old);
      if (tight) { Throwable error = failure(() -> pipeline(valid, path, budget, count)); require(error instanceof Schema.SerializerPackage.PackageError p && p.code == Schema.SerializerPackage.Code.RESOURCE_LIMIT && Arrays.equals(Files.readAllBytes(path), old), "Combined quota published Save"); }
      else pipeline(valid, path, budget, count);
      pass(tight ? "combined reference one tight" : "combined reference exact");
    }
    shape(load(Files.readAllBytes(directory.resolve("output.opc")), new Schema.SerializerPackage.Budget(limits())).value, count); pass("fresh load recovery");
    System.out.println("LEDGER\t{\"load\":" + metrics(baseline) + ",\"combined\":" + metrics(combined) + ",\"checks\":" + checks + "}");
  }
}
