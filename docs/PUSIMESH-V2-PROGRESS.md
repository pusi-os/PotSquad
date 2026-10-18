# PusiMesh v2 — home build report (Ф0 + S3 bring-up)

Date: 2026-10-18. Author: agent «прошивка». Plan: `docs/PUSIMESH-V2-PLAN.md`.
Wire spec: `docs/PUSIMESH-V2-SPEC.md`. Firmware task: `docs/PUSIMESH-V2-TASK-FIRMWARE.md`.

This report covers what was *landed and verified* in this pass, and exactly
where the next pass picks up. It is deliberately honest about the boundary:
Ф0 (spec + tested codecs) is complete; the runtime integration (Ф1 onward)
is scoped but not landed, and the S3 board is a build, not a bench-checked UI.

---

## 1. What is done

### Ф0 — spec, pure codecs, golden vectors, host tests (complete)

New, self-contained, radio-agnostic code. Every byte is pinned by an
independent implementation, and all 45 host suites (including the new one)
pass under `-fsanitize=address,undefined`.

| file | role |
|---|---|
| `docs/PUSIMESH-V2-SPEC.md` | the frozen wire contract for all three platforms |
| `include/pusimesh_v2.h`, `src/pusimesh_v2.cpp` | identity, presence v2, name AD, nonce, text alphabets |
| `include/meshframe_v2.h`, `src/meshframe_v2.cpp` | sealed frame v2, kinds, EXT container, typed helpers |
| `tools/gen_pusimesh_v2_vectors.py` | golden vectors, independent of the firmware |
| `test/pusimesh_v2_vectors.h` | generated vectors (do not edit) |
| `test/pusimesh_v2_test.cpp` | 53 assertions; happy paths + every refusal |
| `test/Makefile` | `pusimesh_v2_test` target (`-DPOTS_MESH=1`) |

Verified:

- **Presence v2** encodes/decodes to the golden bytes for the unnamed, named
  Latin and named Cyrillic forms; every refusal fires (bad magic, bad version,
  bad length, unknown flag bit, flag/length mismatch, control char, junk after
  the NUL terminator). An empty custom name demotes to unnamed.
- **Russian works end to end**: the Cyrillic 6-bit alphabet (50 codes ≤ 64)
  packs and unpacks byte-exactly, including a full **240-character** message
  over 15 parts. No message is ever truncated to `?`.
- **Frames**: header and nonce match the golden bytes; the frame opens; a wrong
  sender id, a flipped ciphertext byte, a future version, a foreign magic and
  an oversized plaintext each fail with the right code; text-part and EXT
  helpers round-trip.

Confirmed no cost to shipping builds yet: the CYD binary is byte-for-byte the
same size as before (1889353 B) because nothing references the new code, so
the linker dead-strips it.

### S3 Xiaozhi-cube board (builds)

| file | change |
|---|---|
| `include/xiaozhi_cube_154_user_setup.h` | ST7789 240x240 TFT_eSPI setup, pins from ComPiXi's board |
| `platformio.ini` | `[env:xiaozhi-cube-154]` (ESP32-S3-N16R8, 16 MB, PSRAM) |
| `src/main.cpp` | backlight pin 13, panel inversion, the three keys, board name |

The cube runs the existing **SQW_SMALL** UI: no touch, and the buttons drive
the same tap handling every screen already has through the on-screen cursor
(`stickDrawCursor`), so it keeps full feature parity — animations, characters,
the scanner — on a small screen with buttons, not a cut-down menu.

Key map (matches ComPiXi's `compixi_keys.h` intent):

| key | GPIO | action |
|---|---|---|
| BOOT | 0 | press where the cursor is; held = home |
| VOL+ | 40 | cursor forward (one step per press) |
| VOL- | 39 | cursor back |

### Builds

| env | result | flash |
|---|---|---|
| `cyd-ili9341` | SUCCESS | 96.1% of 1.92 MB (unchanged) |
| `xiaozhi-cube-154` | SUCCESS | 42.8% of 4 MB |

---

## 2. What is deliberately NOT done yet (and why)

**Ф1 runtime integration is not landed.** It is not a small change: presence
v2, the `linkAddr→nodeId` map, and the nodeId-based nonce must land *together*,
because a device that advertises v2 presence but still opens frames with the
old MAC-derived nonce looks present and then fails to read anybody. A partial
drop would leave the mesh broken between flashes, which is worse than not
starting. The exact integration points, for the next pass:

1. **Identity storage** (new): nodeId (8 B) in NVS `pusimesh/nodeid`, created
   with `esp_random()` once; `squadTag = SHA256(squadKey‖"pusimesh/v2/squad")[0:3]`.
2. `src/detection.cpp` `Mesh::buildSelf()` → build a `PusiMesh2::Presence`
   and `encodePresence` (short form; name via the `PEX` second AD).
3. `src/detection.cpp` the presence parse path (the `PusiMesh::decode` caller)
   → `PusiMesh2::decodePresence`, and record `linkAddr→nodeId` (TTL,
   freshest-wins) in `mesh.cpp`.
4. `src/meshtalk.cpp` frame seal/open: nonce from **own nodeId** on send and
   from the **sender's nodeId** (resolved from the map) on receive, instead of
   the observed MAC.
5. `src/mesh.cpp` peer table: key on nodeId, not `PEER-xxxx`/MAC.

**S3 UI is a build, not a bench check.** The compact layout is tuned for the
240x135 sticks; on a 240x240 panel it will render but the bottom of the screen
is under-used. Visual tuning (and, if wanted, audio for voice messages) is a
hardware pass. No audio code is wired into this board yet.

**Ф2–Ф8** (frame runtime, envelope/relay, ARQ, mailbox, blob/voice, LoRa+server,
hardening) are unchanged from the plan.

---

## 3. Appearance: what a guest's cameo carries (and the gap that was fixed)

A guest is a *presence* advert, not a frame, so this is the presence path.

| appearance bit | before | after |
|---|---|---|
| shirt / shorts / shoes colour | **yes** (v1 `Look` AD → `setClothPreview`) | yes |
| shades colour (4 tints) | **yes** (the `shade` index in the appearance word → `setShadesPreview`) | yes |
| outfit / nick / aura | **yes** | yes |
| **glasses ON/OFF** | **no — the guest wore the HOST's shades** | **yes** (`Look` v2 bit0) |
| **lashes ON/OFF** | **no — the guest wore the HOST's lashes** | **yes** (`Look` v2 bit1) |

The clothes colours were always on the wire, but the two eye choices were not
transferred at all — the drawing code called `glassesOn()`/`lashesOn()`, i.e.
the host's own settings, for every guest. Fixed by:

- `include/pusimesh.h` / `src/pusimesh.cpp` — `Look` v2: an eighth payload byte
  carrying glasses + lashes, `LOOK_VERSION` 1 → 2.
- `src/detection.cpp` — `buildSelfLook()` sends the two bits.
- `include/potsy.h` / `src/potsy.cpp` — `setGlassesPreview`/`setLashesPreview`
  and a `glassesShown()`/`lashesShown()` the draw path reads; the Settings rows
  still show our own switches.
- `src/ui_clear.cpp` — both cameo draw sites set the eye previews from the
  peer's `Look` (and clear them after, like the cloth preview).

Outfits that draw their own fixed costume still ignore the cloth indices and
the eye bits where those are part of the costume (TH3 0N3's shades, for
example) — that is the existing, intended rule, not a regression.

The same fields are now in the **v2 presence body** (spec §3: clothes at 19–21,
eyes at 22), so the new protocol will not lose what v1 had, and adds the eyes.

## 4. Reproduce

```sh
# host codecs + all suites
make -C test            # 45 suites, all pass
make -C test out/pusimesh_v2_test && ./test/out/pusimesh_v2_test

# regenerate the golden vectors (needs python3 + `cryptography`)
python3 tools/gen_pusimesh_v2_vectors.py

# firmware
pio run -e cyd-ili9341
pio run -e xiaozhi-cube-154
```

---

## 5. Addendum — 2026-10-18, later pass (Ф1 foundation landed)

Owner decision on the fork: **switch firmware to v2 now** (the phone/PixelDesk
go dark until agents №2/№3 follow), **voice deferred**.

Landed and verified this pass:

| file | change |
|---|---|
| `include/pusimesh_identity.h`, `src/pusimesh_identity.cpp` | `nodeId` arithmetic (generate/validate) and the `LinkMap` (linkAddr→nodeId, TTL, freshest-wins). Pure; NVS only behind `#if ARDUINO`. |
| `test/pusimesh_identity_test.cpp`, `test/Makefile` | 22 assertions: id validity, the two stuck-RNG escapes, map freshest-wins, TTL, RPA re-point, bounded fill. Target `pusimesh_identity_test`. |
| `src/detection.cpp`, `include/detection.h` | boot-time load-or-generate-or-persist of `pusimesh/nodeid`; `Mesh::nodeId()` accessor; logs `[mesh-id] nodeId …`. |
| `include/pusimesh_v2.h` (`NAME_MAX`→`NAME_BYTES`) | `NAME_MAX` is a POSIX macro from `<limits.h>`; renamed so the header can be pulled into the Arduino build without a clash. |

Verified on the bench: cube `nodeId c2f12f9f62c847ec` (stable across reset,
NVS-persisted); CYD `nodeId c9e1f481d81ae9a1` at boot. All 46 host suites pass;
`cyd-ili9341` and `xiaozhi-cube-154` build and run.

**Not yet wired** (next pass, and it must land as one piece): presence v2 in
`buildSelf`/`onManufacturerData` (short form + the `PEX` name slot, so the name
is time-multiplexed rather than a second AD in one packet), the roster re-keyed
on nodeId via `LinkMap`, and the frame nonce from nodeId in `meshtalk.cpp`. The
appeearance-policy and presence-size host tests (`mesh_appearance_test`,
`mesh_visit_appearance_test`, `mesh_primary_transport_test`) pin the v1 sizes
and move with it.

Also queued for the plan (owner, same day): **Ф5.5 battery gauge** — a vector
battery icon, four sections that recolour with charge, percent legible on every
section, one shared module for all boards. Placed between v2 and voice in
`PUSIMESH-V2-PLAN.md` §7.

Red-message bubble (owner, same day): now points at the **sender's** cameo
(`ui_clear.cpp` `drawMessageUi`), and on the cube like/dislike is a **hold** of
VOL+/VOL− (`SmallKeys::nextLong/prevLong`), so a short tap still walks the
cursor.

---

## 6. Addendum — 2026-10-18, the wire switch (Ф1 presence + nonce landed)

Owner call: cut the firmware over to v2 presence now (the phone/desktop go dark
until their agents follow). This landed the presence half of Ф1 and the frame
nonce, and is bench-verified between the cube and the CYD.

| file | change |
|---|---|
| `src/mesh.cpp` | rewritten around a **guest table keyed on nodeId** (not the link address). `buildSelfShort` (23 B unnamed presence for BLE), `buildSelf` (named, for ESP-NOW), `buildSelfNameAd` (the PEX slot). `onManufacturerData` decodes presence v2 + the PEX name, merges clothes/eyes straight from the presence, and resolves a frame's sender to its nodeId. The one-visitor rule is held by nodeId, so a rotated address keeps the seat. `nodeId()` now lives here (lazy NVS load-or-make). |
| `include/pusimesh.h` / `src/pusimesh.cpp` | `packAppearance`/`unpackAppearance`, shared by v1's payload and v2's presence body. |
| `src/detection.cpp` | the advert is now a single **short** presence AD; a custom name rides a **time-multiplexed PEX slot** (`ADV_ALT_MS` = 8 s) because the two together exceed 31 B. The separate look AD is gone (colours are inline in v2). The idle rebuild interval follows the slot window. `Mesh::nodeId()` warmed at radio start. |
| `src/pusimesh_espnow.cpp` | presence buffers sized for the v2 body (35 B named). |
| `src/meshtalk.cpp` | every sealed frame now uses `s_ownId` = `nodeId[0:6]` for its nonce (`sealCanned/Text/Emote/Nudge/Wifi/Updated/Read/HeadsUp/Hello/InviteKey`); the invite **target** stays the real BLE address (`s_ownMac`). Receive already opens with the sender's id, which mesh now resolves to the sender's nodeId. |

`nonceFor` in `meshmsg.h` was already `mac6 ‖ counter u24 LE ‖ 0×4`, i.e. the
spec's nonce shape, so no frame-format rewrite was needed for this step: the
identity simply replaces the address in its first six bytes.

Bench (cube `c2f12f9f62c847ec`, CYD `c9e1f481d81ae9a1`): each logs the other as
arrived **by nodeId** (`[squad] 62c847ec` / `[squad] d81ae9a1`), no duplicates,
no size failures; and sealed frames open across the hop — the CYD received the
cube's emote 26 ("from POTS") and the cube the CYD's emote 23. Both boards build;
all 46 host suites pass.

**Still open for full three-platform interop (Ф2):** the frame *format* is still
v1 `'ST'`; the spec's v2 `'PU'` frame with the EXT/IDENTITY container and the
Cyrillic kind pair is the next chunk, and it is what the phone and desktop
agents will speak. Until then the phone stays dark by design (accepted).

---

## 7. Addendum — 2026-10-18, the frame switch (Ф2 core landed)

The firmware now speaks the spec's v2 frames, so the three-platform contract is
met end-to-end between two boards.

| file | change |
|---|---|
| `src/meshmsg.cpp` | KDF salt → `"pusimesh/v2/key"` (spec §2). |
| `src/meshtalk.cpp` | birth of a body store for the v2 text budget: `Message::body` is now `MESH_TEXT_BYTES` (480: 240 chars × 2-byte UTF-8). `V2Asm` reassembles up to 15 parts of UTF-8. `deliverV2()` decodes `'PU'` frames (`K_READ/K_HELLO/K_CANNED/K_TEXT*/K_EMOTE/K_EXT`, unknown subkinds dropped). Sends for canned/text/emote/hello/read emit `'PU'`; the key is re-stretched from the phrase once when the `key2` slot is absent (so nobody retypes a phrase). |
| `src/mesh.cpp` | `onManufacturerData` recognises `PusiMesh2::isFrame` as well as the legacy `MeshMsg::isFrame`, so v2 frames reach the runtime. |
| `docs/PUSIMESH-V2-SPEC.md` | READ/CONFIRM `tag 4` is now defined: the reader/confirmer's `nodeId[0:4]` (the spec left it implicit). |
| `test/gen_meshmsg_vectors.py`, `include/meshmsg_vectors.h` | regenerated for the new salt. |

Bench (cube ⇄ CYD), both flashed: a canned line, a **Latin** and a **Cyrillic**
text, emotes both ways, and a READ receipt all crossed the hop — e.g. the CYD
logged `message from ПОЦИК: Ща вернусь.` and the cube `POTS read #8709`. All 46
host suites pass; both boards build.

The legacy `'ST'` path stays for the pairing / WiFi-share / update frames the
firmware still speaks; the app only needs the `'PU'` kinds above.

Still open from the plan: envelope/relay (Ф3), ARQ (Ф4), mailbox (Ф5), blob/voice
(Ф6), LoRa/server (Ф7), hardening (Ф8). Text is UTF-8-safe but the compose
keyboard still types ≤48 characters — the 240-character raise is the next UI step.
