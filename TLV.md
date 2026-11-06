#   ToyTLV Type-Length-Value binary coding

TLV is the most widely used approach to constructing binary protocols.
Protobuf is TLV under the hood, as well as many others.
ToyTLV is the most basic TLV implementation one can make.
The baseline ToyTLV record is like:

 1. The record type as a letter `[A-Z]` (one byte),
 2. the payload length as a 32-bit little-endian number (4 bytes),
 3. followed by the payload/value of the specified length.

As a size optimization, *short* records should use one byte for the length.
Their record type letter is lowercased then.
Using the uppercase form for 0..255 byte records counts as "overlong encoding".

For payloads past 4GB, a *huge* record has the type letter shifted down by 0x20
(`A`..`Z` become `!`..`:`) followed by a 64-bit little-endian length.
The three framings are recognized by `TLVshort`/`TLVlong`/`TLVhuge`;
the drains report the uppercase letter for all of them.

| framing | type byte | length   | header |
|---------|-----------|----------|--------|
| short   | `a`..`z`  | 1 byte   | 2      |
| long    | `A`..`Z`  | 4 bytes  | 5      |
| huge    | `!`..`:`  | 8 bytes  | 9      |

ToyTLV does not define any semantics (ints are no different from strings, etc).
It is an *enveloping* format only.

ToyTLV supports key-value records, which is a very common case.
In such a record, the payload is divided into two parts: key and value.
The first byte of the payload specifies the length of the key.
Namely, a TLKV record is like:

 1. record type (lower/upper case),
 2. payload length (1/4 bytes resp),
 3. key length (1 byte),
 4. key (as specified),
 5. value (the rest).

##  Writing a record of unknown length

`TLVu8sFeed` writes a record whose value is at hand.
When the payload is written in place, open the record with `TLVu8sStart(idle, inner, 'A')`:
it reserves a long header at the head of `idle` and points `inner` past it, leaving `idle` as is.
Write the payload into `inner` (possibly nested records, each with its own inner slice),
then `TLVu8sEnd(idle, inner, 'A')` fills in the length, shrinks a 0..255 byte payload to the short framing
and moves `idle` past the record.
`TLVu8sStartHuge`/`TLVu8sEndHuge` do the same with the 9-byte huge header
(again shrunk to short for 0..255 bytes).
On failure (`TLVNOROOM`, `TLVTOOLONG`), Start leaves `inner` and End leaves `idle` untouched.

    u8s inner = {};
    call(TLVu8sStart, u8bIdle(buf), inner, 'H');
    call(TLVu8sFeed, inner, 'T', text);
    call(TLVu8sEnd, u8bIdle(buf), inner, 'H');

`TLVu8bInto`/`TLVu8bOuto` are a buffer-only wrapper over Start/End (kept for impII):
the header stashes the buffer's DATA length and DATA restarts at the payload until Outo.
