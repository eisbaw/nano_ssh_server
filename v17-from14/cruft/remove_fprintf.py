#!/usr/bin/env python3
"""
Remove all fprintf(stderr, ...) statements from C code
Handles both single-line and multi-line fprintf statements
"""

import sys
import re

def remove_fprintf_statements(input_file, output_file):
    with open(input_file, 'r') as f:
        lines = f.readlines()

    output_lines = []
    i = 0
    removed_count = 0

    while i < len(lines):
        line = lines[i]

        # Check if this line contains fprintf(stderr,
        if 'fprintf(stderr,' in line:
            removed_count += 1
            # Skip this line and continue until we find the semicolon
            if ';' not in line:
                # Multi-line fprintf, keep skipping until we find the semicolon
                i += 1
                while i < len(lines) and ';' not in lines[i]:
                    i += 1
                # Skip the line with the semicolon too
                if i < len(lines):
                    i += 1
            else:
                # Single-line fprintf, just skip this line
                i += 1
        else:
            # Keep this line
            output_lines.append(line)
            i += 1

    with open(output_file, 'w') as f:
        f.writelines(output_lines)

    print(f"Removed {removed_count} fprintf(stderr, ...) statements")
    print(f"Original lines: {len(lines)}")
    print(f"New lines: {len(output_lines)}")
    print(f"Lines removed: {len(lines) - len(output_lines)}")

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: python3 remove_fprintf.py <input_file> <output_file>")
        sys.exit(1)

    remove_fprintf_statements(sys.argv[1], sys.argv[2])
