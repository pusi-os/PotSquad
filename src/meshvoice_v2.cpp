// PusiMesh v2 voice/blob codec. See include/meshvoice_v2.h.
#include "meshvoice_v2.h"

#if POTS_MESH
#include <string.h>

namespace PusiMesh2 {
namespace Voice {

static inline void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static inline uint32_t get32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline void put16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}
static inline uint16_t get16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

uint16_t chunkCountFor(uint32_t size) {
    if (size == 0) return 0;
    return (uint16_t)((size + CHUNK_DATA_MAX - 1) / CHUNK_DATA_MAX);
}

void manifestFinish(Manifest& m) { m.chunkCount = chunkCountFor(m.size); }

size_t encodeManifest(const Manifest& m, uint8_t* out, size_t cap) {
    if (!out || cap < MANIFEST_LEN) return 0;
    put32(out + 0, m.blobId);
    out[4] = m.codec;
    out[5] = m.rateK;
    put16(out + 6, m.durMs);
    put32(out + 8, m.size);
    memcpy(out + 12, m.sha8, SHA8_LEN);
    put16(out + 20, m.chunkCount);
    return MANIFEST_LEN;
}

bool decodeManifest(const uint8_t* in, size_t len, Manifest& m) {
    if (!in || len != MANIFEST_LEN) return false;
    m.blobId     = get32(in + 0);
    m.codec      = in[4];
    m.rateK      = in[5];
    m.durMs      = get16(in + 6);
    m.size       = get32(in + 8);
    memcpy(m.sha8, in + 12, SHA8_LEN);
    m.chunkCount = get16(in + 20);
    // A manifest that disagrees with itself is not a manifest: the count must
    // match the size it claims, or the receiver would wait forever for parts
    // that were never coming.
    if (m.chunkCount != chunkCountFor(m.size)) return false;
    return m.size > 0 && m.codec != 0;
}

size_t encodeChunk(uint32_t blobId, uint16_t seq, const uint8_t* data, size_t dataLen,
                   uint8_t* out, size_t cap) {
    if (!out || dataLen > CHUNK_DATA_MAX) return 0;
    if (dataLen && !data) return 0;
    const size_t len = CHUNK_HDR + dataLen + 2;
    if (cap < len) return 0;
    put32(out, blobId);
    put16(out + 4, seq);
    if (dataLen) memcpy(out + CHUNK_HDR, data, dataLen);
    put16(out + CHUNK_HDR + dataLen, crc16(out, CHUNK_HDR + dataLen));
    return len;
}

bool decodeChunk(const uint8_t* in, size_t len, uint32_t& blobId, uint16_t& seq,
                 const uint8_t*& data, size_t& dataLen) {
    if (!in || len < CHUNK_HDR + 2) return false;
    const size_t body = len - 2;
    const uint16_t want = get16(in + body);
    if (crc16(in, body) != want) return false;
    blobId  = get32(in);
    seq     = get16(in + 4);
    data    = in + CHUNK_HDR;
    dataLen = body - CHUNK_HDR;
    if (dataLen > CHUNK_DATA_MAX) return false;
    return true;
}

size_t encodeAck(uint32_t blobId, const uint8_t* bitmap, size_t bytes,
                 uint8_t* out, size_t cap) {
    if (!out || (bytes && !bitmap)) return 0;
    if (cap < 4 + bytes) return 0;
    put32(out, blobId);
    if (bytes) memcpy(out + 4, bitmap, bytes);
    return 4 + bytes;
}

bool decodeAck(const uint8_t* in, size_t len, uint32_t& blobId,
               const uint8_t*& bitmap, size_t& bytes) {
    if (!in || len < 4) return false;
    blobId = get32(in);
    bitmap = in + 4;
    bytes  = len - 4;
    return bytes > 0;
}

uint16_t crc16(const uint8_t* p, size_t n) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < n; i++) {
        crc ^= (uint16_t)p[i] << 8;
        for (int b = 0; b < 8; b++)
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

} // namespace Voice
} // namespace PusiMesh2
#endif // POTS_MESH
