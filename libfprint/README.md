> ### Personal fork, not upstream libfprint
>
> This is my own fork of [TheWeirdDev/libfprint](https://github.com/TheWeirdDev/libfprint),
> which is itself a fork of [libfprint](https://gitlab.freedesktop.org/libfprint/libfprint)
> carrying driver work for Goodix readers. My changes get the Goodix `27c6:55b4` reader
> working on Linux (Void, in my case), including tighter match gating after a false accept
> and keeping the TLS session warm.
>
> The patches live on the `goodix-55b4-fixes` branch, which is the default branch here.
> This is far behind freedesktop libfprint and is not packaged, supported, or submitted
> upstream. If you want libfprint itself, go to
> [gitlab.freedesktop.org/libfprint/libfprint](https://gitlab.freedesktop.org/libfprint/libfprint).
>
> Upstream's README follows.

<div align="center">

# LibFPrint

*LibFPrint is part of the **[FPrint][Website]** project.*

<br/>

[![Button Website]][Website]
[![Button Documentation]][Documentation]

[![Button Supported]][Supported]
[![Button Unsupported]][Unsupported]

[![Button Contribute]][Contribute]
[![Button Contributors]][Contributors]

</div>

---

# This fork: Goodix 27c6:55b4 support

This is a fork of libfprint that adds a working driver for the **Goodix
`27c6:55b4`** fingerprint reader (reports its firmware family as
`GF3268_RTSEC_APP_*`), used on some recent laptops and unsupported by upstream
libfprint. It is based on [TheWeirdDev/libfprint](https://github.com/TheWeirdDev/libfprint)'s
goodixtls work, with the fixes needed to make the 55b4 actually enroll, verify,
and drive sudo / TTY login / swaylock unlock on Linux.

The driver lives in `libfprint/drivers/goodixtls/goodix55x4.c`.

## What this fork changes

- **Firmware-family accept** for the 55b4 (`GF3268_RTSEC_APP_*`).
- **PSK (re)provisioning**: writes the known white-box PSK when the sensor's PSK
  doesn't match, so the all-zeros TLS-PSK handshake succeeds. Corrected the
  `goodix_send_preset_psk_write` wire framing and enabled PSK ciphers at
  `SECLEVEL=0` in the goodix TLS server.
- **Tuned matching** for this sensor: `nr_enroll_stages` and `bz3_threshold`
  (both near the top of `goodix55x4.c`).

## Build & install

```sh
meson setup builddir            # or: meson setup --reconfigure builddir
meson compile -C builddir
sudo meson install -C "$PWD/builddir"   # absolute path; relative paths fail
sudo ldconfig
sudo pkill -9 -x fprintd         # restart the daemon onto the new lib
```

This overlays the system `libfprint-2.so.2.0.0`. On distros that package
libfprint, put the package on hold so an update doesn't overwrite it (on Void:
`xbps-pkgdb -m hold libfprint`). Re-running install + `ldconfig` restores it if
it ever gets clobbered.

## Enroll & verify

```sh
sudo fprintd-enroll jed          # ~nr_enroll_stages presses; vary angle/pressure
fprintd-verify jed               # expect: verify-match
```

Enrolling via `sudo` is needed because the enroll polkit action defaults to
`auth_self_keep` and a bare Wayland/TTY session has no polkit agent; verify and
PAM login work as your user since fprintd runs as root.

### Tuning matching

In `goodix55x4.c`:

- **Won't match at a different angle/position** → raise `nr_enroll_stages` for
  more enrollment coverage, and re-enroll pressing at varied angles/edges.
- **Fails a marginal press but matches on re-press** → lower `bz3_threshold`
  (verify-time, no re-enroll). Lower = more lenient = slightly higher
  false-accept risk.

### If verify suddenly fails

`verify-disconnected` / instant "Wrong" usually means a stale TLS session — run
`sudo pkill -9 -x fprintd` to force a fresh cold handshake. If it persists, the
sensor PSK was likely rewritten (e.g. by a Windows Hello boot, which can't share
the sensor with Linux); re-enroll to re-provision it.

---

## History

**LibFPrint** was originally developed as part of an
academic project at the **[University Of Manchester]**.

It aimed to hide the differences between consumer
fingerprint scanners and provide a single uniform
API to application developers.

## Goal

The ultimate goal of the **FPrint** project is to make
fingerprint scanners widely and easily usable under
common Linux environments.

## License

`Section 6` of the license states that for compiled works that use
this library, such works must include **LibFPrint** copyright notices
alongside the copyright notices for the other parts of the work.

**LibFPrint** includes code from **NIST's** **[NBIS]** software distribution.

We include **Bozorth3** from the **[US Export Controlled]**
distribution, which we have determined to be fine
being shipped in an open source project.

<br/>

<div align="right">

[![Badge License]][License]

</div>


<!----------------------------------------------------------------------------->

[Documentation]: https://fprint.freedesktop.org/libfprint-dev/
[Contributors]: https://gitlab.freedesktop.org/libfprint/libfprint/-/graphs/master
[Unsupported]: https://gitlab.freedesktop.org/libfprint/wiki/-/wikis/Unsupported-Devices
[Supported]: https://fprint.freedesktop.org/supported-devices.html
[Website]: https://fprint.freedesktop.org/

[Contribute]: ./HACKING.md
[License]: ./COPYING

[University Of Manchester]: https://www.manchester.ac.uk/
[US Export Controlled]: https://fprint.freedesktop.org/us-export-control.html
[NBIS]: http://fingerprint.nist.gov/NBIS/index.html


<!---------------------------------[ Badges ]---------------------------------->

[Badge License]: https://img.shields.io/badge/License-LGPL2.1-015d93.svg?style=for-the-badge&labelColor=blue


<!---------------------------------[ Buttons ]--------------------------------->

[Button Documentation]: https://img.shields.io/badge/Documentation-04ACE6?style=for-the-badge&logoColor=white&logo=BookStack
[Button Contributors]: https://img.shields.io/badge/Contributors-FF4F8B?style=for-the-badge&logoColor=white&logo=ActiGraph
[Button Unsupported]: https://img.shields.io/badge/Unsupported_Devices-EF2D5E?style=for-the-badge&logoColor=white&logo=AdBlock
[Button Contribute]: https://img.shields.io/badge/Contribute-66459B?style=for-the-badge&logoColor=white&logo=Git
[Button Supported]: https://img.shields.io/badge/Supported_Devices-428813?style=for-the-badge&logoColor=white&logo=AdGuard
[Button Website]: https://img.shields.io/badge/Homepage-3B80AE?style=for-the-badge&logoColor=white&logo=freedesktopDotOrg
