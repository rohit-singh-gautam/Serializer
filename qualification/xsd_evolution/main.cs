// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Xml;
using Model = SerializerGenerated.Schema.EvolutionOrder;
using Protocol = SerializerGenerated.Schema.Protocol;

// Exercise unchanged authored APIs through separately generated XSD mapping variants.
internal static class Consumer {
  // Check native state and prove the readonly callable leaves its durable representation intact.
  private static void Check(Model value, string customer, string? note) {
    if (value.Id != 7 || value.Customer != customer || value.Note?.Item1 != note) {
      throw new InvalidDataException("Unexpected authored order state");
    }
    byte[] before = value.Encode(Protocol.BINARY_NONE);
    if (!value.HasId(7) || value.HasId(8) ||
        !before.AsSpan().SequenceEqual(value.Encode(Protocol.BINARY_NONE))) {
      throw new InvalidDataException("Readonly order behavior changed durable state");
    }
  }

  // Create the same native value independently of either XML vocabulary.
  private static Model Authored() => new Model {
    Id = 7,
    Customer = "R&D",
    Note = new ValueTuple<string>("memo")
  };

  // Emit complete native baselines and the variant-specific XML projection for independent inspection.
  private static void Emit(string variant, string directory) {
    var defaults = new Model();
    Check(defaults, "native default", null);
    byte[] defaultBytes = defaults.Encode(Protocol.BINARY_NONE);
    Check(Model.Decode(defaultBytes, Protocol.BINARY_NONE), "native default", null);
    var value = Authored();
    Check(value, "R&D", "memo");
    byte[] authored = value.Encode(Protocol.BINARY_NONE);
    byte[] integer = value.Encode(Protocol.BINARY_INTEGER);
    byte[] names = value.Encode(Protocol.BINARY_STRING);
    Check(Model.Decode(authored, Protocol.BINARY_NONE), "R&D", "memo");
    Check(Model.Decode(integer, Protocol.BINARY_INTEGER), "R&D", "memo");
    Check(Model.Decode(names, Protocol.BINARY_STRING), "R&D", "memo");
    byte[] xml = value.EncodeXml();
    var projection = Authored();
    Check(Model.DecodeXml(xml), "R&D", projection.Note?.Item1);
    Check(value, "R&D", "memo");
    Directory.CreateDirectory(directory);
    File.WriteAllBytes(Path.Combine(directory, "authored.xml"), xml);
    File.WriteAllBytes(Path.Combine(directory, "authored.bin"), authored);
    File.WriteAllBytes(Path.Combine(directory, "authored-integer.bin"), integer);
    File.WriteAllBytes(Path.Combine(directory, "authored-string.bin"), names);
    File.WriteAllBytes(Path.Combine(directory, "default.bin"), defaultBytes);
    File.WriteAllBytes(Path.Combine(directory, "xml-projection.bin"), projection.Encode(Protocol.BINARY_NONE));
  }

  // Report the loaded XML and core runtime assemblies rather than guessing an installed runtime path.
  private static void Dependencies() {
    Console.WriteLine(JsonSerializer.Serialize(new {
      framework = RuntimeInformation.FrameworkDescription,
      artifacts = new[] { typeof(XmlReader).Assembly.Location, typeof(object).Assembly.Location }
    }));
  }

  // Validate a private candidate fully before opening the requested result file.
  private static void Run(string[] args) {
    if (args.Length == 1 && args[0] == "--dependencies") { Dependencies(); return; }
    if (args.Length < 3 || (args[0] != "v1" && args[0] != "v2")) {
      throw new ArgumentException("Expected VARIANT emit OUTDIR or VARIANT check-xml/check-native INPUT OUTPUT");
    }
    if (args[1] == "emit" && args.Length == 3) { Emit(args[0], args[2]); return; }
    if (args.Length != 4) { throw new ArgumentException("Expected one input and one output"); }
    byte[] input = File.ReadAllBytes(args[2]);
    byte[] result;
    if (args[1] == "check-xml") {
      var value = Model.DecodeXml(input);
      Check(value, "R&D", "memo");
      result = value.Encode(Protocol.BINARY_NONE);
    } else if (args[1] == "check-native") {
      var value = Model.Decode(input, Protocol.BINARY_NONE);
      Check(value, "R&D", "memo");
      result = value.EncodeXml();
      Check(value, "R&D", "memo");
    } else { throw new ArgumentException("Unknown evolution command"); }
    File.WriteAllBytes(args[3], result);
  }

  // Convert expected malformed input failures to a nonzero process status without platform crash dialogs.
  private static int Main(string[] args) {
    try { Run(args); return 0; }
    catch (Exception error) { Console.Error.WriteLine(error.Message); return 1; }
  }
}
