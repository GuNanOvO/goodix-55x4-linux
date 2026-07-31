#!/bin/bash
set -e

echo "=== Goodix GF32xx Fingerprint Driver Installer ==="
echo ""

# Detect OS
if [ -f /etc/debian_version ]; then
    DISTRO="debian"
elif [ -f /etc/arch-release ]; then
    DISTRO="arch"
else
    echo "Unsupported distribution. Manual install required."
    exit 1
fi

echo "Detected: $DISTRO"
echo ""

# Install deps
echo "[1/4] Installing dependencies..."
if [ "$DISTRO" = "debian" ]; then
    sudo apt install -y meson ninja-build libfprint-2-dev libglib2.0-dev \
        libgusb-dev libnss3-dev libssl-dev libcairo2-dev gobject-introspection \
        libgirepository1.0-dev libopencv-dev pkg-config
elif [ "$DISTRO" = "arch" ]; then
    sudo pacman -S --needed meson ninja libfprint glib2 libgusb nss openssl \
        cairo gobject-introspection opencv pkgconf
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(dirname "$SCRIPT_DIR")"

# Build driver
echo "[2/4] Building driver..."
cd "$REPO_DIR/libfprint"
meson setup builddir --prefix=/usr -Ddrivers=goodixtls55x4 -Dudev_rules=disabled
ninja -C builddir

# Install driver
echo "[3/4] Installing driver..."
sudo ninja -C builddir install
sudo ldconfig

# Install udev rule
echo "[4/4] Installing udev rule..."
sudo cp "$SCRIPT_DIR/99-goodix-fp.rules" /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger

# Install PSK auto-fix service
sudo cp "$SCRIPT_DIR/goodix-psk-autofix" /usr/local/bin/
sudo chmod +x /usr/local/bin/goodix-psk-autofix
sudo cp "$SCRIPT_DIR/goodix-psk-autofix.service" /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable goodix-psk-autofix.service

echo ""
echo "=== Installation complete ==="
echo ""
echo "Now run: fprintd-enroll right-index-finger"
