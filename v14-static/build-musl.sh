#!/usr/bin/env bash
# Build v14-static with musl libc
# This builds libsodium with musl, then links v14-static statically
# v14-crypto doesn't need OpenSSL (custom AES/SHA-256 implementation)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build-musl"
INSTALL_DIR="$BUILD_DIR/install"

# Versions
LIBSODIUM_VERSION="1.0.19"
LIBSODIUM_DIR="libsodium-stable"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $*"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

# Check prerequisites
info "Checking prerequisites..."
command -v musl-gcc >/dev/null 2>&1 || error "musl-gcc not found. Install: apt-get install musl-tools musl-dev"
command -v wget >/dev/null 2>&1 || command -v curl >/dev/null 2>&1 || error "wget or curl required"
command -v make >/dev/null 2>&1 || error "make not found"

# Create build directory
info "Setting up build directory: $BUILD_DIR"
mkdir -p "$BUILD_DIR/src"
mkdir -p "$INSTALL_DIR"

cd "$BUILD_DIR"

# Build libsodium with musl
if [ ! -f "$INSTALL_DIR/lib/libsodium.a" ]; then
    info "Building libsodium $LIBSODIUM_VERSION with musl..."

    cd src
    if [ ! -f "libsodium-$LIBSODIUM_VERSION.tar.gz" ]; then
        info "Downloading libsodium..."
        wget "https://download.libsodium.org/libsodium/releases/libsodium-$LIBSODIUM_VERSION.tar.gz" 2>/dev/null || \
            curl -L -O "https://download.libsodium.org/libsodium/releases/libsodium-$LIBSODIUM_VERSION.tar.gz"
    fi

    if [ ! -d "$LIBSODIUM_DIR" ]; then
        info "Extracting libsodium..."
        tar xzf "libsodium-$LIBSODIUM_VERSION.tar.gz"
    fi

    cd "$LIBSODIUM_DIR"

    info "Configuring libsodium with musl-gcc..."
    CC=musl-gcc ./configure \
        --prefix="$INSTALL_DIR" \
        --enable-minimal \
        --disable-shared \
        --enable-static \
        --disable-pie

    info "Compiling libsodium..."
    make -j$(nproc)
    make install

    info "✓ libsodium built successfully"
    cd "$BUILD_DIR"
else
    info "✓ libsodium already built"
fi

# Build v14-static
info "Building v14-static with musl..."

# Verify source exists
if [ ! -f "$SCRIPT_DIR/main.c" ]; then
    error "Source file not found: $SCRIPT_DIR/main.c"
fi

# Compile with musl-gcc
info "Compiling with musl-gcc..."
musl-gcc \
    -static \
    -std=c11 \
    -Os \
    -flto \
    -ffunction-sections \
    -fdata-sections \
    -fno-unwind-tables \
    -fno-asynchronous-unwind-tables \
    -fno-stack-protector \
    -fmerge-all-constants \
    -fno-ident \
    -finline-small-functions \
    -fshort-enums \
    -fomit-frame-pointer \
    -ffast-math \
    -fno-math-errno \
    -fvisibility=hidden \
    -fno-builtin \
    -fno-plt \
    -fwhole-program \
    -fipa-pta \
    -fno-common \
    -D_POSIX_C_SOURCE=200809L \
    -I"$INSTALL_DIR/include" \
    -o "$SCRIPT_DIR/nano_ssh_server" \
    "$SCRIPT_DIR/main.c" \
    -L"$INSTALL_DIR/lib" \
    -lsodium \
    -Wl,--gc-sections \
    -Wl,--strip-all \
    -Wl,--as-needed \
    -Wl,--hash-style=gnu \
    -Wl,--build-id=none \
    -Wl,-z,norelro \
    -Wl,--no-eh-frame-hdr

info "Stripping binary..."
strip --strip-all \
      --remove-section=.note \
      --remove-section=.comment \
      "$SCRIPT_DIR/nano_ssh_server" 2>/dev/null || true

# Optional: UPX compression
if command -v upx >/dev/null 2>&1; then
    info "Compressing with UPX..."
    cp "$SCRIPT_DIR/nano_ssh_server" "$SCRIPT_DIR/nano_ssh_server.upx"
    upx --best --ultra-brute "$SCRIPT_DIR/nano_ssh_server.upx" 2>&1 || true
fi

# Verification
echo ""
info "======================================"
info "Build Complete!"
info "======================================"
echo ""

info "Binary: $SCRIPT_DIR/nano_ssh_server"
ls -lh "$SCRIPT_DIR/nano_ssh_server"
echo ""

info "File type:"
file "$SCRIPT_DIR/nano_ssh_server"
echo ""

info "Dynamic dependencies:"
ldd "$SCRIPT_DIR/nano_ssh_server" 2>&1 || info "  (none - statically linked)"
echo ""

info "Size analysis:"
size "$SCRIPT_DIR/nano_ssh_server"
MUSL_SIZE=$(stat -c%s "$SCRIPT_DIR/nano_ssh_server" 2>/dev/null || stat -f%z "$SCRIPT_DIR/nano_ssh_server")
echo ""
echo "  Musl static:  $MUSL_SIZE bytes"
echo ""

if [ -f "$SCRIPT_DIR/nano_ssh_server.upx" ]; then
    UPX_SIZE=$(stat -c%s "$SCRIPT_DIR/nano_ssh_server.upx" 2>/dev/null || stat -f%z "$SCRIPT_DIR/nano_ssh_server.upx")
    COMPRESSION=$( echo "scale=1; 100 * ($MUSL_SIZE - $UPX_SIZE) / $MUSL_SIZE" | bc )
    echo "  UPX compressed: $UPX_SIZE bytes (${COMPRESSION}% reduction)"
    echo ""
fi

info "To run:"
echo "  cd $SCRIPT_DIR"
echo "  ./nano_ssh_server"
echo ""

info "======================================"
info "✅ SUCCESS! v14-static built with musl libc."
info "======================================"
