#include "RON.h"

#include "OK.h"

const char* RON64_CHARS =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz~";

const u8 RON64_REV[256] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x0,  0x1,  0x2,  0x3,  0x4,  0x5,  0x6,  0x7,  0x8,  0x9,  0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xa,  0xb,  0xc,  0xd,  0xe,  0xf,  0x10,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c,
    0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0xff, 0xff, 0xff, 0xff, 0x24,
    0xff, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
    0x3c, 0x3d, 0x3e, 0xff, 0xff, 0xff, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,
};

ok64 RONutf8sFeed(u8** into, ron60 val) {
    u8 tmp[11];
    u8* e = tmp + 11;
    u8* t = e;
    do {
        *--t = RON64_CHARS[val & 63];
        val >>= 6;
    } while (val);
    size_t l = e - t;
    if ($len(into) < l) return SNOROOM;
    memcpy(*into, t, l);
    *into += l;
    return OK;
}

ok64 RONutf8sDrain(ron60* o, u8c* const* from) {
    // ABC-008: cap at 11 digits / 64 bits, mirroring the feed side
    if ($len(from) > 10) return OKBADTEXT;
    ron60 res = 0;
    for (u8c* p = from[0]; p < from[1]; ++p) {
        u64 v = RON64_REV[*p];
        if (v == 0xff) return OKBADTEXT;
        // ABC-008: reject values over 64 bits instead of shifting out
        res = (res << 6) | v;
    }
    *o = res;
    return OK;
}

// Layout: YYMDDhmsll (10 RON64 digits = 60 bits)
//   [9][8] year tens/ones (0-9 each, since tm_year-100 in 00..99)
//   [7]    month (1-12)
//   [6][5] day tens/ones (0-3, 0-9)
//   [4]    hour (0-23)
//   [3]    minute (0-59)
//   [2]    second (0-59)
//   [1][0] milliseconds, packed: ms = [1]*64 + [0]  (0-999)
ok64 RONOfTime(ron60* r, struct tm* t, u32 ms) {
    if (!r || !t) return BADARG;
    u64 y = t->tm_year - 100;
    if (y >= 100) return BADARG;
    if (t->tm_mon < 0 || t->tm_mon >= 12) return BADARG;
    if (t->tm_mday < 1 || t->tm_mday > 31) return BADARG;
    if (t->tm_hour < 0 || t->tm_hour >= 24) return BADARG;
    if (t->tm_min < 0 || t->tm_min >= 60) return BADARG;
    if (t->tm_sec < 0 || t->tm_sec >= 60) return BADARG;
    if (ms >= 1000) return BADARG;
    *r = 0;
    *r |= ((y / 10) << (9 * 6));
    *r |= ((y % 10) << (8 * 6));
    *r |= ((u64)(t->tm_mon + 1) << (7 * 6));
    *r |= ((u64)(t->tm_mday / 10) << (6 * 6));
    *r |= ((u64)(t->tm_mday % 10) << (5 * 6));
    *r |= ((u64)(t->tm_hour) << (4 * 6));
    *r |= ((u64)(t->tm_min) << (3 * 6));
    *r |= ((u64)(t->tm_sec) << (2 * 6));
    *r |= (((u64)ms / 64) << (1 * 6));
    *r |= (((u64)ms % 64) << (0 * 6));
    return OK;
}

ok64 RONToTime(ron60 r, struct tm* t, u32 *ms) {
    if (!t) return BADARG;
    u64 y1   = (r >> (9 * 6)) & 63;
    u64 y0   = (r >> (8 * 6)) & 63;
    u64 mon  = (r >> (7 * 6)) & 63;
    u64 d1   = (r >> (6 * 6)) & 63;
    u64 d0   = (r >> (5 * 6)) & 63;
    u64 hour = (r >> (4 * 6)) & 63;
    u64 min  = (r >> (3 * 6)) & 63;
    u64 sec  = (r >> (2 * 6)) & 63;
    u64 l1   = (r >> (1 * 6)) & 63;
    u64 l0   = (r >> (0 * 6)) & 63;
    //  RON-001: decode-tolerant; clamp out-of-range slots to nearest valid
    //  value (e.g. same-second +1 ms overflow) instead of rejecting.
    if (y1 > 9) y1 = 9;
    if (y0 > 9) y0 = 9;
    if (mon < 1) mon = 1; else if (mon > 12) mon = 12;
    if (d0 > 9) d0 = 9;
    if (d1 > 3) d1 = 3;
    u64 mday = d1 * 10 + d0;
    if (mday < 1) mday = 1; else if (mday > 31) mday = 31;
    if (hour > 23) hour = 23;
    if (min  > 59) min  = 59;
    if (sec  > 59) sec  = 59;
    u64 msv = l1 * 64 + l0;
    if (msv > 999) msv = 999;
    t->tm_year = 100 + y1 * 10 + y0;
    t->tm_mon = mon - 1;
    t->tm_mday = mday;
    t->tm_hour = hour;
    t->tm_min = min;
    t->tm_sec = sec;
    if (ms) *ms = (u32)msv;
    return OK;
}

ok64 RONVerify(u8c** txt) {
    if (txt[0] >= txt[1]) return RONBAD;
    for (u8cp c = txt[0]; c < txt[1]; ++c) {
        if (RON64_REV[*c] == 0xff) return RONBAD;
    }
    return OK;
}

ok64 RONu8sFeedPad(u8** into, ron60 val, u8 width) {
    if ($len(into) < width) return SNOROOM;
    // ABC-008: validate before writing (all-or-nothing Feed contract)
    if (width <= 10 && (val >> (6 * width)) != 0) return SBADARG;
    u8p p = into[0] + width;
    for (u8 i = 0; i < width; i++) {
        *--p = RON64_CHARS[val & 63];
        val >>= 6;
    }
    // ABC-008: advance the cursor so chained feeds do not overwrite
    *into += width;
    return OK;
}

ok64 RONSpliceBase(ron60 *base, u8 *width, u64 rand, u64 prob, ron60 n) {
    if (n == 0 || prob == 0) return SBADARG;
    // ABC-008: saturate need instead of wrapping 2*prob*n
    u64 need = (n > UINT64_MAX / 2 / prob) ? UINT64_MAX : 2 * prob * n;
    u8 w = 1;
    u64 space = 64;
    while (space < need && w < 10) {
        space *= 64;
        w++;
    }
    *width = w;
    // ABC-008: n >= space would make avail 0 (division by zero below)
    if (n >= space) return SBADARG;
    u64 avail = space - n;
    *base = rand % avail;
    return OK;
}

//  DIS-051: reproducible-build clock override.  When SOURCE_DATE_EPOCH is
//  set to a decimal Unix-epoch (git's standard), RONNow returns the ron60
//  for that whole second (ms=0) instead of the wall clock, so two posts of
//  one tree yield the same commit sha.  Encoded via localtime_r exactly
//  like the live path, so at_ts_of_ron60's mktime recovers the epoch
//  tz-stably.  Unset / unparsable / out-of-domain → wall clock unchanged.
static b8 ron_source_date_epoch(time_t *sec_out) {
    char const *v = getenv("SOURCE_DATE_EPOCH");
    if (v == NULL || *v == 0) return NO;
    char *end = NULL;
    errno = 0;
    unsigned long long e = strtoull(v, &end, 10);
    if (errno != 0 || end == v || (end && *end != 0)) return NO;
    *sec_out = (time_t)e;
    return YES;
}

ok64 RONNow() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    u32 ms = (u32)(ts.tv_nsec / 1000000);  // 0-999
    time_t pin = 0;
    if (ron_source_date_epoch(&pin)) {
        ts.tv_sec = pin;
        ms = 0;                            // git pins whole seconds
    }
    struct tm tmbuf;
    localtime_r(&ts.tv_sec, &tmbuf);
    ron60 t = 0;
    //  Outside the 2000-2099 ron60 domain RONOfTime fails; keep `t`'s
    //  caller-safe zero rather than emit a bogus stamp.
    RONOfTime(&t, &tmbuf, ms);
    return t;
}
