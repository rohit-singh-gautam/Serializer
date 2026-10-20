// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import java.lang.management.ManagementFactory;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.function.Supplier;

/** Measure actual owning codecs with setup and result validation outside timings. */
public final class Main {
  private static volatile Object retained;

  /** Retain five warmed batches and one independent thread-allocation sample. */
  public static void main(String[] args) throws Exception {
    byte[] input = Files.readAllBytes(Path.of(args[0]));
    String mode = args[1];
    int repetitions = Integer.parseInt(args[3]);
    // BENCHMARK_SETUP
    byte[] canonical = value.encode(Schema.Protocol.BINARY_NONE);
    Supplier<Object> operation = switch (mode) {
      // BENCHMARK_XML_OPERATIONS
      case "native-encode" -> () -> value.encode(Schema.Protocol.BINARY_NONE);
      case "native-decode" -> () -> Schema.XmlExample.Order.decode(canonical, Schema.Protocol.BINARY_NONE);
      default -> throw new IllegalArgumentException("Unknown benchmark operation");
    };
    for (int warmup = 0; warmup < 3; warmup++) {
      for (int iteration = 0; iteration < repetitions; iteration++) retained = operation.get();
    }
    long[] samples = new long[5];
    for (int sample = 0; sample < samples.length; sample++) {
      long start = System.nanoTime();
      for (int iteration = 0; iteration < repetitions; iteration++) retained = operation.get();
      samples[sample] = System.nanoTime() - start;
    }
    var allocations = (com.sun.management.ThreadMXBean) ManagementFactory.getThreadMXBean();
    if (!allocations.isThreadAllocatedMemorySupported()) throw new IllegalStateException("Missing allocation counter");
    allocations.setThreadAllocatedMemoryEnabled(true);
    long thread = Thread.currentThread().getId();
    long before = allocations.getThreadAllocatedBytes(thread);
    retained = operation.get();
    long allocated = allocations.getThreadAllocatedBytes(thread) - before;
    Schema.XmlExample.Order result;
    if (retained instanceof Schema.XmlExample.Order owning) result = owning;
    else {
      byte[] encoded = (byte[]) retained;
      // BENCHMARK_RESULT
    }
    if (!Arrays.equals(result.encode(Schema.Protocol.BINARY_NONE), canonical)) throw new AssertionError("Measured result changed values");
    Files.write(Path.of(args[2]), canonical);
    // BENCHMARK_XML_OUTPUT
    System.out.println("{\"wall_ns\":" + Arrays.toString(samples) + ",\"repetitions\":" + repetitions +
      ",\"allocation_bytes\":" + allocated + ",\"allocation_method\":\"ThreadMXBean current-thread allocation delta for one separate call\"," +
      "\"sdk\":\"" + System.getProperty("java.runtime.version") + "\"}");
  }
}
