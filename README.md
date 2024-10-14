# Serializer

C++ Serializer that utilized C++ language construct so that it can create simple class and template serializer.

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
<public|private|protected> <type> <variable>;
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