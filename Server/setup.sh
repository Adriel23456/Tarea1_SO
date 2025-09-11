#!/bin/bash

# Setup script for Image Processing Server
# This is a standalone server setup

echo "========================================="
echo "   IMAGE PROCESSING SERVER SETUP"
echo "========================================="
echo ""

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

# Check if make is installed
if ! command -v make &> /dev/null; then
    echo -e "${RED}[ERROR]${NC} make is not installed"
    echo "Installing build-essential..."
    sudo apt update
    sudo apt install -y build-essential
fi

# Create directories
echo "Creating directories..."
mkdir -p received_images/histogram
mkdir -p received_images/colors/verdes
mkdir -p received_images/colors/rojas
mkdir -p received_images/colors/azules
echo -e "${GREEN}[OK]${NC} Directories created"

# Build server
echo ""
echo "Building server..."
make clean
make

if [ -f image-server ]; then
    echo -e "${GREEN}[OK]${NC} Server built successfully!"
    echo ""
    echo "========================================="
    echo "         SETUP COMPLETE!"
    echo "========================================="
    echo ""
    echo "To run the server:"
    echo "  ./image-server        (default port 1717)"
    echo "  ./image-server 8080   (custom port)"
    echo ""
    echo "To view logs:"
    echo "  tail -f server.log"
    echo ""
    echo "Received images will be saved in:"
    echo "  ./received_images/histogram/   (for histogram processing)"
    echo "  ./received_images/colors/      (for color classification)"
    echo ""
else
    echo -e "${RED}[ERROR]${NC} Build failed!"
    echo "Check for compilation errors above"
    exit 1
fi