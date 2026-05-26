#   ABC buffers

>   "As simple as possible, but not simpler" -- A.Einstein

An ABC buffer is an array of four pointers dividing a memory
range into three [slices][S]: PAST, DATA and IDLE.
Consumption of a slice (DATA or IDLE) enlarges the adjoined slice 
(PAST or DATA resp).

```
       PAST         DATA             IDLE
  |----------->------------->-------------------|
buf[0]      buf[1]        buf[2]              buf[3]
```

Buffers imply ownership; if you hold the buffer you own the memory.
Pass buffers down the call stack by pointer (`u8bp`), never duplicate.
Buffers can be stack-allocated, malloc-ed, or memory-mapped.

##  Typed functions (preferred)

| Operation       | Typed function       | Notes                    |
|-----------------|---------------------|--------------------------|
| Get DATA slice  | `u8bData(buf)`      | mutable `[1,2)`          |
| Get DATA const  | `u8bDataC(buf)`     | const slice              |
| Get IDLE slice  | `u8bIdle(buf)`      | mutable `[2,3)`          |
| Get IDLE const  | `u8bIdleC(buf)`     | const slice              |
| Total capacity  | `u8bLen(buf)`       | elements `[0,3)`         |
| DATA length     | `u8bDataLen(buf)`   | elements in DATA         |
| IDLE length     | `u8bIdleLen(buf)`   | room available           |
| Validate        | `u8bOK(buf)`        | check pointers valid     |
| Has room?       | `u8bHasRoom(buf)`   | IDLE non-empty           |
| Has data?       | `u8bHasData(buf)`   | DATA non-empty           |
| First element   | `u8bHead(buf)`      | first in DATA            |
| Last element    | `u8bLast(buf)`      | last in DATA             |
| Feed slice      | `u8bFeed(buf, s)`   | append to DATA           |
| Feed one        | `u8bFeed1(buf, v)`  | append single element    |
| Push by ptr     | `u8bPush(buf, p)`   | append element by ptr    |
| Pop             | `u8bPop(buf)`       | remove last element      |
| Shed N          | `u8bShed(buf, n)`   | trim N from DATA end     |
| Reset           | `u8bReset(buf)`     | clear PAST and DATA      |
| Shift           | `u8bShift(buf, n)`  | move DATA, set PAST to n |

##  Creation and memory

```c
a_pad(u8, buf, 1024)        // stack buffer (common case)
u8bAlloc(buf, len)          // heap allocate
u8bFree(buf)                // heap free
u8bMap(buf, len)            // mmap anonymous
u8bUnMap(buf)               // munmap
u8bReserve(buf, len)        // ensure capacity (may realloc)
```

##  Example

```c
a_pad(u8, buf, 1024);
u8bFeed1(buf, 0x42);              // append byte
u8bFeed(buf, some_slice);         // append slice
size_t n = u8bDataLen(buf);       // check length
u8cs data = u8bDataC(buf);        // get data as const slice
```

Note that Feed- and Drain- functions move slice borders, so no
separate signaling of lengths and consumed counts is ever needed.

##  Why buffers?

ABC discourages small heap allocations and pointer-heavy structures.

  - Pointer overhead: if payload is 64 bits and you need two pointers
    per node, overhead exceeds data.
  - `malloc/free` bookkeeping is expensive.
  - Scattered heap data has no locality.
  - Serialization of pointer structures is complex.

Buffer-based structures (see [BIN][B], [HEAP][H], [HASH][D], [LSM][L])
allocate once, keep data contiguous, and serialize trivially.

##  Buffers as arenas

This use case removed most of need to use malloc() in quite a many
application types. Non-owning slices are safe to use either when
they carve memory from a container that is locally allocated, so
its lifespan is obvious, or when passed in as a function argument,
which case is also self-evident. Any horizontal slice passing is
highly discouraged. In general, ABC implies processes-exchanging-
messages CSP model of concurrency. As C has no runtime, processes
are very cheap, so the only one thing needed for a 80/20 solution
is arena allocation. That is very much hardwired into buffer API.

  - `T##bAlign(arena)` ⇒ `T##gp` — collapse DATA into PAST, align IDLE
    up to `_Alignof(T)`, return a typed gauge over the aligned IDLE.
    Padding lands in PAST.  Cannot fail.
  - `T##bAcq(arena, ren)` — snapshot current DATA into `T##cs ren`,
    then collapse DATA into PAST so the slice survives.
  - `T##bAren(arena, ren, orig)` ⇒ `ok64` — one-shot rental: append
    `orig` (a `T##csc`) with one bounds-checked memcpy, alignment
    padding folded into PAST.  Returns `BNOROOM` on overflow.

The declarative macros in `B.h` wrap these into one-liners (see the
`a_*` convention in `S.h`: type first, declared name second):

    a_lign(T, gauge, arena);     // T##gp gauge = T##bAlign(arena)
    a_cq  (T, slice, arena);     // T##cs slice = {}; T##bAcq(...)
    a_rent(T, news,  arena, orig); // T##cs news  = {}; call(T##bAren, ...)
    a_ren (   news,  arena, orig); // u8 shorthand for a_rent

Example workflow — interleave a u8 string, a typed u32 sequence built
incrementally, and a one-shot rental, all in one arena:

    u8b arena;
    call(u8bAllocate, arena, 4096);

    // u8 string, one shot
    a_ren(stored_str, arena, $u8str("hello"));

    // u32 sequence, built piecewise (alignment auto-handled)
    a_lign(u32, g, arena);
    for (u32 i = 0; i < N; ++i) call(u32gFeed1, g, table[i]);
    a_cq(u32, stored_toks, arena);

    // tok32 one-shot copy of a precomputed slice
    a_rent(tok32, stored_anno, arena, src_toks);

All three rentals coexist in PAST; every earlier slice stays valid as
later ones land.  `u8bReset(arena)` reclaims the whole buffer.

`a_rent` / `a_ren` use `call()`, so the caller must be inside a
`sane()`'d function (see `PRO.h`).  `a_lign` / `a_cq` are infallible.

For interning **paths** specifically, `abc/PATH.h` adds two
NUL-preserving siblings of the trio:

  - `PATHu8bAren` / `a_ren_path` — one-shot path rental.  Writes the
    copy, writes a trailing NUL, parks both in PAST so the rented
    `u8cs` stays NUL-terminated across subsequent rentals.
  - `PATHu8bAcq` / `a_cq_path` — path-flavored close for the
    multi-feed cycle.  Use after `u8bAlign(arena)` and any number of
    `u8bFeed` calls (e.g. when composing a refname via `GITFeedRef`
    or path segments by hand); seals the slice with a NUL parked
    just past it in PAST.

Plain `u8bAren` / `u8bAcq` do not preserve a terminator; chained
rentals will clobber the byte at `*ren[1]`.


[S]: ./S.md
[B]: ./BIN.md
[D]: ./HASH.md
[H]: ./HEAP.md
[L]: ./LSM.md
[N]: ./NEST.md
