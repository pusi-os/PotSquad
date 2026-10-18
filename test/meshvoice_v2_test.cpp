// PusiMesh v2: the voice/blob wire (manifest, chunk, ack). Pure arithmetic —
// no codec, no radio — so every field and every refusal is checkable here.
#include "meshvoice_v2.h"
#include "test_util.h"
#include <cstring>

using namespace PusiMesh2::Voice;

static void testCrc() {
    suite("crc16");
    // CCITT-FALSE, the published check value for "123456789".
    const uint8_t v[] = "123456789";
    ck("the check value is 0x29B1", crc16(v, 9) == 0x29B1);
    ck("an empty run is the init", crc16(v, 0) == 0xFFFF);
}

static void testManifest() {
    suite("manifest");
    ck("1000 bytes take 3 chunks (480 each)", chunkCountFor(1000) == 3);
    ck("480 bytes take one chunk", chunkCountFor(480) == 1);
    ck("481 bytes take two", chunkCountFor(481) == 2);
    ck("zero bytes take none", chunkCountFor(0) == 0);

    Manifest m;
    m.blobId = 0x11223344;
    m.codec  = CODEC_OPUS_16K;
    m.rateK  = 16;
    m.durMs  = 7000;
    m.size   = 1000;
    for (uint8_t i = 0; i < SHA8_LEN; i++) m.sha8[i] = (uint8_t)(0xA0 + i);
    manifestFinish(m);
    ck("  and the finish fills the count", m.chunkCount == 3);

    uint8_t buf[MANIFEST_LEN];
    ck("it encodes", encodeManifest(m, buf, sizeof buf) == MANIFEST_LEN);
    Manifest r;
    ck("it decodes", decodeManifest(buf, sizeof buf, r));
    ck("  id survives", r.blobId == m.blobId);
    ck("  codec survives", r.codec == CODEC_OPUS_16K);
    ck("  rate survives", r.rateK == 16);
    ck("  duration survives", r.durMs == 7000);
    ck("  size survives", r.size == 1000);
    ck("  sha survives", std::memcmp(r.sha8, m.sha8, SHA8_LEN) == 0);
    ck("  count survives", r.chunkCount == 3);

    // A manifest whose count disagrees with its size is refused: it would have
    // the receiver wait for parts that were never sent.
    buf[20] = 9; buf[21] = 0;
    Manifest bad;
    ck("a self-inconsistent count is refused", !decodeManifest(buf, sizeof buf, bad));

    // A short buffer is not a manifest.
    ck("a short read is refused", !decodeManifest(buf, MANIFEST_LEN - 1, bad));
    ck("a long read is refused", !decodeManifest(buf, MANIFEST_LEN + 1, bad));
    ck("encode refuses a small buffer", encodeManifest(m, buf, MANIFEST_LEN - 1) == 0);
}

static void testChunk() {
    suite("chunk");
    uint8_t data[CHUNK_DATA_MAX];
    for (size_t i = 0; i < sizeof data; i++) data[i] = (uint8_t)(i * 7 + 1);

    uint8_t buf[CHUNK_DATA_MAX + 8 + 2];
    const size_t n = encodeChunk(0xCAFEBABE, 0x0102, data, sizeof data, buf, sizeof buf);
    ck("a full chunk encodes", n == CHUNK_DATA_MAX + 8);

    uint32_t id = 0; uint16_t seq = 0; const uint8_t* out = nullptr; size_t outLen = 0;
    ck("it decodes", decodeChunk(buf, n, id, seq, out, outLen));
    ck("  id survives", id == 0xCAFEBABE);
    ck("  seq survives", seq == 0x0102);
    ck("  data survives", outLen == sizeof data && std::memcmp(out, data, outLen) == 0);

    // One flipped byte and the CRC catches it — the part is refetched, not used.
    buf[10] ^= 0x40;
    ck("a corrupted chunk is refused", !decodeChunk(buf, n, id, seq, out, outLen));

    // The last chunk is short; the count still rides on the size, not padding.
    const size_t k = encodeChunk(1, 2, data, 100, buf, sizeof buf);
    ck("a short chunk encodes", k == 100 + 8);
    ck("  and decodes to its true length",
       decodeChunk(buf, k, id, seq, out, outLen) && outLen == 100);

    ck("a chunk with no room is refused", encodeChunk(1, 0, data, 20, buf, 20 + 8 - 1) == 0);
    ck("over-long data is refused",
       encodeChunk(1, 0, data, CHUNK_DATA_MAX + 1, buf, sizeof buf) == 0);
    ck("a run shorter than the header is refused", !decodeChunk(buf, 5, id, seq, out, outLen));
}

static void testAck() {
    suite("ack");
    uint8_t bm[4] = {};
    ackSet(bm, sizeof bm, 0);
    ackSet(bm, sizeof bm, 9);
    ck("bit 0 is set", ackBit(bm, sizeof bm, 0));
    ck("bit 9 is set", ackBit(bm, sizeof bm, 9));
    ck("bit 1 is clear", !ackBit(bm, sizeof bm, 1));
    ck("a bit past the map reads clear", !ackBit(bm, sizeof bm, 99));
    ck("the map holds 32 bits", sizeof bm * 8 == 32);

    uint8_t buf[4 + 4];
    const size_t n = encodeAck(0xDEADBEEF, bm, sizeof bm, buf, sizeof buf);
    ck("it encodes", n == 4 + sizeof bm);
    uint32_t id = 0; const uint8_t* out = nullptr; size_t bytes = 0;
    ck("it decodes", decodeAck(buf, n, id, out, bytes));
    ck("  id survives", id == 0xDEADBEEF);
    ck("  bitmap survives", bytes == sizeof bm && std::memcmp(out, bm, bytes) == 0);
    ck("an ack with only an id is refused", !decodeAck(buf, 4, id, out, bytes));
}

int main() {
    testCrc();
    testManifest();
    testChunk();
    testAck();
    return report();
}
