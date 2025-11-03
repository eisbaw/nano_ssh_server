# Makefile for v0-vanilla
# Phase 1: Unoptimized, working SSH server
# Focus: Correctness and readability, NOT size

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lsodium

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
	@echo "Cleaned v0-vanilla"

# Help
help:
	@echo "v0-vanilla Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all (default) - Build nano_ssh_server"
	@echo "  clean         - Remove build artifacts"
	@echo "  help          - Show this help"
	@echo ""
	@echo "To build: make"
	@echo "To run:   ./nano_ssh_server"
	@echo ""
