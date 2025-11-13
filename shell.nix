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

    # Task automation
    just

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

    # Cryptography libraries
    libsodium    # For Curve25519, Ed25519, HMAC-SHA256
    openssl      # For AES-128-CTR

    # Network tools for debugging
    tcpdump
    wireshark-cli
  ];

  shellHook = ''
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
