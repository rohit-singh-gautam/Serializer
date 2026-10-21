// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
using System;
using System.IO;
using System.Net;
using System.Xml;
using System.Xml.Schema;

// Resolve only the explicitly downloaded normative schema dependencies, with no network access.
sealed class LocalSchemas : XmlResolver {
  readonly string directory;
  // Bind this oracle to a caller-selected local copy of the official schema bundle.
  public LocalSchemas(string selected) { directory = Path.GetFullPath(selected); }
  // Credentials are unused because remote resources are never fetched.
  public override ICredentials Credentials { set { } }
  // Map each known import basename to the selected immutable test inputs.
  public override object GetEntity(Uri uri, string role, Type returnType) {
    string name = Path.GetFileName(uri.AbsolutePath);
    switch (name) {
      case "dc.xsd": case "dcterms.xsd": case "dcmitype.xsd": case "xml.xsd":
      case "opc-coreProperties.xsd": return File.OpenRead(Path.Combine(directory, name));
      default: throw new XmlException("Unlisted schema dependency");
    }
  }
}

static class CorePropertiesOracle {
  // Use Microsoft's independent XSD implementation; this checks schema rules, not extra OPC prose.
  static int Main(string[] args) {
    if (args.Length != 2) { return 1; }
    try {
      var schemas = new XmlSchemaSet();
      schemas.XmlResolver = new LocalSchemas(args[0]);
      // Follow the normative root's actual import graph once; pre-adding XML under a file URI
      // would duplicate the same declarations when DC imports it under its normative HTTP URI.
      schemas.Add(null, Path.Combine(args[0], "opc-coreProperties.xsd"));
      schemas.Compile();
      var settings = new XmlReaderSettings();
      settings.XmlResolver = null;
      settings.DtdProcessing = DtdProcessing.Prohibit;
      settings.ValidationType = ValidationType.Schema;
      settings.Schemas = schemas;
      settings.ValidationFlags = XmlSchemaValidationFlags.ReportValidationWarnings;
      settings.ValidationEventHandler += delegate(object sender, ValidationEventArgs failure) {
        throw new XmlSchemaValidationException(failure.Message);
      };
      using (XmlReader reader = XmlReader.Create(args[1], settings)) {
        while (reader.Read()) { }
      }
      Console.WriteLine("VALID");
      return 0;
    } catch (XmlException failure) { Console.Error.WriteLine(failure.Message); return 2; }
      catch (XmlSchemaException failure) { Console.Error.WriteLine(failure.Message); return 2; }
  }
}
