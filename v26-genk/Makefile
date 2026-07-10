# Makefile for v26-genk
# v25-pack + generated constants + ELF golf. One optimization theme per step,
# each documented in optimization_log.txt:
#  - SHA-512 K table (640 B) + initial state (64 B) computed at startup from
#    integer cube/square roots of the first 80 primes (FIPS 180-4 definition)
#  - SHA-256 K table (256 B) + initial state taken as the top 32 bits of the
#    SHA-512 values, so they cost zero extra bytes
#  - Ed25519 base/neutral points built at startup (only base.x stays as data)
#  - KEXINIT name-lists as one packed NUL-separated string (no pointer array)
#  - -Oz, -fcf-protection=none, compile+link in ONE gcc invocation so the
#    optimization flags also reach the LTO link stage
#  - tiny.ld: single RWX PT_LOAD, no NOTE/GNU_STACK phdrs, packed sections
#  - sstrip.py: drop the section header table (metadata only - the loaded
#    image is byte-identical; this is NOT compression)
# NO libsodium, NO OpenSSL, NO libc. Pure -nostdlib -ffreestanding -static.

CC = musl-gcc
# Freestanding: keep our own mem/str (do NOT let GCC assume libc semantics),
# but allow GCC's builtin memcpy/memset codegen which calls our definitions.
CFLAGS = -Wall -Wextra -std=c11 -Oz -flto -ffunction-sections -fdata-sections \
         -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector \
         -fmerge-all-constants -fno-ident -finline-small-functions \
         -fshort-enums -fomit-frame-pointer -ffast-math -fno-math-errno \
         -fvisibility=hidden -Wno-unused-result -fno-plt \
         -fipa-pta -fno-common -fcf-protection=none \
         -nostdlib -ffreestanding -static -fno-stack-clash-protection
LDFLAGS = -nostdlib -static -Wl,--gc-sections -Wl,--strip-all \
          -Wl,--build-id=none -Wl,-z,norelro -Wl,--no-eh-frame-hdr \
          -Wl,-n -Wl,-T,tiny.ld

SRCS = main.c f25519.c fprime.c ed25519.c edsign.c sha512.c c25519.c nolibc.c
TARGET = nano_ssh_server

.PHONY: all clean verify

all: $(TARGET) verify

# Compile + LTO-link in one invocation: passing -Oz to the link step lets the
# LTO code generator optimize for size too (separate .o + plain link loses
# several hundred bytes). 2>&1 filter: the RWX-segment warning is the
# documented, intentional single-PT_LOAD layout from tiny.ld.
$(TARGET): $(SRCS) tiny.ld sstrip.py
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
	@echo "✅ v26-genk built successfully!"
	@echo ""

clean:
	rm -f *.o $(TARGET)
	@echo "Cleaned v26-genk"
