#!/usr/bin/env bash
# test_cross.sh - run v33-portable's core on processors that differ from
# x86-64 in byte order and word size: the test build of the core
# (test_main.c) is cross-compiled and run under qemu's user-mode emulation
# through all the checks of tests/test_portable.py, and the server itself
# (nanossh.c + net_posix.c) is cross-compiled, started under qemu and
# logged into with the real OpenSSH client.
#
#   s390x   64-bit, big-endian        arm    32-bit, little-endian
#   ppc64   64-bit, big-endian        i686   32-bit, little-endian
#
# Used by `just test-portable-cross`, from the project's nix-shell.  The
# compilers and qemu come from cross.nix (same nixpkgs as shell.nix, from
# the binary cache; about 500 MB on first use).
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
root=$(dirname "$here")
iters=${ITERS:-40}
port=${PORT:-2222}
work=$(mktemp -d)
server=
trap '[ -n "$server" ] && kill "$server" 2>/dev/null; rm -r "$work"' EXIT

tool() {        # tool <attribute>: the store path of one cross.nix entry
    nix-build "$here/cross.nix" -A "$1" --no-out-link
}

qemu=$(tool qemu)
fail=0
for target in "s390x s390x-unknown-linux-gnu-gcc qemu-s390x" \
              "ppc64 powerpc64-unknown-linux-gnuabielfv2-gcc qemu-ppc64" \
              "arm armv7l-unknown-linux-gnueabihf-gcc qemu-arm" \
              "i686 gcc qemu-i386"; do
    set -- $target
    name=$1
    cc=$(tool "$name")/bin/$2
    run=$qemu/bin/$3

    python3 "$root/tests/test_portable.py" --cc "$cc" --run "$run" \
        --label "$name under qemu" --iters "$iters"

    "$cc" -std=c99 -pedantic -Wall -Wextra -Werror -O2 \
        -D_POSIX_C_SOURCE=200809L -DPORT="$port" \
        "$here/nanossh.c" "$here/net_posix.c" -o "$work/server-$name"
    "$run" "$work/server-$name" &
    server=$!
    out=
    for try in 1 2 3 4 5 6; do          # the emulated start-up takes a while
        sleep 5
        out=$(sshpass -p password123 ssh -o StrictHostKeyChecking=no \
            -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR \
            -o ConnectTimeout=120 -p "$port" user@localhost 2>&1 || true)
        case $out in *"Hello World"*) break ;; esac
    done
    kill "$server" 2>/dev/null || true
    wait "$server" 2>/dev/null || true
    server=
    case $out in
    *"Hello World"*) echo "v33-portable ($name under qemu): PASS - OpenSSH login printed Hello World" ;;
    *) echo "v33-portable ($name under qemu): FAIL - login: $out"; fail=1 ;;
    esac
done
exit $fail
