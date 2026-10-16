# varint API reference

```cpp
namespace varint {

std::string serialise_length(unsigned long long len);
char* serialise_length(char* out, unsigned long long len) noexcept;  // room for max_length_bytes

template <typename T>
bool unserialise_length(const char** p, const char* end, T& out);

std::string serialise_string(std::string_view input);
bool unserialise_string(const char** p, const char* end, std::string_view& out);

std::string serialise_bool(bool value);
bool unserialise_bool(const char** p, const char* end, bool& out);

std::string serialise_char(char value);
bool unserialise_char(const char** p, const char* end, char& out);

namespace leb128 {
inline constexpr std::size_t max_bytes = 10;
char* encode(char* out, unsigned long long v) noexcept;   // room for max_bytes
template <typename T>
bool decode(const char** p, const char* end, T& out) noexcept;
}

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
- **`leb128` is a separate, standard format**: smaller than `serialise_length`
  for values of 255 and above, since it has no marker byte. Use it for large
  integers such as sparse id gaps; the two formats do not interoperate.
