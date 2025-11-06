#!/usr/bin/env python3
"""
Generate a valid RSA-2048 key pair and output it in C format
for embedding in rsa.h
"""

from Crypto.PublicKey import RSA

# Generate RSA-2048 key
print("Generating RSA-2048 key pair...")
key = RSA.generate(2048)

# Extract components
n = key.n  # modulus
e = key.e  # public exponent (should be 65537)
d = key.d  # private exponent

# Convert to bytes (big-endian, 256 bytes)
n_bytes = n.to_bytes(256, byteorder='big')
d_bytes = d.to_bytes(256, byteorder='big')

print(f"Public exponent e = {e}")
print(f"Modulus n = {hex(n)[:50]}...")
print(f"Private exponent d = {hex(d)[:50]}...")
print()

# Output C array format
print("/* RSA modulus (n) - 2048 bits */")
print("static const uint8_t rsa_modulus[256] = {")
for i in range(0, 256, 8):
    line = "    " + ", ".join(f"0x{b:02x}" for b in n_bytes[i:i+8])
    if i + 8 < 256:
        line += ","
    print(line)
print("};")
print()

print("/* RSA private exponent (d) - 2048 bits */")
print("static const uint8_t rsa_private_exponent[256] = {")
for i in range(0, 256, 8):
    line = "    " + ", ".join(f"0x{b:02x}" for b in d_bytes[i:i+8])
    if i + 8 < 256:
        line += ","
    print(line)
print("};")
print()

print(f"/* Public exponent (e = {e}) */")
print(f"#define RSA_PUBLIC_EXPONENT {e}")
print()

# Verify the key
print("Verifying key pair...")
# Test: sign "hello" and verify
from Crypto.Signature import pkcs1_15
from Crypto.Hash import SHA256

msg = b"hello world"
h = SHA256.new(msg)
signature = pkcs1_15.new(key).sign(h)

try:
    pkcs1_15.new(key.publickey()).verify(h, signature)
    print("✓ Key pair is valid (signature verification successful)")
except:
    print("✗ Key pair is INVALID (signature verification failed)")

print("\nDone! Copy the arrays above into rsa.h")
