// PusiMesh over BLE-GATT — the bearer a voice message travels on (spec VOICE
// §3). A 27-byte advert cannot carry a clip, so while the voice screen is open
// the board stops scanning and present-advertising and puts up a connectable
// GATT service instead; the phone connects as the central. The bytes are the
// same sealed PusiMesh big frame the rest of the mesh speaks, so the GATT link
// is only a pipe, not a second protocol.
//
// Roles: the board is the peripheral/server (its peripheral role is already
// compiled in for BLE-OTA); the phone is the central/client.
#pragma once
#include <stdint.h>
#include <stddef.h>

#if POTS_MESH
namespace PusimeshBle {

// Enter the session: stop the scan and the presence advert, register the
// service and start connectable advertising. False when the board has no
// voice engine or the radio would not come up.
bool begin();
// Leave: drop the link, stop advertising, hand the radio back to detection.
void end();
bool active();
bool connected();
// The peer has told us its nodeId, so its sealed frames can be opened.
bool peerReady();

// Pump: play received audio, drain the frames the host task copied, and pace
// the outgoing clip. Call every loop while active.
void tick(uint32_t now);

// Send the clip Voice has just recorded. False when not connected, not ready,
// or there is nothing recorded.
bool sendClip();
bool sending();

// What the phone calls us, so it can verify our frames. True once known.
bool ownId(uint8_t out[8]);

}  // namespace PusimeshBle
#endif
