# Serializer

C++ Serializer that utilized C++ language construct so that it can create simple class and template serializerm with limited data types supported.
This will be extended to other languages in future.

## Language Construct
### Namespace
This directly maps to C++ name space this can be hierarchical. This can be similar to C++ syntax like "A::B::C".
```
namespace A { namepace B { } }
```
is equivalent to
```
namespace A::B { }
```

### Struct
By default all the members are public.

Syntax:
```
struct <name> packed : <public|private|protected> <parent> {
<public|private|protected> [array|map] <type> <variable>;
};
```

### Datatypes
|Type|C++ Type|Size byte|Common name|
|---|---|---|---|
|char|char|1|Character|
|int8|int8_t|1|integer|
|int16|int16_t|2|integer|
|int32|int32_t|4|integer|
|int64|int64_t|8|integer|
|uint8|uint8_t|1|unsigned integer|
|uint16|uint16_t|2|unsigned integer|
|uint32|uint32_t|4|unsigned integer|
|uint64|uint64_t|8|unsigned integer|
|float|float|4|Floating point|
|double|double|8|Floating point|
|bool|bool|1|Bool|
|string|std::string|variable|String|

## Collection types
Following collecting types are supported
1. Array
1. Map

## Comments
"//" till new line and anything under "/*" and "*/" will be ignore.

## Serializer Type
Output is template based, hence one of following serializer can be used:
1. JSON
1. Binary
	1. Positional Binary
	1. ID based indexing
	1. String based indexing

if ```cpp test::person pr``` is name of your class different serializer can be applied as follows:

JSON Serializer support.
```cpp
pr.SerializeOut<rohit::serializer::json>(stream);
pr.SerializeIn<rohit::serializer::json>(stream);
```

JSON Output Serializer support with beautification
```cpp
rohit::serializer::JsonOut<true> jsonOut { fullstream1, rohit::serializer::format::beautify };
pr.SerializeOut(jsonOut);
```

There are three predefined format
1. ```cpp rohit::serializer::format::compress ```
1. ```cpp rohit::serializer::format::beautify ```
1. ```cpp rohit::serializer::format::beautify_vertical ```

More can be generated using structure ```cpp rohit::serializer::write_format ```

Positional binary, there will be no indexing either by ID or name.
```cpp
pr.SerializeOut<rohit::serializer::binary_none>(stream);
pr.SerializeIn<rohit::serializer::binary_none>(stream);
```

Binary serialization with Index by ID, currently ID is positional it will allowed to set in future.
```cpp
pr.SerializeOut<rohit::serializer::binary_integer>(stream);
pr.SerializeIn<rohit::serializer::binary_integer>(stream);
```

Binary serialization with String ID. Currently string ID is name of member variable, it will be allowed to be custom in future.
```cpp
pr.SerializeOut<rohit::serializer::binary_string>(stream);
pr.SerializeIn<rohit::serializer::binary_string>(stream);
```

## Example
### Simple class
Below input:
```cpp
namespace test {
class person {
    public string name;
    public uint64 ID;
}
}
```
This will result in:
```cpp
/////////////////////////////////////////////////////////
// This is auto genarated file using serializer. Must  //
// not be manually edited. For more information refer  //
// to https://github.com/rohit-singh-gautam/Serializer //
/////////////////////////////////////////////////////////
#pragma once
#include <rohit/serializer.h>

namespace test {
class person {
public:
	std::string name { };
	uint64_t ID { };

	template <typename SerializeOutProtocol>
	void SerializeOut(SerializeOutProtocol &serializerProtocol) const;
	template <template<rohit::serializer::SerializeType> class SerializerProtocol>
	void SerializeOut(rohit::Stream &stream) const;
	template <typename SerializeInProtocol>
	void SerializeIn(SerializeInProtocol &serializerProtocol);
	template <template<rohit::serializer::SerializeType> class SerializerProtocol>
	void SerializeIn(const rohit::Stream &stream);
}; // class person
}
```

### Array
```cpp
namespace arraytest {
class person {
    public string name;
    public uint64 ID;
}

class personlist {
    public uint64 listid;
    public array person list;
}
}
```

Above input will generate:
```cpp
/////////////////////////////////////////////////////////
// This is auto genarated file using serializer. Must  //
// not be manually edited. For more information refer  //
// to https://github.com/rohit-singh-gautam/Serializer //
/////////////////////////////////////////////////////////
#pragma once
#include <rohit/serializer.h>

namespace arraytest {
class person {
public:
	std::string name { };
	uint64_t ID { };

	template <typename SerializeOutProtocol>
	void SerializeOut(SerializeOutProtocol &serializerProtocol) const;
	template <template<rohit::serializer::SerializeType> class SerializerProtocol>
	void SerializeOut(rohit::Stream &stream) const;
	template <typename SerializeInProtocol>
	void SerializeIn(SerializeInProtocol &serializerProtocol);
	template <template<rohit::serializer::SerializeType> class SerializerProtocol>
	void SerializeIn(const rohit::Stream &stream);
}; // class person

class personlist {
public:
	uint64_t listid { };
	std::vector<person> list { };

	template <typename SerializeOutProtocol>
	void SerializeOut(SerializeOutProtocol &serializerProtocol) const;
	template <template<rohit::serializer::SerializeType> class SerializerProtocol>
	void SerializeOut(rohit::Stream &stream) const;
	template <typename SerializeInProtocol>
	void SerializeIn(SerializeInProtocol &serializerProtocol);
	template <template<rohit::serializer::SerializeType> class SerializerProtocol>
	void SerializeIn(const rohit::Stream &stream);
}; // class personlist

} // namespace arraytest
```

### Map
```cpp
namespace maptest {
class person {
    public string name;
    public uint64 ID;
}

class personlist {
    public uint64 listid;
    public map(uint64) person list;
}
}
```
Above code will result in below C++ code
```cpp
/////////////////////////////////////////////////////////
// This is auto genarated file using serializer. Must  //
// not be manually edited. For more information refer  //
// to https://github.com/rohit-singh-gautam/Serializer //
/////////////////////////////////////////////////////////
#pragma once
#include <rohit/serializer.h>

namespace maptest {
class person {
public:
	std::string name { };
	uint64_t ID { };

	template <typename SerializeOutProtocol>
	void SerializeOut(SerializeOutProtocol &serializerProtocol) const;
	template <template<rohit::serializer::SerializeType> class SerializerProtocol>
	void SerializeOut(rohit::Stream &stream) const;
	template <typename SerializeInProtocol>
	void SerializeIn(SerializeInProtocol &serializerProtocol);
	template <template<rohit::serializer::SerializeType> class SerializerProtocol>
	void SerializeIn(const rohit::Stream &stream);
}; // class person

class personlist {
public:
	uint64_t listid { };
	std::map<uint64_t,person> list { };

	template <typename SerializeOutProtocol>
	void SerializeOut(SerializeOutProtocol &serializerProtocol) const;
	template <template<rohit::serializer::SerializeType> class SerializerProtocol>
	void SerializeOut(rohit::Stream &stream) const;
	template <typename SerializeInProtocol>
	void SerializeIn(SerializeInProtocol &serializerProtocol);
	template <template<rohit::serializer::SerializeType> class SerializerProtocol>
	void SerializeIn(const rohit::Stream &stream);
}; // class personlist

} // namespace maptest
```

### Enum
This is a specialize case where in string and JSON mode value name will be serialized in other binary mode its positional ID will be serialized.

Example of CPP:
```cpp
namespace enumtest {
enum testenum {
    test1,
    test2,
    test3,
    test4,
    test5,
    test6
}

class test {
    public testenum te;
}
}
```

### Member Modifier
Member can have custom numeric ID for indexing with integer or custom string for indexing with string. This can be done by adding number or a string in double quote inside a round bracket after definition of member variable.

Example:
```cpp
namespace test {
class person {
    public string name ("Name", 3);
    public uint64 ID ("id", 4);
}
}
```

Similar parameter can also be added for parent class definition example:
```cpp
namespace test {
class personex : public person ("Person", 5) {
    public uint64 ID ("id", 6);
}
}
```

### Default value
Default value can be added for member variable by adding a value in braces after definition of member variable example:
```cpp
namespace test {
class person {
    public string name {"None"}("Name", 3);
    public uint64 ID ("id", 4) { 4 };
}
}
```

## Roadmap
1. Check for validity for default value.
1. Store position of member variable in input stream.
1. Bit field.
