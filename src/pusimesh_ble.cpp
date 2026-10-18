// PusiMesh over BLE-GATT — see include/pusimesh_ble.h.
#include "pusimesh_ble.h"

#if POTS_MESH && POTS_VOICE
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <string.h>
#include <esp_heap_caps.h>
#include "mbedtls/sha256.h"
#include "mbedtls_compat.h"
#include "detection.h"
#include "meshtalk.h"
#include "meshcrypto.h"
#include "meshframe_v2.h"
#include "meshvoice_v2.h"
#include "voice.h"

extern DetectionEngine engine;

namespace PusimeshBle {

static const char* SVC_UUID  = "7a1c5e10-4b9d-4f3a-8c22-9e6f0d1a2b00";
static const char* IN_UUID   = "7a1c5e10-4b9d-4f3a-8c22-9e6f0d1a2b01";
static const char* OUT_UUID  = "7a1c5e10-4b9d-4f3a-8c22-9e6f0d1a2b02";
static const char* CTRL_UUID = "7a1c5e10-4b9d-4f3a-8c22-9e6f0d1a2b03";

// The control byte the phone writes first on ctrl, followed by its nodeId.
static const uint8_t CTRL_ID = 0x49;   // 'I'

static NimBLEServer*          s_server = nullptr;
static NimBLECharacteristic*  s_in     = nullptr;
static NimBLECharacteristic*  s_out    = nullptr;
static NimBLECharacteristic*  s_ctrl   = nullptr;
static NimBLEService*         s_svc    = nullptr;

static bool     s_active = false;
static volatile bool s_connected = false;
static uint16_t s_conn = 0xFFFF;
static uint16_t s_mtu  = 23;

static uint8_t  s_peerId[PusiMesh2::NODE_ID_LEN] = { 0 };
static bool     s_peerSet = false;

// ---- the receive side ------------------------------------------------------
// The host task copies complete frames here; tick() opens them on the loop
// task, where the cipher and the codec are already used.
struct RxSlot { uint16_t len; uint8_t data[PusiMesh2::BIG_FRAME_MAX]; };
static RxSlot            s_q[4];
static volatile uint8_t  s_qHead = 0, s_qTail = 0;

// The reassembly buffer and its state. PSRAM: a clip is up to 32 KB.
static uint8_t*  s_rxBuf = nullptr;
static bool      s_rxValid = false;
static PusiMesh2::Voice::Manifest s_rxMan = {};
static uint8_t   s_rxBitmap[16] = {};
static uint16_t  s_rxHave = 0;   // how many distinct chunks have landed

// ---- the send side ---------------------------------------------------------
static uint32_t  s_ctr = 0x5A17u;
static bool      s_txOn = false;
static uint32_t  s_txId = 0;
static uint16_t  s_txSeq = 0, s_txCount = 0;
static uint8_t   s_txFrame[PusiMesh2::BIG_FRAME_MAX];

// ---- frame I/O -------------------------------------------------------------
// Both directions carry the whole frame length in a two-byte LE prefix on
// every write, so a frame split across writes can be reassembled and a
// retried prefix simply restarts the one in progress.
static uint16_t s_inTotal = 0, s_inGot = 0;
static uint8_t  s_inBuf[PusiMesh2::BIG_FRAME_MAX];

static void pushFrame(const uint8_t* d, size_t n) {
    if (n == 0 || n > sizeof s_inBuf) return;
    const uint8_t next = (uint8_t)((s_qHead + 1) % 4);
    if (next == s_qTail) return;                 // full: drop, the sender retries
    memcpy(s_q[s_qHead].data, d, n);
    s_q[s_qHead].len = (uint16_t)n;
    s_qHead = next;
}

class InCb : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
        NimBLEAttValue v = c->getValue();
        const uint8_t* p = v.data();
        const size_t   n = v.size();
        if (n < 3) return;
        const uint16_t total = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
        if (total == 0 || total > sizeof s_inBuf) return;
        if (total != s_inTotal) { s_inTotal = total; s_inGot = 0; }
        size_t take = n - 2;
        if (s_inGot + take > s_inTotal) take = s_inTotal - s_inGot;
        memcpy(s_inBuf + s_inGot, p + 2, take);
        s_inGot = (uint16_t)(s_inGot + take);
        if (s_inGot == s_inTotal) {
            pushFrame(s_inBuf, s_inTotal);
            s_inTotal = 0; s_inGot = 0;
        }
    }
};

static void sendFrame(const uint8_t* f, size_t n) {
    if (!s_connected || !s_out || !n) return;
    const size_t chunk = s_mtu > 5 ? (size_t)(s_mtu - 5) : 18;   // MTU-3, less the prefix
    static uint8_t out[2 + 520];
    size_t off = 0;
    while (off < n) {
        size_t take = n - off;
        if (take > chunk) take = chunk;
        out[0] = (uint8_t)(n & 0xFF);
        out[1] = (uint8_t)(n >> 8);
        memcpy(out + 2, f + off, take);
        s_out->notify(out, 2 + take, s_conn);
        off += take;
    }
}

class ServerCb : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* srv, NimBLEConnInfo& info) override {
        s_conn      = info.getConnHandle();
        s_connected = true;
        s_mtu       = srv->getPeerMTU(s_conn);
        s_inTotal = s_inGot = 0;
        // Tell the phone who we are, so it can verify the frames we seal.
        uint8_t hello[1 + PusiMesh2::NODE_ID_LEN];
        hello[0] = CTRL_ID;
        memcpy(hello + 1, Mesh::nodeId(), PusiMesh2::NODE_ID_LEN);
        if (s_ctrl) s_ctrl->notify(hello, sizeof hello, s_conn);
        Serial.printf("[pusimesh-ble] phone connected, mtu %u\n", (unsigned)s_mtu);
    }
    void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason) override {
        s_connected = false;
        s_conn = 0xFFFF;
        s_inTotal = s_inGot = 0;
        s_txOn = false;
        Serial.printf("[pusimesh-ble] phone left (reason %d)\n", reason);
    }
};

class CtrlCb : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
        NimBLEAttValue v = c->getValue();
        if (v.size() >= 1 + PusiMesh2::NODE_ID_LEN && v.data()[0] == CTRL_ID) {
            memcpy(s_peerId, v.data() + 1, PusiMesh2::NODE_ID_LEN);
            s_peerSet = true;
            Serial.printf("[pusimesh-ble] peer nodeId %02x%02x%02x%02x\n",
                          s_peerId[0], s_peerId[1], s_peerId[2], s_peerId[3]);
        }
    }
};

static InCb   s_inCb;
static ServerCb s_srvCb;
static CtrlCb s_ctrlCb;

// ---- voice session ---------------------------------------------------------
static size_t sealExt(uint8_t sub, const uint8_t* payload, size_t len, uint8_t* out) {
    if (!MeshTalk::ready()) return 0;
    return PusiMesh2::sealExtBig(MeshCrypto::impl(), Mesh::nodeId(), s_ctr++,
                                 sub, payload, len, out, PusiMesh2::BIG_FRAME_MAX);
}

static bool allChunksIn() {
    if (!s_rxValid || s_rxMan.chunkCount == 0) return false;
    for (uint16_t i = 0; i < s_rxMan.chunkCount; i++)
        if (!PusiMesh2::Voice::ackBit(s_rxBitmap, sizeof s_rxBitmap, i)) return false;
    return true;
}

static void handleVoiceFrame(const uint8_t* frame, size_t len) {
    if (!s_peerSet || !MeshTalk::ready()) return;
    uint8_t sub = 0; size_t plen = 0;
    uint8_t payload[PusiMesh2::BIG_PT_MAX];
    uint32_t ctr = 0;
    if (PusiMesh2::openExtBig(MeshCrypto::impl(), s_peerId, frame, len, ctr,
                              sub, payload, sizeof payload, plen) != PusiMesh2::Open::OK)
        return;

    using namespace PusiMesh2::Voice;
    if (sub == PusiMesh2::S_VOICE_MANIFEST) {
        Manifest m;
        if (!decodeManifest(payload, plen, m)) return;
        if (m.codec != CODEC_OPUS_16K || m.size == 0 || m.size > Voice::BLOB_MAX ||
            m.chunkCount == 0 || m.chunkCount > sizeof s_rxBitmap * 8) return;
        s_rxMan = m;
        memset(s_rxBitmap, 0, sizeof s_rxBitmap);
        s_rxHave = 0;
        s_rxValid = true;
        Serial.printf("[pusimesh-ble] voice in: %lu bytes, %u ms, %u chunks\n",
                      (unsigned long)m.size, (unsigned)m.durMs, (unsigned)m.chunkCount);
        return;
    }
    if (sub == PusiMesh2::S_BLOB_CHUNK) {
        uint32_t id = 0; uint16_t seq = 0; const uint8_t* data = nullptr; size_t dlen = 0;
        if (!decodeChunk(payload, plen, id, seq, data, dlen)) return;
        if (!s_rxValid || id != s_rxMan.blobId || seq >= s_rxMan.chunkCount) return;
        const size_t off = (size_t)seq * CHUNK_DATA_MAX;
        if (off + dlen > Voice::BLOB_MAX || off + dlen > s_rxMan.size) return;
        memcpy(s_rxBuf + off, data, dlen);
        if (!ackBit(s_rxBitmap, sizeof s_rxBitmap, seq)) {
            ackSet(s_rxBitmap, sizeof s_rxBitmap, seq);
            s_rxHave++;
        }
        if (allChunksIn()) {
            uint8_t full[32];
            mbedtls_sha256_context sc;
            mbedtls_sha256_init(&sc);
            mbedtls_sha256_starts(&sc, 0);
            mbedtls_sha256_update(&sc, s_rxBuf, s_rxMan.size);
            mbedtls_sha256_finish(&sc, full);
            mbedtls_sha256_free(&sc);
            if (memcmp(full, s_rxMan.sha8, 8) == 0) {
                Serial.printf("[pusimesh-ble] voice complete, playing %u ms\n", (unsigned)s_rxMan.durMs);
                Voice::play(s_rxBuf, s_rxMan.size, s_rxMan.durMs);
            } else {
                Serial.println("[pusimesh-ble] voice sha8 mismatch, dropped");
            }
            s_rxValid = false;
        }
        return;
    }
    // ACKs and anything newer are ignored: the sender paced itself and a
    // failed chunk is re-fetched on the next clip, not mid-stream.
}

static void drainRx() {
    while (s_qTail != s_qHead) {
        const RxSlot& s = s_q[s_qTail];
        handleVoiceFrame(s.data, s.len);
        s_qTail = (uint8_t)((s_qTail + 1) % 4);
    }
}

// ---- the public surface ----------------------------------------------------
bool begin() {
    if (s_active) return true;
    if (!Voice::ready()) return false;

    engine.startUpdateRadio();
    NimBLEScan* scan = NimBLEDevice::getScan();
    const uint32_t t0 = millis();
    while (scan && scan->isScanning() && millis() - t0 < 2000) delay(10);

    if (!s_server) {
        NimBLEDevice::setMTU(517);
        s_server = NimBLEDevice::createServer();
        s_server->setCallbacks(&s_srvCb, false);
        s_server->advertiseOnDisconnect(false);
        s_svc  = s_server->createService(SVC_UUID);
        s_in   = s_svc->createCharacteristic(IN_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR, PusiMesh2::BIG_FRAME_MAX);
        s_out  = s_svc->createCharacteristic(OUT_UUID, NIMBLE_PROPERTY::NOTIFY, PusiMesh2::BIG_FRAME_MAX);
        s_ctrl = s_svc->createCharacteristic(CTRL_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY, 32);
        s_in->setCallbacks(&s_inCb);
        s_ctrl->setCallbacks(&s_ctrlCb);
        s_svc->start();
        s_server->start();
    }

    if (!s_rxBuf) {
        s_rxBuf = (uint8_t*)heap_caps_malloc(Voice::BLOB_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_rxBuf) s_rxBuf = (uint8_t*)heap_caps_malloc(Voice::BLOB_MAX, MALLOC_CAP_8BIT);
    }
    if (!s_rxBuf) { engine.stopUpdateRadio(); return false; }

    s_ctr = (esp_random() & 0x7FFFFFFF) | 0x10000000u;
    s_rxValid = false;
    s_peerSet = false;
    s_txOn = false;
    s_inTotal = s_inGot = 0;
    s_qHead = s_qTail = 0;

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->stop();
    const NimBLEAddress a = NimBLEDevice::getAddress();
    char name[24];
    snprintf(name, sizeof name, "PusiMesh-%02X%02X", a.getBase()->val[1], a.getBase()->val[0]);
    NimBLEAdvertisementData d;
    d.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    d.setCompleteServices(NimBLEUUID(SVC_UUID));
    adv->setAdvertisementData(d);
    NimBLEAdvertisementData r;
    r.setName(name);
    adv->setScanResponseData(r);
    adv->enableScanResponse(true);
    adv->setConnectableMode(BLE_GAP_CONN_MODE_UND);
    adv->setMinInterval(48);
    adv->setMaxInterval(96);
    adv->start();

    s_active = true;
    Serial.printf("[pusimesh-ble] GATT up as %s, heap %lu\n", name, (unsigned long)ESP.getFreeHeap());
    return true;
}

void end() {
    if (!s_active) return;
    s_active = false;
    s_txOn = false;
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    if (adv) adv->stop();
    if (s_connected && s_server) s_server->disconnect(s_conn);
    s_connected = false;
    engine.stopUpdateRadio();
    Serial.println("[pusimesh-ble] GATT down");
}

bool active()   { return s_active; }
bool connected(){ return s_connected; }
bool peerReady(){ return s_peerSet; }
bool sending()  { return s_txOn; }

bool ownId(uint8_t out[PusiMesh2::NODE_ID_LEN]) {
    if (!Mesh::nodeId()) return false;
    memcpy(out, Mesh::nodeId(), PusiMesh2::NODE_ID_LEN);
    return true;
}

bool sendClip() {
    if (!s_active || !s_connected || !MeshTalk::ready()) return false;
    const size_t n = Voice::blobLen();
    if (n == 0 || n > Voice::BLOB_MAX) return false;

    PusiMesh2::Voice::Manifest m;
    m.blobId = s_ctr++;
    m.codec  = Voice::CODEC_OPUS_16K;
    m.rateK  = Voice::RATE_KHZ;
    m.durMs  = Voice::blobMs();
    m.size   = (uint32_t)n;
    memcpy(m.sha8, Voice::blobSha8(), 8);
    PusiMesh2::Voice::manifestFinish(m);

    uint8_t pl[64];
    const size_t pn = PusiMesh2::Voice::encodeManifest(m, pl, sizeof pl);
    if (pn == 0) return false;
    const size_t fn = sealExt(PusiMesh2::S_VOICE_MANIFEST, pl, pn, s_txFrame);
    if (fn == 0) return false;

    s_txId    = m.blobId;
    s_txCount = m.chunkCount;
    s_txSeq   = 0;
    s_txOn    = true;
    sendFrame(s_txFrame, fn);
    Serial.printf("[pusimesh-ble] voice out: %lu bytes, %u ms, %u chunks\n",
                  (unsigned long)n, (unsigned)m.durMs, (unsigned)m.chunkCount);
    return true;
}

void tick(uint32_t now) {
    if (Voice::playing()) Voice::playPump();
    if (!s_active) return;
    drainRx();
    if (!s_connected) return;
    if (s_txOn) {
        for (int burst = 0; burst < 4 && s_txSeq < s_txCount; burst++) {
            const uint8_t* blob = Voice::blob();
            const size_t off = (size_t)s_txSeq * PusiMesh2::Voice::CHUNK_DATA_MAX;
            const size_t total = Voice::blobLen();
            if (off >= total) { s_txOn = false; break; }
            size_t take = total - off;
            if (take > PusiMesh2::Voice::CHUNK_DATA_MAX) take = PusiMesh2::Voice::CHUNK_DATA_MAX;
            uint8_t pl[PusiMesh2::Voice::CHUNK_HDR + PusiMesh2::Voice::CHUNK_DATA_MAX + 2];
            const size_t pn = PusiMesh2::Voice::encodeChunk(s_txId, s_txSeq, blob + off, take, pl, sizeof pl);
            if (pn == 0) { s_txOn = false; break; }
            const size_t fn = sealExt(PusiMesh2::S_BLOB_CHUNK, pl, pn, s_txFrame);
            if (fn == 0) { s_txOn = false; break; }
            sendFrame(s_txFrame, fn);
            s_txSeq++;
        }
        if (s_txSeq >= s_txCount) s_txOn = false;
    }
}

}  // namespace PusimeshBle
#endif
