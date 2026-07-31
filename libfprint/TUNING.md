# Fingerprint match tuning (Goodix 27c6:55b4)

How to make the reader stricter or more forgiving, and how to check the change
actually took effect.

## The one thing that wastes the most time

**Verify what is loaded, not what is installed.** A whole day of tuning was
lost to changes that were built and installed correctly but never loaded: the
stock library had been kept as a backup *inside* `/usr/lib`, and it still
carries `SONAME=libfprint-2.so.2`, so `ldconfig` regenerated the symlink
pointing at the **backup** instead of the tuned build.

```sh
readlink -f /usr/lib/libfprint-2.so.2                       # must be .../libfprint-2.so.2.0.0
sudo grep -o '/usr/lib/libfprint[^ ]*' /proc/$(pgrep -x fprintd)/maps | sort -u
md5sum /usr/lib/libfprint-2.so.2.0.0 \
       builddir2/libfprint/libfprint-2.so.2.0.0             # must match
```

Never keep a library backup in a linker search path. The stock copy lives at
`/var/backups/libfprint/`, outside `/usr/lib`, for exactly this reason.

## How matching works

The sensor produces an image; `sigfm_match_score()` in
`libfprint/sigfm/sigfm.cpp` scores it against each enrolled template:

1. Find keypoints and descriptors in the image.
2. Match them against the template, keeping a match only when the best
   candidate beats the second best by the Lowe ratio (`distance_match`).
3. **Gate 1** — fewer than `min_match` surviving keypoints returns **0**
   immediately.
4. Build pairs of matched points and keep pairs whose relative distance and
   angle agree between image and template. This geometric check is what stops
   a random scatter of keypoints from passing.
5. **Gate 2** — fewer than `min_match` consistent angle pairs also returns 0.
6. Otherwise return the count of consistent pairs, which `fpi-print.c` accepts
   when `score >= bz3_threshold`.

The returned score counts *pairs*, so it grows roughly quadratically with the
number of matched keypoints. A good press blows past the threshold easily;
that is why raising or lowering `bz3_threshold` alone often changes nothing.

## The knobs

| Knob | File | Current | Direction |
|---|---|---|---|
| `min_match` | `libfprint/sigfm/sigfm.cpp` | 12 | lower = more forgiving |
| `distance_match` | `libfprint/sigfm/sigfm.cpp` | 0.74 | higher = more forgiving |
| `bz3_threshold` | `libfprint/drivers/goodixtls/goodix55x4.c` | 48 | lower = more forgiving |
| `nr_enroll_stages` | `libfprint/drivers/goodixtls/goodix55x4.c` | 24 | higher = more coverage |

`min_match` is usually the binding constraint on a marginal press, not
`bz3_threshold` — it returns 0 before the threshold is ever consulted.

All except `nr_enroll_stages` are verify-time and need **no re-enroll**.
Changing `nr_enroll_stages` **requires** a re-enroll, since it changes how many
captures make up a template.

## Which knob for which symptom

- **"Doesn't pick up my finger", fails on a marginal press** — lower
  `min_match`, raise `distance_match`. No re-enroll.
- **"Won't match at a different angle or position"** — raise
  `nr_enroll_stages` and re-enroll, deliberately varying angle, edge and
  pressure. More templates beat looser thresholds here.
- **Instant failure with no finger touch** — not a tuning problem. That is a
  stale fprintd session (`verify-disconnected`); see the self-heal in
  `swaylock-fprintd`'s `pam.c`.

## Measure before guessing

`fpi-print.c` logs the real numbers per attempt:

```sh
sudo pkill -x fprintd
sudo G_MESSAGES_DEBUG=all /usr/libexec/fprintd 2>&1 | grep --line-buffered 'sigfm score'
# in another shell: fprintd-verify
```

- scores just under the threshold (say 7–9 against 10) — ordinary tuning
- scores of **0** — a gate rejected it, or the image yields no usable
  keypoints. Lowering `bz3_threshold` cannot help; lower `min_match`, or
  re-enroll with better coverage.

## Apply a change

```sh
cd /mnt/shared/projects/libfprint
git checkout goodix-55b4-fixes          # never build from a detached HEAD
$EDITOR libfprint/sigfm/sigfm.cpp
ninja -C builddir2
sudo install -m0755 builddir2/libfprint/libfprint-2.so.2.0.0 /usr/lib/libfprint-2.so.2.0.0
sudo ldconfig
readlink -f /usr/lib/libfprint-2.so.2   # confirm before going further
sudo pkill -x fprintd
fprintd-list "$USER" >/dev/null 2>&1    # dbus reactivates the daemon
sudo grep -o '/usr/lib/libfprint[^ ]*' /proc/$(pgrep -x fprintd)/maps | sort -u
fprintd-verify
```

Use `builddir2`; the old `builddir` is configured against a dead source path.
Commit the constants — an uncommitted tuning edit is lost on the next checkout,
which has happened before.

## Security note — a false accept actually happened here

Loosening these is not free, and the failure mode is not theoretical. At
**`min_match = 6`, `distance_match = 0.82`, `bz3_threshold = 10`** an
*unenrolled* finger unlocked the machine. Do not go back to those values.

Known points on the curve, both observed on this hardware:

| Setting | Result |
|---|---|
| 15 / 0.70 / 120 | upstream-ish; rejected too many genuine presses |
| 12 / 0.74 / 48 | current compromise |
| 6 / 0.82 / 10 | **accepted the wrong finger — unsafe** |

The right value is found by measuring, not guessing: capture scores for the
enrolled finger and for an unenrolled one (see "Measure before guessing"), and
set `bz3_threshold` in the gap between the two distributions. If they overlap,
no threshold is safe and the fix is a better enrollment — more captures with
varied angle, edge and pressure — not a lower bar.

Treat the fingerprint as convenience. The password fallback is the real
security boundary; keep it strong.
