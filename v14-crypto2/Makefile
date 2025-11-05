# Makefile for v14-crypto2
# Crypto optimization series: Step 2
# Focus: Replace OpenSSL AES-128-CTR with minimal implementation
# Still using libsodium for: Curve25519, Ed25519, randombytes

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Os -flto -ffunction-sections -fdata-sections \
         -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector \
         -fmerge-all-constants -fno-ident -finline-small-functions \
         -fshort-enums -fomit-frame-pointer -ffast-math -fno-math-errno \
         -fvisibility=hidden -Wno-unused-result -fno-builtin -fno-plt \
         -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lsodium -Wl,--gc-sections -Wl,--strip-all -Wl,--as-needed \
          -Wl,--hash-style=gnu -Wl,--build-id=none -Wl,-z,norelro -Wl,--no-eh-frame-hdr

SRCS = main.c
OBJS = $(SRCS:.c=.o)
TARGET = nano_ssh_server
TARGET_COMPRESSED = nano_ssh_server.upx

.PHONY: all clean compress

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
	@echo "Cleaned v14-crypto2"
