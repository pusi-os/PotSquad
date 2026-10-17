# Over-the-air updates, and updating from a local server

This is how a board updates itself without a cable, and how to point that at a
server on your own network instead of `potsquad.com`.

## The three files the board fetches

The transport is plain HTTP, from a base URL the build sets (default
`http://potsquad.com/`, see `include/ota_wifi.h`). Every request names the
board's own build — the PlatformIO environment, e.g. `cyd-ili9341`:

| File | What it is |
| --- | --- |
| `manifest-<env>.json` | `{"version": "1.34.0", "release_name": "...", "whats_new": ["...", "..."]}` |
| `<env>-firmware.sig` | the DER ECDSA P-256 signature of the image |
| `<env>-firmware.bin` | the app image itself |

Only `version` is acted on. The board compares it with what it is running
(`MeshMsg::versionNewer`) and offers the update only when it is newer.
`release_name` and `whats_new` (up to four lines) are shown on the UPDATE
screen. `whats_new` may be omitted.

`check()` fetches the manifest **and** the signature before it will reach the
"update available" state, so both must be present. The `.sig` must be at least
eight bytes.

## The signature is the security

The board streams the image into the spare app slot, hashing as it goes, and
installs it only when

    SHA-256("SQWOTA1\n" + env + "\n" + image)

verifies (ECDSA P-256, DER) against the public key compiled into
`include/ota_pubkey.h`. The build name is inside what is signed, so an image
signed for one board is refused by every other one. `tools/sign_firmware.py`
makes a signature with the matching **private** key — the `OTA_SIGNING_KEY` CI
secret and the owner's offline backup.

This is also why a local server is not a way around signing: you can serve the
manifest and the image and the board will offer the update, but the install
stops at *bad signature* unless the image is signed with that key. If you hold
the private key, sign locally (below). If you do not, the only images a board
will take are ones already signed for its build.

## Two ways it runs

- **The boot check.** On every boot, if UPDATE CHECK is on, the board is
  unlocked, the build has a spare slot, and a WiFi network is saved, the board
  joins it for a few seconds (`OtaWifi::bootCheck`), reads the manifest and
  signature, and notes anything newer. It *tells, never installs* — a window
  opens offering the update. Settings → SYSTEM → UPDATE CHECK turns this off.
- **The manual mode.** Settings → SYSTEM → UPDATE FIRMWARE (the UPDATE screen)
  → **UPDATE OVER WIFI**: scan, pick a network, type its password, and the
  board joins, checks, and shows LATEST vs running with INSTALL. Detection and
  the scan pause for the radio; nothing restarts unless an image is installed.
  There is also **UPDATE OVER BLUETOOTH (BETA)** on the S3 boards, where a
  browser page on a computer feeds the same `OtaCore` installer.

Everything that makes an install safe lives in `OtaCore` (`include/ota_core.h`):
the spare-slot stream, the signature check, and the ESP32 rollback (a fresh
image boots on probation and confirms itself after 30 s, or the bootloader
returns to the old slot).

## Updating from a local server

Build the artifact set, serve it, and build the board pointed at it:

```bash
# 1. Build the image for the board's environment.
python3 -m platformio run -e cyd-ili9341

# 2. Assemble manifest + sig + bin, and serve them (needs the signing key).
python tools/ota_local.py --env cyd-ili9341 --version 1.34.0 \
    --key ~/ota-key.pem --serve
#    (without --key, add --dummy-sig to test the transport; the install will fail)

# 3. Build the board with the local base URL and flash it over USB once.
PLATFORMIO_BUILD_FLAGS='-DOTA_WIFI_BASE="http://192.168.4.42:8767/"' \
    python3 -m platformio run -e cyd-ili9341 -t upload
```

Then, on the board, run **UPDATE OVER WIFI** (or just reboot, for the boot
check). Plain `http` is accepted for `OTA_WIFI_BASE` and nowhere else, so the
build flag is what makes the local server usable at all.

Notes:

- `OTA_WIFI_BASE` is `http://host[:port]/prefix/`. The three files are fetched
  at that prefix, so `http://192.168.4.42:8767/` means
  `http://192.168.4.42:8767/manifest-cyd-ili9341.json`, and so on.
- The board's clock is set over NTP during the join; the manifest does not need
  any date.
- `-DOTA_TEST_NO_DNS` makes name lookups fail on purpose, to exercise the
  fallback to `potsquad.com`'s fixed addresses (`connectSite()`).
- `SQW_VERSION=9.9.9 python3 -m platformio run -e <env>` makes a build *claim*
  a version, handy for making a squad nudge or a manifest count as newer.
- The WiFi update ships in the firmware already; there is nothing to enable on
  the server side beyond serving the three files. `tools/ota_local.py` is a
  convenience, not a dependency.
