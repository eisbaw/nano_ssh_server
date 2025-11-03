# Porting Guide: Nano SSH Server to Microcontrollers

## Overview

This guide explains how to port the nano SSH server from Linux to embedded microcontrollers.

## Architecture

The server is designed with a **Network Abstraction Layer** to enable easy porting.

```
┌─────────────────────────────────┐
│   SSH Protocol Implementation   │  <- Platform independent
│  (ssh_proto.c, crypto.c, etc.)  │
└────────────┬────────────────────┘
             │
             │ Uses abstract interface
             │
┌────────────┴────────────────────┐
│    Network Abstraction Layer    │  <- Define once
│         (network.h)              │
└────────────┬────────────────────┘
             │
             │ Implement per platform
             │
    ┌────────┴────────┬────────────────┐
    │                 │                 │
┌───┴────────┐  ┌─────┴──────┐  ┌──────┴────────┐
│ net_posix.c│  │ net_lwip.c │  │ net_custom.c  │
│  (Linux)   │  │   (ESP32)  │  │  (Bare metal) │
└────────────┘  └────────────┘  └───────────────┘
```

## Network Abstraction Interface

### network.h

```c
#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stddef.h>

// Opaque connection context
typedef void* net_conn_t;

// Network operations interface
typedef struct {
    // Initialize network stack
    int (*init)(uint16_t port);

    // Accept incoming connection (blocking or non-blocking)
    // Returns connection handle or NULL
    net_conn_t (*accept)(void);

    // Read data from connection
    // Returns number of bytes read, 0 on EOF, -1 on error
    ssize_t (*read)(net_conn_t conn, uint8_t* buf, size_t len);

    // Write data to connection
    // Returns number of bytes written, -1 on error
    ssize_t (*write)(net_conn_t conn, const uint8_t* buf, size_t len);

    // Close connection
    void (*close)(net_conn_t conn);

    // Cleanup network stack
    void (*cleanup)(void);
} net_ops_t;

// Platform provides this
extern const net_ops_t* get_net_ops(void);

#endif // NETWORK_H
```

## Platform Implementations

### Linux (POSIX Sockets)

**File:** `net_posix.c`

```c
#include "network.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>

static int listen_fd = -1;

static int posix_init(uint16_t port) {
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) return -1;

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(listen_fd);
        return -1;
    }

    if (listen(listen_fd, 1) < 0) {
        close(listen_fd);
        return -1;
    }

    printf("Listening on port %d\n", port);
    return 0;
}

static net_conn_t posix_accept(void) {
    int client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd < 0) return NULL;
    return (net_conn_t)(intptr_t)client_fd;
}

static ssize_t posix_read(net_conn_t conn, uint8_t* buf, size_t len) {
    int fd = (int)(intptr_t)conn;
    return recv(fd, buf, len, 0);
}

static ssize_t posix_write(net_conn_t conn, const uint8_t* buf, size_t len) {
    int fd = (int)(intptr_t)conn;
    return send(fd, buf, len, 0);
}

static void posix_close(net_conn_t conn) {
    int fd = (int)(intptr_t)conn;
    close(fd);
}

static void posix_cleanup(void) {
    if (listen_fd >= 0) {
        close(listen_fd);
        listen_fd = -1;
    }
}

static const net_ops_t posix_ops = {
    .init = posix_init,
    .accept = posix_accept,
    .read = posix_read,
    .write = posix_write,
    .close = posix_close,
    .cleanup = posix_cleanup,
};

const net_ops_t* get_net_ops(void) {
    return &posix_ops;
}
```

### ESP32 (lwIP)

**File:** `net_lwip.c`

```c
#include "network.h"
#include "lwip/sockets.h"
#include "esp_log.h"

static const char *TAG = "net_lwip";
static int listen_fd = -1;

static int lwip_init(uint16_t port) {
    listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_fd < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        return -1;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Bind failed");
        close(listen_fd);
        return -1;
    }

    if (listen(listen_fd, 1) < 0) {
        ESP_LOGE(TAG, "Listen failed");
        close(listen_fd);
        return -1;
    }

    ESP_LOGI(TAG, "SSH server listening on port %d", port);
    return 0;
}

static net_conn_t lwip_accept(void) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd < 0) {
        ESP_LOGE(TAG, "Accept failed");
        return NULL;
    }

    ESP_LOGI(TAG, "Client connected");
    return (net_conn_t)(intptr_t)client_fd;
}

static ssize_t lwip_read(net_conn_t conn, uint8_t* buf, size_t len) {
    int fd = (int)(intptr_t)conn;
    return recv(fd, buf, len, 0);
}

static ssize_t lwip_write(net_conn_t conn, const uint8_t* buf, size_t len) {
    int fd = (int)(intptr_t)conn;
    return send(fd, buf, len, 0);
}

static void lwip_close(net_conn_t conn) {
    int fd = (int)(intptr_t)conn;
    close(fd);
    ESP_LOGI(TAG, "Connection closed");
}

static void lwip_cleanup(void) {
    if (listen_fd >= 0) {
        close(listen_fd);
        listen_fd = -1;
    }
}

static const net_ops_t lwip_ops = {
    .init = lwip_init,
    .accept = lwip_accept,
    .read = lwip_read,
    .write = lwip_write,
    .close = lwip_close,
    .cleanup = lwip_cleanup,
};

const net_ops_t* get_net_ops(void) {
    return &lwip_ops;
}
```

### STM32 (Bare Metal with ETH)

**File:** `net_stm32.c`

```c
#include "network.h"
#include "stm32f4xx_hal.h"
// Include your ethernet driver

// Custom TCP implementation or lightweight TCP stack
// This is platform-specific

static int stm32_init(uint16_t port) {
    // Initialize ethernet hardware
    // Set up TCP listener
    return 0;
}

static net_conn_t stm32_accept(void) {
    // Poll for incoming connections
    // Return connection handle
    return NULL;
}

static ssize_t stm32_read(net_conn_t conn, uint8_t* buf, size_t len) {
    // Read from TCP connection
    return -1;
}

static ssize_t stm32_write(net_conn_t conn, const uint8_t* buf, size_t len) {
    // Write to TCP connection
    return -1;
}

static void stm32_close(net_conn_t conn) {
    // Close TCP connection
}

static void stm32_cleanup(void) {
    // Cleanup
}

static const net_ops_t stm32_ops = {
    .init = stm32_init,
    .accept = stm32_accept,
    .read = stm32_read,
    .write = stm32_write,
    .close = stm32_close,
    .cleanup = stm32_cleanup,
};

const net_ops_t* get_net_ops(void) {
    return &stm32_ops;
}
```

## Main Application (Platform Independent)

```c
#include "network.h"
#include "ssh_server.h"

int main(void) {
    // Platform-specific initialization
    #ifdef ESP32
    // Initialize WiFi, NVS, etc.
    #endif

    // Get platform network operations
    const net_ops_t* net = get_net_ops();

    // Initialize network
    if (net->init(2222) < 0) {
        return -1;
    }

    // Main server loop
    while (1) {
        // Accept connection
        net_conn_t conn = net->accept();
        if (!conn) continue;

        // Handle SSH session (platform independent)
        ssh_handle_connection(conn, net);

        // Close connection
        net->close(conn);
    }

    net->cleanup();
    return 0;
}
```

## Platform-Specific Considerations

### Memory Constraints

Microcontrollers have limited RAM. Optimize buffer sizes:

```c
// config.h
#ifdef PLATFORM_ESP32
  #define MAX_PACKET_SIZE 4096
  #define RECV_BUFFER_SIZE 2048
#elif defined(PLATFORM_STM32F4)
  #define MAX_PACKET_SIZE 2048
  #define RECV_BUFFER_SIZE 1024
#else
  #define MAX_PACKET_SIZE 16384
  #define RECV_BUFFER_SIZE 8192
#endif
```

### Static vs Dynamic Allocation

**Microcontrollers:** Prefer static allocation

```c
// Instead of:
uint8_t *buffer = malloc(size);

// Use:
static uint8_t buffer[MAX_PACKET_SIZE];
```

### Random Number Generation

Each platform has different RNG:

```c
// random.h
void platform_randombytes(uint8_t *buf, size_t len);

// random_posix.c
void platform_randombytes(uint8_t *buf, size_t len) {
    int fd = open("/dev/urandom", O_RDONLY);
    read(fd, buf, len);
    close(fd);
}

// random_esp32.c
void platform_randombytes(uint8_t *buf, size_t len) {
    esp_fill_random(buf, len);
}

// random_stm32.c
extern RNG_HandleTypeDef hrng;
void platform_randombytes(uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i += 4) {
        uint32_t random;
        HAL_RNG_GenerateRandomNumber(&hrng, &random);
        memcpy(buf + i, &random, (len - i) < 4 ? (len - i) : 4);
    }
}
```

### Host Key Storage

**Development:** Generate and hardcode

```bash
# Generate Ed25519 key
ssh-keygen -t ed25519 -f host_key

# Convert to C array
xxd -i host_key > host_key.c
```

**host_key.c:**
```c
const uint8_t host_private_key[64] = {
    0x1a, 0x2b, 0x3c, /* ... */
};

const uint8_t host_public_key[32] = {
    0x4d, 0x5e, 0x6f, /* ... */
};
```

**Production:** Store in secure element or flash

### Timing

Microcontrollers may not have `time()`:

```c
// Use millisecond counter
uint32_t get_ms(void) {
    #ifdef PLATFORM_ESP32
    return esp_timer_get_time() / 1000;
    #elif defined(PLATFORM_STM32)
    return HAL_GetTick();
    #else
    return 0;  // Not needed for minimal implementation
    #endif
}
```

## Build System Integration

### Makefile for Multiple Platforms

```makefile
# v1-portable/Makefile

# Platform selection
PLATFORM ?= posix

# Common sources
COMMON_SRCS = main.c ssh_proto.c crypto.c

# Platform-specific sources
ifeq ($(PLATFORM),posix)
    PLATFORM_SRCS = net_posix.c random_posix.c
    CFLAGS += -DPLATFORM_POSIX
else ifeq ($(PLATFORM),esp32)
    PLATFORM_SRCS = net_lwip.c random_esp32.c
    CFLAGS += -DPLATFORM_ESP32
    # Add ESP-IDF flags
else ifeq ($(PLATFORM),stm32)
    PLATFORM_SRCS = net_stm32.c random_stm32.c
    CFLAGS += -DPLATFORM_STM32
    # Add STM32 HAL flags
endif

SRCS = $(COMMON_SRCS) $(PLATFORM_SRCS)

all: nano_ssh_server

nano_ssh_server: $(SRCS)
    $(CC) $(CFLAGS) -o $@ $^

clean:
    rm -f nano_ssh_server *.o
```

### Usage

```bash
# Build for Linux
make PLATFORM=posix

# Build for ESP32
make PLATFORM=esp32

# Build for STM32
make PLATFORM=stm32
```

## ESP32 Complete Example

### Project Structure

```
v1-portable-esp32/
├── CMakeLists.txt
├── sdkconfig
├── main/
│   ├── CMakeLists.txt
│   ├── main.c
│   ├── ssh_proto.c
│   ├── crypto.c
│   ├── net_lwip.c
│   └── random_esp32.c
└── components/
    └── tweetnacl/
```

### main/CMakeLists.txt

```cmake
idf_component_register(
    SRCS "main.c" "ssh_proto.c" "crypto.c" "net_lwip.c" "random_esp32.c"
    INCLUDE_DIRS "."
    REQUIRES lwip nvs_flash esp_wifi
)
```

### main/main.c (ESP32)

```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "network.h"
#include "ssh_server.h"

#define WIFI_SSID "your-ssid"
#define WIFI_PASS "your-password"

void wifi_init(void) {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
}

void app_main(void) {
    wifi_init();

    // Wait for WiFi connection
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    const net_ops_t* net = get_net_ops();

    if (net->init(2222) < 0) {
        ESP_LOGE("main", "Failed to initialize network");
        return;
    }

    while (1) {
        net_conn_t conn = net->accept();
        if (conn) {
            ssh_handle_connection(conn, net);
            net->close(conn);
        }
    }
}
```

## Testing on Target Hardware

### ESP32 Testing

```bash
# Build
cd v1-portable-esp32
idf.py build

# Flash
idf.py flash

# Monitor
idf.py monitor

# From another machine on same network:
ssh -p 2222 user@<esp32-ip-address>
```

### Serial Console Debugging

Add debug output:

```c
#define DEBUG_PRINTF(...) printf(__VA_ARGS__)

// In code:
DEBUG_PRINTF("Received KEXINIT, length: %u\n", packet_len);
```

## Memory Optimization for Microcontrollers

### Stack Usage

```c
// Check stack usage
#ifdef ESP32
    UBaseType_t high_water_mark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI("main", "Stack high water mark: %u bytes", high_water_mark);
#endif
```

### Heap Usage

```c
#ifdef ESP32
    ESP_LOGI("main", "Free heap: %u bytes", esp_get_free_heap_size());
#endif
```

### Code Size Optimization

```cmake
# ESP32: Optimize for size
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os -ffunction-sections -fdata-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
```

## Porting Checklist

- [ ] Implement network abstraction layer for target platform
- [ ] Implement random number generation
- [ ] Adapt memory allocation (static buffers)
- [ ] Embed host key in firmware
- [ ] Adjust buffer sizes for available RAM
- [ ] Test on target hardware
- [ ] Measure memory usage (stack, heap, code size)
- [ ] Optimize if needed
- [ ] Document platform-specific setup

## Platform-Specific Documentation

Create separate guides:
- `PORTING_ESP32.md`
- `PORTING_STM32.md`
- `PORTING_PICO.md`

Each with specific build instructions, pin configurations, and examples.
