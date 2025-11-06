#!/bin/bash

echo "Test 1: Empty string"
echo -n '' | openssl dgst -sha256 | awk '{print $2}'

echo ""
echo "Test 2: abc"
echo -n 'abc' | openssl dgst -sha256 | awk '{print $2}'

echo ""
echo "Test 3: abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
echo -n 'abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq' | openssl dgst -sha256 | awk '{print $2}'

echo ""
echo "Test 4: 'The quick brown fox jumps over the lazy dog. This is a test.'"
echo -n 'The quick brown fox jumps over the lazy dog. This is a test.' | openssl dgst -sha256 | awk '{print $2}'

echo ""
echo "Test 5: Multi-block"
echo -n 'The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog.' | openssl dgst -sha256 | awk '{print $2}'

echo ""
echo "Test 6: 'a'"
echo -n 'a' | openssl dgst -sha256 | awk '{print $2}'

echo ""
echo "Test 7: 56 bytes"
echo -n '12345678901234567890123456789012345678901234567890123456' | openssl dgst -sha256 | awk '{print $2}'

echo ""
echo "Test 8: 63 bytes"
echo -n '123456789012345678901234567890123456789012345678901234567890123' | openssl dgst -sha256 | awk '{print $2}'
