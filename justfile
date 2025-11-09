# Justfile for Nano SSH Server
# Task automation interface - ALWAYS use this instead of raw commands

# Use bash for better shell compatibility
set shell := ["bash", "-c"]

# Default recipe - show help
default:
    @just --list

# Build specific version
build VERSION:
    @echo "Building {{VERSION}}..."
    @if [ ! -d "{{VERSION}}" ]; then echo "Error: {{VERSION}} directory does not exist"; exit 1; fi
    @cd {{VERSION}} && make

# Build all versions
build-all:
    #!/usr/bin/env bash
    echo "Building all versions..."
    for dir in v*-*/; do
        if [ -d "${dir}" ] && [ -f "${dir}/Makefile" ]; then
            echo "Building ${dir}..."
            (cd "${dir}" && make)
        fi
    done

# Clean specific version
clean VERSION:
    @echo "Cleaning {{VERSION}}..."
    @if [ ! -d "{{VERSION}}" ]; then echo "Error: {{VERSION}} directory does not exist"; exit 1; fi
    @cd {{VERSION}} && make clean

# Clean all versions
clean-all:
    #!/usr/bin/env bash
    echo "Cleaning all versions..."
    for dir in v*-*/; do
        if [ -d "${dir}" ] && [ -f "${dir}/Makefile" ]; then
            echo "Cleaning ${dir}..."
            (cd "${dir}" && make clean)
        fi
    done

# Run specific version (starts SSH server on port 2222)
run VERSION:
    @echo "Starting {{VERSION}} on port 2222..."
    @if [ ! -f "{{VERSION}}/nano_ssh_server" ]; then \
        echo "Error: {{VERSION}}/nano_ssh_server not found. Run 'just build {{VERSION}}' first"; \
        exit 1; \
    fi
    @cd {{VERSION}} && ./nano_ssh_server

# Run in debug mode with gdb
debug VERSION:
    @echo "Starting {{VERSION}} in gdb..."
    @if [ ! -f "{{VERSION}}/nano_ssh_server" ]; then \
        echo "Error: {{VERSION}}/nano_ssh_server not found. Run 'just build {{VERSION}}' first"; \
        exit 1; \
    fi
    @cd {{VERSION}} && gdb ./nano_ssh_server

# Run with valgrind to check for memory leaks
valgrind VERSION:
    @echo "Running {{VERSION}} with valgrind..."
    @if [ ! -f "{{VERSION}}/nano_ssh_server" ]; then \
        echo "Error: {{VERSION}}/nano_ssh_server not found. Run 'just build {{VERSION}}' first"; \
        exit 1; \
    fi
    @cd {{VERSION}} && valgrind --leak-check=full --show-leak-kinds=all ./nano_ssh_server

# Test specific version
test VERSION:
    @echo "Testing {{VERSION}}..."
    @if [ ! -f "tests/run_tests.sh" ]; then \
        echo "Warning: tests/run_tests.sh not found. Skipping tests."; \
        exit 0; \
    fi
    @bash tests/run_tests.sh {{VERSION}}

# Test all versions
test-all:
    #!/usr/bin/env bash
    echo "Testing all versions (quick mode - version check only)..."
    if [ ! -f "tests/test_version.sh" ]; then
        echo "Warning: tests/test_version.sh not found. Skipping tests."
        exit 0
    fi

    # Kill any existing servers before starting
    pkill -9 -f nano_ssh_server 2>/dev/null || true
    sleep 1

    # Track test results
    PASSED=0
    FAILED=0
    TOTAL=0

    for dir in v*-*/; do
        if [ -d "${dir}" ] && [ -f "${dir}/nano_ssh_server" ]; then
            version=${dir%/}
            TOTAL=$((TOTAL + 1))

            echo ""
            echo "========================================"
            echo "Testing ${version}... ($TOTAL)"
            echo "========================================"

            # Ensure clean state before each test
            pkill -9 -f nano_ssh_server 2>/dev/null || true

            # Run only version test with 15-second timeout (faster for CI)
            if timeout 15 bash tests/test_version.sh "${version}"; then
                PASSED=$((PASSED + 1))
                echo "✓ ${version}: PASSED"
            else
                FAILED=$((FAILED + 1))
                echo "✗ ${version}: FAILED"
            fi

            # Ensure cleanup after each test
            pkill -9 -f nano_ssh_server 2>/dev/null || true
        fi
    done

    # Final cleanup
    pkill -9 -f nano_ssh_server 2>/dev/null || true

    # Print summary
    echo ""
    echo "========================================"
    echo "Test Summary"
    echo "========================================"
    echo "Total:  ${TOTAL}"
    echo "Passed: ${PASSED}"
    echo "Failed: ${FAILED}"
    echo "========================================"

# Connect to running server with SSH client (run in separate terminal)
connect:
    @echo "Connecting to SSH server on port 2222..."
    @echo "Username: user"
    @echo "Password: password123"
    @ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p 2222 user@localhost

# Connect to running server with SSH client (automated with sshpass)
connect-auto:
    @echo "Connecting to SSH server on port 2222 (automated)..."
    @sshpass -p password123 ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p 2222 user@localhost

# Test all server versions with real SSH client (using sshpass)
test-all-sshpass:
    @echo "Testing all versions with real SSH client..."
    @./test_ssh_versions_final.sh

# Show binary sizes for all versions
size-report:
    #!/usr/bin/env bash
    echo "======================================"
    echo "Binary Size Comparison"
    echo "======================================"
    printf "%-20s %15s %15s\n" "Version" "Size (bytes)" "Size (KB)"
    echo "--------------------------------------"
    for dir in v*-*/; do
        if [ -f "${dir}/nano_ssh_server" ]; then
            version=${dir%/}
            size=$(stat -c%s "${dir}/nano_ssh_server" 2>/dev/null || stat -f%z "${dir}/nano_ssh_server" 2>/dev/null)
            kb=$(echo "scale=2; ${size}/1024" | bc)
            printf "%-20s %15s %15s\n" "${version}" "${size}" "${kb}"
        fi
    done
    echo "======================================"

# Generate Ed25519 host key (for v0-vanilla)
generate-hostkey:
    @echo "Generating Ed25519 host key..."
    @ssh-keygen -t ed25519 -f host_key -N "" -C "nano_ssh_server"
    @echo "Host key generated: host_key (private), host_key.pub (public)"
    @echo "Use 'xxd -i host_key' to convert to C header format"

# Show project status
status:
    #!/usr/bin/env bash
    echo "======================================"
    echo "Nano SSH Server - Project Status"
    echo "======================================"
    echo ""
    echo "Versions:"
    for dir in v*-*/; do
        if [ -d "${dir}" ]; then
            version=${dir%/}
            if [ -f "${dir}/nano_ssh_server" ]; then
                printf "  [BUILT]  %s\n" "${version}"
            elif [ -f "${dir}/Makefile" ]; then
                printf "  [READY]  %s\n" "${version}"
            else
                printf "  [EMPTY]  %s\n" "${version}"
            fi
        fi
    done
    echo ""
    echo "To get started:"
    echo "  1. just build v0-vanilla"
    echo "  2. just run v0-vanilla    (in one terminal)"
    echo "  3. just connect           (in another terminal)"
    echo ""

# Test musl-gcc with simple program
test-musl:
    @echo "Testing musl-gcc compilation..."
    @musl-gcc -static -Os test_musl.c -o test_musl
    @echo "✅ Musl binary built"
    @echo ""
    @echo "Binary info:"
    @file test_musl
    @echo ""
    @echo "Dependencies:"
    @ldd test_musl 2>&1 || echo "(none - statically linked)"
    @echo ""
    @echo "Running test:"
    @./test_musl
    @echo ""
    @echo "Size comparison with glibc:"
    @gcc -static -Os test_musl.c -o test_glibc 2>/dev/null || true
    @ls -lh test_musl test_glibc 2>/dev/null | awk '{print $$9 " : " $$5}' || true
    @rm -f test_glibc
    @echo ""
    @echo "✅ musl-gcc works! See MUSL_TEST_RESULTS.md for details."

# Build with musl libc (native, no Docker)
build-musl:
    @echo "Building full SSH server with musl libc..."
    @echo "⚠️  Note: This requires building OpenSSL and libsodium from source"
    @echo "    For a quick test, run: just test-musl"
    @echo ""
    @./build-musl-native.sh

# Clean musl build artifacts
clean-musl:
    @echo "Cleaning musl build..."
    @rm -rf build-musl test_musl test_glibc
    @echo "Cleaned musl build artifacts"

# Build v14-static with musl (requires musl-tools and static libsodium)
build-v14-static:
    @echo "Building v14-static with musl libc..."
    @echo "This requires musl-tools and statically compiled libsodium"
    @echo ""
    @if [ ! -d "v14-static" ]; then \
        echo "Error: v14-static directory does not exist"; \
        exit 1; \
    fi
    @cd v14-static && make
    @echo ""
    @echo "✅ v14-static built successfully!"
    @echo ""
    @ls -lh v14-static/nano_ssh_server
    @echo ""

# Run v14-static
run-v14-static:
    @echo "Starting v14-static on port 2222..."
    @if [ ! -f "v14-static/nano_ssh_server" ]; then \
        echo "Error: v14-static/nano_ssh_server not found. Run 'just build-v14-static' first"; \
        exit 1; \
    fi
    @cd v14-static && ./nano_ssh_server

# Test v14-static with real SSH client
test-v14-static:
    @echo "Testing v14-static with real SSH client..."
    @if [ ! -f "v14-static/nano_ssh_server" ]; then \
        echo "Error: v14-static/nano_ssh_server not found. Run 'just build-v14-static' first"; \
        exit 1; \
    fi
    @echo ""
    @echo "Starting v14-static server in background..."
    @cd v14-static && ./nano_ssh_server > server_test.log 2>&1 & echo $$! > server.pid
    @sleep 2
    @echo ""
    @echo "Testing SSH connection..."
    @sshpass -p password123 ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p 2222 user@localhost "echo 'Connection successful!'" || true
    @echo ""
    @echo "Stopping server..."
    @kill $$(cat v14-static/server.pid) 2>/dev/null || true
    @rm -f v14-static/server.pid
    @echo ""
    @echo "Server output:"
    @cat v14-static/server_test.log
    @echo ""

# Show help
help:
    @echo "Nano SSH Server - Task Automation"
    @echo ""
    @echo "Common workflows:"
    @echo "  just build v0-vanilla     # Build a version"
    @echo "  just run v0-vanilla       # Run the server"
    @echo "  just connect              # Connect with SSH client"
    @echo "  just test v0-vanilla      # Run tests"
    @echo "  just size-report          # Compare binary sizes"
    @echo ""
    @echo "Musl builds:"
    @echo "  just build-musl           # Build with musl (native, no Docker)"
    @echo "  just clean-musl           # Clean musl build artifacts"
    @echo ""
    @echo "Development:"
    @echo "  just debug v0-vanilla     # Run in debugger"
    @echo "  just valgrind v0-vanilla  # Check for leaks"
    @echo ""
    @echo "Run 'just --list' to see all available commands"
