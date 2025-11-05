#!/bin/bash
# build-musl.sh - Build nano_ssh_server with musl libc
# This creates a truly minimal static binary without glibc bloat

set -e

echo "🔨 Building Nano SSH Server with musl..."

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Build directories
MUSL_PREFIX="/tmp/musl-libs"
BUILD_DIR="/tmp/musl-build"

echo -e "${BLUE}Step 1/4: Checking musl-gcc...${NC}"
if ! which musl-gcc > /dev/null; then
    echo -e "${RED}Error: musl-gcc not found${NC}"
    echo "Install with: apt-get install musl-tools musl-dev"
    exit 1
fi
echo -e "${GREEN}✓ musl-gcc found${NC}"

echo -e "${BLUE}Step 2/4: Building libsodium with musl...${NC}"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [ ! -f "$MUSL_PREFIX/lib/libsodium.a" ]; then
    echo "  Downloading libsodium 1.0.18..."
    wget -q https://download.libsodium.org/libsodium/releases/libsodium-1.0.18.tar.gz
    tar xf libsodium-1.0.18.tar.gz
    cd libsodium-1.0.18

    echo "  Configuring..."
    CC=musl-gcc ./configure --enable-minimal --prefix="$MUSL_PREFIX" --disable-shared --enable-static > /dev/null

    echo "  Compiling (this takes ~1 min)..."
    make -j4 > /dev/null 2>&1
    make install > /dev/null

    cd ..
    echo -e "${GREEN}✓ libsodium built and installed${NC}"
else
    echo -e "${GREEN}✓ libsodium already built${NC}"
fi

echo -e "${BLUE}Step 3/4: Using system OpenSSL (statically linked)...${NC}"
# Note: We use the existing glibc OpenSSL static libraries
# Building OpenSSL with musl requires patches
# For now, mixing one glibc library is acceptable
if [ ! -f /usr/lib/x86_64-linux-gnu/libcrypto.a ]; then
    echo -e "${RED}Error: libcrypto.a not found${NC}"
    echo "Install with: apt-get install libssl-dev"
    exit 1
fi
echo -e "${GREEN}✓ OpenSSL static library found${NC}"

echo -e "${BLUE}Step 4/4: Building nano_ssh_server...${NC}"
cd /home/user/nano_ssh_server/v12-static

musl-gcc -o nano_ssh_server.musl main.c \
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
    -I"$MUSL_PREFIX/include" \
    -I/usr/include/x86_64-linux-gnu \
    -I/usr/include/openssl \
    -L"$MUSL_PREFIX/lib" \
    -L/usr/lib/x86_64-linux-gnu \
    -lsodium \
    -lcrypto \
    -lpthread \
    -Wl,--gc-sections \
    -Wl,--strip-all \
    -Wl,--as-needed \
    -Wl,--hash-style=gnu \
    -Wl,--build-id=none \
    -Wl,-z,norelro \
    -Wl,--no-eh-frame-hdr

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Build successful!${NC}"

    # Verify
    echo ""
    echo "📊 Results:"
    ls -lh nano_ssh_server.musl

    echo ""
    echo "🔍 Binary info:"
    file nano_ssh_server.musl

    echo ""
    echo "📦 Dependencies:"
    ldd nano_ssh_server.musl 2>&1 || echo "(static - no dependencies)"

    echo ""
    echo "📏 Size comparison:"
    if [ -f nano_ssh_server.glibc ]; then
        echo "  glibc:  $(stat -c%s nano_ssh_server.glibc) bytes"
    fi
    echo "  musl:   $(stat -c%s nano_ssh_server.musl) bytes"

    echo ""
    echo -e "${GREEN}✨ Done! Binary: v12-static/nano_ssh_server.musl${NC}"
else
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi
