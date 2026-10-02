# Small schema examples

[Back to the project overview](../README.md)

Use these declarations as building blocks. Each code block below is a complete
schema; save it in its own `.serializer` file and generate code with:

```sh
serializer --input example.serializer --output example.hpp
```

For complete applications and expected output, use the
[runnable examples](../example/README.md). For syntax details, see the
[schema reference](schema_reference.md).

## Simple class

Declare an access level for each field. Fields end with semicolons; class and
enum declarations do not.

```text
serializer version 1;

namespace demo {
  class person {
    public string name;
    public uint64 id;
  }
}
```

The default C++ output is an owning class. Its fields store their own values,
and generated methods read and write them through the chosen protocol.

## Array

Use `array Type` for a sequence. The default C++ owning representation uses
`std::vector`.

```text
serializer version 1;

namespace demo {
  class person {
    public string name;
    public uint64 id;
  }

  class person_list {
    public uint64 list_id;
    public array person people;
  }
}
```

## Map

Use `map(KeyType) ValueType` for a keyed collection. This example maps numeric
IDs to people; the default C++ owning representation uses `std::map`.

```text
serializer version 1;

namespace demo {
  class person {
    public string name;
  }

  class directory {
    public map(uint64) person people;
  }
}
```

## Enum

An enum declares a set of named values. JSON and string-key binary encode the
value name; the other native binary modes encode its numeric ordinal.

```text
serializer version 1;

namespace demo {
  enum status {
    pending,
    active,
    closed
  }

  class account {
    public status state;
  }
}
```

## Field names and IDs

Metadata after a field name can set its wire name, numeric ID, or both. These
identifiers are independent of generated language naming profiles.

```text
serializer version 1;

namespace demo {
  class person stable_ids {
    public string name ("Name", 3);
    public uint64 id ("id", 4);
  }

  class employee stable_ids : public person ("Person", 5) {
    public uint64 employee_id ("employeeId", 6);
  }
}
```

`stable_ids` requires explicit IDs for every field and parent. Read the
[stable ID contract](schema_reference.md#explicit-field-ids-with-stable_ids)
before evolving a stored or shared schema.

## Default values

Place a default value in braces. Quoted strings preserve spaces without escaping
them. Field metadata can appear before the default.

```text
serializer version 1;

namespace demo {
  class person {
    public string name ("Name", 3) { "New person" };
    public uint64 id ("id", 4) { 4 };
  }
}
```

For arrays, maps, unions, includes, and decoding behavior in working applications,
continue with the [usage guide](usage.md) or [language examples](../example/README.md).
