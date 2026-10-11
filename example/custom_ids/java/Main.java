import java.util.Arrays;
import java.util.HashSet;
import java.util.OptionalInt;

/** Ordinary durable records with a host-owned scoped/type-tag registry policy. */
public final class Main {
  private static final int WIDGET_DOMAIN = 1;
  private static final int GADGET_DOMAIN = 2;

  /** Create an independently owned key; Java's long retains uint64 wire bits. */
  private static Schema.CustomId makeId(int scope, int domain, long local) {
    var result = new Schema.CustomId();
    result.scope = scope; result.domain = domain; result.local = local;
    return result;
  }
  /** Compare every key component rather than a runtime object identity. */
  private static String key(Schema.CustomId id) { return id.scope + ":" + id.domain + ":" + id.local; }
  /** Reserve zero values and reject unknown durable domain tags. */
  private static boolean valid(Schema.CustomId id) { return id.scope != 0 && id.local != 0 && (id.domain == WIDGET_DOMAIN || id.domain == GADGET_DOMAIN); }
  /** Fail immediately when an application or codec contract is violated. */
  private static void check(boolean success) { if (!success) throw new AssertionError("Custom-ID registry contract failed"); }

  /** A single-threaded host owns the model and validates imported state before lookups. */
  private static final class ApplicationRegistry {
    private final Schema.RegistryState state;
    /** Transfer ownership of an ordinary model and verify its binding/retirement invariants. */
    private ApplicationRegistry(Schema.RegistryState value) { state = value; check(validate()); }
    /** Reject invalid and duplicate keys, including overlap between live and retired entries. */
    private boolean validate() {
      var seen = new HashSet<String>();
      for (var binding : state.bindings) if (!valid(binding.id) || !seen.add(key(binding.id))) return false;
      for (var id : state.retired) if (!valid(id) || !seen.add(key(id))) return false;
      return true;
    }
    /** Missing, retired, and wrong-domain IDs return an absent payload. */
    private OptionalInt lookup(Schema.CustomId id, int expectedDomain) {
      if (valid(id) && id.domain == expectedDomain) {
        for (var binding : state.bindings) if (key(binding.id).equals(key(id))) return OptionalInt.of(binding.payload);
      }
      return OptionalInt.empty();
    }
    /** Copy and bind a fresh key; retirement permanently forbids reusing that complete key. */
    private boolean bind(Schema.CustomId id, int payload) {
      if (!valid(id) || lookup(id, id.domain).isPresent()) return false;
      for (var retired : state.retired) if (key(retired).equals(key(id))) return false;
      var binding = new Schema.EntityBinding();
      binding.id = makeId(id.scope, id.domain, id.local); binding.payload = payload;
      var expanded = Arrays.copyOf(state.bindings, state.bindings.length + 1);
      expanded[expanded.length - 1] = binding; state.bindings = expanded;
      return true;
    }
    /** Replace payload without changing any logical ID component. */
    private boolean rebind(Schema.CustomId id, int payload) {
      for (var binding : state.bindings) if (key(binding.id).equals(key(id))) { binding.payload = payload; return true; }
      return false;
    }
    /** Build new owned arrays before publishing retirement and removal together. */
    private boolean retire(Schema.CustomId id) {
      for (int index = 0; index < state.bindings.length; ++index) {
        if (!key(state.bindings[index].id).equals(key(id))) continue;
        var retired = Arrays.copyOf(state.retired, state.retired.length + 1);
        retired[retired.length - 1] = makeId(id.scope, id.domain, id.local);
        var live = new Schema.EntityBinding[state.bindings.length - 1];
        System.arraycopy(state.bindings, 0, live, 0, index);
        System.arraycopy(state.bindings, index + 1, live, index, live.length - index);
        state.retired = retired; state.bindings = live; return true;
      }
      return false;
    }
  }

  /** Demonstrate binding policy and revalidate independently decoded state in all four protocols. */
  public static void main(String[] args) {
    var registry = new ApplicationRegistry(new Schema.RegistryState());
    var first = makeId(7, WIDGET_DOMAIN, 42); var scoped = makeId(8, WIDGET_DOMAIN, 42); var typed = makeId(7, GADGET_DOMAIN, 42);
    check(registry.bind(first, 10) && registry.bind(scoped, 20) && registry.bind(typed, 30));
    check(!registry.bind(first, 99) && registry.lookup(first, WIDGET_DOMAIN).orElseThrow() == 10);
    check(registry.lookup(typed, WIDGET_DOMAIN).isEmpty() && registry.lookup(makeId(9, WIDGET_DOMAIN, 42), WIDGET_DOMAIN).isEmpty());
    check(registry.lookup(makeId(7, WIDGET_DOMAIN, 99), WIDGET_DOMAIN).isEmpty());
    check(registry.rebind(first, 11) && registry.lookup(first, WIDGET_DOMAIN).orElseThrow() == 11);
    check(key(registry.state.bindings[0].id).equals(key(first)));
    check(registry.retire(first) && registry.lookup(first, WIDGET_DOMAIN).isEmpty());
    check(!registry.bind(first, 12) && !registry.rebind(first, 12));
    var canonical = registry.state.encode(Schema.Protocol.BINARY_NONE);
    for (var protocol : Schema.Protocol.values()) {
      var restored = new ApplicationRegistry(Schema.RegistryState.decode(registry.state.encode(protocol), protocol));
      check(Arrays.equals(restored.state.encode(Schema.Protocol.BINARY_NONE), canonical));
      check(!restored.bind(first, 12) && restored.lookup(scoped, WIDGET_DOMAIN).orElseThrow() == 20);
    }
    System.out.println("Scoped custom IDs and four protocols passed");
  }
}
