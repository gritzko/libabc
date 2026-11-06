// CODE-26B065z50Q: a plain main() links abc-core via ABC_INIT; before
// ABCStart a BASS rental fails cleanly, after it the same call works.
#include "FILE.h"
#include "PRO.h"

ABC_INIT

static int spawn_true(ok64 *o) {
    a_cstr(path, "true");
    u8cs a[] = {u8slit("true")};
    u8css argv = {a, a + 1};
    pid_t pid = 0;
    int rc = -1;
    *o = FILESpawn(path, argv, NULL, NULL, &pid);
    if (*o == OK) *o = FILEReap(pid, &rc);
    return rc;
}

int main(int argn, char **args) {
    ok64 o = OK;
    spawn_true(&o);
    if (o != BNOROOM) {
        fprintf(stderr, "no BASS: want BNOROOM, got %s\n", ok64str(o));
        return 1;
    }
    if (ABCStart(argn, args) != OK) return 1;
    int rc = spawn_true(&o);
    int bad = o != OK || rc != 0 || $arglen != (size_t)argn;
    ABCStop();
    if (bad) fprintf(stderr, "with BASS: %s rc %d\n", ok64str(o), rc);
    return bad;
}
