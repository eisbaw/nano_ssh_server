# Installation Guide for BASH SSH Server

## Prerequisites

The BASH SSH server requires the following tools:

### Required Tools

| Tool | Purpose | Installation |
|------|---------|--------------|
| **bash** | Shell interpreter (4.0+) | Usually pre-installed |
| **xxd** | Hex dump utility | `apt install vim-common` or `yum install vim-common` |
| **openssl** | Cryptographic operations | `apt install openssl` or `yum install openssl` |
| **bc** | Bignum calculator | `apt install bc` or `yum install bc` |
| **socat** or **nc** | TCP server | `apt install socat` or use built-in `nc` |

### Testing Tools (Optional)

| Tool | Purpose | Installation |
|------|---------|--------------|
| **ssh** | SSH client for testing | `apt install openssh-client` |
| **sshpass** | Non-interactive password auth | `apt install sshpass` |

## Quick Installation

### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install -y vim-common openssl bc socat openssh-client sshpass
```

### CentOS/RHEL/Fedora

```bash
sudo yum install -y vim-common openssl bc socat openssh-clients sshpass
```

### macOS (with Homebrew)

```bash
# Most tools are pre-installed on macOS
brew install socat

# For sshpass (requires adding unofficial tap)
brew install hudochenkov/sshpass/sshpass
```

### Alpine Linux

```bash
apk add bash vim openssl bc socat openssh-client sshpass
```

## Verify Installation

Run this command to check all dependencies:

```bash
./check_dependencies.sh
```

Or manually:

```bash
command -v bash && echo "✅ bash" || echo "❌ bash"
command -v xxd && echo "✅ xxd" || echo "❌ xxd"
command -v openssl && echo "✅ openssl" || echo "❌ openssl"
command -v bc && echo "✅ bc" || echo "❌ bc"
command -v socat && echo "✅ socat" || command -v nc && echo "✅ nc" || echo "❌ socat/nc"
command -v ssh && echo "✅ ssh" || echo "❌ ssh (optional)"
command -v sshpass && echo "✅ sshpass" || echo "❌ sshpass (optional)"
```

## Alternative: Using Docker

If you prefer not to install dependencies on your host system, use Docker:

```bash
# Build Docker image
docker build -t bash-ssh-server .

# Run the server
docker run -p 2222:2222 bash-ssh-server

# Connect from host
ssh -p 2222 user@localhost
```

## Troubleshooting

### Missing `xxd`

If `xxd` is not available, it's usually part of the `vim-common` package:

```bash
# Ubuntu/Debian
sudo apt-get install vim-common

# CentOS/RHEL
sudo yum install vim-common
```

### Missing `socat`

If `socat` is not available, the script can fall back to `nc` (netcat):

```bash
# Ubuntu/Debian
sudo apt-get install netcat-openbsd

# CentOS/RHEL
sudo yum install nmap-ncat
```

### Missing `sshpass`

`sshpass` is only needed for automated testing. For manual testing, use regular `ssh`:

```bash
# Manual connection (will prompt for password)
ssh -p 2222 user@localhost
# Password: password123
```

### Permission Denied on Port

If you get "Permission denied" when starting the server on port 2222:

```bash
# Option 1: Use a port > 1024 (recommended)
./nano_ssh.sh 2222

# Option 2: Run as root (not recommended)
sudo ./nano_ssh.sh 2222

# Option 3: Grant capability to bind to low ports
sudo setcap 'cap_net_bind_service=+ep' $(which socat)
```

## Next Steps

After installing dependencies:

1. **Start the server:**
   ```bash
   ./nano_ssh.sh
   ```

2. **Connect with SSH:**
   ```bash
   ssh -p 2222 user@localhost
   # Password: password123
   ```

3. **Run automated tests:**
   ```bash
   ./test_bash_ssh.sh
   ```

## Minimum Requirements

- **OS**: Linux, macOS, BSD, or any UNIX-like system
- **BASH**: Version 4.0 or later
- **Disk space**: ~50 MB for temporary files during operation
- **Memory**: ~20 MB
- **CPU**: Any (minimal processing needed)

## Production Use

⚠️ **WARNING**: This server is for **educational purposes only**!

**DO NOT** use in production because:
- Hardcoded credentials
- Minimal security hardening
- Single-threaded (one connection at a time)
- No DoS protection
- Limited error handling

For production use, use established SSH servers like OpenSSH.
