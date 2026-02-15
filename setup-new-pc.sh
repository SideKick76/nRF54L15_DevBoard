#!/bin/bash
# Setup script for nRF54L15 DevBoard development environment
# Run this in WSL2 on your new PC: bash setup-new-pc.sh

set -e

echo "=== nRF54L15 DevBoard - New PC Setup ==="
echo ""

# 1. Update system
echo "[1/6] Updating system packages..."
sudo apt update && sudo apt upgrade -y

# 2. Install dependencies
echo "[2/6] Installing dependencies..."
sudo apt install -y git curl build-essential

# 3. Install Node.js
echo "[3/6] Installing Node.js..."
if ! command -v node &> /dev/null; then
    curl -fsSL https://deb.nodesource.com/setup_22.x | sudo -E bash -
    sudo apt install -y nodejs
else
    echo "Node.js already installed: $(node --version)"
fi

# 4. Install Claude Code
echo "[4/6] Installing Claude Code..."
sudo npm install -g @anthropic-ai/claude-code

# 5. Setup SSH key for GitHub
echo "[5/6] Setting up SSH key..."
if [ ! -f ~/.ssh/id_rsa ]; then
    ssh-keygen -t rsa -b 4096 -f ~/.ssh/id_rsa -N ""
    echo ""
    echo "=========================================="
    echo "  Add this SSH key to your GitHub account:"
    echo "  https://github.com/settings/ssh/new"
    echo "=========================================="
    echo ""
    cat ~/.ssh/id_rsa.pub
    echo ""
    read -p "Press Enter after adding the key to GitHub..."
else
    echo "SSH key already exists"
fi

# Test GitHub connection
echo "Testing GitHub connection..."
ssh -T git@github.com 2>&1 || true

# 6. Clone and setup project
echo "[6/6] Cloning project..."
cd ~
if [ ! -d "nRF54L15_DevBoard" ]; then
    git clone git@github.com:SideKick76/nRF54L15_DevBoard.git
else
    echo "Project already cloned"
fi

cd ~/nRF54L15_DevBoard

# Build Docker image
echo ""
echo "=== Building Docker image (this takes ~10 min) ==="
docker build -t nrf54-dev .
docker-compose up -d

echo ""
echo "=========================================="
echo "  Setup complete!"
echo ""
echo "  To start working:"
echo "    cd ~/nRF54L15_DevBoard"
echo "    claude"
echo ""
echo "  Don't forget to attach J-Link USB:"
echo "    (PowerShell Admin on Windows)"
echo "    usbipd list"
echo "    usbipd bind --busid <BUSID>"
echo "    usbipd attach --wsl --busid <BUSID>"
echo "=========================================="
