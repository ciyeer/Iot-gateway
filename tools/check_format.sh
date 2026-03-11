#!/bin/bash

# Check if clang-format is installed
if ! command -v clang-format &> /dev/null; then
    echo "Error: clang-format is not installed."
    echo "Please install it using: sudo apt-get install clang-format (Ubuntu/Debian) or brew install clang-format (macOS)"
    exit 1
fi

# Set the source directory (adjust if needed)
SRC_DIR="IotEdgeGateway"

# Find all C/C++ files
FILES=$(find "$SRC_DIR" -name "*.cpp" -o -name "*.hpp" -o -name "*.c" -o -name "*.h")

# Check format
FAILED=0
for FILE in $FILES; do
    # Use -output-replacements-xml to check for changes without modifying files
    # If there are replacements, grep will find "replacement"
    if clang-format -output-replacements-xml "$FILE" | grep -q "<replacement "; then
        echo "Format error: $FILE"
        FAILED=1
    fi
done

if [ $FAILED -eq 1 ]; then
    echo "--------------------------------------------------"
    echo "Code style check failed!"
    echo "Run './tools/format_code.sh' to fix the issues automatically."
    echo "--------------------------------------------------"
    exit 1
else
    echo "Code style check passed!"
    exit 0
fi
