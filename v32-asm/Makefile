# Makefile for v32-asm
# The server of v31-trim - same key exchange, host key, cipher and protocol
# steps - written directly in x86-64 assembly: one source file, main.S,
# which also spells out the ELF header, so the linker only has to resolve
# addresses and write the bytes out.
# NO libsodium, NO OpenSSL, NO libc, NO compiler-generated code, and no
# executable compression: the file is the image the kernel maps.
# See optimization_log.txt for the measured steps and the verification.
# test_main.S is only assembled into the unit-test build (-DTEST) that
# tests/test_asm.py makes for itself; the server does not contain it.

CC ?= cc
LD ?= ld
PORT ?= 2222

TARGET = nano_ssh_server

.PHONY: all clean verify

all: $(TARGET) verify

# The C compiler driver is only used to run the preprocessor and the
# assembler over main.S (-DPORT picks the listening port).  flat.ld puts the
# one section at 0x400000 and --oformat binary writes it out as it is.
# $(TARGET).elf holds the same bytes at the same addresses in an ordinary
# ELF file with symbols (for nm, objdump, gdb, valgrind); it is not shipped.
# (Its one segment is read-write-execute like the image's; binutils before
# 2.39 does not know the option that silences the warning about that.)
$(TARGET): main.S flat.ld debug.ld Makefile
	$(CC) -c -DPORT=$(PORT) main.S -o main.o
	$(LD) -T flat.ld --oformat binary -o $(TARGET) main.o
	$(LD) -T debug.ld --no-warn-rwx-segments -o $(TARGET).elf main.o 2>/dev/null || \
		$(LD) -T debug.ld -o $(TARGET).elf main.o
	chmod +x $(TARGET)
	@echo "Built $(TARGET) (hand-written, flat image)"

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
	@echo "✅ v32-asm built successfully!"
	@echo ""

clean:
	rm -f *.o $(TARGET) $(TARGET).elf
	@echo "Cleaned v32-asm"
