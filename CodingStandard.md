# C++ Coding Standard

## Functions
Name must always start with a capital. For multiple word name first letter cap for each word. E.g. ToString
First letter of word preferred over use of underscore '_'.
Max Level 3, few exception case max level 5 tab allowed.
Functions must be broken to smaller one with single functionality.
Wherever possible use auto for return type as well as parameter.
Wherever it is logically correct to use constexpr use it.
All parameter must be const unless it has to be modified.
If there is require of C++ for certain function name it can start with small character and all function create in conjunction with it can begin in small character.

## Functional Programming
When class or structure is not required must not be used instead use namespace to contain functions with common functionality.

## Variables
All variable must start with small letter, there can be caps from second letter. Hungarian naming convention preferred.
Preferably class must start with very where possible.
Name must be descriptive so that comment is not required to explain it.

## Class
All class name must start with caps.
All members must be private unless it is require to be used outside class.
Class method must comply to guidance provided for functions.
Class must be used for single logical functionality.

## Enum
All enum must be enum class.
Enum member must be first letter capital

## Namespace
All namespace must be in small letter.