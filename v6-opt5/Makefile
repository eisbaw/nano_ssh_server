# Makefile for v6-opt5
# Phase 3: Ultimate size optimization - Remove all diagnostic strings
# Focus: Aggressive .rodata reduction by stripping error messages

CC = gcc
# Aggressive size optimization flags:
# -Os: Optimize for size
# -flto: Link-time optimization
# -ffunction-sections -fdata-sections: Separate sections for linker GC
# -fno-unwind-tables -fno-asynchronous-unwind-tables: Remove unwind tables
# -fno-stack-protector: Remove stack protector
# -fmerge-all-constants: Merge identical constants
# -fno-ident: Remove compiler identification
# -fno-exceptions: No C++ exceptions (not needed)
# -finline-small-functions: Inline small functions
# -Wno-unused-result: Suppress warnings for unused results
CFLAGS = -Wall -Wextra -std=c11 -Os -flto -ffunction-sections -fdata-sections \
         -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector \
         -fmerge-all-constants -fno-ident -finline-small-functions \
         -Wno-unused-result -D_POSIX_C_SOURCE=200809L
# Aggressive linker flags:
# -Wl,--gc-sections: Garbage collect unused sections
# -Wl,--strip-all: Strip all symbols
# -Wl,--as-needed: Only link needed libraries
# -Wl,--hash-style=gnu: Use GNU hash style (smaller)
# -Wl,--build-id=none: Remove build ID
LDFLAGS = -lsodium -lcrypto -Wl,--gc-sections -Wl,--strip-all -Wl,--as-needed \
          -Wl,--hash-style=gnu -Wl,--build-id=none

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
	@echo "Cleaned v6-opt5"

# Help
help:
	@echo "v6-opt5 Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all (default) - Build nano_ssh_server"
	@echo "  clean         - Remove build artifacts"
	@echo "  help          - Show this help"
	@echo ""
	@echo "To build: make"
	@echo "To run:   ./nano_ssh_server"
	@echo ""
