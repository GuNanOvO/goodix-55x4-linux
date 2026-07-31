# Goodix 55x4 Fingerprint Driver for Linux

Linux driver for Goodix GF3208/GF3268 (55x4 series) fingerprint sensors. USB `27c6:55a4`, `27c6:55b4`.

Built on top of [jedbillyb/libfprint](https://github.com/jedbillyb/libfprint/tree/goodix-55b4-fixes) (PR [TheWeirdDev/libfprint#3](https://github.com/TheWeirdDev/libfprint/pull/3)). Tested on Debian forky/sid and Ubuntu 22.04 / 24.04.

[中文文档](docs/README_CN.md) · [Build Guide](docs/BUILDING.md) · [Technical Details](docs/TECHNICAL.md) · [Security](docs/SECURITY.md) · [RE Notes](docs/RE_NOTES.md)

## Supported Hardware

| USB ID | Chip | Status |
|--------|------|--------|
| `27c6:55a4` | GF3208 / GF3268 | ✅ Tested |
| `27c6:55b4` | GF3268 | ✅ Supported |
| `27c6:5584` | GF3268 | 🔧 Extendable |

## Features

- **Auto PSK provisioning** — detects Windows-provisioned sensors and writes the community key automatically
- **Firmware prefix matching** — accepts any `GF32xx_RTSEC_APP_*` firmware version
- **TLS-PSK cipher fix** — works with OpenSSL 3.x
- **Dual-boot safe** — optional systemd service auto-calibrates PSK when switching between Windows/Linux

## Quick Install

### Option A: Pre-built .deb (Ubuntu/Debian)

```bash
wget https://github.com/GuNanOvO/goodix-55x4-linux/releases/latest/download/goodix-gf32xx-driver_amd64.deb
sudo dpkg -i goodix-gf32xx-driver_amd64.deb
sudo apt install --fix-broken -y
```

### Option B: Build from source

```bash
git clone https://github.com/GuNanOvO/goodix-55x4-linux.git
cd goodix-55x4-linux
sudo ./tools/install.sh
```

### After install

```bash
sudo apt install -y fprintd libpam-fprintd

# Enroll
fprintd-enroll -f right-index-finger $USER

# Verify
fprintd-verify $USER

# Enable system auth (sudo/login)
sudo pam-auth-update   # check "Fingerprint authentication"
```

## First-time firmware flash (one-time)

If your sensor was previously used with Windows, you may need to flash community firmware once:

```bash
git clone --recurse-submodules https://github.com/mpi3d/goodix-fp-dump.git
cd goodix-fp-dump
python3 -m venv .venv && source .venv/bin/activate
pip install pyusb crcmod
sudo systemctl stop fprintd
python3 -c "import driver_55x4; driver_55x4.main(0x55a4)"
```

## Modifications

Forked from `TheWeirdDev/libfprint` (`55b4-experimental`), incorporating fixes from:

- [PR #3](https://github.com/TheWeirdDev/libfprint/pull/3) (jedbillyb): PSK reprovisioning, firmware prefix match, OpenSSL PSK ciphers, payload size fix
- Additional: image routing fix, udev rules, PSK auto-fix service, CI pipeline

Full details in [TECHNICAL.md](docs/TECHNICAL.md) and [RE_NOTES.md](docs/RE_NOTES.md).

## License

LGPL-2.1

## Acknowledgments

This project stands on the work of:

- [TheWeirdDev/libfprint](https://github.com/TheWeirdDev/libfprint) — original goodixtls driver
- [jedbillyb/libfprint](https://github.com/jedbillyb/libfprint/tree/goodix-55b4-fixes) — PSK reprovisioning, OpenSSL fixes, firmware matching
- [mpi3d/goodix-fp-dump](https://github.com/mpi3d/goodix-fp-dump) — firmware flashing & PSK provisioning tool
- [goodix-fp-linux-dev](https://github.com/goodix-fp-linux-dev) — community firmware maintenance
