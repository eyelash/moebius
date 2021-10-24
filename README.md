# Moebius

a simple and fast functional programming language

## Selling Points

- simplicity
- performance
- safety
- a fast compiler

## Instructions

```bash
# clone the repo
git clone https://github.com/eyelash/moebius
# compile the compiler
cmake .
make
# compile some moebius code
./compiler examples/HelloWorld.moeb
# run the compiled code
examples/HelloWorld.moeb.exe
```

## Roadmap

- codegen
  - [ ] x86
  - [x] C
  - [x] JavaScript
- optimizations
  - [x] monomorphization
  - [x] dead code elimination
  - [x] inlining
  - [x] constant propagation
  - [x] tail call optimization
  - [ ] common subexpression elimination
  - [ ] graph coloring register allocation
- types
  - [x] integers
  - [x] closures
  - [x] structs
  - [x] arrays
  - [x] strings
  - [ ] sum types
  - [ ] interfaces
  - [ ] floating point
- operators
  - [x] arithmetic (`+`, `-`, `*`, `/`, and `%`)
  - [x] relational (`==`, `!=`, `<`, `<=`, `>`, and `>=`)
  - [ ] logical (`&&`, `||`, and `!`)
- [x] compile-time garbage collection
- [x] compile-time reflection
- [ ] testing
- [ ] monadic IO
