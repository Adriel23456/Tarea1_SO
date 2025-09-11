#!/bin/bash
set -e

echo "=== Server setup ==="

if [ -f /etc/os-release ]; then . /etc/os-release; fi

case "$ID" in
    ubuntu|debian)
        sudo apt update
        sudo apt install -y build-essential uuid-dev
        ;;
    fedora)
        sudo dnf install -y gcc libuuid-devel
        ;;
    arch)
        sudo pacman -Syu --noconfirm base-devel util-linux-libs
        ;;
    *)
        echo "Install manually: gcc, libuuid-dev"
        ;;
esac

mkdir -p assets assets/incoming
[ -f assets/log.txt ] || echo "== Image Server Log ==" > assets/log.txt

make
echo "Run with: ./image-server"