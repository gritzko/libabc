#!/bin/sh
# .beagle/sandbox.sh <configure|build|test|asan|fuzz> [ARG...] | run CMD... — the
# local runner, cherry-picked from bee.  bwrap binds the tree rw at /work (ro when
# BEAGLE_RO is set), /usr, /etc and the toolchain ro, $BEAGLE_CCACHE at /ccache
# and $BEAGLE_ROOT at /root when set, then runs .beagle/build.sh <step> [ARG...]
# inside with cwd /work.  Every tree builds at the same path, so caches survive
# a reflink fork.  SAND-26A237Y7pN: `run CMD...` runs CMD instead, its cwd the
# caller's mirrored under the tree, stdio and exit status its own.
# PORT-26A2347BZn: per OS.  Linux: bwrap.  macOS: sandbox-exec, writes only
# to the tree, HOME, ccache and a private tmp.  FreeBSD: `chroot -n` into
# nullfs mounts at the real paths (needs security.bsd.unprivileged_chroot=1,
# vfs.usermount=1, nullfs loaded).  Else, or OpenBSD: bare, with a warning
# (none with BEAGLE_BARE set).
# Only bwrap remaps the tree to /work: builds read $BEAGLE_TREE instead.
# BEAGLE_BIND off Linux: a DST under /root lands under the sandbox HOME, by
# nullfs on FreeBSD, else as a symlink to SRC (macOS also opens SRC to writes).
set -eu
STEP=${1:-build}
[ $# -gt 0 ] && shift
case "$STEP" in configure|build|test|asan|fuzz|run) ;;
    *) echo "sandbox.sh: unknown step '$STEP' (configure|build|test|asan|fuzz|run)" >&2; exit 2 ;;
esac
[ "$STEP" = run ] && [ $# = 0 ] && { echo "sandbox.sh: run: no command" >&2; exit 2; }
TREE=${BEAGLE_WORK:-$(cd "$(dirname "$0")/.." && pwd -P)}
REL=
if [ "$STEP" = run ]; then
    HERE=$(pwd -P)
    case "$HERE/" in "$TREE"/*) REL=${HERE#"$TREE"} ;; esac
else set -- sh .beagle/build.sh "$STEP" "$@"
fi
RT=${LITEJAB:-$(command -v qjab 2>/dev/null || true)}
case "$RT" in /*) RT=$(realpath "$RT"); export LITEJAB="$RT" ;; esac
warn() { echo "sandbox.sh: $*" >&2; }
nobox() { [ -n "${BEAGLE_BARE:-}" ] || warn "$*, running bare (BEAGLE_BARE=1: silently)"; }

# The toolchain off /usr (say ~/bin/ninja) and the bee runtime, one per line.
tools() {
    { OLDIFS=$IFS; IFS=:
      for p in $PATH; do case "$p" in /*) [ -e "$p" ] && echo "$p" ;; esac; done
      IFS=$OLDIFS
      case "$RT" in /*) echo "$RT" ;; esac; } | sort -u
}

# No sandbox: the tree at its own path, HOME and ccache as the sandboxes set.
# BEAGLE_BIND off bwrap -> "SRC DST" lines, a DST under /root moved under the
# sandbox HOME ($1, if any), as bwrap's /root is that HOME.
binds() {
    for b in ${BEAGLE_BIND:-}; do
        s=${b%%:*} d=${b#*:}
        [ -e "$s" ] || { warn "BEAGLE_BIND: no $s"; exit 2; }
        case "$d" in /root|/root/*) [ -n "$1" ] && d=$1${d#/root} ;; esac
        echo "$s $d"
    done
}
# No remapping without bwrap or nullfs: a DST elsewhere is a symlink to SRC.
linkbinds() {
    binds "$1" | while read -r s d; do
        [ "$s" = "$d" ] || [ -e "$d" ] || { mkdir -p "${d%/*}" && ln -s "$s" "$d"; } ||
            warn "BEAGLE_BIND: cannot link $d to $s"
    done
}

bare() {
    cd "$TREE$REL"
    export BEAGLE_TREE="$TREE"
    binds "" >/dev/null
    linkbinds "${BEAGLE_ROOT:-}"
    [ -n "${BEAGLE_ROOT:-}" ] && export HOME="$BEAGLE_ROOT"
    [ -n "${BEAGLE_CCACHE:-}" ] && export CCACHE_DIR="$BEAGLE_CCACHE"
    exec "$@"
}

linux() {
    command -v bwrap >/dev/null 2>&1 || { echo "sandbox.sh: no bwrap" >&2; exit 2; }
    CD=/work$REL N=$#    # the command rides in front; rotated behind bwrap's own at exec
    BIND=--bind
    [ -n "${BEAGLE_RO:-}" ] && BIND=--ro-bind
    set -- "$@" --ro-bind /usr /usr --ro-bind /etc /etc
    # Merged-/usr distros symlink these, Alpine keeps them as real dirs.
    for d in /bin /sbin /lib /lib64 /lib32; do
        if [ -L "$d" ]; then set -- "$@" --symlink "$(readlink "$d")" "$d"
        elif [ -d "$d" ]; then set -- "$@" --ro-bind "$d" "$d"; fi
    done
    set -- "$@" --ro-bind /sys /sys --dev /dev --proc /proc --tmpfs /tmp $BIND "$TREE" /work
    for p in $(tools); do case "$p" in /usr|/usr/*|/bin|/sbin|/lib*|/etc*|/work*) ;;
        *) set -- "$@" --ro-bind "$p" "$p" ;; esac; done
    if [ -n "${BEAGLE_CCACHE:-}" ]; then
        set -- "$@" --bind "$BEAGLE_CCACHE" /ccache --setenv CCACHE_DIR /ccache
    fi
    if [ -n "${BEAGLE_ROOT:-}" ]; then
        set -- "$@" --bind "$BEAGLE_ROOT" /root --setenv HOME /root
    else
        set -- "$@" --setenv HOME /tmp
    fi
    # SAND-26A238VAGJ: BEAGLE_BIND="SRC[:DST] ..." binds host dirs rw at DST (SRC),
    # after HOME, so BEAGLE_BIND=~/src/x.data:/root/src/x.data maps $HOME/src/x.data
    for b in ${BEAGLE_BIND:-}; do
        [ -e "${b%%:*}" ] || { echo "sandbox.sh: BEAGLE_BIND: no ${b%%:*}" >&2; exit 2; }
        set -- "$@" --bind "${b%%:*}" "${b#*:}"
    done
    set -- "$@" --setenv BEAGLE_TREE /work --unshare-all --share-net --die-with-parent --chdir "$CD" --
    while [ "$N" -gt 0 ]; do set -- "$@" "$1"; shift; N=$((N - 1)); done
    exec bwrap "$@"
}

# macOS: deny every file write but the tree (unless BEAGLE_RO), HOME, ccache,
# a fresh tmp and the tty/null devices; reads and network stay open, as bwrap
# leaves the net shared.
SBPL='(version 1)
(allow default)
(deny file-write*)
(allow file-write*
  (subpath (param "TREE")) (subpath (param "HOME")) (subpath (param "CCACHE"))
  (subpath (param "TMP")) (literal "/dev/null") (literal "/dev/zero")
  (literal "/dev/tty") (literal "/dev/dtracehelper") (regex #"^/dev/fd/")
  (regex #"^/dev/ttys"))'
darwin() {
    command -v sandbox-exec >/dev/null 2>&1 || { nobox "no sandbox-exec"; bare "$@"; }
    T=$(mktemp -d "${TMPDIR:-/tmp}/beagle-sbx.XXXXXX")
    T=$(cd "$T" && pwd -P)
    trap 'rm -rf "$T"' EXIT
    W=$TREE; [ -n "${BEAGLE_RO:-}" ] && W=$T
    H=${BEAGLE_ROOT:-$T} C=${BEAGLE_CCACHE:-$T}
    binds "$H" >/dev/null
    linkbinds "$H"
    P=$SBPL
    for s in $(binds "$H" | while read -r s d; do echo "$s"; done); do
        case "$s" in *'"'*|*\\*) warn "BEAGLE_BIND: bad path $s"; exit 2 ;; esac
        P="$P
(allow file-write* (subpath \"$(cd "$s" && pwd -P)\"))"
    done
    set +e
    (cd "$TREE$REL" && BEAGLE_TREE=$TREE HOME=$H TMPDIR=$T CCACHE_DIR=$C &&
     export BEAGLE_TREE HOME TMPDIR CCACHE_DIR && exec sandbox-exec -p "$P" \
        -D TREE="$W" -D HOME="$H" -D CCACHE="$C" -D TMP="$T" "$@")
    exit $?
}

# FreeBSD: a scratch root of nullfs mounts at the real paths, the system and
# the toolchain ro, the tree rw (ro if BEAGLE_RO), HOME and ccache rw, a devfs,
# a private /tmp; `chroot -n` sets no_new_privs, so no setuid escapes.
freebsd() {
    [ "$(sysctl -n security.bsd.unprivileged_chroot 2>/dev/null)" = 1 ] &&
    [ "$(sysctl -n vfs.usermount 2>/dev/null)" = 1 ] && lsvfs nullfs >/dev/null 2>&1 || {
        nobox "no unprivileged chroot (sysctl security.bsd.unprivileged_chroot=1" \
              "vfs.usermount=1, kldload nullfs)"; bare "$@"; }
    R=$(mktemp -d "${TMPDIR:-/tmp}/beagle-sbx.XXXXXX")
    R=$(cd "$R" && pwd -P)
    M=
    out() {   # unmount, newest first; remove the scratch only once all are off
        for m in $M; do umount "$m" 2>/dev/null || umount -f "$m" 2>/dev/null || true; done
        mount -p | awk -v r="$R" 'index($2, r "/") == 1 {f = 1} END {exit !f}' &&
            { warn "mounts left under $R"; return; }
        rm -rf "$R"
    }
    trap out EXIT
    mnt() {   # mnt ro|rw PATH [AT]
        t=$R${3:-$2}
        mkdir -p "$t" && mount_nullfs -o "$1" "$2" "$t" && M="$t $M" ||
            { warn "cannot nullfs $2"; exit 2; }
    }
    # /var/run: ld-elf.so.hints, which finds /usr/local/lib for pkg's binaries.
    for d in /bin /sbin /lib /libexec /usr /etc /var/run; do [ -d "$d" ] && mnt ro "$d"; done
    for p in $(tools | while read -r p; do [ -d "$p" ] && echo "$p" || echo "${p%/*}"; done | sort -u); do
        case "$p" in /usr|/usr/*|/bin|/sbin|/lib*|/etc*|"$TREE"|"$TREE"/*) continue ;; esac
        mkdir -p "$R$p" 2>/dev/null && mount_nullfs -o ro "$p" "$R$p" && M="$R$p $M" ||
            warn "cannot nullfs $p, left out"
    done
    if [ -n "${BEAGLE_RO:-}" ]; then mnt ro "$TREE"; else mnt rw "$TREE"; fi
    H=/tmp
    [ -n "${BEAGLE_ROOT:-}" ] && { mnt rw "$BEAGLE_ROOT"; H=$BEAGLE_ROOT; }
    [ -n "${BEAGLE_CCACHE:-}" ] && mnt rw "$BEAGLE_CCACHE"
    binds "$H" >/dev/null
    for sd in $(binds "$H" | tr ' ' :); do mnt rw "${sd%%:*}" "${sd#*:}"; done
    mkdir -p "$R/dev" && mount -t devfs devfs "$R/dev" && M="$R/dev $M" || { warn "no devfs"; exit 2; }
    mkdir -p -m 1777 "$R/tmp"
    set +e
    BEAGLE_TREE=$TREE HOME=$H TMPDIR=/tmp CCACHE_DIR=${BEAGLE_CCACHE:-} \
        chroot -n "$R" /bin/sh -c 'export BEAGLE_TREE HOME TMPDIR CCACHE_DIR
        [ -n "$CCACHE_DIR" ] || unset CCACHE_DIR; cd "$0" && exec "$@"' "$TREE$REL" "$@"
    rc=$?
    exit $rc
}

case "$(uname -s)" in
    Linux) linux "$@" ;;
    Darwin) darwin "$@" ;;
    FreeBSD) freebsd "$@" ;;
    *) nobox "no sandbox on $(uname -s)"; bare "$@" ;;
esac
