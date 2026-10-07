using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using Rohit.Serializer.VisualStudio;

internal static class NavigationTest {
  /// <summary>Check the installed interpreter and path adapter against real schemas and generated output.</summary>
  private static int Main(string[] args) {
    try {
      var repository = Path.GetFullPath(args[0]);
      var generated = Path.GetFullPath(args[1]);
      var model = Path.GetFullPath(Path.Combine(repository, "example/schemas/complex/model.serializer"));
      var text = File.ReadAllText(model).Replace("serializer version 1;", "serializer version 1.0.0;");
      var live = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase) { [model] = text };
      var checks = 0;
      foreach (var name in new[] { "demo::order", "demo::snapshot", "demo::customer" }) {
        var start = text.IndexOf(name, StringComparison.Ordinal);
        foreach (var offset in new[] { start, start + name.Length - 1, start + name.Length }) {
          var reference = NavigationRunner.Reference(model, text, offset, CancellationToken.None);
          Require(reference != null && reference.start == start && reference.end == start + name.Length, "schema token range");
          var declarations = NavigationRunner.Resolve(model, offset, false, generated, live, CancellationToken.None);
          Require(declarations.Length == 1, "one source declaration for " + name);
          Require(File.ReadAllText(declarations[0].file).Substring(declarations[0].start, declarations[0].end - declarations[0].start) == name.Split(':').Last(), "exact source identifier");
          ++checks;
        }
        var definitions = NavigationRunner.Resolve(model, start + name.Length, true, generated, live, CancellationToken.None);
        Require(definitions.Length == 11, "all 11 output languages for " + name + ": " + string.Join(", ", definitions.Select(item => item.file)));
        foreach (var definition in definitions) {
          var source = NavigationRunner.Resolve(definition.file, definition.end, false, generated, live, CancellationToken.None);
          Require(source.Length == 1 && Path.GetFileName(source[0].file) == name.Split(':').Last() + ".serializer", "reverse output mapping " + definition.file);
          ++checks;
        }
      }
      // Exercise the new metadata type operands against the same transitive and generated enum graph.
      var typedText = "serializer version 1.1.0; include sales/order; class typed_model { " +
        "public array[] float numbers (2) {1'000.25, 2'000.5}; " +
        "private magic demo::order_state (99) {demo::order_state::draft}; " +
        "public array[] demo::order_state states (1) {demo::order_state::draft}; }";
      live[model] = typedText;
      var enumName = "demo::order_state";
      for (var occurrence = typedText.IndexOf(enumName, StringComparison.Ordinal); occurrence >= 0;
           occurrence = typedText.IndexOf(enumName, occurrence + enumName.Length, StringComparison.Ordinal)) {
        for (var offset = occurrence; offset <= occurrence + enumName.Length; ++offset) {
          var destination = NavigationRunner.Resolve(model, offset, false, generated, live, CancellationToken.None);
          Require(destination.Length == 1 && Path.GetFileName(destination[0].file) == "order.serializer",
            "typed magic/inferred array enum cursor boundary");
          ++checks;
        }
        var definitions = NavigationRunner.Resolve(model, occurrence, true, generated, live, CancellationToken.None);
        Require(definitions.Select(item => item.file).Distinct().Count() == 11,
          "typed magic/inferred array enum definitions in every output language");
        ++checks;
      }
      live[model] = text;
      // Generic compact operands remain navigable and modifiers never become destinations.
      var compactText = "serializer version 1.2.0; include sales/order; class compact_box<T> { " +
        "public compact_prefix strict T first; public compact_varint lenient T second; " +
        "public compact_varint T third; public demo::order owner; }";
      live[model] = compactText;
      foreach (var member in new[] { "T first", "T second", "T third" }) {
        var start = compactText.IndexOf(member, StringComparison.Ordinal);
        for (var offset = start; offset <= start + 1; ++offset) {
          var destination = NavigationRunner.Resolve(model, offset, false, generated, live, CancellationToken.None);
          Require(destination.Length == 1 && destination[0].file == model &&
            compactText.Substring(destination[0].start, destination[0].end - destination[0].start) == "T",
            "compact generic type cursor boundary");
          ++checks;
        }
      }
      foreach (var modifier in new[] { "compact_prefix", "compact_varint", "strict", "lenient" }) {
        var offset = compactText.IndexOf(modifier, StringComparison.Ordinal);
        Require(NavigationRunner.Resolve(model, offset, false, generated, live, CancellationToken.None).Length == 0,
          "compact modifier is not a type reference");
        ++checks;
      }
      var compactOwner = compactText.IndexOf("demo::order", StringComparison.Ordinal);
      Require(NavigationRunner.Resolve(model, compactOwner, true, generated, live, CancellationToken.None).Length == 11,
        "adjacent compact output navigation in every language");
      ++checks;
      live[model] = text;
      var order = Path.GetFullPath(Path.Combine(repository, "example/schemas/complex/sales/order.serializer"));
      live[order] = File.ReadAllText(order).Replace("serializer version 1;", "serializer version 1.0.0;")
        .Replace("class order ", "class unsaved_order ");
      live[model] = text.Replace("demo::order", "demo::unsaved_order");
      var unsaved = NavigationRunner.Resolve(model, live[model].IndexOf("demo::unsaved_order", StringComparison.Ordinal), false, generated, live, CancellationToken.None);
      Require(unsaved.Length == 1 && live[order].Substring(unsaved[0].start, unsaved[0].end - unsaved[0].start) == "unsaved_order", "unsaved transitive schema edits");
      using (var cancelled = new CancellationTokenSource()) {
        cancelled.Cancel();
        try {
          NavigationRunner.Resolve(model, 0, true, generated, live, cancelled.Token);
          throw new Exception("Cancelled navigation did not stop");
        } catch (OperationCanceledException) { }
      }
      if (args.Length > 2) {
        var genericOutput = Path.GetFullPath(args[2]);
        var genericModel = Path.GetFullPath(Path.Combine(repository, "example/generics/result.serializer"));
        var genericText = File.ReadAllText(genericModel);
        live[genericModel] = genericText;
        var genericStart = genericText.IndexOf("result<T>", StringComparison.Ordinal);
        var definitions = NavigationRunner.Resolve(genericModel, genericStart, true, genericOutput, live, CancellationToken.None);
        Require(definitions.Select(item => Path.GetExtension(item.file)).Distinct().Count() == 11,
          "generic definitions in all eleven output languages");
        foreach (var definition in definitions) {
          var source = NavigationRunner.Resolve(definition.file, definition.end, false, genericOutput, live, CancellationToken.None);
          Require(source.Length == 1 && source[0].file == genericModel && source[0].start == genericStart,
            "generic reverse mapping " + definition.file);
          ++checks;
        }
        var parameter = genericText.IndexOf("T value", StringComparison.Ordinal);
        var local = NavigationRunner.Resolve(genericModel, parameter, false, genericOutput, live, CancellationToken.None);
        Require(local.Length == 1 && local[0].start == genericStart + "result<".Length, "generic parameter declaration");
        ++checks;
      }
      var dimensionsFile = Path.GetFullPath(Path.Combine(repository, "test/resources/dimensions.serializer"));
      var dimensionsText = File.ReadAllText(dimensionsFile);
      live[dimensionsFile] = dimensionsText;
      foreach (var parameterName in new[] { "Rows", "Cols" }) {
        var declaration = dimensionsText.IndexOf(parameterName, StringComparison.Ordinal);
        for (var occurrence = declaration; occurrence >= 0;
             occurrence = dimensionsText.IndexOf(parameterName, occurrence + parameterName.Length, StringComparison.Ordinal)) {
          for (var offset = occurrence; offset <= occurrence + parameterName.Length; ++offset) {
            var destination = NavigationRunner.Resolve(dimensionsFile, offset, false, generated, live, CancellationToken.None);
            Require(destination.Length == 1 && destination[0].file == dimensionsFile && destination[0].start == declaration,
              "dimension parameter/default/extent boundary " + parameterName);
            ++checks;
          }
        }
      }
      Console.WriteLine("Visual Studio shared resolver passed: " + checks + " source/output checks, real .NET interpreter, all 11 languages, dimension defaults/extents, typed magic/inferred arrays, selection endpoints, unsaved includes and cancellation.");
      return 0;
    } catch (Exception error) { Console.Error.WriteLine(error); return 1; }
  }

  /// <summary>Fail the executable on any observable navigation mismatch.</summary>
  private static void Require(bool condition, string message) {
    if (!condition) { throw new Exception(message); }
  }
}
