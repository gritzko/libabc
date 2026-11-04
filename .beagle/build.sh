#!/bin/sh
# .beagle/build.sh <configure|build|test|asan> [SUITE...] — abc's build, run
# at the tree root by .beagle/sandbox.sh or bare.  cmake+ninja into build/,
# clang with compiler-rt (BOOT-26A23Apv3i: Alpine's clang links nothing
# without it).  lz4/sodium are optional in CMakeLists: suites needing a
# missing one are not built, build/skipped.txt names them.
# test runs every ctest suite, prints a `be-suite: NAME PASS|FAIL|TIMEOUT
# SECSs [LOG]` line per suite and a `N/M suites passed, failed: ...` summary;
# its exit status is the number of failed suites.  `test SUITE...` runs only
# those (`be test SUITE`): each name matches suite X or Xtest exactly.
# BUILD-26A26IKs0g: a failed suite's LOG is build/Testing/NAME.log, its own
# output cut from LastTest.log (which holds every suite's); the suite list
# for a bad `test SUITE` is taken first, as `ctest -N` empties LastTest.log.
set -eu
CF="--rtlib=compiler-rt" LF= CM= B=build
# BUILD-26B03F7and: `asan [SUITE...]` is `test` in build-asan/ under ASAN+UBSAN;
# the sanitizer runtime wants libunwind at link (wiki/Build.mkd Scratch programs).
if [ "${1:-}" = asan ]; then
    shift; set -- test "$@"
    CF="$CF -fsanitize=address,undefined" LF=--unwindlib=libunwind
    CM=-DWITH_ASAN=ON B=build-asan
fi
configure() {
    cmake -S . -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_C_COMPILER=clang -DCMAKE_C_FLAGS="$CF" \
        -DCMAKE_EXE_LINKER_FLAGS="$CF $LF" $CM >/dev/null
}
skipped() {
    s=$(cat "$B/skipped.txt" 2>/dev/null || :)
    [ -z "$s" ] || echo "build.sh: skipped: $s"
}
LAST=$B/Testing/Temporary/LastTest.log
# suite NAME's output in $LAST -> $B/Testing/NAME.log, the path echoed
own_log() {
    awk -v n="$1" '/^[0-9]+\/[0-9]+ Testing: / { in_ = ($3 == n); out = 0; next }
        in_ && /^<end of output>$/ { out = 0 }
        in_ && out { print }
        in_ && /^Output:$/ { getline; out = 1 }' "$LAST" >"$B/Testing/$1.log"
    echo "$B/Testing/$1.log"
}
case "${1:-}" in
    configure) configure ;;
    build) [ -f "$B/build.ninja" ] || configure; ninja -C "$B" ;;
    test) shift; [ -f "$B/build.ninja" ] || configure; ninja -C "$B"
          if [ $# -gt 0 ]; then
              all=$(ctest --test-dir "$B" -N | sed -n 's/^ *Test *#[0-9]*: //p' | tr '\n' ' ')
              ctest --test-dir "$B" --no-tests=error --output-on-failure \
                  -R "^($(IFS='|'; echo "$*"))(test)?\$" && exit 0 || r=$?
              echo "build.sh: suites: $all" >&2
              exit $r
          fi
          O=$B/ctest.out
          ctest --test-dir "$B" -j16 --output-on-failure >"$O" 2>&1 || :
          cat "$O"
          # ` 3/61 Test  #3: Stest ....   Passed    0.01 sec`
          #          `...***Failed    0.02 sec`, `...***Timeout  20.00 sec`
          awk '$2 == "Test" && $3 ~ /^#[0-9]+:$/ && $NF == "sec" {
                   v = / Passed +[0-9.]+ sec$/ ? "Passed" : /\*\*\*Timeout/ ? "Timeout" : "Failed"
                   print $4, v, $(NF-1) }' "$O" |
          while read -r n v t; do
              case "$v" in Passed) echo "be-suite: $n PASS ${t}s" ;;
                  Timeout) echo "be-suite: $n TIMEOUT ${t}s $(own_log "$n")" ;;
                  *) echo "be-suite: $n FAIL ${t}s $(own_log "$n")" ;; esac
          done | sort >"$B/suites.txt"
          cat "$B/suites.txt"
          ALL=$(wc -l <"$B/suites.txt")
          FAILED=$(awk '$3 != "PASS" { print $2 }' "$B/suites.txt" | tr '\n' ' ')
          FAILED=$(echo $FAILED)
          nf=$(echo $FAILED | wc -w)
          [ "$ALL" -gt 0 ] || { echo "build.sh test: no suites ran"; exit 1; }
          skipped
          echo "build.sh test: $((ALL - nf))/$ALL suites passed${FAILED:+, failed: $FAILED}"
          exit $nf ;;
    *) echo "build.sh: unknown step '${1:-}' (configure|build|test|asan)" >&2; exit 2 ;;
esac
