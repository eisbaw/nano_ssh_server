# Makefile for v30-chacha
# v29-p256 with a shared ChaCha20 add/XOR/rotate step, packed quarter-round
# operands, and an unsigned word-indexed keystream loop. The curve, hash,
# packet protocol, ELF layout and compiler flags are unchanged.
# See optimization_log.txt for the measured steps and verification.
# NO libsodium, NO OpenSSL, NO libc. Pure -nostdlib -ffreestanding -static.
# CC=gcc and musl-gcc emit identical bytes: no libc headers or objects are used.

CC ?= musl-gcc
PORT ?= 2222
CFLAGS = -Wall -Wextra -std=c11 -Oz -flto -ffunction-sections -fdata-sections \
         -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector \
         -fmerge-all-constants -fno-ident -finline-small-functions \
         -fshort-enums -fomit-frame-pointer -ffast-math -fno-math-errno \
         -fvisibility=hidden -Wno-unused-result -fno-plt \
         -fno-common -fcf-protection=none \
         -fno-inline-functions-called-once -fno-pie \
         -fno-move-loop-invariants -mtune=nocona -fno-caller-saves \
         -fcall-used-r12 -fno-tree-reassoc -fno-tree-tail-merge \
         -nostdlib -ffreestanding -static -fno-stack-clash-protection \
         -DPORT=$(PORT)
LDFLAGS = -nostdlib -static -Wl,--gc-sections \
          -Wl,--build-id=none -Wl,-z,norelro -Wl,--no-eh-frame-hdr \
          -Wl,-n -Wl,-T,tiny.ld -Wl,--defsym=PORT=$(PORT)

SRCS = main.c fp.c p256.c nolibc.c
TARGET = nano_ssh_server

.PHONY: all clean verify

all: $(TARGET) verify

# Compile + LTO-link in one invocation: passing -Oz to the link step lets the
# LTO code generator optimize for size too (separate .o + plain link loses
# several hundred bytes). The RWX-segment warning is the documented,
# intentional single-PT_LOAD layout from tiny.ld.
HDRS = $(wildcard *.h)

# tiny.ld writes the packed ELF header itself, as data at 0x400000, so the
# linker fills in the entry point and the sizes and objcopy -O binary emits
# the finished file: no post-processing script.  The linked ELF keeps its
# symbols ($(TARGET).elf, for nm/objdump); the raw image is what counts.
$(TARGET): $(SRCS) $(HDRS) tiny.ld Makefile
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET).elf $(LDFLAGS)
	objcopy -O binary $(TARGET).elf $(TARGET)
	@echo "Built $(TARGET) (freestanding static, raw image)"

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
	@echo "✅ v30-chacha built successfully!"
	@echo ""

clean:
	rm -f *.o $(TARGET) $(TARGET).elf
	@echo "Cleaned v30-chacha"
