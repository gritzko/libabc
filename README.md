#   Algebraic Bricklaying C

ABC is an experimental C dialect for systems programming without a runtime
and without abstraction stacking — yet powerful enough to build real things.

ABC is influenced by Go, kernel C, and the [JPL C standard][j]. It stays in C
because C is our Latin — it is not going away, and system utilities have no
use for GC, runtime or feature creep.

##  The idea

- **No runtime, no encapsulation.** A file descriptor is an `int fd`. A byte
  slice is a pointer pair. No objects hiding the primitives. Modules that
  don't know each other still compose seamlessly.
- **Memory safety through buffers and slices**, not through a borrow checker
  or GC. All "star types" (slices like `u8cs`, gauges like `u32g`, buffers
  like `u64b`) are pointer arrays internally, with standard accessors that 
  bounds-check in debug builds. Pointer arithmetic is banned by convention.
- **Record types have fixed bit layouts** (`u64`, `uuid128`, `sha256`), so
  they are trivially serializable and mmap-friendly. Messy structs are fine —
  just keep them private.
- **No rug-pulling reallocations.** Only the owner of a buffer can resize it;
  downstream code returns `NOROOM` instead of reallocating under the
  caller's feet. Arenas, ring buffers and pre-sized limits are preferred over
  `malloc`/`free`. Resource acquisition happens at the top of the call chain.
- **Errors are `ok64` codes** with human-readable Base64 names like
  `MMAPFAIL`. Procedures that can fail return `ok64`; functions that can't
  return their value. The [PRO][P] module wires this into stack-traced
  `call()`/`done`/`fail()` macros.

##  Code sample

````c
// ok64: 64-bit error code, 0 == OK (01.h)
ok64 SortAndDedup() {
    // entry guard, declares hidden status var (PRO.h)
    sane(1);
    // u64 = uint64_t (01.h)
    u64 arr[] = {5, 3, 1, 3, 2, 5, 1, 4, 2};
    // a$: declare u64 *data[2] = {arr, arr + sizeof(arr)/sizeof(u64)} (S.h)
    a$(u64, data, arr);
    // in-place quicksort over the slice (QSORTx.h)
    u64sSort(data);
    // collapse equal runs in place (QSORTx.h)
    u64sDedup(data);
    // u64sLen = term - head (Sx.h); testeq: equal or FAILEQ (PRO.h)
    testeq(u64sLen(data), (size_t)5);
    // u64sAt: 0-indexed element access (Sx.h)
    for (u64 i = 0; i < 5; i++) testeq(u64sAt(data, i), i + 1);
    // return accumulated status (PRO.h)
    done;
}
````
Libabc heavily relies on C templates (see `*x.h` files, e.g. QSORTx.h).

##  Modules

See [INDEX.md](./INDEX.md) for the full list of modules and functions.
Notable starting points:

- [S.md](./S.md), [B.md](./B.md), [C.md](./C.md) — slices, buffers, cursors
- [HEX.md](./HEX.md) — minimal example of idiomatic slice code
- [HEAP.md](./HEAP.md) — non-trivial container built on buffers
- [PRO.md](./PRO.md) — error handling and call macros
- [LEX.md](./LEX.md) — Ragel-based lexer generator
- [FILE.md](./FILE.md), [MMAP.md](./MMAP.md) — I/O and memory mapping
- [TLV.md](./TLV.md), [ZINT.md](./ZINT.md) — variable-length serialization

[P]: ./PRO.md
[j]: https://yurichev.com/mirrors/C/JPL_Coding_Standard_C.pdf
