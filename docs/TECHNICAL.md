# Technical Details

## Architecture

```
┌─────────────┐    USB (Vendor Specific 0xff)
│  Goodix MCU │◄──────────────────────────────┐
│  (GF3208/   │  Bulk EP 0x01 OUT             │
│   GF3268)   │  Bulk EP 0x82 IN              │
└──────┬──────┘                               │
       │ firmware: GF3268_RTSEC_APP_10041      │
       │ PSK: community PMK_HASH               │
       │                                       │
┌──────▼──────────────────────────────────────┐
│  goodixtls driver (libfprint plugin)        │
│                                             │
│  ┌──────────┐  ┌───────────┐  ┌──────────┐ │
│  │ goodix.c │  │goodixtls.c│  │55x4.c    │ │
│  │ protocol │  │TLS proxy  │  │activation│ │
│  │ layer    │  │(OpenSSL)  │  │SSM       │ │
│  └────┬─────┘  └─────┬─────┘  └────┬─────┘ │
│       │              │              │       │
│  USB commands  TLS handshake  device init   │
│  encode/decode  via socketpair  + enroll    │
└───────┼──────────────┼──────────────┼───────┘
        │              │              │
   ┌────▼──────────────▼──────────────▼───────┐
   │              libfprint-2                 │
   └──────────────────┬──────────────────────┘
                      │
                 ┌────▼────┐
                 │ fprintd │
                 └────┬────┘
                      │
                 ┌────▼────┐
                 │   PAM   │  →  login / sudo / lockscreen
                 └─────────┘
```

## Communication Stack

The sensor uses a layered USB protocol:

```
Layer 1: USB bulk transfer (64-byte aligned)
Layer 2: Message Pack   {flags:1, length:2, checksum:1, payload}
Layer 3: Message Protocol {cmd:1, length:2, payload, checksum}
Layer 4: TLS-PSK (after handshake)
```

### Flags

| Flag | Value | Purpose |
|------|-------|---------|
| `MSG_PROTOCOL` | `0xa0` | Plain protocol commands |
| `TLS` | `0xb0` | TLS handshake data |
| `TLS_DATA` | `0xb2` | TLS encrypted payload |

### Command List

See [RE_NOTES.md](RE_NOTES.md) for the full command table.

## Modifications from Upstream

### Base: TheWeirdDev/libfprint `55b4-experimental`

The original goodixtls driver supports `27c6:55b4` with `GF3268_RTSEC_APP_10041`.

### PR #3: jedbillyb `goodix-55b4-fixes` (14 commits)

These fixes are included in our codebase. Summary:

| Fix | File | What Changed |
|-----|------|-------------|
| **Firmware prefix matching** | `goodix55x4.c` | `strcmp()` → `g_str_has_prefix("GF3268_RTSEC_APP_")`. Accepts any minor revision (10041, 10056, 10062). |
| **PSK auto-reprovision** | `goodix55x4.c` | New `ACTIVATE_WRITE_PSK` state. On activation, reads device PMK hash; if it differs from `PMK_HASH`, writes the community white-box PSK automatically. Once provisioned, subsequent writes are skipped. |
| **PSK write framing fix** | `goodix.c` | `goodix_send_preset_psk_write` had an uninitialized offset field and wrong payload size (8 vs actual). Rewritten to build payload explicitly as `flags + length + key`. |
| **OpenSSL PSK ciphers** | `goodixtls.c` | Cipher list changed from `"ALL"` to `"PSK:@SECLEVEL=0"`. Modern OpenSSL 3.x excludes PSK suites from `ALL` and rejects them at default security level. Scope limited to the in-process TLS-PSK socketpair to the sensor. |
| **Payload size fixes** | `goodix.c` | `GoodixNone payload = {}` (undefined struct) replaced with explicit `guint8 payload[2] = {0, 0}` for firmware version, TLS connection, TLS established, and OTP read commands. |
| **sigfm doctest** | `sigfm/meson.build` | `doctest` dependency marked `required: false`; test target gated on `doctest.found()`. |
| **sigfm thresholds** | `sigfm.cpp` | Tightened matching: `min_match` 5→15, Lowe's ratio 0.75→0.70. Reduces false positives. |
| **TLS warm session** | `goodix55x4.c` | Caches TLS session across verify retries (reverted in subsequent commit, available as reference). |

### Our additions

| Addition | File | Purpose |
|----------|------|---------|
| **Image routing fix** | `goodix.c` | After command `0x20` (MCU_GET_IMAGE), the device signals via `0xd0` that image data is ready via TLS channel. Accepted `0xd0` as valid reply to `0x20`. |
| **udev rules** | `tools/99-goodix-fp.rules` | Grant user access to USB devices `27c6:55a4` and `27c6:55b4`. |
| **PSK auto-fix service** | `tools/goodix-psk-autofix` | systemd service that checks PSK on boot and rewrites if Windows changed it during a prior session. |
| **Build system fixes** | `libfprint/meson.build` | Removed `openssl_dep`/`threads_dep` from direct dependency list (already in `optional_deps` via foreach). |

## Firmware

The driver expects firmware in the `GF32xx_RTSEC_APP_*` family. Tested versions:

- `GF3268_RTSEC_APP_10041` — community firmware (confirmed working)
- `GF3268_RTSEC_APP_10056` — jedbillyb tested (confirmed working)
- `GF3208_RTSEC_APP_10062` — Windows-provisioned (should work with prefix match + PSK reprovision)

Firmware flashing is handled by [goodix-fp-dump](https://github.com/mpi3d/goodix-fp-dump). Firmware images are in the [goodix-firmware](https://github.com/goodix-fp-linux-dev/goodix-firmware) repository.

## PSK Lifecycle

```
Device Powers On
     │
     ▼
┌─────────────┐    NO   ┌───────────────┐
│ PSK matches │────────▶│ Write community│
│ PMK_HASH?   │         │ white-box PSK │
└──────┬──────┘         └───────┬───────┘
       │ YES                    │
       ▼                        ▼
┌─────────────┐         ┌───────────────┐
│ Ready       │         │ PSK matches   │
│ (no write)  │         │ now → Ready   │
└─────────────┘         └───────────────┘
```

- PSK is checked on every activation (probe + enroll/verify)
- Write only occurs on mismatch (~500ms, USB command `0xe0`)
- Enrolled fingerprint templates are stored separately and unaffected
- Works transparently across Windows/Linux dual boot
