// PusiMesh v2 -- presence, identity and the text alphabets. The whole wire
// contract is docs/PUSIMESH-V2-SPEC.md; this is its pure half.
//
// No radio and no cipher live here: presence is unauthenticated by design and
// the frame layer injects its cipher (meshframe_v2.h). Both are arithmetic over
// byte buffers, so both compile and are tested on a desktop -- see
// test/pusimesh_v2_test.cpp. The three Pusi platforms share these exact rules,
// which is the point: two implementations that must agree about a format
// neither owns is a test, and one checked against itself is not.
#pragma once

#if POTS_MESH
#include <stdint.h>
#include <stddef.h>

namespace PusiMesh2 {

// ---- constants (spec §1) --------------------------------------------------
static const uint16_t COMPANY_ID = 0xFFFF;

static const uint8_t  MAGIC[3]       = { 'P', 'M', '2' };   // presence
static const uint8_t  FRAME_MAGIC[2] = { 'P', 'U' };        // frame
static const uint8_t  NAME_AD_MAGIC[3] = { 'P', 'X', 'E' }; // BLE-ADV name split
static const uint8_t  VERSION = 2;

static const size_t   NODE_ID_LEN   = 8;
static const size_t   SQUAD_TAG_LEN = 3;
static const size_t   NONCE_LEN     = 13;

// ---- presence (spec §3) ---------------------------------------------------
// flags byte. Reserved means reserved: a decode refuses a bit this build does
// not know, so a later meaning can be given safely.
static const uint8_t FLAG_NAME  = 0x01;
static const uint8_t FLAG_PHONE = 0x02;
static const uint8_t FLAG_GROUP = 0x04;

// caps bitmap.
static const uint8_t CAP_EXT   = 1u << 0;
static const uint8_t CAP_BLOB  = 1u << 1;
static const uint8_t CAP_VOICE = 1u << 2;
static const uint8_t CAP_RELAY = 1u << 3;

static const size_t NAME_BYTES = 12;                 // bytes, not characters
// body bytes AFTER the two company bytes.
//   magic(3) ver(1) flags(1) nodeId(8) squadTag(3) appearance(2) caps(1)
//   shirt(1) shorts(1) shoes(1) eyes(1)
static const size_t BODY_UNNAMED = 3 + 1 + 1 + NODE_ID_LEN + SQUAD_TAG_LEN + 2 + 1 + 4;  // 23
static const size_t BODY_NAMED   = BODY_UNNAMED + NAME_BYTES;                              // 35

// How many slots the clothes palette has (shared with v1's LOOK_CLOTH_N).
static const uint8_t CLOTH_N = 8;

struct Presence {
    uint8_t  flags = 0;
    uint8_t  nodeId[NODE_ID_LEN] = {};
    uint8_t  squadTag[SQUAD_TAG_LEN] = {};
    uint16_t appearance = 0;      // the v1 appearance word, verbatim
    uint8_t  caps = 0;
    // The cameo's clothing and eyes, which the presence advert has room for
    // in v2 (v1 needed a second advert for the colours and dropped the eyes
    // entirely, so a guest wore the host's shades and lashes).
    uint8_t  shirt = 0, shorts = 0, shoes = 0;   // indices into CLOTH_COLORS
    bool     glasses = false;     // the removable shades layer is on
    bool     lashes = true;       // eyelashes on the bare eyes (default on)
    uint8_t  mood = 0;            // the sender's animation mood, 0..31
    char     name[NAME_BYTES + 1] = {};  // NUL-terminated; empty when unnamed
    uint8_t  nameLen = 0;              // bytes used, <= NAME_BYTES

    bool phone() const { return (flags & FLAG_PHONE) != 0; }
    bool named() const { return (flags & FLAG_NAME) != 0; }
};

// Writes the presence body (no company ID). Returns BODY_UNNAMED or BODY_NAMED,
// or 0 when `out` is too small. A NAME flag with an empty name encodes unnamed:
// a nameless custom name is not a thing, and letting one on the air would make
// every receiver carry a rule for it.
size_t encodePresence(const Presence& p, uint8_t* out, size_t cap);

// Parses a presence body. False -- and `out` untouched -- unless it is exactly
// one of the two legal lengths, has the right magic and version, uses only
// known flag bits, and (when named) carries a well-formed UTF-8 name that is
// NUL-terminated with nothing but padding after it.
bool decodePresence(const uint8_t* in, size_t len, Presence& out);

// The second BLE-ADV advertisement that carries a name the presence advert had
// no room for (spec §3). Body: 'P','X','E' ‖ version ‖ flags ‖ nodeId ‖ name.
// `out` gets NAME_AD_HEADER + name bytes; 0 when the name does not fit.
size_t encodeNameAd(const uint8_t nodeId[NODE_ID_LEN], uint8_t flags,
                    const char* name, uint8_t* out, size_t cap);
bool   decodeNameAd(const uint8_t* in, size_t len, uint8_t nodeId[NODE_ID_LEN],
                    uint8_t& flags, char name[NAME_BYTES + 1]);

// ---- identity helpers (spec §2) -------------------------------------------
// The two hop bytes the envelope trace uses.
inline void nodeShortTag(const uint8_t nodeId[NODE_ID_LEN], uint8_t out[2]) {
    out[0] = nodeId[6];
    out[1] = nodeId[7];
}

// The frame nonce: nodeId[0:6] ‖ counter u24 LE ‖ 0x00 ×4. Built from the
// SENDER'S IDENTITY, never a link address -- that is what survives RPA.
void nonceFor(const uint8_t nodeId[NODE_ID_LEN], uint32_t counter, uint8_t nonce[NONCE_LEN]);

// ---- the text alphabets (spec §5) -----------------------------------------
// Codes index a table of UNICODE CODE POINTS, not bytes: a Cyrillic letter is
// two bytes of UTF-8 but one 6-bit code. The packing is byte-oriented; only the
// human-readable string is multi-byte, so a part's display buffer is twice the
// character count.
enum class Alpha : uint8_t { LATIN = 0, CYRILLIC = 1 };

static const uint8_t  TEXT_PART_CHARS = 16;
static const uint8_t  TEXT_PARTS_MAX  = 15;
static const uint16_t TEXT_MAX        = TEXT_PART_CHARS * TEXT_PARTS_MAX;   // 240
static const size_t   TEXT_PART_BYTES = TEXT_PART_CHARS * 6 / 8;            // 12
// Room for one part's characters as UTF-8 (every character here is <= 2 bytes).
static const size_t   TEXT_PART_UTF8  = TEXT_PART_CHARS * 2;
static const uint8_t  TEXT_END        = 63;   // pads a final part
// The code past the last character in either table.
static const uint8_t  TEXT_CODES_MAX  = 63;

extern const uint16_t CHARSET_LATIN[];
extern const uint16_t CHARSET_CYRILLIC[];
extern const uint8_t  CHARSET_LATIN_N;
extern const uint8_t  CHARSET_CYRILLIC_N;
const uint16_t* charset(Alpha a);
uint8_t         charsetN(Alpha a);
bool    textChar(Alpha a, uint32_t cp);          // is this code point in the set?
// Decodes one UTF-8 code point from `s` at byte offset `i`, advancing `i`.
// Returns 0xFFFFFFFF on a malformed sequence or the end.
uint32_t utf8Decode(const char* s, size_t& i);
// How many parts `s` needs: 1..15, or 0 for empty, over TEXT_MAX, or a
// character outside the alphabet. Never a truncated message.
uint8_t textParts(Alpha a, const char* s);
// Packs part `part` of the whole `s` into 12 bytes (spec §5's bit order).
// Returns the byte count, or 0 for a bad part/total or a bad character.
size_t  packTextPart(Alpha a, const char* s, uint8_t part, uint8_t total,
                     uint8_t out[TEXT_PART_BYTES]);
// Unpacks one part's characters as UTF-8, NUL-terminated, stopping at END.
// `out` must hold TEXT_PART_UTF8 + 1 bytes.
bool    unpackTextPart(Alpha a, const uint8_t in[TEXT_PART_BYTES],
                       char out[TEXT_PART_UTF8 + 1]);
// The part byte: index high four, total low four.
inline uint8_t partByte(uint8_t idx, uint8_t total) {
    return (uint8_t)(((idx & 0x0F) << 4) | (total & 0x0F));
}
inline void splitPartByte(uint8_t b, uint8_t& idx, uint8_t& total) {
    idx   = (uint8_t)((b >> 4) & 0x0F);
    total = (uint8_t)(b & 0x0F);
}

// ---- names ----------------------------------------------------------------
// The number of leading bytes of a UTF-8 string that fit in `maxBytes` without
// splitting a code point, stopping at the NUL. Never returns a partial letter.
size_t trimName(const char* name, size_t maxBytes);

} // namespace PusiMesh2
#endif // POTS_MESH
