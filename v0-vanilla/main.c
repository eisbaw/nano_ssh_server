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

/*
 * Helper function: Create and configure TCP server socket
 */
int create_server_socket(int port) {
    int listen_fd;
    struct sockaddr_in server_addr;
    int reuse = 1;

    /* Create socket */
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        fprintf(stderr, "Error: socket() failed: %s\n", strerror(errno));
        return -1;
    }

    /* Set SO_REUSEADDR to avoid "Address already in use" errors */
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        fprintf(stderr, "Warning: setsockopt(SO_REUSEADDR) failed: %s\n", strerror(errno));
    }

    /* Bind to address and port */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "Error: bind() failed: %s\n", strerror(errno));
        close(listen_fd);
        return -1;
    }

    /* Listen for connections */
    if (listen(listen_fd, BACKLOG) < 0) {
        fprintf(stderr, "Error: listen() failed: %s\n", strerror(errno));
        close(listen_fd);
        return -1;
    }

    return listen_fd;
}

/*
 * Helper function: Accept client connection
 */
int accept_client(int listen_fd, struct sockaddr_in *client_addr) {
    socklen_t addr_len = sizeof(*client_addr);
    int client_fd;

    client_fd = accept(listen_fd, (struct sockaddr *)client_addr, &addr_len);
    if (client_fd < 0) {
        fprintf(stderr, "Error: accept() failed: %s\n", strerror(errno));
        return -1;
    }

    return client_fd;
}

/*
 * Helper function: Send data with error handling
 */
ssize_t send_data(int fd, const void *buf, size_t len) {
    ssize_t sent = 0;
    ssize_t n;

    while (sent < (ssize_t)len) {
        n = send(fd, (const uint8_t *)buf + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;  /* Interrupted, try again */
            }
            fprintf(stderr, "Error: send() failed: %s\n", strerror(errno));
            return -1;
        }
        if (n == 0) {
            fprintf(stderr, "Error: send() returned 0 (connection closed)\n");
            return -1;
        }
        sent += n;
    }

    return sent;
}

/*
 * Helper function: Receive data with error handling
 */
ssize_t recv_data(int fd, void *buf, size_t len) {
    ssize_t n;

    n = recv(fd, buf, len, 0);
    if (n < 0) {
        if (errno == EINTR) {
            return 0;  /* Interrupted, try again */
        }
        fprintf(stderr, "Error: recv() failed: %s\n", strerror(errno));
        return -1;
    }
    if (n == 0) {
        /* Connection closed by peer */
        return 0;
    }

    return n;
}

/*
 * Handle SSH connection
 */
void handle_client(int client_fd, struct sockaddr_in *client_addr) {
    char client_ip[INET_ADDRSTRLEN];
    char client_version[256];
    char server_version_line[256];
    ssize_t n;
    int i, version_len;

    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, sizeof(client_ip));
    printf("\n[+] Client connected: %s:%d\n", client_ip, ntohs(client_addr->sin_port));

    /* ======================
     * Phase 1.2: Version Exchange
     * ====================== */

    /* Send server version string (plaintext, ends with \r\n) */
    snprintf(server_version_line, sizeof(server_version_line), "%s\r\n", SERVER_VERSION);
    if (send_data(client_fd, server_version_line, strlen(server_version_line)) < 0) {
        fprintf(stderr, "[-] Failed to send version string\n");
        close(client_fd);
        return;
    }
    printf("[+] Sent version: %s", server_version_line);

    /* Receive client version string (read until \n) */
    memset(client_version, 0, sizeof(client_version));
    version_len = 0;

    for (i = 0; i < (int)sizeof(client_version) - 1; i++) {
        n = recv_data(client_fd, &client_version[i], 1);
        if (n <= 0) {
            fprintf(stderr, "[-] Failed to receive version string\n");
            close(client_fd);
            return;
        }
        version_len++;

        /* Stop at \n */
        if (client_version[i] == '\n') {
            break;
        }
    }

    /* Null-terminate */
    client_version[version_len] = '\0';

    /* Remove \r\n if present */
    if (version_len > 0 && client_version[version_len - 1] == '\n') {
        client_version[version_len - 1] = '\0';
        version_len--;
    }
    if (version_len > 0 && client_version[version_len - 1] == '\r') {
        client_version[version_len - 1] = '\0';
        version_len--;
    }

    printf("[+] Received version: %s\n", client_version);

    /* Validate client version starts with "SSH-2.0-" */
    if (strncmp(client_version, "SSH-2.0-", 8) != 0) {
        fprintf(stderr, "[-] Invalid SSH version: %s\n", client_version);
        fprintf(stderr, "[-] Expected: SSH-2.0-*\n");
        close(client_fd);
        return;
    }

    printf("[+] Version exchange complete\n");

    /* TODO: Store version strings for exchange hash H
     * V_C = client_version (without \r\n)
     * V_S = SERVER_VERSION (without \r\n)
     */

    /* TODO: Implement remaining SSH protocol
     *
     * Phase 1 tasks (see TODO.md):
     * 2. Binary Packet Protocol - Framing
     * 3. Cryptography - Random, SHA-256, Curve25519, Ed25519, ChaCha20-Poly1305
     * 4. Key Exchange - KEXINIT, Curve25519 DH
     * 5. Key Derivation - Derive encryption keys
     * 6. NEWKEYS - Activate encryption
     * 7. Encrypted Packets - ChaCha20-Poly1305
     * 8. Service Request - ssh-userauth
     * 9. Authentication - Password auth
     * 10. Channel Open - Session channel
     * 11. Channel Requests - PTY, shell
     * 12. Data Transfer - Send "Hello World"
     * 13. Channel Close - Clean disconnect
     */

    printf("[!] SSH protocol not fully implemented yet\n");
    printf("[+] Closing connection\n");

    close(client_fd);
}

int main(int argc, char *argv[]) {
    int listen_fd;
    int client_fd;
    struct sockaddr_in client_addr;

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

    /* Create TCP server socket */
    listen_fd = create_server_socket(SERVER_PORT);
    if (listen_fd < 0) {
        fprintf(stderr, "Error: Failed to create server socket\n");
        return 1;
    }
    printf("[+] Server socket created and listening on port %d\n", SERVER_PORT);

    /* Main server loop - accept connections */
    printf("[+] Waiting for connections...\n\n");

    while (1) {
        /* Accept client connection */
        client_fd = accept_client(listen_fd, &client_addr);
        if (client_fd < 0) {
            fprintf(stderr, "Warning: Failed to accept client, continuing...\n");
            continue;
        }

        /* Handle client connection */
        handle_client(client_fd, &client_addr);

        /* For now, only handle one connection then exit */
        /* TODO: In production, this should loop forever or handle multiple clients */
        break;
    }

    close(listen_fd);
    printf("\n[+] Server shutting down\n");

    return 0;
}
