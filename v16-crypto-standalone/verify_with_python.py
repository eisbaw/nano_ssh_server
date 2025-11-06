# Verify RSA computation with Python
n_hex = "a73e9d978aeba112405d963cc7665ca7dee3bf6bebf44720c723e01435afc535eb24c419f2f6e8184a5e672573bd60adb24bf08c784db1f78540d7c2b2c5f724b8d21bbb6d96565020 3a2559f84256849442a800169c9aa46b13535149255b433c09a20ffdefc46e4c470e619d5d490018955e6d6309487f91127426d716a9c38aebaa67f09f3f1e3034ed1116db3ba27ab64e99c927691102c52bf98b86274b42a2721e1c292d90314e60b6b4395b560ad0d3bb4acd09ec55a2f54964cec8940816 4640d165fa8434b2b72ada11113599e41f91732f331b5d4227ddf5c7c39348dd1b76e6946fd6206f8a674b9db3c3a9a4b7af5544004ccb62701f4ae486bf"

n = int(n_hex.replace(" ", ""), 16)
print(f"n = {n}")
print()

# Test small cases first
for exp_val in [3, 65537]:
    result = pow(2, exp_val, n)
    print(f"2^{exp_val} mod n =")
    result_hex = hex(result)[2:]
    print(f"  Hex: {result_hex}")
    print(f"  Low word: {result & 0xFFFFFFFF}")
    print()

# Test with base=42
for exp_val in [3, 65537]:
    result = pow(42, exp_val, n)
    print(f"42^{exp_val} mod n =")
    result_hex = hex(result)[2:]
    print(f"  Hex: {result_hex[:64]}...")
    print(f"  Low word: {result & 0xFFFFFFFF}")
    print()
