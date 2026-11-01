# varint

Part of [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries), a collection of small, reusable C++ libraries.

[![CI](https://github.com/Kronuz/varint/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/varint/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![Header-only](https://img.shields.io/badge/header--only-yes-green.svg)

Header-only varint codec for C++17: one byte for lengths below 255, length-prefixed
strings, LEB128, no exceptions. A length of 255 or more is a `0xff` marker and a base-128
tail whose last byte has its top bit set, and every decoder returns `false` on truncated,
overlong or overflowing input. Reach for it for length prefixes and counts in wire
protocols and on-disk records that decode untrusted bytes.

## ✨ Highlights

- **One byte for the common case:** `5` is `05`, `300` is `ff ad`, and `1000000` is
  `ff 41 02 bd`; at most 11 bytes for any 64-bit length.
- **No exceptions:** every decoder returns `false` on bad input and never reads past
  `end`, so parsing a socket buffer is a plain `if`.
- **Overflow checks on every byte,** and a libFuzzer target over the length, string and
  LEB128 decoders.
- **No allocation on the hot path:** `serialise_length(char* out, v)` writes into the
  caller's buffer.
- **Strings, bools and chars:** `serialise_string`, `serialise_bool` and
  `serialise_char`, each with its `unserialise_*`.
- **LEB128 too** (`varint::leb128`), for large integers such as sparse id gaps.
- **Wire-compatible** with Xapiand's `length.h` encoding; the tests pin exact bytes
  against reference vectors.

## 🚀 Quick start

```cpp
#include "varint.hh"

#include <cstdio>
#include <string>
#include <string_view>

int main() {
	std::string wire = varint::serialise_string("hello") + varint::serialise_length(1'000'000);

	const char* p = wire.data();
	const char* end = p + wire.size();
	std::string_view name;
	unsigned long long count = 0;
	if (!varint::unserialise_string(&p, end, name) || !varint::unserialise_length(&p, end, count)) {
		return 1;  // truncated or malformed
	}
	std::printf("%.*s %llu, %zu bytes\n", int(name.size()), name.data(), count, wire.size());
	// hello 1000000, 10 bytes
}
```

[`examples/demo.cc`](examples/demo.cc) encodes and decodes a short message.

## 📦 Installation

Requires C++17 and CMake 3.14 or newer; no other dependencies. CI builds and tests it on
Linux (x86_64 with GCC and Clang, arm64), macOS (arm64, x86_64) and FreeBSD, plus ASan,
UBSan and TSan builds.

```cmake
include(FetchContent)
FetchContent_Declare(varint
  GIT_REPOSITORY https://github.com/Kronuz/varint.git
  GIT_TAG        d9631ab7948a76196c67e7490ab2aebc3ca6473d)  # or main, to track the tip
FetchContent_MakeAvailable(varint)

target_link_libraries(my_app PRIVATE varint::varint)
```

Link `varint::varint` and include `varint.hh`, or copy `varint.hh` onto the include path.

## 🎯 When to use it

- Length prefixes and counts in a binary format, decoded from untrusted bytes.
- Records that must stay byte-compatible with Xapiand.

Not for:

- Schema-driven messages: use a serialization format (MessagePack, protobuf) when you
  need fields, types and evolution.
- Interchange with standard LEB128 readers through `serialise_length`: its tail marks the
  last byte, the reverse of LEB128's continuation bit. Use `varint::leb128` there.

## ⚡ Performance

`varint::leb128::decode` over streams of 4,000,000 values, in nanoseconds per value, best of
5 rounds. The streams model what a search index decodes: one-byte deltas, skip entries of one
to three bytes, and uniform 32-bit and 64-bit values.

| Values | Apple M4 Pro | Intel Core i9-9980HK |
|---|---:|---:|
| one byte | 0.52 | 1.38 |
| 1-3 bytes (skip entries) | 2.10 | 3.61 |
| uniform 32-bit | 2.98 | 5.56 |
| uniform 64-bit | 7.88 | 13.52 |

Apple clang 17 on macOS, `-O3 -DNDEBUG`; the same command on both machines:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target varint_bench
./build/varint_bench
```

## 📚 Documentation

- [docs/API.md](docs/API.md): every encoder and decoder, including `leb128`.
- [ARCHITECTURE.md](ARCHITECTURE.md): the wire format and the overlong-input guards.
- [AGENTS.md](AGENTS.md): repository map, invariants and traps.
- [Catalog entry](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#varint) in
  Kronuz C++ Libraries.

## 🛠️ Building and testing

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

`./build/varint_demo` runs the tour and `./build/varint_bench` times LEB128 decoding;
`-DVARINT_FUZZ=ON` adds the libFuzzer target, which needs a Clang with libFuzzer.

## 🤝 Contributing

Issues and pull requests are welcome; the wire format is fixed. Read
[AGENTS.md](AGENTS.md) and the catalog's
[rules of engagement](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#rules-of-engagement)
first.

## 📄 License

MIT, see [LICENSE](LICENSE). An independent implementation, wire-compatible with
[Xapiand](https://github.com/Kronuz/Xapiand)'s `length.h`.
