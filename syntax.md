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

```
 2 3 dup       // STACK: [ 2 | 3 ] -> [ 2 | 3 | 3 ]
 2 3 over      // STACK: [ 2 | 3 ] -> [ 2 | 3 | 2 ]
 2 3 dup2      // STACK: [ 2 | 3 ] -> [ 2 | 3 | 2 | 3 ]
 2 3 drop      // STACK: [ 2 | 3 ] -> [ 2 ]
 2 3 swap      // STACK: [ 2 | 3 ] -> [ 3 | 2 ]
 2 3 9 8 over2 // STACK: [ 2 | 3 | 9 | 8 ] -> [ 2 | 3 | 9 | 8 | 2 | 3 ]
 2 3 9 8 swap2 // STACK: [ 2 | 3 | 9 | 8 ] -> [ 9 | 8 | 2 | 3 ]
```

## Other operations

There is also a `neg` operation, which negates the top value on the stack.

## Conditionals

If statements are declared with either the word `if`, or the symbol `?`, and are ended with the symbol `;`. Else is either the word `else`, or the symbol `:`.

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

To break out of a loop use `brk`, and to immediately go to the next iteration, use `continue`.

```
 // Same thing but without while
 0 loop
     dup 10 >= if brk ;
     1+
 end
```

