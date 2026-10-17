// PotsSquad-CYD — PusiMesh over ESP-NOW. See include/pusimesh_espnow.h.
#include "pusimesh_espnow.h"

#if POTS_MESH

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>

#include "detection.h"     // Mesh::buildSelf / Mesh::onManufacturerData, PusiMesh
#include "settings.h"
#include "meshtalk.h"
#include "meshmsg.h"

namespace PusiMeshEsp {

static const uint32_t PRESENCE_MS = 1500;   // same cadence as the BLE advert

static bool     s_active    = false;
static uint32_t s_presAt    = 0;
static uint32_t s_sealedGen = 0;
static uint8_t  s_ownMac[6] = { 0 };
static uint8_t  s_lastPres[PusiMesh::LEN_MAX];
static size_t   s_lastPresLen = 0;
static uint32_t s_rx = 0, s_tx = 0, s_drop = 0, s_ackFail = 0;

static const uint8_t BROADCAST[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// RX ring: written by the WiFi task, drained by tick() on the loop task.
// Single producer, single consumer; head/tail as bytes so a torn read is
// impossible on this core. 8 slots is a storm's worth at presence cadence.
static const uint8_t RING_N = 8;
struct Slot { uint8_t mac[6]; uint16_t len; uint8_t data[256]; };
static Slot    s_ring[RING_N];
static volatile uint8_t s_head = 0;   // WiFi task writes here
static volatile uint8_t s_tail = 0;   // loop task reads here

static void onRecv(const uint8_t* mac, const uint8_t* data, int len) {
    if (!mac || !data || len <= 0) return;
    const uint8_t h = s_head;
    const uint8_t n = (uint8_t)((h + 1) % RING_N);
    if (n == s_tail) { s_drop++; return; }   // full: count and drop
    Slot& s = s_ring[h];
    memcpy(s.mac, mac, 6);
    s.len = (uint16_t)(len > 256 ? 256 : len);
    memcpy(s.data, data, s.len);
    s_head = n;
}

static void onSent(const uint8_t* /*mac*/, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) s_ackFail++;
}

bool active()     { return s_active; }
uint32_t rxCount()   { return s_rx; }
uint32_t txCount()   { return s_tx; }
uint32_t dropCount() { return s_drop + s_ackFail; }
const uint8_t* ownMac() { return s_ownMac; }

bool begin() {
    if (s_active) return true;
    // STA without ever connecting: ESP-NOW needs the interface up and on a
    // channel, nothing more. Disconnect first so a stale OTA join cannot hold
    // us on somebody else's channel -- the known silent-break, where TX reports
    // success and nobody hears a thing.
    //
    // NOTE: modem-sleep stays ON. esp_wifi_set_ps(WIFI_PS_NONE) aborts the WiFi
    // task on this IDF while the Bluetooth controller is enabled, and NimBLE
    // stays initialized under ESP-NOW so the way back to BLE is a switch, not a
    // reboot. An unassociated STA has no beacons to sleep on, and the wake
    // window below keeps the small TX pool from wedging.
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_max_tx_power(78);                    // 19.5 dBm, the long-range job
    if (esp_now_init() != ESP_OK) {
        Serial.println("[espnow] esp_now_init failed");
        return false;
    }
    esp_now_register_recv_cb(onRecv);
    esp_now_register_send_cb(onSent);
    esp_now_set_wake_window(0xFFFF);                  // ESP32: keep the TX pool awake
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, BROADCAST, 6);
    peer.channel = CHANNEL;
    peer.encrypt = false;                             // frames are AES-CCM inside already
    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("[espnow] broadcast peer refused");
        esp_now_deinit();
        return false;
    }
    esp_wifi_get_mac(WIFI_IF_STA, s_ownMac);
    MeshTalk::setOwnMac(s_ownMac);                    // the nonce binds frames to the sender
    s_head = s_tail = 0;
    s_presAt = 0;                                     // presence goes out on the next tick
    s_sealedGen = 0;
    s_lastPresLen = 0;
    s_active = true;
    Serial.printf("[espnow] up on ch %u, mac %02X:%02X:%02X:%02X:%02X:%02X\n",
                  CHANNEL, s_ownMac[0], s_ownMac[1], s_ownMac[2],
                  s_ownMac[3], s_ownMac[4], s_ownMac[5]);
    return true;
}

void end() {
    if (!s_active) return;
    s_active = false;
    esp_now_deinit();
    s_head = s_tail = 0;
    Serial.println("[espnow] down");
}

// One transport packet out. False when the radio refused it (counted, never
// retried: the presence cadence and MeshTalk's own rotation re-send).
static bool sendPacket(TKind kind, const uint8_t* payload, size_t len) {
    if (len > 250 - T_HDR) { s_drop++; return false; }
    uint8_t buf[250];
    buf[0] = T_MAGIC0;
    buf[1] = T_MAGIC1;
    buf[2] = T_VER;
    buf[3] = (uint8_t)kind;
    memcpy(buf + T_HDR, payload, len);
    const esp_err_t e = esp_now_send(BROADCAST, buf, T_HDR + len);
    if (e != ESP_OK) { s_drop++; return false; }
    s_tx++;
    return true;
}

static void drainRx(uint32_t now) {
    while (s_tail != s_head) {
        Slot& s = s_ring[s_tail];
        s_tail = (uint8_t)((s_tail + 1) % RING_N);
        s_rx++;
        if (s.len < T_HDR + 2) continue;
        if (s.data[0] != T_MAGIC0 || s.data[1] != T_MAGIC1 || s.data[2] != T_VER) continue;
        const uint8_t* pl = s.data + T_HDR;
        const size_t   plLen = s.len - T_HDR;
        if (s.data[3] == T_PRESENCE) {
            // company[2]+buildSelf: exactly what the BLE scan hands over.
            Mesh::onManufacturerData(pl, plLen, s.mac, now);
        } else if (s.data[3] == T_SEALED) {
            // Rebuild the company header the isFrame branch expects, then take
            // the same road every BLE message frame walks: name cache,
            // messagesOn gate, replay filter, decrypt on this task. FRAME_MAX
            // (a sealed message) is the bound, not LEN_MAX -- a typed part is
            // 27 bytes and would otherwise be dropped.
            if (plLen <= MeshMsg::FRAME_MAX && Settings::messagesOn()) {
                uint8_t tmp[2 + MeshMsg::FRAME_MAX];
                tmp[0] = (uint8_t)(PusiMesh::COMPANY_ID & 0xFF);
                tmp[1] = (uint8_t)(PusiMesh::COMPANY_ID >> 8);
                memcpy(tmp + 2, pl, plLen);
                Mesh::onManufacturerData(tmp, plLen + 2, s.mac, now);
            }
        }
    }
}

void reassert() {
    if (!s_active) return;
    uint8_t ch = 0;
    wifi_second_chan_t sch = WIFI_SECOND_CHAN_NONE;
    if (esp_wifi_get_channel(&ch, &sch) == ESP_OK && ch != CHANNEL)
        esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
}

void tick(uint32_t now) {
    if (!s_active) return;
    // Re-assert the channel: any WiFi join (OTA, NTP) parks the radio on
    // somebody else's channel, and ESP-NOW then fails silently.
    reassert();
    drainRx(now);

    if (!Settings::meshTransmit()) return;
    // Presence: who we are, every 1.5 s, sent unconditionally (unlike BLE
    // there is no advertising interval to ride), but rebuilt only when the
    // appearance changed so the heap is not churned one rebuild at a time.
    if (!s_presAt || (now - s_presAt) >= PRESENCE_MS) {
        s_presAt = now;
        uint8_t self[PusiMesh::LEN_MAX];
        const size_t n = Mesh::buildSelf(self);
        if (n != s_lastPresLen || memcmp(self, s_lastPres, n) != 0) {
            memcpy(s_lastPres, self, n);
            s_lastPresLen = n;
        }
        uint8_t pl[2 + PusiMesh::LEN_MAX];
        pl[0] = (uint8_t)(PusiMesh::COMPANY_ID & 0xFF);
        pl[1] = (uint8_t)(PusiMesh::COMPANY_ID >> 8);
        memcpy(pl + 2, s_lastPres, s_lastPresLen);
        sendPacket(T_PRESENCE, pl, 2 + s_lastPresLen);
    }
    // The sealed ferry: whatever MeshTalk::outgoing() carries right now, once
    // per generation change -- the same part rotation the BLE scan response
    // makes, since the advertiser is off while ESP-NOW owns the radio.
    size_t   ol = 0;
    uint32_t og = 0;
    const uint8_t* out = MeshTalk::outgoing(now, ol, og);
    if (out && ol && ol <= MeshMsg::FRAME_MAX && og != s_sealedGen) {
        s_sealedGen = og;
        sendPacket(T_SEALED, out, ol);
    }
}

} // namespace PusiMeshEsp
#endif // POTS_MESH
