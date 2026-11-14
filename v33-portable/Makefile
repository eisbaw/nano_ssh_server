# Makefile for v33-portable
# The server of v31-trim / v32-asm with its core (nanossh.c) in portable
# ISO C99 - no assembly, no compiler extensions, no assumptions about byte
# order, alignment or word size - behind a five-function platform
# interface (nanossh.h).
#
#   nano_ssh_server        nanossh.c + net_linux_x86_64.c + tiny.ld: no C
#                          library, Linux system calls on x86-64.  This is
#                          the binary the size comparison counts.  No
#                          executable compression: the file is the image.
#   nano_ssh_server.posix  nanossh.c + net_posix.c against the C library:
#                          the same server for any POSIX system.
#
# On a machine that is not x86-64 Linux, nano_ssh_server is the POSIX
# build.  See optimization_log.txt for the sizes and the verification
# (sanitizers, big-endian and 32-bit targets).

CC ?= cc
PORT ?= 2222
ARCH := $(shell uname -sm)

TARGET = nano_ssh_server
CORE = nanossh.c nanossh.h

# The core is compiled as strict C99 with warnings as errors in both
# builds; the freestanding platform file needs GNU C for its system calls.
# The optimisation flags behind -Oz are from a sweep: each was kept only
# if it made the binary smaller (optimization_log.txt), and the result is
# what the tests run.  -fzero-call-used-regs=skip switches off a hardening
# default of the Nix compiler wrapper, so that the binary is the same
# inside and outside nix-shell (net_linux_x86_64.h deals with another).
WARN = -Wall -Wextra -Werror
TINY_CFLAGS = $(WARN) -Oz -flto -ffunction-sections -fdata-sections \
         -fno-unwind-tables -fno-asynchronous-unwind-tables \
         -fno-stack-protector -fno-ident -fomit-frame-pointer \
         -fvisibility=hidden -fno-plt -fno-common -fcf-protection=none \
         -fno-pie -fzero-call-used-regs=skip \
         -fno-inline-functions-called-once -fno-move-loop-invariants \
         -mtune=core2 -fno-caller-saves -fno-tree-reassoc -fno-ssa-phiopt \
         -fira-region=all -freorder-blocks-algorithm=simple -fno-tree-ccp \
         -nostdlib -ffreestanding -static -fno-stack-clash-protection \
         -DPORT=$(PORT) -DNANOSSH_CONFIG_FILE='"net_linux_x86_64.h"'
TINY_LDFLAGS = -nostdlib -static -Wl,--gc-sections \
          -Wl,--build-id=none -Wl,-z,norelro -Wl,--no-eh-frame-hdr \
          -Wl,-T,tiny.ld -Wl,--defsym=PORT=$(PORT)
POSIX_CFLAGS = -std=c99 -pedantic $(WARN) -O2 -D_POSIX_C_SOURCE=200809L \
          -DPORT=$(PORT)

.PHONY: all clean verify

all: $(TARGET) $(TARGET).posix verify

$(TARGET).posix: $(CORE) net_posix.c Makefile
	$(CC) $(POSIX_CFLAGS) nanossh.c net_posix.c -o $@

ifeq ($(ARCH),Linux x86_64)
# Two compilations, so that the core can be held to -std=c99 -pedantic;
# -flto and the same options at the link let the code generator see both.
# The linked ELF, $(TARGET).elf, holds the same bytes at the same
# addresses behind an ordinary ELF header and keeps its symbols: it runs
# too, and is what nm, objdump, gdb and valgrind can work with.  The raw
# image is what counts.  (The read-write-execute segment tiny.ld asks for
# is intentional.)
$(TARGET): $(CORE) net_linux_x86_64.c net_linux_x86_64.h tiny.ld Makefile
	$(CC) -std=c99 -pedantic $(TINY_CFLAGS) -c nanossh.c -o nanossh.o
	$(CC) -std=gnu99 $(TINY_CFLAGS) -c net_linux_x86_64.c -o net_linux_x86_64.o
	$(CC) $(TINY_CFLAGS) nanossh.o net_linux_x86_64.o -o $(TARGET).elf \
		$(TINY_LDFLAGS) -Wl,--no-warn-rwx-segments 2>/dev/null || \
	$(CC) $(TINY_CFLAGS) nanossh.o net_linux_x86_64.o -o $(TARGET).elf \
		$(TINY_LDFLAGS)
	objcopy -O binary $(TARGET).elf $(TARGET)
	chmod +x $(TARGET)
	@echo "Built $(TARGET) (freestanding static, raw image)"
else
$(TARGET): $(TARGET).posix
	cp $(TARGET).posix $(TARGET)
	@echo "Built $(TARGET) (POSIX build: this is not x86-64 Linux)"
endif

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
	@echo "✅ v33-portable built successfully!"
	@echo ""

clean:
	rm -f *.o $(TARGET) $(TARGET).elf $(TARGET).posix
	@echo "Cleaned v33-portable"
