// LEB128 decoding throughput in nanoseconds per value, best of five, over streams that model
// what a search index decodes: small deltas, skip entries of one to three bytes, and
// uniform 32- and 64-bit values. A candidate decoder is compared against these rows in the
// same run (AGENTS.md records one that was rejected).
#include "../varint.hh"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace {

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

template <class Decode>
double ns_per_value(const std::string& s, std::size_t n, Decode decode) {
	double best = 1e300;
	for (int round = 0; round < 5; ++round) {
		std::uint64_t sum = 0;
		const auto t0 = std::chrono::steady_clock::now();
		const char* p = s.data();
		const char* end = s.data() + s.size();
		for (std::size_t i = 0; i < n; ++i) {
			std::uint64_t v = 0;
			if (!decode(&p, end, v)) { return -1; }
			sum += v;
		}
		const auto t1 = std::chrono::steady_clock::now();
		if (sum == 42) { std::puts(""); }  // keeps the loop
		best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count() / double(n));
	}
	return best;
}

}  // namespace

int main() {
	std::mt19937_64 rng(1);
	const std::size_t n = 4'000'000;
	const char* names[] = {"one byte", "1-3 bytes (skip entries)", "uniform 32-bit", "uniform 64-bit"};
	std::printf("| Values | ns per value |\n|---|---:|\n");
	for (int kind = 0; kind < 4; ++kind) {
		const std::string s = stream(rng, kind, n);
		const double t = ns_per_value(s, n, [](const char** p, const char* e, std::uint64_t& v) { return varint::leb128::decode(p, e, v); });
		std::printf("| %s | %.2f |\n", names[kind], t);
	}
	return 0;
}
