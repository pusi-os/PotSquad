// The voice gesture on the DESK screen: hold BOOT to record (a countdown in
// the middle of the clock), release to finish, then tap to send or hold to
// cancel. The transport is PusimeshBle (a connectable GATT link); the clip is
// Voice. This module owns only the gesture and the overlay.
#pragma once
#include <stdint.h>

class TFT_eSPI;

#if POTS_VOICE
namespace UiVoice {

// True while the gesture owns BOOT, so the button mapper does not read a
// long hold as "go home" under it.
bool holding();
void press(uint32_t now);
void release(uint32_t now);
// Draw the overlay (after the desk) and pump the recorder, the player and the
// GATT session.
void tick(TFT_eSPI& t, uint32_t now);
// Leaving the desk: stop anything in flight and hand the radio back.
void abort();
}  // namespace UiVoice
#else
namespace UiVoice {
inline bool holding() { return false; }
inline void press(uint32_t) {}
inline void release(uint32_t) {}
inline void tick(TFT_eSPI&, uint32_t) {}
inline void abort() {}
}
#endif
