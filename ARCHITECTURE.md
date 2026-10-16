# Architecture

The internals of `varint`: why the length encoding looks the way it does,
and the two traps in decoding it. For usage see `README.md`; for the repo
map and invariants see `AGENTS.md`.

## Shape

A single header, nothing else:

```
  varint.hh   serialise_length/unserialise_length, serialise_string/
              unserialise_string, serialise_bool/unserialise_bool,
              serialise_char/unserialise_char
```

No compiled translation unit, no external dependency. `varint.hh` includes
only `<cstddef>`, `<string>`, and `<string_view>`.

## Why a byte-literal fast path at all

Most lengths encountered in practice -- a field name, a short string, a
small count -- fit in a single byte (0-254). Encoding every length through a
general base-128 varint would spend at least 2 bytes on these overwhelmingly
common small values (1 continuation byte minimum, per most LEB128-style
schemes, since 7 bits per byte means values 128-254 already need a second
byte in a scheme with no single-byte fast path). Reserving one sentinel
value (255, `0xff`) to mean "the real length follows, base-128-encoded"
keeps the single-byte case truly single-byte while still supporting
arbitrarily large values through the escape.

## Why the continuation bit means "stop" instead of "continue"

Classic LEB128 sets the top bit on every byte EXCEPT the last, meaning
"there's more after this one." This encoding does the reverse: every byte
has its top bit CLEAR except the last, which has it SET. The two schemes are
equally valid and equally simple to decode (a loop that exits -- "stop
accumulating" -- on a different bit state is not harder than one that exits
on the other). This specific choice matches an existing wire protocol this
library is compatible with (see README's "Provenance"), not a claim that
it's superior to the alternative. The decode loop:

```cpp
for (;;) {
    if (*p == end || shift >= std::numeric_limits<unsigned long long>::digits) return false;
    auto byte = static_cast<unsigned char>(*(*p)++);
    auto chunk = static_cast<unsigned long long>(byte & 0x7f);
    if (chunk > (std::numeric_limits<unsigned long long>::max() >> shift)) return false;
    len |= chunk << shift;
    shift += 7;
    if (byte & 0x80) break; // this was the last byte
}
```

Each byte contributes its low 7 bits at the current `shift`, then `shift`
advances by 7; the loop ends when a byte's top bit is set, not when some
fixed byte count is reached -- so the encoding is self-terminating and the
decoder never needs to know the value's magnitude in advance.

## The overlong-input guard

Three checks keep a corrupted or hostile buffer from being misread:

1. **The shift stops at 64.** The loop gives up once `shift` reaches the bit
   width of `unsigned long long`, so a run of bytes that never sets the top bit
   cannot read on past where any valid encoding ends, and never shifts by 64 or
   more (undefined behavior).
2. **No bit is silently dropped.** A chunk whose bits would be shifted out of 64
   bits is an overflow, rejected rather than truncated.
3. **The final `+ 255` cannot wrap.** A decoded remainder above `max - 255` is
   rejected before the bias is added; a value that does not fit the destination
   type `T` is rejected last.

Each check returns `false` rather than throwing or reading further on untrusted
input. `fuzz/fuzz_decode.cc` exercises every decoder against arbitrary bytes.

## Why `unserialise_length` is templated on the destination type

`std::uint64_t`, `unsigned long long`, and `std::size_t` are distinct types
in the C++ type system even when they happen to have the same width on a
given platform (true for all three under LP64, the common Linux/macOS
model). A single overload taking a fixed `unsigned long long&` out-parameter
would reject a `std::uint64_t&` or `std::size_t&` argument without an
explicit cast at every call site, which is exactly the kind of friction that
makes a small utility library annoying to actually use. Templating on the
destination type (`template <typename T> bool unserialise_length(const
char**, const char*, T&)`) accepts any integral destination directly, with a
single `static_cast<T>(len)` at the end converting from the one actual
working type (`unsigned long long`) that decoding is performed in.

## Why bool serializes as ASCII, not a raw byte

`serialise_bool` emits the character `'0'` or `'1'` (0x30/0x31), not the
byte values `0x00`/`0x01`. This makes a raw hex/text dump of a serialised
message distinguishable at a glance (a `0x01` byte could be a bool, a short
length, or a continuation digit; `'1'` is unambiguously the ASCII bool
convention this format uses). `unserialise_bool` is correspondingly strict:
it rejects any byte other than `'0'`/`'1'`, rather than treating "nonzero"
as true the way a raw-byte encoding typically would -- a byte that isn't
either exact value is a sign the stream is desynchronized (e.g. a caller
misread the preceding field's length), and silently coercing it to a bool
would hide that instead of surfacing it as a decode failure.
