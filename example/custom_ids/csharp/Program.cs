using System;
using System.Collections.Generic;
using static SerializerGenerated.Schema;

internal static class Program {
  private const uint WidgetDomain = 1;
  private const uint GadgetDomain = 2;

  // Create an independent value key; no object address or CLR runtime type enters bytes.
  private static CustomId MakeId(uint scope, uint domain, ulong local) {
    return new CustomId { Scope = scope, Domain = domain, Local = local };
  }
  // Compare the complete scoped/type-tag identity in an application-owned host index.
  private static (uint, uint, ulong) Key(CustomId id) { return (id.Scope, id.Domain, id.Local); }
  // Reserve zero values and reject unknown stable application domain tags.
  private static bool Valid(CustomId id) { return id.Scope != 0 && id.Local != 0 && (id.Domain == WidgetDomain || id.Domain == GadgetDomain); }
  // Stop immediately on any policy or codec mismatch.
  private static void Check(bool success) { if (!success) throw new Exception("Custom-ID registry contract failed"); }

  // This single-threaded host owns policy over ordinary generated durable records.
  private sealed class ApplicationRegistry {
    internal RegistryState State { get; }
    // Transfer ownership and validate before any application lookup.
    internal ApplicationRegistry(RegistryState state) { State = state; Check(Validate()); }
    // Reject duplicate keys and overlapping live/retired identity sets after decoding.
    internal bool Validate() {
      var seen = new HashSet<(uint, uint, ulong)>();
      foreach (var binding in State.Bindings) if (!Valid(binding.Id) || !seen.Add(Key(binding.Id))) return false;
      foreach (var id in State.Retired) if (!Valid(id) || !seen.Add(Key(id))) return false;
      return true;
    }
    // Missing, retired, and wrong-domain keys return an absent payload.
    internal uint? Lookup(CustomId id, uint expectedDomain) {
      if (Valid(id) && id.Domain == expectedDomain) {
        foreach (var binding in State.Bindings) if (Key(binding.Id) == Key(id)) return binding.Payload;
      }
      return null;
    }
    // Copy a new identity and forbid reuse of any retired complete key.
    internal bool Bind(CustomId id, uint payload) {
      if (!Valid(id) || Lookup(id, id.Domain).HasValue) return false;
      foreach (var retired in State.Retired) if (Key(retired) == Key(id)) return false;
      State.Bindings.Add(new EntityBinding { Id = MakeId(id.Scope, id.Domain, id.Local), Payload = payload });
      return true;
    }
    // Update payload while preserving scope, domain, and local logical ID.
    internal bool Rebind(CustomId id, uint payload) {
      foreach (var binding in State.Bindings) if (Key(binding.Id) == Key(id)) { binding.Payload = payload; return true; }
      return false;
    }
    // Persist retirement before removal; the primitive-key list removal cannot allocate.
    internal bool Retire(CustomId id) {
      for (var index = 0; index < State.Bindings.Count; ++index) {
        if (Key(State.Bindings[index].Id) != Key(id)) continue;
        State.Retired.Add(MakeId(id.Scope, id.Domain, id.Local));
        State.Bindings.RemoveAt(index); return true;
      }
      return false;
    }
  }

  // Exercise all policy operations and independently restore ordinary state in every protocol.
  private static void Main() {
    var registry = new ApplicationRegistry(new RegistryState());
    var first = MakeId(7, WidgetDomain, 42); var scoped = MakeId(8, WidgetDomain, 42); var typed = MakeId(7, GadgetDomain, 42);
    Check(registry.Bind(first, 10) && registry.Bind(scoped, 20) && registry.Bind(typed, 30));
    Check(!registry.Bind(first, 99) && registry.Lookup(first, WidgetDomain) == 10);
    Check(!registry.Lookup(typed, WidgetDomain).HasValue && !registry.Lookup(MakeId(9, WidgetDomain, 42), WidgetDomain).HasValue);
    Check(!registry.Lookup(MakeId(7, WidgetDomain, 99), WidgetDomain).HasValue);
    Check(registry.Rebind(first, 11) && registry.Lookup(first, WidgetDomain) == 11 && Key(registry.State.Bindings[0].Id) == Key(first));
    Check(registry.Retire(first) && !registry.Lookup(first, WidgetDomain).HasValue);
    Check(!registry.Bind(first, 12) && !registry.Rebind(first, 12));
    var canonical = registry.State.Encode(Protocol.BINARY_NONE);
    foreach (Protocol protocol in Enum.GetValues<Protocol>()) {
      var restored = new ApplicationRegistry(RegistryState.Decode(registry.State.Encode(protocol), protocol));
      Check(restored.State.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(canonical));
      Check(!restored.Bind(first, 12) && restored.Lookup(scoped, WidgetDomain) == 20);
    }
    Console.WriteLine("Scoped custom IDs and four protocols passed");
  }
}
