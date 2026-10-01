# AGENTS.md

Working notes for agents modifying this repository. For the design read
`ARCHITECTURE.md`; for usage read `README.md`. This file covers the repo
layout, how to build and test, the invariants you must not break, and the
traps that are easy to fall into.

## Repo map

```
varint.hh            The whole library: length/string/bool/char codecs.
test/test.cc          Runnable smoke test: exact byte vectors against a
                       captured reference, round-trips, cross-type binding,
                       truncated-input rejection.
examples/demo.cc       A runnable tour (not a test): encode/decode a small
                       multi-field message.
CMakeLists.txt         Header-only INTERFACE target `varint` (+ alias
                       varint::varint); no FetchContent, no external
                       dependency; CTest test `varint`.
LICENSE                MIT, Copyright (c) 2026 Germán Méndez Bravo.
README.md              What it is, install, usage, API, the format.
ARCHITECTURE.md        Why the format looks the way it does, the overlong-
                       input guard, why bool is ASCII not a raw byte.
```

Header-only. There is no `.cc` to compile; `varint.hh` is both the
declaration and the implementation.

## Build and run the test

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

No network is needed. Expected output ends with `all varint tests passed`,
exit 0. The test target is `varint_test`; the registered CTest name is
`varint`. The test and demo are only added when this repo is the top-level
project (`CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR`), so a
consumer vendoring it via `FetchContent` won't build them.

## Dependencies

None beyond the standard library: `<cstddef>`, `<string>`, `<string_view>`.
C++17 is the floor; nothing here needs C++20.

## Conventions

- **C++17**, not 20 -- simple enough it doesn't need anything newer, and a
  lower bar is easier for a consumer to clear.
- Tabs for indentation, double quotes in code, no em dashes in prose.
- MIT-licensed; keep the copyright header (Copyright (c) 2026 Germán Méndez
  Bravo) on source files.
- Every `unserialise_*` function returns `bool`, never throws. Don't add a
  throwing variant to this header; a caller that wants exceptions can wrap
  the bool-returning call itself.

## Load-bearing invariants

- **The wire format is frozen.** `serialise_length`'s output for any given
  input must match the byte vectors in `test/test.cc` exactly, forever --
  this is what lets `cluster` (and any other consumer migrated onto this
  library) stay wire-compatible with data/peers that predate the migration.
  If you ever need a genuinely different encoding, it is a new function, not
  a change to this one.
- **The overlong-input guard (`kMaxShift`) must stay in `unserialise_length`.**
  Removing it turns a corrupted/hostile buffer into either an out-of-bounds
  read (a buffer that isn't null-terminated and never produces a
  top-bit-set byte) or undefined behavior (shifting a 64-bit value by 70+
  bits). See ARCHITECTURE.md's own section on this.
- **`unserialise_string`'s output is a view into the input buffer, not a
  copy.** Don't change this to return an owning `std::string` without
  updating every call site that currently relies on zero-copy decode (this
  is the whole reason a view is used here instead of a copy).

## Traps

- **Don't "simplify" the continuation-bit direction.** This encoding sets
  the top bit on the LAST byte of a continuation sequence, the opposite of
  classic LEB128 (which sets it on every byte EXCEPT the last). This is
  deliberate, wire-format-compatibility, not an oversight -- see
  ARCHITECTURE.md. Flipping it breaks every existing consumer.
- **`std::uint64_t`, `unsigned long long`, and `std::size_t` are distinct
  types**, even at the same width under LP64. `unserialise_length`'s
  template parameter exists specifically so callers don't need a cast at
  the call site for whichever of these they're using -- don't replace it
  with a fixed-type overload.
- **A bool byte that isn't `'0'` or `'1'` is a decode failure, not a
  truthy/falsy coercion.** Don't change `unserialise_bool` to accept "any
  nonzero byte is true" -- that would silently paper over a desynchronized
  stream instead of surfacing it.

## Standalone vs. cluster

This continues from a 2026 rewrite inside
[`cluster`](https://github.com/Kronuz/cluster) (`length.h`, vendored there
header-only and exception-free for wire compatibility with an existing
discovery protocol) -- not from Xapiand's original `length.h`/`length.cc`,
which is derived from Xapian's GPL-licensed `pack_uint`/`unpack_uint` and
`net/length.cc` (Copyright Olly Betts) and is NOT safe to treat as a source
for an MIT-licensed extraction. `cluster`'s rewrite is a structurally
independent implementation (bool-returning instead of throwing, templated
destination type, no file-descriptor streaming variants) written for
different constraints, not a copy of that GPL code. This library is that
same independent implementation, pulled out so `cluster` can depend on it
instead of carrying a private copy, and so any other wire-compatible
consumer can too. If you are ever tempted to "restore" a feature from
Xapiand's original (the fd-streaming variants, `serialise_strings`,
`unserialise_string_at`), reimplement it from this library's own encoding
description (README/ARCHITECTURE), not by porting Xapiand's code -- keep
this repository's provenance clean.
