// A runnable tour, not a test: encode a small message (a length-prefixed
// string plus a couple of integers) and decode it back.
#include "../varint.hh"

#include <cstdio>

int main() {
	std::string wire;
	wire += varint::serialise_string("hello");
	wire += varint::serialise_length(1'000'000);
	wire += varint::serialise_bool(true);

	std::printf("encoded %zu bytes\n", wire.size());

	const char* p = wire.data();
	const char* end = p + wire.size();

	std::string_view name;
	if (!varint::unserialise_string(&p, end, name)) {
		std::puts("decode failed: string");
		return 1;
	}
	std::printf("name = %.*s\n", static_cast<int>(name.size()), name.data());

	unsigned long long count = 0;
	if (!varint::unserialise_length(&p, end, count)) {
		std::puts("decode failed: length");
		return 1;
	}
	std::printf("count = %llu\n", count);

	bool flag = false;
	if (!varint::unserialise_bool(&p, end, flag)) {
		std::puts("decode failed: bool");
		return 1;
	}
	std::printf("flag = %s\n", flag ? "true" : "false");

	std::printf("consumed all %zu bytes: %s\n", wire.size(), (p == end) ? "yes" : "no");
	return 0;
}
