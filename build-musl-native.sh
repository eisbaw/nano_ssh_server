#!/usr/bin/env bash
# Build nano_ssh_server with musl libc on a glibc host
# This builds libsodium and OpenSSL with musl, then links everything statically

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build-musl"
INSTALL_DIR="$BUILD_DIR/install"

# Versions
LIBSODIUM_VERSION="1.0.19"
LIBSODIUM_DIR="libsodium-stable"  # Tarball extracts to this name
OPENSSL_VERSION="3.0.13"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

info() { echo -e "${GREEN}[INFO]${NC} $*"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

# Check prerequisites
info "Checking prerequisites..."
command -v musl-gcc >/dev/null 2>&1 || error "musl-gcc not found. Install with: apt-get install musl-tools musl-dev"
command -v wget >/dev/null 2>&1 || warn "wget not found, trying curl"
command -v make >/dev/null 2>&1 || error "make not found"

# Create build directory
info "Setting up build directory: $BUILD_DIR"
mkdir -p "$BUILD_DIR/src"
mkdir -p "$INSTALL_DIR"

cd "$BUILD_DIR"

#
# Build libsodium with musl
#
if [ ! -f "$INSTALL_DIR/lib/libsodium.a" ]; then
    info "Building libsodium $LIBSODIUM_VERSION with musl..."

    cd src
    if [ ! -f "libsodium-$LIBSODIUM_VERSION.tar.gz" ]; then
        info "Downloading libsodium..."
        wget "https://download.libsodium.org/libsodium/releases/libsodium-$LIBSODIUM_VERSION.tar.gz" || \
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

#
# Build OpenSSL with musl
#
if [ ! -f "$INSTALL_DIR/lib/libcrypto.a" ]; then
    info "Building OpenSSL $OPENSSL_VERSION with musl..."

    cd src
    if [ ! -f "openssl-$OPENSSL_VERSION.tar.gz" ]; then
        info "Downloading OpenSSL..."
        wget "https://www.openssl.org/source/openssl-$OPENSSL_VERSION.tar.gz" || \
            curl -L -O "https://www.openssl.org/source/openssl-$OPENSSL_VERSION.tar.gz"
    fi

    if [ ! -d "openssl-$OPENSSL_VERSION" ]; then
        info "Extracting OpenSSL..."
        tar xzf "openssl-$OPENSSL_VERSION.tar.gz"
    fi

    cd "openssl-$OPENSSL_VERSION"

    info "Configuring OpenSSL with musl-gcc..."
    CC=musl-gcc ./Configure linux-x86_64 \
        --prefix="$INSTALL_DIR" \
        --libdir=lib \
        no-shared \
        no-tests \
        no-ui-console \
        -static \
        -Os

    info "Compiling OpenSSL (this may take a while)..."
    make -j$(nproc)
    make install_sw install_ssldirs

    info "✓ OpenSSL built successfully"
    cd "$BUILD_DIR"
else
    info "✓ OpenSSL already built"
fi

#
# Build nano_ssh_server
#
info "Building nano_ssh_server with musl..."

# Verify source exists
if [ ! -f "$SCRIPT_DIR/v12-static/main.c" ]; then
    error "Source file not found: $SCRIPT_DIR/v12-static/main.c"
fi

# Compile with musl-gcc
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
    -D_POSIX_C_SOURCE=200809L \
    -I"$INSTALL_DIR/include" \
    -o nano_ssh_server.musl \
    "$SCRIPT_DIR/v12-static/main.c" \
    -L"$INSTALL_DIR/lib" \
    -lsodium \
    -lcrypto \
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
      nano_ssh_server.musl 2>/dev/null || true

#
# Verification
#
info "======================================"
info "Build Complete!"
info "======================================"
echo ""

info "Binary: $BUILD_DIR/nano_ssh_server.musl"
ls -lh nano_ssh_server.musl
echo ""

info "File type:"
file nano_ssh_server.musl
echo ""

info "Dynamic dependencies:"
ldd nano_ssh_server.musl 2>&1 || info "  (none - statically linked)"
echo ""

info "Size comparison:"
if [ -f "$SCRIPT_DIR/v12-static/nano_ssh_server" ]; then
    GLIBC_SIZE=$(stat -c%s "$SCRIPT_DIR/v12-static/nano_ssh_server" 2>/dev/null || stat -f%z "$SCRIPT_DIR/v12-static/nano_ssh_server")
    MUSL_SIZE=$(stat -c%s "nano_ssh_server.musl" 2>/dev/null || stat -f%z "nano_ssh_server.musl")
    REDUCTION=$(echo "scale=1; 100 * ($GLIBC_SIZE - $MUSL_SIZE) / $GLIBC_SIZE" | bc)

    echo "  Glibc static: $GLIBC_SIZE bytes ($(numfmt --to=iec-i --suffix=B $GLIBC_SIZE 2>/dev/null || echo $GLIBC_SIZE))"
    echo "  Musl static:  $MUSL_SIZE bytes ($(numfmt --to=iec-i --suffix=B $MUSL_SIZE 2>/dev/null || echo $MUSL_SIZE))"
    echo "  Reduction:    ${REDUCTION}%"
else
    MUSL_SIZE=$(stat -c%s "nano_ssh_server.musl" 2>/dev/null || stat -f%z "nano_ssh_server.musl")
    echo "  Musl static:  $MUSL_SIZE bytes"
fi
echo ""

info "To test:"
echo "  cd $BUILD_DIR"
echo "  ./nano_ssh_server.musl"
echo ""

info "To install:"
echo "  cp nano_ssh_server.musl /usr/local/bin/nano_ssh_server"
echo ""

info "To copy to project root:"
echo "  cp $BUILD_DIR/nano_ssh_server.musl $SCRIPT_DIR/"
echo ""

info "======================================"
info "✅ SUCCESS! Musl binary built natively on glibc host."
info "======================================"
