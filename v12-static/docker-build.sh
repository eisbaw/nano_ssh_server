#!/bin/bash
# docker-build.sh - Simple script to build static musl binary using Docker
#
# Prerequisites:
#   - Docker or Podman installed and working
#   - Internet connection
#
# Usage:
#   ./docker-build.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "🔨 Building nano_ssh_server with musl libc using Docker..."
echo ""

# Detect Docker or Podman
if command -v docker &> /dev/null; then
    DOCKER_CMD="docker"
    echo "✓ Using Docker"
elif command -v podman &> /dev/null; then
    DOCKER_CMD="podman"
    echo "✓ Using Podman"
else
    echo "❌ Error: Neither Docker nor Podman found"
    echo "   Please install Docker or Podman first"
    exit 1
fi

echo ""
echo "Step 1/4: Building image..."
$DOCKER_CMD build -f Dockerfile.alpine -t nano-ssh-musl .

echo ""
echo "Step 2/4: Creating temporary container..."
$DOCKER_CMD create --name nano-ssh-temp nano-ssh-musl

echo ""
echo "Step 3/4: Extracting binary..."
$DOCKER_CMD cp nano-ssh-temp:/nano_ssh_server ./nano_ssh_server.musl

echo ""
echo "Step 4/4: Cleaning up..."
$DOCKER_CMD rm nano-ssh-temp

echo ""
echo "✅ Build complete!"
echo ""
echo "📊 Binary Information:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
ls -lh nano_ssh_server.musl
echo ""
file nano_ssh_server.musl
echo ""
echo "📦 Dependencies:"
ldd nano_ssh_server.musl 2>&1 || echo "   ✓ Statically linked (no dependencies)"
echo ""
echo "📏 Size breakdown:"
size nano_ssh_server.musl 2>/dev/null || echo "   (size command not available)"
echo ""
echo "✨ Binary ready: $(pwd)/nano_ssh_server.musl"
echo ""
echo "💡 Next steps:"
echo "   1. Test it: ./nano_ssh_server.musl"
echo "   2. Verify portability: copy to another Linux system and run"
echo "   3. Try UPX compression: upx --best --lzma nano_ssh_server.musl"
