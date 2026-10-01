// Runnable smoke test (no framework): round-trips plus the exact byte
// vectors below, captured from the implementation this library replaces
// (cluster's own vendored length.h) to guarantee wire compatibility.
#include "../varint.hh"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>

namespace {

std::string hex(std::string_view s) {
	static constexpr char digits[] = "0123456789abcdef";
	std::string out;
	for (unsigned char c : s) {
		out += digits[c >> 4];
		out += digits[c & 0xf];
		out += ' ';
	}
	return out;
}

void expect_length_bytes(unsigned long long value, std::string_view expected_hex_bytes) {
	auto encoded = varint::serialise_length(value);
	auto got = hex(encoded);
	if (got != expected_hex_bytes) {
		std::printf("MISMATCH for %llu: got [%s] expected [%s]\n", value, got.c_str(), std::string(expected_hex_bytes).c_str());
		assert(false);
	}
}

} // namespace

int main() {
	// Exact byte vectors captured from cluster's prior length.h, covering
	// the single-byte-literal range, the 0xff continuation boundary, and
	// values spanning 1, 2, 4, 5, and 10 continuation bytes.
	expect_length_bytes(0, "00 ");
	expect_length_bytes(1, "01 ");
	expect_length_bytes(42, "2a ");
	expect_length_bytes(127, "7f ");
	expect_length_bytes(128, "80 ");
	expect_length_bytes(254, "fe ");
	expect_length_bytes(255, "ff 80 ");
	expect_length_bytes(256, "ff 81 ");
	expect_length_bytes(300, "ff ad ");
	expect_length_bytes(65535, "ff 00 7e 83 ");
	expect_length_bytes(65536, "ff 01 7e 83 ");
	expect_length_bytes(4294967296ULL, "ff 01 7e 7f 7f 8f ");
	expect_length_bytes(1099511627776ULL, "ff 01 7e 7f 7f 7f 9f ");
	expect_length_bytes(~0ULL, "ff 00 7e 7f 7f 7f 7f 7f 7f 7f 81 ");
	std::puts("exact byte-vector compatibility with the prior implementation OK");

	// Round-trip every captured value, plus a sweep, through unserialise.
	unsigned long long values[] = {
		0, 1, 42, 127, 128, 254, 255, 256, 300, 65535, 65536,
		4294967296ULL, 1099511627776ULL, ~0ULL};
	for (auto v : values) {
		auto encoded = varint::serialise_length(v);
		const char* p = encoded.data();
		const char* end = p + encoded.size();
		unsigned long long decoded = 0;
		assert(varint::unserialise_length(&p, end, decoded));
		assert(decoded == v);
		assert(p == end); // consumed exactly the encoded bytes, nothing more
	}
	std::puts("length round-trip over the captured value set OK");

	// unserialise_length must bind to distinct destination types without
	// a cast at the call site (std::uint64_t, unsigned long long, size_t
	// are all distinct types under LP64 even at the same width).
	{
		auto encoded = varint::serialise_length(12345);
		const char* p = encoded.data();
		const char* end = p + encoded.size();
		std::uint64_t as_u64 = 0;
		assert(varint::unserialise_length(&p, end, as_u64));
		assert(as_u64 == 12345);

		p = encoded.data();
		std::size_t as_size_t = 0;
		assert(varint::unserialise_length(&p, end, as_size_t));
		assert(as_size_t == 12345);
	}
	std::puts("unserialise_length binds to uint64_t/size_t alike OK");

	// Truncated input is rejected, not misread.
	{
		auto encoded = varint::serialise_length(65536); // multi-byte
		const char* p = encoded.data();
		const char* end = p + encoded.size() - 1; // chop the last byte off
		unsigned long long decoded = 0;
		assert(!varint::unserialise_length(&p, end, decoded));
	}
	{
		const char empty[1] = {};
		const char* p = empty;
		const char* end = empty; // zero-length buffer
		unsigned long long decoded = 0;
		assert(!varint::unserialise_length(&p, end, decoded));
	}
	std::puts("truncated/empty input rejected, not misread OK");

	// Strings: exact vectors, then round-trip, including the empty string.
	expect_length_bytes(0, "00 "); // sanity: reused by the empty-string case below
	{
		struct Case { std::string_view input; std::string_view expected_hex; };
		Case cases[] = {
			{"", "00 "},
			{"a", "01 61 "},
			{"hello world", "0b 68 65 6c 6c 6f 20 77 6f 72 6c 64 "},
			{"x", "01 78 "},
		};
		for (auto& c : cases) {
			auto encoded = varint::serialise_string(c.input);
			assert(hex(encoded) == c.expected_hex);

			const char* p = encoded.data();
			const char* end = p + encoded.size();
			std::string_view decoded;
			assert(varint::unserialise_string(&p, end, decoded));
			assert(decoded == c.input);
			assert(p == end);
		}
	}
	std::puts("string exact vectors + round-trip, including empty, OK");

	// Bool and char round-trip.
	{
		for (bool v : {true, false}) {
			auto encoded = varint::serialise_bool(v);
			const char* p = encoded.data();
			const char* end = p + encoded.size();
			bool decoded = false;
			assert(varint::unserialise_bool(&p, end, decoded));
			assert(decoded == v);
		}
		auto bad = std::string("2"); // not '0' or '1'
		const char* p = bad.data();
		const char* end = p + bad.size();
		bool decoded = false;
		assert(!varint::unserialise_bool(&p, end, decoded));
	}
	{
		auto encoded = varint::serialise_char('Q');
		const char* p = encoded.data();
		const char* end = p + encoded.size();
		char decoded = 0;
		assert(varint::unserialise_char(&p, end, decoded));
		assert(decoded == 'Q');
	}
	std::puts("bool and char round-trip, including rejecting a bad bool byte, OK");

	std::puts("all varint tests passed");
	return 0;
}
