// PusiMesh payload codec. See include/pusimesh.h for the layout and
// for why it is as small as it is.
//
// Everything here runs against bytes a stranger composed, inside the BLE
// advertisement callback, on every advert in the air. So the shape of this
// file is the same as the pwnagotchi parser next door in signatures.cpp:
// the interesting code is the refusals, and the test suite is mostly cases
// that must NOT parse.
#include "pusimesh.h"

// Compiled ONLY into the mesh build. Without this the codec links into
// every board -- measured at +8,680 bytes of flash on cyd-fast -- and the
// promise that a shipping build is untouched by this feature stops being
// checkable. The host tests define POTS_MESH themselves; see test/Makefile.
#if POTS_MESH

#include <string.h>

namespace PusiMesh {

static inline uint8_t maskOf(uint8_t bits) { return (uint8_t)((1u << bits) - 1u); }

// Printable ASCII only, the same rule and the same reason as the pwnagotchi
// name parser: this string lands in a field the screen renders, and a
// control character in it would be somebody else deciding what our display
// does.
static inline bool printable(char c) {
    return (uint8_t)c >= 0x20 && (uint8_t)c <= 0x7E;
}

uint16_t packAppearance(uint8_t nick, uint8_t outfit, uint8_t shade, bool custom, bool aura) {
    uint16_t word = 0;
    word |= (uint16_t)((nick   & maskOf(NICK_BITS))   << NICK_SHIFT);
    word |= (uint16_t)((outfit & maskOf(OUTFIT_BITS)) << OUTFIT_SHIFT);
    if (outfit & (1u << OUTFIT_BITS)) word |= OUTFIT_HI_BIT;
    word |= (uint16_t)((shade  & maskOf(SHADE_BITS))  << SHADE_SHIFT);
    if (custom) word |= CUSTOM_BIT;
    if (aura)   word |= AURA_BIT;
    return word;
}

void unpackAppearance(uint16_t word, uint8_t& nick, uint8_t& outfit, uint8_t& shade,
                      bool& custom, bool& aura) {
    nick   = (uint8_t)((word >> NICK_SHIFT)   & maskOf(NICK_BITS));
    outfit = (uint8_t)((word >> OUTFIT_SHIFT) & maskOf(OUTFIT_BITS));
    if (word & OUTFIT_HI_BIT) outfit |= (uint8_t)(1u << OUTFIT_BITS);
    shade  = (uint8_t)((word >> SHADE_SHIFT)  & maskOf(SHADE_BITS));
    custom = (word & CUSTOM_BIT) != 0;
    aura   = (word & AURA_BIT)   != 0;
    // Clamp last, once the word is trusted. Modulo as decode() does: an index
    // this build lacks is a newer peer, and a wrong hat beats refusing them.
    if (nick   >= NICK_N)   nick   %= NICK_N;
    if (outfit >= OUTFIT_N) outfit %= OUTFIT_N;
    if (shade  >= SHADE_N)  shade  %= SHADE_N;
}

size_t encode(const Peer& p, uint8_t* out) {
    // Decide first whether this is really a custom name. An empty one is
    // not, and encoding it as custom would put a payload on the air that
    // every receiver then has to have a rule for.
    bool custom = p.custom && p.name[0] != '\0';

    const uint16_t word = packAppearance(p.nick, p.outfit, p.shade, custom, p.aura);

    memcpy(out, MAGIC, sizeof(MAGIC));
    out[4] = VERSION;
    out[5] = (uint8_t)(word & 0xFF);          // little-endian, as the rest
    out[6] = (uint8_t)((word >> 8) & 0xFF);   // of this project stores 16s
    out[7] = p.phone ? FLAG_PHONE : 0;         // flags: reserved, 0 but for a phone

    size_t n = LEN_INDEXED;
    if (custom) {
        // Fixed-width and NUL-padded rather than length-prefixed. A length byte
        // is one more field that can lie, and at twelve bytes the padding costs
        // less than the validation would.
        memset(out + LEN_INDEXED, 0, NAME_LEN);
        for (size_t i = 0; i < NAME_LEN && p.name[i]; i++) {
            out[LEN_INDEXED + i] = (uint8_t)p.name[i];
        }
        n = LEN_NAMED;
    }
    if (p.phone) {
        memcpy(out + n, p.id, PHONE_ID_LEN);
        n += PHONE_ID_LEN;
    }
    return n;
}

void phoneMac(const uint8_t id[PHONE_ID_LEN], uint8_t out[6]) {
    out[0] = 0x02; out[1] = 'P';
    memcpy(out + 2, id, PHONE_ID_LEN);
}

size_t encodeLook(const Look& l, uint8_t* out) {
    out[0] = LOOK_MAGIC;
    const uint16_t v = (uint16_t)((l.shirt  & maskOf(3))        |
                                  ((l.shorts & maskOf(3)) << 3)  |
                                  ((l.shoes  & maskOf(3)) << 6)  |
                                  ((l.state  & maskOf(5)) << 9)  |
                                  ((l.glasses ? 1u : 0u)  << 14) |
                                  ((l.lashes  ? 1u : 0u)  << 15));
    out[1] = (uint8_t)(v & 0xFF);
    out[2] = (uint8_t)(v >> 8);
    return LOOK_PAYLOAD;
}

bool decodeLook(const uint8_t* in, size_t len, Look& out) {
    if (!in) return false;
    if (len != LOOK_PAYLOAD) return false;
    // One magic byte, which doubles as the version: an older extension used a
    // four-byte magic and simply is not this one, so it decodes as nothing.
    if (in[0] != LOOK_MAGIC) return false;

    const uint16_t v = (uint16_t)(in[1] | ((uint16_t)in[2] << 8));
    Look l;
    l.shirt   = (uint8_t)(v & maskOf(3));
    l.shorts  = (uint8_t)((v >> 3) & maskOf(3));
    l.shoes   = (uint8_t)((v >> 6) & maskOf(3));
    l.state   = (uint8_t)((v >> 9) & maskOf(5));
    l.glasses = ((v >> 14) & 1u) != 0;
    l.lashes  = ((v >> 15) & 1u) != 0;
    if (l.shirt  >= LOOK_CLOTH_N) l.shirt  %= LOOK_CLOTH_N;
    if (l.shorts >= LOOK_CLOTH_N) l.shorts %= LOOK_CLOTH_N;
    if (l.shoes  >= LOOK_CLOTH_N) l.shoes  %= LOOK_CLOTH_N;
    out = l;
    return true;
}

bool decode(const uint8_t* in, size_t len, Peer& out) {
    if (!in) return false;
    // Length is checked before anything is read, so a short buffer can
    // never reach the field accesses below.
    if (len != LEN_INDEXED && len != LEN_NAMED && len != LEN_INDEXED_PHONE && len != LEN_NAMED_PHONE) return false;
    if (memcmp(in, MAGIC, sizeof(MAGIC)) != 0) return false;
    if (in[4] != VERSION) return false;
    // Reserved means reserved. Refusing a flags byte with bits this build
    // does not know is what makes it safe to give those bits a meaning
    // later: a device from before will decline a payload using them rather
    // than misread it. The phone bit is the first one spent.
    if (in[7] != 0 && in[7] != FLAG_PHONE) return false;
    const bool phone = in[7] == FLAG_PHONE;

    const uint16_t word = (uint16_t)(in[5] | ((uint16_t)in[6] << 8));
    const bool custom = (word & CUSTOM_BIT) != 0;

    // The three must agree. A custom bit with no name bytes, or name bytes
    // with no custom bit, is a malformed payload either way -- and catching
    // it here is what lets the caller trust `custom` without re-checking
    // the length it was derived from. A phone's four bytes come after.
    const size_t want = (custom ? LEN_NAMED : LEN_INDEXED) + (phone ? PHONE_ID_LEN : 0);
    if (len != want) return false;

    Peer p;
    unpackAppearance(word, p.nick, p.outfit, p.shade, p.custom, p.aura);
    p.phone  = phone;
    memset(p.id, 0, sizeof p.id);
    if (phone) memcpy(p.id, in + len - PHONE_ID_LEN, PHONE_ID_LEN);
    p.name[0] = '\0';

    if (custom) {
        const uint8_t* n = in + LEN_INDEXED;
        size_t used = 0;
        while (used < NAME_LEN && n[used] != 0) {
            if (!printable((char)n[used])) return false;
            used++;
        }
        // A custom payload whose name is empty is the case encode() refuses
        // to produce, so seeing one means it did not come from us.
        if (used == 0) return false;
        // Everything past the terminator must be padding. Without this a
        // sender could park a second string in the tail -- invisible to a
        // renderer that stops at the NUL, and waiting for any future code
        // that reads the field as a fixed twelve bytes.
        for (size_t i = used; i < NAME_LEN; i++) {
            if (n[i] != 0) return false;
        }
        memcpy(p.name, n, used);
        p.name[used] = '\0';
    }

    out = p;
    return true;
}

} // namespace PusiMesh

#endif // POTS_MESH
