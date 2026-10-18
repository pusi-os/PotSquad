// PusiMesh v2 frame codec. See include/meshframe_v2.h and
// docs/PUSIMESH-V2-SPEC.md §4 for the layout.
//
// The cipher is injected, so this file contains no crypto: seal() and open()
// are header arithmetic around a call through MeshMsg::Crypto, and the host
// suite exercises every refusal against a toy AEAD (test/pusimesh_v2_test.cpp).
#include "meshframe_v2.h"

#if POTS_MESH

#include <string.h>

namespace PusiMesh2 {

bool isFrame(const uint8_t* in, size_t len) {
    return in && len >= FRAME_MIN && len <= FRAME_MAX &&
           in[0] == FRAME_MAGIC[0] && in[1] == FRAME_MAGIC[1];
}

bool parseHeader(const uint8_t* in, size_t len, uint32_t& counter, uint8_t& kind) {
    if (!isFrame(in, len)) return false;
    if ((in[2] >> 4) != VERSION) return false;
    kind = (uint8_t)(in[2] & 0x0F);
    counter = (uint32_t)in[3] | ((uint32_t)in[4] << 8) | ((uint32_t)in[5] << 16);
    return true;
}

size_t seal(const MeshMsg::Crypto& c, const uint8_t nodeId[NODE_ID_LEN],
            uint32_t counter, uint8_t kind, const uint8_t* pt, size_t ptLen,
            uint8_t* out, size_t cap) {
    if (!out || kind > K_TEXT_LONG_CYR) return 0;
    if (ptLen > PT_MAX) return 0;
    if (ptLen && !pt) return 0;
    const size_t len = HDR_LEN + ptLen + TAG_LEN;
    if (cap < len) return 0;

    out[0] = FRAME_MAGIC[0];
    out[1] = FRAME_MAGIC[1];
    out[2] = (uint8_t)((VERSION << 4) | (kind & 0x0F));
    out[3] = (uint8_t)(counter & 0xFF);
    out[4] = (uint8_t)((counter >> 8) & 0xFF);
    out[5] = (uint8_t)((counter >> 16) & 0xFF);

    uint8_t nonce[NONCE_LEN];
    nonceFor(nodeId, counter, nonce);
    if (!c.seal(nonce, out, HDR_LEN, pt, ptLen, out + HDR_LEN, out + HDR_LEN + ptLen))
        return 0;
    return len;
}

Open open(const MeshMsg::Crypto& c, const uint8_t nodeId[NODE_ID_LEN],
          const uint8_t* in, size_t len, uint32_t& counter, uint8_t& kind,
          uint8_t* pt, size_t cap, size_t& ptLen) {
    if (!in) return Open::NOT_OURS;
    if (in[0] != FRAME_MAGIC[0] || in[1] != FRAME_MAGIC[1]) return Open::NOT_OURS;
    if (len < FRAME_MIN || len > FRAME_MAX) return Open::BAD_FORMAT;
    if ((in[2] >> 4) != VERSION) return Open::BAD_FORMAT;

    kind = (uint8_t)(in[2] & 0x0F);
    if (kind > K_TEXT_LONG_CYR) return Open::BAD_FORMAT;
    counter = (uint32_t)in[3] | ((uint32_t)in[4] << 8) | ((uint32_t)in[5] << 16);
    const size_t ctLen = len - HDR_LEN - TAG_LEN;
    if (ctLen > cap) return Open::TOO_BIG;

    uint8_t nonce[NONCE_LEN];
    nonceFor(nodeId, counter, nonce);
    if (!c.open(nonce, in, HDR_LEN, in + HDR_LEN, ctLen, in + HDR_LEN + ctLen, pt))
        return Open::BAD_TAG;

    ptLen = ctLen;
    return Open::OK;
}

// ---- the big frame (BLE-GATT) ---------------------------------------------
// Identical header and cipher to seal()/open(); only the width cap differs, so
// the two share every line that matters and split on the one line that does not.

static bool magicOK(const uint8_t* in, size_t len, size_t max) {
    return in && len >= FRAME_MIN && len <= max &&
           in[0] == FRAME_MAGIC[0] && in[1] == FRAME_MAGIC[1];
}

bool isBigFrame(const uint8_t* in, size_t len) { return magicOK(in, len, BIG_FRAME_MAX); }

size_t sealBig(const MeshMsg::Crypto& c, const uint8_t nodeId[NODE_ID_LEN],
               uint32_t counter, uint8_t kind, const uint8_t* pt, size_t ptLen,
               uint8_t* out, size_t cap) {
    if (!out || kind > K_TEXT_LONG_CYR) return 0;
    if (ptLen > BIG_PT_MAX) return 0;
    if (ptLen && !pt) return 0;
    const size_t len = HDR_LEN + ptLen + TAG_LEN;
    if (cap < len) return 0;

    out[0] = FRAME_MAGIC[0];
    out[1] = FRAME_MAGIC[1];
    out[2] = (uint8_t)((VERSION << 4) | (kind & 0x0F));
    out[3] = (uint8_t)(counter & 0xFF);
    out[4] = (uint8_t)((counter >> 8) & 0xFF);
    out[5] = (uint8_t)((counter >> 16) & 0xFF);

    uint8_t nonce[NONCE_LEN];
    nonceFor(nodeId, counter, nonce);
    if (!c.seal(nonce, out, HDR_LEN, pt, ptLen, out + HDR_LEN, out + HDR_LEN + ptLen))
        return 0;
    return len;
}

Open openBig(const MeshMsg::Crypto& c, const uint8_t nodeId[NODE_ID_LEN],
             const uint8_t* in, size_t len, uint32_t& counter, uint8_t& kind,
             uint8_t* pt, size_t cap, size_t& ptLen) {
    if (!magicOK(in, len, BIG_FRAME_MAX)) return Open::NOT_OURS;
    if ((in[2] >> 4) != VERSION) return Open::BAD_FORMAT;

    kind = (uint8_t)(in[2] & 0x0F);
    if (kind > K_TEXT_LONG_CYR) return Open::BAD_FORMAT;
    counter = (uint32_t)in[3] | ((uint32_t)in[4] << 8) | ((uint32_t)in[5] << 16);
    const size_t ctLen = len - HDR_LEN - TAG_LEN;
    if (ctLen > cap) return Open::TOO_BIG;

    uint8_t nonce[NONCE_LEN];
    nonceFor(nodeId, counter, nonce);
    if (!c.open(nonce, in, HDR_LEN, in + HDR_LEN, ctLen, in + HDR_LEN + ctLen, pt))
        return Open::BAD_TAG;

    ptLen = ctLen;
    return Open::OK;
}

size_t sealExtBig(const MeshMsg::Crypto& c, const uint8_t nodeId[NODE_ID_LEN],
                  uint32_t counter, uint8_t sub, const uint8_t* payload, size_t len,
                  uint8_t* out, size_t cap) {
    if (len > BIG_PT_MAX - 1) return 0;
    if (len && !payload) return 0;
    static uint8_t pt[BIG_PT_MAX];
    pt[0] = sub;
    if (len) memcpy(pt + 1, payload, len);
    return sealBig(c, nodeId, counter, K_EXT, pt, 1 + len, out, cap);
}

Open openExtBig(const MeshMsg::Crypto& c, const uint8_t nodeId[NODE_ID_LEN],
                const uint8_t* in, size_t len, uint32_t& counter,
                uint8_t& sub, uint8_t* payload, size_t cap, size_t& payloadLen) {
    uint8_t kind = 0;
    static uint8_t pt[BIG_PT_MAX];
    size_t ptLen = 0;
    const Open r = openBig(c, nodeId, in, len, counter, kind, pt, sizeof pt, ptLen);
    if (r != Open::OK) return r;
    if (kind != K_EXT || ptLen < 1) return Open::BAD_FORMAT;

    sub = pt[0];
    payloadLen = ptLen - 1;
    if (payloadLen > cap) return Open::TOO_BIG;
    if (payloadLen) memcpy(payload, pt + 1, payloadLen);
    return Open::OK;
}

// ---- typed helpers --------------------------------------------------------

bool kindIsText(uint8_t kind) {
    return kind == K_TEXT || kind == K_TEXT_LONG ||
           kind == K_TEXT_CYR || kind == K_TEXT_LONG_CYR;
}

char kindAlpha(uint8_t kind) {
    switch (kind) {
        case K_TEXT:      case K_TEXT_LONG:     return 'L';
        case K_TEXT_CYR:  case K_TEXT_LONG_CYR: return 'C';
        default: return 0;
    }
}

static Alpha alphaOfKind(uint8_t kind) {
    return kind == K_TEXT_CYR || kind == K_TEXT_LONG_CYR ? Alpha::CYRILLIC : Alpha::LATIN;
}

size_t sealTextPart(const MeshMsg::Crypto& c, const uint8_t nodeId[NODE_ID_LEN],
                    uint32_t counter, Alpha alpha, const char* text,
                    uint8_t part, uint8_t total, uint8_t* out, size_t cap) {
    if (textParts(alpha, text) != total) return 0;

    uint8_t kind;
    if (alpha == Alpha::CYRILLIC)
        kind = total == 1 ? K_TEXT_CYR : K_TEXT_LONG_CYR;
    else
        kind = total == 1 ? K_TEXT : K_TEXT_LONG;

    uint8_t pt[PT_MAX];
    pt[0] = partByte(part, total);
    if (packTextPart(alpha, text, part, total, pt + 1) != TEXT_PART_BYTES) return 0;
    return seal(c, nodeId, counter, kind, pt, 1 + TEXT_PART_BYTES, out, cap);
}

Open openTextPart(const MeshMsg::Crypto& c, const uint8_t nodeId[NODE_ID_LEN],
                  const uint8_t* in, size_t len, uint32_t& counter,
                  uint8_t& part, uint8_t& total, char chars[TEXT_PART_UTF8 + 1]) {
    uint8_t kind = 0;
    uint8_t pt[PT_MAX];
    size_t ptLen = 0;
    const Open r = open(c, nodeId, in, len, counter, kind, pt, sizeof pt, ptLen);
    if (r != Open::OK) return r;
    if (!kindIsText(kind) || ptLen != 1 + TEXT_PART_BYTES) return Open::BAD_FORMAT;

    splitPartByte(pt[0], part, total);
    if (total < 1 || total > TEXT_PARTS_MAX || part >= total) return Open::BAD_FORMAT;
    unpackTextPart(alphaOfKind(kind), pt + 1, chars);
    return Open::OK;
}

size_t sealExt(const MeshMsg::Crypto& c, const uint8_t nodeId[NODE_ID_LEN],
               uint32_t counter, uint8_t sub, const uint8_t* payload, size_t len,
               uint8_t* out, size_t cap) {
    if (len > PT_MAX - 1) return 0;
    if (len && !payload) return 0;
    uint8_t pt[PT_MAX];
    pt[0] = sub;
    if (len) memcpy(pt + 1, payload, len);
    return seal(c, nodeId, counter, K_EXT, pt, 1 + len, out, cap);
}

Open openExt(const MeshMsg::Crypto& c, const uint8_t nodeId[NODE_ID_LEN],
             const uint8_t* in, size_t len, uint32_t& counter,
             uint8_t& sub, uint8_t* payload, size_t cap, size_t& payloadLen) {
    uint8_t kind = 0;
    uint8_t pt[PT_MAX];
    size_t ptLen = 0;
    const Open r = open(c, nodeId, in, len, counter, kind, pt, sizeof pt, ptLen);
    if (r != Open::OK) return r;
    if (kind != K_EXT || ptLen < 1) return Open::BAD_FORMAT;

    sub = pt[0];
    payloadLen = ptLen - 1;
    if (payloadLen > cap) return Open::TOO_BIG;
    if (payloadLen) memcpy(payload, pt + 1, payloadLen);
    return Open::OK;
}

} // namespace PusiMesh2

#endif // POTS_MESH
