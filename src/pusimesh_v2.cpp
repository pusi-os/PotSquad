// PusiMesh v2 presence/identity codec. See include/pusimesh_v2.h for the
// layout and docs/PUSIMESH-V2-SPEC.md for why it is shaped this way.
//
// Everything here runs against bytes a stranger composed, on every advert in
// the air, so the interesting code is the refusals and the test suite is
// mostly cases that must NOT parse -- the same shape as v1's pusimesh.cpp.
#include "pusimesh_v2.h"

#if POTS_MESH

#include <string.h>

namespace PusiMesh2 {

// Code i is CHARSET[i]. Space, letters, digits, then punctuation -- the same
// order as v1's Latin table so a Latin code means what it always did.
const uint16_t CHARSET_LATIN[] = {
    0x20, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    '.', ',', '?', '!', '\'', '-'
};
const uint16_t CHARSET_CYRILLIC[] = {
    0x20,
    0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0401,  // А Б В Г Д Е Ё
    0x0416, 0x0417, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C,  // Ж З И Й К Л М
    0x041D, 0x041E, 0x041F, 0x0420, 0x0421, 0x0422, 0x0423,  // Н О П Р С Т У
    0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 0x0429, 0x042A,  // Ф Х Ц Ч Ш Щ Ъ
    0x042B, 0x042C, 0x042D, 0x042E, 0x042F,                  // Ы Ь Э Ю Я
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    '.', ',', '?', '!', '\'', '-'
};
const uint8_t CHARSET_LATIN_N    = (uint8_t)(sizeof CHARSET_LATIN / sizeof CHARSET_LATIN[0]);
const uint8_t CHARSET_CYRILLIC_N = (uint8_t)(sizeof CHARSET_CYRILLIC / sizeof CHARSET_CYRILLIC[0]);
static_assert(CHARSET_LATIN_N <= TEXT_CODES_MAX, "the Latin table must fit 6-bit codes");
static_assert(CHARSET_CYRILLIC_N <= TEXT_CODES_MAX, "the Cyrillic table must fit 6-bit codes");

const uint16_t* charset(Alpha a) {
    return a == Alpha::CYRILLIC ? CHARSET_CYRILLIC : CHARSET_LATIN;
}
uint8_t charsetN(Alpha a) {
    return a == Alpha::CYRILLIC ? CHARSET_CYRILLIC_N : CHARSET_LATIN_N;
}

uint32_t utf8Decode(const char* s, size_t& i) {
    const uint8_t b = (uint8_t)s[i];
    if (b == 0) return 0xFFFFFFFFu;
    size_t len = 1;
    uint32_t cp = b;
    if ((b & 0xE0) == 0xC0) { len = 2; cp = b & 0x1Fu; }
    else if ((b & 0xF0) == 0xE0) { len = 3; cp = b & 0x0Fu; }
    else if ((b & 0xF8) == 0xF0) { len = 4; cp = b & 0x07u; }
    else if (b >= 0x80) return 0xFFFFFFFFu;
    for (size_t k = 1; k < len; k++) {
        const uint8_t c = (uint8_t)s[i + k];
        if ((c & 0xC0) != 0x80) return 0xFFFFFFFFu;
        cp = (cp << 6) | (c & 0x3Fu);
    }
    i += len;
    return cp;
}

// Writes `cp` as UTF-8 to `out`; returns the byte count (1..3 here).
static size_t utf8Encode(uint32_t cp, char* out) {
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    out[0] = (char)(0xE0 | (cp >> 12));
    out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[2] = (char)(0x80 | (cp & 0x3F));
    return 3;
}

static size_t seqLen(uint8_t b) {
    if (b < 0x80) return 1;
    if ((b & 0xE0) == 0xC0) return 2;
    if ((b & 0xF0) == 0xE0) return 3;
    if ((b & 0xF8) == 0xF0) return 4;
    return 0;
}

// A complete, control-character-free UTF-8 string of exactly `n` bytes. Kept
// deliberately plain: this guards a name that lands on a screen, so its job is
// to reject garbage, not to be a spec-perfect decoder.
static bool utf8Valid(const uint8_t* p, size_t n) {
    size_t i = 0;
    while (i < n) {
        const uint8_t b = p[i];
        if (b < 0x20 || b == 0x7F) return false;
        const size_t len = seqLen(b);
        if (len == 0 || i + len > n) return false;
        for (size_t k = 1; k < len; k++)
            if ((p[i + k] & 0xC0) != 0x80) return false;
        i += len;
    }
    return true;
}

size_t trimName(const char* name, size_t maxBytes) {
    size_t used = 0;
    while (name[used]) {
        const size_t len = seqLen((uint8_t)name[used]);
        if (len == 0 || used + len > maxBytes) break;
        used += len;
    }
    return used;
}

// ---- presence -------------------------------------------------------------

size_t encodePresence(const Presence& p, uint8_t* out, size_t cap) {
    if (!out) return 0;

    size_t nameBytes = 0;
    if ((p.flags & FLAG_NAME) && p.name[0])
        nameBytes = trimName(p.name, NAME_BYTES);

    const bool named = nameBytes > 0;
    const size_t need = named ? BODY_NAMED : BODY_UNNAMED;
    if (cap < need) return 0;

    out[0] = MAGIC[0]; out[1] = MAGIC[1]; out[2] = MAGIC[2];
    out[3] = VERSION;
    out[4] = (uint8_t)((p.flags & ~FLAG_NAME) | (named ? FLAG_NAME : 0));
    memcpy(out + 5, p.nodeId, NODE_ID_LEN);
    memcpy(out + 13, p.squadTag, SQUAD_TAG_LEN);
    out[16] = (uint8_t)(p.appearance & 0xFF);
    out[17] = (uint8_t)((p.appearance >> 8) & 0xFF);
    out[18] = p.caps;
    out[19] = (uint8_t)(p.shirt  & 0x07);
    out[20] = (uint8_t)(p.shorts & 0x07);
    out[21] = (uint8_t)(p.shoes  & 0x07);
    out[22] = (uint8_t)((p.glasses ? 1u : 0u) | (p.lashes ? 2u : 0u) |
                        (uint8_t)((p.mood & 0x1F) << 2));

    if (named) {
        memset(out + BODY_UNNAMED, 0, NAME_BYTES);   // NUL-padded, never length-prefixed
        memcpy(out + BODY_UNNAMED, p.name, nameBytes);
    }
    return need;
}

bool decodePresence(const uint8_t* in, size_t len, Presence& out) {
    if (!in) return false;
    if (len != BODY_UNNAMED && len != BODY_NAMED) return false;
    if (memcmp(in, MAGIC, sizeof(MAGIC)) != 0) return false;
    if (in[3] != VERSION) return false;

    const uint8_t flags = in[4];
    if (flags & ~(uint8_t)(FLAG_NAME | FLAG_PHONE | FLAG_GROUP)) return false;
    const bool named = (flags & FLAG_NAME) != 0;
    if (named != (len == BODY_NAMED)) return false;   // the bit and the length must agree

    Presence p;
    p.flags = flags;
    memcpy(p.nodeId, in + 5, NODE_ID_LEN);
    memcpy(p.squadTag, in + 13, SQUAD_TAG_LEN);
    p.appearance = (uint16_t)(in[16] | ((uint16_t)in[17] << 8));
    p.caps = in[18];
    p.shirt  = (uint8_t)(in[19] & 0x07);
    p.shorts = (uint8_t)(in[20] & 0x07);
    p.shoes  = (uint8_t)(in[21] & 0x07);
    if (p.shirt  >= CLOTH_N) p.shirt  %= CLOTH_N;
    if (p.shorts >= CLOTH_N) p.shorts %= CLOTH_N;
    if (p.shoes  >= CLOTH_N) p.shoes  %= CLOTH_N;
    p.glasses = (in[22] & 0x01) != 0;
    p.lashes  = (in[22] & 0x02) != 0;
    p.mood    = (uint8_t)((in[22] >> 2) & 0x1F);
    p.name[0] = '\0';
    p.nameLen = 0;

    if (named) {
        const uint8_t* n = in + BODY_UNNAMED;
        size_t used = 0;
        while (used < NAME_BYTES && n[used] != 0) used++;
        if (used == 0) return false;                  // named but nothing there
        if (!utf8Valid(n, used)) return false;
        for (size_t i = used; i < NAME_BYTES; i++)
            if (n[i] != 0) return false;              // nothing hiding after the NUL
        memcpy(p.name, n, used);
        p.name[used] = '\0';
        p.nameLen = (uint8_t)used;
    }

    out = p;
    return true;
}

size_t encodeNameAd(const uint8_t nodeId[NODE_ID_LEN], uint8_t flags,
                    const char* name, uint8_t* out, size_t cap) {
    if (!out) return 0;
    const size_t used = trimName(name, NAME_BYTES);
    const size_t need = 3 + 1 + 1 + NODE_ID_LEN + used;   // 13 + name
    if (used == 0 || cap < need) return 0;
    out[0] = NAME_AD_MAGIC[0]; out[1] = NAME_AD_MAGIC[1]; out[2] = NAME_AD_MAGIC[2];
    out[3] = VERSION;
    out[4] = (uint8_t)(flags | FLAG_NAME);
    memcpy(out + 5, nodeId, NODE_ID_LEN);
    memcpy(out + 13, name, used);
    return need;
}

bool decodeNameAd(const uint8_t* in, size_t len, uint8_t nodeId[NODE_ID_LEN],
                  uint8_t& flags, char name[NAME_BYTES + 1]) {
    if (!in) return false;
    if (len < 13 || len > 13 + NAME_BYTES) return false;
    if (memcmp(in, NAME_AD_MAGIC, sizeof(NAME_AD_MAGIC)) != 0) return false;
    if (in[3] != VERSION) return false;
    flags = in[4];
    if (flags & ~(uint8_t)(FLAG_NAME | FLAG_PHONE | FLAG_GROUP)) return false;
    const size_t used = len - 13;
    if (used == 0 || !utf8Valid(in + 13, used)) return false;
    memcpy(nodeId, in + 5, NODE_ID_LEN);
    memcpy(name, in + 13, used);
    name[used] = '\0';
    return true;
}

// ---- identity -------------------------------------------------------------

void nonceFor(const uint8_t nodeId[NODE_ID_LEN], uint32_t counter,
              uint8_t nonce[NONCE_LEN]) {
    memcpy(nonce, nodeId, 6);
    nonce[6] = (uint8_t)(counter & 0xFF);
    nonce[7] = (uint8_t)((counter >> 8) & 0xFF);
    nonce[8] = (uint8_t)((counter >> 16) & 0xFF);
    memset(nonce + 9, 0, 4);
}

// ---- text -----------------------------------------------------------------

bool textChar(Alpha a, uint32_t cp) {
    const uint16_t* set = charset(a);
    for (uint8_t i = 0; i < charsetN(a); i++) if (set[i] == cp) return true;
    return false;
}

uint8_t textParts(Alpha a, const char* s) {
    if (!s || !s[0]) return 0;
    size_t n = 0, i = 0;
    while (s[i]) {
        if (n >= TEXT_MAX) return 0;
        const uint32_t cp = utf8Decode(s, i);
        if (cp == 0xFFFFFFFFu || !textChar(a, cp)) return 0;
        n++;
    }
    uint8_t parts = (uint8_t)((n + TEXT_PART_CHARS - 1) / TEXT_PART_CHARS);
    if (parts < 1) parts = 1;
    if (parts > TEXT_PARTS_MAX) return 0;
    return parts;
}

static int codeFor(Alpha a, uint32_t cp) {
    const uint16_t* set = charset(a);
    for (uint8_t i = 0; i < charsetN(a); i++) if (set[i] == cp) return i;
    return -1;
}

size_t packTextPart(Alpha a, const char* s, uint8_t part, uint8_t total,
                    uint8_t out[TEXT_PART_BYTES]) {
    if (!out || total < 1 || total > TEXT_PARTS_MAX || part >= total) return 0;
    memset(out, 0, TEXT_PART_BYTES);

    // Walk `s` to the first character of this part, one code point at a time.
    size_t i = 0;
    for (uint8_t skip = 0; skip < part; skip++) {
        for (uint8_t k = 0; k < TEXT_PART_CHARS; k++) {
            if (!s[i]) break;
            if (utf8Decode(s, i) == 0xFFFFFFFFu) return 0;
        }
    }

    size_t bit = 0;
    for (uint8_t k = 0; k < TEXT_PART_CHARS; k++) {
        uint8_t code = TEXT_END;
        if (s[i]) {
            const uint32_t cp = utf8Decode(s, i);
            if (cp == 0xFFFFFFFFu) return 0;
            const int c = codeFor(a, cp);
            if (c < 0) return 0;             // a character this alphabet cannot hold
            code = (uint8_t)c;
        }
        for (int b = 5; b >= 0; b--) {
            if ((code >> b) & 1u) out[bit >> 3] |= (uint8_t)(1u << (7 - (bit & 7)));
            bit++;
        }
    }
    return TEXT_PART_BYTES;
}

bool unpackTextPart(Alpha a, const uint8_t in[TEXT_PART_BYTES],
                    char out[TEXT_PART_UTF8 + 1]) {
    if (!in || !out) return false;
    const uint16_t* set = charset(a);
    const uint8_t setN = charsetN(a);
    size_t n = 0;
    size_t bit = 0;
    for (uint8_t k = 0; k < TEXT_PART_CHARS; k++) {
        uint8_t code = 0;
        for (int b = 5; b >= 0; b--) {
            code = (uint8_t)((code << 1) | ((in[bit >> 3] >> (7 - (bit & 7))) & 1u));
            bit++;
        }
        if (code == TEXT_END) break;
        const uint32_t cp = code < setN ? set[code] : '?';   // an unknown code is a version skew
        n += utf8Encode(cp, out + n);
    }
    out[n] = '\0';
    return true;
}

} // namespace PusiMesh2

#endif // POTS_MESH
