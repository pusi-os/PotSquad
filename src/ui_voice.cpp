// The DESK voice gesture — see include/ui_voice.h.
#include "ui_voice.h"

#if POTS_VOICE
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "theme.h"
#include "voice.h"
#include "pusimesh_ble.h"

namespace UiVoice {

enum class St : uint8_t { IDLE, REC, READY, DECIDE, DONE };
static St       s_st = St::IDLE;
static uint32_t s_pressAt = 0;
static uint32_t s_readyAt = 0;
static uint32_t s_doneAt  = 0;
static bool     s_sent    = false;

static const uint32_t HOLD_CANCEL_MS = 2000;
static const uint32_t READY_MS       = 20000;
static const uint32_t DONE_MS        = 2500;

bool holding() { return s_st != St::IDLE; }

void abort() {
    if (Voice::recActive()) Voice::recStop();
    Voice::stop();
    Voice::blobClear();
    if (PusimeshBle::active()) PusimeshBle::end();
    s_st = St::IDLE;
}

void press(uint32_t now) {
    if (s_st == St::IDLE) {
        PusimeshBle::begin();
        Voice::blobClear();
        Voice::recStart(now);
        s_st = St::REC;
        s_pressAt = now;
        return;
    }
    if (s_st == St::READY) {
        s_st = St::DECIDE;
        s_pressAt = now;
    }
}

void release(uint32_t now) {
    if (s_st == St::REC) {
        Voice::recStop();
        s_st = St::READY;
        s_readyAt = now;
        return;
    }
    if (s_st == St::DECIDE) {
        const uint32_t held = now - s_pressAt;
        if (held >= HOLD_CANCEL_MS) {
            Voice::blobClear();
            s_sent = false;
        } else if (PusimeshBle::connected() && PusimeshBle::sendClip()) {
            s_sent = true;
        } else {
            // No phone on the link: play it back here, so the gesture still
            // proves the mic, the codec and the speaker.
            Voice::play(Voice::blob(), Voice::blobLen(), Voice::blobMs());
            s_sent = false;
        }
        s_st = St::DONE;
        s_doneAt = now;
    }
}

static void box(TFT_eSPI& t) {
    const int w = t.width(), h = t.height();
    const int bw = 150, bh = 54;
    t.fillRoundRect(w / 2 - bw / 2, h / 2 - bh / 2, bw, bh, 8, Theme::BG);
    t.drawRoundRect(w / 2 - bw / 2, h / 2 - bh / 2, bw, bh, 8, Theme::CYAN);
}

static void centre(TFT_eSPI& t, const char* s, uint16_t col, uint8_t font, int dy = 0) {
    t.setTextDatum(MC_DATUM);
    t.setTextColor(col, Theme::BG);
    t.setTextFont(font);
    t.drawString(s, t.width() / 2, t.height() / 2 + dy, font);
}

void tick(TFT_eSPI& t, uint32_t now) {
    PusimeshBle::tick(now);
    if (Voice::playing()) Voice::playPump();

    if (s_st == St::REC) {
        if (!Voice::recActive()) { s_st = St::READY; s_readyAt = now; }
    } else if (s_st == St::READY && now - s_readyAt > READY_MS) {
        abort();
        return;
    } else if (s_st == St::DONE && now - s_doneAt > DONE_MS) {
        s_st = St::IDLE;
        PusimeshBle::end();
        return;
    }

    if (s_st == St::IDLE) return;

    char line[24];
    box(t);
    if (s_st == St::REC) {
        const uint16_t ms = Voice::recMs(now);
        snprintf(line, sizeof line, "0:%02u", (unsigned)(ms / 1000));
        centre(t, line, Theme::AMBER, 6);
        const int barW = 120, x = t.width() / 2 - barW / 2, y = t.height() / 2 + 18;
        t.fillRect(x, y, barW, 3, Theme::BG);
        const int fill = (int)((uint32_t)barW * (ms < Voice::MAX_MS ? ms : Voice::MAX_MS) / Voice::MAX_MS);
        t.fillRect(x, y, fill, 3, Theme::AMBER);
    } else if (s_st == St::READY) {
        centre(t, Theme::tr("SEND", "ОТПРАВИТЬ"), Theme::GREEN, 4, -8);
        centre(t, Theme::tr("tap / hold = cancel", "тап / удерж. = отмена"), Theme::CYAN, 2, 16);
    } else if (s_st == St::DECIDE) {
        const uint32_t held = now - s_pressAt;
        const int barW = 120, x = t.width() / 2 - barW / 2, y = t.height() / 2 + 18;
        snprintf(line, sizeof line, "%s", Theme::tr("CANCEL", "ОТМЕНА"));
        centre(t, line, Theme::RED, 4);
        t.fillRect(x, y, barW, 3, Theme::BG);
        const int fill = (int)((uint32_t)barW * (held < HOLD_CANCEL_MS ? held : HOLD_CANCEL_MS) / HOLD_CANCEL_MS);
        t.fillRect(x, y, fill, 3, Theme::RED);
    } else {
        centre(t, s_sent ? Theme::tr("SENT", "ОТПРАВЛЕНО") : Theme::tr("DONE", "ГОТОВО"),
               s_sent ? Theme::GREEN : Theme::CYAN, 4);
    }
}

}  // namespace UiVoice
#endif
