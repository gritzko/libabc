#   ABC slices

An ABC slice is a pair of pointers: `[head, term)`. A slice is
consumed by moving `head` towards `term`. Four shades of const:

 1. `u8s` - writable slice, typically output/idle space
 2. `u8cs` - const-value slice, typically input to consume
 3. `u8sc` - immovable slice (const pointers), can edit values
 4. `u8csc` - fully immutable slice

Slices are passed by pointer; use `a_dup` to create a consumable copy.

##  `u8s` vs `u8sp` — consume vs assign

The two shapes have the same ABI (both decay to `u8 **`) but mark
different roles. Pick the type by what the function *does* to the
slice:

  - **`u8s` (slice consumed)** — the function reads from or writes
    into the slice, advancing `head` as it goes. The caller hands
    over a working slice and the function eats / fills it. Streaming
    helpers (`u8sFeed`, `HEXu8sFeedSome`, `SHA1u8sFeedHex`) are the
    canonical shape: they push bytes forward, never look back.
    Buffer callers pass `u8bIdle(buf)`; gauge callers pass
    `u8gRest(g)`; either yields the right slice cursor.
  - **`u8sp` (slice assigned)** — the function writes a fresh
    `(head, term)` pair into the two cells the pointer addresses.
    Use this for output parameters whose value is the slice itself
    (e.g. `*ren[0] = …; *ren[1] = …`), not the bytes inside it.
    `PATHu8bAcq`, `u8bAren` and friends are typical assigners.

Rule of thumb: if the body of the function does `s[0] += n` or
`u8sFeed(s, …)`, it's `u8s` (consumed). If the body does `out[0] =
…; out[1] = …`, it's `u8sp` (assigned). Prefer streaming helpers to
take `u8s` so they compose with both buffers and gauges at the call
site.

##  Typed functions (preferred)

Always prefer typed functions over generic macros:

| Operation      | Typed function     | Generic macro |
|----------------|-------------------|---------------|
| Length         | `u8sLen(s)`       | `$len(s)`     |
| Empty check    | `u8sEmpty(s)`     | `$empty(s)`   |
| Validate       | `u8sOK(s)`        | `$ok(s)`      |
| First element  | `u8sHead(s)`      | `**s`         |
| Last element   | `u8sLast(s)`      | `*$last(s)`   |
| At position    | `u8sAtP(s, i)`    | `s[0]+i`      |
| Consume N      | `u8sUsed(s, n)`   | `s[0]+=n`     |
| Trim end by N  | `u8sShed(s, n)`   | `s[1]-=n`     |
| Copy slice ptrs| `u8sMv(a, b)`     | `$mv(a, b)`   |
| Copy data      | `u8sCopy(to, fr)` | `$copy()`     |
| Feed data      | `u8sFeed(to, fr)` | `$feed()`     |
| Feed one       | `u8sFeed1(s, v)`  | `$feed1()`    |
| Find element   | `u8sFind(s, v)`   | -             |
| Find slice     | `u8sFindS(s, n)`  | -             |

Const variants use `cs`: `u8csLen()`, `u8csHead()`, etc.

##  Creation macros

```c
a_pad(u8, buf, 1024)        // stack buffer + slice helpers
a$(u8, s, array)            // slice from array
a_dup(u8, copy, orig)       // duplicate slice (same data)
a_head(u8, h, s, len)       // first len elements
a_tail(u8, t, s, len)       // last len elements
a_rest(u8, r, s, off)       // skip first off elements
```

##  Fork/Join pattern

Track writes into a slice without losing the original bounds:

```c
u8s idle = u8bIdle(buf);
u8s writer;
u8sFork(idle, writer);      // writer = copy of idle
// ... write into writer ...
u8sJoin(idle, writer);      // idle[0] = writer[0]
```

##  Iteration

```c
$for(u8, p, data) { ... }   // forward: for (u8 *p = ...)
$rof(u8, p, data) { ... }   // reverse
$eat(data) { **data ... }   // consume while iterating
```

##  Gauges

A gauge `g[3]` represents two adjacent slices sharing a boundary:
left `[0,1)` and rest `[1,2)`. Unlike buffers, gauges imply no
memory ownership.

```c
u8g g;
u8gOf(g, slice);            // left empty, rest = full slice
u8gFeed1(g, byte);          // append to left, shrink rest
u8gLeftLen(g);              // left slice length
u8gRestLen(g);              // rest slice length
```

See [B.md](./B.md) for buffers (`b[4]`), which own their memory
and should only be passed down the call stack by 
a pointer, never duplicated.
