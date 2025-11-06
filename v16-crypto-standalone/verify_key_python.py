#!/usr/bin/env python3
"""
Verify that the extracted RSA key values work correctly
"""

# Modulus n (from rsa.h)
n_bytes = bytes.fromhex("""
a73e9d978aeba11240  5d963cc7665ca7dee3bf6bebf44720c723e01435afc535eb24c419f2f6e8184a
5e672573bd60adb24bf08c784db1f78540d7c2b2c5f724b8d21bbb6d96565020  3a2559f8425684944  2a800169c9aa46b13
5351492  55b433c09a20ffdefc46e4c470e619d5d490018955e6d6309487f91127426d716a9c38aebaa67f09f3f
1e3034ed1116db3ba27ab64e99c927691102c52bf98b86274b42a2721e1c292d90314e60b6b4395b560ad0d3bb4acd09
ec55a2f54964cec89408164640d165fa8434b2b72ada1111359  9e41f91732f331b5d4227ddf5c7c39348dd1b76e6946fd6
206f8a674b9db3c3a9a4b7af55440  04ccb62701f4ae486bf
""".replace('\n', '').replace(' ', ''))

# Private exponent d (from rsa.h)
d_bytes = bytes.fromhex("""
52ca55e40f629e8d34ecbf331e387474988b6096b1aaeecd0497b754f9d0a5cd0fb6d716cb663bb5ce96d5f3bdcc4940
230ba1ac3fdfa25258162  5d8dd7bcf60cfd73ee0351b1f6631d5e6e897536b95dcf8f4467aeb124883330163335  4f94d
1abae00d8f94f245f19f99386c58a20b18a054aaccc46b2daf2895f42634b35b6982a4fd1f2d35edf3bc8c98a34bdb6f
33daae82d0f939846e85e1745b7c10bd010f4acdcdbbb414e156417a4d2376f11adab57c3b20a94c709a70ca7c7ae257
0da1df1821f7153cf554c2304cd67af0d5b9bce69f819c2a35d0baa730288bdcc4c751c84bffa9c66f43f2df81cf08d0
3925855  62ceff633376a8fd3fe6ffce5
""".replace('\n', '').replace(' ', ''))

# Public exponent e
e = 65537

# Convert to integers
n = int.from_bytes(n_bytes, byteorder='big')
d = int.from_bytes(d_bytes, byteorder='big')

print("=== Python RSA Key Verification ===\n")

print(f"Modulus n bits: {n.bit_length()}")
print(f"Private exp d bits: {d.bit_length()}")
print(f"Public exp e: {e}\n")

# Test RSA property: (m^e)^d mod n == m
m = 42
print(f"Test message m = {m}")

c = pow(m, e, n)
print(f"Encrypt: c = m^e mod n = {c}")

m2 = pow(c, d, n)
print(f"Decrypt: m2 = c^d mod n = {m2}")

if m == m2:
    print("\n✓ RSA math WORKS with Python")
    print("  The extracted key values are CORRECT")
    print("  The bug must be in bn_modexp implementation")
else:
    print(f"\n✗ RSA math BROKEN with Python")
    print(f"  Expected m={m}, got m2={m2}")
    print("  The extracted key values are WRONG")
