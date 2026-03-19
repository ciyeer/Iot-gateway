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

echo "Formatting code..."
for FILE in $FILES; do
    echo "Formatting: $FILE"
    clang-format -i "$FILE"
done

echo "Code formatting complete!"
