#!/bin/sh
# .beagle/build.sh <configure|build|test> [SUITE...] — abc's build, run at the
# tree root by .beagle/sandbox.sh or bare.  cmake+ninja into build/, clang
# with compiler-rt (BOOT-26A23Apv3i: Alpine's clang links nothing without
# it).  lz4/sodium are optional in CMakeLists: suites needing a missing one
# are not built, build/skipped.txt names them.
# test runs every ctest suite, prints a `be-suite: NAME PASS|FAIL|TIMEOUT
# SECSs [LOG]` line per suite and a `N/M suites passed, failed: ...` summary;
# its exit status is the number of failed suites.  `test SUITE...` runs only
# those (`be test SUITE`): each name matches suite X or Xtest exactly.
set -eu
CF="--rtlib=compiler-rt"
configure() {
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_C_COMPILER=clang -DCMAKE_C_FLAGS="$CF" \
        -DCMAKE_EXE_LINKER_FLAGS="$CF" >/dev/null
}
skipped() {
    s=$(cat build/skipped.txt 2>/dev/null || :)
    [ -z "$s" ] || echo "build.sh: skipped: $s"
}
LOG=build/Testing/Temporary/LastTest.log
case "${1:-}" in
    configure) configure ;;
    build) [ -f build/build.ninja ] || configure; ninja -C build ;;
    test) shift; [ -f build/build.ninja ] || configure; ninja -C build
          if [ $# -gt 0 ]; then
              ctest --test-dir build --no-tests=error --output-on-failure \
                  -R "^($(IFS='|'; echo "$*"))(test)?\$" && exit 0 || r=$?
              echo "build.sh: suites: $(ctest --test-dir build -N |
                  sed -n 's/^ *Test *#[0-9]*: //p' | tr '\n' ' ')" >&2
              exit $r
          fi
          O=build/ctest.out
          ctest --test-dir build -j16 --output-on-failure >"$O" 2>&1 || :
          cat "$O"
          # ` 3/61 Test  #3: Stest ....   Passed    0.01 sec`
          #          `...***Failed    0.02 sec`, `...***Timeout  20.00 sec`
          awk '$2 == "Test" && $3 ~ /^#[0-9]+:$/ && $NF == "sec" {
                   v = / Passed +[0-9.]+ sec$/ ? "Passed" : /\*\*\*Timeout/ ? "Timeout" : "Failed"
                   print $4, v, $(NF-1) }' "$O" |
          while read -r n v t; do
              case "$v" in Passed) echo "be-suite: $n PASS ${t}s" ;;
                  Timeout) echo "be-suite: $n TIMEOUT ${t}s $LOG" ;;
                  *) echo "be-suite: $n FAIL ${t}s $LOG" ;; esac
          done | sort >build/suites.txt
          cat build/suites.txt
          ALL=$(wc -l <build/suites.txt)
          FAILED=$(awk '$3 != "PASS" { print $2 }' build/suites.txt | tr '\n' ' ')
          FAILED=$(echo $FAILED)
          nf=$(echo $FAILED | wc -w)
          [ "$ALL" -gt 0 ] || { echo "build.sh test: no suites ran"; exit 1; }
          skipped
          echo "build.sh test: $((ALL - nf))/$ALL suites passed${FAILED:+, failed: $FAILED}"
          exit $nf ;;
    *) echo "build.sh: unknown step '${1:-}' (configure|build|test)" >&2; exit 2 ;;
esac
