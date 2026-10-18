// PusiMesh v2 -- the sealed frame and its kinds (spec §4). Pure: the cipher is
// INJECTED (MeshMsg::Crypto), so this compiles and is tested without a crypto
// library, exactly like v1's meshmsg.h. The nonce comes from the sender's
// nodeId, not a link address, which is what lets an RPA-rotating phone be read.
#pragma once

#if POTS_MESH
#include <stdint.h>
#include <stddef.h>
#include "meshmsg.h"      // the injected cipher (shared with v1)
#include "pusimesh_v2.h"

namespace PusiMesh2 {

// ---- the frame (spec §4) --------------------------------------------------
static const size_t HDR_LEN   = 6;   // magic(2) ‖ ver|kind(1) ‖ counter(3)
static const size_t TAG_LEN   = 8;
// The most plaintext a single frame can carry: a text part is one part byte
// plus twelve packed bytes, and that is the widest thing any kind sends.
static const size_t PT_MAX    = 1 + TEXT_PART_BYTES;               // 13
static const size_t FRAME_MIN = HDR_LEN + 1 + TAG_LEN;             // 15
static const size_t FRAME_MAX = HDR_LEN + PT_MAX + TAG_LEN;        // 27
static_assert(2 + FRAME_MAX <= 31, "a frame and its company ID must fit one legacy AD");

// ---- the big frame (the BLE-GATT bearer) ----------------------------------
// BLE-ADV fits 27 bytes; a GATT write fits an MTU. The SAME header and the
// same cipher carry a larger plaintext unchanged -- only the width cap moves,
// because on GATT the frame length is the write length, not a 31-byte AD. The
// 2-byte magic still rejects with one comparison and the tag still pins the
// sender (his nodeId is the nonce). The ADV path keeps isFrame()/seal() at 27;
// the GATT path uses isBigFrame()/sealBig() up to one 512-byte write.
static const size_t BIG_FRAME_MAX = 512;
static const size_t BIG_PT_MAX    = BIG_FRAME_MAX - HDR_LEN - TAG_LEN;  // 498

// ---- kinds (spec §4.1) ----------------------------------------------------
enum Kind : uint8_t {
    K_EXT           = 0x0,
    K_CANNED        = 0x1,
    K_TEXT          = 0x2,
    K_EMOTE         = 0x3,
    K_HELLO         = 0x4,
    K_READ          = 0x5,
    K_SACK          = 0x6,
    K_REPAIR        = 0x7,
    K_TEXT_LONG     = 0x8,
    K_CONTROL       = 0x9,
    K_IDENTITY      = 0xA,
    K_CONFIRM       = 0xB,
    K_ROUTE_PROBE   = 0xC,
    K_TEXT_CYR      = 0xD,
    K_TEXT_LONG_CYR = 0xE,
    // 0xF reserved
};

// ---- EXT subkinds (spec §4.2) -- one registry, additive only --------------
enum Sub : uint8_t {
    S_DM             = 1,
    S_FILE_MANIFEST  = 2,
    S_FILE_CHUNK     = 3,
    S_FILE_ACK       = 4,
    S_FRIEND_REQ     = 5,
    S_FRIEND_RESP    = 6,
    S_CONFIRM        = 7,
    S_BLOB_MANIFEST  = 8,
    S_BLOB_CHUNK     = 9,
    S_BLOB_ACK       = 10,
    S_VOICE_MANIFEST = 11,
    S_MSG_QUERY      = 12,
    S_MSG_SUMMARY    = 13,
    S_MSG_FETCH      = 14,
    S_MSG_CHUNK      = 15,
    S_MSG_ACK        = 16,
    S_IDENTITY       = 17,
    S_PRESENCE_EXT   = 18,
    S_HEADS_UP       = 19,
    S_VOICE_FRAME    = 21,
    S_FIX            = 23,
    S_RC_CHALLENGE   = 24,
    S_RC_ACK         = 25,
    S_RADAR_SIGHT    = 26,
    S_CAPS           = 27,
    S_NUDGE          = 28,
    S_WIFI           = 29,
    S_UPDATED        = 30,
};

// ---- the codec ------------------------------------------------------------
// Magic only: is this frame ours at all. Cheap reject before any crypto.
bool isFrame(const uint8_t* in, size_t len);
// Header only, no crypto: what the receiver checks BEFORE spending an open().
bool parseHeader(const uint8_t* in, size_t len, uint32_t& counter, uint8_t& kind);

// Seals `pt` under `kind`. `nodeId` is the sender's; `counter` must never have
// been used with this key before. Returns the frame length, or 0 on a bad
// argument or a buffer too small.
size_t seal(const MeshMsg::Crypto& c, const uint8_t nodeId[NODE_ID_LEN],
            uint32_t counter, uint8_t kind, const uint8_t* pt, size_t ptLen,
            uint8_t* out, size_t cap);

enum class Open : uint8_t {
    OK,
    NOT_OURS,     // a different magic/company payload -- leave it alone
    BAD_FORMAT,   // ours, but a version, length or kind this build does not read
    BAD_TAG,      // forged, corrupted, another group's, or the wrong sender
    TOO_BIG,      // the plaintext does not fit the caller's buffer
};

// Opens a frame. `nodeId` is the SENDER's, resolved from presence; a wrong id
// is simply BAD_TAG. Writes the plaintext into `pt` (capacity `cap`).
Open open(const MeshMsg::Crypto& c, const uint8_t nodeId[NODE_ID_LEN],
          const uint8_t* in, size_t len, uint32_t& counter, uint8_t& kind,
          uint8_t* pt, size_t cap, size_t& ptLen);

// The big-frame twins of the four calls above, for the BLE-GATT bearer. Same
// header and cipher, a 512-byte write instead of a 27-byte advert (see the
// note by BIG_FRAME_MAX).
bool   isBigFrame(const uint8_t* in, size_t len);
size_t sealBig(const MeshMsg::Crypto& c, const uint8_t nodeId[NODE_ID_LEN],
               uint32_t counter, uint8_t kind, const uint8_t* pt, size_t ptLen,
               uint8_t* out, size_t cap);
Open   openBig(const MeshMsg::Crypto& c, const uint8_t nodeId[NODE_ID_LEN],
               const uint8_t* in, size_t len, uint32_t& counter, uint8_t& kind,
               uint8_t* pt, size_t cap, size_t& ptLen);
// EXT container on the big frame: subkind + payload up to BIG_PT_MAX-1.
size_t sealExtBig(const MeshMsg::Crypto& c, const uint8_t nodeId[NODE_ID_LEN],
                  uint32_t counter, uint8_t sub, const uint8_t* payload, size_t len,
                  uint8_t* out, size_t cap);
Open   openExtBig(const MeshMsg::Crypto& c, const uint8_t nodeId[NODE_ID_LEN],
                  const uint8_t* in, size_t len, uint32_t& counter,
                  uint8_t& sub, uint8_t* payload, size_t cap, size_t& payloadLen);

// ---- typed helpers --------------------------------------------------------
// One part of a typed message. `alpha` picks the kind (TEXT/TEXT_LONG vs the
// _CYR pair). Returns the frame length or 0.
size_t sealTextPart(const MeshMsg::Crypto& c, const uint8_t nodeId[NODE_ID_LEN],
                    uint32_t counter, Alpha alpha, const char* text,
                    uint8_t part, uint8_t total, uint8_t* out, size_t cap);
// Opens a text part: gives its alphabet's characters and the part byte.
Open openTextPart(const MeshMsg::Crypto& c, const uint8_t nodeId[NODE_ID_LEN],
                  const uint8_t* in, size_t len, uint32_t& counter,
                  uint8_t& part, uint8_t& total,
                  char chars[TEXT_PART_UTF8 + 1]);
// '?' for a non-text kind (see the kinds table).
bool kindIsText(uint8_t kind);
char kindAlpha(uint8_t kind);   // 'L' Latin, 'C' Cyrillic, 0 otherwise

// An EXT container: subkind + payload, sealed under K_EXT.
size_t sealExt(const MeshMsg::Crypto& c, const uint8_t nodeId[NODE_ID_LEN],
               uint32_t counter, uint8_t sub, const uint8_t* payload, size_t len,
               uint8_t* out, size_t cap);
Open openExt(const MeshMsg::Crypto& c, const uint8_t nodeId[NODE_ID_LEN],
             const uint8_t* in, size_t len, uint32_t& counter,
             uint8_t& sub, uint8_t* payload, size_t cap, size_t& payloadLen);

} // namespace PusiMesh2
#endif // POTS_MESH
