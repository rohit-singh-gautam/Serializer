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
```cpp
pr.serialize_out<rohit::serializer::json>(stream);
pr.serialize_in<rohit::serializer::json>(stream);
pr.serialize_out<rohit::serializer::binary<rohit::serializer::SerializeKeyType::None>>(stream);
pr.serialize_in<rohit::serializer::binary<rohit::serializer::SerializeKeyType::None>>(stream);
pr.serialize_out<rohit::serializer::binary<rohit::serializer::SerializeKeyType::Integer>>(stream);
pr.serialize_in<rohit::serializer::binary<rohit::serializer::SerializeKeyType::Integer>>(stream);
pr.serialize_out<rohit::serializer::binary<rohit::serializer::SerializeKeyType::String>>(stream);
pr.serialize_in<rohit::serializer::binary<rohit::serializer::SerializeKeyType::String>>(stream);
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

	template <typename SerializerProtocol>
	void serialize_out(rohit::Stream &stream) const {
		SerializerProtocol::struct_serialize_out(
			stream,
			std::make_pair(std::string_view { "name" }, name),
			std::make_pair(std::string_view { "ID" }, ID)
		);
	}

	template <typename SerializerProtocol>
	void serialize_in(const rohit::FullStream &stream) {
		SerializerProtocol::struct_serialize_in(
			stream,
			std::pair<std::string_view, std::function<void(const rohit::FullStream &)>> { std::string_view {"name"}, [this] (const rohit::FullStream &stream) { SerializerProtocol::template serialize_in<std::string>(stream, this->name); }},
			std::pair<std::string_view, std::function<void(const rohit::FullStream &)>> { std::string_view {"ID"}, [this] (const rohit::FullStream &stream) { SerializerProtocol::template serialize_in<uint64_t>(stream, this->ID); }}
		);
	}
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

	template <typename SerializerProtocol>
	void serialize_out(rohit::Stream &stream) const {
		SerializerProtocol::struct_serialize_out(
			stream,
			std::make_pair(std::string_view { "name" }, name),
			std::make_pair(std::string_view { "ID" }, ID)
		);}

	template <typename SerializerProtocol>
	void serialize_in(const rohit::FullStream &stream) {
		SerializerProtocol::struct_serialize_in(
			stream,
			std::pair<std::string_view, std::function<void(const rohit::FullStream &)>> { std::string_view {"name"}, [this] (const rohit::FullStream &stream) { SerializerProtocol::template serialize_in<std::string>(stream, this->name); }},
			std::pair<std::string_view, std::function<void(const rohit::FullStream &)>> { std::string_view {"ID"}, [this] (const rohit::FullStream &stream) { SerializerProtocol::template serialize_in<uint64_t>(stream, this->ID); }}
		);
	}
}; // class person

class personlist {
public:
	uint64_t listid { };
	std::vector<person> list { };

	template <typename SerializerProtocol>
	void serialize_out(rohit::Stream &stream) const {
		SerializerProtocol::struct_serialize_out(
			stream,
			std::make_pair(std::string_view { "listid" }, listid),
			std::make_pair(std::string_view { "list" }, list)
		);}

	template <typename SerializerProtocol>
	void serialize_in(const rohit::FullStream &stream) {
		SerializerProtocol::struct_serialize_in(
			stream,
			std::pair<std::string_view, std::function<void(const rohit::FullStream &)>> { std::string_view {"listid"}, [this] (const rohit::FullStream &stream) { SerializerProtocol::template serialize_in<uint64_t>(stream, this->listid); }},
			std::pair<std::string_view, std::function<void(const rohit::FullStream &)>> { std::string_view {"list"}, [this] (const rohit::FullStream &stream) { SerializerProtocol::template serialize_in<std::vector<person>>(stream, this->list); }}
		);
	}
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

	template <typename SerializerProtocol>
	void serialize_out(rohit::Stream &stream) const {
		SerializerProtocol::struct_serialize_out(
			stream,
			std::make_pair(std::string_view { "name" }, name),
			std::make_pair(std::string_view { "ID" }, ID)
		);}

	template <typename SerializerProtocol>
	void serialize_in(const rohit::FullStream &stream) {
		SerializerProtocol::struct_serialize_in(
			stream,
			std::pair<std::string_view, std::function<void(const rohit::FullStream &)>> { std::string_view {"name"}, [this] (const rohit::FullStream &stream) { SerializerProtocol::template serialize_in<std::string>(stream, this->name); }},
			std::pair<std::string_view, std::function<void(const rohit::FullStream &)>> { std::string_view {"ID"}, [this] (const rohit::FullStream &stream) { SerializerProtocol::template serialize_in<uint64_t>(stream, this->ID); }}
		);
	}
}; // class person

class personlist {
public:
	uint64_t listid { };
	std::map<uint64_t,person> list { };

	template <typename SerializerProtocol>
	void serialize_out(rohit::Stream &stream) const {
		SerializerProtocol::struct_serialize_out(
			stream,
			std::make_pair(std::string_view { "listid" }, listid),
			std::make_pair(std::string_view { "list" }, list)
		);}

	template <typename SerializerProtocol>
	void serialize_in(const rohit::FullStream &stream) {
		SerializerProtocol::struct_serialize_in(
			stream,
			std::pair<std::string_view, std::function<void(const rohit::FullStream &)>> { std::string_view {"listid"}, [this] (const rohit::FullStream &stream) { SerializerProtocol::template serialize_in<uint64_t>(stream, this->listid); }},
			std::pair<std::string_view, std::function<void(const rohit::FullStream &)>> { std::string_view {"list"}, [this] (const rohit::FullStream &stream) { SerializerProtocol::template serialize_in<std::map<uint64_t,person>>(stream, this->list); }}
		);
	}
}; // class personlist

} // namespace maptest
```
