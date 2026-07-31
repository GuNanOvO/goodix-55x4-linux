#!/bin/bash
set -e

echo "=== Uninstalling Goodix GF32xx Fingerprint Driver ==="

sudo rm -f /etc/udev/rules.d/99-goodix-fp.rules
sudo rm -f /usr/local/bin/goodix-psk-autofix
sudo rm -f /etc/systemd/system/goodix-psk-autofix.service
sudo systemctl daemon-reload

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(dirname "$SCRIPT_DIR")"

cd "$REPO_DIR/libfprint/builddir" 2>/dev/null && sudo ninja uninstall 2>/dev/null || true

sudo ldconfig

echo "Uninstall complete."
