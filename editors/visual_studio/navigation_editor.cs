using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Editor;
using Microsoft.VisualStudio.Language.Intellisense;
using Microsoft.VisualStudio.OLE.Interop;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.TextManager.Interop;
using Microsoft.VisualStudio.Utilities;
using Microsoft.VisualStudio.Threading;

namespace Rohit.Serializer.VisualStudio {
  [Export]
  internal sealed class NavigationService : IDisposable {
    private readonly ITextDocumentFactoryService documents;
    private readonly ConcurrentDictionary<ITextDocument, byte> live = new ConcurrentDictionary<ITextDocument, byte>();
    private readonly JoinableTaskCollection tasks;
    private readonly JoinableTaskFactory factory;
    private readonly CancellationTokenSource shutdown = new CancellationTokenSource();

    /// <summary>Track editor buffers so unsaved include changes participate without saving documents.</summary>
    [ImportingConstructor]
    public NavigationService(ITextDocumentFactoryService documents) {
      this.documents = documents;
      tasks = ThreadHelper.JoinableTaskContext.CreateCollection();
      factory = ThreadHelper.JoinableTaskContext.CreateFactory(tasks);
      documents.TextDocumentCreated += Created;
      documents.TextDocumentDisposed += Disposed;
    }

    /// <summary>Capture documents opened after this shared service was composed.</summary>
    private void Created(object sender, TextDocumentEventArgs args) { Track(args.TextDocument.TextBuffer); }

    /// <summary>Release snapshots when their document is closed.</summary>
    private void Disposed(object sender, TextDocumentEventArgs args) { byte ignored; live.TryRemove(args.TextDocument, out ignored); }

    /// <summary>Cancel and join owned work before the IDE releases the MEF service.</summary>
    public void Dispose() {
      shutdown.Cancel();
      documents.TextDocumentCreated -= Created;
      documents.TextDocumentDisposed -= Disposed;
      factory.Run(() => tasks.JoinTillEmptyAsync());
      shutdown.Dispose();
    }

    /// <summary>Register existing documents when an editor view is attached.</summary>
    public ITextDocument Track(ITextBuffer buffer) {
      ITextDocument document;
      if (!documents.TryGetTextDocument(buffer, out document)) { return null; }
      live.TryAdd(document, 0);
      return document;
    }

    /// <summary>Limit command interception to schemas and file types emitted by Serializer.</summary>
    public static bool Supported(string file) {
      return new[] { ".serializer", ".hpp", ".h", ".hxx", ".hh", ".java", ".js", ".mjs", ".cjs", ".ts", ".mts", ".cts", ".go", ".cs", ".rs", ".py", ".swift", ".kt" }
        .Contains(Path.GetExtension(file), StringComparer.OrdinalIgnoreCase);
    }

    /// <summary>Use the opened solution/folder, then repository/build markers, without scanning a drive root.</summary>
    private static string WorkspaceRoot(string file) {
      ThreadHelper.ThrowIfNotOnUIThread();
      var solution = ServiceProvider.GlobalProvider.GetService(typeof(SVsSolution)) as IVsSolution;
      string directory, name, options;
      if (solution != null && ErrorHandler.Succeeded(solution.GetSolutionInfo(out directory, out name, out options)) &&
          !string.IsNullOrEmpty(directory) && Directory.Exists(directory)) { return directory; }
      var fallback = Path.GetDirectoryName(file);
      for (var current = new DirectoryInfo(fallback); current != null && current.Parent != null; current = current.Parent) {
        if (Directory.Exists(Path.Combine(current.FullName, ".git")) || File.Exists(Path.Combine(current.FullName, ".git"))) { return current.FullName; }
        if (File.Exists(Path.Combine(current.FullName, "CMakeLists.txt"))) { fallback = current.FullName; }
      }
      return fallback;
    }

    /// <summary>Capture live buffers on the UI thread and run bounded read-only lookup in the background.</summary>
    public async Task<NavigationTarget[]> ResolveAsync(ITextBuffer buffer, int offset, bool definition, CancellationToken token) {
      await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync(token);
      var document = Track(buffer);
      if (document == null) { return new NavigationTarget[0]; }
      var snapshots = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
      foreach (var item in live.Keys) { snapshots[Path.GetFullPath(item.FilePath)] = item.TextBuffer.CurrentSnapshot.GetText(); }
      var file = document.FilePath;
      var root = WorkspaceRoot(file);
      return await Task.Run(() => NavigationRunner.Resolve(file, offset, definition, root, snapshots, token), token).ConfigureAwait(false);
    }

    /// <summary>Navigate only if the initiating editor has not changed while lookup was running.</summary>
    public void Navigate(IWpfTextView view, bool definition, Action fallback = null) {
      ThreadHelper.ThrowIfNotOnUIThread();
      var snapshot = view.TextSnapshot;
      var offset = view.Caret.Position.BufferPosition.Position;
      var cancellation = CancellationTokenSource.CreateLinkedTokenSource(shutdown.Token);
      EventHandler closed = (sender, args) => cancellation.Cancel();
      view.Closed += closed;
      factory.RunAsync(async () => {
        try {
          var targets = await ResolveAsync(view.TextBuffer, offset, definition, cancellation.Token);
          await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync(cancellation.Token);
          if (view.IsClosed || view.TextSnapshot != snapshot || view.Caret.Position.BufferPosition.Position != offset) { return; }
          if (targets.Length == 0) { fallback?.Invoke(); return; }
          var target = targets.Length == 1 ? targets[0] : Choose(targets);
          if (target != null) { Open(target); }
        } catch (OperationCanceledException) { }
        catch (Exception error) { ActivityLog.LogError("Rohit Serializer", error.ToString()); }
        finally {
          await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
          view.Closed -= closed;
          cancellation.Dispose();
        }
      }).FileAndForget("RohitSerializer/Navigation");
    }

    /// <summary>Move to a clicked schema link and run definition lookup on the UI thread.</summary>
    public void NavigateSymbol(IWpfTextView view, SnapshotSpan span) {
      factory.RunAsync(async () => {
        await factory.SwitchToMainThreadAsync(shutdown.Token);
        if (view.IsClosed) { return; }
        view.Caret.MoveTo(span.TranslateTo(view.TextSnapshot, SpanTrackingMode.EdgeInclusive).Start);
        Navigate(view, true);
      }).FileAndForget("RohitSerializer/ControlClick");
    }

    /// <summary>Offer each actual destination when several profiles or languages are available.</summary>
    private NavigationTarget Choose(NavigationTarget[] targets) {
      ThreadHelper.ThrowIfNotOnUIThread();
      var list = new ListBox { ItemsSource = targets.Select(DestinationLabel).ToArray(), SelectedIndex = 0, Margin = new Thickness(8) };
      var open = new Button { Content = "Open", IsDefault = true, Width = 90, Margin = new Thickness(8), HorizontalAlignment = HorizontalAlignment.Right };
      var panel = new DockPanel();
      DockPanel.SetDock(open, Dock.Bottom);
      panel.Children.Add(open);
      panel.Children.Add(list);
      var dialog = new Microsoft.VisualStudio.PlatformUI.DialogWindow { Title = "Serializer destinations", Width = 850, Height = 360, Content = panel };
      open.Click += (sender, args) => dialog.DialogResult = true;
      list.MouseDoubleClick += (sender, args) => dialog.DialogResult = true;
      return dialog.ShowModal() == true && list.SelectedIndex >= 0 ? targets[list.SelectedIndex] : null;
    }

    /// <summary>Distinguish specializations in one file with familiar one-based source line numbers.</summary>
    private string DestinationLabel(NavigationTarget target) {
      var document = live.Keys.FirstOrDefault(item => string.Equals(item.FilePath, target.file, StringComparison.OrdinalIgnoreCase));
      try {
        var text = document != null ? document.TextBuffer.CurrentSnapshot.GetText() : File.ReadAllText(target.file);
        return target.file + ":" + (text.Take(target.start).Count(character => character == '\n') + 1);
      } catch (IOException) { return target.file; } catch (UnauthorizedAccessException) { return target.file; }
    }

    /// <summary>Open the destination with native document services and select the exact identifier.</summary>
    private static void Open(NavigationTarget target) {
      ThreadHelper.ThrowIfNotOnUIThread();
      IVsUIHierarchy hierarchy;
      uint item;
      IVsWindowFrame frame;
      IVsTextView view;
      VsShellUtilities.OpenDocument(ServiceProvider.GlobalProvider, target.file, VSConstants.LOGVIEWID.Code_guid, out hierarchy, out item, out frame, out view);
      if (view == null) { return; }
      IVsTextLines buffer;
      ErrorHandler.ThrowOnFailure(view.GetBuffer(out buffer));
      int startLine, startColumn, endLine, endColumn;
      ErrorHandler.ThrowOnFailure(buffer.GetLineIndexOfPosition(target.start, out startLine, out startColumn));
      ErrorHandler.ThrowOnFailure(buffer.GetLineIndexOfPosition(target.end, out endLine, out endColumn));
      view.SetSelection(startLine, startColumn, endLine, endColumn);
      view.EnsureSpanVisible(new TextSpan { iStartLine = startLine, iStartIndex = startColumn, iEndLine = endLine, iEndIndex = endColumn });
    }
  }

  [Export(typeof(IVsTextViewCreationListener))]
  [ContentType("text")]
  [TextViewRole(PredefinedTextViewRoles.Document)]
  internal sealed class NavigationEditor : IVsTextViewCreationListener {
    [Import] internal IVsEditorAdaptersFactoryService Adapters = null;
    [Import] internal NavigationService Navigation = null;

    /// <summary>Attach a native declaration/definition command filter without replacing the language service.</summary>
    public void VsTextViewCreated(IVsTextView adapter) {
      ThreadHelper.ThrowIfNotOnUIThread();
      var view = Adapters.GetWpfTextView(adapter);
      if (view == null) { return; }
      var document = Navigation.Track(view.TextBuffer);
      if (document == null || !NavigationService.Supported(document.FilePath)) { return; }
      var filter = new NavigationCommand(view, document, Navigation);
      IOleCommandTarget next;
      if (ErrorHandler.Succeeded(adapter.AddCommandFilter(filter, out next))) {
        filter.Next = next;
        view.Closed += (sender, args) => adapter.RemoveCommandFilter(filter);
      }
    }
  }

  internal sealed class NavigationCommand : IOleCommandTarget {
    private readonly IWpfTextView view;
    private readonly ITextDocument document;
    private readonly NavigationService navigation;
    public IOleCommandTarget Next;

    /// <summary>Retain the editor context and the rest of its existing command chain.</summary>
    public NavigationCommand(IWpfTextView view, ITextDocument document, NavigationService navigation) {
      this.view = view; this.document = document; this.navigation = navigation;
    }

    /// <summary>Handle both native commands for schemas, and declaration mapping for generated code.</summary>
    private bool Handles(Guid group, uint command) {
      return group == VSConstants.GUID_VSStandardCommandSet97 &&
        (command == (uint)VSConstants.VSStd97CmdID.GotoDecl ||
         (command == (uint)VSConstants.VSStd97CmdID.GotoDefn && document.FilePath.EndsWith(".serializer", StringComparison.OrdinalIgnoreCase)));
    }

    /// <summary>Enable the standard menu entries while preserving all unrelated command status.</summary>
    public int QueryStatus(ref Guid group, uint count, OLECMD[] commands, IntPtr text) {
      ThreadHelper.ThrowIfNotOnUIThread();
      if (count == 1 && Handles(group, commands[0].cmdID)) {
        commands[0].cmdf = (uint)(OLECMDF.OLECMDF_SUPPORTED | OLECMDF.OLECMDF_ENABLED);
        return VSConstants.S_OK;
      }
      return Next?.QueryStatus(ref group, count, commands, text) ?? (int)Microsoft.VisualStudio.OLE.Interop.Constants.OLECMDERR_E_NOTSUPPORTED;
    }

    /// <summary>Resolve asynchronously; ordinary generated-language misses continue down the native chain.</summary>
    public int Exec(ref Guid group, uint command, uint options, IntPtr input, IntPtr output) {
      ThreadHelper.ThrowIfNotOnUIThread();
      if (!Handles(group, command)) { return Next?.Exec(ref group, command, options, input, output) ?? (int)Microsoft.VisualStudio.OLE.Interop.Constants.OLECMDERR_E_NOTSUPPORTED; }
      var originalGroup = group;
      navigation.Navigate(view, command == (uint)VSConstants.VSStd97CmdID.GotoDefn,
        document.FilePath.EndsWith(".serializer", StringComparison.OrdinalIgnoreCase) ? (Action)null :
        () => {
          ThreadHelper.ThrowIfNotOnUIThread();
          Next?.Exec(ref originalGroup, command, options, IntPtr.Zero, IntPtr.Zero);
        });
      return VSConstants.S_OK;
    }
  }

  [Export(typeof(INavigableSymbolSourceProvider))]
  [Name("Rohit Serializer navigation")]
  [ContentType("text")]
  internal sealed class NavigationSymbols : INavigableSymbolSourceProvider {
    [Import] internal NavigationService Navigation = null;

    /// <summary>Offer Ctrl+click only for schemas, leaving every other language's source untouched.</summary>
    public INavigableSymbolSource TryCreateNavigableSymbolSource(ITextView textView, ITextBuffer buffer) {
      var document = Navigation.Track(buffer);
      return document != null && document.FilePath.EndsWith(".serializer", StringComparison.OrdinalIgnoreCase) && textView is IWpfTextView
        ? new Source((IWpfTextView)textView, document, Navigation) : null;
    }

    private sealed class Source : INavigableSymbolSource {
      private readonly IWpfTextView view;
      private readonly ITextDocument document;
      private readonly NavigationService navigation;

      /// <summary>Bind hover requests to one view and its schema document.</summary>
      public Source(IWpfTextView view, ITextDocument document, NavigationService navigation) {
        this.view = view; this.document = document; this.navigation = navigation;
      }

      /// <summary>Resolve schema ranges off the UI thread and suppress links to unknown types.</summary>
      public async Task<INavigableSymbol> GetNavigableSymbolAsync(SnapshotSpan triggerSpan, CancellationToken token) {
        try {
          var text = triggerSpan.Snapshot.GetText();
          var span = await Task.Run(() => NavigationRunner.Reference(document.FilePath, text, triggerSpan.Start.Position, token), token);
          if (span == null) { return null; }
          var targets = await navigation.ResolveAsync(document.TextBuffer, triggerSpan.Start.Position, false, token);
          return targets.Length == 0 ? null : new Symbol(view, navigation, new SnapshotSpan(triggerSpan.Snapshot, Span.FromBounds(span.start, span.end)));
        } catch (OperationCanceledException) { return null; }
        catch (Exception error) { ActivityLog.LogError("Rohit Serializer", error.ToString()); return null; }
      }

      /// <summary>No unmanaged resources are retained by the per-view symbol source.</summary>
      public void Dispose() { }
    }

    private sealed class Symbol : INavigableSymbol {
      private readonly IWpfTextView view;
      private readonly NavigationService navigation;
      public SnapshotSpan SymbolSpan { get; }
      public IEnumerable<INavigableRelationship> Relationships { get; } = new[] { PredefinedNavigableRelationships.Definition };

      /// <summary>Retain a precise source range for the editor's link underline.</summary>
      public Symbol(IWpfTextView view, NavigationService navigation, SnapshotSpan span) {
        this.view = view; this.navigation = navigation; SymbolSpan = span;
      }

      /// <summary>Move to the clicked symbol before invoking the same standard definition resolver.</summary>
      public void Navigate(INavigableRelationship relationship) {
        ThreadHelper.ThrowIfNotOnUIThread();
        navigation.NavigateSymbol(view, SymbolSpan);
      }
    }
  }
}
