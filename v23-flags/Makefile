# Makefile for v22-c25519
# 100% independent crypto implementation
# Custom: AES-128-CTR, SHA-256, HMAC-SHA256, CSPRNG
# Public domain c25519 (Daniel Beer): Curve25519 X25519 + Ed25519, SHA-512
# Replaces v19-donna's 20 KB curve25519-donna-c64 with the small c25519 ladder.
# NO libsodium dependency!
# NO OpenSSL dependency!

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Os -flto -ffunction-sections -fdata-sections \
         -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector \
         -fmerge-all-constants -fno-ident -finline-small-functions \
         -fshort-enums -fomit-frame-pointer -ffast-math -fno-math-errno \
         -fvisibility=hidden -Wno-unused-result -fno-builtin -fno-plt \
         -D_POSIX_C_SOURCE=200809L
LDFLAGS = -Wl,--gc-sections -Wl,--strip-all -Wl,--as-needed \
          -Wl,--hash-style=gnu -Wl,--build-id=none -Wl,-z,norelro -Wl,--no-eh-frame-hdr \
          -Wl,-z,noseparate-code

SRCS = main_production.c f25519.c fprime.c ed25519.c edsign.c sha512.c c25519.c
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
	@echo "Cleaned v22-c25519"
