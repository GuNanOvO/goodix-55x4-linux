# Security Notes

## PSK (Pre-Shared Key)

The driver uses a **hardcoded community PSK** (`PMK_HASH` / `PSK_WHITE_BOX`) to communicate with the fingerprint sensor. This is a design constraint of the open-source driver — the sensor requires a pre-shared key to establish the TLS session used for image transfer.

### Implications

1. **This key is public.** Anyone who reads the driver source has access to it. The sensor's TLS channel is a **transport encryption** layer, not an authentication or access-control layer. It prevents passive eavesdropping on the USB bus but does not authenticate the host.

2. **Physical access.** An attacker with physical access to the USB bus could:
   - Read the community PSK from the open-source driver
   - Man-in-the-middle the TLS connection
   - Capture or replay fingerprint images

   This threat exists regardless of the PSK mechanism and is inherent to USB bus access.

3. **Dual-boot PSK changes.** When switching between Windows and Linux, each OS writes its own PSK. The community driver detects mismatches and reprovisions the community key. This write is a one-shot USB command (`0xe0`) that takes ~500ms and does not touch enrolled fingerprint data.

### Mitigations

- Keep your system's USB ports physically secure
- Use full-disk encryption to protect fingerprint templates at rest (`/var/lib/fprint/`)
- The PSK is a transport key, not a storage key — fingerprint matching data is not exposed through it

## Fingerprint Templates

Enrolled fingerprint data is stored:

- **On Linux:** in `/var/lib/fprint/` (fprintd). These files are owner-read-only (`0600`).
- **On the sensor MCU:** the community firmware stores up to 10 fingerprint templates. The driver communicates with the sensor to enroll, verify, and delete templates.

Template storage on the MCU is managed through the same TLS-PSK encrypted channel. No plaintext fingerprint image is stored on disk — only derived feature vectors.

## OpenSSL TLS-PSK Context

The TLS-PSK connection is established over a **local socketpair** within the fprintd process:

```
fingerprint sensor ←──USB──→ goodixtls ←──socketpair──→ OpenSSL (in-process)
```

- `@SECLEVEL=0` is set on the sensor's TLS context only. This does not affect system-wide OpenSSL security levels.
- The TLS context is destroyed when fprintd closes the device.
- The PSK cipher configuration (`PSK:@SECLEVEL=0`) is not propagated to any other OpenSSL context.

## udev Permissions

The udev rule grants `0666` access to Goodix USB devices (`27c6:55a4`, `27c6:55b4`). This is necessary for fprintd (running as the user's session) to access the sensor. The alternative would require a system-level daemon with USB access delegation, which fprintd provides natively.

## Reporting

Security concerns should be reported via GitHub Issues on [GuNanOvO/goodix-55x4-linux](https://github.com/GuNanOvO/goodix-55x4-linux/issues).

## Firmware Write Exposure

The driver implements PSK write (`GOODIX_CMD_PRESET_PSK_WRITE`, `0xe0`) and firmware update commands (`GOODIX_CMD_WRITE_FIRMWARE`, `0xf0`; `GOODIX_CMD_MCU_ERASE_APP`, `0xa4`) for device provisioning. These are necessary for the driver's core functionality (auto-PSK provisioning, initial firmware setup).

Risk mitigation: USB device access is restricted to `plugdev` group via udev rules (`MODE="0660", GROUP="plugdev"`), not world-writable. Only logged-in users with a physical session can interact with the device. Firmware flashing is further gated by the separate `goodix-fp-dump` tool which requires explicit user confirmation.

A physically-present attacker with USB bus access could still modify the device firmware. This is an inherent risk of USB-connected peripherals and is not specific to this driver.

## PSK Race Condition

PSK check-then-write is performed as sequential SSM states (`ACTIVATE_CHECK_PSK` → `ACTIVATE_WRITE_PSK`) without a device-level lock. A process with USB access could theoretically inject a PSK write between the check and the write (~milliseconds). Mitigation: USB device access requires `plugdev` group membership (C-1), and the write state re-verifies the PSK hash after writing. An attacker would need both group membership and precise timing during device activation.
