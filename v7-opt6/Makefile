# Makefile for v7-opt6
# Phase 3: Advanced size optimization with function attributes
# Focus: Additional compiler flags from 2024 research

CC = gcc
# Advanced size optimization flags based on research:
# -Os: Optimize for size
# -flto: Link-time optimization
# -ffunction-sections -fdata-sections: Separate sections for linker GC
# -fno-unwind-tables -fno-asynchronous-unwind-tables: Remove unwind tables
# -fno-stack-protector: Remove stack protector
# -fmerge-all-constants: Merge identical constants
# -fno-ident: Remove compiler identification
# -finline-small-functions: Inline small functions
# -fshort-enums: Use smallest enum size (NEW - embedded optimization)
# -fomit-frame-pointer: Remove frame pointer (NEW - saves stack ops)
# -ffast-math: Fast non-IEEE math (NEW - smaller code)
# -fno-math-errno: No errno for math operations (NEW)
# -fvisibility=hidden: Hide symbols by default (NEW - smaller symbol table)
# -Wno-unused-result: Suppress warnings for unused results
CFLAGS = -Wall -Wextra -std=c11 -Os -flto -ffunction-sections -fdata-sections \
         -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector \
         -fmerge-all-constants -fno-ident -finline-small-functions \
         -fshort-enums -fomit-frame-pointer -ffast-math -fno-math-errno \
         -fvisibility=hidden -Wno-unused-result -D_POSIX_C_SOURCE=200809L
# Advanced linker flags:
# -Wl,--gc-sections: Garbage collect unused sections
# -Wl,--strip-all: Strip all symbols
# -Wl,--as-needed: Only link needed libraries
# -Wl,--hash-style=gnu: Use GNU hash style (smaller)
# -Wl,--build-id=none: Remove build ID
# -Wl,-z,norelro: Disable relocation read-only (NEW - saves space)
# -Wl,--no-eh-frame-hdr: Remove exception frame header (NEW)
LDFLAGS = -lsodium -lcrypto -Wl,--gc-sections -Wl,--strip-all -Wl,--as-needed \
          -Wl,--hash-style=gnu -Wl,--build-id=none -Wl,-z,norelro -Wl,--no-eh-frame-hdr

# Source files (add as you implement)
SRCS = main.c
OBJS = $(SRCS:.c=.o)
TARGET = nano_ssh_server

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Built $(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	@echo "Cleaned v7-opt6"

# Help
help:
	@echo "v7-opt6 Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all (default) - Build nano_ssh_server"
	@echo "  clean         - Remove build artifacts"
	@echo "  help          - Show this help"
	@echo ""
	@echo "To build: make"
	@echo "To run:   ./nano_ssh_server"
	@echo ""
