#!/usr/bin/env python3
"""
Generate test vectors for bignum operations
Uses Python's built-in arbitrary precision integers
"""

def int_to_bytes_be(n, length):
    """Convert integer to big-endian bytes"""
    return n.to_bytes(length, byteorder='big')

def bytes_to_hex(b):
    """Convert bytes to hex string for C arrays"""
    return ', '.join(f'0x{x:02x}' for x in b)

# DH Group14 Prime (RFC 3526)
P = int(
    'FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1'
    '29024E088A67CC74020BBEA63B139B22514A08798E3404DD'
    'EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245'
    'E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED'
    'EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D'
    'C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F'
    '83655D23DCA3AD961C62F356208552BB9ED529077096966D'
    '670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B'
    'E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9'
    'DE2BCBF6955817183995497CEA956AE515D2261898FA0510'
    '15728E5A8AACAA68FFFFFFFFFFFFFFFF', 16
)

print("=" * 80)
print("BIGNUM TEST VECTORS")
print("=" * 80)

# Test 1: Simple addition (small numbers)
print("\n=== Test 1: Addition (small) ===")
a = 12345
b = 67890
c = a + b
print(f"a = {a}")
print(f"b = {b}")
print(f"c = a + b = {c}")
a_bytes = int_to_bytes_be(a, 256)
b_bytes = int_to_bytes_be(b, 256)
c_bytes = int_to_bytes_be(c, 256)
print(f"a_bytes[255] = 0x{a_bytes[255]:02x}")
print(f"b_bytes[255] = 0x{b_bytes[255]:02x}")
print(f"c_bytes[255] = 0x{c_bytes[255]:02x}")

# Test 2: Multiplication (medium numbers)
print("\n=== Test 2: Multiplication ===")
a = 123456789
b = 987654321
c = a * b
print(f"a = {a}")
print(f"b = {b}")
print(f"c = a * b = {c}")
print(f"c = {c:064x}")

# Test 3: Modulo with large number
print("\n=== Test 3: Modulo (large) ===")
a = 2**256 - 1  # Max 256-byte number
m = 2**255      # Half
c = a % m
print(f"a = 2^256 - 1")
print(f"m = 2^255")
print(f"c = a mod m = {c}")
print(f"c_hex = {c:064x}")

# Test 4: Modular exponentiation (DH-like)
print("\n=== Test 4: Modexp (DH Group14) ===")
base = 2  # Generator
exp = 123456789  # Small private key for testing
result = pow(base, exp, P)
print(f"base = {base}")
print(f"exp = {exp}")
print(f"result = base^exp mod P")
result_bytes = int_to_bytes_be(result, 256)
print(f"First 32 bytes: {bytes_to_hex(result_bytes[:32])}")
print(f"Last 32 bytes:  {bytes_to_hex(result_bytes[224:256])}")

# Test 5: Full DH key generation (2^private mod P)
print("\n=== Test 5: DH Key Generation (g=2) ===")
# Use a fixed private key for reproducibility
private_key = 0x123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0
public_key = pow(2, private_key, P)
print(f"private_key = 0x{private_key:064x}")
print(f"public_key = 2^private_key mod P")
public_bytes = int_to_bytes_be(public_key, 256)
print(f"public_key first 16 bytes: {bytes_to_hex(public_bytes[:16])}")
print(f"public_key last 16 bytes:  {bytes_to_hex(public_bytes[240:256])}")

# Verify it's not zero or one
if public_key == 0:
    print("ERROR: Public key is ZERO!")
elif public_key == 1:
    print("ERROR: Public key is ONE!")
else:
    print(f"✓ Public key is valid (not 0 or 1)")
    print(f"  Public key = {public_key:064x}...")

# Test 6: Byte conversion round-trip
print("\n=== Test 6: Byte Conversion ===")
test_val = 0xDEADBEEFCAFEBABE123456789ABCDEF0
test_bytes = int_to_bytes_be(test_val, 256)
print(f"Original: 0x{test_val:032x}")
print(f"Bytes[240:256]: {bytes_to_hex(test_bytes[240:256])}")
# Convert back
recovered = int.from_bytes(test_bytes, byteorder='big')
print(f"Recovered: 0x{recovered:032x}")
print(f"Match: {test_val == recovered}")

# Test 7: 2^10 mod 1000 (from our existing test)
print("\n=== Test 7: Small modexp (2^10 mod 1000) ===")
result = pow(2, 10, 1000)
print(f"2^10 mod 1000 = {result}")
print(f"Expected: 24")
print(f"Match: {result == 24}")

print("\n" + "=" * 80)
print("Test vectors generated successfully")
print("=" * 80)
