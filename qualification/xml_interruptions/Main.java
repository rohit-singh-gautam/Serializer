// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;

/** Qualify public owning XML callbacks on the maintained large benchmark inputs. */
public final class Main {
  /** Preserve exact caller exceptions and recover with fresh complete operations after each failure. */
  public static void main(String[] args) throws Exception {
    for (int index = 1; index < args.length; ++index) {
      byte[] input = Files.readAllBytes(Path.of(args[index]));
      var value = Schema.XmlExample.Order.decodeXml(input);
      byte[] canonical = value.encode(Schema.Protocol.BINARY_NONE);
      int verified = 0;
      for (boolean writing : new boolean[]{false, true}) {
        for (int policy : new int[]{1, 1000, 0}) {
          int[] polls = {0};
          long deadline = System.nanoTime() - 1;
          var marker = new IllegalStateException("Caller XML interruption");
          var defaults = Schema.Limits.defaults();
          var limits = new Schema.Limits(defaults.maxBytes, defaults.maxStringBytes,
              defaults.maxElements, defaults.maxDepth, defaults.readPolicy, (resource, amount, field) -> {
                if (++polls[0] >= policy && policy != 0 || policy == 0 && System.nanoTime() >= deadline) throw marker;
              });
          boolean failed = false;
          try {
            if (writing) value.encodeXml(limits, null);
            else Schema.XmlExample.Order.decodeXml(input, limits, null);
          } catch (IllegalStateException failure) {
            if (failure != marker) throw failure;
            failed = true;
          }
          if (!failed || policy != 0 && polls[0] != policy) throw new AssertionError("Interruption returned a candidate");
          if (!Arrays.equals(canonical, value.encode(Schema.Protocol.BINARY_NONE)) ||
              !Arrays.equals(canonical, Schema.XmlExample.Order.decodeXml(value.encodeXml()).encode(Schema.Protocol.BINARY_NONE))) {
            throw new AssertionError("Interrupted operation changed values or recovery failed");
          }
          ++verified;
        }
      }
      Files.write(Path.of(args[0], index + ".wire"), canonical);
      System.out.println("{\"fixture\":" + index + ",\"verified_interruptions\":" + verified + ",\"recovered\":true}");
    }
  }
}
