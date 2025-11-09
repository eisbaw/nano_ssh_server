# Makefile for v20-rsa
# RSA/DH implementation instead of elliptic curve
# Custom: RSA-2048, DH Group14, AES-128-CTR, SHA-256, HMAC-SHA256, CSPRNG, Big Integer Arithmetic
# NO external crypto library dependencies!

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -g \
         -D_POSIX_C_SOURCE=200809L
LDFLAGS = -Wl,--gc-sections -Wl,--as-needed

SRCS = main_production.c
OBJS = $(SRCS:.c=.o)
TARGET = nano_ssh_server_production
TARGET_COMPRESSED = nano_ssh_server.upx

# Header-only dependencies (no .c files needed)
HEADERS = bigint.h dh_group14.h rsa_minimal.h aes128_minimal.h sha256_minimal.h random_minimal.h

.PHONY: all clean compress

all: $(TARGET)

$(TARGET): $(OBJS) $(HEADERS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Built $(TARGET)"
	@ls -lh $(TARGET)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

compress: $(TARGET)
	@cp $(TARGET) $(TARGET_COMPRESSED)
	@upx --best --ultra-brute $(TARGET_COMPRESSED) 2>&1 || true
	@echo "Compressed: $(TARGET_COMPRESSED)"
	@ls -lh $(TARGET) $(TARGET_COMPRESSED) 2>/dev/null || true

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET_COMPRESSED)
	@echo "Cleaned v20-rsa"
