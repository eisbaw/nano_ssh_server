# Makefile for v23-min
# v23-scratch's tight, malloc-free main + v23-nolibc's FREESTANDING syscall
# layer (no libc at all). Replaces musl libc with a tiny syscall layer
# (nolibc.c/.h): direct Linux x86-64 syscalls + minimal mem/str + _start.
# The scratch main uses fixed buffers (no malloc), so the arena allocator is
# unreferenced and removed by --gc-sections.
# NO libsodium, NO OpenSSL, NO libc. Pure -nostdlib -ffreestanding -static.

CC = musl-gcc
# Freestanding: keep our own mem/str (do NOT let GCC assume libc semantics),
# but allow GCC's builtin memcpy/memset codegen which calls our definitions.
CFLAGS = -Wall -Wextra -std=c11 -Os -flto -ffunction-sections -fdata-sections \
         -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector \
         -fmerge-all-constants -fno-ident -finline-small-functions \
         -fshort-enums -fomit-frame-pointer -ffast-math -fno-math-errno \
         -fvisibility=hidden -Wno-unused-result -fno-plt \
         -fipa-pta -fno-common \
         -nostdlib -ffreestanding -static -fno-stack-clash-protection
LDFLAGS = -nostdlib -static -e _start -Wl,--gc-sections -Wl,--strip-all \
          -Wl,--as-needed -Wl,--hash-style=gnu -Wl,--build-id=none \
          -Wl,-z,norelro -Wl,--no-eh-frame-hdr -Wl,-z,noseparate-code

SRCS = main.c f25519.c fprime.c ed25519.c edsign.c sha512.c c25519.c nolibc.c
OBJS = $(SRCS:.c=.o)
TARGET = nano_ssh_server
TARGET_COMPRESSED = nano_ssh_server.upx

.PHONY: all clean compress verify

all: $(TARGET) verify

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Built $(TARGET) (musl static, uncompressed)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

compress: $(TARGET)
	@cp $(TARGET) $(TARGET_COMPRESSED)
	@upx --best --ultra-brute $(TARGET_COMPRESSED) 2>&1 || true
	@echo "Compressed: $(TARGET_COMPRESSED)"
	@ls -lh $(TARGET) $(TARGET_COMPRESSED) 2>/dev/null || true

verify: $(TARGET)
	@echo ""
	@echo "=== Verification ==="
	@echo -n "File type: "
	@file $(TARGET)
	@echo -n "Dynamic deps: "
	@ldd $(TARGET) 2>&1 || echo "(none - statically linked)"
	@echo ""
	@echo "=== Size Analysis ==="
	@size $(TARGET)
	@echo ""
	@ls -lh $(TARGET)
	@stat -c "Size: %s bytes" $(TARGET) 2>/dev/null || stat -f "Size: %z bytes" $(TARGET)
	@echo ""
	@echo "✅ Musl static binary built successfully!"
	@echo ""

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET_COMPRESSED)
	@echo "Cleaned v22-static"
