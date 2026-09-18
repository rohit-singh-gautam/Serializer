using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Web.Script.Serialization;
using Jint;

namespace Rohit.Serializer.VisualStudio {
  public sealed class NavigationTarget {
    public string file { get; set; }
    public int start { get; set; }
    public int end { get; set; }
  }

  /// <summary>Runs the shared, bundled resolver in-process; no compiler, Node.js or build process is launched.</summary>
  public static class NavigationRunner {
    private const long MaximumFileBytes = 16 * 1024 * 1024;
    private const int MaximumInventoryFiles = 100000;
    private static readonly string Script = LoadScript();

    /// <summary>Read the bundled source from this assembly so it cannot drift from the installed package.</summary>
    private static string LoadScript() {
      using (var stream = typeof(NavigationRunner).Assembly.GetManifestResourceStream("Rohit.Serializer.navigation.js")) {
        using (var reader = new StreamReader(stream)) { return reader.ReadToEnd(); }
      }
    }

    /// <summary>Create request-local interpreter state with cancellation and resource limits.</summary>
    private static Engine CreateEngine(CancellationToken token) {
      token.ThrowIfCancellationRequested();
      var engine = new Engine(options => options.CancellationToken(token)
        .TimeoutInterval(TimeSpan.FromSeconds(30)).LimitMemory(256 * 1024 * 1024));
      engine.SetValue("hostResolve", new Func<string, string, string>((directory, name) => Path.GetFullPath(Path.Combine(directory, name))));
      engine.Execute(Script);
      return engine;
    }

    /// <summary>Return a schema token range without searching the workspace.</summary>
    public static NavigationTarget Reference(string file, string text, int offset, CancellationToken token) {
      using (var engine = CreateEngine(token)) {
        return new JavaScriptSerializer().Deserialize<NavigationTarget>(
          engine.Invoke(engine.GetValue("serializerNavigation").AsObject().Get("reference"), file, text, offset).AsString());
      }
    }

    /// <summary>Resolve a schema or generated declaration using immutable copies of live documents.</summary>
    public static NavigationTarget[] Resolve(string file, int offset, bool definition, string root,
      IDictionary<string, string> liveDocuments, CancellationToken token) {
      var json = new JavaScriptSerializer { MaxJsonLength = int.MaxValue };
      using (var engine = CreateEngine(token)) {
        engine.SetValue("hostCancelled", new Func<bool>(() => token.IsCancellationRequested));
        engine.SetValue("hostRead", new Func<string, string>(name => Read(name, liveDocuments, token)));
        engine.SetValue("hostFiles", new Func<string>(() => json.Serialize(Inventory(root, liveDocuments.Keys, token))));
        try {
          var result = engine.Invoke(engine.GetValue("serializerNavigation").AsObject().Get("navigate"), file, offset, definition)
            .UnwrapIfPromise().AsString();
          return json.Deserialize<NavigationTarget[]>(result);
        } catch (Jint.Runtime.ExecutionCanceledException) {
          throw new OperationCanceledException(token);
        }
      }
    }

    /// <summary>Prefer captured editor contents; inaccessible or oversized disk files are navigation misses.</summary>
    private static string Read(string file, IDictionary<string, string> liveDocuments, CancellationToken token) {
      token.ThrowIfCancellationRequested();
      string text;
      if (liveDocuments.TryGetValue(Path.GetFullPath(file), out text)) { return text; }
      try {
        var info = new FileInfo(file);
        return info.Exists && info.Length <= MaximumFileBytes ? File.ReadAllText(file) : null;
      } catch (IOException) { return null; } catch (UnauthorizedAccessException) { return null; }
    }

    /// <summary>Walk an explicit project root, skipping dependency caches and directory links.</summary>
    private static string[] Inventory(string root, IEnumerable<string> liveFiles, CancellationToken token) {
      var files = new HashSet<string>(liveFiles, StringComparer.OrdinalIgnoreCase);
      var pending = new Stack<string>();
      pending.Push(root);
      var excluded = new HashSet<string>(new[] { ".git", "node_modules", ".venv", ".vscode-test", ".vs" }, StringComparer.OrdinalIgnoreCase);
      var extensions = new HashSet<string>(new[] { ".serializer", ".d", ".hpp", ".h", ".hh", ".hxx", ".java", ".js", ".mjs", ".cjs", ".ts", ".mts", ".cts", ".go", ".cs", ".rs", ".py", ".swift", ".kt" }, StringComparer.OrdinalIgnoreCase);
      while (pending.Count > 0 && files.Count < MaximumInventoryFiles) {
        token.ThrowIfCancellationRequested();
        var directory = pending.Pop();
        try {
          foreach (var file in Directory.EnumerateFiles(directory)) {
            if (files.Count >= MaximumInventoryFiles) { break; }
            if (extensions.Contains(Path.GetExtension(file))) { files.Add(file); }
          }
          foreach (var child in Directory.EnumerateDirectories(directory)) {
            if (!excluded.Contains(Path.GetFileName(child)) && (File.GetAttributes(child) & FileAttributes.ReparsePoint) == 0) {
              pending.Push(child);
            }
          }
        } catch (IOException) { } catch (UnauthorizedAccessException) { }
      }
      return files.ToArray();
    }
  }
}
