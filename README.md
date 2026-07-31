# Goodix 55x4 Fingerprint Driver for Linux

[![CI](https://github.com/GuNanOvO/goodix-55x4-linux/actions/workflows/build.yml/badge.svg)](https://github.com/GuNanOvO/goodix-55x4-linux/actions/workflows/build.yml)

Linux driver for the Goodix 55x4 series fingerprint sensors — the `27c6:55a4` / `27c6:55b4` USB modules found in many Lenovo ThinkBook, ThinkPad, IdeaPad, and DIY modules.

Built on the work of [TheWeirdDev/libfprint](https://github.com/TheWeirdDev/libfprint), [jedbillyb/libfprint](https://github.com/jedbillyb/libfprint/tree/goodix-55b4-fixes), and [mpi3d/goodix-fp-dump](https://github.com/mpi3d/goodix-fp-dump).

[中文文档](docs/README_CN.md) · [Build Guide](docs/BUILDING.md) · [Technical Details](docs/TECHNICAL.md) · [Security](docs/SECURITY.md) · [RE Notes](docs/RE_NOTES.md)

---

## The Problem

Goodix does not provide Linux drivers. Their sensors ship firmware configured for Windows-only use (Intel SGX-based PSK provisioning, Windows Biometric Driver Interface). As of 2026, **no upstream libfprint release supports the 55x4 family** — `27c6:55a4` is explicitly listed as "known but unsupported" in Debian/Ubuntu's packaged libfprint.

## The Solution

This driver replaces the system libfprint with a patched build that:

| Capability | Detail |
|------------|--------|
| **Hardware detection** | Recognizes sensors by USB ID (`27c6:55a4`, `27c6:55b4`) |
| **Firmware acceptance** | Accepts any `GF32xx_RTSEC_APP_*` version (10041, 10056, 10062, …) |
| **PSK auto-provision** | Detects Windows-provisioned sensors and writes the community key on first use. No manual PSK step needed. |
| **TLS cipher fix** | Works with OpenSSL 3.x (security level relaxation scoped to the sensor socketpair) |
| **Dual-boot safe** | Optional systemd service auto-calibrates PSK when switching from Windows to Linux |
| **Warm TLS sessions** | Reuses TLS session across verify retries for instant re-authentication |

## Supported Hardware

| USB ID | Chip / Firmware | Tested On |
|--------|-----------------|-----------|
| `27c6:55a4` | GF3208 / GF3268, `GF3268_RTSEC_APP_10041` | Debian forky/sid, Lenovo ThinkBook 16 G4+ IAP |
| `27c6:55b4` | GF3268, `GF3268_RTSEC_APP_10056` | Void Linux, Lenovo IdeaPad Flex 5 |
| `27c6:5584` | GF3268 | Awaiting confirmation |

## Install

### Quick: Pre-built .deb

```bash
wget https://github.com/GuNanOvO/goodix-55x4-linux/releases/latest/download/goodix-gf32xx-driver_0.1.0_ubuntu-24.04_amd64.deb
sudo dpkg -i goodix-gf32xx-driver_*.deb
sudo apt install --fix-broken -y   # pull missing deps if any
```

Choose `ubuntu-24.04` or `ubuntu-22.04` depending on your base system.

### From Source

```bash
git clone https://github.com/GuNanOvO/goodix-55x4-linux.git
cd goodix-55x4-linux
sudo ./tools/install.sh
```

The install script handles dependencies, builds the driver, installs udev rules, and enables the PSK auto-fix service.

See [BUILDING.md](docs/BUILDING.md) for manual steps and Arch Linux instructions.

### One-time: Firmware Flash

If your sensor has never been touched by this driver before, you may need to flash community firmware **once**. This is required because the sensor's on-chip firmware dictates the expected PSK and protocol version.

```bash
git clone --recurse-submodules https://github.com/mpi3d/goodix-fp-dump.git
cd goodix-fp-dump
python3 -m venv .venv && source .venv/bin/activate
pip install pyusb crcmod python-periphery spidev
sudo systemctl stop fprintd
python3 -c "import driver_55x4; driver_55x4.main(0x55a4)"
sudo systemctl start fprintd
```

The tool backs up your current firmware before flashing. After this one-time step, the PSK auto-provision handles all future scenarios.

## Usage

```bash
# Install fprintd if missing
sudo apt install -y fprintd libpam-fprintd

# Enroll a finger
fprintd-enroll -f right-index-finger $USER

# Verify
fprintd-verify $USER
#   verify-match     → correct finger
#   verify-no-match  → wrong finger (expected)

# Enable system-wide fingerprint auth (sudo / login / lockscreen)
sudo pam-auth-update
# Check "Fingerprint authentication" → OK
```

## What Gets Installed

| File | Purpose |
|------|---------|
| `/usr/lib/x86_64-linux-gnu/libfprint-2.so*` | Patched libfprint with goodixtls55x4 driver |
| `/etc/udev/rules.d/99-goodix-fp.rules` | USB device permissions (`0666` for `27c6:55a4`/`55b4`) |
| `/usr/local/bin/goodix-psk-autofix` | PSK detection and auto-repair script |
| `/etc/systemd/system/goodix-psk-autofix.service` | Runs before `fprintd.service`, calibrates PSK on boot |

## Dual-Boot

Switching between Windows and Linux rewrites the PSK. This driver handles it automatically:

```
Boot Linux
  → goodix-psk-autofix.service runs
  → reads sensor PSK hash
  → mismatch? writes community key (~500ms)
  → fprintd starts with valid PSK
```

No user intervention. Windows continues to work normally — it reprovisions its own PSK when it boots.

## Troubleshooting

### "failed to claim device: The device has already been opened"

Another process has the device locked. Usually fprintd itself.

```bash
sudo systemctl restart fprintd
```

### fprintd-list shows no device

Check USB visibility:

```bash
lsusb -d 27c6:
# Expected: "Shenzhen Goodix Technology Co.,Ltd. Goodix FingerPrint Device"
```

If the device is visible but fprintd doesn't see it, the udev rules may not be loaded:

```bash
sudo udevadm control --reload-rules && sudo udevadm trigger
```

### Enroll hangs / no finger detection

Some hardware revisions don't fire the finger-down interrupt reliably. Workaround: run the [goodix-fp-dump](https://github.com/mpi3d/goodix-fp-dump) `run_55a4.py` script to verify the sensor can capture images. If Python works but fprintd doesn't, the sensor may need a different FDT threshold — open an issue.

### "Device reported an error: Cannot run while suspended"

The sensor overheated or timed out. Wait 30 seconds and retry.

## Modifications

This driver is forked from `TheWeirdDev/libfprint` (`55b4-experimental`) and incorporates 14 commits from [PR #3](https://github.com/TheWeirdDev/libfprint/pull/3) (jedbillyb):

- PSK auto-reprovision (write on mismatch, skip on match)
- PSK write framing fix (uninitialized struct → explicit payload)
- Firmware version prefix matching (`GF3268_RTSEC_APP_*`)
- OpenSSL 3.x PSK cipher enablement
- Payload size fixes for multiple commands
- TLS warm-session caching
- sigfm matching threshold tuning

Plus our additions: image routing fix (cmd `0xd0` acceptance), udev rules, PSK auto-fix systemd service, CI/CD pipeline.

Full technical documentation in [TECHNICAL.md](docs/TECHNICAL.md).

## Security

The community PSK is hardcoded and public. The TLS channel provides **transport encryption** (prevents passive USB bus eavesdropping) but does **not** authenticate the host. Fingerprint templates are stored at `/var/lib/fprint/` (0600) and on the sensor MCU (encrypted channel). The OpenSSL `@SECLEVEL=0` configuration is scoped to the in-process sensor socketpair and does not affect any other TLS usage on the system.

See [SECURITY.md](docs/SECURITY.md) for full details.

## License

LGPL-2.1

## Acknowledgments

This project stands on the work of:

- [TheWeirdDev/libfprint](https://github.com/TheWeirdDev/libfprint) — original goodixtls driver
- [jedbillyb/libfprint](https://github.com/jedbillyb/libfprint/tree/goodix-55b4-fixes) — PSK reprovisioning, OpenSSL fixes, firmware matching
- [mpi3d/goodix-fp-dump](https://github.com/mpi3d/goodix-fp-dump) — firmware flashing & PSK provisioning tool
- [goodix-fp-linux-dev](https://github.com/goodix-fp-linux-dev) — community firmware maintenance
