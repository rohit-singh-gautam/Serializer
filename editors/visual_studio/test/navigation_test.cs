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
      var text = File.ReadAllText(model);
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
      var order = Path.GetFullPath(Path.Combine(repository, "example/schemas/complex/sales/order.serializer"));
      live[order] = File.ReadAllText(order).Replace("class order ", "class unsaved_order ");
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
      Console.WriteLine("Visual Studio shared resolver passed: " + checks + " source/output checks, real .NET interpreter, all 11 languages, selection endpoints, unsaved includes and cancellation.");
      return 0;
    } catch (Exception error) { Console.Error.WriteLine(error); return 1; }
  }

  /// <summary>Fail the executable on any observable navigation mismatch.</summary>
  private static void Require(bool condition, string message) {
    if (!condition) { throw new Exception(message); }
  }
}
