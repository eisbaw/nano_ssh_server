# Makefile for v3-opt2
# Phase 3: Minimal crypto library optimization
# Focus: Combined compiler optimizations + minimal crypto

CC = gcc
# Size optimization flags:
# -Os: Optimize for size
# -flto: Link-time optimization
# -ffunction-sections -fdata-sections: Separate sections for linker GC
# -fno-unwind-tables -fno-asynchronous-unwind-tables: Remove unwind tables
# -fno-stack-protector: Remove stack protector (save size, less secure)
CFLAGS = -Wall -Wextra -std=c11 -Os -flto -ffunction-sections -fdata-sections \
         -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector \
         -D_POSIX_C_SOURCE=200809L
# Linker flags:
# -Wl,--gc-sections: Garbage collect unused sections
# -Wl,--strip-all: Strip all symbols
# -Wl,--as-needed: Only link needed libraries
LDFLAGS = -lsodium -lcrypto -Wl,--gc-sections -Wl,--strip-all -Wl,--as-needed

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
	@echo "Cleaned v3-opt2"

# Help
help:
	@echo "v3-opt2 Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all (default) - Build nano_ssh_server"
	@echo "  clean         - Remove build artifacts"
	@echo "  help          - Show this help"
	@echo ""
	@echo "To build: make"
	@echo "To run:   ./nano_ssh_server"
	@echo ""
