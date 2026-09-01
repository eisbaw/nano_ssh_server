# Makefile for v28-chapoly
# v27-onecurve with the packet cipher changed from aes128-ctr + hmac-sha2-256
# to chacha20-poly1305@openssh.com, plus the folding that follows from it.
# One optimization theme per step, each measured in optimization_log.txt:
#  - Poly1305 is h = (h + c) * r mod 2^130-5, i.e. the generic modular
#    arithmetic fprime.c already links for Ed25519, so the AEAD costs a
#    ChaCha20 core and 3 bytes of modulus; the AES core and HMAC are gone
#  - the SHA constant generator, the field normalization, the scalar
#    reduction and the field inverse likewise run on routines already linked
#    (fprime_mul, raw_try_sub, shift-and-reduce, one square-and-multiply)
#  - protocol golf: bswap + unaligned access for wire words, whole-message
#    userauth match, packets received in place, a global connection socket
#  - inherited from v26/v27: SHA K tables generated from prime roots at
#    startup, one Curve25519 implementation, -Oz + single-invocation LTO,
#    tiny.ld, sstrip.py
# NO libsodium, NO OpenSSL, NO libc. Pure -nostdlib -ffreestanding -static.
#
# CC is only a wrapper here: -nostdlib -ffreestanding means no libc headers
# or objects are used, so musl-gcc and plain gcc emit byte-identical output
# (verified). Override with `make CC=gcc` outside the Nix shell.

CC ?= musl-gcc
# Freestanding: keep our own mem/str (do NOT let GCC assume libc semantics),
# but allow GCC's builtin memcpy/memset codegen which calls our definitions.
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

SRCS = main.c f25519.c fprime.c ed25519.c edsign.c sha512.c x25519_ed.c nolibc.c
TARGET = nano_ssh_server

.PHONY: all clean verify

all: $(TARGET) verify

# Compile + LTO-link in one invocation: passing -Oz to the link step lets the
# LTO code generator optimize for size too (separate .o + plain link loses
# several hundred bytes). 2>&1 filter: the RWX-segment warning is the
# documented, intentional single-PT_LOAD layout from tiny.ld.
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
	@echo "✅ v28-chapoly built successfully!"
	@echo ""

clean:
	rm -f *.o $(TARGET)
	@echo "Cleaned v28-chapoly"
