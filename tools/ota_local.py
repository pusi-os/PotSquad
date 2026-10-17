#!/usr/bin/env python3
"""Build the three files a board fetches for a WiFi update, and optionally
serve them, so a board can update from a local server instead of potsquad.com.

WHAT THE BOARD ASKS FOR. Over plain HTTP from OTA_WIFI_BASE (see
include/ota_wifi.h), the name in the URL being the board's own build:

    manifest-<env>.json      {"version": "1.34.0", "release_name": "...",
                              "whats_new": ["line", "line"]}
    <env>-firmware.sig       the DER ECDSA P-256 signature
    <env>-firmware.bin       the app image

`version` is the ONLY field the board acts on: it is installed only when it is
newer than what is running (MeshMsg::versionNewer). The other two are shown on
the UPDATE screen and are optional. The .sig must exist and be at least eight
bytes even to reach "update available" -- check() fetches it before READY.

THE SIGNATURE IS THE WHOLE SECURITY OF THIS. The board hashes
SHA-256("SQWOTA1\\n" + env + "\\n" + image) and checks the result against the
public key compiled into include/ota_pubkey.h. tools/sign_firmware.py makes a
signature with the matching PRIVATE key (the OTA_SIGNING_KEY CI secret and the
owner's offline backup). Without that key you can serve the manifest and the
image, and the board will offer the update, but the install will stop at
"bad signature". That is by design: a local server is not a way around the
signing key.

Usage:
    # Make the artifact set and serve it on 0.0.0.0:8767
    python tools/ota_local.py --env cyd-ili9341 --version 1.34.0 \\
        --key ~/ota-key.pem --serve

    # Then build the board pointed at it and flash:
    PLATFORMIO_BUILD_FLAGS='-DOTA_WIFI_BASE="http://192.168.4.42:8767/"' \\
        python3 -m platformio run -e cyd-ili9341 -t upload

--dummy-sig writes a placeholder signature so the manifest/sig/bin transport
can be exercised end to end (the check reaches READY); the install itself will
fail until a real signature is made.
"""
import argparse
import http.server
import json
import os
import shutil
import socket
import socketserver
import subprocess
import sys


def lan_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--env", default="cyd-ili9341", help="PlatformIO environment (the build name)")
    ap.add_argument("--version", required=True, help="the version the manifest offers, e.g. 1.34.0")
    ap.add_argument("--bin", help="app image; default .pio/build/<env>/firmware.bin")
    ap.add_argument("--key", help="P-256 private key PEM; without it, no valid signature")
    ap.add_argument("--dummy-sig", action="store_true",
                    help="write a placeholder .sig so the check reaches READY (install will fail)")
    ap.add_argument("--release-name", default="", help="optional release_name for the UPDATE screen")
    ap.add_argument("--notes", action="append", default=[],
                    help="a whats_new line; repeat for more (up to four are shown)")
    ap.add_argument("--out", default=None, help="output directory; default /tmp/potsquad-ota-<env>")
    ap.add_argument("--port", type=int, default=8767, help="port to serve on (default 8767)")
    ap.add_argument("--serve", action="store_true", help="serve the directory and block")
    a = ap.parse_args()

    out = a.out or os.path.join("/tmp", "potsquad-ota-" + a.env)
    os.makedirs(out, exist_ok=True)

    bin_in = a.bin or os.path.join(root, ".pio", "build", a.env, "firmware.bin")
    if not os.path.isfile(bin_in):
        sys.exit("no image at %s -- build it first:\n  python3 -m platformio run -e %s"
                 % (bin_in, a.env))
    bin_out = os.path.join(out, "%s-firmware.bin" % a.env)
    shutil.copyfile(bin_in, bin_out)
    print("image  %s (%d bytes)" % (bin_out, os.path.getsize(bin_out)))

    sig_out = os.path.join(out, "%s-firmware.sig" % a.env)
    if a.key:
        subprocess.run([sys.executable, os.path.join(here, "sign_firmware.py"),
                        "--key", a.key, "--env", a.env, "--bin", bin_out, "--out", sig_out],
                       check=True)
    elif a.dummy_sig:
        with open(sig_out, "wb") as f:
            f.write(b"\0" * 64)
        print("sig    %s (PLACEHOLDER -- the install will fail until it is really signed)" % sig_out)
    else:
        print("sig    NOT WRITTEN -- pass --key to sign, or --dummy-sig to test the transport")

    manifest = {"version": a.version}
    if a.release_name:
        manifest["release_name"] = a.release_name
    if a.notes:
        manifest["whats_new"] = a.notes[:4]
    man_out = os.path.join(out, "manifest-%s.json" % a.env)
    with open(man_out, "w") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")
    print("manifest %s" % man_out)

    ip = lan_ip()
    print("\nserve    cd %s && python3 -m http.server %d" % (out, a.port))
    print("build    PLATFORMIO_BUILD_FLAGS='-DOTA_WIFI_BASE=\"http://%s:%d/\"' "
          "python3 -m platformio run -e %s -t upload" % (ip, a.port, a.env))
    print("note     the board installs only when %s is newer than what it runs"
          % a.version)

    if a.serve:
        os.chdir(out)
        handler = http.server.SimpleHTTPRequestHandler
        with socketserver.TCPServer(("0.0.0.0", a.port), handler) as httpd:
            print("\nserving %s on http://%s:%d/  (Ctrl-C to stop)" % (out, ip, a.port))
            try:
                httpd.serve_forever()
            except KeyboardInterrupt:
                pass


if __name__ == "__main__":
    main()
