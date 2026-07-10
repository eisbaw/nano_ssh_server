{
  pkgs ? (import (builtins.fetchTarball {
    # Pinned to nixos-24.11 (2024-11-30)
    url = "https://github.com/nixos/nixpkgs/archive/057f63b6dc1a2c67301286152eb5af20747a9cb4.tar.gz";
    sha256 = "17srzd2vif6if6mq6k5prd2kw7zhfnh6bg8ywgz28xj99rvgg4xz";
  }) {})
}:

pkgs.mkShell {
  buildInputs = with pkgs; [
    # Compiler and build tools
    gcc
    gnumake
    pkg-config
    # musl-gcc is exposed via shellHook PATH below, NOT as a (native)BuildInput.
    # Adding `musl`/`musl.dev` to either input list propagates musl's lib/ onto
    # NIX_LDFLAGS (-L .../musl/lib), so plain gcc links the non-musl versions
    # against musl's libc.so and they segfault before main(). Verified that
    # nativeBuildInputs leaks too; only the PATH export keeps gcc's link path clean.

    # Task automation
    just

    # v26-genk build step (sstrip.py section-header stripper)
    python3

    # SSH client for testing
    openssh

    # Debugging and profiling
    gdb
    valgrind

    # Testing utilities
    sshpass
    netcat

    # Utilities
    xxd
    coreutils
    bc           # used by `just size-report` for KB calculation

    # Cryptography libraries
    libsodium    # For Curve25519, Ed25519, HMAC-SHA256
    openssl      # For AES-128-CTR

    # Network tools for debugging
    tcpdump
    wireshark-cli
  ];

  shellHook = ''
    # Expose musl-gcc without adding musl libc.so to gcc's default link path
    export PATH="${pkgs.musl.dev}/bin:$PATH"

    echo "======================================"
    echo "Nano SSH Server Development Environment"
    echo "======================================"
    echo ""
    echo "Available tools:"
    echo "  - gcc, make: Compilation"
    echo "  - just: Task automation"
    echo "  - ssh: Client for testing"
    echo "  - gdb, valgrind: Debugging"
    echo "  - sshpass: Automated testing"
    echo ""
    echo "Quick start:"
    echo "  just --list       # Show available commands"
    echo "  just build v0-vanilla"
    echo "  just test v0-vanilla"
    echo ""
    echo "Read CLAUDE.md for development guidelines"
    echo "======================================"
    echo ""
  '';
}
