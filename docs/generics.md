# Schema generics

Serializer supports type parameters on owning schema classes. The compiler expands
each concrete application before running a language generator or compatibility
check. All eleven output languages use the same resolved field types and wire
contract. C++ additionally exposes template aliases for the applications present
in the schema.

```text
serializer version 1;

class result<T> stable_ids {
  public bool success (1);
  public T value (2);
  public string message (3);
}

class person stable_ids {
  public string name (1);
  public uint32 age (2);
}

instantiate person_result = result<person>;

class response stable_ids {
  public result<person> person_result (1);
  public result<uint32> count_result (2);
}
```

Class declarations have no trailing semicolon. An `instantiate` declaration has
the form `instantiate name = generic<arguments>;` and requires its semicolon.
It creates a named concrete class for a root that applications use directly.
The named class and the automatically generated specialization are distinct
types with identical fields and serialization behavior.

## Type arguments and resolution

Parameters may appear as direct field types, array element types, map keys or
values, and arguments of another generic. For example:

```text
class pair<Key, Value> {
  public Key key;
  public Value value;
}

class batch<T> {
  public array T values;
}

class lookup<Key, Value> {
  public map(Key) Value values;
}

instantiate pairs = batch<pair<uint32, string>>;
instantiate labels = lookup<uint32, string>;
```

Arguments are schema primitives, enums, owning classes, or other concrete generic
applications. Existing backend restrictions, including map-key support, still
apply after substitution. Collection modifiers are field syntax, not standalone
type arguments: wrap a collection in a generic class instead of `box<array uint32>`.

Definitions and argument types must precede their use, including across transitive
`include` directives. Parameters are local to their declaration. Other names in a
generic body resolve in the definition's namespace. Argument names resolve where
the application occurs. Whitespace and comments are permitted around `<`, `,`, and
`>`; adjacent closing brackets in nested applications work.

The compiler caches applications by the qualified declaration and resolved
arguments. Repeated references share one concrete class. Compiler-owned names
encode the canonical schema identity as hexadecimal after `serializer_instance_`;
language naming profiles may change their presentation. Use explicit named
instantiations for a readable, stable application-facing root name.

## Generated APIs

For the first example, C++ offers both the named `person_result` class and:

```cpp
result<person> value{};
value.success = true;
value.value.name = "Ada";
value.serialize_out<rohit::serializer::json>(output);
```

The alias resolves to exactly the concrete type used by `response.person_result`.
It has the normal generated codec methods and adds no runtime dispatch, allocation,
base class, or wire overhead. C++ naming profiles also apply to generic aliases.

Only applications present in fields or `instantiate` declarations receive bindings.
An unused generic emits no model; `result<some_other_type>` in application C++ is
unsupported unless that application is also declared in the schema. These are
finite schema bindings, not unrestricted C++ templates accepting arbitrary host
types. The compiler-generated `<name>_serializer_binding` helper name must not
collide with a schema declaration after applying the C++ naming profile.

Java, JavaScript, TypeScript, Go, C#, Rust, Python, Swift, Kotlin, and C receive
ordinary concrete models and their existing typed codec APIs. There is no new
runtime type registry or codec argument. Native generic APIs in those languages
remain future work.

## Wire compatibility

A generic application has the same representation as an equivalent ordinary
class. Parameters add no type tags, IDs, or envelope bytes. Field IDs, wire names,
order, defaults, and codec rules come from the declaration after substitution.
The reader must already know the concrete message type.

The compatibility checker compares expanded classes. Renaming a parameter does
not change that contract; changing an argument or a generic field can change it.
`stable_ids` retains its existing meaning and does not override positional field
order or native keyed readers' rejection of unknown fields. Reservations can
target a named instantiation's ordinary qualified class name. Shared generic
definitions must be evolved consistently for every application.

## Initial profile and limits

Generic declarations support unpacked, unmanaged owning classes without
inheritance or unions. Managed or view classes cannot be type arguments. Value
parameters, parameter defaults, specialization, variadic parameters, dependent
default expressions, and generic bases are not supported. Existing non-generic
owning/view APIs are unchanged.

Malformed or missing arguments, unknown types, duplicate/reserved parameters,
recursive generic ownership, generated-name collisions, and unsupported shapes
are rejected. Parsing and expansion each have a depth limit of 32. A compilation
permits at most 1,024 concrete applications and 4,096 bytes in a canonical
application identity. The normal runtime decoding limits remain independent.

Both editor extensions highlight generic syntax and navigate parameters, type
arguments, named roots, and generated concrete classes. A generic definition may
have several generated destinations; an unused definition falls back to its
schema location.

Run the [generic examples](../example/generics/README.md) for a C++ template-alias
executable and dedicated four-protocol consumers in `example/<language>/generics`.
Each consumer edits a typed nested payload; the runner checks that arrays and maps
retain their independent values. See the
[compiled fixture](../test/resources/generics.serializer),
[schema reference](schema_reference.md), and [usage guide](usage.md).

## Verification

The implementation was checked on Windows with the full 55-test CTest suite and
the generated C++ examples. The shared example runner compiled and exercised all
eleven language consumers through all four native protocols; Rust, Swift, and C
used explicitly selected WSL SDKs. The C++ unit suite also checks ordinary/generic
wire equivalence, invalid arguments, recursion and expansion budgets, namespace
binding, and compatibility changes.

Editor verification passed 68 shared/provider tests, 10,899 fresh-output navigation
checks (including all C++ naming profiles), and 64 checks in the Visual Studio
.NET interpreter, including generic definitions, local parameters, and reverse
navigation. Both 1.1.13 VSIX packages were rebuilt and validated. This change did
not install them or rerun the interactive editor-host tests.
