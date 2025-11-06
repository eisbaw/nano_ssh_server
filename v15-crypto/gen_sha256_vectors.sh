#!/bin/bash

# Generate correct SHA-256 test vectors using OpenSSL

echo "Test 1: 'The quick brown fox jumps over the lazy dog. This is a test.'"
echo -n 'The quick brown fox jumps over the lazy dog. This is a test.' | openssl dgst -sha256 | awk '{print $2}'

echo ""
echo "Test 2: Multi-block"
echo -n 'The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog.' | openssl dgst -sha256 | awk '{print $2}'

echo ""
echo "Test 3: 56 bytes"
echo -n '12345678901234567890123456789012345678901234567890123456' | openssl dgst -sha256 | awk '{print $2}'

echo ""
echo "Test 4: 63 bytes"
echo -n '123456789012345678901234567890123456789012345678901234567890123' | openssl dgst -sha256 | awk '{print $2}'
