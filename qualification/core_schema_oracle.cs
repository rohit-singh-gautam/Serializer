// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Text.Json;
using System.Xml;
using System.Xml.Schema;
using System.Xml.Linq;

// Independently validate original ZIP model streams without changing finite occurrence bounds.
static class core_schema_oracle {
  // Exhaust the streaming validator so later malformed bytes cannot hide behind a valid prefix.
  static string validate(Stream source, XmlSchemaSet schemas) {
    string error = "";
    var settings = new XmlReaderSettings {
      Schemas = schemas, ValidationType = ValidationType.Schema,
      DtdProcessing = DtdProcessing.Prohibit, XmlResolver = null
    };
    settings.ValidationEventHandler += (_, event_args) => {
      if (error.Length == 0) { error = event_args.Message; }
    };
    try {
      using var reader = XmlReader.Create(source, settings);
      while (reader.Read()) {
        if (reader.NodeType == XmlNodeType.Element && reader.Depth == 0
            && (reader.LocalName != "model" || reader.NamespaceURI != "http://schemas.microsoft.com/3dmanufacturing/core/2015/02")
            && error.Length == 0) {
          error = "Unexpected Core model root";
        }
      }
    } catch (Exception exception) { if (error.Length == 0) { error = exception.Message; } }
    return error;
  }

  // Compile the unchanged two-file bundle with no network resolution or external entity access.
  static int Main(string[] arguments) {
    if (arguments.Length < 3 || arguments.Length > 4) {
      throw new ArgumentException("core schema, XML schema and input list, optionally --xml, required");
    }
    var schemas = new XmlSchemaSet { XmlResolver = null };
    schemas.Add("http://www.w3.org/XML/1998/namespace", arguments[1]);
    schemas.Add("http://schemas.microsoft.com/3dmanufacturing/core/2015/02", arguments[0]);
    schemas.Compile();
    foreach (var archive in File.ReadAllLines(arguments[2])) {
      if (arguments.Length == 4) {
        if (arguments[3] != "--xml") { throw new ArgumentException("Unknown oracle mode"); }
        using var input = File.OpenRead(archive);
        var failure = validate(input, schemas);
        Console.WriteLine(JsonSerializer.Serialize(new {
          archive, valid = failure.Length == 0, diagnostic = failure
        }));
        continue;
      }
      using var package = ZipFile.OpenRead(archive);
      var overrides = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
      var defaults = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
      var content_types = package.GetEntry("[Content_Types].xml");
      if (content_types != null) {
        try {
          using var source = content_types.Open();
          using var reader = XmlReader.Create(source, new XmlReaderSettings {
            DtdProcessing = DtdProcessing.Prohibit, XmlResolver = null
          });
          var document = XDocument.Load(reader);
          XNamespace type_namespace = "http://schemas.openxmlformats.org/package/2006/content-types";
          foreach (var item in document.Root.Elements()) {
            if (item.Name == type_namespace + "Override") {
              var name = (string)item.Attribute("PartName");
              if (name != null) { overrides[Uri.UnescapeDataString(name.TrimStart('/'))] = (string)item.Attribute("ContentType"); }
            } else if (item.Name == type_namespace + "Default") {
              var extension = (string)item.Attribute("Extension");
              if (extension != null) { defaults[extension] = (string)item.Attribute("ContentType"); }
            }
          }
        } catch (XmlException) {
          // Package-invalid negative cases still have independently identifiable .model XML payloads.
        }
      }
      foreach (var entry in package.Entries) {
        if (!overrides.TryGetValue(entry.FullName, out var content_type)) {
          var dot = entry.FullName.LastIndexOf('.');
          if (dot >= 0) { defaults.TryGetValue(entry.FullName.Substring(dot + 1), out content_type); }
        }
        if (!entry.FullName.EndsWith(".model", StringComparison.OrdinalIgnoreCase)
            && content_type != "application/vnd.ms-package.3dmanufacturing-3dmodel+xml") { continue; }
        using var source = entry.Open();
        string error = validate(source, schemas);
        Console.WriteLine(JsonSerializer.Serialize(new {
          archive, part = entry.FullName, bytes = entry.Length, valid = error.Length == 0, diagnostic = error
        }));
      }
    }
    return 0;
  }
}
