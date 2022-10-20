
# General Structure:

Code is organized in `module`s. A file corresponds to a module, and
contains a list of:

* `import` statements for other modules;
* function definitions
* assignments of parameters of constants
* schema definitions


## Basics:

* The language is case sensitive sensitive.
* It is strongly type, but types can also be infered.
* The spaces outside strings do not matter.
* Semicolons are not normally necessary, unless to resolve parsing
conflicts. They are recommended for spliting expressions in code blocks.
* Code blocks are enclosed in braces `{` and `}`.


### Identifiers

These are names of variables, functions, types, modules etc. and have the
familiar form of other languages: start with a letter or `_`, continue w/
letters, `_` or digits.

A composed identifier is created by concatenating several identifiers with
dots (`.`). E.g.: `foo.bar.baz`.

### Literas

These constructs represent basic types in the code:

* integers base ten: `123`, `43453`. Beside `0`, they do not start with `0`.
* integers base sixteen: start with prefix `0x` / `0X`: `0xab34`, `0XC3`.
* floating point numbers: `123.33`, `0.123` or `.123`, `1.23e+3`
* strings are double quoted, with usual escape chars: `"foo bar"`,
`"foo\nbar"`, `"foo\u2665"`. All strings are converted to UTF-8.
Use `\uXXXX` for a 32bit unicode value, and `\UXXXXXXXX` for a 64bit one.
* byte strings start with `b` and can contain the `\xXX` for hexadecimal
values, beside the usual  E.g. `b"ab\xf7\xd3"`
* boolean values are expressed by keywords: `false` or `true`.
* null values are expressed with `null` keyword.

### Comments

The comments are C++ style: anything between  `/*` and `*/` is a comment,
or everithing from `//` to the end of the line:

```
/* Some nice comment here */
a = 10  // some other comment
```

### Type Names

They are expressed using composed identifiers, with type parameters specified
inside angle brackets `<` and `>`. For example: `int`, `string`, `Array<int>`,
`Decimal<10, 2>`, `Map<int, Array<string>>`. Pondering if we should allow
lengs specification for types e.g. `int<16>` for a 16 bit integer etc.

### Imports

The name of the file + directory structure determines the module name (a'la
python). E.g. file `foo/bar/baz.nuc` coresponds to module `foo.bar.baz`,
and can be imported with `import foo.bar.baz`. Names defined in it can be
accessed as `foo.bar.baz.<name>`.

### Assignments

Variables, constants and parameters are instantiated using the `=` operator.
Some decorations can be added to specify the type.

```
foo = 10   // foo is variable of implicit type int, initialized w/ 10
bar : int<16> = 22  // bar is a variable of 16 bit integer, initialized w/ 22
baz : Decimal<10, 2> = 1.34 // baz is a decimal w/ scale 10 and precision 2
```

### Array and Map initializers

The elements of maps and array have to be enumerated inside `[` and `]`
brackets.

```
a = [1, 2, 3]  // An array of integers.
b : Array<int16> = [1, 2, 3]  // An array of 16 bit integers.
c : Set<int> = [1, 2, 3]   // A set of integers.
m = [1: "foo", 2: "bar"]  // A map of integers to strings.
```

### Expressions

These are constucts that return a value. Most of the constructs in the
DSL return a value. For exampe `1`, `x`, `x + 1`, `y = x + 1`,
`z = y = x + 1`, or `if (x > 0) 1 elif (x < 0) -1 else 0`. Expressions
can be combined in blocks, by concatenating them with `;`, and wrapping
inside braces`{`, `}`for example: `{ y = x + 1; z = 3 }`. The return value
of an expression block is the return value of the last expression. In our
example would be the return value of `z = 3`, which in turn would be `z`
or `3`.

### Operators

Operatios can be used to perform, well, operations on their operands.
The operands are expressions. The values produced by operators sometimes
depend on the operands, and sometimes depend on the operators themselves.
Here are the supported operators in their precedence order:

* unary:
  * `+`, `-` - applicable to numeric values - sign operators
  * `~` - applicable to integer numeric values - binary negation.
  * `not` - applicable to boolean - logic negation.

* multiplicative:
  * `*` - multiplication - applicable to numeric values, returns the
  type that produces the widest range.
  * `/` - division - applicable to numeric values, same as `*`. Dividing
  by `0` outcome depends on the underlying implementation language.
  * `%` - modulo - applicable to integer numeric values.

* additive:
  * `+` - addition - applicable to numeric values, string, bytes, others (tbd).
  Adds two values together. Return value depends on operands.
  * `-` - subtraction - applicable to numeric values. Returns signed
  numeric value.

* bit shift:
  * `>>` and `<<` - applicable to integer numeric values.

* relational
  * `<`, `<=`, `>=`, `>` - less than, less or equal, greater or equal,
  greater than, applicabale to numeric values, string, bytes,
  others (tbd). Return value is boolean.

* equality
  * `==` and `!=` - equals and not equals. Applicable to a lot of
  types (tbd - structures ? arrays ? etc?). Returns boolean.

* bit-wise AND
  * `&` - applicable to integer numeric values. Returns integer

* bit-wise XOR
  * `^` - applicable to integer numeric values. Returns integer

* bit-wise OR
  * `|` - applicable to integer numeric values. Returns integer

* between
  * `between` ternary operator. Checks if the first operand lies
  between the second and third. Applies to the same types on which relational
  operators apply. Returns boolean. Usage: `x between (10, 20)`.

* in
  * `in` - determines if the value of first operand is included
  in the set determined by the second. The second operand needs
  to be an array, map or set. Applies to the same types as equality.

* logical AND
  * `and` - applicable to boolean values. Returns boolean

* logical XOR
  * `or` - applicable to boolean values. Returns boolean

* logical OR
  * `or` - applicable to boolean values. Returns boolean

* conditional
  * `?` ternary operator. Returns a boolean. The first operand
  is a boolean expression, while second and third are values
  return in the case the first operand is `true` or `false`, respectively.
  Example: `a > b ? (a + 1, b + 2)`.

Parenthesis can be used to group expressions together in order to
override operator precedence: E.g. `a + b * c` performs the multiplication
of `b` and `c` before adding `a`, but `(a + b) * c` performs the addition
of `a` and `b` before multiplying the result with `c`.

## Function Definitions

Functions have a name that is an identifier. They can have a list of
parameters, that have names, and optionally types and default values.
Functions can be specified to have a return type, else the type
is deduced. Functions are defined using keyword `def`, at the top level
of a module.

The general form is:

```
def <function_name> ( [<parameters>] ) [: <return type> ] => <expression>
<parametes> := <parameter> (, <parameter>) *
<parameter> := <name> [: <typename>] [= <default_value>]
```

For example:
```
def f(x) => x + 1
// Same thing, but we enforce a int return value:
def f(x) : int => { x + 1 }
// Same thing, but we specify a type and default value for x:
def f(x: int = 0) => { x + 1 }
```

Inside function blocks, one can specify a `return` statement to
quickly end the expression and return a value. Note that the return
values need to be all the same type on all the paths, or convertible.

```
def f(x) {
  if (x < 0) {
    return 0
  }
  z = x * 2;
  z + 4  // return value if x >= 0. Can also be written as return z + 4
}
```

## Function Calls

The call of functions is done by specifying the name of the function,
and parameters between parenthesis `(` and `)`. The parameters can be
specified by name as well. For example:

```
// Simple definition with default values:
def f(x = 0, y = 0) => x - y
// Calling:
f()   // returns 0
f(1)  // return 1 - 0 => 1
f(y = 1)  // return 0 - 1 => -1
f(x = 3, y = 1)  // return 3 - 1 => 2
// f(x = 1, x = 2)  // this generates an error
```

## Lambda Expressions

Functions are just another type, and one can create an inline function
using a lambda expression.
It has the form `<parameters> => <expression/block>`.
Where `<parameters>` can be just a identifier or a list of identifiers
inside parenthesis. Lambda expressions do not support specifying a
return type or parameter types or their default values. For example:

```
f = x => x + 1;
g = (x, y) => { x + y }
// Used as arguments:
filtered_source = source.filter(p => p.x > 10 or p.y < 20)
```

Expression inside a lambda expression can acces and use variables defined in
the scope they are used. E.g.

```
// Computes if the sum of some numbers in a set is greater than twice the sum
// of even numbers in the set.
def fun(numbers: Array[int]) => bool:
    full_sum = 10;
    evens_sum = numbers.filter(x: full_sum += full_sum; x % 2 == 0).sum();
    full_sum > evens_sum * 2
}
```

## Types

The exact nature of this is to be decided, but here is a proposed list:

  * `int` by default a 64 bit integer, but can be 'templetized' with
  the witdth in bits: `int<8>`, `int<16>`, `int<32>`. Thinking of
  actually naming them `int8`, `int16`, `int32`
  * `uint` a 64 bit unsigned integer. Same as `int` can be templetized
  with the width.
  * `float` a 64 bit double precision float. Same thinking of `float<32>`
  * `bool` a boolean
  * `string`
  * `bytes`
  * `Date` a date
  * `DateTime` - a date and a time in a time zone.
  There is a thinking here going on: we should only have a `Timestamp`
  that is in UTC, that is the only time stored, then from here have `DateTime`
  for a specific timezone.
  * `Decimal` with scale and precision, templetized as
  `Decimal<scale, precision>`
  * `Array` - a templetized type representing an array e.g. `Array<int>`
  * `Set` - a templetized type representing a set e.g. `Set<string>`
  * `Map` - a templetized type representing a map e.t. `Map<int, string>`
  * `Null` - the null type.
  * `Nullable` - a templetized type that basically specifies that
  `Null` or template type. E.g. `Nullable<string>` represents `Null` or `string`
  * `Function` - templetized type that represents a function. The
  last argument is the return type. E.g. `Function<int, string, float>` is
  a function w/ two argument `int` and `string`, and returns a `float`.

Operators and functions would convert automatically from a type to another.
Some are not possible. E.g. `int` to `bool` would be if it is 0 or not, etc.
Some would not be possible, especially for some templated types,  e.g.
`Nullable<int>` would not convert to `int`, users would need to specify
how to convert.

Note that because this is statically typed, a variable cannot change its type.
E.g.

```
x : int = 20;
x = [1,2,3]  // error: requires Array<int> - cannot be converted to int.
```

## Object functions

A lot of types have 'member functions', which can be invoked via the
dot `.` operator. E.g.

```
d = Date(2022, 8, 28);
day = d.weekday()
x = [1,2,3,4]
len = x.len()
```

## Schemas

One can define a structure schema using a schema construct:

```
schema <identifier> := {
   <field_definition> ( , <field_definition)*
}
<field_defintion> := <field_name> : <type> [field_options] ?
<field_name> := identifier
<field_options> := '[' <field_option> (, [field_option]*) ']'
<field_option> := identifier = <expression>
```

For example:
```
schema Member = {
  member_id : int [ is_id = true ],
  name : Nullable<string> = null,
  join_date : Date
}
```

Field options can be from a well determined set, and their values
depend on what they represent. They are direct descendents of the schema
annotations that we currently have.

Once a schema is created, it serves as a structure definition as well.
E.g. one can do

```
member = Member(
   member_id = 20,
   name = 'foo',
   join_date = Date(2022, 1, 22)
)
```

Fields in a structure can be refered with `.`: e.g. `member.member_id`.

## Data Structures

Array members can be referd with `array[<index>]`.
Map values with `map[<key>]`.
Elements in a map are basically structures with `key` and `value` fields.

We have functions to iterate on these: `map`, `filter`, `reduce`, `aggregate`.

For now, we do not plan to have something similar with `for`.


## Datasources

To manipulate data, one needs to define data sources. They can come as
basic data sources, like tables, or derived data sources, through various
operations.

The basic source is a table, which can be specified with a schema, and maybe
a parametrized source:

```
members = Table(schema.Members)
param claims_source: string = ''
claims = Table(schema.Claims, claims_source)
```

Operations can be defined on data sources to create other sources.
These operations usually accept a function as an argument:


```
filtered_claims = claims.filter(c => c.date.month() == current_month);
members_with_claims = member.join(
    m => [
        'claims': filtered_claims.join_by(c => c.member_id == m.member_id),
        'enrolements': enrolement.join_by(e => e.member_id == m.member_id)
    ]
)
```

Here are some operations on datasources:
* `map` - transforms each element in a datasource to something else

```
member.map(m => foo(
   member_id = m.member_id,
   amount_cents = m.amount * 100
))
```

* `filter` - filters the datasource by a boolean expression

```
claims.filter(c => c.date.month() == current_month);
```

* `aggregate` - aggregates using an aggregation specification:

```
claims.aggregate(
    c => group_by(month = c.date.month())
            .sum(total_amount = c.amount)
            .max(max_amount = c.amount)
)
```

* `join` - joins the multiple datasources (see example above)

* `limit` - limits the number of elements in a datasource: `members.limit(10)`

* `sort_asc` / `sort_desc` - sorts the datasource by an expression:
`members.sort_asc(m => m.birth_date)`;


The datasources are just descriptions. To execute something, one needs
to either `.collect(..)` or `.write(<destination>)`.

E.g. may have in a module named `foo.bar.compute`:

```
computed_members = member
    .filter(m => m.gender = 'F')
    .join(m => ['claims': claims
                             .filter(c => c.amount > 1000)
                             .join_by(c => c.member_id == m.member_id)])
    .filter(m => len(m.claims) > 10)
```

Then, in another place may use:

```
import foo.bar.compute

computed_members.write('s3://foo/bar/baz.parquet')

top_members = computed_members.limit(100).collect()
// top_members is an Array type with inferred type.
```
