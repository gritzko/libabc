#include "PACK.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "PRO.h"
#include "TEST.h"

#define TEST_FILE "/tmp/pack_test.lz4"

ok64 PACKtestBasic() {
    sane(1);

    // Init PAGE registry
    call(PAGEInit, 8);

    pack pw = {};

    // Create pack for writing
    call(PACKCreate, &pw, TEST_FILE, PAGESIZE * 10);

    // Write some data directly to buffer
    u8p buf = pw.pg->buf[0];
    for (int i = 0; i < PAGESIZE * 3; i++) {
        buf[i] = (u8)(i & 0xFF);
    }
    // Update buffer pointer to reflect written data
    ((u8 **)pw.pg->buf)[2] = buf + PAGESIZE * 3;

    // Flush
    call(PACKFlush, &pw);

    // Verify data length after flush
    test(pw.datalen == 3 * PAGESIZE, PACKFAIL);

    // Write more data
    buf = pw.pg->buf[2];
    for (int i = 0; i < PAGESIZE + 100; i++) {
        buf[i] = (u8)((i + 0x33) & 0xFF);
    }
    ((u8 **)pw.pg->buf)[2] = buf + PAGESIZE + 100;

    // Close (flushes remaining)
    call(PACKClose, &pw);

    // Reopen for reading
    pack pr = {};
    call(PACKOpen, &pr, TEST_FILE);

    test(pr.datalen == 4 * PAGESIZE + 100, PACKFAIL);  // 3 + 1 full + 1 partial

    // Read first page
    call(PACKEnsure, pr.pg, NO, 0, PAGESIZE);
    test(PAGEIdxRead(pr.pg, 0) == PAGE_LOADED, PACKFAIL);

    // Verify data
    u8cp data = pr.pg->buf[0];
    for (int i = 0; i < PAGESIZE; i++) {
        test(data[i] == (u8)(i & 0xFF), PACKCORRUPT);
    }

    // Read page 3 (first of second batch)
    call(PACKEnsure, pr.pg, NO, PAGESIZE * 3, PAGESIZE);
    test(PAGEIdxRead(pr.pg, 3) == PAGE_LOADED, PACKFAIL);

    data = pr.pg->buf[0] + PAGESIZE * 3;
    for (int i = 0; i < PAGESIZE; i++) {
        test(data[i] == (u8)((i + 0x33) & 0xFF), PACKCORRUPT);
    }

    call(PACKClose, &pr);

    // Cleanup
    unlink(TEST_FILE);

    done;
}

ok64 PACKtestLargeFile() {
    sane(1);

    pack pw = {};

    // Create pack for 100 pages
    call(PACKCreate, &pw, TEST_FILE, PAGESIZE * 100);

    // Write pattern data
    u8p buf = pw.pg->buf[0];
    u64 buflen = pw.pg->buf[3] - pw.pg->buf[0];

    for (int round = 0; round < 10; round++) {
        // Fill buffer
        for (u64 i = 0; i < buflen; i++) {
            buf[i] = (u8)((round * 17 + i) & 0xFF);
        }
        ((u8 **)pw.pg->buf)[2] = buf + buflen;

        call(PACKFlush, &pw);
    }

    call(PACKClose, &pw);

    // Verify
    pack pr = {};
    call(PACKOpen, &pr, TEST_FILE);

    // Read random pages
    call(PACKEnsure, pr.pg, NO, PAGESIZE * 50, PAGESIZE);
    test(PAGEIdxRead(pr.pg, 50) == PAGE_LOADED, PACKFAIL);

    call(PACKEnsure, pr.pg, NO, PAGESIZE * 99, PAGESIZE);
    test(PAGEIdxRead(pr.pg, 99) == PAGE_LOADED, PACKFAIL);

    call(PACKClose, &pr);

    unlink(TEST_FILE);

    done;
}

ok64 PACKtestIndex() {
    sane(1);

    // Test index helper functions
    u64 block[4] = {0};

    // Set some lengths
    PACKIdxSetLen(block, 0, 100);
    PACKIdxSetLen(block, 5, 200);
    PACKIdxSetLen(block, 11, 300);

    test(PACKIdxLen(block, 0) == 100, PACKFAIL);
    test(PACKIdxLen(block, 5) == 200, PACKFAIL);
    test(PACKIdxLen(block, 11) == 300, PACKFAIL);

    // Test offset calculation
    block[0] = 1000;  // base offset
    for (int i = 0; i < 12; i++) {
        PACKIdxSetLen(block, i, 50 + i);
    }

    // Page 0 offset should be base
    u64 off0 = block[0];
    test(off0 == 1000, PACKFAIL);

    // Page 3 offset = base + len[0] + len[1] + len[2]
    // = 1000 + 50 + 51 + 52 = 1153
    u64 expected = 1000 + 50 + 51 + 52;
    u64 off3 = block[0];
    for (int i = 0; i < 3; i++) {
        off3 += PACKIdxLen(block, i);
    }
    test(off3 == expected, PACKFAIL);

    done;
}

// Count PAGE registry slots currently in use (buf[0] != NULL).
// A leaked PAGE keeps its slot occupied (and its mmap reachable from
// the global registry), so this is a deterministic, ASan-independent
// signal of a PAGE/index leak on a failed PACKCreate/PACKOpen.
static u32 PACKtestPagesUsed() {
    u32 n = 0;
    for (u32 i = 0; i < PAGE_REGISTRY_LEN; i++) {
        if (PAGE_REGISTRY[i].buf[0] != NULL) n++;
    }
    return n;
}

// MEM-015: PACKCreate must not leak the PAGE + index mmap when open()
// fails *after* both maps succeed.  Force open() failure with an
// unwritable path (parent dir does not exist); do NOT PACKClose the
// failed handle — a correct Create releases its own resources.
ok64 PACKtestCreateFail() {
    sane(1);

    u32 before = PACKtestPagesUsed();

    pack pw = {};
    // /nonexistent_dir_mem015/x.lz4 — open(O_CREAT) fails ENOENT,
    // after PAGECreate + idx mmap have already succeeded.
    __ = PACKCreate(&pw, "/nonexistent_dir_mem015/x.lz4", PAGESIZE * 10);
    test(__ != OK, PACKFAIL);  // Create must report the failure
    __ = OK;                   // consume the deliberate failure

    u32 after = PACKtestPagesUsed();
    // No PAGE slot may stay occupied: the PAGE (and its idx mmap) leaked
    // if this fails.  Also asserts the anonymous idx mmap is released
    // (it is unreachable once pw goes out of scope -> ASan would flag it).
    test(after == before, PACKFAIL);
    // Handle must be fully reset so a caller cannot double-free.
    test(pw.pg == NULL && pw.fd < 0 && pw.idx[0] == NULL, PACKFAIL);

    done;
}

// MEM-015: PACKOpen must not leak the fd + index mmap when a later
// step (here: read of a truncated/corrupt index) fails after the
// idx mmap succeeds.  Build a file whose trailer claims a valid-sized
// index but the file is too short to contain it, so the index read
// fails after mmap.
ok64 PACKtestOpenFail() {
    sane(1);

    u32 before = PACKtestPagesUsed();

    // Craft a file: trailer says datalen=PAGESIZE (npages=1), idxsize
    // matches PACKIdxSize(1), but file has no room for the index body,
    // so the index read at PACKOpen fails after the idx mmap succeeds.
    u64 datalen = PAGESIZE;
    u64 npages = 1;
    u64 idxsize = PACKIdxSize(npages);
    int fd = open(TEST_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    test(fd >= 0, PACKFAIL);
    // Layout: [pad 8B][trailer 16B] so fsize=24 (>16, passes the early
    // check) and the LAST 16B are the trailer.  idxoff = 24-16-idxsize
    // = -24 < 0 -> the index lseek fails *after* the idx mmap succeeds,
    // which is exactly the leak window we want to exercise.
    u64 pad = 0;
    test(write(fd, &pad, sizeof(pad)) == sizeof(pad), PACKFAIL);
    u64 trailer[2] = {datalen, idxsize};
    test(write(fd, trailer, sizeof(trailer)) == sizeof(trailer), PACKFAIL);
    close(fd);

    pack pr = {};
    __ = PACKOpen(&pr, TEST_FILE);
    test(__ != OK, PACKFAIL);  // Open must report the failure
    __ = OK;                   // consume the deliberate failure

    u32 after = PACKtestPagesUsed();
    test(after == before, PACKFAIL);
    test(pr.pg == NULL && pr.fd < 0 && pr.idx[0] == NULL, PACKFAIL);

    unlink(TEST_FILE);
    done;
}

// ABC-014: an untrusted trailer that lies about datalen must be bounded
// against the file.  A page holds >=1 compressed byte, so npages can never
// exceed the on-disk data area (fsize-16-idxsize).  Here the trailer claims
// 100 pages but the file has ZERO data area: pre-fix PACKOpen returned OK
// with a wild datalen (buf[2]=buf[0]+datalen); post-fix rejects PACKCORRUPT.
// (The same bound also guards the near-u64max ceil-div wrap.)
ok64 PACKtestTrailerWrap() {
    sane(1);

    u32 before = PACKtestPagesUsed();

    u64 datalen = PAGESIZE * 100;  // npages = 100
    u64 npages = 100;
    u64 idxsize = PACKIdxSize(npages);

    // File = [idxsize bytes fake index][16B trailer]; data area = 0.
    int fd = open(TEST_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    test(fd >= 0, PACKFAIL);
    u8 *idxbuf = calloc(idxsize, 1);
    test(idxbuf != NULL, PACKNOROOM);
    ssize_t iw = write(fd, idxbuf, idxsize);
    free(idxbuf);
    test(iw == (ssize_t)idxsize, PACKFAIL);
    u64 trailer[2] = {datalen, idxsize};
    test(write(fd, trailer, sizeof(trailer)) == sizeof(trailer), PACKFAIL);
    close(fd);

    pack pr = {};
    __ = PACKOpen(&pr, TEST_FILE);
    test(__ != OK, PACKFAIL);  // must reject: 100 pages, 0 data bytes
    __ = OK;

    u32 after = PACKtestPagesUsed();
    test(after == before, PACKFAIL);
    test(pr.pg == NULL && pr.fd < 0 && pr.idx[0] == NULL, PACKFAIL);

    unlink(TEST_FILE);
    done;
}

// ABC-014: PACKClose must free fd + PAGE + index mmap even when the
// flush/index/trailer write fails (ENOSPC).  Write to /dev/full so every
// write() returns ENOSPC; a leaked PAGE would keep its registry slot.
ok64 PACKtestCloseFail() {
    sane(1);

    // /dev/full is Linux/BSD-only; skip where absent (macOS)
    int probe = open("/dev/full", O_WRONLY);
    if (probe < 0) done;
    close(probe);

    u32 before = PACKtestPagesUsed();

    pack pw = {};
    call(PACKCreate, &pw, "/dev/full", PAGESIZE * 10);

    // Stage a full page so PACKClose actually writes (and thus fails).
    u8p buf = pw.pg->buf[0];
    for (int i = 0; i < PAGESIZE; i++) buf[i] = (u8)(i & 0xFF);
    ((u8 **)pw.pg->buf)[2] = buf + PAGESIZE;

    __ = PACKClose(&pw);
    test(__ != OK, PACKFAIL);  // write to /dev/full fails (ENOSPC)
    __ = OK;

    u32 after = PACKtestPagesUsed();
    test(after == before, PACKFAIL);  // no leaked PAGE slot
    test(pw.pg == NULL && pw.fd < 0 && pw.idx[0] == NULL, PACKFAIL);

    done;
}

ok64 PACKtest() {
    sane(1);
    call(PACKtestIndex);
    call(PACKtestBasic);
    call(PACKtestLargeFile);
    call(PACKtestCreateFail);
    call(PACKtestOpenFail);
    call(PACKtestTrailerWrap);
    call(PACKtestCloseFail);
    done;
}

TEST(PACKtest);
