#!/bin/sh
# CODE-26B065bwvG: a line over the old 8 KB fgets buffer is one line.
set -eu
G=$1 T=${TMPDIR:-/tmp}/nfagrep.$$
trap 'rm -f "$T"' EXIT
awk 'BEGIN{s="";for(i=0;i<9000;i++)s=s "a";print s "b";print "x"}' >"$T"
[ "$("$G" x "$T")" = "$T:2:x" ]
[ "$("$G" b "$T" | cut -d: -f3 | wc -c)" -eq 9002 ]
[ "$("$G" b <"$T" | wc -c)" -eq 9002 ]
[ "$("$G" a <"$T" | wc -l)" -eq 1 ]
