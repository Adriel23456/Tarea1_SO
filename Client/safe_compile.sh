#!/bin/bash

# Safe compilation script - Preserves existing configuration files

echo "================================"
echo "  SAFE COMPILATION"
echo "================================"
echo ""

# Create assets directory if it doesn't exist
if [ ! -d "assets" ]; then
    mkdir -p assets
    echo "✓ Created assets directory"
fi

# Check and preserve existing configuration files
if [ -f "assets/connection.json" ]; then
    echo "✓ Existing connection.json preserved"
else
    echo "Creating default connection.json..."
    cat > assets/connection.json << 'EOF'
{
  "server": {
    "host": "localhost",
    "port": 1717,
    "protocol": "http"
  },
  "client": {
    "timeout": 30,
    "max_retries": 3
  }
}
EOF
    echo "✓ Created default connection.json"
fi

if [ -f "assets/credits.txt" ]; then
    echo "✓ Existing credits.txt preserved"
else
    echo "Creating default credits.txt..."
    cat > assets/credits.txt << 'EOF'
IMAGE PROCESSING CLIENT
=======================

Version 1.0.0

Developed for Systems Operations Course

© 2024
EOF
    echo "✓ Created default credits.txt"
fi

echo ""
echo "Compiling application..."
echo ""

# Clean object files only (not the binary)
rm -f src/*.o

# Compile
gcc -o image-client \
    src/main.c \
    src/gui.c \
    src/dialogs.c \
    `pkg-config --cflags --libs gtk4` \
    -Wall -Wextra -g

if [ $? -eq 0 ]; then
    echo ""
    echo "================================"
    echo "  ✓ BUILD SUCCESSFUL!"
    echo "================================"
    echo ""
    echo "Your configuration files in assets/ are preserved."
    echo "Run the application with: ./image-client"
else
    echo ""
    echo "✗ Build failed"
    exit 1
fi