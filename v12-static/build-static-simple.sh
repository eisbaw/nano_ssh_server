#!/usr/bin/env bash
# Simple static build with musl - avoiding glibc header conflicts
set -e

echo "Building static nano_ssh_server with musl..."

# Build libsodium for musl if not exists
if [ ! -f "/tmp/musl-libs/lib/libsodium.a" ]; then
    echo "Building libsodium..."
    cd /tmp
    wget -q https://download.libsodium.org/libsodium/releases/libsodium-1.0.18.tar.gz
    tar xf libsodium-1.0.18.tar.gz
    cd libsodium-1.0.18
    CC=musl-gcc ./configure --enable-minimal --prefix=/tmp/musl-libs --disable-shared --enable-static
    make -j4
    make install
    cd -
fi

# Get musl's actual include path
MUSL_INC=$(musl-gcc -print-file-name=libc.so | xargs dirname | xargs dirname)/include

echo "Using musl include: $MUSL_INC"
echo "Building with explicit paths..."

# Compile with explicit paths - musl first, then only openssl headers
musl-gcc \
    -o /home/user/nano_ssh_server/v12-static/nano_ssh_server.musl \
    /home/user/nano_ssh_server/v12-static/main.c \
    -static \
    -std=c11 \
    -Os \
    -flto \
    -ffunction-sections \
    -fdata-sections \
    -nostdinc \
    -isystem "$MUSL_INC" \
    -isystem /usr/lib/gcc/x86_64-linux-gnu/13/include \
    -isystem /tmp/musl-libs/include \
    -isystem /usr/include \
    -I/usr/include/openssl \
    -L/tmp/musl-libs/lib \
    -L/usr/lib/x86_64-linux-gnu \
    -lsodium \
    -lcrypto \
    -Wl,--gc-sections \
    -Wl,--strip-all \
    -Wl,--as-needed \
    2>&1

if [ $? -eq 0 ]; then
    echo "✓ Build successful!"
    ls -lh /home/user/nano_ssh_server/v12-static/nano_ssh_server.musl
    file /home/user/nano_ssh_server/v12-static/nano_ssh_server.musl
    ldd /home/user/nano_ssh_server/v12-static/nano_ssh_server.musl 2>&1 || echo "(static - good!)"
else
    echo "✗ Build failed"
    exit 1
fi
