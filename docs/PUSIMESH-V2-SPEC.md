# PusiMesh v2 — wire specification (Ф0)

Status: frozen for Ф0, 2026-10-18. Source of truth: `docs/PUSIMESH-V2-PLAN.md`.
This document is the byte-level contract the three platforms (PotSquad firmware,
Android PixelTag-App, Desktop PusiDesk) must agree on. It fixes the layouts the
plan left ambiguous (presence magic length, the Cyrillic alphabet's carrier
byte) and every value here is pinned by golden vectors in
`test/pusimesh_v2_vectors.h`, generated independently by
`tools/gen_pusimesh_v2_vectors.py` (Python `cryptography`).

Wraparound: `x ‖ y` is concatenation, `[a..b]` is a half-open byte range,
integers are little-endian unless the row says BE.

## 1. Constants

| name | value |
|---|---|
| company id | `0xFFFF` (BLE manufacturer-specific) |
| presence magic | `'P','M','2'` (3 bytes) |
| frame magic | `'P','U'` (2 bytes, distinct from presence and from v1's `'S','T'`) |
| version | `2` |
| frame cipher | AES-128-CCM, tag 8 |
| nonce | `nodeId[0:6] ‖ counter u24 LE ‖ 0x00 ×4` (13 B) |
| text packing | 6-bit codes, 16 per part, 12 bytes |
| TEXT_MAX | 240 chars (15 parts × 16) |
| presence body | 19 B unnamed, 31 B named |

## 2. Identity (`nodeId`)

- `nodeId` — 8 random bytes, generated once, persisted forever
  (firmware NVS `pusimesh/nodeid`, app `SharedPreferences("mesh")/nodeId`).
  It is the logical identity; the link address (BLE RPA / STA MAC) is ephemeral.
- `nodeShortTag` — `nodeId[6:8]`, a hop label in the envelope trace.
- `squadKey` — `PBKDF2-HMAC-SHA256(phrase, "pusimesh/v2/key", 20000) → 16 B`.
- `routeKey` — `HMAC-SHA256(squadKey, "pusimesh/v2/route")[0:16]`.
- `squadTag` (3 B) — `SHA256(squadKey ‖ "pusimesh/v2/squad")[0:3]`. A relay can
  check membership without decrypting the frame.
- `counter` — 24 bits, per sender, never reused under one key, persisted ahead
  of use in blocks of 64 (same rule as v1: reserve, write, then use).

## 3. Presence v2

Body after the 2 company bytes:

| off | size | field |
|---|---|---|
| 0 | 3 | magic `'P','M','2'` |
| 3 | 1 | version = 2 |
| 4 | 1 | flags: `0x01` NAME, `0x02` PHONE, `0x04` GROUP_ROLE |
| 5 | 8 | `nodeId` |
| 13 | 3 | `squadTag` |
| 16 | 2 | appearance word (LE) — reused from v1 (`pusimesh.h`): nick/outfit/shade/aura |
| 18 | 1 | caps bitmap: bit0 EXT, bit1 blob, bit2 voice, bit3 relay |
| 19 | 1 | shirt colour index (0..7 into the shared `CLOTH_COLORS`) |
| 20 | 1 | shorts colour index |
| 21 | 1 | shoes colour index |
| 22 | 1 | eyes: bit0 glasses on, bit1 lashes on, bits2..6 mood (0..31) |
| 23 | 0..12 | name, UTF-8, trimmed to a character boundary — only if NAME |

Length is exactly 23 (unnamed) or 35 (named: 23 + 12). Decode refuses any other
length, an unknown version, or unknown flag bits (reserved means reserved).

**Why the clothes and eyes are here and not in a second advert.** v1 carried
the shirt/shorts/shoes colours in a *separate* manufacturer AD and dropped the
eyes entirely — so a guest wore the host's shades and lashes, which reads as a
reflection rather than a visitor. In v2 the unnamed presence is 23 bytes; with
the 2 company bytes and a 2-byte AD header that is 27, so it still fits one
legacy 31-byte advert with room to spare. A named presence (35) does **not**
fit any 31-byte advert beside itself, so a custom name rides its own
advertising slot, time-multiplexed with the presence (see the transport
profile), never a second AD in the same packet. The `PEX` name advert body is
`'P','X','E' ‖ version ‖ flags ‖ nodeId ‖ name`.

Presence is best-effort and unauthenticated by design: it is the "who is around"
channel, and its only security job is to not be *misread*. Identity is proven by
the sealed frame, never by presence.

## 4. MeshFrame v2

| off | size | field |
|---|---|---|
| 0 | 2 | magic `'P','U'` |
| 2 | 1 | version `(2 << 4)` \| kind |
| 3 | 3 | counter u24 LE |
| 6 | n | ciphertext |
| 6+n | 8 | tag |

AAD is `[0..6)` (the six header bytes). The nonce is built from the sender's
`nodeId` and the counter (§2), which is what lets a phone with a rotating RPA be
read: the key is not derived from an address the receiver cannot see.

Sizes: `min frame` (canned, ct=1) = 15 B; `text part` (ct=13) = 27 B. Both fit a
legacy BLE AD after the 2 company bytes (`2+27 = 29 ≤ 31`).

### 4.1 kinds

| kind | name | plaintext |
|---|---|---|
| 0x0 | EXT | `[subkind 1][payload…]` — container (§4.2) |
| 0x1 | CANNED | `[index 1]` |
| 0x2 | TEXT | `[part 1][12 packed]` — Latin, 1 part |
| 0x3 | EMOTE | `[emote 1][setup 1]` |
| 0x4 | HELLO | `[maj 1][min 1][pat 1][epoch u32][zone+1 1]` |
| 0x5 | READ | `[base u24][tag 4]` — tag = the reader's `nodeId[0:4]` |
| 0x6 | SACK | `[base u24][mask u16]` |
| 0x7 | REPAIR | `[base u24][missing u16]` |
| 0x8 | TEXT_LONG | `[part 1][12 packed]` — Latin, multi-part |
| 0x9 | CONTROL | `[base u24][state 1]` |
| 0xA | IDENTITY | `[nodeId 8][caps 1][name…]` |
| 0xB | CONFIRM | `[base u24][tag 4]` — tag = the confirmer's `nodeId[0:4]` |
| 0xC | ROUTE_PROBE | `[nonce 4]` |
| 0xD | TEXT_CYR | `[part 1][12 packed]` — Cyrillic, 1 part |
| 0xE | TEXT_LONG_CYR | `[part 1][12 packed]` — Cyrillic, multi-part |
| 0xF | — | reserved |

**Deviation from the plan, recorded:** the plan put the alphabet "in the part
byte". There is no spare bit there (index 4 + total 4 are full), and a separate
alphabet byte overruns the 13-byte plaintext budget. The alphabet therefore
gets its own kinds (`TEXT_CYR`, `TEXT_LONG_CYR`) rather than its own byte. Kind
space is 16 wide and 0xF stays free, so this costs nothing and keeps the frame
27 bytes. Mixed-script text is sent as parts of different kinds.

The `part` byte is `partIdx (high 4) | partTotal (low 4)`; index is 0-based,
total is 1..15. Code `63` (`TEXT_END`) pads the final part.

### 4.2 EXT subkinds (single registry)

```
1 DM             12 MSG_QUERY     23 FIX
2 FILE_MANIFEST  13 MSG_SUMMARY   24 RC_CHALLENGE
3 FILE_CHUNK     14 MSG_FETCH     25 RC_ACK
4 FILE_ACK       15 MSG_CHUNK     26 RADAR_SIGHT
5 FRIEND_REQ     16 MSG_ACK       27 CAPS
6 FRIEND_RESP    17 IDENTITY      28 NUDGE
7 CONFIRM        18 PRESENCE_EXT  29 WIFI
8 BLOB_MANIFEST  19 HEADS_UP      30 UPDATED
9 BLOB_CHUNK     20 (reserved)    31.. reserved
10 BLOB_ACK      21 VOICE_FRAME
11 VOICE_MANIFEST 22 (reserved)
```

An unknown subkind is dropped silently; the container never changes to add one.

## 5. Text alphabets

Code `63` is END. Both sets share digits and `.,?!'-`; only the letters differ.

- **Latin** (`TEXT`, `TEXT_LONG`):
  `" ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,?!'-"` (43 codes)
- **Cyrillic** (`TEXT_CYR`, `TEXT_LONG_CYR`):
  `" АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ0123456789.,?!'-"` (50 codes)

Both are ≤ 64, so no message is ever truncated to `?`. `textParts(text, alpha)`
returns 1..15, or 0 for empty / over `TEXT_MAX` / a character outside the set.

## 6. Names

UTF-8, trimmed on a code-point boundary (never split a 2-byte Cyrillic letter).
Presence carries up to 12 B (6 Cyrillic letters); a longer name (up to 63 B)
rides an `IDENTITY` frame (kind 0xA) and is kept in the receiver's roster. The
roster keys on `nodeId`, never on a link address or a `PEER-xxxx` fallback.

## 7. Testing

Every layout above has a golden vector in `test/pusimesh_v2_vectors.h`, built by
an independent implementation and checked byte-for-byte by
`test/pusimesh_v2_test.cpp`; that suite also covers the refusals (bad magic,
bad version, bad length, unknown flags, over-long name, END mid-part). Host:

```
make -C test out/pusimesh_v2_test && ./test/out/pusimesh_v2_test
```
