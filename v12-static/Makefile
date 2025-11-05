# Makefile for v12-static
# Phase 3: Static linking + minification
# Focus: Fully static binary with all optimizations + optional UPX compression
# Note: Currently using glibc. See MUSL_ANALYSIS.md for musl/diet alternatives

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
# -fshort-enums: Use smallest enum size (embedded optimization)
# -fomit-frame-pointer: Remove frame pointer (saves stack ops)
# -ffast-math: Fast non-IEEE math (smaller code)
# -fno-math-errno: No errno for math operations
# -fvisibility=hidden: Hide symbols by default (smaller symbol table)
# -fno-builtin: Don't use builtin functions (reduces libc deps)
# -fno-plt: No PLT indirection (smaller code)
# -fwhole-program: Whole program optimization
# -fipa-pta: Interprocedural pointer analysis
# -fno-common: Place uninitialized globals in BSS
CFLAGS = -Wall -Wextra -std=c11 -Os -flto -ffunction-sections -fdata-sections \
         -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector \
         -fmerge-all-constants -fno-ident -finline-small-functions \
         -fshort-enums -fomit-frame-pointer -ffast-math -fno-math-errno \
         -fvisibility=hidden -Wno-unused-result -fno-builtin -fno-plt \
         -fwhole-program -fipa-pta -fno-common \
         -D_POSIX_C_SOURCE=200809L
# Advanced linker flags for static linking:
# -static: Create standalone binary with all libraries linked statically
# -L/usr/lib/x86_64-linux-gnu: Library path
# -Wl,--gc-sections: Garbage collect unused sections (critical for static)
# -Wl,--strip-all: Strip all symbols
# -Wl,--as-needed: Only link needed libraries
# -Wl,--hash-style=gnu: Use GNU hash style
# -Wl,--build-id=none: Remove build ID
# -Wl,-z,norelro: Disable relocation read-only
# -Wl,--no-eh-frame-hdr: Remove exception frame header
LDFLAGS = -static -L/usr/lib/x86_64-linux-gnu -lsodium -lcrypto -Wl,--gc-sections -Wl,--strip-all -Wl,--as-needed \
          -Wl,--hash-style=gnu -Wl,--build-id=none -Wl,-z,norelro -Wl,--no-eh-frame-hdr

SRCS = main.c
OBJS = $(SRCS:.c=.o)
TARGET = nano_ssh_server
TARGET_COMPRESSED = nano_ssh_server.upx

.PHONY: all clean compress help

all: $(TARGET) compress

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Built $(TARGET) (uncompressed)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

compress: $(TARGET)
	@cp $(TARGET) $(TARGET_COMPRESSED)
	@upx --best --ultra-brute $(TARGET_COMPRESSED) 2>&1 || true
	@echo "Compressed: $(TARGET_COMPRESSED)"
	@ls -lh $(TARGET) $(TARGET_COMPRESSED) 2>/dev/null || true

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET_COMPRESSED)
	@echo "Cleaned v12-static"

# Help
help:
	@echo "v12-static Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all (default) - Build nano_ssh_server and compress with UPX"
	@echo "  compress      - Compress binary with UPX"
	@echo "  clean         - Remove build artifacts"
	@echo "  help          - Show this help"
	@echo ""
	@echo "To build: make"
	@echo "To run:   ./nano_ssh_server (or ./nano_ssh_server.upx for compressed)"
	@echo ""
