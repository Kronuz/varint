// LEB128 decoding throughput in nanoseconds per value, best of five rounds, over streams that
// model what a search index decodes: one-byte deltas, skip entries of one to three bytes, and
// uniform 32- and 64-bit values. varint::leb128::decode runs beside the obvious byte loop and,
// with -DVARINT_BENCH_COMPARATORS=ON, beside LLVM's decodeULEB128 and protobuf's
// CodedInputStream::ReadVarint64. Every decoder first returns the same values and consumes the
// same bytes as varint on each stream (checked value by value), and each timed round must
// reproduce the stream's sum, so no loop can be dropped or made cheaper by skipping the decode.
#include "../varint.hh"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

#ifdef VARINT_BENCH_LLVM
#include <llvm/Config/llvm-config.h>
#include <llvm/Support/LEB128.h>
#endif
#ifdef VARINT_BENCH_PROTOBUF
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/runtime_version.h>
#endif

namespace {

#define BENCH_INLINE inline __attribute__((always_inline))

std::string stream(std::mt19937_64& rng, int kind, std::size_t n) {
	std::string out;
	char buf[varint::leb128::max_bytes];
	for (std::size_t i = 0; i < n; ++i) {
		std::uint64_t v = 0;
		switch (kind) {
		case 0: v = rng() % 128; break;                                       // one byte
		case 1: v = rng() % 4 == 0 ? rng() % (1u << 21) : rng() % (1u << 7); break;  // one to three bytes
		case 2: v = std::uint32_t(rng()); break;                              // uniform 32-bit
		default: v = rng(); break;                                            // uniform 64-bit
		}
		out.append(buf, varint::leb128::encode(buf, v));
	}
	return out;
}

[[noreturn]] void die(const char* name, const char* what) {
	std::fprintf(stderr, "%s: %s\n", name, what);
	std::exit(1);
}

// Each decoder is a cursor over [begin, end) with next() and consumed(). next() is always
// inlined so the timed loop holds each decoder's own code, not a call into the wrapper. Bounds
// handling:
//  Varint:   checks end before every byte, rejects a value over 64 bits (varint::leb128::decode).
//  ByteLoop: checks end before every byte and stops at ten bytes; no overflow check.
//  Llvm:     checks end before every byte and rejects a value over 64 bits (decodeULEB128).
//  Protobuf: one check up front that ten bytes remain or the last byte ends a value, then an
//            unrolled decode with no per-byte end check; rejects a varint over ten bytes.
struct Varint {
	const char* begin;
	const char* p;
	const char* end;
	Varint(const char* b, const char* e) : begin(b), p(b), end(e) {}
	BENCH_INLINE bool next(std::uint64_t& v) { return varint::leb128::decode(&p, end, v); }
	std::size_t consumed() const { return std::size_t(p - begin); }
};

struct ByteLoop {
	const unsigned char* begin;
	const unsigned char* p;
	const unsigned char* end;
	ByteLoop(const char* b, const char* e)
		: begin(reinterpret_cast<const unsigned char*>(b)), p(begin), end(reinterpret_cast<const unsigned char*>(e)) {}
	BENCH_INLINE bool next(std::uint64_t& out) {
		std::uint64_t v = 0;
		for (unsigned shift = 0; shift < 64 && p != end; shift += 7) {
			const unsigned char byte = *p++;
			v |= std::uint64_t(byte & 0x7f) << shift;
			if (!(byte & 0x80)) {
				out = v;
				return true;
			}
		}
		return false;
	}
	std::size_t consumed() const { return std::size_t(p - begin); }
};

#ifdef VARINT_BENCH_LLVM
struct Llvm {
	const std::uint8_t* begin;
	const std::uint8_t* p;
	const std::uint8_t* end;
	Llvm(const char* b, const char* e)
		: begin(reinterpret_cast<const std::uint8_t*>(b)), p(begin), end(reinterpret_cast<const std::uint8_t*>(e)) {}
	BENCH_INLINE bool next(std::uint64_t& v) {
		unsigned n = 0;
		const char* error = nullptr;
		v = llvm::decodeULEB128(p, &n, end, &error);
		p += n;
		return error == nullptr;
	}
	std::size_t consumed() const { return std::size_t(p - begin); }
};
#endif

#ifdef VARINT_BENCH_PROTOBUF
struct Protobuf {
	google::protobuf::io::CodedInputStream in;
	Protobuf(const char* b, const char* e)
		: in(reinterpret_cast<const std::uint8_t*>(b), int(e - b)) {}
	BENCH_INLINE bool next(std::uint64_t& v) { return in.ReadVarint64(&v); }
	std::size_t consumed() const { return std::size_t(in.CurrentPosition()); }
};
#endif

template <class Decoder>
void check(const char* name, const std::string& s, const std::vector<std::uint64_t>& want) {
	Decoder d(s.data(), s.data() + s.size());
	for (const std::uint64_t expected : want) {
		std::uint64_t v = 0;
		if (!d.next(v)) { die(name, "decode failed"); }
		if (v != expected) { die(name, "decoded a different value than varint::leb128::decode"); }
	}
	if (d.consumed() != s.size()) { die(name, "consumed a different byte count than varint::leb128::decode"); }
}

// One timed pass over the stream; the sum is compared with the reference so the values stay live.
template <class Decoder>
__attribute__((noinline)) double round_ns(const char* name, const std::string& s, std::size_t n, std::uint64_t want_sum) {
	Decoder d(s.data(), s.data() + s.size());
	std::uint64_t sum = 0;
	const auto t0 = std::chrono::steady_clock::now();
	for (std::size_t i = 0; i < n; ++i) {
		std::uint64_t v = 0;
		if (!d.next(v)) { die(name, "decode failed"); }
		sum += v;
	}
	const auto t1 = std::chrono::steady_clock::now();
	if (sum != want_sum) { die(name, "sum differs from varint::leb128::decode"); }
	return std::chrono::duration<double, std::nano>(t1 - t0).count() / double(n);
}

struct Impl {
	const char* name;
	void (*check)(const char*, const std::string&, const std::vector<std::uint64_t>&);
	double (*round)(const char*, const std::string&, std::size_t, std::uint64_t);
};

template <class Decoder>
Impl impl(const char* name) {
	return {name, check<Decoder>, round_ns<Decoder>};
}

}  // namespace

int main() {
	std::vector<Impl> impls = {impl<Varint>("varint"), impl<ByteLoop>("byte loop")};
#ifdef VARINT_BENCH_LLVM
	impls.push_back(impl<Llvm>("LLVM"));
	std::fprintf(stderr, "LLVM %s decodeULEB128\n", LLVM_VERSION_STRING);
#endif
#ifdef VARINT_BENCH_PROTOBUF
	impls.push_back(impl<Protobuf>("protobuf"));
	std::fprintf(stderr, "protobuf runtime %d.%d.%d CodedInputStream::ReadVarint64\n", PROTOBUF_VERSION / 1000000,
		PROTOBUF_VERSION / 1000 % 1000, PROTOBUF_VERSION % 1000);
#endif

	std::mt19937_64 rng(1);
	const std::size_t n = 4'000'000;
	const char* names[] = {"one byte", "1-3 bytes (skip entries)", "uniform 32-bit", "uniform 64-bit"};
	std::printf("| Values |");
	for (const Impl& i : impls) { std::printf(" %s |", i.name); }
	std::printf("\n|---|");
	for (std::size_t i = 0; i < impls.size(); ++i) { std::printf("---:|"); }
	std::printf("\n");
	for (int kind = 0; kind < 4; ++kind) {
		const std::string s = stream(rng, kind, n);
		std::vector<std::uint64_t> want;
		want.reserve(n);
		std::uint64_t want_sum = 0;
		const char* p = s.data();
		for (std::size_t i = 0; i < n; ++i) {
			std::uint64_t v = 0;
			if (!varint::leb128::decode(&p, s.data() + s.size(), v)) { die("varint", "decode failed"); }
			want.push_back(v);
			want_sum += v;
		}
		if (p != s.data() + s.size()) { die("varint", "stream not fully consumed"); }
		for (const Impl& i : impls) { i.check(i.name, s, want); }

		// Rounds alternate between implementations so slow drift on a shared host hits all of them.
		std::vector<double> best(impls.size(), 1e300);
		for (int round = 0; round < 5; ++round) {
			for (std::size_t i = 0; i < impls.size(); ++i) {
				best[i] = std::min(best[i], impls[i].round(impls[i].name, s, n, want_sum));
			}
		}
		std::printf("| %s |", names[kind]);
		for (const double t : best) { std::printf(" %.2f |", t); }
		std::printf("\n");
	}
	return 0;
}
