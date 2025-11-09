#!/bin/bash
# Minimal echo server to test socat

echo "Echo server started" >&2
echo "HELLO FROM SERVER"
read -r line
echo "Got: $line" >&2
echo "ECHO: $line"
