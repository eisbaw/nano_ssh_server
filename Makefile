# Top-level Makefile for Nano SSH Server
# Orchestrates builds across all versions

# All version directories
VERSIONS := $(wildcard v*-*/)

# Default target
.PHONY: all
all: build-all

# Build all versions
.PHONY: build-all
build-all:
	@echo "Building all versions..."
	@for dir in $(VERSIONS); do \
		if [ -f "$$dir/Makefile" ]; then \
			echo "Building $$dir..."; \
			$(MAKE) -C $$dir || exit 1; \
		fi \
	done

# Clean all versions
.PHONY: clean
clean:
	@echo "Cleaning all versions..."
	@for dir in $(VERSIONS); do \
		if [ -f "$$dir/Makefile" ]; then \
			echo "Cleaning $$dir..."; \
			$(MAKE) -C $$dir clean; \
		fi \
	done

# Size comparison report
.PHONY: size-report
size-report:
	@echo "======================================"
	@echo "Binary Size Comparison"
	@echo "======================================"
	@printf "%-20s %15s %15s\n" "Version" "Size (bytes)" "Size (KB)"
	@echo "--------------------------------------"
	@for dir in $(VERSIONS); do \
		if [ -f "$$dir/nano_ssh_server" ]; then \
			version=$$(basename "$$dir"); \
			size=$$(stat -c%s "$$dir/nano_ssh_server" 2>/dev/null || stat -f%z "$$dir/nano_ssh_server" 2>/dev/null); \
			kb=$$(echo "scale=2; $$size/1024" | bc); \
			printf "%-20s %15s %15s\n" "$$version" "$$size" "$$kb"; \
		fi \
	done
	@echo "======================================"

# Help
.PHONY: help
help:
	@echo "Nano SSH Server - Top-level Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all (default)   - Build all versions"
	@echo "  build-all       - Build all versions"
	@echo "  clean           - Clean all versions"
	@echo "  size-report     - Show binary size comparison"
	@echo "  help            - Show this help"
	@echo ""
	@echo "For more control, use 'just' commands:"
	@echo "  just --list     - Show all available commands"
	@echo "  just build v0-vanilla"
	@echo "  just test v0-vanilla"
	@echo ""
