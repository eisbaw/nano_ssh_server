#!/bin/bash
# Comprehensive SSH Server Version Analysis Script

echo "=== Building All Versions ==="
echo ""

# First, try to build all versions
for dir in v*-*/; do
    if [ -d "$dir" ] && [ -f "$dir/Makefile" ]; then
        version=$(basename "$dir")
        echo "Building $version..."
        cd "$dir" && make clean &>/dev/null && make &>/dev/null
        cd ..
    fi
done

echo ""
echo "=== Analysis Complete ==="
echo ""

# Header for the table
printf "%-25s | %12s | %8s | %15s | %s\n" "Version" "Size (bytes)" "Size (KB)" "Static/Dynamic" "Libraries"
printf "%s\n" "--------------------------------------------------------------------------------------------------------------"

# Analyze each version
for dir in v*-*/; do
    if [ -d "$dir" ]; then
        version=$(basename "$dir")
        binary="$dir/nano_ssh_server"

        if [ -f "$binary" ]; then
            # Get size
            size=$(stat -c%s "$binary" 2>/dev/null || stat -f%z "$binary" 2>/dev/null)
            kb=$(echo "scale=2; $size/1024" | bc)

            # Check if static or dynamic
            file_output=$(file "$binary")
            ldd_output=$(ldd "$binary" 2>&1)

            if echo "$file_output" | grep -q "statically linked"; then
                link_type="Static"
                libs="(none)"
            elif echo "$ldd_output" | grep -q "not a dynamic executable"; then
                link_type="Static"
                libs="(none)"
            else
                link_type="Dynamic"
                # Extract library names
                libs=$(echo "$ldd_output" | grep "=>" | awk '{print $1}' | tr '\n' ', ' | sed 's/,$//')
                if [ -z "$libs" ]; then
                    libs=$(echo "$ldd_output" | head -3 | tr '\n' ' ')
                fi
            fi

            printf "%-25s | %12s | %8s | %15s | %s\n" "$version" "$size" "$kb" "$link_type" "$libs"
        else
            printf "%-25s | %12s | %8s | %15s | %s\n" "$version" "N/A" "N/A" "Not Built" "N/A"
        fi
    fi
done

echo ""
echo "=== Detailed Analysis ==="
echo ""

# For each built version, show detailed info
for dir in v*-*/; do
    if [ -d "$dir" ]; then
        version=$(basename "$dir")
        binary="$dir/nano_ssh_server"

        if [ -f "$binary" ]; then
            echo "---"
            echo "Version: $version"
            echo "File info:"
            file "$binary" | sed 's/^/  /'
            echo "Dependencies:"
            ldd "$binary" 2>&1 | sed 's/^/  /'
            echo ""
        fi
    fi
done
