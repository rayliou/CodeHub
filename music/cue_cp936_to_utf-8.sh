#!/bin/bash

# Usage: ./cue_cp936_to_utf-8.sh [directory]
# If no directory is provided, use current directory (.)

ROOT_DIR="${1:-.}"
if [ ! -d "$ROOT_DIR" ]; then
    echo "Error: Directory '$ROOT_DIR' does not exist"
    exit 1
fi
is_utf8() {
    local file="$1"
    # Use file command to check encoding, grep for UTF-8
    file -bi "$file" | grep -q "utf-8"
    return $?
}

# Function to convert file from CP936 to UTF-8
convert_to_utf8() {
    local file="$1"
    local temp_file=$(mktemp)
    
    if iconv -f cp936 -t utf-8 "$file" > "$temp_file" 2>/dev/null; then
        mv "$temp_file" "$file"
        echo "Converted: $file (CP936 -> UTF-8)"
        return 0
    else
        rm -f "$temp_file"
        echo "Error: Failed to convert $file"
        return 1
    fi
}

# Find all .cue files in the specified directory
echo "Searching for .cue files in: $ROOT_DIR"
echo "----------------------------------------"

# Use find to locate .cue files and process each one
find "$ROOT_DIR" -name "*.cue" -type f | while read -r cue_file; do
    if is_utf8 "$cue_file"; then
        echo "Already UTF-8: $cue_file"
    else
        convert_to_utf8 "$cue_file"
    fi
done

echo "----------------------------------------"
echo "Processing complete!"
