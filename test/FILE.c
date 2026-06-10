#include "FILE.h"

#include <dirent.h>
#include <stdlib.h>

#include "INT.h"
#include "PRO.h"
#include "TEST.h"

#define TRACE fprintf(stderr, "  %s:%d\n", __func__, __LINE__)

ok64 FILEtest1() {
    sane(1);
    a_path(path, $cstr("/tmp"));
    a_cstr(tmpl, "FILEtest1_XXXXXX");
    call(PATHu8bAddTmp, path, tmpl);
    u8cs text = $u8str("Hello world!\n");
    int fd = 0;
    call(FILECreate, &fd, $path(path));
    call(FILEFeed, fd, text);
    call(FILEClose, &fd);
    call(FILEUnLink, $path(path));
    done;
}

ok64 FILEtest2() {
    sane(1);
    a_path(path, $cstr("/tmp"));
    a_cstr(tmpl, "FILEtest2_XXXXXX");
    call(PATHu8bAddTmp, path, tmpl);
    int fd = 0;
    call(FILECreate, &fd, $path(path));
    call(FILEResize, &fd, 4096);
    u8bp mapbuf = NULL;
    call(FILEMapFD, &mapbuf, &fd, PROT_READ | PROT_WRITE);
    testeqv((long long)(Bsize(mapbuf)), (long long)(4096), "%lld");
    Bat(mapbuf, 42) = 1;
    call(FILEUnMap, mapbuf);
    call(FILEMapRO, &mapbuf, $path(path));
    testeqv((long long)(Blen(mapbuf)), (long long)(4096), "%lld");
    testeqv((long long)(Bat(mapbuf, 41)), (long long)(0), "%lld");
    testeqv((long long)(Bat(mapbuf, 42)), (long long)(1), "%lld");
    call(FILEUnMap, mapbuf);
    done;
}

ok64 FILE3() {
    sane(1);
    a_path(path, $cstr("/tmp"));
    a_cstr(tmpl, "FILE3_XXXXXX");
    call(PATHu8bAddTmp, path, tmpl);
    a_cstr(text, "Hello world!");
    u8bp buf = NULL;
    call(FILEMapCreate, &buf, $path(path), PAGESIZE);
    u8bReset(buf);
    call(u8bFeed, buf, text);
    call(FILEUnMap, buf);
    u8bp buf2 = NULL;
    call(FILEMapRO, &buf2, $path(path));
    call(FILEUnMap, buf2);
    // nedo(FILEUnLink(path));
    done;
}

ok64 FILEtest4() {
    sane(1);
    a_path(path, $cstr("/tmp"));
    a_cstr(tmpl, "FILEtest4_XXXXXX");
    call(PATHu8bAddTmp, path, tmpl);
    a$str(one, "Hello");
    a$str(two, " beautiful");
    a$str(three, " world!");
    aBpad2(u8cs, queue, 4);
    call(u8cssFeed3, queueidle, one, two, three);
    int fd;
    call(FILECreate, &fd, $path(path));
    call(FILEFeedv, fd, queuedata);
    want($empty(queuedata));
    aBpad2(u8, back, 64);
    testeqv((long long)(0), (long long)(lseek(fd, 0, SEEK_SET)), "%lld");
    call(FILEdrainall, backidle, fd);
    a$str(correct, "Hello beautiful world!");
    $testeq(correct, backdata);
    done;
}

ok64 FILEtest5() {
    sane(1);
    // Test streaming I/O primitives
    a_path(path, $cstr("/tmp"));
    a_cstr(tmpl, "FILEtest5_XXXXXX");
    call(PATHu8bAddTmp, path, tmpl);
    a_cstr(testdata, "The quick brown fox jumps over the lazy dog");

    // Create test file
    int wfd;
    call(FILECreate, &wfd, $path(path));
    a_dup(u8 const, data, testdata);
    call(FILEFeedAll, wfd, data);
    call(FILEClose, &wfd);

    // Test FILEEnsureSoft
    int rfd;
    call(FILEOpen, &rfd, $path(path), O_RDONLY);
    aBpad2(u8, buf, 64);
    call(FILEEnsureSoft, rfd, bufbuf, 10);
    test(u8bDataLen(bufbuf) >= 10,
         BADPOS);  // Should have read at least 10 bytes

    // Test FILEEnsureHard
    call(FILEEnsureHard, rfd, bufbuf, 20);
    want(u8bDataLen(bufbuf) >= 20);  // Must have exactly 20 bytes

    // Test FILEEnsureHard with buffer too small (should fail)
    aBpad2(u8, smallbuf, 8);
    ok64 err = FILEEnsureHard(rfd, smallbufbuf, 100);
    want(err != OK);  // Should fail - buffer too small

    call(FILEClose, &rfd);

    // Test FILEFlushThreshold
    int wfd2;
    call(FILECreate, &wfd2, $path(path));
    aBpad2(u8, outbuf, 64);
    call(u8bFeed, outbufbuf, testdata);

    // Flush should not trigger (below threshold)
    call(FILEFlushThreshold, wfd2, outbufbuf, 100);
    want(u8bDataLen(outbufbuf) > 0);  // Still has data

    // Flush should trigger (above threshold)
    call(FILEFlushThreshold, wfd2, outbufbuf, 10);
    testeqv((long long)(u8bPastLen(outbufbuf)), (long long)(u8csLen(testdata)), "%lld");  // Data moved to past

    call(FILEClose, &wfd2);
    call(FILEUnLink, $path(path));
    done;
}

ok64 FILEtest6() {
    sane(1);
    // Test FILEMakeDir and FILERmDir (non-recursive)
    a_path(dirpath, $cstr("/tmp"));
    a_cstr(tmpl, "FILEtest6_XXXXXX");
    call(PATHu8bAddTmp, dirpath, tmpl);

    // Create directory
    call(FILEMakeDir, $path(dirpath));

    // Verify it exists
    filestat s = {};
    test(OK == FILEStat(&s, $path(dirpath)), FILEFAIL);
    test(s.kind == FILE_KIND_DIR, FILEFAIL);

    // Remove directory (non-recursive)
    call(FILERmDir, $path(dirpath), false);

    // Verify it's gone
    test(OK != FILEStat(&s, $path(dirpath)), FILEFAIL);

    done;
}

ok64 FILEtest7() {
    sane(1);
    // Test FILERmDir fails on non-empty directory when non-recursive
    a_path(dirpath, $cstr("/tmp"));
    a_cstr(tmpl, "FILEtest7_XXXXXX");
    call(PATHu8bAddTmp, dirpath, tmpl);

    // Create directory
    call(FILEMakeDir, $path(dirpath));

    // Create file inside
    a_cstr(fname, "file.txt");
    call(PATHu8bPush, dirpath, fname);
    int fd;
    call(FILECreate, &fd, $path(dirpath));
    call(FILEClose, &fd);
    call(PATHu8bPop, dirpath);

    // FILERmDir should fail on non-empty dir when non-recursive
    ok64 err = FILERmDir($path(dirpath), false);
    test(err != OK, FILEFAIL);

    // Clean up with recursive delete
    call(FILERmDir, $path(dirpath), true);

    // Verify it's gone
    filestat s = {};
    test(OK != FILEStat(&s, $path(dirpath)), FILEFAIL);

    done;
}

ok64 FILEtest8() {
    sane(1);
    // Test FILERmDir recursive on nested structure
    a_path(path, $cstr("/tmp"));
    a_cstr(tmpl, "FILEtest8_XXXXXX");
    call(PATHu8bAddTmp, path, tmpl);
    a_cstr(sub1, "sub1");
    a_cstr(sub2, "sub2");
    a_cstr(f1, "file1.txt");
    a_cstr(f2, "file2.txt");
    a_cstr(f3, "file3.txt");

    // Create nested structure: base/sub1/sub2
    call(FILEMakeDir, $path(path));
    call(PATHu8bPush, path, sub1);
    call(FILEMakeDir, $path(path));
    call(PATHu8bPush, path, sub2);
    call(FILEMakeDir, $path(path));
    call(PATHu8bPop, path);
    call(PATHu8bPop, path);

    // Create files
    int fd;
    call(PATHu8bPush, path, f1);
    call(FILECreate, &fd, $path(path));
    call(FILEClose, &fd);
    call(PATHu8bPop, path);

    call(PATHu8bPush, path, sub1);
    call(PATHu8bPush, path, f2);
    call(FILECreate, &fd, $path(path));
    call(FILEClose, &fd);
    call(PATHu8bPop, path);

    call(PATHu8bPush, path, sub2);
    call(PATHu8bPush, path, f3);
    call(FILECreate, &fd, $path(path));
    call(FILEClose, &fd);

    // Verify deepest file exists
    filestat s = {};
    test(OK == FILEStat(&s, $path(path)), FILEFAIL);

    // Back to base and delete recursively
    call(PATHu8bPop, path);
    call(PATHu8bPop, path);
    call(PATHu8bPop, path);
    call(FILERmDir, $path(path), true);

    // Verify it's gone
    test(OK != FILEStat(&s, $path(path)), FILEFAIL);

    done;
}

// Test FILERmDir with a_path full path (no path8gPush)
// Reproduces PUT.c cleanup pattern
ok64 FILEtest8b() {
    sane(1);

    // Step 1: create a temp dir using the normal path8g pattern
    a_path(setup, $cstr("/tmp"));
    a_cstr(tmpl, "FILEtest8b_XXXXXX");
    call(PATHu8bAddTmp, setup, tmpl);
    call(FILEMakeDir, $path(setup));

    // Create nested content
    a_cstr(sub, "sub");
    call(PATHu8bPush, setup, sub);
    call(FILEMakeDir, $path(setup));
    a_cstr(fname, "file.txt");
    call(PATHu8bPush, setup, fname);
    int fd;
    call(FILECreate, &fd, $path(setup));
    call(FILEClose, &fd);
    call(PATHu8bPop, setup);
    call(PATHu8bPop, setup);

    // Step 2: get the path as a C string, then use a_path to reconstruct
    // This is the pattern from PUT.c: a_path(path, g_tmpdir)
    char cpath[256];
    snprintf(cpath, sizeof(cpath), "%.*s",
             (int)u8bDataLen(setup), (char *)setup[1]);

    a_path(repath, $cstr(cpath));
    call(PATHu8bTerm, repath);
    test($ok($path(repath)) && !$empty($path(repath)), FAIL);

    // Now remove with FILERmDir
    call(FILERmDir, $path(repath), true);

    // Verify it's gone
    filestat s = {};
    test(OK != FILEStat(&s, $path(repath)), FILEFAIL);

    done;
}

// Test FILEerrno translation
ok64 FILEtest9() {
    sane(1);


    // FILEStat on non-existent file should return FILENONE
    a_path(nofile, $cstr("/tmp"));
    a_cstr(tmpl, "FILEtest9_XXXXXX");
    call(PATHu8bAddTmp, nofile, tmpl);
    filestat s = {};
    ok64 res = FILEStat(&s, $path(nofile));
    test(res == FILENONE, FILEFAIL);

    // Verify FILEerrno translates correctly
    test(FILEerrno(ENOENT) == FILENONE, FILEFAIL);
    test(FILEerrno(EACCES) == FILEACCES, FILEFAIL);
    test(FILEerrno(EEXIST) == FILEEXIST, FILEFAIL);
    test(FILEerrno(0) == OK, FILEFAIL);
    test(FILEerrno(9999) == FILEFAIL, FILEFAIL);  // unknown errno

    done;
}

// Test file iterator (into/next/outo pattern)
ok64 FILEIterTest() {
    sane(1);

    // Create test directory structure
    a_path(base, $cstr("/tmp"));
    a_cstr(tmpl, "FILEIterTest_XXXXXX");
    call(PATHu8bAddTmp, base, tmpl);
    call(FILEMakeDir, $path(base));


    // Create files and subdirs
    a_cstr(f1, "file1.txt");
    a_cstr(f2, "file2.txt");
    a_cstr(sub, "subdir");
    a_cstr(f3, "nested.txt");

    int fd;
    call(PATHu8bPush, base, f1);
    call(FILECreate, &fd, $path(base));
    call(FILEClose, &fd);
    call(PATHu8bPop, base);

    call(PATHu8bPush, base, f2);
    call(FILECreate, &fd, $path(base));
    call(FILEClose, &fd);
    call(PATHu8bPop, base);

    call(PATHu8bPush, base, sub);
    call(FILEMakeDir, $path(base));
    call(PATHu8bPush, base, f3);
    call(FILECreate, &fd, $path(base));
    call(FILEClose, &fd);
    call(PATHu8bPop, base);
    call(PATHu8bPop, base);


    // Now test iterator
    int file_count = 0;
    int dir_count = 0;
    fileit it = {};
    call(FILEIterOpen, &it, base);
    scan(FILENext, &it) {
        if (it.type == DT_REG) file_count++;
        if (it.type == DT_DIR) {
            dir_count++;
            // Recurse into subdir
            fileit child = {};
            call(FILEInto, &child, &it);
            scan(FILENext, &child) {
                if (child.type == DT_REG) file_count++;
            }
            seen(END);
            call(FILEOuto, &child, &it);
        }
    }
    seen(END);
    call(FILEIterClose, &it);


    testeqv((long long)(file_count), (long long)(3), "%lld");  // file1.txt, file2.txt, nested.txt
    testeqv((long long)(dir_count), (long long)(1), "%lld");   // subdir

    // Cleanup
    call(FILERmDir, $path(base), true);

    done;
}

// Test sorted file iterator
ok64 FILEIterSortedTest() {
    sane(1);

    // Create test directory structure
    a_path(base, $cstr("/tmp"));
    a_cstr(tmpl, "FILEIterSorted_XXXXXX");
    call(PATHu8bAddTmp, base, tmpl);
    call(FILEMakeDir, $path(base));


    // Create files with names that sort differently than creation order
    a_cstr(fz, "zebra.txt");
    a_cstr(fa, "alpha.txt");
    a_cstr(fm, "middle.txt");
    a_cstr(sub, "beta_dir");
    a_cstr(fc, "charlie.txt");

    int fd;
    // Create in reverse alphabetical order
    call(PATHu8bPush, base, fz);
    call(FILECreate, &fd, $path(base));
    call(FILEClose, &fd);
    call(PATHu8bPop, base);

    call(PATHu8bPush, base, fm);
    call(FILECreate, &fd, $path(base));
    call(FILEClose, &fd);
    call(PATHu8bPop, base);

    call(PATHu8bPush, base, sub);
    call(FILEMakeDir, $path(base));
    call(PATHu8bPush, base, fc);
    call(FILECreate, &fd, $path(base));
    call(FILEClose, &fd);
    call(PATHu8bPop, base);
    call(PATHu8bPop, base);

    call(PATHu8bPush, base, fa);
    call(FILECreate, &fd, $path(base));
    call(FILEClose, &fd);
    call(PATHu8bPop, base);


    // Test sorted iterator
    aB(u8, sortbuf);
    call(u8bAllocate, sortbufbuf, 4096);

    fileit it = {};
    call(FILEIterOpenSorted, &it, base, sortbufbuf, FILEentryZ);


    // Collect entries in order
    u8 names[4][32] = {};
    int count = 0;
    scan(FILENext, &it) {
        // Extract just the filename (last component). Dir paths end with
        // '/' — back up over it before scanning for the separator.
        u8cp end = u8bIdleHead(it.path);
        u8cp head = u8bDataHead(it.path);
        if (end > head && *(end - 1) == '/') end--;
        u8cp p = end;
        while (p > head && *(p - 1) != '/') p--;
        size_t len = end - p;
        if (len < 32) {
            memcpy(names[count], p, len);
        }
        if (it.type == DT_DIR) {
            // Recurse - should inherit sorting
            fileit child = {};
            call(FILEInto, &child, &it);
            scan(FILENext, &child) {
                // Child iteration
            }
            seen(END);
            call(FILEOuto, &child, &it);
        }
        count++;
    }
    seen(END);
    call(FILEIterClose, &it);


    // Verify sorted order: alpha.txt, beta_dir, middle.txt, zebra.txt
    testeqv((long long)(count), (long long)(4), "%lld");
    testeqv((long long)(strcmp((char *)names[0], "alpha.txt")), (long long)(0), "%lld");
    testeqv((long long)(strcmp((char *)names[1], "beta_dir")), (long long)(0), "%lld");
    testeqv((long long)(strcmp((char *)names[2], "middle.txt")), (long long)(0), "%lld");
    testeqv((long long)(strcmp((char *)names[3], "zebra.txt")), (long long)(0), "%lld");

    // Cleanup
    call(u8bFree, sortbufbuf);
    call(FILERmDir, $path(base), true);

    done;
}

// Test FILEBook - booked VA range with growable file mapping
ok64 FILEBookTest() {
    sane(1);

    a_path(path, $cstr("/tmp"));
    a_cstr(tmpl, "FILEBookTest_XXXXXX");
    call(PATHu8bAddTmp, path, tmpl);

    // Create with 1MB booked range, 4KB initial size
    u8bp buf = NULL;
    call(FILEBookCreate, &buf, $path(path), 1 * MB, 4 * KB);


    // Verify initial size
    size_t sp = sysconf(_SC_PAGESIZE);
    testeqv((long long)(Bsize(buf)), (long long)(roundup2(4 * KB, sp)), "%lld");

    // Save base address
    u8p base = buf[0];

    // Write some data
    u8bReset(buf);
    a_cstr(text, "Hello booked world!");
    call(u8bFeed, buf, text);

    // Extend to 64KB
    call(FILEBookExtend, buf, 64 * KB);

    // Verify base address unchanged (the whole point!)
    testeqv((long long)(buf[0]), (long long)(base), "%lld");
    testeqv((long long)(Bsize(buf)), (long long)(roundup2(64 * KB, sp)), "%lld");

    // Verify data survived
    testeqv((long long)(memcmp(buf[1], "Hello booked world!", 19)), (long long)(0), "%lld");

    // Write more data at higher offset
    u8p far = buf[0] + 60 * KB;
    memcpy(far, "Far away data", 13);

    // Sync to disk
    call(FILEMSync, buf);


    // Extend again
    call(FILEBookExtend, buf, 128 * KB);

    testeqv((long long)(buf[0]), (long long)(base), "%lld");  // still same address

    // Verify far data survived
    testeqv((long long)(memcmp(buf[0] + 60 * KB, "Far away data", 13)), (long long)(0), "%lld");

    // Verify FILEIsBooked
    testeqv((long long)(FILEIsBooked(buf)), (long long)(YES), "%lld");

    // Cleanup
    call(FILEUnBook, buf);

    call(FILEUnLink, $path(path));

    done;
}

// Test FILEBook with existing file
ok64 FILEBookExistingTest() {
    sane(1);

    a_path(path, $cstr("/tmp"));
    a_cstr(tmpl, "FILEBookExist_XXXXXX");
    call(PATHu8bAddTmp, path, tmpl);

    // Create file with some content first
    u8bp prebuf = NULL;
    call(FILEMapCreate, &prebuf, $path(path), PAGESIZE);

    u8bReset(prebuf);
    a_cstr(initial, "Initial content here");
    call(u8bFeed, prebuf, initial);
    call(FILEUnMap, prebuf);


    // Now book with larger range
    u8bp buf = NULL;
    call(FILEBook, &buf, $path(path), 1 * MB);


    // Verify existing content is there
    testeqv((long long)(memcmp(buf[0], "Initial content here", 20)), (long long)(0), "%lld");

    // Extend and write more
    call(FILEBookExtend, buf, 64 * KB);

    u8p far = buf[0] + 32 * KB;
    memcpy(far, "Extended data", 13);

    call(FILEUnBook, buf);
    call(FILEUnLink, $path(path));


    done;
}

// Test FILEBookExtend beyond booked range fails
ok64 FILEBookLimitTest() {
    sane(1);

    a_path(path, $cstr("/tmp"));
    a_cstr(tmpl, "FILEBookLimit_XXXXXX");
    call(PATHu8bAddTmp, path, tmpl);

    u8bp buf = NULL;
    call(FILEBookCreate, &buf, $path(path), 64 * KB, 4 * KB);


    // Extending beyond booked range should fail
    ok64 err = FILEBookExtend(buf, 128 * KB);
    test(err != OK, FILEFAIL);


    // But extending within range should work
    call(FILEBookExtend, buf, 32 * KB);


    call(FILEUnBook, buf);
    call(FILEUnLink, $path(path));


    done;
}

// Test FILEBookEnsure - auto-grow
ok64 FILEBookEnsureTest() {
    sane(1);

    a_path(path, $cstr("/tmp"));
    a_cstr(tmpl, "FILEBookEnsure_XXXXXX");
    call(PATHu8bAddTmp, path, tmpl);

    // Create with small initial size (1 page), large booked range
    u8bp book = NULL;
    call(FILEBookCreate, &book, $path(path), 8 * MB, PAGESIZE);

    u8bReset(book);

    // Write in a loop, calling FILEBookEnsure before each write
    for (int i = 0; i < 1000; i++) {
        call(FILEBookEnsure, book, 4096);
        memset(*u8bIdle(book), 'A' + (i % 26), 4096);
        u8sFed(u8bIdle(book), 4096);
    }


    // Verify ~4MB written, file grew automatically
    testeqv((long long)(u8bDataLen(book)), (long long)((size_t)(1000 * 4096)), "%lld");

    // Trim and verify
    call(FILETrimBook, book);

    call(FILEUnBook, book);
    call(FILEUnLink, $path(path));


    done;
}

// MEM-011 repro: FILEFlush must consume the written DATA prefix (advance
// buf[1]), not grow DATA into uninitialised IDLE (advance buf[2]).  Drive a
// FILE_WANT_BUFS stream past PAGESIZE, flush, and assert the written prefix
// was consumed: DATA shrinks, PAST grows, the buffer never reaches into the
// uninitialised IDLE tail.  Pre-fix the buggy `u8bFed(buf,r)` left DATA at
// 2*N and PAST at 0, so a second flush re-wrote the data plus N zero bytes.
ok64 FILEFlushStreamTest() {
    sane(1);
    a_path(path, $cstr("/tmp"));
    a_cstr(tmpl, "FILEFlushStream_XXXXXX");
    call(PATHu8bAddTmp, path, tmpl);

    // Booked write stream: 8MB reserved, 1 page initial -> FILE_WANT_BUFS slot
    u8bp buf = NULL;
    call(FILEBookCreate, &buf, $path(path), 8 * MB, PAGESIZE);
    int fd = FILEBookedFD(buf);
    test(fd >= 0, FILEFAIL);

    // Feed a position-dependent pattern well past PAGESIZE so the flush fires.
    size_t const N = 3 * PAGESIZE;
    for (size_t i = 0; i < N; i++) {
        call(FILEBookEnsure, buf, 1);
        call(u8bFeed1, buf, (u8)(i & 0xFF));
    }
    testeqv((long long)(u8bDataLen(buf)), (long long)(N), "%lld");
    testeqv((long long)(u8bPastLen(buf)), (long long)(0), "%lld");

    // First flush: writes the whole DATA, must consume the written prefix.
    call(FILEFlush, &fd);
    // FIX: written prefix consumed -> DATA emptied, never grown into IDLE.
    // BUG: u8bFed grew DATA to 2*N (the extra N bytes uninitialised IDLE).
    testeqv((long long)(u8bDataLen(buf)), (long long)(0), "%lld");

    // Second flush is a no-op (DATA below threshold); must not corrupt.
    call(FILEFlush, &fd);
    testeqv((long long)(u8bDataLen(buf)), (long long)(0), "%lld");

    call(FILEUnBook, buf);

    // Read the file back: exactly the N fed bytes, in order, no duplication
    // and no trailing garbage from a re-flushed uninitialised tail.
    int rfd = FILE_CLOSED;
    call(FILEOpen, &rfd, $path(path), O_RDONLY);
    size_t fsize = 0;
    call(FILESize, &fsize, &rfd);
    testeqv((long long)(fsize), (long long)(N), "%lld");
    aBpad2(u8, back, 3 * PAGESIZE);
    call(FILEDrain, rfd, backidle);
    while (u8bDataLen(backbuf) < N && u8bHasRoom(backbuf)) {
        call(FILEDrain, rfd, backidle);
    }
    testeqv((long long)(u8bDataLen(backbuf)), (long long)(N), "%lld");
    u8csp got = u8bDataC(backbuf);
    for (size_t i = 0; i < N; i++) {
        testeqv((long long)((*got)[i]), (long long)((u8)(i & 0xFF)), "%lld");
    }
    call(FILEClose, &rfd);
    call(FILEUnLink, $path(path));
    done;
}

//  FILEExists guards a seed so it never truncates a live file (SUBS-016 /
//  ULOG-001): FILENONE for a missing path, OK once present; the guard
//  `if (FILENONE==FILEExists) FILECreate` leaves existing content intact.
ok64 FILEExistsTest() {
    sane(1);
    a_path(path, $cstr("/tmp"));
    a_cstr(tmpl, "FILEExists_XXXXXX");
    call(PATHu8bAddTmp, path, tmpl);
    want(FILEExists($path(path)) == FILENONE);   // AddTmp yields an unused name

    //  Fresh seed: guard sees absent → create empty.
    if (FILENONE == FILEExists($path(path))) {
        int fd = FILE_CLOSED;
        call(FILECreate, &fd, $path(path));
        call(FILEClose, &fd);
    }
    want(FILEExists($path(path)) == OK);
    {
        filestat s = {};
        want(FILEStat(&s, $path(path)) == OK);
        want(s.kind == FILE_KIND_REG);
        want(s.size == 0);
    }

    //  Put real content in it (a live refs/wtlog).
    int wfd = FILE_CLOSED;
    call(FILEOpen, &wfd, $path(path), O_WRONLY);
    a_cstr(payload, "LIVE-LOG-DO-NOT-TRUNCATE\n");
    call(FILEFeedAll, wfd, payload);
    call(FILEClose, &wfd);

    //  Re-seed: guard sees present → skips the create, content preserved.
    if (FILENONE == FILEExists($path(path))) {
        int fd = FILE_CLOSED;
        call(FILECreate, &fd, $path(path));
        call(FILEClose, &fd);
    }
    {
        filestat s = {};
        want(FILEStat(&s, $path(path)) == OK);
        want(s.size == (u64)u8csLen(payload));   // content preserved
    }

    call(FILEUnLink, $path(path));
    done;
}

ok64 FILEtest() {
    sane(1);
    call(FILEExistsTest);
    call(FILEFlushStreamTest);
    call(FILEtest1);
    call(FILEtest2);
    call(FILE3);
    call(FILEtest4);
    call(FILEtest5);
    call(FILEtest6);
    call(FILEtest7);
    call(FILEtest8);
    call(FILEtest8b);
    call(FILEtest9);
    call(FILEIterTest);
    call(FILEIterSortedTest);
    call(FILEBookTest);
    call(FILEBookExistingTest);
    call(FILEBookLimitTest);
    call(FILEBookEnsureTest);
    done;
}

TEST(FILEtest);
