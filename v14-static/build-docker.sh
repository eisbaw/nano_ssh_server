#!/bin/bash
# Build v14-static with musl libc using Docker/Alpine
# This builds in an Alpine container which uses musl libc natively

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $*"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }

info "Building v14-static with musl libc (Alpine Linux)..."
echo ""

# Check Docker
if ! command -v docker &> /dev/null; then
    warn "Docker not found. Install Docker to use this build method."
    echo ""
    echo "Alternatives:"
    echo "  1. Use nix-shell shell-musl.nix (if Nix is available)"
    echo "  2. Install musl-tools: apt-get install musl-tools musl-dev"
    echo "  3. Build in an Alpine Linux environment"
    echo ""
    exit 1
fi

# Build image
info "Building Docker image..."
docker build -t nano-ssh-v14-static -f Dockerfile.alpine .

# Create a temporary container to extract the binary
info "Extracting binary from Docker image..."
CONTAINER_ID=$(docker create nano-ssh-v14-static)
docker cp "$CONTAINER_ID:/nano_ssh_server" ./nano_ssh_server
docker rm "$CONTAINER_ID" > /dev/null

# Also extract compressed version if it exists
docker run --rm nano-ssh-v14-static cat /build/nano_ssh_server.upx > nano_ssh_server.upx 2>/dev/null || true

info "Build complete!"
echo ""

# Show results
info "Binary info:"
ls -lh nano_ssh_server*
echo ""

file nano_ssh_server
echo ""

ldd nano_ssh_server 2>&1 || info "(statically linked)"
echo ""

info "Size:"
stat -c "Size: %s bytes" nano_ssh_server 2>/dev/null || stat -f "Size: %z bytes" nano_ssh_server
echo ""

info "✅ Success! Binary: $SCRIPT_DIR/nano_ssh_server"
echo ""
info "To run: ./nano_ssh_server"
echo ""
