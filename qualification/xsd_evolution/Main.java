// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package evolution_consumer;

import generated.Schema.Evolution.Order;
import generated.Schema.Protocol;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.Optional;

/** Exercise unchanged native APIs against independently evolved XML associations. */
public final class Main {
  /** Reject an unexpected model without publishing a result file. */
  private static void check(boolean condition, String message) {
    if (!condition) { throw new AssertionError(message); }
  }

  /** Check native fields and the generated readonly callable independently of XML names. */
  private static void inspect(Order value, String customer, Optional<String> note) {
    check(value.id == 7, "Native id changed");
    check(value.customer.equals(customer), "Native customer changed");
    check(value.note.equals(note), "Native note changed");
    check(value.hasId(7) && !value.hasId(8), "Native readonly callable changed");
  }

  /** Construct the authored model through the same public field names in either variant. */
  private static Order authored(Optional<String> note) {
    var value = new Order();
    value.id = 7;
    value.customer = "R&D";
    value.note = note;
    inspect(value, "R&D", note);
    return value;
  }

  /** Encode every output only after the owning defaults, XML projection and native APIs pass. */
  public static void main(String[] args) throws Exception {
    if (args.length != 3 && args.length != 4) {
      throw new IllegalArgumentException("Expected VARIANT emit OUTDIR or VARIANT check-xml/check-native INPUT OUTPUT");
    }
    String variant = args[0], action = args[1];
    check(variant.equals("v1") || variant.equals("v2"), "Unknown variant");
    Optional<String> xmlNote = Optional.of("memo");
    if (action.equals("emit") && args.length == 3) {
      var fresh = new Order();
      inspect(fresh, "native default", Optional.empty());
      var value = authored(Optional.of("memo"));
      byte[] xml = value.encodeXml();
      byte[] nativeBytes = value.encode(Protocol.BINARY_NONE);
      byte[] integerBytes = value.encode(Protocol.BINARY_INTEGER);
      byte[] stringBytes = value.encode(Protocol.BINARY_STRING);
      byte[] defaultBytes = fresh.encode(Protocol.BINARY_NONE);
      byte[] projected = authored(xmlNote).encode(Protocol.BINARY_NONE);
      var decoded = Order.decodeXml(xml);
      inspect(decoded, "R&D", xmlNote);
      check(Arrays.equals(decoded.encode(Protocol.BINARY_NONE), projected), "XML projection differs");
      inspect(Order.decode(nativeBytes, Protocol.BINARY_NONE), "R&D", Optional.of("memo"));
      Path directory = Path.of(args[2]);
      Files.createDirectories(directory);
      Files.write(directory.resolve("authored.xml"), xml);
      Files.write(directory.resolve("authored.bin"), nativeBytes);
      Files.write(directory.resolve("authored-integer.bin"), integerBytes);
      Files.write(directory.resolve("authored-string.bin"), stringBytes);
      Files.write(directory.resolve("default.bin"), defaultBytes);
      Files.write(directory.resolve("xml-projection.bin"), projected);
      return;
    }
    check(args.length == 4, "Expected input and output paths");
    byte[] input = Files.readAllBytes(Path.of(args[2]));
    byte[] output;
    if (action.equals("check-xml")) {
      var value = Order.decodeXml(input);
      inspect(value, "R&D", xmlNote);
      output = value.encode(Protocol.BINARY_NONE);
    } else if (action.equals("check-native")) {
      var value = Order.decode(input, Protocol.BINARY_NONE);
      inspect(value, "R&D", Optional.of("memo"));
      output = value.encodeXml();
    } else {
      throw new IllegalArgumentException("Unknown action");
    }
    Files.write(Path.of(args[3]), output);
  }
}
