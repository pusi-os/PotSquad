// PotsSquad-CYD — PusiMesh over ESP-NOW: the second transport.
//
// The squad has always ridden BLE (detection.cpp's scanner doubles as the
// mesh listener, promote the same adverts). ESP-NOW is the other radio the
// ESP32 has: it is WiFi, so it reaches further and answers each frame with a
// link-layer ACK, but the WiFi radio takes the antenna, so while it is up the
// BLE scan has to stand down -- and with it detection. That is the trade the
// menu warns about before it lets you switch.
//
// The wire is the same on both radios: a 'BN' header in front of the same
// company+advert and sealed-frame bytes the BLE path already parses. Kind 0
// carries a presence payload (company 0xFFFF + buildSelf, fed straight into
// Mesh::onManufacturerData so the roster, names and visits are unchanged);
// kind 1 carries one sealed MeshMsg frame. Nothing above this file knows
// which radio it is on.
//
// THREADING. The ESP-NOW receive callback runs on the WiFi task and does
// nothing but copy bytes into a small ring. Parsing, decryption and relay all
// happen in tick(), on the loop task, where the cipher already lives -- the
// same discipline the BLE callback follows (never work in the callback; the
// only thing either one touches is a ring the loop drains).
#pragma once
#if POTS_MESH
#include <stdint.h>
#include <stddef.h>

namespace PusiMeshEsp {

// Packet: [0]='B', [1]='N', [2]=ver(1), [3]=kind, [4..]=payload.
// Matches BroWatch's ESP-NOW transport so the two squads can share one air.
static const uint8_t  T_MAGIC0 = 'B';
static const uint8_t  T_MAGIC1 = 'N';
static const uint8_t  T_VER    = 1;
enum TKind : uint8_t { T_PRESENCE = 0, T_SEALED = 1 };
static const size_t   T_HDR    = 4;
// Channel 6, BroWatch's squad channel: one fixed channel both ends park on, so
// a receiver never has to hunt.
static const uint8_t  CHANNEL  = 6;

bool active();          // ESP-NOW is up and owns the squad radio
bool begin();           // STA + fixed channel + ESP-NOW up. False = refused.
void end();             // ESP-NOW down. Idempotent.
void tick(uint32_t now);// drain RX, send presence and any outgoing frame
// Re-assert the squad channel without touching anything else: a raw WiFi scan
// or an OTA join may have parked the radio elsewhere while ESP-NOW stayed up.
void reassert();

uint32_t rxCount();
uint32_t txCount();
uint32_t dropCount();   // RX ring overruns + rejected sends

// This device's own address, as ESP-NOW sees it (the STA MAC). All zeros until
// begin().
const uint8_t* ownMac();

} // namespace PusiMeshEsp
#endif // POTS_MESH
