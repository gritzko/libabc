#!/bin/sh
# .beagle/build.sh <configure|build|test> [SUITE...] — seeded by `be init` for
# a cmake project: cmake+ninja into build/, ctest runs the suites, or just the
# named ones (`be test SUITE`).  Run at the tree root by .beagle/sandbox.sh or
# bare; edit to taste.
set -eu
configure() { cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null; }
case "${1:-}" in
    configure) configure ;;
    build) [ -f build/build.ninja ] || configure; ninja -C build ;;
    test) shift; [ -f build/build.ninja ] || configure; ninja -C build
          if [ $# = 0 ]; then ctest --test-dir build -j16 --output-on-failure
          else ctest --test-dir build --no-tests=error --output-on-failure \
                   -R "^($(IFS='|'; echo "$*"))\$"; fi ;;
    *) echo "build.sh: unknown step '${1:-}' (configure|build|test)" >&2; exit 2 ;;
esac
