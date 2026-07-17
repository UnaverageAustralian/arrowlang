# Arrowlang syntax

## Pushing

Literals are immediately pushed onto the stack:
```
 2 3
 // STACK: [ 2 | 3 ]
```

## Arithmetic

Arrowlang's arithmetic operators are: `+` (add), `-` (subtract), `*` (multiply), `/` (divide), and `%`/`mod` (modulo).

Operators are written in reverse Polish notation, like all other stack languages:
```
 2 3 + 5 *
 // STACK: [ 2 | 3 ] -> [ 5 ] -> [ 5 | 5 ] -> [ 25 ]
```

## Bit operations

Arrowlang's bit operators are: `&`/`and`, `|`/`or`, `^`/`xor`, `<<`/`shl`, `>>`/`shr`,
`<<<`/`rol`, `>>>`/`ror`, and `~`/`not`.

```
 10 9 &  // 8
 10 9 |  // 11
 10 9 ^  // 3
 10 1 >> // 5
 10 1 << // 20
 56 ~    // -57
```

## Stack operations

`dup` duplicates the top of the stack.

`over` pushes the second-to-top element onto the stack.

`dup2` duplicates the top two elements.

`.`/`drop` drops the top of the stack.

`\`/`swap` swaps the top two elements.

`over2` pushes the second-to-top pair of elements onto the stack.

`swap2` swaps the top two pairs of elements.

`rot` moves the third-to-top element to the front of the stack.

```
 2 3 dup       // STACK: [ 2 | 3 ] -> [ 2 | 3 | 3 ]
 2 3 over      // STACK: [ 2 | 3 ] -> [ 2 | 3 | 2 ]
 2 3 dup2      // STACK: [ 2 | 3 ] -> [ 2 | 3 | 2 | 3 ]
 2 3 drop      // STACK: [ 2 | 3 ] -> [ 2 ]
 2 3 swap      // STACK: [ 2 | 3 ] -> [ 3 | 2 ]
 2 3 9 8 over2 // STACK: [ 2 | 3 | 9 | 8 ] -> [ 2 | 3 | 9 | 8 | 2 | 3 ]
 2 3 9 8 swap2 // STACK: [ 2 | 3 | 9 | 8 ] -> [ 9 | 8 | 2 | 3 ]
 2 3 9 rot     // STACK: [ 2 | 3 | 9 ] -> [ 3 | 9 | 2 ]
```

## Other operations

There is also a `neg` operation, which negates the top value on the stack.

## Conditionals

If statements are declared with either the word `if` or the symbol `?`, and are ended with the word `end` or the symbol `;`. Else is either the word `else` or the symbol `:`.

Arrowlang's comparison operators are: `<`/`lt`, `<=`/`lteq`, `>`/`gt`, `>=`/`gteq`, `=`/`eq`, and `!` (logical not).

```
 2 3 < ?
     1
 :
     0
 ;
```

## Loops

Loops are declared with either the symbol `{` or the word `loop`, and end with either the symbol `}` or the word `end`.

```
 loop end // Infinite loop
```

While loops are the same, except with the word `while` followed by a condition.

```
 // Iterates from 0 to 10
 0 while dup 10 < loop
     1+ // By the way this is two operations and is the same as '1 +'
 end
```

To break out of a loop use `break`, and to immediately go to the next iteration, use `continue`.

```
 // Same thing but without while
 0 loop
     dup 10 >= if break ;
     1+
 end
```

## Functions

Functions are declared with the symbol `$` and end with the same symbol. And they must have declared stack effects as well.

```
 $floor10 ( i64 -> i64 )
     dup 10 mod -
 $
```

You can return early from a function with `return`.

```
 $foo ( i64 -> i64 )
     5 + return 2
 $
```

## Modules

Modules are imported using `import`. You can either import standard library modules by putting a word after it (`import io`), or a local module by putting a string after it (`import "foo"`).

The standard library modules are:
`io` - For input-output functions (but no input yet).

Here is an example using the `io` module:
```
 import io

 $main ( -> u8 )
    71 io::printc // Prints the character G to the io buffer
    io::flush     // Flushes the io buffer
 $
```

## Types

Arrowlang's types are: `i8`, `char`, `u8`, `i16`, `u16`, `i32`, `u32`, `i64`, `u64`, `int_lit`, `f32`, `f64`, `real_lit`, and `str`.
Most types are only compatible with themselves, except for `int_lit` and `real_lit`, which are compatible with all integer types (including `char`) and all float types respectively.

You can convert between types just by putting in the type's name:

```
 30 f32                  // Converts the 30 from int_lit to f32
 255u8 1 + i64 io::print // Prints 0
```

## Structs

Structs are declared with `struct` followed by a name, and end with `end`. Structs contain fields which can be accessed with `#` followed by the field's name (this operation does not drop the struct), and can be initialised with values by putting in their name:
```
 struct Foo
     x : i32
     y : i32
 end

 $main ( -> u8 )
     2 3 Foo
     #x // 2
 $
```

You can also store values into fields with `>#` followed by the field's name (this operation also does not drop the struct):
```
 2 3 Foo
 4 >#x
 #x // 4
```

There is also a variant of the `#` operation which does drop the struct, the `.` operation:
```
 struct Foo
     x : i32
     y : i32
 end

 struct Bar
     foo : Foo
 end

 $main ( -> u8 )
     2 3 Foo Bar
     #foo.x      // 2
     drop #foo.y // 3
 $
```

You can also declare anonymous structs:
```
 2 3 struct
     x : i32
     y : i32
 end
 4 >#x
 #x // 4
```
