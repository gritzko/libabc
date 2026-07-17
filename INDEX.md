#   abc — module index

abc is the foundation C dialect (slices, buffers, no pointer arithmetic; see [README.md], [ABC.md]). Values live in `[head, term)` slices or four-pointer buffers; procedures return `ok64` via the `PRO.h` `call`/`try`/`done` cycle. Naming: `TypeZ(a,b)` is the `a<b` comparator, `Typehash`/`TypehashEq` hash key + key-eq, `*Feed`/`*Drain` serialize, `*Used`/`*Shed`/`*Fed` move borders. Deeper prose lives in the linked per-module `.md` docs.

##  Foundation

###  01.h — primitive types, bit ops, comparators

The bedrock: fixed-width integers, `ok64`, word unions, and free functions every other header builds on. No slices yet, just scalars.

 -  `u8`/`u16`/`u32`/`u64`/`i64`/`f64` and their `c`/`p` const/pointer aliases — the canonical scalar types.
 -  `ok64` — 64-bit error code, `0 == OK`.
 -  `w64`/`w128`/`w256`/`w512` (`u128`/`u256`/`u512`) — word unions giving `_64`/`_32`/`_16`/`_8` views of one blob.
 -  `u64bytelen`/`u64byte`/`u64setbyte`/`u64lowbytes` — minimal-width byte accounting for a u64.
 -  `clz64`/`ctz64`/`popc64`/`flip64`/`rotl64`/`round_power_of_2` — bit counting.
 -  `mix32`/`mix64` — multiplicative scramblers (PRIME constants) used as the default integer hashers.
 -  `u8Z`/`u16Z`/`u32Z`/`u64Z`/`i64Z`/`f64Z` — the per-type less-than comparators (`*a < *b`).
 -  `i64Zig`/`u64Zag` — zig-zag map between signed and unsigned, so small magnitudes get short varint encodings.
 -  `bitpack`/`bitpick`/`bitpack4816`/`bitpack4888` — pack/unpack named bit fields into a u64 (offset/id/type).

###  S.h, Sx.h — slices (the core abstraction)

A slice is `T *s[2]`, a `[head, term)` pointer pair; consuming a slice means moving a border, never copying. `Sx.h` is the template that generates the typed family (`u8s`, `u32s`, …). Full guide: [S.md].

 -  `u8s`/`u8cs`/`u8sc`/`u8csc` — the four const-shades of a slice (writable, consumable, immovable, immutable).
 -  `u8g` — a gauge `[3]`: two adjacent slices (left + rest) sharing a boundary, with no memory ownership.
 -  `u8sLen`/`u8sEmpty`/`u8sOK`/`u8sHead`/`u8sLast`/`u8sAt` — length, empties, validation and element access.
 -  `u8sUsed`/`u8sShed`/`u8sFed`/`u8sUsedAll` — the border movers: advance head (consume left).
 -  `u8sFeed`/`u8sFeed1`/`u8sFeedSome`/`u8sCopy`/`u8sDrain1` — write a slice or element into a writable slice and advance.
 -  `u8sFind`/`u8sFindS`/`u8csRevFind`/`u8csHasPrefix`/`u8csHasSuffix` — element/sub-slice search, prefix/suffix.
 -  `u8csEq`/`u8csIn`/`u8sFork`/`u8sJoin`/`u8csSub` — equality, containment.
 -  `a_pad`/`a$`/`a_dup`/`a_head`/`a_tail`/`a_rest`/`a_part` — stack-slice and sub-slice construction macros (type first).
 -  `$for`/`$rof`/`$eat` — forward/reverse/consuming iteration over a slice.

###  B.h, Bx.h, BUF.h — buffers and arenas

A buffer is four pointers splitting memory into PAST/DATA/IDLE; the verb that creates it (`Allocate`/`Map`/`Acquire`/`a_pad`) decides who frees it. An arena (`u8a`) dispenses buffers LIFO down the call tree. Guide: [B.md].

 -  `u8b`/`u8bp`/`u8a` — owned buffer, its pointer (pass buffers by pointer, never duplicate).
 -  `u8bAllocate`/`u8bMap`/`u8bFree` — heap- and mmap-backed buffer creation and release.
 -  `u8bData`/`u8bIdle`/`u8bDataLen`/`u8bIdleLen`/`u8bHasRoom` — slice views over DATA (the live payload) and IDLE (free).
 -  `u8bFeed`/`u8bFeed1`/`u8bShed`/`u8bReset`/`u8bShift`/`u8bSplice` — append to DATA, trim it, clear it.
 -  `u8aMark`/`u8aRewind`/`u8aReset` — save/pop/wipe an arena's dispense point.
 -  `u8bAlign`/`u8bAcq`/`u8bAren` — the slice-rental trio: align IDLE, snapshot DATA into a surviving `u8cs`.
 -  `Balloc`/`Brealloc`/`Breserve`/`Bfree` — the untyped (`Bvoid`) backing-memory ops the typed `bAllocate` use.
 -  `range32`/`match32` + `range32Z`/`match32Z` — byte-range and haystack/needle-range records with comparators.
 -  `u8bPrintf`/`u8bWantIdleLen`/`u8bWantDataLen` (BUF.h) — formatted append + grow-on-demand helpers over a buffer.

###  OK.h, PRO.h, TEST.h — errors, call discipline, tests

`ok64` codes are human-readable Base64 names; `PRO.h` turns them into stack-traced control flow. `PRO.h` must never be included by a header (it pollutes the namespace). Guide: [PRO.md].

 -  `ok64str`/`OKprint`/`OKscan`/`ok64is` — render an `ok64` to its Base64 name, parse one back.
 -  `sane(c)` — function entry guard: declares the hidden status var and bails `FAILSANITY` if the precondition fails.
 -  `call(f, …)`/`try(f, …)`/`done`/`fail(code)` — invoke a procedure, return-on-error or accumulate-and-continue.
 -  `ABC_BASS` — the thread-local LIFO scratch arena set up by the entry macros.
 -  `a_carve`/`a_cquire`/`a_rent`/`a_ren`/`a_lign` — BASS-implicit scratch acquisition (no explicit arena arg).
 -  `MAIN(f)`/`TEST(f)`/`FUZZ(f)` — entry-point macros that declare the `PRO.h` globals and initialise BASS.
 -  `TEST`/`want`/`same`/`testeq` (TEST.h) — table-driven test scaffolding.
 -  `SAN.h` — undefines the abc macros (`call`, `done`, `sane`, …) so a translation unit can hand the names back to non-abc.

###  INT.h — integer slices and fixed-width (de)serialization

Typed integer slice helpers plus little-endian put/get of fixed-width ints into byte slices — the raw layer under ZINT/TLV. Guide: [INT.md].

 -  `u8sFeed8`/`u8sFeed16`/`u8sFeed32`/`u8sFeed64` and `u8sDrain8…64` — read/write a fixed-width LE integer.
 -  `u64hash`/`u32hash`/`u64hashEq` — the integer hashers and key-equality used by scalar-keyed `HASHx`.
 -  `u128Z`/`u256Z`/`u512Z` — multi-word less-than comparators (big-endian digit order) for the wide word types.
 -  `a_u64cs`/`a_u32cs`/`a_i64cs` — make a const integer slice from a static C array.

##  Encoding & serialization

###  HEX.h, HEXx.h — hexadecimal

Streaming bin↔hex over slices, plus a fast validity gate. `HEXx.h` instantiates the typed wrappers for a record type.

 -  `HEXu8sFeedSome`/`HEXu8sDrainSome` — stream bytes→hex and hex→bytes, advancing both slices; the core converters.
 -  `HEXu8sValid` — YES iff every byte is a hex digit.
 -  `u64hexfeed`/`u64hexdrain` — minimal-width hex render / parse of a single u64.

###  RON.h — RON Base64 and ron60 timestamps

`ron60` is a 60-bit value in RON's sortable Base64 alphabet; it backs both error codes and logical timestamps.

 -  `ron60` — a u64 holding a 60-bit RON Base64 word.
 -  `RONutf8sFeed`/`RONutf8sDrain`/`RONu8sFeedPad`/`RONVerify` — encode/decode a value to/from RON64 text.
 -  `ron60Norm`/`ron60DeNorm`/`ron60Inc`/`ron60Ink` — left-align (pad) and right-align (strip) a short ron60.
 -  `ron60Z` — comparator that orders ron60s by their normalized (left-aligned) form.
 -  `RONOfTime`/`RONToTime`/`RONNow` — convert `struct tm`+ms ↔ a 10-digit ron60 timestamp; `RONNow` reads the wall clock.

###  ZINT.h — variable-length integers

Canonical 1/2/4/8-byte little-endian ints plus delta and blocked codings; shorter values produce shorter encodings. Guide: [ZINT.md].

 -  `ZINTu64feed`/`ZINTu64drain`/`ZINTlen` — encode/decode a u64 to its minimal canonical width.
 -  `ZINTu8sFeed128`/`ZINTu8sDrain128`/`ZINT128len` — the (big, lil) u128 pair codec, length-prefix-free and canonical.
 -  `ZINTu8sFeedInt`/`ZINTu8sFeedFloat` (+ Drain) — zig-zag signed and bit-flipped float codecs layered on the u64.
 -  `ZINTu64sDelta`/`Undelta`/`ZINTu8sFeedBlocked`/`…Drain` — delta-transform a slice + pack a run as blocked varints.
 -  `ZINTu128Z`/`ZINTi64Z`/`ZINTf64Z` — comparators that decode two encoded slices and order by decoded value.

###  TLV.h — type-length-value records

Tagged self-describing records with short (inline-length) and long (streamed) framings; the wire format for logs and stores. Guide: [TLV.md].

 -  `TLVu8sFeed`/`TLVu8sDrain` — write/read one `(type, value)` record into a slice.
 -  `TLVu8bInto`/`TLVu8bOuto` — open a long record on a buffer (reserve the header) and close it once the payload is.
 -  `TLVInitShort`/`TLVInitLong`/`TLVEndAny` — header-open/finalize for a record of yet-unknown length.
 -  `TLVFeedKeyVal`/`TLVDrainKeyVal` — key/value record convenience codec.
 -  `TLVhuge`/`TLVlong`/`TLVshort`/`TLVlen` — classify a type byte's framing class + compute a payload's framed length.

###  NUM.h — numbers to English words

Renders a u64 as English words ("one hundred twenty-three").

 -  `NUMu8sFeed`/`NUMu8sFeedSmall`/`NUMLen` — append a number as words into a slice, the sub-thousand helper.

##  Hashing & crypto

###  SHA.h — SHA-256 (libsodium)

The `sha256` record plus one-shot and streaming hashing.

 -  `sha256` — a 32-byte hash record; `sha256empty` tests the all-zero value.
 -  `SHASum` — one-shot hash of a slice into a `sha256`.
 -  `SHAOpen`/`SHAFeed`/`SHAClose` (`SHAstate`) — streaming hash: init, feed slices, finalize into a `sha256`.
 -  `sha256Z`/`sha256eq` — the ordering comparator (memcmp `< 0`) and the equality test over the 32 bytes.

###  NACL.h — Ed25519 signatures & BLAKE2b (libsodium)

Public-key signing and a fast keyed hash.

 -  `edpub256`/`edsec512`/`edsig512` — Ed25519 public key, secret key and signature record types (word unions).
 -  `NACLed25519create`/`NACLed25519sign`/`NACLed25519verify` — keypair gen, detached-sign a `sha256`.
 -  `NACLBlakeInit`/`NACLBlakeUpdate`/`NACLBlakeFinal` (`blake0`/`blake256`) — streaming BLAKE2b hashing.

###  RAP.h, rapidhash.h — fast non-cryptographic hashing

A rapidhash wrapper for hash tables and bloom-style keying.

 -  `RAPHash`/`RAPHashSeed` — hash a slice (balanced speed/quality), with an optional seed.
 -  `RAPMicro`/`RAPNano` (+`Seed`) — cache-tuned and embedded-tuned variants, faster for small inputs.

###  KV.h — key/value pair records

`(key,val)` structs that plug into the hash and sort templates.

 -  `kv32`/`kv64` — 32- and 64-bit key/value pair records.
 -  `kv32hash`/`kv64hash`/`kv32hashEq` — key hasher and key-equality.
 -  `kv32Z`/`kv64Z` — comparators ordering pairs by `key` (value ignored).

##  Containers

###  HASH.h, HASHx.h — open-addressing hash table

A bucketed linear-probe table laid out directly in a buffer slice; the length MUST be a power of two. Guide: [HASH.md].

 -  `HASHGet`/`HASHPut`/`HASHDel`/`HASHfind` — point lookup, insert/upsert, backward-shift delete.
 -  Each instantiating type supplies `Typehash` + `TypehashEq`; `HASHx` keys and probes through those, never the name.
 -  `ABC_HASH_LINE`/`ABC_HASH_CONVERGE` — compile-time knobs for probe-line width and Robin-Hood-style convergence on insert.

###  HEAPx.h — binary heap

In-place binary heap over a slice or buffer; the comparator is either the type's default `Z` or an explicit `z` argument. Guide: [HEAP.md].

 -  `HEAPPush`/`HEAPPop`/`HEAPPushZ`/`HEAPPopZ` — push/pop on a buffer-backed heap.
 -  `sUp`/`sDown`/`sHeap`/`sEjectAtZ` — sift-up, sift-down, heapify a slice, and remove an interior element.
 -  `sTops`/`sTopsZ` — gather the run of elements equal to the minimum (the primitive behind multiway-merge "all).

###  LIST.h, LISTx.h — intrusive doubly-linked list

Index-based prev/next links living inside buffer records (no pointers).

 -  `list64` — a `{u32 prev, u32 next}` link cell embedded in a record.
 -  `LISTfor`/`LISTnext`/`LISTat` — iterate and dereference list nodes by buffer index.

###  BIN.h — logarithmic interval bins

`bin64` numbers each aligned `[offset·2^L, …)` interval in the tail111 encoding (RFC7574 Merkle-tree binmap layout). Guide: [BIN.md].

 -  `bin64`/`bin64of`/`bin64empty` — the bin number type, constructor from `(level, offset)`, and the empty sentinel.
 -  `bin64level`/`bin64offset`/`bin64head`/`bin64term`/`bin64size` — decode a bin's level, offset, byte range.
 -  `bin64parent`/`bin64sibling`/`bin64son`/`bin64contains`/`bin64find_peak` — tree navigation and containment.
 -  `BINPeaks`/`BINPath` — list the peak bins covering a length, and the sibling path from a bin up to the covering peak.
 -  `bin64Z` — numeric ordering comparator on bin numbers.

###  BOX.h, BOXx.h — leveled overflow box

A fixed multi-level staging container that flushes to a save slice when a level fills (paired with sorted output).

 -  `BOXOpen`/`BOXFeed1`/`BOXFlush`/`BOXClose` — open a box over a range, push records, flush a full level.
 -  `BOXLevels` — compute the per-level capacities for a byte budget.

###  MSET.h, MSETx.h — merge-set / multiway iterator

A heap of sorted runs presenting a single merged, deduplicated stream; seekable. The `Z`/`z` argument is the run comparator.

 -  `MSETStart`/`MSETStartZ`/`MSETNext`/`MSETMerge` — initialize the merge over a stack of runs, step it.
 -  `MSETSeek`/`MSETSeekZ`/`MSETTopZ`/`MSETAdvZ` — seek to a key and advance past the current equal-tops group.

###  BIT.h — `u1` bitmap (bit addressing over u64 words)

A bit VALUE (`u1`, carried as a b8 0/1; no addressable `u1*`) plus bit addressing layered **directly over the real u64 families** (INT.h): a map IS a `u64s`/`u64cs` view or an owned word buffer `u1b` (== `u64b`) — no bespoke struct. Bit i lives in word `i>>6`, bit `i&63`, LSB-first within the LE word (matches the old `BitAt`). Set algebra is word-parallel; lengths are whole words (`u1sLen` == words*64). Replaces the ad-hoc `BitAt`/`BitSet`/`BitUnset` (BUF.h) — `u1sClr` is `&= ~mask`, so the broken `BitUnset` `|= ~(1<<bit)` cannot be expressed.

 -  `u1At`/`u1sSet`/`u1sClr`/`u1sPut`/`u1sLen`/`u1sConst` — bit get/set/clear/put over a `u64s`/`u64cs`, length (bits), const view of a writable map.
 -  `u1Words(bits)` — u64 words needed for `bits` bits.
 -  `u1bMap`/`u1bAcquire`/`u1bUnMap`/`u1bFree` — owned map creation/release (capacity in BITS, rounded to whole words; DATA spans the whole zeroed map so it is immediately bit-addressable).
 -  `u1bReset`/`u1bData`/`u1bDataC` — re-zero the map (keep size); writable / const word-slice views (use as `u64s` / `u64cs`).
 -  `u1sOr`/`u1sAnd`/`u1sAndNot`/`u1sXor` — word-parallel equal-WORD-length set algebra (`BITLEN` on length mismatch).
 -  `u1sCount`/`u1sNext`/`u1sEq`/`u1sAny` + `u1$for(i, set)` — popcount, next-set-bit, equality, non-empty, set-bit iterator.

##  Algorithms

###  QSORTx.h, SORT.h — sorting

Introsort with an inline comparator (no function-pointer overhead) plus binary search and dedup; SORT.h adds the merge-style u64 sort glue.

 -  `sSort`/`gSort`/`bSort` — introsort a slice/gauge/buffer in place using the type's inline `Z` comparator.
 -  `sBinSearch` — binary-search a `Z`-sorted slice; "match" is `!Z(a,b)&& !Z(b,a)`. Returns a pointer or NULL.
 -  `sDedup`/`gDedup`/`bDedup` — collapse adjacent equal runs in a sorted slice (the `SortAndDedup` pattern in [README.md]).
 -  `SORTu64`/`SORTu64x`/`SORTu64y`/`SORTu64z` — the slicer/merger/comparator trio (`x`/`y`/`z`) that drives.

###  DIFF.h, DIFFx.h — Myers diff

Linear-space Myers diff producing a run of `(op, len)` edit entries.

 -  `e32` + `DIFF_OP`/`DIFF_LEN`/`DIFF_ENTRY` — the packed edit entry (2-bit op in the top bits, 30-bit run length below).
 -  `DIFFWorkSize`/`DIFFEdlMaxEntries` — scratch and output sizing for a diff of two given lengths.
 -  `DIFFx` instantiates the typed `Diff` over a slice element type; `DIFF_D_BUDGET` caps the search before bail-out.

###  BSD.h — in-memory bsdiff/bspatch

Binary delta between two byte blobs, no compression, no malloc.

 -  `BSDDiff`/`BSDPatch` — produce a patch from old→new, and apply a patch to old to reconstruct new.
 -  `BSDWorkSize`/`BSDPatchNewSize` — scratch sizing for a diff and the output size a patch will produce.

###  DIJK.h, DIJKx.h — Dijkstra shortest path

Shortest path over an implicit graph using a `kv64` heap (by cost) and a `kv64` hash (by node id).

 -  `kv64Zval` — comparator ordering `kv64` entries by `val` (cost), giving a min-heap on distance.
 -  `DIJKNOPATH`/`DIJKNOROOM` — the no-route and out-of-scratch outcomes.

###  NFA.h, NFAx.h — Thompson NFA regex

Compile a pattern to a Thompson NFA and simulate it over text (full or prefix match), no backtracking.

 -  `NFAu8Compile` — compile a `u8cs` pattern into an NFA program (`nfau8g`), using a caller-supplied work area.
 -  `NFAu8Match`/`NFAu8MatchPrefix` — run the program over text, consuming all input or stopping at the first MATCH state.
 -  `NFAu8Class`/`NFAu8RAtom`/`NFAu8RCounted` — character-class and atom/counted-repeat fragment builders.

###  LSM.h, Yx.h, HITx.h — multiway merge iterators

`LSM` merges sorted record streams via a heap of cursors; `Yx` is the generic slicer/merger glue; `HITx` is a heap-of-iterators for set ops. Guides: [LSM.md], [HIT.md].

 -  `LSM`/`LSMMore`/`LSMNext`/`LSMMerge`/`LSMSort` — feed a sorted run, step the merge, drain it.
 -  `HITMerge`/`HITIntersect`/`HITSeek`/`HITSeekRange`/`HITTops` — union, intersection.
 -  `Yx` (`_X(push)`/`_X(next)`/`_X(merge)`/`_X(sort)`) — the lower-level push/merge primitives the `SORT`/`LSM` users.

##  Storage & I/O

###  MMAP.h, MMAPx.h — memory-mapped buffers

mmap-backed `Bvoid` buffers with grow-by-remap.

 -  `MMAPopen`/`MMAPclose`/`MMAPresize` — map a buffer of a size, unmap, and resize (remap) it.
 -  `MMAPresize2`/`MMAPresize4`/`MMAPmayresize` — double, grow-by-quarter.

###  PAGE.h, PACK.h, COMB.h — paged & packed storage

Lazy-loaded paged buffers (`PAGE`), LZ4-compressed page storage (`PACK`), and a self-describing combined-buffer header (`COMB`). Guide: [COMB.md].

 -  `page`/`pagef`/`PAGECreate`/`PAGEClose` — a paged buffer with a per-page load/flush callback and read/write-progress.
 -  `pack`/`PACKIdxLen`/`PACKIdxSetLen` — LZ4 page store with a 32-byte index block tracking 12 pages' compressed lengths.
 -  `COMBinit`/`COMBsave`/`COMBload` — stamp a magic header and persist/restore a buffer's PAST/DATA/IDLE offsets.

###  SST.h, SKIP.h, SLOG.h — sorted tables & skip logs

Sorted-string tables with a skip index (`SST`/`SKIP`), and a skip-record log for seekable append streams (`SLOG`). Guides: [SST.md], [SKIP.md], [SLOG.md].

 -  `SSTheader`/`SSTab` — the 16-byte SST file header and the skip table type aliased to `SKIPu8tab`.
 -  `SKIPfeed`/`SKIPdrain`/`SKIPload`/`SKIPhop` — build a block-granular skip index over a buffer and hop through it.
 -  `SLOGCreate`/`SLOGFeed`/`SLOGMark`/`SLOGClose` — write path: emit a stream with rank-based skip records + a close record.
 -  `SLOGOpen`/`SLOGSeek` — read path: load the skip list from the close record and seek to the first entry `≥` a target.
 -  `SLOGu8sFeedSkips`/`SLOGu8bDrainSkips` — the delta+ZINT codec for a skip record's offset list.

###  FILE.h, FSW.h, PATH.h, MIME.h — filesystem

POSIX file/dir wrappers returning `ok64` (errno mapped to `FILE*` codes), path manipulation, fs-watching, and MIME lookup. Guides: [FILE.md], [FSW.md].

 -  `FILEOpen`/`FILECreate`/`FILEClose`/`FILEStat`/`FILEResize`/`FILERename` — open/create/close/stat/resize/rename.
 -  `FILEDrain`/`FILEFeed`/`FILEFeedv`/`FILEEnsureSoft`/`FILEEnsureHard` — read into a slice.
 -  `FILEScan`/`FILEScanSorted`/`FILEIterOpen`/`FILENext` — directory walk (callback or iterator form).
 -  `path8s`/`path8b`/`PATHu8sBase`/`PATHu8sDir`/`PATHu8sExt`/`PATHu8sDrain` — NUL-terminated path slice/buffer types.
 -  `FSWInit`/`FSWDir`/`FSWPoll`/`FSWDrain` — inotify-style filesystem watcher: watch a dir, poll.
 -  `MIMEByExt`/`MIMEByPath` — map a file extension or path to its MIME type string (`MIMEdefault` otherwise).

###  ROCK.h, ROCKMERGE.h — RocksDB wrapper

A thin `ok64`-returning wrapper over the RocksDB C API, with an overflow-safe merge-operator sizing helper (rocksdb-free, so it is testable).

 -  `ROCKOpen`/`ROCKOpenRO`/`ROCKOpenMerge`/`ROCKClose` — open (rw / read-only / merge-operator) and close a database.
 -  `ROCKGet`/`ROCKPut`/`ROCKDel`/`ROCKSetMerge` — point get/put/delete and install a `u8ys` merge operator.
 -  `ROCKmergeTotal`/`ROCKmergeOnStack`/`ROCKmergeCap` — overflow-checked size accounting for a merge of N operands.

##  Networking

###  NET.h, TCP.h, UDP.h, DNS.h — sockets & addresses

Address parsing/resolution and thin TCP/UDP socket verbs; a DNS message codec for resolver work.

 -  `NETaddr`/`NETInfo`/`NETResolve`/`NETFreeAddress` — address buffer (host/port views), text↔raw conversion.
 -  `TCPListen`/`TCPConnect`/`TCPAccept`/`TCPClose` — TCP server/client socket lifecycle (connect has a non-blocking).
 -  `UDPBind`/`UDPConnect`/`UDPFeed`/`UDPDrain`/`UDPClose` — UDP bind/connect, datagram send/receive (with peer `NETaddr`).
 -  `DNS_QR`/`DNS_OP_*`/`DNS_RC_*` — DNS header flag.

###  POL.h, POLL.h, CURL.h — event loop & HTTP client

An fd+timer event poller (`POL`), its read/write buffer layer (`POLL`), and an async libcurl wrapper (`CURL`).

 -  `poller`/`POLInit`/`POLLoop`/`POLStop`/`POLSleep` — set up, run, and stop the event loop.
 -  `POLTrackEvents`/`AddEvents`/`Events`/`IgnoreEvents`/`TrackTime`/`AddTime` — register/read an fd's interest set + timer callbacks.
 -  `CURLInit`/`CURLGet`/`CURLPost`/`CURLTick`/`CURLFree` — async HTTP GET/POST driven by callbacks.

###  URI.h, URI.rl.h — URI parsing

A `uri` is eight slice views into the original text (scheme … fragment); walk path segments via `PATH`. Guide: [URI.md].

 -  `uri` — the parsed struct: `data` (the original text) plus the eight component slices.
 -  `URIutf8Drain`/`URIutf8Feed`/`URIutf8FeedSafe`/`URILexer` — parse text into a `uri`, serialize one back.
 -  `URIRelative`/`URIAbsolute` — resolve a relative reference against a base and the inverse (compute the relative form).
 -  `URIu8sEsc`/`URIu8sUnesc` — percent-encode and decode a component.

###  HTTP.h, JSON.h (+ .rl) — HTTP & JSON parsers

Ragel-generated SAX-style streaming lexers; the `.rl.h` files are the generated lexer enums/callbacks. Guide: [JSON.md].

 -  `HTTPLexer`/`HTTPutf8Drain`/`HTTPutf8Feed`/`HTTPfind` — drive the HTTP lexer over a stream, look up a header by key.
 -  `JSONLexer`/`JSONfmtInit`/`JSONFmt` — the JSON SAX lexer and a re-indenting pretty-printer.
 -  `JSONEscape`/`JSONUnEscape`/`JSONEscapeAll`/`JSONUnEscapeAll` — escape/unescape JSON text ↔ raw bytes.

##  Text & terminal

###  UTF8.h — UTF-8

Codepoint encode/decode and validation over slices.

 -  `cp32`/`utf8s`/`utf8cs` — a 32-bit codepoint and the UTF-8 byte-slice types.
 -  `utf8sFeed32`/`utf8sDrain32`/`utf8sValid` — encode/decode one codepoint and validate a whole slice.
 -  `utf8CPLen`/`UTF8_LEN`/`utf8sDrain1utf8` — count codepoints, the lead-byte→length table, and copy one whole character.

###  ANSI.h, TTY.h — terminal styling

ANSI CSI parsing (`ANSI`) and styled-output rendering with RGB/256/basic colour, padding and trimming (`TTY`).

 -  `ANSIu8sDrainCSI` (`csip`) — parse one ANSI CSI escape sequence from input into a structured `csi`.
 -  `ANSIIsTTY`/`ANSISetTTY`/`ANSIBgColor` — cached `isatty(stdout)`; OSC 11 background-colour probe over `/dev/tty` (briefly raw).
 -  `ANSIRaw`/`ANSICook`/`ANSITtySize` — terminal control (JS-053): stateless raw-mode enter (returns the saved termios bytes) / restore / `TIOCGWINSZ`, sharing the raw-mode dance with `ANSIBgColor`; `ANSIOpenPty`/`ANSISetSize` are pty test support.
 -  `tty64`/`TTYutf8sFeed` — a packed style word + the renderer emitting styled UTF-8 text (colour, attributes, pad/trim).
 -  `TTYansifeed`/`TTYrgbfeed`/`TTYresetfeed` — emit basic-colour, RGB, and reset escape sequences.

###  LEX.h — Ragel lexer generator

The shared lexer framework and a code generator that emits typed lexer templates (C / Go). Guide: [LEX.md].

 -  `LEXstate`/`LEXLexer` — the lexer state record and the driver shared by the generated `*.rl.h` lexers.
 -  `LEX_TEMPL` — the table of per-language lexer source templates the generator instantiates.

###  NEST.h — nested-context tracking

Tracks open/close of nested markup or container contexts in a buffer, with splice-on-close rendering. Guide: [NEST.md].

 -  `NESTSplice`/`NESTSpliceAll`/`NESTSpliceAny` — splice (close) a context variable: one, all, any.
 -  `NESTFeed`/`NESTRender` — feed literal content into the current context and render the accumulated nested structure.

###  ABC.h — umbrella header

`#include "ABC.h"` pulls in the major abc modules at once. It is a convenience aggregator; pull individual headers when you can.

[README.md]: ./README.md
[ABC.md]: ../ABC.md
[S.md]: ./S.md
[B.md]: ./B.md
[AREN.md]: ./AREN.md
[PRO.md]: ./PRO.md
[INT.md]: ./INT.md
[HEX.md]: ./HEX.md
[ZINT.md]: ./ZINT.md
[TLV.md]: ./TLV.md
[HASH.md]: ./HASH.md
[HEAP.md]: ./HEAP.md
[BIN.md]: ./BIN.md
[LSM.md]: ./LSM.md
[HIT.md]: ./HIT.md
[COMB.md]: ./COMB.md
[SST.md]: ./SST.md
[SKIP.md]: ./SKIP.md
[SLOG.md]: ./SLOG.md
[FILE.md]: ./FILE.md
[FSW.md]: ./FSW.md
[URI.md]: ./URI.md
[JSON.md]: ./JSON.md
[LEX.md]: ./LEX.md
[NEST.md]: ./NEST.md
