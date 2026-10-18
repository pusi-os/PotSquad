// PusiMesh v2: the wire codec against independent golden vectors.
//
// The cipher is not here on purpose -- CI has no crypto library -- so the frame
// tests run against a TOY AEAD that binds key, nonce, AAD and ciphertext the way
// a real one does, which is enough to make every tamper case behave as it will
// on the device. The layout itself is pinned by test/pusimesh_v2_vectors.h,
// built by tools/gen_pusimesh_v2_vectors.py from the spec alone.
#include "pusimesh_v2.h"
#include "meshframe_v2.h"
#include "pusimesh_v2_vectors.h"
#include "test_util.h"
#include <cstring>
#include <cstdio>

using namespace PusiMesh2;
namespace V = PusiMesh2Vectors;

// ---- the toy AEAD ----------------------------------------------------------
static uint8_t s_key[MeshMsg::KEY_LEN];
static bool    s_keyed = false;

static uint64_t fnv(uint64_t h, const uint8_t* p, size_t n) {
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}
static void toyTag(const uint8_t* nonce, const uint8_t* aad, size_t aadLen,
                   const uint8_t* ct, size_t ctLen, uint8_t tag[MeshMsg::TAG_LEN]) {
    uint64_t h = 0xcbf29ce484222325ULL;
    h = fnv(h, s_key, MeshMsg::KEY_LEN);
    h = fnv(h, nonce, NONCE_LEN);
    const uint8_t alen = (uint8_t)aadLen;
    h = fnv(h, &alen, 1);
    h = fnv(h, aad, aadLen);
    h = fnv(h, ct, ctLen);
    for (size_t i = 0; i < MeshMsg::TAG_LEN; i++) tag[i] = (uint8_t)(h >> (8 * i));
}
static bool toyDerive(const char*, size_t, const uint8_t*, size_t,
                      uint32_t, uint8_t key[MeshMsg::KEY_LEN]) {
    for (size_t i = 0; i < MeshMsg::KEY_LEN; i++) key[i] = (uint8_t)(0x5A + i);
    return true;
}
static bool toySetKey(const uint8_t key[MeshMsg::KEY_LEN]) {
    memcpy(s_key, key, MeshMsg::KEY_LEN); s_keyed = true; return true;
}
static bool toySeal(const uint8_t* nonce, const uint8_t* aad, size_t aadLen,
                    const uint8_t* pt, size_t ptLen, uint8_t* ct, uint8_t* tag) {
    if (!s_keyed) return false;
    for (size_t i = 0; i < ptLen; i++)
        ct[i] = pt[i] ^ s_key[i % MeshMsg::KEY_LEN] ^ nonce[i % NONCE_LEN];
    toyTag(nonce, aad, aadLen, ct, ptLen, tag);
    return true;
}
static bool toyOpen(const uint8_t* nonce, const uint8_t* aad, size_t aadLen,
                    const uint8_t* ct, size_t ctLen, const uint8_t* tag, uint8_t* pt) {
    if (!s_keyed) return false;
    uint8_t want[MeshMsg::TAG_LEN];
    toyTag(nonce, aad, aadLen, ct, ctLen, want);
    if (memcmp(want, tag, sizeof want) != 0) return false;
    for (size_t i = 0; i < ctLen; i++)
        pt[i] = ct[i] ^ s_key[i % MeshMsg::KEY_LEN] ^ nonce[i % NONCE_LEN];
    return true;
}
static MeshMsg::Crypto cipher() { return { toyDerive, toySetKey, toySeal, toyOpen }; }

static bool eq(const uint8_t* a, const uint8_t* b, size_t n) { return memcmp(a, b, n) == 0; }

// ---- presence --------------------------------------------------------------
static Presence makePres(bool name) {
    Presence p;
    p.flags = name ? FLAG_NAME : 0;
    memcpy(p.nodeId, V::NODE_ID, NODE_ID_LEN);
    memcpy(p.squadTag, V::SQUAD_TAG, SQUAD_TAG_LEN);
    p.appearance = V::APPEARANCE;
    p.caps = V::CAPS;
    p.shirt = 2; p.shorts = 5; p.shoes = 1;
    p.glasses = true; p.lashes = false; p.mood = 9;
    if (name) strcpy(p.name, "POTS");
    return p;
}

static void testPresence() {
    suite("PusiMesh2 presence v2");

    uint8_t buf[BODY_NAMED];
    // encode matches the independent vector, unnamed and named.
    size_t n = encodePresence(makePres(false), buf, sizeof buf);
    ck("unnamed encodes to the golden length", n == BODY_UNNAMED);
    ck("unnamed encodes to the golden bytes", n == sizeof V::PRES_UNNAMED &&
       eq(buf, V::PRES_UNNAMED, n));

    n = encodePresence(makePres(true), buf, sizeof buf);
    ck("named latin encodes to the golden bytes", n == sizeof V::PRES_NAMED_LATIN &&
       eq(buf, V::PRES_NAMED_LATIN, n));

    Presence c;
    c.flags = FLAG_NAME;
    memcpy(c.nodeId, V::NODE_ID, NODE_ID_LEN);
    memcpy(c.squadTag, V::SQUAD_TAG, SQUAD_TAG_LEN);
    c.appearance = V::APPEARANCE;
    c.caps = V::CAPS;
    c.shirt = 2; c.shorts = 5; c.shoes = 1;
    c.glasses = true; c.lashes = false; c.mood = 9;
    strcpy(c.name, "ПУСЯ");
    n = encodePresence(c, buf, sizeof buf);
    ck("named cyrillic encodes to the golden bytes", n == sizeof V::PRES_NAMED_CYR &&
       eq(buf, V::PRES_NAMED_CYR, n));

    // decode the golden unnamed and recover every field.
    Presence d;
    ck("unnamed golden decodes", decodePresence(V::PRES_UNNAMED, sizeof V::PRES_UNNAMED, d));
    ck("  nodeId survives", memcmp(d.nodeId, V::NODE_ID, NODE_ID_LEN) == 0);
    ck("  squadTag survives", memcmp(d.squadTag, V::SQUAD_TAG, SQUAD_TAG_LEN) == 0);
    ck("  appearance survives", d.appearance == V::APPEARANCE);
    ck("  caps survive", d.caps == V::CAPS);
    ck("  the clothes survive", d.shirt == 2 && d.shorts == 5 && d.shoes == 1);
    ck("  glasses on, lashes off", d.glasses && !d.lashes);
    ck("  the mood survives", d.mood == 9);
    ck("  unnamed has no name", !d.named() && d.name[0] == 0);

    // decode the golden named (Cyrillic) and read the name back.
    ck("named golden decodes", decodePresence(V::PRES_NAMED_CYR, sizeof V::PRES_NAMED_CYR, d));
    ck("  name is the cyrillic string", d.named() && strcmp(d.name, "ПУСЯ") == 0);
    ck("  nameLen is 8 bytes", d.nameLen == 8);

    // refusals: each is a case the decoder must NOT accept.
    uint8_t bad[BODY_NAMED];
    memcpy(bad, V::PRES_UNNAMED, sizeof V::PRES_UNNAMED);
    bad[0] ^= 0xFF;
    ck("a wrong magic is refused", !decodePresence(bad, sizeof V::PRES_UNNAMED, d));
    memcpy(bad, V::PRES_UNNAMED, sizeof V::PRES_UNNAMED);
    bad[3] = 3;
    ck("a bad version is refused", !decodePresence(bad, sizeof V::PRES_UNNAMED, d));
    memcpy(bad, V::PRES_UNNAMED, sizeof V::PRES_UNNAMED);
    bad[4] = 0x80;
    ck("an unknown flag bit is refused", !decodePresence(bad, sizeof V::PRES_UNNAMED, d));
    ck("a length between the two is refused",
       !decodePresence(V::PRES_UNNAMED, BODY_UNNAMED + 1, d));
    // name bytes with no NAME flag: the length and the bit must agree.
    uint8_t nf[BODY_NAMED];
    // encode will drop the name and the flag together, so hand-build the mismatch:
    memcpy(nf, V::PRES_NAMED_CYR, sizeof V::PRES_NAMED_CYR);
    nf[4] = 0;   // named length, no NAME flag
    ck("a flag/length mismatch is refused", !decodePresence(nf, sizeof nf, d));

    // a control character in the name.
    uint8_t ctl[BODY_NAMED];
    memcpy(ctl, V::PRES_NAMED_LATIN, sizeof V::PRES_NAMED_LATIN);
    ctl[BODY_UNNAMED] = 0x07;
    ck("a control character in the name is refused", !decodePresence(ctl, sizeof ctl, d));

    // junk hidden after the terminator.
    uint8_t junk[BODY_NAMED];
    memcpy(junk, V::PRES_NAMED_LATIN, sizeof V::PRES_NAMED_LATIN);
    junk[BODY_NAMED - 1] = 'X';
    ck("padding of non-zero bytes is refused", !decodePresence(junk, sizeof junk, d));

    // the empty-name encode demotes to unnamed rather than lying.
    Presence empty = makePres(true);
    empty.name[0] = '\0';
    n = encodePresence(empty, buf, sizeof buf);
    ck("an empty custom name encodes unnamed", n == BODY_UNNAMED);
}

static void testNameAd() {
    suite("PusiMesh2 name advertisement");

    uint8_t buf[64];
    const size_t n = encodeNameAd(V::NODE_ID, 0, "ПУСЯ", buf, sizeof buf);
    ck("name ad encodes to the golden bytes", n == sizeof V::NAME_AD_CYR &&
       eq(buf, V::NAME_AD_CYR, n));

    uint8_t id[NODE_ID_LEN], flags = 0;
    char name[NAME_BYTES + 1];
    ck("name ad decodes", decodeNameAd(V::NAME_AD_CYR, sizeof V::NAME_AD_CYR, id, flags, name));
    ck("  the name is the cyrillic string", strcmp(name, "ПУСЯ") == 0);
    ck("  the nodeId survives", memcmp(id, V::NODE_ID, NODE_ID_LEN) == 0);
    ck("  the flag carries NAME", (flags & FLAG_NAME) != 0);

    uint8_t bad[64];
    memcpy(bad, V::NAME_AD_CYR, sizeof V::NAME_AD_CYR);
    bad[0] = 0;
    ck("a wrong magic is refused",
       !decodeNameAd(bad, sizeof V::NAME_AD_CYR, id, flags, name));
}

static void testText() {
    suite("PusiMesh2 text alphabets");

    ck("latin needs two parts", textParts(Alpha::LATIN, V::TEXT_LATIN) == 2);
    ck("cyrillic needs two parts", textParts(Alpha::CYRILLIC, V::TEXT_CYR) == 2);
    ck("lowercase is not in the latin set", textParts(Alpha::LATIN, "abc") == 0);
    ck("a latin string is not valid cyrillic",
       textParts(Alpha::CYRILLIC, "HELLO") == 0);
    ck("an empty message has no parts", textParts(Alpha::LATIN, "") == 0);

    uint8_t p[TEXT_PART_BYTES];
    ck("latin part 0 packs to the golden bytes",
       packTextPart(Alpha::LATIN, V::TEXT_LATIN, 0, 2, p) == TEXT_PART_BYTES &&
       eq(p, V::TEXT_LATIN_P0, TEXT_PART_BYTES));
    ck("latin part 1 packs to the golden bytes",
       packTextPart(Alpha::LATIN, V::TEXT_LATIN, 1, 2, p) == TEXT_PART_BYTES &&
       eq(p, V::TEXT_LATIN_P1, TEXT_PART_BYTES));
    ck("cyrillic part 0 packs to the golden bytes",
       packTextPart(Alpha::CYRILLIC, V::TEXT_CYR, 0, 2, p) == TEXT_PART_BYTES &&
       eq(p, V::TEXT_CYR_P0, TEXT_PART_BYTES));
    ck("cyrillic part 1 packs to the golden bytes",
       packTextPart(Alpha::CYRILLIC, V::TEXT_CYR, 1, 2, p) == TEXT_PART_BYTES &&
       eq(p, V::TEXT_CYR_P1, TEXT_PART_BYTES));

    char s[TEXT_PART_UTF8 + 1];
    unpackTextPart(Alpha::LATIN, V::TEXT_LATIN_P0, s);
    ck("latin part 0 unpacks to its first sixteen", strcmp(s, "HELLO WORLD 123.") == 0);
    unpackTextPart(Alpha::LATIN, V::TEXT_LATIN_P1, s);
    ck("latin part 1 unpacks to the tail", strcmp(s, "?!") == 0);
    unpackTextPart(Alpha::CYRILLIC, V::TEXT_CYR_P0, s);
    ck("cyrillic part 0 unpacks to its first sixteen",
       strcmp(s, "ПРИВЕТ МИР! КАК ") == 0);
    unpackTextPart(Alpha::CYRILLIC, V::TEXT_CYR_P1, s);
    ck("cyrillic part 1 unpacks to the tail", strcmp(s, "ДЕЛА") == 0);

    // round-trip a full 240-character message: pack every part, unpack, join.
    char msg[TEXT_MAX + 1];
    for (int i = 0; i < TEXT_MAX; i++) msg[i] = "0123456789"[i % 10];
    msg[TEXT_MAX] = 0;
    char joined[TEXT_MAX + 1];
    size_t at = 0;
    const uint8_t parts = textParts(Alpha::LATIN, msg);
    for (uint8_t i = 0; i < parts; i++) {
        uint8_t b[TEXT_PART_BYTES];
        packTextPart(Alpha::LATIN, msg, i, parts, b);
        char one[TEXT_PART_UTF8 + 1];
        unpackTextPart(Alpha::LATIN, b, one);
        memcpy(joined + at, one, strlen(one));
        at += strlen(one);
    }
    joined[at] = 0;
    ck("a full 240-character message round-trips", parts == 15 && strcmp(joined, msg) == 0);
}

// ---- frames ----------------------------------------------------------------
static void testFrames() {
    suite("PusiMesh2 frame v2");

    const MeshMsg::Crypto c = cipher();
    uint8_t key[MeshMsg::KEY_LEN];
    for (size_t i = 0; i < MeshMsg::KEY_LEN; i++) key[i] = (uint8_t)(0x5A + i);
    c.setKey(key);

    ck("the golden nonce matches", [&] {
        uint8_t n[NONCE_LEN]; nonceFor(V::NODE_ID, V::FRAME_COUNTER, n);
        return eq(n, V::FRAME_NONCE, NONCE_LEN);
    }());

    uint8_t pt[PT_MAX], frame[FRAME_MAX];
    for (size_t i = 0; i < PT_MAX; i++) pt[i] = (uint8_t)(0xC0 + i);
    const size_t flen = seal(c, V::NODE_ID, V::FRAME_COUNTER, V::FRAME_KIND,
                             pt, PT_MAX, frame, sizeof frame);
    ck("a frame seals to the golden length", flen == FRAME_MAX);
    ck("the header matches the golden bytes", eq(frame, V::FRAME_HDR, HDR_LEN));
    uint32_t counter = 0; uint8_t kind = 0;
    ck("  parseHeader reads it back", parseHeader(frame, flen, counter, kind) &&
       counter == V::FRAME_COUNTER && kind == V::FRAME_KIND);

    uint8_t got[PT_MAX], gotKind = 0; uint32_t gotCounter = 0; size_t gotLen = 0;
    ck("the frame opens", PusiMesh2::open(c, V::NODE_ID, frame, flen, gotCounter, gotKind,
                               got, sizeof got, gotLen) == Open::OK);
    ck("  and the plaintext round-trips", gotLen == PT_MAX &&
       eq(got, pt, PT_MAX));

    // a wrong sender id is a BAD_TAG, not a crash.
    uint8_t other[NODE_ID_LEN]; memcpy(other, V::NODE_ID, NODE_ID_LEN); other[0] ^= 1;
    ck("the wrong sender id fails the tag",
       PusiMesh2::open(c, other, frame, flen, gotCounter, gotKind, got, sizeof got, gotLen) == Open::BAD_TAG);

    // tampering any ciphertext byte fails the tag.
    uint8_t tampered[FRAME_MAX];
    memcpy(tampered, frame, flen);
    tampered[HDR_LEN] ^= 0x01;
    ck("a flipped ciphertext byte fails the tag",
       PusiMesh2::open(c, V::NODE_ID, tampered, flen, gotCounter, gotKind, got, sizeof got, gotLen) == Open::BAD_TAG);

    // a foreign payload is NOT_OURS, and does not even reach the cipher.
    uint8_t foreign[FRAME_MAX] = { 'S', 'T', 0x22, 0, 0, 0 };
    ck("a foreign magic is NOT_OURS",
       PusiMesh2::open(c, V::NODE_ID, foreign, FRAME_MIN, gotCounter, gotKind, got, sizeof got, gotLen) == Open::NOT_OURS);
    ck("isFrame rejects it", !isFrame(foreign, FRAME_MIN));

    // a bad version is BAD_FORMAT.
    uint8_t badver[FRAME_MAX];
    memcpy(badver, frame, flen);
    badver[2] = (uint8_t)((3 << 4) | V::FRAME_KIND);
    ck("a future version is BAD_FORMAT",
       PusiMesh2::open(c, V::NODE_ID, badver, flen, gotCounter, gotKind, got, sizeof got, gotLen) == Open::BAD_FORMAT);

    // a text part that does not fit the caller's buffer is TOO_BIG.
    uint8_t tiny[4];
    ck("a plaintext larger than the buffer is TOO_BIG",
       PusiMesh2::open(c, V::NODE_ID, frame, flen, gotCounter, gotKind, tiny, sizeof tiny, gotLen) == Open::TOO_BIG);

    // text helpers round-trip end to end.
    uint8_t f2[FRAME_MAX];
    ck("a latin part seals", sealTextPart(c, V::NODE_ID, 100, Alpha::LATIN,
        V::TEXT_LATIN, 0, 2, f2, sizeof f2) == FRAME_MAX);
    uint8_t part = 0, total = 0; char chars[TEXT_PART_UTF8 + 1];
    ck("and opens", PusiMesh2::openTextPart(c, V::NODE_ID, f2, FRAME_MAX, gotCounter, part, total, chars) == Open::OK);
    ck("  with the right part byte", part == 0 && total == 2);
    ck("  and the right text", strcmp(chars, "HELLO WORLD 123.") == 0);

    // the EXT container carries a subkind and a payload.
    uint8_t extf[FRAME_MAX], payload[3] = { 0xDE, 0xAD, 0xBE };
    const size_t xlen = sealExt(c, V::NODE_ID, 200, S_CAPS, payload, sizeof payload,
                                extf, sizeof extf);
    uint8_t sub = 0, back[PT_MAX]; size_t backLen = 0;
    ck("an ext container opens", openExt(c, V::NODE_ID, extf, xlen, gotCounter,
        sub, back, sizeof back, backLen) == Open::OK);
    ck("  with the right subkind", sub == S_CAPS);
    ck("  and payload", backLen == sizeof payload && eq(back, payload, backLen));
}

// ---- the big frame (BLE-GATT) ----------------------------------------------
static void testBigFrames() {
    suite("PusiMesh2 big frame (GATT)");

    const MeshMsg::Crypto c = cipher();
    uint8_t key[MeshMsg::KEY_LEN];
    for (size_t i = 0; i < MeshMsg::KEY_LEN; i++) key[i] = (uint8_t)(0x5A + i);
    c.setKey(key);

    // A voice chunk-sized plaintext: far past what an advert frame could hold,
    // and the whole reason the width cap moved.
    uint8_t pt[BIG_PT_MAX]; for (size_t i = 0; i < sizeof pt; i++) pt[i] = (uint8_t)(i * 3 + 5);
    static uint8_t frame[BIG_FRAME_MAX];
    const size_t flen = sealBig(c, V::NODE_ID, 4242, K_EXT, pt, sizeof pt, frame, sizeof frame);
    ck("a big frame seals", flen == HDR_LEN + sizeof pt + TAG_LEN);
    ck("isBigFrame accepts it", isBigFrame(frame, flen));
    ck("isFrame rejects it (past the advert cap)", !isFrame(frame, flen));

    static uint8_t got[BIG_PT_MAX]; uint32_t counter = 0; uint8_t kind = 0; size_t gotLen = 0;
    ck("it opens", openBig(c, V::NODE_ID, frame, flen, counter, kind, got, sizeof got, gotLen) == Open::OK);
    ck("  same counter", counter == 4242);
    ck("  plaintext survives", gotLen == sizeof pt && eq(got, pt, gotLen));

    uint8_t other[NODE_ID_LEN]; memcpy(other, V::NODE_ID, NODE_ID_LEN); other[0] ^= 1;
    ck("a wrong sender id fails the tag",
       openBig(c, other, frame, flen, counter, kind, got, sizeof got, gotLen) == Open::BAD_TAG);

    // The EXT container at full width, which is what a BLOB_CHUNK rides in.
    static uint8_t payload[BIG_PT_MAX - 1];
    for (size_t i = 0; i < sizeof payload; i++) payload[i] = (uint8_t)(0x11 ^ i);
    static uint8_t extf[BIG_FRAME_MAX], back[BIG_PT_MAX];
    const size_t xlen = sealExtBig(c, V::NODE_ID, 7, S_BLOB_CHUNK, payload, sizeof payload,
                                   extf, sizeof extf);
    uint8_t sub = 0; size_t backLen = 0;
    ck("a big ext container opens",
       openExtBig(c, V::NODE_ID, extf, xlen, counter, sub, back, sizeof back, backLen) == Open::OK);
    ck("  right subkind", sub == S_BLOB_CHUNK);
    ck("  full payload", backLen == sizeof payload && eq(back, payload, backLen));

    // Past the width cap is refused, not silently truncated.
    ck("a plaintext past BIG_PT_MAX is refused",
       sealBig(c, V::NODE_ID, 1, K_EXT, pt, BIG_PT_MAX + 1, frame, sizeof frame) == 0);
    // A tiny read is not ours.
    ck("a run shorter than a frame is NOT_OURS",
       openBig(c, V::NODE_ID, frame, FRAME_MIN - 1, counter, kind, got, sizeof got, gotLen) == Open::NOT_OURS);
}

int main() {
    testPresence();
    testNameAd();
    testText();
    testFrames();
    testBigFrames();
    return report();
}
