# mlgmpidl Agent Guide

## Build/Test Commands
- `make` - builds the library (both byte and opt)
- `make byte` - builds bytecode version
- `make opt` - builds native code version
- `make session.byte` - builds bytecode test session
- `make session.opt` - builds native test session
- `make test` - builds and runs serialization tests
- `make test-build` - builds all test executables
- `make test-simple` - runs simple serialization test
- `make test-debug` - runs debug serialization test
- `make clean` - removes build artifacts (including tests)
- `make install` - installs the library
- `make html` - generates HTML documentation
- `make doc` - generates documentation

## Architecture
- OCaml bindings for GMP and MPFR (arbitrary precision arithmetic)
- 5 main modules: Mpz (integers), Mpzf (functional integers), Mpq (rationals), Mpqf (functional rationals), Mpf (floats), Mpfr (extended floats), Gmp_random
- IDL files (*.idl) define C bindings, processed by CamlIDL
- Imperative interface (in-place operations) vs functional interface (f suffix modules)
- Uses CamlIDL for C interface generation

## Code Style
- OCaml syntax with C FFI bindings
- Imperative modules modify first parameter (out-parameter)
- Functional modules create new values
- Module naming: Mpz/Mpzf, Mpq/Mpqf, etc.
- No specific test framework - manual testing via session.ml
