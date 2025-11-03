/*
 * Nano SSH Server - v0-vanilla
 * World's smallest SSH server for microcontrollers
 *
 * Phase 1: Working implementation (correctness first, size later)
 *
 * This version prioritizes:
 * - Correctness and completeness
 * - Code readability
 * - Standard library usage
 * - Comprehensive error handling
 *
 * Size optimization comes in later versions (v2+)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sodium.h>

/* Configuration */
#define SERVER_PORT 2222
#define SERVER_VERSION "SSH-2.0-NanoSSH_0.1"
#define BACKLOG 5

/* Hardcoded credentials (for minimal implementation) */
#define VALID_USERNAME "user"
#define VALID_PASSWORD "password123"

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("=================================\n");
    printf("Nano SSH Server v0-vanilla\n");
    printf("=================================\n");
    printf("Port: %d\n", SERVER_PORT);
    printf("Version: %s\n", SERVER_VERSION);
    printf("Credentials: %s / %s\n", VALID_USERNAME, VALID_PASSWORD);
    printf("=================================\n\n");

    /* Initialize libsodium */
    if (sodium_init() < 0) {
        fprintf(stderr, "Error: Failed to initialize libsodium\n");
        return 1;
    }
    printf("[+] Libsodium initialized\n");

    /* TODO: Implement SSH server
     *
     * Phase 1 tasks (see TODO.md):
     * 1. Network Layer - TCP server socket
     * 2. Version Exchange - SSH-2.0 handshake
     * 3. Binary Packet Protocol - Framing
     * 4. Cryptography - Random, SHA-256, Curve25519, Ed25519, ChaCha20-Poly1305
     * 5. Key Exchange - KEXINIT, Curve25519 DH
     * 6. Key Derivation - Derive encryption keys
     * 7. NEWKEYS - Activate encryption
     * 8. Encrypted Packets - ChaCha20-Poly1305
     * 9. Service Request - ssh-userauth
     * 10. Authentication - Password auth
     * 11. Channel Open - Session channel
     * 12. Channel Requests - PTY, shell
     * 13. Data Transfer - Send "Hello World"
     * 14. Channel Close - Clean disconnect
     *
     * For now, this is just a skeleton.
     */

    printf("[+] Server ready (skeleton only - not functional yet)\n");
    printf("[!] Implementation needed - see TODO.md Phase 1\n");

    return 0;
}
