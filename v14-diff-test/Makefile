# Makefile for v14-diff-test
# Minimal diff testing: replace functions one at a time

CC = gcc
BASE_CFLAGS = -Wall -Wextra -std=c11 -O2 -D_POSIX_C_SOURCE=200809L
BASE_LDFLAGS = -lsodium

# Test flags (uncomment ONE at a time)
# TEST_FLAGS = -DTEST_RANDOMBYTES
# TEST_FLAGS = -DTEST_CURVE25519
# TEST_FLAGS = -DTEST_KEYGEN
# TEST_FLAGS = -DTEST_SIGNING

CFLAGS = $(BASE_CFLAGS) $(TEST_FLAGS)
LDFLAGS = $(BASE_LDFLAGS)

# Conditional object files for c25519
C25519_OBJS = f25519.o fprime.o ed25519.o edsign.o sha512.o
ifeq ($(findstring TEST_KEYGEN,$(TEST_FLAGS)),TEST_KEYGEN)
    EXTRA_OBJS = $(C25519_OBJS)
endif
ifeq ($(findstring TEST_SIGNING,$(TEST_FLAGS)),TEST_SIGNING)
    EXTRA_OBJS = $(C25519_OBJS)
endif

MAIN_OBJ = main.o
TARGET = nano_ssh_server

.PHONY: all clean test-baseline test-randombytes test-curve25519 test-keygen test-signing

all: $(TARGET)

$(TARGET): $(MAIN_OBJ) $(EXTRA_OBJS)
	$(CC) $^ -o $(TARGET) $(LDFLAGS)
	@echo "Built $(TARGET) with flags: $(TEST_FLAGS)"
	@ls -lh $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Individual test targets
test-baseline:
	@echo "=== TEST 0: Baseline (100% libsodium) ==="
	$(MAKE) clean
	$(MAKE) TEST_FLAGS="" all

test-randombytes:
	@echo "=== TEST 1: Replace randombytes_buf ==="
	$(MAKE) clean
	$(MAKE) TEST_FLAGS="-DTEST_RANDOMBYTES" all

test-curve25519:
	@echo "=== TEST 2: Replace Curve25519 ==="
	$(MAKE) clean
	$(MAKE) TEST_FLAGS="-DTEST_CURVE25519" all

test-keygen:
	@echo "=== TEST 3: Replace crypto_sign_keypair ==="
	$(MAKE) clean
	$(MAKE) TEST_FLAGS="-DTEST_KEYGEN" all

test-signing:
	@echo "=== TEST 4: Replace crypto_sign_detached ==="
	$(MAKE) clean
	$(MAKE) TEST_FLAGS="-DTEST_SIGNING" all

clean:
	rm -f $(MAIN_OBJ) $(TARGET)
	@echo "Cleaned (c25519 objects preserved)"
