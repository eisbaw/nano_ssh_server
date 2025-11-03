# RFC 4252: SSH Authentication Protocol

**Reference:** https://datatracker.ietf.org/doc/html/rfc4252

## Overview

The SSH Authentication Protocol runs on top of the SSH Transport Layer Protocol. It provides client authentication to the server using one or more authentication methods.

## Prerequisites

- SSH Transport layer established (RFC 4253 complete)
- Service "ssh-userauth" must be requested and accepted
- All packets are encrypted at this point

## Protocol Flow

```
Client                              Server
------                              ------
[Transport layer established]
|                                   |
|-- SSH_MSG_SERVICE_REQUEST ------->| (service: "ssh-userauth")
|<-- SSH_MSG_SERVICE_ACCEPT --------|
|                                   |
|-- SSH_MSG_USERAUTH_REQUEST ------>|
|                                   |
|<-- SSH_MSG_USERAUTH_SUCCESS ------| (if accepted)
|        OR                         |
|<-- SSH_MSG_USERAUTH_FAILURE ------| (if rejected)
|                                   |
```

## Message Types

```
SSH_MSG_USERAUTH_REQUEST            50
SSH_MSG_USERAUTH_FAILURE            51
SSH_MSG_USERAUTH_SUCCESS            52
SSH_MSG_USERAUTH_BANNER             53
SSH_MSG_USERAUTH_PK_OK              60  // for publickey
```

## 1. Starting Authentication

Already covered in Transport layer, but for completeness:

```
byte      SSH_MSG_SERVICE_REQUEST (5)
string    "ssh-userauth"
```

Server responds:
```
byte      SSH_MSG_SERVICE_ACCEPT (6)
string    "ssh-userauth"
```

## 2. Authentication Request (Generic)

```
byte      SSH_MSG_USERAUTH_REQUEST (50)
string    user_name
string    service_name  (usually "ssh-connection")
string    method_name   ("password", "publickey", "none")
... method-specific fields follow
```

## 3. Password Authentication (SIMPLEST)

### Request Format
```
byte      SSH_MSG_USERAUTH_REQUEST (50)
string    user_name (e.g., "user")
string    service_name ("ssh-connection")
string    "password"
boolean   FALSE (change password flag, always FALSE for minimal impl)
string    plaintext_password
```

### Implementation Example
```c
// Build password auth request (server receives this)
uint8_t msg_type = SSH_MSG_USERAUTH_REQUEST;
// string user_name
// string "ssh-connection"
// string "password"
// boolean FALSE
// string password

// On server side:
// 1. Extract username and password
// 2. Compare with expected values (hardcoded for minimal impl)
// 3. Send success or failure
```

### Success Response
```
byte      SSH_MSG_USERAUTH_SUCCESS (52)
```

### Failure Response
```
byte      SSH_MSG_USERAUTH_FAILURE (51)
name-list authentications_that_can_continue
boolean   partial_success (FALSE for total failure)
```

Example failure:
```
byte      51 (SSH_MSG_USERAUTH_FAILURE)
string    "password,publickey"  // methods that can continue
boolean   FALSE
```

## 4. Minimal Implementation (Password Only)

### Server Side

```c
// After receiving SSH_MSG_USERAUTH_REQUEST (50):

// Parse message:
char username[32];
char service[32];
char method[32];
boolean change_password;
char password[32];

// Extract fields...

// For minimal implementation, hardcode credentials:
const char *EXPECTED_USER = "user";
const char *EXPECTED_PASS = "password123";

if (strcmp(method, "password") == 0 &&
    change_password == FALSE &&
    strcmp(username, EXPECTED_USER) == 0 &&
    strcmp(password, EXPECTED_PASS) == 0) {

    // Send SUCCESS
    send_packet(SSH_MSG_USERAUTH_SUCCESS);

} else {
    // Send FAILURE
    send_packet(SSH_MSG_USERAUTH_FAILURE, "password", FALSE);
}
```

### Packet Encoding Helpers

```c
// String encoding: uint32 length + data
void write_string(uint8_t *buf, size_t *offset, const char *str) {
    uint32_t len = strlen(str);
    write_uint32(buf, offset, len);
    memcpy(buf + *offset, str, len);
    *offset += len;
}

// Read string
void read_string(uint8_t *buf, size_t *offset, char *out, size_t maxlen) {
    uint32_t len = read_uint32(buf, offset);
    if (len < maxlen) {
        memcpy(out, buf + *offset, len);
        out[len] = '\0';
        *offset += len;
    }
}
```

## 5. Authentication Banner (Optional)

Server can send informational message before authentication:

```
byte      SSH_MSG_USERAUTH_BANNER (53)
string    message (UTF-8)
string    language_tag (empty string for default)
```

Example:
```
byte      53
string    "Welcome to NanoSSH\n"
string    ""
```

**For minimal implementation:** Skip this, not required.

## 6. Public Key Authentication (Advanced)

**Note:** This is more complex but may save code size if you eliminate password handling.

### Two-phase process:

#### Phase 1: Query (optional)
```
byte      SSH_MSG_USERAUTH_REQUEST (50)
string    user_name
string    "ssh-connection"
string    "publickey"
boolean   FALSE (not sending signature)
string    public_key_algorithm_name ("ssh-ed25519")
string    public_key_blob
```

Server responds:
```
byte      SSH_MSG_USERAUTH_PK_OK (60)
string    public_key_algorithm_name
string    public_key_blob
```

#### Phase 2: Authentication
```
byte      SSH_MSG_USERAUTH_REQUEST (50)
string    user_name
string    "ssh-connection"
string    "publickey"
boolean   TRUE (sending signature)
string    public_key_algorithm_name ("ssh-ed25519")
string    public_key_blob
string    signature
```

The signature is over:
```
string    session_identifier (exchange hash H from transport)
byte      SSH_MSG_USERAUTH_REQUEST (50)
string    user_name
string    "ssh-connection"
string    "publickey"
boolean   TRUE
string    public_key_algorithm_name
string    public_key_blob
```

### For Minimal Implementation

**Password is simpler** unless you want to embed a single public key in firmware.

If using publickey:
- Hardcode one Ed25519 public key in server
- Skip the query phase (optional anyway)
- Verify signature using TweetNaCl

## Implementation Checklist

- [ ] Parse SSH_MSG_USERAUTH_REQUEST
- [ ] Extract username, service, method
- [ ] For password method:
  - [ ] Extract password
  - [ ] Compare with hardcoded credentials
- [ ] Send SSH_MSG_USERAUTH_SUCCESS on match
- [ ] Send SSH_MSG_USERAUTH_FAILURE on failure
- [ ] Handle multiple attempts (optional)

## Security Notes

- Password sent encrypted (transport layer handles this)
- For minimal implementation: hardcode one username/password pair
- No brute-force protection needed for microcontroller use case
- Consider single public key instead of password for better security

## Size Optimization

**Password authentication:**
- Pros: Simple logic, minimal code
- Cons: Password string comparison

**Publickey authentication:**
- Pros: More secure, no password storage
- Cons: Signature verification code (but TweetNaCl is small)

**Recommendation for minimal size:** Start with password (simpler), test thoroughly, then try publickey in optimized version and compare binary sizes.

## Example Hardcoded Credentials

```c
// Option 1: Simple password (for testing)
#define AUTH_USER "admin"
#define AUTH_PASS "nano123"

// Option 2: Embedded public key (more secure)
static const uint8_t authorized_key[32] = {
    0x1a, 0x2b, 0x3c, 0x4d, /* ... 32 bytes ... */
};
```

## Common Errors

1. **Wrong service name:** Client usually requests "ssh-connection", not "ssh-userauth"
2. **Forgetting change_password boolean:** Always FALSE in minimal implementation
3. **String encoding:** Remember the uint32 length prefix on ALL strings
4. **Not checking method:** Always verify method == "password" before parsing password field

## Testing with OpenSSH Client

```bash
# Test with password
ssh -p 2222 admin@localhost

# Test with verbose output to see auth flow
ssh -vvv -p 2222 admin@localhost

# Force password authentication only
ssh -o PreferredAuthentications=password -p 2222 admin@localhost
```
