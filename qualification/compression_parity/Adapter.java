// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package compression.parity;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.Locale;
import static compression.parity.Schema.SerializerCompression.*;

/** Replay the same frames and policies through actual Java owning model codecs. */
public final class Adapter {
  /** Keep qualification failures independent of JVM assertion switches. */
  private static void require(boolean value, String message) { if (!value) throw new AssertionError(message); }
  /** Require all declared decisions; unexpected application/provider errors remain failures. */
  public static void main(String[] args) throws Exception {
    Path root = Path.of(args[1]);
    for (Format format : Format.values()) require(available(format), "Missing Java provider " + format);
    if (args[0].equals("produce")) {
      Schema.AccountRecord value = new Schema.AccountRecord(); value.accountId = 42; value.label = "shared \u03bb \ud83d\ude42";
      for (Schema.Protocol protocol : Schema.Protocol.values()) for (Format format : Format.values()) {
        String stem = "java-" + protocol + "-" + format;
        int level = format == Format.NONE || format == Format.LZ4 ? 0 : 3;
        byte[] plain = value.encode(protocol), frame = encode(() -> value.encode(protocol), new EncodeOptions(format, level, 8388608, 8388608, null));
        require(Arrays.equals(frame, encode(() -> value.encode(protocol), new EncodeOptions(format, level, plain.length, frame.length, null))), "Exact encode admission");
        for (int[] bounds : new int[][]{{plain.length-1,frame.length},{plain.length,frame.length-1}}) {
          try { encode(() -> value.encode(protocol), new EncodeOptions(format, level, bounds[0], bounds[1], null)); throw new AssertionError("One-tight encode admission"); }
          catch (Failure failure) { require(failure.code == Code.RESOURCE_LIMIT, "Wrong encode admission category"); }
        }
        Files.write(root.resolve(stem + ".frame"), frame);
        Files.write(root.resolve(stem + ".plain"), plain);
      }
      System.out.println("{\"produced\":24,\"encode_bounds\":72}"); return;
    }
    int decisions = 0, exchanges = 0;
    for (String line : Files.readAllLines(root.resolve("cases.tsv"))) {
      String[] row = line.split("\t"); String observed = "ok";
      try {
        byte[] plain = decompress(Files.readAllBytes(root.resolve(row[2])), new DecodeOptions(Format.valueOf(row[1]),
            Integer.parseInt(row[5]), Integer.parseInt(row[6]), Integer.parseInt(row[7]), null));
        if (row[4].equals("ok")) require(Arrays.equals(plain, Files.readAllBytes(root.resolve(row[3]))), row[0]);
      } catch (Failure failure) { observed = failure.code.name().toLowerCase(Locale.ROOT); }
      require(observed.equals(row[4]), row[0] + ": " + observed + " != " + row[4]); ++decisions;
    }
    for (String line : Files.readAllLines(root.resolve("exchange.tsv"))) {
      String[] row = line.split("\t"); Schema.Protocol protocol = Schema.Protocol.valueOf(row[1]);
      byte[] frame = Files.readAllBytes(root.resolve(row[3])); DecodeOptions options = new DecodeOptions(Format.valueOf(row[2]));
      Schema.AccountRecord value = decode(frame, options, bytes -> Schema.AccountRecord.decode(bytes, protocol));
      require(value.accountId == 42 && value.label.equals("shared \u03bb \ud83d\ude42"), row[0]);
      require(Arrays.equals(decompress(frame, options), Files.readAllBytes(root.resolve(row[4]))), row[0]); ++exchanges;
    }
    System.out.println("{\"decisions\":" + decisions + ",\"exchanges\":" + exchanges + "}");
  }
}
