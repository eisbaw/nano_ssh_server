# BASH SSH Server - Quick Start Guide

## What is This?

An **educational** SSH server implementation written entirely in BASH, demonstrating SSH protocol handling using shell scripting.

## Quick Test (30 seconds)

```bash
# Terminal 1: Start the server
cd vbash-ssh-server
./nano_ssh_server_complete.sh 2222

# Terminal 2: Test with netcat
echo "SSH-2.0-TestClient" | nc localhost 2222
# You should see: SSH-2.0-BashSSH_0.1
```

## Using justfile (Recommended)

```bash
# From project root
just run-bash-simple     # Start simple BASH server
just test-bash           # Run tests
just clean-bash          # Clean temporary files
```

## Features

✅ **Works**:
- SSH version exchange
- Binary packet structure
- Protocol demonstration
- Educational value

⚠️ **Limited**:
- No full encryption (BASH limitations)
- Simplified authentication
- Single connection at a time
- Slow (spawns external processes)

## Requirements

```bash
# Check dependencies
command -v bash nc openssl dd od printf
```

All these tools are standard on Linux/Unix systems.

## File Overview

| File | Purpose | Status |
|------|---------|--------|
| `nano_ssh_server_complete.sh` | Basic demo | ✅ Works |
| `nano_ssh_server_complete.sh` | Full attempt | ⚠️ Partial |
| `crypto_helpers.sh` | Crypto operations | 📚 Library |
| `test_server.sh` | Test suite | ✅ Works |
| `IMPLEMENTATION_NOTES.md` | Technical details | 📖 Read this! |
| `README.md` | Full documentation | 📖 Documentation |

## Try It With SSH Client

```bash
# Start server
./nano_ssh_server_complete.sh

# In another terminal, try SSH client
ssh -p 2222 user@localhost
# This will fail after version exchange (expected)
# But you'll see the SSH handshake begin!
```

## What You'll Learn

1. **SSH Protocol**: How SSH handshake works
2. **Binary Protocols**: Handling binary data in BASH
3. **Network Programming**: TCP servers in shell scripts
4. **Cryptography**: Integration with OpenSSL
5. **Tool Mastery**: od, dd, printf, nc

## Size Comparison

| Implementation | Language | Source Size | Compiled Size |
|----------------|----------|-------------|---------------|
| v17-from14 | C | 1821 LOC | 25 KB |
| vbash-ssh-server | BASH | ~500 LOC | N/A (script) |

The BASH version is more **readable** but less **functional**.

## Production Use?

**NO!** This is:
- ❌ Not secure
- ❌ Not complete
- ❌ Not performant
- ✅ Educational only

For real SSH servers, use OpenSSH.

## Next Steps

1. **Read** `IMPLEMENTATION_NOTES.md` for technical details
2. **Experiment** with the code - it's designed to be readable
3. **Compare** with C version in `../v17-from14/`
4. **Learn** about SSH by modifying the protocol handling

## Common Issues

### "command not found: nc"
```bash
# Install netcat
sudo apt-get install netcat-openbsd  # Ubuntu/Debian
sudo yum install nc                   # RHEL/CentOS
```

### "command not found: openssl"
```bash
sudo apt-get install openssl
```

### Server doesn't start
```bash
# Check if port 2222 is in use
nc -z localhost 2222 && echo "Port in use" || echo "Port free"

# Use a different port
./nano_ssh_server_complete.sh 9999
```

## Support

This is an experimental/educational project. For questions:
1. Read `IMPLEMENTATION_NOTES.md`
2. Check the source code (it's commented)
3. Compare with the C implementation

## Credits

- Based on `nano_ssh_server` C implementation
- SSH protocol specs: RFC 4253, 4252, 4254
- Inspired by pure-bash experiments

---

**"The best way to learn a protocol is to implement it. The second best way is to implement it in BASH and see where you break."** - This project
