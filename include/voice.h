// PotsSquad — the voice recorder/player, on the boards that have a mic and a
// speaker (the Xiaozhi-style cube and zhengchen: an I2S mic and an I2S amp,
// no codec chip — NoAudioCodecSimplex wiring). Everywhere else this header is
// a set of no-ops, so the mesh and the UI can ask without a #if of their own.
//
// A voice message is Opus, 16 kHz mono, 60 ms frames, the same profile Xiaozhi
// speaks. Recording streams the mic through the encoder into a PSRAM buffer;
// playing streams the buffer back through the decoder into the amp one frame
// at a time, so a clip never needs its PCM in RAM all at once. Nothing here
// knows about the radio: it fills and drains a byte buffer, and meshtalk hands
// that buffer to the wire.
#pragma once
#include <stdint.h>
#include <stddef.h>

#if defined(POTS_VOICE)
#define SQW_VOICE 1
#else
#define SQW_VOICE 0
#endif

namespace Voice {

// Opus 16 kHz VOIP, the defaults a clip is sent under (meshvoice_v2 Codec).
static const uint8_t  CODEC_OPUS_16K = 1;
static const uint8_t  RATE_KHZ       = 16;
static const uint16_t FRAME_MS       = 60;
static const uint16_t MAX_MS         = 10000;   // the cap: ten seconds
// Worst-case Opus at this bitrate plus framing, rounded up. 10 s at ~24 kbps is
// 30 KB; the buffer is 32 KB so a clip can never overflow it mid-record.
static const size_t   BLOB_MAX       = 32 * 1024;

#if SQW_VOICE
// Once, before the mesh is up. Brings up both I2S directions and the codec.
void begin();
// Whether the codec came up; the UI reads it to offer the voice screen at all.
bool ready();

// ---- recording -------------------------------------------------------------
void recStart(uint32_t now);
// Call every loop while recording: pulls whatever the mic has, encodes whole
// frames, and returns false once the cap is hit (the caller ends the clip).
bool recPump(uint32_t now);
bool recActive();
// Milliseconds recorded so far, for the countdown.
uint16_t recMs(uint32_t now);
// Finish the clip (pads a short last frame) and stop. Returns the encoded
// length, 0 if nothing was captured.
size_t recStop();

// ---- the finished clip -----------------------------------------------------
const uint8_t* blob();
size_t         blobLen();
uint16_t       blobMs();
// The first 8 bytes of SHA-256 over the clip, for the manifest.
const uint8_t* blobSha8();
void           blobClear();

// ---- playing ---------------------------------------------------------------
// Decode and play a clip. `durMs` sets the nominal length for the progress the
// UI draws; a zero length does nothing.
void play(const uint8_t* opus, size_t len, uint16_t durMs);
void playPump();          // call every loop while playing()
bool playing();
void stop();

// ---- the bench -------------------------------------------------------------
// Record `ms` and play it straight back, for a wiring check. Returns false if
// the codec is not up.
bool selfTest(uint32_t now);
#else
inline void begin() {}
inline bool ready() { return false; }
inline void recStart(uint32_t) {}
inline bool recPump(uint32_t) { return false; }
inline bool recActive() { return false; }
inline uint16_t recMs(uint32_t) { return 0; }
inline size_t recStop() { return 0; }
inline const uint8_t* blob() { return nullptr; }
inline size_t blobLen() { return 0; }
inline uint16_t blobMs() { return 0; }
inline const uint8_t* blobSha8() { return nullptr; }
inline void blobClear() {}
inline void play(const uint8_t*, size_t, uint16_t) {}
inline void playPump() {}
inline bool playing() { return false; }
inline void stop() {}
inline bool selfTest(uint32_t) { return false; }
#endif

} // namespace Voice
