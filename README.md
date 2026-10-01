# varint

A small, header-only **length/integer codec**, for C++17 and later: a
byte-below-255-is-the-length fast path, a compact base-128 continuation for
anything larger, plus a length-prefixed string, a one-byte bool, and a
one-byte char -- all exception-free.

## What it is

Four encode/decode pairs over a compact binary wire format:

- `serialise_length` / `unserialise_length<T>` -- an unsigned integer.
- `serialise_string` / `unserialise_string` -- a length-prefixed byte string.
- `serialise_bool` / `unserialise_bool` -- one ASCII byte, `'0'` or `'1'`.
- `serialise_char` / `unserialise_char` -- one raw byte.

The length encoding: a byte below 255 IS the length, verbatim -- the
overwhelmingly common case (short strings, small counts) costs exactly one
byte. A byte of `0xff` introduces `(length - 255)`, encoded as a
little-endian base-128 sequence: every byte holds 7 bits of the value in its
low bits, every byte except the last has its top bit clear, and the LAST
byte has its top bit set, marking the end (the reverse convention from
classic LEB128, where the continuation bit marks "more bytes follow" rather
than "this is the last one").

Every `unserialise_*` function returns `bool` -- `false` on truncated or
malformed input -- instead of throwing, so decoding untrusted/partial input
from a socket or a file is a plain `if`, not a `try`/`catch`.

## Install

Header-only: `varint.hh` is the whole library. Requires C++17 and nothing
else beyond the standard library.

With CMake `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(
  varint
  GIT_REPOSITORY https://github.com/Kronuz/varint.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(varint)

target_link_libraries(your_target PRIVATE varint::varint)
```

Then:

```cpp
#include "varint.hh"
```

## Usage

```cpp
#include "varint.hh"

std::string wire;
wire += varint::serialise_string("hello");
wire += varint::serialise_length(1'000'000);
wire += varint::serialise_bool(true);

const char* p = wire.data();
const char* end = p + wire.size();

std::string_view name;
if (!varint::unserialise_string(&p, end, name)) { /* truncated/malformed */ }

unsigned long long count = 0;
if (!varint::unserialise_length(&p, end, count)) { /* truncated/malformed */ }

bool flag = false;
if (!varint::unserialise_bool(&p, end, flag)) { /* truncated/malformed */ }
```

Each `unserialise_*` call advances `*p` past what it consumed, so a sequence
of fields decodes by chaining calls and checking each return value -- there
is no separate "how many bytes did that take" step.

## API reference

```cpp
namespace varint {

std::string serialise_length(unsigned long long len);

template <typename T>
bool unserialise_length(const char** p, const char* end, T& out);

std::string serialise_string(std::string_view input);
bool unserialise_string(const char** p, const char* end, std::string_view& out);

std::string serialise_bool(bool value);
bool unserialise_bool(const char** p, const char* end, bool& out);

std::string serialise_char(char value);
bool unserialise_char(const char** p, const char* end, char& out);

} // namespace varint
```

- **`unserialise_length` is templated on the destination type** so it binds
  equally to `std::uint64_t`, `unsigned long long`, or `std::size_t` --
  distinct types under LP64 even at the same width, so a single fixed-type
  reference parameter would reject some of them without a cast at the call
  site.
- **`unserialise_string`'s output is a view, not a copy** -- it points into
  the same buffer `*p` was reading from, valid exactly as long as that
  buffer is.
- **A bool serialises as ASCII `'0'`/`'1'`, not byte `0x00`/`0x01`** -- a raw
  hex dump of a message stays readable at a glance; `unserialise_bool`
  rejects any other byte rather than treating it as truthy/falsy.

## Build & test

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

The test checks exact byte output against a captured reference vector set
(so a change here can't silently drift the wire format), round-trips every
value through encode+decode, confirms `unserialise_length` binds to
different destination types without a cast, and confirms truncated/empty
input is rejected rather than misread.

## Examples

[`examples/demo.cc`](examples/demo.cc) is a runnable tour:

```sh
cmake -B build && cmake --build build && ./build/varint_demo
```

It encodes a string, a length, and a bool into one buffer, then decodes
them back out in order.

## Provenance

This continues from a 2026 rewrite inside [`cluster`](https://github.com/Kronuz/cluster)
(Germán Méndez Bravo's own code, vendored there header-only and
exception-free for wire compatibility with an existing discovery protocol),
not from Xapiand's original `length.h`/`length.cc`. That distinction matters:
Xapiand's original length codec is derived from Xapian's own GPL-licensed
`pack_uint`/`unpack_uint` (`xapian-core/common/pack.h`) and `net/length.cc`
(Copyright Olly Betts), so it is not safe to republish under this
collection's MIT license. `cluster`'s rewrite is a structurally independent
implementation -- different error-handling model (`bool` return instead of
exceptions), templated destination type, no file-descriptor streaming
variants -- written for different constraints, not a copy of that GPL code;
this library is that same independent implementation, extracted to stand on
its own so `cluster` (and any other wire-compatible consumer) can depend on
it instead of each carrying a private copy. See
[ARCHITECTURE.md](ARCHITECTURE.md) for the format in detail and
[AGENTS.md](AGENTS.md) for the repo map and invariants.

## License

MIT, Copyright (c) 2026 Germán Méndez Bravo. See [LICENSE](LICENSE).
