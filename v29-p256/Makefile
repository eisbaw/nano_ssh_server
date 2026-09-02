# Makefile for v29-p256
# v28-chapoly with the key exchange and host key moved to NIST P-256:
# ecdh-sha2-nistp256 + ecdsa-sha2-nistp256 + chacha20-poly1305@openssh.com.
# One optimization theme per step, each measured in optimization_log.txt:
#  - one curve in one coordinate system for both the key exchange and the
#    signature; ECDSA hashes with SHA-256, so SHA-512 is gone and only one
#    hash core remains
#  - one modular multiplier (fp.c, big-endian, any 256-bit modulus) for the
#    P-256 field, the P-256 group order and Poly1305 - no dedicated field
#    code, no carry folding, no square root, no curve mapping
#  - the double-and-add step as a 41-instruction program over a flat file of
#    field elements, run by a tiny interpreter (3 bytes per field operation)
#  - SHA-256 constants generated at startup from 128-bit integer root
#    extraction (GCC __int128)
#  - inherited from v26..v28: -Oz + single-invocation LTO, tiny.ld,
#    sstrip.py, bswap + unaligned wire access, in-place packet receive
# NO libsodium, NO OpenSSL, NO libc. Pure -nostdlib -ffreestanding -static.
#
# CC is only a wrapper here: -nostdlib -ffreestanding means no libc headers
# or objects are used, so musl-gcc and plain gcc emit byte-identical output.
# Override with `make CC=gcc` outside the Nix shell.

CC ?= musl-gcc
CFLAGS = -Wall -Wextra -std=c11 -Oz -flto -ffunction-sections -fdata-sections \
         -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector \
         -fmerge-all-constants -fno-ident -finline-small-functions \
         -fshort-enums -fomit-frame-pointer -ffast-math -fno-math-errno \
         -fvisibility=hidden -Wno-unused-result -fno-plt \
         -fipa-pta -fno-common -fcf-protection=none \
         -fno-inline-functions-called-once \
         -nostdlib -ffreestanding -static -fno-stack-clash-protection
LDFLAGS = -nostdlib -static -Wl,--gc-sections -Wl,--strip-all \
          -Wl,--build-id=none -Wl,-z,norelro -Wl,--no-eh-frame-hdr \
          -Wl,-n -Wl,-T,tiny.ld

SRCS = main.c fp.c p256.c nolibc.c
TARGET = nano_ssh_server

.PHONY: all clean verify

all: $(TARGET) verify

# Compile + LTO-link in one invocation: passing -Oz to the link step lets the
# LTO code generator optimize for size too (separate .o + plain link loses
# several hundred bytes). The RWX-segment warning is the documented,
# intentional single-PT_LOAD layout from tiny.ld.
HDRS = $(wildcard *.h)

$(TARGET): $(SRCS) $(HDRS) tiny.ld sstrip.py
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)
	python3 sstrip.py $(TARGET)
	@echo "Built $(TARGET) (freestanding static, section headers stripped)"

verify: $(TARGET)
	@echo ""
	@echo "=== Verification ==="
	@echo -n "File type: "
	@file $(TARGET)
	@echo ""
	@echo "=== Size ==="
	@ls -lh $(TARGET)
	@stat -c "Size: %s bytes" $(TARGET) 2>/dev/null || stat -f "Size: %z bytes" $(TARGET)
	@echo ""
	@echo "✅ v29-p256 built successfully!"
	@echo ""

clean:
	rm -f *.o $(TARGET)
	@echo "Cleaned v29-p256"
