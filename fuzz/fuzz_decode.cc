// libFuzzer target: every decoder walks arbitrary bytes without leaving them, and values
// taken from the input round-trip through both formats.
#include "varint.hh"

#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const char* begin = reinterpret_cast<const char*>(data);
	const char* end = begin + size;
	for (const char* p = begin; p < end;) {
		unsigned long long v;
		if (!varint::unserialise_length(&p, end, v) || p > end) { break; }
	}
	for (const char* p = begin; p < end;) {
		unsigned long long v;
		if (!varint::leb128::decode(&p, end, v) || p > end) { break; }
	}
	for (const char* p = begin; p < end;) {
		std::string_view s;
		if (!varint::unserialise_string(&p, end, s) || p > end || s.data() < begin || s.data() + s.size() > end) { break; }
	}
	for (std::size_t i = 0; i + 8 <= size; i += 8) {
		unsigned long long v;
		std::memcpy(&v, data + i, 8);
		char buf[varint::max_length_bytes];
		const char* p = buf;
		char* e = varint::serialise_length(buf, v);
		unsigned long long back = 0;
		if (!varint::unserialise_length(&p, e, back) || back != v || p != e) { std::abort(); }
		e = varint::leb128::encode(buf, v);
		p = buf;
		if (!varint::leb128::decode(&p, e, back) || back != v || p != e) { std::abort(); }
	}
	return 0;
}
