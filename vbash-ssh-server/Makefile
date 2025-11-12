# Makefile for BASH SSH Server
# Note: No compilation needed, but provides consistent interface

.PHONY: all clean test run check install help

SCRIPT_NAME = nano_ssh_server_complete.sh
PORT = 2222

all: check
	@echo "BASH SSH Server - No compilation needed"
	@echo "Run 'make run' to start server or 'make test' to test"

check:
	@echo "Checking dependencies..."
	@command -v bash >/dev/null 2>&1 || { echo "ERROR: bash not found"; exit 1; }
	@command -v openssl >/dev/null 2>&1 || { echo "ERROR: openssl not found"; exit 1; }
	@command -v xxd >/dev/null 2>&1 || { echo "ERROR: xxd not found"; exit 1; }
	@command -v dd >/dev/null 2>&1 || { echo "ERROR: dd not found"; exit 1; }
	@command -v socat >/dev/null 2>&1 || command -v nc >/dev/null 2>&1 || { echo "ERROR: neither socat nor nc found"; exit 1; }
	@echo "✓ All dependencies found"
	@chmod +x $(SCRIPT_NAME)
	@echo "✓ Script is executable"

run: check
	@echo "Starting BASH SSH Server on port $(PORT)..."
	@./$(SCRIPT_NAME)

test: check
	@echo "Testing BASH SSH Server..."
	@echo "Note: Server must be running in another terminal"
	@echo "Run: ssh -p $(PORT) user@localhost"
	@echo "Password: password123"

# For automated testing (if server is running)
test-auto:
	@echo "Attempting automatic connection test..."
	@timeout 5 ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p $(PORT) user@localhost exit || echo "Test requires manual password entry or sshpass"

install: check
	@echo "Installing to /usr/local/bin/bash-ssh-server..."
	@sudo cp $(SCRIPT_NAME) /usr/local/bin/bash-ssh-server
	@sudo chmod +x /usr/local/bin/bash-ssh-server
	@echo "✓ Installed successfully"
	@echo "Run with: bash-ssh-server"

uninstall:
	@echo "Removing /usr/local/bin/bash-ssh-server..."
	@sudo rm -f /usr/local/bin/bash-ssh-server
	@echo "✓ Uninstalled"

clean:
	@echo "Cleaning temporary files..."
	@rm -rf /tmp/bash_ssh_*
	@echo "✓ Clean complete"

help:
	@echo "BASH SSH Server - Makefile targets:"
	@echo ""
	@echo "  make check     - Check dependencies"
	@echo "  make run       - Start the SSH server"
	@echo "  make test      - Show test instructions"
	@echo "  make install   - Install to /usr/local/bin"
	@echo "  make uninstall - Remove from /usr/local/bin"
	@echo "  make clean     - Remove temporary files"
	@echo "  make help      - Show this help"
	@echo ""
	@echo "Configuration:"
	@echo "  Port: $(PORT)"
	@echo "  User: user"
	@echo "  Password: password123"
