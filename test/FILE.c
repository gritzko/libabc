#include "FILE.h"

#include <dirent.h>
#include <errno.h>
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

// STD-26B068lwbj: rm -r unlinks symlinks (top level or inside), never
// descends into their targets.
ok64 FILERmDirSymlinkTest() {
    sane(1);
    a_path(out, $cstr("/tmp"));
    a_cstr(t1, "FILErmout_XXXXXX");
    call(PATHu8bAddTmp, out, t1);
    call(FILEMakeDir, $path(out));
    a_path(file, $path(out));
    a_cstr(fn, "keep.txt");
    call(PATHu8bPush, file, fn);
    int fd;
    call(FILECreate, &fd, $path(file));
    call(FILEClose, &fd);

    a_path(base, $cstr("/tmp"));
    a_cstr(t2, "FILErmbase_XXXXXX");
    call(PATHu8bAddTmp, base, t2);
    call(FILEMakeDir, $path(base));
    a_path(in, $path(base));
    a_cstr(ln, "link");
    call(PATHu8bPush, in, ln);
    call(FILESymLink, $path(out), $path(in));
    a_path(top, $path(base));
    a_cstr(tl, "-top");
    call(u8bFeed, top, tl);
    call(PATHu8bTerm, top);
    call(FILESymLink, $path(out), $path(top));

    filestat s = {};
    call(FILERmDir, $path(top), true);
    test(OK != FILELStat(&s, $path(top)), FILEFAIL);
    test(OK == FILEStat(&s, $path(file)), FILEFAIL);
    call(FILERmDir, $path(base), true);
    test(OK != FILEStat(&s, $path(base)), FILEFAIL);
    test(OK == FILEStat(&s, $path(file)), FILEFAIL);
    call(FILERmDir, $path(out), true);
    done;
}

// Test FILEErr errno translation
ok64 FILEtest9() {
    sane(1);


    // FILEStat on non-existent file should return FILENONE
    a_path(nofile, $cstr("/tmp"));
    a_cstr(tmpl, "FILEtest9_XXXXXX");
    call(PATHu8bAddTmp, nofile, tmpl);
    filestat s = {};
    ok64 res = FILEStat(&s, $path(nofile));
    test(res == FILENONE, FILEFAIL);

    // Verify FILEErr translates the current errno correctly
    errno = ENOENT; test(FILEErr(FILEFAIL) == FILENONE, FILEFAIL);
    errno = EACCES; test(FILEErr(FILEFAIL) == FILEACCES, FILEFAIL);
    errno = EEXIST; test(FILEErr(FILEFAIL) == FILEEXIST, FILEFAIL);
    errno = 0;      test(FILEErr(FILEFAIL) == OK, FILEFAIL);
    errno = 9999;   test(FILEErr(FILEFAIL) == FILEFAIL, FILEFAIL);  // unknown errno
    errno = 0;

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
            seen(FILEEND);
            call(FILEOuto, &child, &it);
        }
    }
    seen(FILEEND);
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
            seen(FILEEND);
            call(FILEOuto, &child, &it);
        }
        count++;
    }
    seen(FILEEND);
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

//  ABC-002: one errno→ok64 source of truth — the switch lives in
//  FILEErr() (FILE.c).  Assert it maps each errno to its canonical FILE*
//  symbol and falls back to the caller's `def` for an errno it does not
//  map.  Pre-fix the old FILE_ERR_VOCAB table returned a separate
//  prefix-less encoding that never matched these symbols.
ok64 FILEErrTest() {
    sane(1);
    static const struct { int e; ok64 want; } M[] = {
        {0, OK},                   {EACCES, FILEACCES},
        {EAGAIN, FILEAGAIN},       {EBADF, FILEBADF},
        {EBUSY, FILEBUSY},         {EEXIST, FILEEXIST},
        {EFAULT, FILEFAULT},       {EFBIG, FILEFBIG},
        {EINTR, FILEINTR},         {EINVAL, FILEINVAL},
        {EIO, FILEIO},             {EISDIR, FILEISDIR},
        {ELOOP, FILELOOP},         {EMFILE, FILEMFILE},
        {ENAMETOOLONG, FILE2LONG}, {ENFILE, FILENFILE},
        {ENODEV, FILENODEV},       {ENOENT, FILENONE},
        {ENOMEM, FILENOMEM},       {ENOSPC, FILENOSPC},
        {ENOTDIR, FILENOTDIR},     {ENOTEMPTY, FILENOTEMP},
        {EPERM, FILEPERM},         {EROFS, FILEROFS},
        {EXDEV, FILEXDEV},
    };
    for (size_t i = 0; i < sizeof(M) / sizeof(M[0]); i++) {
        errno = M[i].e;
        //  `def` is a sentinel we must never see for a mapped errno.
        testeqv((unsigned long long)FILEErr(FILEFAIL),
                (unsigned long long)M[i].want, "0x%llx");
    }
    //  An errno the switch does NOT map returns the caller's `def`.
    errno = ECHILD;
    testeqv((unsigned long long)FILEErr(FILEFAIL),
            (unsigned long long)FILEFAIL, "0x%llx");
    errno = 0;
    done;
}

//  MEM-037 repro: a map/book that opens OK but fails the map must not
//  leak the opened fd.  A zero-length file opens fine but mmap(NULL, 0)
//  fails (EINVAL) inside FILEMapFD; FILEBookFD on the same file fails its
//  internal checks too.  Loop N failing maps and assert the open-fd count
//  is stable via a sentinel fd: open /dev/null, record its number, run the
//  loop, then re-open /dev/null — the kernel hands out the lowest free fd,
//  so a stable number proves no fds leaked.  An fd leak is invisible to
//  LeakSanitizer (it tracks malloc, not fds), so this count check is the
//  only guard.
ok64 FILEFdLeakTest() {
    sane(1);

    //  A zero-length file: opens OK, but mmap(NULL, 0) fails (EINVAL)
    //  inside FILEMapFD — exercises the map-failure error branch.
    a_path(zpath, $cstr("/tmp"));
    a_cstr(ztmpl, "FILEFdLeakMap_XXXXXX");
    call(PATHu8bAddTmp, zpath, ztmpl);
    int zfd = FILE_CLOSED;
    call(FILECreate, &zfd, $path(zpath));
    call(FILEClose, &zfd);  // leaves a 0-byte file on disk

    //  A file LARGER than the requested book_size: opens OK, but
    //  FILEBookFD's `map_size <= book_size` check fails — exercises the
    //  book-failure error branch.
    a_path(bpath, $cstr("/tmp"));
    a_cstr(btmpl, "FILEFdLeakBook_XXXXXX");
    call(PATHu8bAddTmp, bpath, btmpl);
    int bfd = FILE_CLOSED;
    call(FILECreate, &bfd, $path(bpath));
    call(FILEResize, &bfd, 256 * KB);  // larger than the 64 KB book below
    call(FILEClose, &bfd);

    //  Sentinel: the next free fd number before the loop.
    int sentinel = open("/dev/null", O_RDONLY);
    test(sentinel >= 0, FILEFAIL);
    close(sentinel);

    //  Each iteration opens the file then fails the map/book; a leaked
    //  fd would raise the lowest-free-fd number returned below.
    for (int i = 0; i < 64; i++) {
        u8bp buf = NULL;
        ok64 r = FILEMapRO(&buf, $path(zpath));
        test(r != OK, FILEFAIL);  // must fail (0-byte mmap)
        test(buf == NULL, FILEFAIL);
    }
    for (int i = 0; i < 64; i++) {
        u8bp buf = NULL;
        ok64 r = FILEBook(&buf, $path(bpath), 64 * KB);
        test(r != OK, FILEFAIL);  // must fail (file > book_size)
        test(buf == NULL, FILEFAIL);
    }

    //  Re-check the lowest free fd.  Leaked fds shift this upward.
    int after = open("/dev/null", O_RDONLY);
    test(after >= 0, FILEFAIL);
    close(after);
    testeqv((long long)after, (long long)sentinel, "%lld");

    call(FILEUnLink, $path(zpath));
    call(FILEUnLink, $path(bpath));
    done;
}

//  ABC-013 repro: a partial write in FILEFlushThreshold must consume only
//  the written prefix; Bate() dropped the unwritten tail.
ok64 FILEFlushThresholdPipeTest() {
    sane(1);
    size_t const CHUNK = 4096, TAIL = 512;
    int pfd[2] = {FILE_CLOSED, FILE_CLOSED};
    test(0 == pipe(pfd), FILEFAIL);
    test(0 <= fcntl(pfd[1], F_SETFL, O_NONBLOCK), FILEFAIL);

    //  Fill the pipe to capacity with junk (atomic 4 KiB chunks), then
    //  drain one chunk back: exactly one page of room remains.
    a_pad(u8, junk, 4096);
    memset(junk[0], 0xEE, CHUNK);
    size_t filled = 0;
    for (;;) {
        ssize_t j = write(pfd[1], junk[0], CHUNK);
        if (j < 0) break;  // EAGAIN: pipe full
        filled += (size_t)j;
    }
    test(filled >= CHUNK, FILEFAIL);
    a_pad(u8, sink, 4096);
    test(CHUNK == (size_t)read(pfd[0], sink[0], CHUNK), FILEFAIL);
    filled -= CHUNK;  // junk bytes still queued ahead of the pattern

    //  DATA = one page + a tail; the nonblocking write accepts one page.
    aBpad2(u8, out, 8192);
    for (size_t i = 0; i < CHUNK + TAIL; i++)
        call(u8bFeed1, outbuf, (u8)(i & 0xFF));
    call(FILEFlushThreshold, pfd[1], outbuf, 1);
    //  FIX: written page consumed, tail kept.  BUG: tail silently lost.
    testeqv((long long)u8bDataLen(outbuf), (long long)TAIL, "%lld");
    testeqv((long long)u8bPastLen(outbuf), (long long)CHUNK, "%lld");

    //  Drain the junk, then verify every pattern byte arrives, gap-free.
    while (filled > 0) {
        ssize_t j = read(pfd[0], sink[0], filled < CHUNK ? filled : CHUNK);
        test(j > 0, FILEFAIL);
        filled -= (size_t)j;
    }
    a_pad(u8, got, 4096 + 512);
    size_t recvd = 0;
    while (recvd < CHUNK) {
        ssize_t r = read(pfd[0], got[0] + recvd, CHUNK - recvd);
        test(r > 0, FILEFAIL);
        recvd += (size_t)r;
    }
    call(FILEFlushThreshold, pfd[1], outbuf, 1);  // room is back: send tail
    testeqv((long long)u8bDataLen(outbuf), (long long)0, "%lld");
    testeqv((long long)u8bPastLen(outbuf), (long long)(CHUNK + TAIL), "%lld");
    while (recvd < CHUNK + TAIL) {
        ssize_t r = read(pfd[0], got[0] + recvd, CHUNK + TAIL - recvd);
        test(r > 0, FILEFAIL);
        recvd += (size_t)r;
    }
    for (size_t i = 0; i < CHUNK + TAIL; i++)
        testeqv((long long)got[0][i], (long long)(u8)(i & 0xFF), "%lld");
    close(pfd[0]);
    close(pfd[1]);
    done;
}

//  ABC-013 repro: FILETrimMap must trim to PAST+DATA (b[2]-b[0]) like
//  FILETrimBook; trimming to DATA alone SIGBUSes still-mapped pages.
ok64 FILETrimMapPastTest() {
    sane(1);
    a_path(path, $cstr("/tmp"));
    a_cstr(tmpl, "FILETrimMapPast_XXXXXX");
    call(PATHu8bAddTmp, path, tmpl);
    size_t const sp = FILESysPage();
    u8bp buf = NULL;
    call(FILEMapCreate, &buf, $path(path), 2 * sp);
    u8bReset(buf);
    for (size_t i = 0; i < sp + 100; i++) call(u8bFeed1, buf, (u8)(i & 0xFF));
    call(u8bUsed, buf, sp);  // consume page 0: PAST=sp, DATA=100
    call(FILETrimMap, buf);
    //  BUG: file truncated to 100 -> page 1 unbacked -> this read SIGBUSes.
    testeqv((long long)Bat(buf, sp), (long long)(u8)(sp & 0xFF), "%lld");
    call(FILEUnMap, buf);
    filestat s = {};
    call(FILEStat, &s, $path(path));
    testeqv((long long)s.size, (long long)(sp + 100), "%lld");
    call(FILEUnLink, $path(path));
    done;
}

//  PORT-26A239KGXD repro: bytes written through a RW map must survive the
//  trim that shrinks the file under that live map.  OpenBSD (no unified
//  buffer cache) dropped the dirty page holding the new EOF on ftruncate,
//  so a sub-page memtable sealed as a run of zeros (`be index`: no rows).
//  Checked through a fresh fd and pread, never through the map.
static ok64 file_trim_kept(path8s path, size_t len) {
    sane(1);
    int fd = open((char const *)*path, O_RDONLY);
    test(fd >= 0, FILEFAIL);
    u8 *got = calloc(len + 1, 1);
    if (got == NULL) { close(fd); fail(FILEFAIL); }
    ssize_t n = pread(fd, got, len + 1, 0);
    close(fd);
    size_t bad = len;
    for (size_t i = 0; i < len && bad == len; i++)
        if (got[i] != (u8)(0xA0 ^ (i & 0x7F))) bad = i;
    free(got);
    testeqv((long long)n, (long long)len, "%lld");
    testeqv((long long)bad, (long long)len, "%lld");  // first lost byte
    done;
}

ok64 FILETrimKeepsDirtyTest() {
    sane(1);
    size_t const sp = FILESysPage();
    size_t const lens[] = {432, sp + 904};  // EOF mid page 0, mid page 1
    for (int k = 0; k < 2; k++) {
        size_t len = lens[k];
        a_path(path, $cstr("/tmp"));
        a_cstr(tmpl, "FILETrimKeeps_XXXXXX");
        call(PATHu8bAddTmp, path, tmpl);
        //  FILEMapCreate + FILETrimMap: the PUP memtable seal.
        u8bp buf = NULL;
        call(FILEMapCreate, &buf, $path(path), 2 * sp);
        u8bReset(buf);
        for (size_t i = 0; i < len; i++)
            call(u8bFeed1, buf, (u8)(0xA0 ^ (i & 0x7F)));
        call(FILETrimMap, buf);
        call(file_trim_kept, $path(path), len);
        call(FILEUnMap, buf);
        call(file_trim_kept, $path(path), len);
        call(FILEUnLink, $path(path));
        //  FILEBookCreate + FILETrimBook: the ULOG close.
        u8bp book = NULL;
        call(FILEBookCreate, &book, $path(path), 64 * KB, 2 * sp);
        for (size_t i = 0; i < len; i++)
            call(u8bFeed1, book, (u8)(0xA0 ^ (i & 0x7F)));
        call(FILETrimBook, book);
        call(file_trim_kept, $path(path), len);
        call(FILEUnBook, book);
        call(file_trim_kept, $path(path), len);
        call(FILEUnLink, $path(path));
    }
    done;
}

//  ABC-013 repro: a mapped (not booked) fd below a booked one must read
//  book_end NULL and fail FILENOBOOK, not mmap garbage.
ok64 FILEBookGapTest() {
    sane(1);
    //  Push the next free fd high so the map lands on a lower fd.
    int pads[40];
    int npads = 0;
    for (; npads < 40; npads++) {
        pads[npads] = open("/dev/null", O_RDONLY);
        test(pads[npads] >= 0, FILEFAIL);
    }
    a_path(bpath, $cstr("/tmp"));
    a_cstr(btmpl, "FILEBookGapB_XXXXXX");
    call(PATHu8bAddTmp, bpath, btmpl);
    u8bp book = NULL;
    call(FILEBookCreate, &book, $path(bpath), 64 * KB, 4 * KB);
    int bookfd = FILEBookedFD(book);
    test(bookfd > pads[npads - 1], FILEFAIL);
    //  Free the pads: the next opens land inside the just-fed gap.
    for (int i = 0; i < npads; i++) close(pads[i]);
    //  A plain mapped file whose fd falls in the gap: FILEBookExtend must
    //  see a NULL gap entry and fail FILENOBOOK, not mmap over random VA.
    a_path(mpath, $cstr("/tmp"));
    a_cstr(mtmpl, "FILEBookGapM_XXXXXX");
    call(PATHu8bAddTmp, mpath, mtmpl);
    u8bp map = NULL;
    call(FILEMapCreate, &map, $path(mpath), 4 * KB);
    int mapfd = FILEBookedFD(map);
    test(mapfd >= 0 && mapfd < bookfd, FILEFAIL);
    testeqv((unsigned long long)FILEBookExtend(map, 8 * KB),
            (unsigned long long)FILENOBOOK, "0x%llx");
    //  Book/UnBook churn must leave no stale non-NULL entries behind.
    call(FILEUnBook, book);
    call(FILEBookCreate, &book, $path(bpath), 64 * KB, 4 * KB);
    call(FILEUnBook, book);
    testeqv((unsigned long long)FILEBookExtend(map, 8 * KB),
            (unsigned long long)FILENOBOOK, "0x%llx");
    call(FILEUnMap, map);
    call(FILEUnLink, $path(mpath));
    call(FILEUnLink, $path(bpath));
    done;
}

//  CODE-26B068lK6V: a buffered fd: FILEFlush writes only past a page,
//  FILEFlushAll drains, an oversize feed goes straight out, close drains.
ok64 FILEStreamTest() {
    sane(1);
    int pfd[2] = {FILE_CLOSED, FILE_CLOSED};
    test(0 == pipe(pfd), FILEFAIL);
    size_t const sp = PAGESIZE, BIG = 3 * sp, TOTAL = 100 + sp + 7 + BIG + 5;
    call(FILEBuffer, pfd[1], 2 * sp);
    test(FILEBuffer(pfd[1], sp) == FILEBADARG, FAILSANITY);  // slot is taken
    u8bp buf = FILESlot(pfd[1]);
    want(FILEBookedFD(buf) == FILE_CLOSED);  // never seen as a mapping
    want(FILEReMap(buf, sp) == FILENONE);
    a_pad(u8, pat, 4 * 4096);
    for (size_t i = 0; i < BIG; i++) u8bFeed1(pat, (u8)(i * 7));
    u8cs all = {pat[0], pat[0] + BIG};
    u8cs a = {all[0], all[0] + 100}, b = {all[0] + 100, all[0] + 100 + sp};
    call(FILEBufFeed, pfd[1], a);
    call(FILEFlush, pfd + 1);
    same(u8bDataLen(buf), 100);  // below the threshold: kept
    call(FILEBufFeed, pfd[1], b);
    call(FILEFlush, pfd + 1);
    same(u8bDataLen(buf), 0);
    u8cs c = {all[0], all[0] + 7};
    call(FILEBufFeed, pfd[1], c);
    call(FILEFlushAll, pfd + 1);
    same(u8bDataLen(buf), 0);
    call(FILEBufFeed, pfd[1], all);  // larger than the buffer
    u8cs d = {all[0], all[0] + 5};
    call(FILEBufFeed, pfd[1], d);
    same(u8bDataLen(buf), 5);
    int wfd = pfd[1];
    call(FILEClose, &wfd);  // drains the 5 and unmaps
    want(buf[0] == NULL);
    a_pad(u8, got, 6 * 4096);
    size_t n = 0;
    for (ssize_t r; (r = read(pfd[0], got[0] + n, 6 * 4096 - n)) > 0;)
        n += (size_t)r;
    same(n, TOTAL);
    want(0 == memcmp(got[0], all[0], 100 + sp));
    want(0 == memcmp(got[0] + 100 + sp + 7, all[0], BIG));
    close(pfd[0]);
    done;
}

//  CODE-26B06BfNAx: a DEFER_PIPE fd survives a gone reader under default
//  SIGPIPE: feeds after EPIPE are no-ops, FILEClose reports it once.
ok64 FILEDeferPipeTest() {
    sane(1);
    void (*was)(int) = signal(SIGPIPE, SIG_DFL);
    int pfd[2] = {FILE_CLOSED, FILE_CLOSED};
    test(0 == pipe(pfd), FILEFAIL);
    close(pfd[0]);
    call(FILEBuffer, pfd[1], PAGESIZE);
    call(FILESetFlags, pfd[1], FILE_DEFER_PIPE);
    u8bp buf = FILESlot(pfd[1]);
    a_pad(u8, pat, 3 * 4096);
    u8bFed(pat, 3 * 4096);
    u8cs a = {pat[0], pat[0] + 100}, big = {pat[0], pat[0] + 3 * 4096};
    call(FILEBufFeed, pfd[1], a);
    call(FILEFlushAll, pfd + 1);  // EPIPE: dropped, no signal
    same(u8bDataLen(buf), 0);
    call(FILEBufFeed, pfd[1], a);
    same(u8bDataLen(buf), 0);  // gone: a no-op
    call(FILEFlush, pfd + 1);
    call(FILEBufFeed, pfd[1], big);
    int wfd = pfd[1];
    testeqv((unsigned long long)FILEClose(&wfd),
            (unsigned long long)FILEPIPE, "0x%llx");
    want(fcntl(pfd[1], F_GETFD) < 0);
    //  The flag is gone with the slot; a reused number starts clean
    test(0 == pipe(pfd), FILEFAIL);
    close(pfd[0]);
    call(FILEBuffer, pfd[1], PAGESIZE);
    call(FILESetFlags, pfd[1], FILE_DEFER_PIPE);
    call(FILEBufFeed, pfd[1], big);  // oversize, straight out
    wfd = pfd[1];
    testeqv((unsigned long long)FILEClose(&wfd),
            (unsigned long long)FILEPIPE, "0x%llx");
    //  Unflagged, SIGPIPE ignored: the flush fails with FILEPIPE
    signal(SIGPIPE, SIG_IGN);
    test(0 == pipe(pfd), FILEFAIL);
    close(pfd[0]);
    call(FILEBuffer, pfd[1], PAGESIZE);
    call(FILEBufFeed, pfd[1], a);
    testeqv((unsigned long long)FILEFlushAll(pfd + 1),
            (unsigned long long)FILEPIPE, "0x%llx");
    wfd = pfd[1];
    FILEClose(&wfd);
    signal(SIGPIPE, was);
    done;
}

//  CODE-26B06BfNAx: a HOLD fd writes only on FILEFlushAll or when full
ok64 FILEHoldTest() {
    sane(1);
    int pfd[2] = {FILE_CLOSED, FILE_CLOSED};
    test(0 == pipe(pfd), FILEFAIL);
    call(FILEBuffer, pfd[1], 4 * PAGESIZE);
    call(FILESetFlags, pfd[1], FILE_HOLD);
    u8bp buf = FILESlot(pfd[1]);
    a_pad(u8, pat, 2 * 4096);
    u8bFed(pat, 2 * 4096);
    call(FILEBufFeed, pfd[1], u8cbData(pat));
    call(FILEFlush, pfd + 1);
    same(u8bDataLen(buf), 2 * 4096);  // held past a page
    call(FILEFlushAll, pfd + 1);
    same(u8bDataLen(buf), 0);
    int wfd = pfd[1];
    call(FILEClose, &wfd);
    close(pfd[0]);
    done;
}

//  CODE-26B068lK6V: FILECloseAll unbooks/unmaps and closes live slots.
ok64 FILECloseAllTest() {
    sane(1);
    a_path(path, $cstr("/tmp"));
    a_cstr(tmpl, "FILECloseAll_XXXXXX");
    call(PATHu8bAddTmp, path, tmpl);
    u8bp book = NULL, map = NULL;
    call(FILEBookCreate, &book, $path(path), 64 * KB, 4 * KB);
    call(FILEMapRO, &map, $path(path));
    int bfd = FILEBookedFD(book), mfd = FILEBookedFD(map);
    want(bfd >= 0 && mfd >= 0);
    call(FILECloseAll);
    want(book[0] == NULL && map[0] == NULL);
    want(fcntl(bfd, F_GETFD) < 0 && fcntl(mfd, F_GETFD) < 0);
    call(FILEUnLink, $path(path));
    done;
}

ok64 FILEtest() {
    sane(1);
    call(FILEFdLeakTest);
    call(FILEErrTest);
    call(FILEExistsTest);
    call(FILEFlushThresholdPipeTest);
    call(FILETrimMapPastTest);
    call(FILETrimKeepsDirtyTest);
    call(FILEBookGapTest);
    call(FILEStreamTest);
    call(FILEDeferPipeTest);
    call(FILEHoldTest);
    call(FILECloseAllTest);
    call(FILEtest1);
    call(FILEtest2);
    call(FILE3);
    call(FILEtest4);
    call(FILEtest5);
    call(FILEtest6);
    call(FILEtest7);
    call(FILEtest8);
    call(FILEtest8b);
    call(FILERmDirSymlinkTest);
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
