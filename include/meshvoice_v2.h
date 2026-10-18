// PusiMesh v2 — voice/blob over the mesh (spec PLAN §5.4–5.5). Pure: the
// manifest, the chunk framing and the ACK bitmap are arithmetic over byte
// buffers, so they compile and are tested with no radio, no codec and no
// Arduino. The sealed envelope around them is PusiMesh2::sealExtBig(); this
// file never touches the cipher, only the shape it carries.
//
// Wire (EXT payloads, the subkind byte rides in front and is not counted here):
//   VOICE_MANIFEST: [blobId u32][codec 1][rateK 1][durMs u16][size u32][sha8 8][chunkCount u16]
//   BLOB_CHUNK:     [blobId u32][seq u16][data…][crc16 2]
//   BLOB_ACK:       [blobId u32][bitmap…]   one bit per chunk, LSB-first
//
// All multi-byte fields little-endian, matching the rest of the wire. A chunk
// carries its own CRC16 so a single corrupted part is dropped and re-fetched,
// and the manifest carries the first 8 bytes of SHA-256 over the whole clip so
// the reassembly is checked end to end.
#pragma once

#if POTS_MESH
#include <stdint.h>
#include <stddef.h>

namespace PusiMesh2 {
namespace Voice {

// 1 = Opus 16 kHz VOIP (the default; the Xiaozhi profile). 2 = Codec2 3200 is
// reserved for the LoRa carrier and is not produced by the S3 build yet.
enum Codec : uint8_t { CODEC_OPUS_16K = 1, CODEC_CODEC2_3200 = 2 };

// The most audit data one chunk carries. BIG_PT_MAX - 1 (sub) - 4 (id) - 2
// (seq) - 2 (crc) = 489; rounded down so the sealed frame stays inside one
// 512-byte GATT write with room for the tag.
static const size_t CHUNK_DATA_MAX = 480;

static const size_t MANIFEST_LEN = 22;   // the fixed manifest, no name
static const size_t SHA8_LEN     = 8;
static const size_t CHUNK_HDR    = 4 + 2;   // blobId + seq

struct Manifest {
    uint32_t blobId     = 0;
    uint8_t  codec      = CODEC_OPUS_16K;
    uint8_t  rateK      = 16;     // sample rate in kHz
    uint16_t durMs      = 0;
    uint32_t size       = 0;      // total Opus bytes
    uint8_t  sha8[SHA8_LEN] = {};
    uint16_t chunkCount = 0;
};

// How many chunks a size takes at the chunk profile above; 0 for size 0.
uint16_t chunkCountFor(uint32_t size);
// Fill in chunkCount from size (a convenience for the sender).
void manifestFinish(Manifest& m);

size_t encodeManifest(const Manifest& m, uint8_t* out, size_t cap);
bool   decodeManifest(const uint8_t* in, size_t len, Manifest& m);

// One chunk: id + seq + data + crc16. `cap` needs CHUNK_HDR + dataLen + 2.
size_t encodeChunk(uint32_t blobId, uint16_t seq, const uint8_t* data, size_t dataLen,
                   uint8_t* out, size_t cap);
// Decodes and validates the CRC; `data` points INTO `in` (never copied).
bool   decodeChunk(const uint8_t* in, size_t len, uint32_t& blobId, uint16_t& seq,
                   const uint8_t*& data, size_t& dataLen);

// ACK bitmap: id + one bit per chunk (LSB-first within each byte).
size_t encodeAck(uint32_t blobId, const uint8_t* bitmap, size_t bytes,
                 uint8_t* out, size_t cap);
bool   decodeAck(const uint8_t* in, size_t len, uint32_t& blobId,
                 const uint8_t*& bitmap, size_t& bytes);
inline bool ackBit(const uint8_t* bitmap, size_t bytes, uint16_t seq) {
    return (seq / 8 < bytes) && (bitmap[seq / 8] & (1u << (seq % 8)));
}
inline void ackSet(uint8_t* bitmap, size_t bytes, uint16_t seq) {
    if (seq / 8 < bytes) bitmap[seq / 8] |= (uint8_t)(1u << (seq % 8));
}

// CCITT-FALSE (poly 0x1021, init 0xFFFF) — the CRC16 the whole blob layer uses.
uint16_t crc16(const uint8_t* p, size_t n);

} // namespace Voice
} // namespace PusiMesh2
#endif // POTS_MESH
