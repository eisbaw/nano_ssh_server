#!/bin/bash
# Clean static build with musl - using correct musl headers
set -e

echo "🔨 Building static nano_ssh_server with musl libc..."

# Build libsodium for musl if not exists
if [ ! -f "/tmp/musl-libs/lib/libsodium.a" ]; then
    echo "Building libsodium..."
    mkdir -p /tmp/musl-libs
    cd /tmp
    if [ ! -f "libsodium-1.0.18.tar.gz" ]; then
        wget -q https://download.libsodium.org/libsodium/releases/libsodium-1.0.18.tar.gz
    fi
    tar xf libsodium-1.0.18.tar.gz
    cd libsodium-1.0.18
    CC=musl-gcc ./configure --enable-minimal --prefix=/tmp/musl-libs --disable-shared --enable-static
    make -j4
    make install
    echo "✓ libsodium built"
fi

cd /home/user/nano_ssh_server/v12-static

echo "Compiling with musl..."

# Use musl's headers exclusively, then add OpenSSL
musl-gcc \
    -o nano_ssh_server.musl \
    main.c \
    -static \
    -std=c11 \
    -Os \
    -flto \
    -ffunction-sections \
    -fdata-sections \
    -fno-unwind-tables \
    -fno-asynchronous-unwind-tables \
    -fmerge-all-constants \
    -fomit-frame-pointer \
    -nostdinc \
    -isystem /usr/include/x86_64-linux-musl \
    -isystem /usr/lib/gcc/x86_64-linux-gnu/13/include \
    -isystem /tmp/musl-libs/include \
    -I/usr/include \
    -L/tmp/musl-libs/lib \
    -L/usr/lib/x86_64-linux-gnu \
    -lsodium \
    -lcrypto \
    -Wl,--gc-sections \
    -Wl,--strip-all \
    -Wl,--as-needed \
    -Wl,--hash-style=gnu \
    -Wl,--build-id=none

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Build successful!"
    echo ""
    echo "📊 Binary info:"
    ls -lh nano_ssh_server.musl
    echo ""
    file nano_ssh_server.musl
    echo ""
    echo "📦 Dependencies check:"
    ldd nano_ssh_server.musl 2>&1 || echo "✓ Statically linked!"
    echo ""
    echo "✨ Done: v12-static/nano_ssh_server.musl"
else
    echo "✗ Build failed"
    exit 1
fi
