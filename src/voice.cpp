// PotsSquad — the voice recorder/player. See include/voice.h.
//
// The cubic boards carry a bare I2S microphone and a bare I2S amplifier (no
// codec chip), so both directions are the legacy I2S driver, the same one
// sound.cpp uses on the Freenove boards. The clip is Opus at the Xiaozhi
// profile — 16 kHz, mono, 60 ms frames — encoded straight from the mic into a
// PSRAM buffer and decoded frame by frame on the way out, so the PCM of a
// whole message is never in RAM.
//
// The clip is a sequence of [u16 len][opus packet], little-endian. Raw Opus
// packets are not self-delimiting and the wire has no container, so one length
// per frame is what makes the blob walkable; it is also what lets the receiver
// resume on a byte offset without re-deriving frame sizes.
#include "voice.h"

#if defined(POTS_VOICE)
#include <Arduino.h>
#include <driver/i2s.h>
#include <opus.h>
#include <string.h>
#include "mbedtls/sha256.h"
#include "mbedtls_compat.h"

#if !defined(VOICE_MIC_SCK)
#error "POTS_VOICE needs the audio pins (see the board's user_setup header)"
#endif

namespace Voice {

static const i2s_port_t PORT_SPK = I2S_NUM_0;
static const i2s_port_t PORT_MIC = I2S_NUM_1;
static const int        RATE     = RATE_KHZ * 1000;
static const int        FRAME_N  = RATE * FRAME_MS / 1000;   // 960 samples

static OpusEncoder* s_enc = nullptr;
static OpusDecoder* s_dec = nullptr;
static bool         s_up  = false;
static bool         s_inited = false;

static uint8_t* s_blob = nullptr;       // PSRAM, BLOB_MAX
static size_t   s_blobLen = 0;
static uint8_t  s_sha8[8] = {};

// Recording staging: the mic hands over whatever it has, the encoder wants
// whole 960-sample frames, so a short carry lives here across loop passes.
static int16_t  s_stage[FRAME_N];
static int      s_stageN = 0;
static bool     s_rec = false;
static uint32_t s_recStart = 0;
static uint16_t s_recMs = 0;

// Playback: the clip pointer, the byte cursor, and the nominal length the UI
// counts against.
static const uint8_t* s_playData = nullptr;
static size_t   s_playLen = 0;
static size_t   s_playPos = 0;

static void i2sTx() {
    i2s_config_t cfg = {};
    cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate          = RATE;
    cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count        = 6;
    cfg.dma_buf_len          = 240;
    cfg.use_apll             = false;
    cfg.tx_desc_auto_clear   = true;
    cfg.fixed_mclk           = 0;
    i2s_driver_install(PORT_SPK, &cfg, 0, nullptr);
    i2s_pin_config_t pins = {};
    pins.bck_io_num   = VOICE_SPK_BCLK;
    pins.ws_io_num    = VOICE_SPK_WS;
    pins.data_out_num = VOICE_SPK_DOUT;
    pins.data_in_num  = I2S_PIN_NO_CHANGE;
    i2s_set_pin(PORT_SPK, &pins);
    i2s_zero_dma_buffer(PORT_SPK);
}

static void i2sRx() {
    i2s_config_t cfg = {};
    cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    cfg.sample_rate          = RATE;
    cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count        = 6;
    cfg.dma_buf_len          = 240;
    cfg.use_apll             = false;
    i2s_driver_install(PORT_MIC, &cfg, 0, nullptr);
    i2s_pin_config_t pins = {};
    pins.bck_io_num   = VOICE_MIC_SCK;
    pins.ws_io_num    = VOICE_MIC_WS;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num  = VOICE_MIC_DIN;
    i2s_set_pin(PORT_MIC, &pins);
    i2s_zero_dma_buffer(PORT_MIC);
}

void begin() {
    if (s_inited) return;
    s_inited = true;
    i2sTx();
    i2sRx();

    int err = 0;
    s_enc = opus_encoder_create(RATE, 1, OPUS_APPLICATION_VOIP, &err);
    if (err != OPUS_OK || !s_enc) { Serial.printf("[voice] enc fail %d\n", err); s_enc = nullptr; }
    else {
        opus_encoder_ctl(s_enc, OPUS_SET_BITRATE(16000));
        opus_encoder_ctl(s_enc, OPUS_SET_COMPLEXITY(1));   // S3 has no headroom to spare
        opus_encoder_ctl(s_enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    }
    s_dec = opus_decoder_create(RATE, 1, &err);
    if (err != OPUS_OK || !s_dec) { Serial.printf("[voice] dec fail %d\n", err); s_dec = nullptr; }

    s_blob = (uint8_t*)heap_caps_malloc(BLOB_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_blob) s_blob = (uint8_t*)malloc(BLOB_MAX);

    s_up = s_enc && s_dec && s_blob;
    Serial.printf("[voice] up=%d blob=%p\n", (int)s_up, s_blob);
}

bool ready() { return s_up; }

// ---- recording -------------------------------------------------------------

void recStart(uint32_t now) {
    if (!s_up) return;
    s_blobLen = 0;
    s_stageN  = 0;
    s_rec     = true;
    s_recMs   = 0;
    s_recStart = now;
    i2s_zero_dma_buffer(PORT_MIC);   // drop whatever sat in the mic's DMA
}

// Encode the staged frame into the blob. Returns false if the blob is full.
static bool encodeStage() {
    if (s_stageN < FRAME_N) return true;
    int16_t pcm[FRAME_N];
    memcpy(pcm, s_stage, sizeof pcm);
    s_stageN = 0;
    uint8_t out[512];
    const int n = opus_encode(s_enc, pcm, FRAME_N, out, sizeof out);
    if (n <= 0) return true;                       // a dropped frame beats a stall
    if (s_blobLen + 2 + (size_t)n > BLOB_MAX) return false;
    s_blob[s_blobLen++] = (uint8_t)(n & 0xFF);
    s_blob[s_blobLen++] = (uint8_t)((n >> 8) & 0xFF);
    memcpy(s_blob + s_blobLen, out, (size_t)n);
    s_blobLen += (size_t)n;
    return true;
}

bool recPump(uint32_t now) {
    if (!s_rec) return false;
    int16_t buf[256];
    size_t got = 0;
    while (i2s_read(PORT_MIC, buf, sizeof buf, &got, 0) == ESP_OK && got >= 2) {
        const int n = (int)(got / 2);
        for (int i = 0; i < n; i++) {
            s_stage[s_stageN++] = buf[i];
            if (s_stageN == FRAME_N && !encodeStage()) { recStop(); return false; }
        }
        if (got < sizeof buf) break;   // the DMA drained
    }
    s_recMs = (uint16_t)(now - s_recStart);
    if (s_recMs >= MAX_MS) { recStop(); return false; }
    return true;
}

bool recActive() { return s_rec; }
uint16_t recMs(uint32_t now) { return s_rec ? (uint16_t)(now - s_recStart) : s_recMs; }

size_t recStop() {
    if (!s_rec) return s_blobLen;
    s_rec = false;
    // Pad the tail of a short final frame with silence so the last words are
    // not clipped by the flush.
    if (s_stageN > 0) {
        memset(s_stage + s_stageN, 0, (size_t)(FRAME_N - s_stageN) * sizeof(int16_t));
        s_stageN = FRAME_N;
        encodeStage();
        s_stageN = 0;
    }
    s_recMs = (uint16_t)(millis() - s_recStart);
    if (s_blobLen) {
        mbedtls_sha256_context c;
        mbedtls_sha256_init(&c);
        mbedtls_sha256_starts(&c, 0);
        mbedtls_sha256_update(&c, s_blob, s_blobLen);
        uint8_t full[32];
        mbedtls_sha256_finish(&c, full);
        mbedtls_sha256_free(&c);
        memcpy(s_sha8, full, sizeof s_sha8);
    }
    return s_blobLen;
}

const uint8_t* blob() { return s_blob; }
size_t         blobLen() { return s_blobLen; }
uint16_t       blobMs() { return s_recMs; }
const uint8_t* blobSha8() { return s_sha8; }
void           blobClear() { s_blobLen = 0; }

// ---- playing ---------------------------------------------------------------

void play(const uint8_t* opus, size_t len, uint16_t durMs) {
    (void)durMs;
    if (!s_up || !opus || len == 0) return;
    s_playData = opus; s_playLen = len; s_playPos = 0;
    i2s_zero_dma_buffer(PORT_SPK);
    i2s_start(PORT_SPK);
}

void playPump() {
    if (!s_playData) return;
    if (s_playPos + 2 > s_playLen) { stop(); return; }
    const uint16_t len = (uint16_t)s_playData[s_playPos] |
                         ((uint16_t)s_playData[s_playPos + 1] << 8);
    const size_t off = s_playPos + 2;
    if (len == 0 || off + len > s_playLen) { stop(); return; }
    static int16_t pcm[FRAME_N];
    const int n = opus_decode(s_dec, s_playData + off, (opus_int32)len, pcm, FRAME_N, 0);
    if (n < 0) { stop(); return; }
    // i2s_write blocks until the DMA takes it, which paces playback to real
    // time without a timer of our own.
    size_t done = 0;
    i2s_write(PORT_SPK, pcm, (size_t)n * sizeof(int16_t), &done, portMAX_DELAY);
    s_playPos = off + len;
}

bool playing() { return s_playData != nullptr; }

void stop() {
    s_playData = nullptr; s_playLen = 0; s_playPos = 0;
    i2s_stop(PORT_SPK);
}

bool selfTest(uint32_t now) {
    if (!s_up) return false;
    recStart(now);
    const uint32_t t0 = millis();
    while (millis() - t0 < 3000) recPump(millis());
    recStop();
    if (s_blobLen) play(s_blob, s_blobLen, s_recMs);
    const uint32_t t1 = millis();
    while (playing() && millis() - t1 < 5000) playPump();
    return true;
}

} // namespace Voice
#endif // POTS_VOICE
