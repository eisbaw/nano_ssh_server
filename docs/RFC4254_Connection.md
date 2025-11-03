# RFC 4254: SSH Connection Protocol

**Reference:** https://datatracker.ietf.org/doc/html/rfc4254

## Overview

The SSH Connection Protocol runs on top of the SSH Transport and Authentication layers. It multiplexes multiple logical channels over a single encrypted SSH connection.

## Prerequisites

- SSH Transport layer established (RFC 4253)
- User authenticated (RFC 4252)
- All packets encrypted

## Protocol Flow (Minimal - Single Session)

```
Client                              Server
------                              ------
[Authenticated]
|                                   |
|-- SSH_MSG_CHANNEL_OPEN ---------->| (channel type: "session")
|<-- SSH_MSG_CHANNEL_OPEN_CONFIRM --| (assign channel IDs)
|                                   |
|-- SSH_MSG_CHANNEL_REQUEST ------->| (request type varies)
|<-- SSH_MSG_CHANNEL_SUCCESS/FAIL --|
|                                   |
|<-- SSH_MSG_CHANNEL_DATA -----------| (server sends data)
|-- SSH_MSG_CHANNEL_DATA ---------->| (client sends data)
|                                   |
|-- SSH_MSG_CHANNEL_EOF ----------->|
|-- SSH_MSG_CHANNEL_CLOSE ---------->|
|<-- SSH_MSG_CHANNEL_CLOSE ----------|
```

## Message Types

```
SSH_MSG_GLOBAL_REQUEST                80
SSH_MSG_REQUEST_SUCCESS               81
SSH_MSG_REQUEST_FAILURE               82
SSH_MSG_CHANNEL_OPEN                  90
SSH_MSG_CHANNEL_OPEN_CONFIRMATION     91
SSH_MSG_CHANNEL_OPEN_FAILURE          92
SSH_MSG_CHANNEL_WINDOW_ADJUST         93
SSH_MSG_CHANNEL_DATA                  94
SSH_MSG_CHANNEL_EXTENDED_DATA         95
SSH_MSG_CHANNEL_EOF                   96
SSH_MSG_CHANNEL_CLOSE                 97
SSH_MSG_CHANNEL_REQUEST               98
SSH_MSG_CHANNEL_SUCCESS               99
SSH_MSG_CHANNEL_FAILURE              100
```

## 1. Channel Concepts

### Channel Numbers
- Each side assigns its own channel numbers
- Client picks a number for client→server messages
- Server picks a number for server→client messages
- Numbers are per-direction and can be the same

### Window Size
- Flow control mechanism
- Sender cannot send more data than window allows
- Receiver sends SSH_MSG_CHANNEL_WINDOW_ADJUST to grant more

### Maximum Packet Size
- Maximum size of data in SSH_MSG_CHANNEL_DATA
- Typical: 32768 bytes
- Minimal implementation: 16384 or even 4096

## 2. Opening a Channel

### Client Opens Session

```
byte      SSH_MSG_CHANNEL_OPEN (90)
string    channel_type ("session")
uint32    sender_channel (client's channel number, e.g., 0)
uint32    initial_window_size (e.g., 32768)
uint32    maximum_packet_size (e.g., 16384)
```

Example:
```c
// Client sends (we receive this on server):
msg_type:     90
channel_type: "session"
sender_ch:    0        // client's channel ID
window_size:  32768    // client's receive window
max_packet:   16384    // max data per packet
```

### Server Confirms

```
byte      SSH_MSG_CHANNEL_OPEN_CONFIRMATION (91)
uint32    recipient_channel (client's channel number, echo back)
uint32    sender_channel (server's channel number, e.g., 0)
uint32    initial_window_size (server's receive window)
uint32    maximum_packet_size (server's max packet)
```

Example:
```c
// Server sends:
msg_type:     91
recipient_ch: 0        // client's channel ID (echo)
sender_ch:    0        // server's channel ID
window_size:  32768    // server's receive window
max_packet:   16384    // max data per packet
```

### Server Rejects (if needed)

```
byte      SSH_MSG_CHANNEL_OPEN_FAILURE (92)
uint32    recipient_channel (client's channel number)
uint32    reason_code
string    description (UTF-8)
string    language_tag
```

Reason codes:
```
SSH_OPEN_ADMINISTRATIVELY_PROHIBITED    1
SSH_OPEN_CONNECT_FAILED                 2
SSH_OPEN_UNKNOWN_CHANNEL_TYPE           3
SSH_OPEN_RESOURCE_SHORTAGE             4
```

## 3. Channel Requests

After channel is open, client makes requests on that channel.

### Generic Format

```
byte      SSH_MSG_CHANNEL_REQUEST (98)
uint32    recipient_channel
string    request_type
boolean   want_reply
... request-specific data follows
```

### Common Request Types

#### Shell Request
```
byte      SSH_MSG_CHANNEL_REQUEST (98)
uint32    recipient_channel (server's channel ID)
string    "shell"
boolean   TRUE (want reply)
```

#### Exec Request
```
byte      SSH_MSG_CHANNEL_REQUEST (98)
uint32    recipient_channel
string    "exec"
boolean   TRUE
string    command
```

#### PTY Request
```
byte      SSH_MSG_CHANNEL_REQUEST (98)
uint32    recipient_channel
string    "pty-req"
boolean   FALSE (usually)
string    TERM_environment_variable (e.g., "xterm")
uint32    terminal_width_chars
uint32    terminal_height_rows
uint32    terminal_width_pixels (0 if not available)
uint32    terminal_height_pixels (0 if not available)
string    encoded_terminal_modes
```

### Server Response

If want_reply was TRUE:

**Success:**
```
byte      SSH_MSG_CHANNEL_SUCCESS (99)
uint32    recipient_channel (client's channel ID)
```

**Failure:**
```
byte      SSH_MSG_CHANNEL_FAILURE (100)
uint32    recipient_channel (client's channel ID)
```

## 4. Sending Data

### Normal Data

```
byte      SSH_MSG_CHANNEL_DATA (94)
uint32    recipient_channel
string    data (length-prefixed)
```

Example:
```c
// Server sends "Hello World\r\n" to client:
msg_type:   94
channel:    0  // client's channel ID
data_len:   13
data:       "Hello World\r\n"
```

### Extended Data (stderr)

```
byte      SSH_MSG_CHANNEL_EXTENDED_DATA (95)
uint32    recipient_channel
uint32    data_type_code (1 = stderr)
string    data
```

**For minimal implementation:** Only use SSH_MSG_CHANNEL_DATA.

## 5. Flow Control

### Window Management

Sender maintains:
- **Window size**: bytes allowed to send before blocking
- Decreases when sending data
- Increases when receiving WINDOW_ADJUST

Receiver maintains:
- **Consumed**: bytes received and consumed
- Sends WINDOW_ADJUST to grant more

### Window Adjust

```
byte      SSH_MSG_CHANNEL_WINDOW_ADJUST (93)
uint32    recipient_channel
uint32    bytes_to_add
```

Example:
```c
// After consuming 1024 bytes, grant 1024 more:
msg_type:   93
channel:    0
bytes:      1024
```

### Minimal Implementation Strategy

- Start with large windows (32768 bytes)
- For "Hello World" use case, window never exhausts
- Can ignore WINDOW_ADJUST if we only send small messages
- **But MUST implement for real SSH client compatibility**

## 6. Closing a Channel

### EOF (No More Data)

```
byte      SSH_MSG_CHANNEL_EOF (96)
uint32    recipient_channel
```

Indicates no more data will be sent, but channel still open for receiving.

### Close

```
byte      SSH_MSG_CHANNEL_CLOSE (97)
uint32    recipient_channel
```

Both sides must send CLOSE. After receiving CLOSE, respond with CLOSE if not already sent, then free channel resources.

### Proper Shutdown Sequence

```
Server sends data → EOF → waits → sends CLOSE → waits for CLOSE → done
```

```c
// Example:
send_channel_data(channel, "Hello World\r\n");
send_channel_eof(channel);
send_channel_close(channel);
// Wait for client's CLOSE, then free resources
```

## 7. Minimal Implementation for "Hello World"

### Server Side

```c
// 1. Receive SSH_MSG_CHANNEL_OPEN
//    Parse: channel_type, sender_channel, window, max_packet
//    Verify channel_type == "session"

// 2. Send SSH_MSG_CHANNEL_OPEN_CONFIRMATION
//    recipient_channel = client's sender_channel
//    sender_channel = 0 (our channel ID)
//    window = 32768
//    max_packet = 16384

// 3. Receive SSH_MSG_CHANNEL_REQUEST
//    Common types: "pty-req", "shell", "exec"
//    For minimal: accept "shell" or "exec", ignore "pty-req"
//    Send SSH_MSG_CHANNEL_SUCCESS if want_reply == TRUE

// 4. Send SSH_MSG_CHANNEL_DATA with "Hello World\r\n"
//    recipient_channel = client's channel ID
//    data = "Hello World\r\n"

// 5. Send SSH_MSG_CHANNEL_EOF

// 6. Send SSH_MSG_CHANNEL_CLOSE

// 7. Receive SSH_MSG_CHANNEL_CLOSE from client

// 8. Connection complete
```

### Minimal Channel State

```c
struct channel {
    uint32_t client_id;      // client's channel number
    uint32_t server_id;      // server's channel number (can be 0)
    uint32_t client_window;  // how much we can send
    uint32_t server_window;  // how much client can send
    uint32_t max_packet;     // max data per packet
    bool closed;
};
```

## 8. Channel Types

### "session" (REQUIRED, what we use)
- Interactive shell
- Command execution
- Single use: one command or shell per channel

### "x11" (skip)
- X11 forwarding

### "forwarded-tcpip" (skip)
- Port forwarding

### "direct-tcpip" (skip)
- Port forwarding

**For minimal implementation:** Only support "session" type.

## 9. Implementation Checklist

- [ ] Parse SSH_MSG_CHANNEL_OPEN ("session")
- [ ] Send SSH_MSG_CHANNEL_OPEN_CONFIRMATION
- [ ] Store channel IDs and window sizes
- [ ] Parse SSH_MSG_CHANNEL_REQUEST
  - [ ] Handle "pty-req" (can ignore or accept)
  - [ ] Handle "shell" (accept)
  - [ ] Handle "exec" (optional)
- [ ] Send SSH_MSG_CHANNEL_SUCCESS/FAILURE
- [ ] Send SSH_MSG_CHANNEL_DATA with "Hello World\r\n"
- [ ] Handle incoming SSH_MSG_CHANNEL_DATA (optional for one-way)
- [ ] Send SSH_MSG_CHANNEL_EOF
- [ ] Send SSH_MSG_CHANNEL_CLOSE
- [ ] Receive and handle SSH_MSG_CHANNEL_CLOSE
- [ ] Window management (at least basic tracking)

## 10. Testing with OpenSSH Client

```bash
# Simple connection (client will request PTY and shell)
ssh -p 2222 user@localhost

# Execute command
ssh -p 2222 user@localhost "echo test"

# Without PTY (simpler for testing)
ssh -T -p 2222 user@localhost

# Verbose for debugging
ssh -vvv -p 2222 user@localhost
```

## Size Optimization

- Only support "session" channel type
- Ignore PTY requests (respond with success anyway)
- Fixed window size (no dynamic adjustment)
- Single channel only (no multiplexing)
- Skip extended data (stderr)
- Minimal state tracking

## Common Client Behavior

1. Opens session channel
2. Requests PTY (pty-req)
3. Requests shell or exec
4. Sends window size changes (window-change)
5. Sends data (keyboard input)
6. Receives data (command output)
7. Sends EOF when done
8. Sends CLOSE

**For "Hello World" server:**
- Accept PTY request (respond success, but ignore)
- Accept shell/exec request
- Send "Hello World\r\n"
- Send EOF and CLOSE immediately
- Client disconnects

## Error Handling

Minimal implementation can be strict:
- Only accept "session" channel type
- Only accept first channel (reject subsequent opens)
- Only accept "shell" request
- Ignore other requests (respond with failure)

This reduces code size significantly.
