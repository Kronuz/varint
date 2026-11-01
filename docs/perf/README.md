# varint benchmarks

`bench/bench.cc` times `varint::leb128::decode` over four streams of 4,000,000 LEB128
values beside the obvious byte loop and, with `-DVARINT_BENCH_COMPARATORS=ON`, LLVM's
`decodeULEB128` and protobuf's `CodedInputStream::ReadVarint64`. The numbers below were
measured at `varint:3411f6b`.

## Streams

The generator is `std::mt19937_64` seeded with 1; each stream holds 4,000,000 values.

| Row | Values |
|---|---|
| `one byte` | uniform below 128 |
| `1-3 bytes (skip entries)` | three in four below 128, one in four uniform below 2^21 |
| `uniform 32-bit` | uniform 32-bit values, five bytes for 15 in 16 |
| `uniform 64-bit` | uniform 64-bit values, nine or ten bytes for all but one in 256 |

## Decoders

Every decoder reads a byte buffer up to an `end` pointer and returns a failure instead of
reading past it.

| Decoder | Version | Bounds handling |
|---|---|---|
| `varint` | `varint:3411f6b` | `end` before every byte; rejects a value over 64 bits |
| `byte loop` | `bench/bench.cc` | `end` before every byte; stops after ten bytes; no check on the tenth byte's value |
| `LLVM` | [LLVM](https://github.com/llvm/llvm-project) 23.1.4 and 23.1.2, `llvm/Support/LEB128.h` | `end` before every byte; rejects a value over 64 bits through its `error` out-parameter |
| `protobuf` | [protobuf](https://github.com/protocolbuffers/protobuf) v36.2 (`protobuf:2c74169`, C++ runtime 7.36.2) | a value below 128 inline; otherwise one check that ten bytes remain (or that the buffer's last byte ends a value), then up to ten bytes with no per-byte `end` check |

The two LLVM versions are the two machines' Homebrew installs; `LEB128.h` is byte-identical
in both and in the upstream `llvmorg-23.1.4` and `llvmorg-23.1.2` tags. CMake builds
libprotobuf-lite from the `v36.2` tag with the bench's compiler and optimization flags;
abseil is the Homebrew install (20260817.0), off the decode path.

## Method

- Before timing, each decoder decodes the whole stream and must return every value and
  the byte count `varint::leb128::decode` returns; each timed round must reproduce the
  stream's sum. The bench exits with an error otherwise.
- `next()` of every decoder is `always_inline`, and the disassembly of the timed loops
  shows no call left except protobuf's out-of-line `ReadVarint64Fallback`, the path its
  multi-byte values take in the library itself. `varint`, `byte loop` and `LLVM` run
  entirely inline.
- A cell is the best of five rounds; rounds alternate between the decoders so slow drift
  reaches all of them. The bench ran five times back to back on each machine with nothing
  else running, built just before; the README shows the median of the five runs.

## Machines and commands

| | Apple M4 Pro | Intel Core i9-9980HK |
|---|---|---|
| OS | macOS 26 (arm64) | macOS 15 (x86-64) |
| Compiler | Apple clang 17.0.0 | Apple clang 17.0.0 |
| CMake | 4.4.5 | 4.4.4 |
| Flags | `-O3 -DNDEBUG -std=gnu++17` | `-O3 -DNDEBUG -std=gnu++17` |

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DVARINT_BENCH_COMPARATORS=ON -DLLVM_DIR="$(brew --prefix llvm)/lib/cmake/llvm"
cmake --build build --target varint_bench --parallel
./build/varint_bench
```

## Results

Nanoseconds per value, lower is better: median of five runs, with the range of the five
in brackets. The last column is the fastest of the other three decoders divided by
`varint`: above 1 `varint` is faster.

Apple M4 Pro:

| Values | varint | byte loop | LLVM | protobuf | varint × faster than the best other |
|---|---:|---:|---:|---:|---:|
| one byte | 0.51 [0.50-0.51] | 0.51 [0.50-0.52] | 0.64 [0.63-0.64] | 1.80 [1.77-1.81] | 1.00 |
| 1-3 bytes (skip entries) | 2.22 [2.18-2.24] | 1.97 [1.93-1.98] | 2.15 [2.12-2.17] | 3.37 [3.35-3.39] | 0.89 |
| uniform 32-bit | 2.70 [2.55-2.89] | 1.97 [1.95-1.99] | 2.41 [2.40-2.45] | 2.37 [2.32-2.37] | 0.73 |
| uniform 64-bit | 7.86 [7.84-7.92] | 5.98 [5.93-6.06] | 6.62 [6.50-6.68] | 6.75 [6.57-6.97] | 0.76 |

Intel Core i9-9980HK:

| Values | varint | byte loop | LLVM | protobuf | varint × faster than the best other |
|---|---:|---:|---:|---:|---:|
| one byte | 1.09 [1.07-1.23] | 1.20 [1.18-1.28] | 2.17 [2.14-2.44] | 1.16 [1.14-1.25] | 1.06 |
| 1-3 bytes (skip entries) | 3.27 [3.21-3.36] | 3.92 [3.86-3.99] | 4.61 [4.59-4.90] | 4.13 [4.01-4.22] | 1.20 |
| uniform 32-bit | 5.85 [5.68-6.20] | 8.39 [7.97-9.03] | 8.51 [8.38-8.79] | 3.80 [3.66-3.89] | 0.65 |
| uniform 64-bit | 12.80 [12.33-13.05] | 18.69 [18.11-19.11] | 18.37 [17.52-18.65] | 10.84 [10.68-11.39] | 0.85 |

## Reading the numbers

- The ranking depends on the machine. On the M4 Pro `varint` ties the byte loop on one-byte
  values and is slower than the byte loop, LLVM and protobuf on longer ones. On the i9 it
  beats the byte loop and LLVM on every row and loses to protobuf on the uniform rows.
- The byte loop checks less than `varint` and LLVM: it does not reject a tenth byte above 1.
- Differences under about 10% are inside the run-to-run range of this table; the widest
  cells are the M4 Pro's `uniform 32-bit` `varint` (2.55-2.89) and the i9's `one byte` rows.
- Every stream has a fixed length mix, so lengths are mostly predictable. The `1-3 bytes`
  row is the least predictable, and [AGENTS.md](../../AGENTS.md) records a faster-looking
  decoder that won the uniform rows and lost on that mix and in strata's queries.
- Not measured: decoding into a 32-bit destination, and protobuf's refillable-stream path.
