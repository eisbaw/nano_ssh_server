{ pkgs ? import <nixpkgs> {} }:

let
  # Use the musl-based package set - everything is compiled with musl
  muslPkgs = pkgs.pkgsMusl;

in muslPkgs.mkShell {
  buildInputs = with muslPkgs; [
    # Compiler and build tools (musl-based)
    gcc
    gnumake
    pkg-config

    # Task automation (can use from main pkgs since it's just a tool)
    pkgs.just

    # Cryptography libraries - COMPILED WITH MUSL
    libsodium
    libsodium.static  # Static library built with musl
    openssl
    openssl.static    # Static library built with musl

    # Utilities
    pkgs.xxd
    pkgs.coreutils

    # For testing the result
    pkgs.file
    pkgs.binutils
  ];

  shellHook = ''
    echo "======================================"
    echo "Nano SSH Server - MUSL Build Environment"
    echo "======================================"
    echo ""
    echo "This shell uses musl libc instead of glibc"
    echo "All libraries are compiled with musl"
    echo ""
    echo "Verify musl setup:"
    echo "  gcc --version"
    echo "  ldd --version    # Should say 'musl libc'"
    echo ""
    echo "Build commands:"
    echo "  cd v12-static"
    echo "  make clean"
    echo "  make"
    echo ""
    echo "Verify static binary:"
    echo "  file nano_ssh_server     # Should say 'statically linked'"
    echo "  ldd nano_ssh_server      # Should say 'not a dynamic executable'"
    echo "  stat -c%s nano_ssh_server  # Check size"
    echo ""
    echo "======================================"
    echo ""
  '';

  # Important: Set these to ensure we're using musl
  CC = "${muslPkgs.gcc}/bin/gcc";
  PKG_CONFIG_PATH = "${muslPkgs.libsodium.dev}/lib/pkgconfig:${muslPkgs.openssl.dev}/lib/pkgconfig";
}
