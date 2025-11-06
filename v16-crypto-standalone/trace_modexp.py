#!/usr/bin/env python3
"""
Generate step-by-step trace of modular exponentiation for comparison with C
"""

# RSA modulus (same as in C code)
n_hex = "a73e9d978aeba112405d963cc7665ca7dee3bf6bebf44720c723e01435afc535eb24c419f2f6e8184a5e672573bd60adb24bf08c784db1f78540d7c2b2c5f724b8d21bbb6d965650203a2559f84256849442a800169c9aa46b13535149255b433c09a20ffdefc46e4c470e619d5d490018955e6d6309487f91127426d716a9c38aebaa67f09f3f1e3034ed1116db3ba27ab64e99c927691102c52bf98b86274b42a2721e1c292d90314e60b6b4395b560ad0d3bb4acd09ec55a2f54964cec89408164640d165fa8434b2b72ada11113599e41f91732f331b5d4227ddf5c7c39348dd1b76e6946fd6206f8a674b9db3c3a9a4b7af5544004ccb62701f4ae486bf"

n = int(n_hex.replace(" ", ""), 16)

def trace_modexp(base, exp, mod, max_iterations=20):
    """Trace modular exponentiation with detailed output"""
    print(f"=== Tracing: {base}^{exp} mod n ===")
    print(f"base = {base}")
    print(f"exp = {exp} (0x{exp:x}, binary: {bin(exp)[2:]})")
    print(f"mod first 8 bytes = {hex(mod >> (2048-64))}")
    print()
    
    # Initialize
    result = 1
    base_copy = base % mod
    
    print(f"Initial: result = 1, base_copy = {base_copy}")
    print()
    
    # Binary exponentiation - process bits from LSB to MSB
    exp_bits = bin(exp)[2:][::-1]  # Reverse to process LSB first
    
    for i, bit in enumerate(exp_bits):
        if i >= max_iterations and max_iterations > 0:
            print(f"... (stopping trace after {max_iterations} iterations)")
            break
            
        print(f"--- Iteration {i} (bit {i} of exponent) ---")
        print(f"Exponent bit = {bit}")
        
        if bit == '1':
            old_result = result
            result = (result * base_copy) % mod
            print(f"  Bit is 1: result = (result * base_copy) mod n")
            print(f"           = ({old_result} * {base_copy}) mod n")
            print(f"           = {result}")
            print(f"           Low 32 bits: 0x{result & 0xFFFFFFFF:08x}")
        else:
            print(f"  Bit is 0: result unchanged = {result}")
            
        # Square base for next iteration (except on last bit)
        if i < len(exp_bits) - 1:
            old_base = base_copy
            base_copy = (base_copy * base_copy) % mod
            print(f"  Square base: base_copy = base_copy^2 mod n")
            print(f"             = {old_base}^2 mod n")
            print(f"             = {base_copy}")
            print(f"             Low 32 bits: 0x{base_copy & 0xFFFFFFFF:08x}")
        
        print()
    
    # Complete calculation without tracing
    for i in range(max_iterations, len(exp_bits)):
        if exp_bits[i] == '1':
            result = (result * base_copy) % mod
        if i < len(exp_bits) - 1:
            base_copy = (base_copy * base_copy) % mod
    
    print(f"=== Final Result ===")
    print(f"result = {result}")
    print(f"result (hex) = {hex(result)}")
    print(f"Low 32 bits: 0x{result & 0xFFFFFFFF:08x} ({result & 0xFFFFFFFF})")
    print()
    
    return result

# Test with small exponent first
print("TEST 1: Small exponent")
print("=" * 60)
trace_modexp(42, 3, n, max_iterations=10)

print("\n" * 2)
print("TEST 2: Large exponent (first 10 iterations)")
print("=" * 60)
trace_modexp(42, 65537, n, max_iterations=10)

print("\n" * 2)
print("TEST 3: Power of 2 exponent")
print("=" * 60)
trace_modexp(2, 17, n, max_iterations=20)

