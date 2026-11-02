//  HASH-26B02GGjI3: one HASHx instantiation's bench.  Wants X, HASHx.h
//  already instantiated for it, BNAME (a label) and X(HB, mk)(i, rec): a
//  nonzero record whose key is a bijection of i.  Keys 0..n-1 hit,
//  n..2n-1 miss.  Only API both sides of the HASH-26B02G* review have.

fun ok64 X(HB, run)(size_t n, int reps) {
    sane(n > 0);
    size_t loops = n >= HB_OPS ? 1 : HB_OPS / n;
    double best[HB_NOPS];
    for (int k = 0; k < HB_NOPS; k++) best[k] = 1e30;
    size_t slots = 0;
    u64 sum = 0;
    for (int r = 0; r < reps; r++) {
        X(B, ) buf = {}, into = {};
        call(X(, bMap), buf, HB_MAP);
        call(X(, bFed), buf, 64);
        double t0 = hbnow();
        for (size_t i = 0; i < n; i++) {
            T rec;
            X(HB, mk)(i, &rec);
            call(X(HASH, PutGrow), buf, &rec);
        }
        double t1 = hbnow();
        X($, ) data = {buf[1], buf[2]};
        for (size_t l = 0; l < loops; l++)
            for (size_t i = 0; i < n; i++) {
                T rec;
                X(HB, mk)(i, &rec);
                if (OK == X(HASH, Get)(&rec, data)) sum += *(u8 *)&rec;
            }
        double t2 = hbnow();
        for (size_t l = 0; l < loops; l++)
            for (size_t i = n; i < 2 * n; i++) {
                T rec;
                X(HB, mk)(i, &rec);
                if (OK == X(HASH, Get)(&rec, data)) sum++;
            }
        double t3 = hbnow();
        size_t len = (size_t)$len(data);
        call(X(, bMap), into, 2 * len);
        call(X(, bFed), into, 2 * len);
        double t4 = hbnow();
        call(X(HASH, Move), into, buf);
        double t5 = hbnow();
        //  churn: delete key i, put key i + n; the table keeps n entries
        for (size_t i = 0; i < n; i++) {
            T rec;
            X(HB, mk)(i, &rec);
            X(HASH, Del)(data, &rec);
            X(HB, mk)(i + n, &rec);
            call(X(HASH, PutGrow), buf, &rec);
            data[0] = buf[1], data[1] = buf[2];
        }
        double t6 = hbnow();
        slots = len;
        double ns[HB_NOPS] = {
            (t1 - t0) / n, (t2 - t1) / (n * loops), (t3 - t2) / (n * loops),
            (t5 - t4) / n, (t6 - t5) / n};
        for (int k = 0; k < HB_NOPS; k++)
            if (ns[k] * 1e9 < best[k]) best[k] = ns[k] * 1e9;
        X(, bUnMap)(into);
        X(, bUnMap)(buf);
    }
    printf("%-10s %9zu %6.1f %6.1f %6.1f %6.1f %6.1f %5.2f %5zuMB\n", BNAME,
           n, best[0], best[1], best[2], best[3], best[4],
           (double)n / slots, slots * sizeof(T) >> 20);
    if (sum == 42) printf(" ");  // keep the Gets
    done;
}

#undef BNAME
