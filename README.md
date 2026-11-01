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

Time to decode one LEB128 value, in nanoseconds (lower is better), from a byte buffer that
ends at an `end` pointer, over streams of 4,000,000 values; each cell is the median of five
runs of the bench's best of five rounds. Each row is a stream and each column a decoder, not
a mode.

Rows:

- `one byte`: values below 128.
- `1-3 bytes (skip entries)`: three in four values below 128 and one in four below 2^21, the
  mix of a search index's skip entries.
- `uniform 32-bit` and `uniform 64-bit`: random values of that width.

Columns:

- `varint`: `varint::leb128::decode`.
- `byte loop`: a hand-written loop that checks `end` before each byte and stops after ten.
- `LLVM`: `decodeULEB128` from [LLVM](https://github.com/llvm/llvm-project) 23.1
  (`llvm/Support/LEB128.h`, identical in 23.1.2 and 23.1.4).
- `protobuf`: `CodedInputStream::ReadVarint64` from
  [protobuf](https://github.com/protocolbuffers/protobuf) v36.2, built from source.
- `varint × faster than the best other`: the fastest of the other three decoders' time
  divided by `varint`'s, so above 1 `varint` is faster.

All four return the same values and consume the same bytes on these streams (the bench exits
with an error otherwise). `varint`, `LLVM` and the byte loop check `end` before every byte,
and `varint` and `LLVM` also reject a value over 64 bits; `protobuf` checks once per value
that ten bytes remain.

Apple M4 Pro, macOS 26 (arm64):

| Values | varint | byte loop | LLVM | protobuf | varint × faster than the best other |
|---|---:|---:|---:|---:|---:|
| one byte | 0.51 | 0.51 | 0.64 | 1.80 | 1.00 |
| 1-3 bytes (skip entries) | 2.22 | 1.97 | 2.15 | 3.37 | 0.89 |
| uniform 32-bit | 2.70 | 1.97 | 2.41 | 2.37 | 0.73 |
| uniform 64-bit | 7.86 | 5.98 | 6.62 | 6.75 | 0.76 |

Intel Core i9-9980HK, macOS 15 (x86-64):

| Values | varint | byte loop | LLVM | protobuf | varint × faster than the best other |
|---|---:|---:|---:|---:|---:|
| one byte | 1.09 | 1.20 | 2.17 | 1.16 | 1.06 |
| 1-3 bytes (skip entries) | 3.27 | 3.92 | 4.61 | 4.13 | 1.20 |
| uniform 32-bit | 5.85 | 8.39 | 8.51 | 3.80 | 0.65 |
| uniform 64-bit | 12.80 | 18.69 | 18.37 | 10.84 | 0.85 |

On the M4 Pro `varint` ties the byte loop on one-byte values and trails the fastest other
decoder on longer ones; on the i9 it beats the byte loop and LLVM on every row and trails
protobuf on the uniform rows.

Both machines: Apple clang 17.0.0, CMake 4.4, `-O3 -DNDEBUG` for the bench and for protobuf,
which the bench builds from source (abseil is Homebrew's). To rebuild and rerun the table:

```sh
brew install llvm abseil
cmake -B build -DCMAKE_BUILD_TYPE=Release -DVARINT_BENCH_COMPARATORS=ON -DLLVM_DIR="$(brew --prefix llvm)/lib/cmake/llvm"
cmake --build build --target varint_bench --parallel
./build/varint_bench
```

Without `-DVARINT_BENCH_COMPARATORS=ON` the bench times `varint` and the byte loop and needs
nothing else. [docs/perf](docs/perf/README.md) has the run-to-run range, the method and the
bounds each decoder checks.

## 📚 Documentation

- [docs/API.md](docs/API.md): every encoder and decoder, including `leb128`.
- [docs/perf](docs/perf/README.md): the benchmark's method, spread and bounds handling.
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
`-DVARINT_BENCH_COMPARATORS=ON` adds LLVM and protobuf to the bench (see Performance), and
`-DVARINT_FUZZ=ON` adds the libFuzzer target, which needs a Clang with libFuzzer.

## 🤝 Contributing

Issues and pull requests are welcome; the wire format is fixed. Read
[AGENTS.md](AGENTS.md) and the catalog's
[rules of engagement](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#rules-of-engagement)
first.

## 📄 License

MIT, see [LICENSE](LICENSE). An independent implementation, wire-compatible with
[Xapiand](https://github.com/Kronuz/Xapiand)'s `length.h`.
