// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

/** Replay mandatory shared fixtures through actual generated Java models and helpers. */
public final class CompressionExchange {
  private static final List<String> PROTOCOLS = List.of("JSON", "BINARY_NONE", "BINARY_INTEGER", "BINARY_STRING");
  private static final List<String> FORMATS = List.of("NONE", "GZIP", "ZLIB", "DEFLATE");

  /** Prevent construction of the command-line qualification adapter. */
  private CompressionExchange() { }

  /** Compare every owning field after complete native decoding. */
  private static void sameModel(Schema.ExchangeRecord actual, Schema.ExchangeRecord expected) {
    if (actual.code != expected.code || !actual.text.equals(expected.text)) {
      throw new AssertionError("Shared generated model differs");
    }
  }

  /** Normalize one common framing/limit decision without masking unrelated failures. */
  private static String replay(String[] row, Path output) throws Exception {
    if (row.length != 9) throw new IllegalArgumentException("Incomplete common corpus row");
    Schema.SerializerCompression.Format format = Schema.SerializerCompression.Format.valueOf(row[2]);
    byte[] data = Files.readAllBytes(Path.of(row[3]));
    try {
      if (row[1].equals("encode_bounds")) {
        byte[] packed = Schema.SerializerCompression.compress(data, new Schema.SerializerCompression.EncodeOptions(format));
        byte[] exact = Schema.SerializerCompression.compress(data, new Schema.SerializerCompression.EncodeOptions(
            format, 6, data.length, packed.length, null));
        if (!Arrays.equals(exact, packed)) throw new AssertionError("Exact output admission changed the frame");
        try {
          Schema.SerializerCompression.compress(data, new Schema.SerializerCompression.EncodeOptions(
              format, 6, data.length, packed.length - 1, null));
          throw new AssertionError("One-tight encoding output bound was accepted");
        } catch (Schema.SerializerCompression.Failure failure) {
          if (failure.code != Schema.SerializerCompression.Code.RESOURCE_LIMIT) throw failure;
        }
        Files.write(output.resolve(row[0] + ".bin"), packed);
      } else if (row[1].equals("encode")) {
        Schema.SerializerCompression.compress(data, new Schema.SerializerCompression.EncodeOptions(
            format, 6, Integer.parseInt(row[6]), Integer.parseInt(row[7]), null));
      } else {
        Schema.SerializerCompression.DecodeOptions options = new Schema.SerializerCompression.DecodeOptions(
            format, Integer.parseInt(row[6]), Integer.parseInt(row[7]), Integer.parseInt(row[8]), null);
        if (row[1].equals("inner")) {
          Schema.Protocol protocol = Schema.Protocol.valueOf(row[4]);
          Schema.SerializerCompression.decode(data, options, bytes -> Schema.ExchangeRecord.decode(bytes, protocol));
        } else if (!Arrays.equals(Schema.SerializerCompression.decompress(data, options), Files.readAllBytes(Path.of(row[4])))) {
          throw new AssertionError("Independent compression fixture differs");
        }
      }
    } catch (Schema.SerializerCompression.Failure failure) {
      return failure.code.name().toLowerCase(Locale.ROOT);
    } catch (IllegalArgumentException failure) {
      if (row[1].equals("inner")) return "native_error";
      throw failure;
    }
    return "ok";
  }

  /** Produce or consume every mandatory case; missing SDKs/providers never become skips. */
  public static void main(String[] args) throws Exception {
    if (args.length != 3 && args.length != 5) throw new IllegalArgumentException("Expected qualification mode and paths");
    Schema.ExchangeRecord expected = Schema.ExchangeRecord.decode(Files.readAllBytes(Path.of(args[1])), Schema.Protocol.JSON);
    for (String name : FORMATS) {
      if (!Schema.SerializerCompression.available(Schema.SerializerCompression.Format.valueOf(name))) {
        throw new IllegalStateException("A required Java compression provider is unavailable: " + name);
      }
    }
    if (args[0].equals("produce") && args.length == 3) {
      Path directory = Path.of(args[2]);
      for (String protocolName : PROTOCOLS) {
        Schema.Protocol protocol = Schema.Protocol.valueOf(protocolName);
        for (String format : FORMATS) {
          byte[] packed = Schema.SerializerCompression.encode(() -> expected.encode(protocol),
              new Schema.SerializerCompression.EncodeOptions(Schema.SerializerCompression.Format.valueOf(format)));
          Files.write(directory.resolve("java-" + protocolName + "-" + format + ".bin"), packed);
        }
      }
      System.out.println("{\"produced\":" + PROTOCOLS.size() * FORMATS.size() + "}");
      return;
    }
    if (!args[0].equals("check") || args.length != 5) throw new IllegalArgumentException("Unknown qualification mode");
    Path output = Path.of(args[4]);
    Files.createDirectories(output);
    int exchanges = 0;
    for (String line : Files.readAllLines(Path.of(args[2]))) {
      String[] row = line.split("\t", -1);
      if (row.length != 3) throw new IllegalArgumentException("Incomplete model exchange row");
      Schema.Protocol protocol = Schema.Protocol.valueOf(row[0]);
      Schema.ExchangeRecord actual = Schema.SerializerCompression.decode(Files.readAllBytes(Path.of(row[2])),
          new Schema.SerializerCompression.DecodeOptions(Schema.SerializerCompression.Format.valueOf(row[1])),
          bytes -> Schema.ExchangeRecord.decode(bytes, protocol));
      sameModel(actual, expected);
      ++exchanges;
    }
    int decisions = 0;
    for (String line : Files.readAllLines(Path.of(args[3]))) {
      String[] row = line.split("\t", -1);
      String observed = replay(row, output);
      if (!observed.equals(row[5])) throw new AssertionError(row[0] + ": expected " + row[5] + ", observed " + observed);
      ++decisions;
    }
    System.out.println("{\"model_exchanges\":" + exchanges + ",\"corpus_decisions\":" + decisions + "}");
  }
}
