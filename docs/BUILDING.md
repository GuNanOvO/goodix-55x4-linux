# Building from Source

## Prerequisites

### Debian / Ubuntu

```bash
sudo apt install -y meson ninja-build pkg-config \
  libfprint-2-dev libglib2.0-dev libgusb-dev \
  libnss3-dev libssl-dev libcairo2-dev \
  gobject-introspection libgirepository1.0-dev \
  libopencv-dev gtk-doc-tools
```

### Arch Linux

```bash
sudo pacman -S --needed meson ninja libfprint glib2 libgusb \
  nss openssl cairo gobject-introspection opencv pkgconf gtk-doc
```

## Build

```bash
git clone https://github.com/GuNanOvO/goodix-55x4-linux.git
cd goodix-55x4-linux/libfprint

meson setup builddir --prefix=/usr -Ddrivers=goodixtls55x4 -Dudev_rules=disabled -Ddoc=false
ninja -C builddir
sudo ninja -C builddir install
sudo ldconfig
```

## Install Tools

```bash
cd ..
sudo ./tools/install.sh
```

This installs:

- udev rules (`/etc/udev/rules.d/99-goodix-fp.rules`) — USB device permissions
- PSK auto-fix service (`/etc/systemd/system/goodix-psk-autofix.service`) — dual-boot PSK calibration

## Verify

```bash
sudo systemctl restart fprintd
fprintd-list $USER
# Expected: "Goodix TLS Fingerprint Sensor 55X4"
```

## Uninstall

```bash
sudo ./tools/uninstall.sh
```
