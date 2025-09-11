#!/bin/bash

# Image Processing Client - Setup Script
# This script helps you quickly set up the development environment

echo "================================================================"
echo "         IMAGE PROCESSING CLIENT - SETUP SCRIPT"
echo "================================================================"
echo ""

# Detect the Linux distribution
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$NAME
    VER=$VERSION_ID
else
    echo "Cannot detect OS version"
    exit 1
fi

echo "Detected OS: $OS"
echo ""

# Function to install dependencies based on distribution
install_dependencies() {
    echo "Installing dependencies..."
    
    if [[ "$OS" == *"Ubuntu"* ]] || [[ "$OS" == *"Debian"* ]]; then
        sudo apt update
        sudo apt install -y build-essential pkg-config libgtk-4-dev git libjson-c-dev
    elif [[ "$OS" == *"Fedora"* ]]; then
        sudo dnf install -y gcc gtk4-devel pkg-config git make json-c-devel
    elif [[ "$OS" == *"Arch"* ]]; then
        sudo pacman -Syu --noconfirm base-devel gtk4 pkg-config git json-c
    else
        echo "Unsupported distribution. Please install manually:"
        echo "  - GCC compiler"
        echo "  - GTK4 development libraries"
        echo "  - pkg-config"
        echo "  - Make"
        exit 1
    fi
    
    echo "Dependencies installed successfully!"
}

# Function to create project structure
create_structure() {
    echo "Creating project structure..."
    
    # Create directories
    mkdir -p src assets
    
    echo "Project structure created!"
}

# Function to verify GTK4 installation
verify_gtk4() {
    echo "Verifying GTK4 installation..."
    
    if pkg-config --exists gtk4; then
        GTK_VERSION=$(pkg-config --modversion gtk4)
        echo "✓ GTK4 version $GTK_VERSION is installed"
    else
        echo "✗ GTK4 is not properly installed"
        echo "Please run: $0 --install-deps"
        exit 1
    fi
}

# Function to build the project
build_project() {
    echo "Building the project..."
    
    if [ -f Makefile ]; then
        make clean
        make setup
        make
        
        if [ -f image-client ]; then
            echo "✓ Build successful!"
            echo ""
            echo "You can now run the application with:"
            echo "  ./image-client"
            echo ""
            echo "Or use:"
            echo "  make run"
        else
            echo "✗ Build failed. Please check for errors above."
            exit 1
        fi
    else
        echo "Makefile not found. Please ensure all files are present."
        exit 1
    fi
}

# Function to show help
show_help() {
    echo "Usage: $0 [OPTION]"
    echo ""
    echo "Options:"
    echo "  --install-deps    Install system dependencies"
    echo "  --verify          Verify GTK4 installation"
    echo "  --build           Build the project"
    echo "  --full            Run full setup (install, verify, build)"
    echo "  --help            Show this help message"
    echo ""
    echo "Example:"
    echo "  $0 --full         # Complete setup for new installation"
}

# Main script logic
case "${1:-}" in
    --install-deps)
        install_dependencies
        ;;
    --verify)
        verify_gtk4
        ;;
    --build)
        verify_gtk4
        build_project
        ;;
    --full)
        install_dependencies
        echo ""
        create_structure
        echo ""
        verify_gtk4
        echo ""
        build_project
        ;;
    --help)
        show_help
        ;;
    *)
        echo "Quick Setup for Image Processing Client"
        echo ""
        echo "Choose an option:"
        echo "1) Full setup (install deps + build)"
        echo "2) Install dependencies only"
        echo "3) Build project only"
        echo "4) Verify GTK4 installation"
        echo "5) Exit"
        echo ""
        read -p "Enter choice [1-5]: " choice
        
        case $choice in
            1)
                $0 --full
                ;;
            2)
                $0 --install-deps
                ;;
            3)
                $0 --build
                ;;
            4)
                $0 --verify
                ;;
            5)
                echo "Exiting..."
                exit 0
                ;;
            *)
                echo "Invalid choice"
                exit 1
                ;;
        esac
        ;;
esac

echo ""
echo "================================================================"
echo "                    SETUP COMPLETE"
echo "================================================================"