# Makefile for v22-hacky
# EXTREMELY HACKY OPTIMIZATIONS - v21-static + aggressive size reduction
# WARNING: May break on some systems, use at your own risk!

CC = musl-gcc
CFLAGS = -std=c11 -Oz -flto -ffunction-sections -fdata-sections \
         -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector \
         -fmerge-all-constants -fno-ident -finline-small-functions \
         -fshort-enums -fomit-frame-pointer -ffast-math -fno-math-errno \
         -fvisibility=hidden -Wno-unused-result -fno-builtin -fno-plt \
         -fwhole-program -fipa-pta -fno-common \
         -fmerge-constants -fstrict-aliasing -fstrict-overflow \
         -fdelete-null-pointer-checks -fno-stack-check \
         -fno-unroll-loops \
         -D_POSIX_C_SOURCE=200809L -DNDEBUG -w
LDFLAGS = -static -Wl,--gc-sections -Wl,--strip-all -Wl,--as-needed \
          -Wl,--hash-style=gnu -Wl,--build-id=none -Wl,-z,norelro \
          -Wl,--no-eh-frame-hdr -Wl,--relax -Wl,-O2 -Wl,--sort-common

SRCS = main_production.c f25519.c fprime.c ed25519.c edsign.c sha512.c curve25519-donna-c64.c
OBJS = $(SRCS:.c=.o)
TARGET = nano_ssh_server
TARGET_COMPRESSED = nano_ssh_server.upx

.PHONY: all clean compress verify

all: $(TARGET) compress verify

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@objcopy -R .comment -R .note -R .note.gnu.build-id -R .gnu.version -R .eh_frame $(TARGET) $(TARGET).tmp
	@mv $(TARGET).tmp $(TARGET)
	@echo "Built $(TARGET) (musl static, sections stripped)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

compress: $(TARGET)
	@cp $(TARGET) $(TARGET_COMPRESSED)
	@upx --best --ultra-brute $(TARGET_COMPRESSED) 2>&1 || true
	@echo "Compressed: $(TARGET_COMPRESSED)"
	@ls -lh $(TARGET) $(TARGET_COMPRESSED) 2>/dev/null || true

verify: $(TARGET)
	@echo ""
	@echo "=== Verification ==="
	@echo -n "File type: "
	@file $(TARGET)
	@echo -n "Dynamic deps: "
	@ldd $(TARGET) 2>&1 || echo "(none - statically linked)"
	@echo ""
	@echo "=== Size Analysis ==="
	@size $(TARGET)
	@echo ""
	@ls -lh $(TARGET)
	@stat -c "Size: %s bytes" $(TARGET) 2>/dev/null || stat -f "Size: %z bytes" $(TARGET)
	@echo ""
	@echo "✅ Musl static binary built successfully!"
	@echo ""

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET_COMPRESSED)
	@echo "Cleaned v22-hacky"
