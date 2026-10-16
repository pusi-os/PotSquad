// PotsSquad-CYD — Potsy's diary screen implementation
#include "ui_diary.h"
#include "theme.h"
#include "type_names.h"
#include "potsy.h"
#include <Arduino.h>

// Stamped in by extra_script.py from `git describe` at build time — see
// platformio.ini. Falls back if that step is somehow skipped.
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "unknown"
#endif

void uiDiaryInit(TFT_eSPI& t) {
    // fillScreen() relies on TFT_eSPI's base-class width/height, which
    // TFT_eSprite::createSprite() never updates — it leaves stale
    // remnants of whatever screen was drawn before when t is a sprite.
    t.fillRect(0, 0, t.width(), t.height(), Theme::BG);
}

static void formatDuration(uint32_t ms, char* buf, size_t n) {
    uint32_t sec  = ms / 1000;
    uint32_t days = sec / 86400; sec %= 86400;
    uint32_t hrs  = sec / 3600;  sec %= 3600;
    uint32_t mins = sec / 60;
    if (days > 0)      snprintf(buf, n, "%lud %luh", (unsigned long)days, (unsigned long)hrs);
    else if (hrs > 0)  snprintf(buf, n, "%luh %lum", (unsigned long)hrs, (unsigned long)mins);
    else if (mins > 0) snprintf(buf, n, "%lum", (unsigned long)mins);
    else               snprintf(buf, n, "<1m");
}

static void drawStat(TFT_eSPI& t, int w, int y, int h, const char* label, const char* value) {
    Theme::bodyFontOn(t);
    t.setTextColor(Theme::CYAN, Theme::BG);
    const bool lru = Theme::isRu(label);
    if (lru) { t.setTextFont(1); t.setTextSize(1); }
    t.setCursor(8, y + (h - t.fontHeight()) / 2);
    if (lru) Theme::printRU(t, label); else t.print(label);
    t.setTextColor(Theme::WHITE, Theme::BG);
    const bool vru = Theme::isRu(value);
    if (vru && !lru) { t.setTextFont(1); t.setTextSize(1); }
    int vw = vru ? Theme::textWidthRU(t, value) : t.textWidth(value);
    t.setCursor(w - 8 - vw, y + (h - t.fontHeight()) / 2);
    if (vru) Theme::printRU(t, value); else t.print(value);
    Theme::bodyFontOff(t);
    t.drawFastHLine(4, y + h - 1, w - 8, Theme::PURPLE);
}

void uiDiaryTick(TFT_eSPI& t, uint32_t now, const DetectionEngine& eng) {
    int w = t.width();

    char title[32];
    snprintf(title, sizeof(title), Theme::tr(">> %s'S DIARY <<", ">> ДНЕВНИК %s <<"), Potsy::nickname());
    Theme::drawTitleBar(t, title);

    const int top  = 16;
    // 22 rather than 24: nine rows plus the hint is 226px, which fits a
    // 240px panel. At 24 the ninth row pushed the hint off the bottom.
    // Thirteen rows a stat on a compact screen (240x135): all nine fit, with
    // no room for the hint, which the cursor makes unneeded there.
    // 30 on the large panels, whose rows are the 16-row body font.
    const int rowH = Theme::compact() ? 13 : Theme::large() ? 30 : 22;
    char buf[24];

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)eng.lifetimeTotal());
    drawStat(t, w, top + 0 * rowH, rowH, Theme::tr("LIFETIME CATCHES", "НАХОДОК ВСЕГО"), buf);

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)Potsy::bootCount());
    drawStat(t, w, top + 1 * rowH, rowH, Theme::tr("BOOTS", "ВКЛЮЧЕНИЙ"), buf);

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)Potsy::petCount());
    drawStat(t, w, top + 2 * rowH, rowH, Theme::tr("TIMES PETTED", "РАЗ ПОГЛАДИЛИ"), buf);

    formatDuration(Potsy::currentClearStreakMs(), buf, sizeof(buf));
    drawStat(t, w, top + 3 * rowH, rowH, Theme::tr("CURRENT CLEAR STREAK", "ТИШИНА СЕЙЧАС"), buf);

    formatDuration(Potsy::bestClearStreakMs(), buf, sizeof(buf));
    drawStat(t, w, top + 4 * rowH, rowH, Theme::tr("BEST CLEAR STREAK", "ЛУЧШАЯ ТИШИНА"), buf);

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)Potsy::bestSessionCount());
    drawStat(t, w, top + 5 * rowH, rowH, Theme::tr("BEST SESSION CATCH", "ЛУЧШАЯ СЕССИЯ"), buf);

    DetectionType ft = Potsy::firstDetectionType();
    drawStat(t, w, top + 6 * rowH, rowH, Theme::tr("FIRST EVER CATCH", "ПЕРВАЯ НАХОДКА"),
             ft == DetectionType::UNKNOWN ? Theme::tr("none yet", "пока никого") : TypeNames::display(ft));

    // Whichever type you have logged most, ever. The per-type lifetime
    // counters behind this persist independently of the live counts, which
    // decay as detections go stale -- so this answers "what do I actually
    // keep running into", which a session counter never could.
    {
        DetectionType best = DetectionType::UNKNOWN;
        uint32_t bestN = 0;
        for (uint8_t i = 1; i < (uint8_t)DetectionType::COUNT; i++) {
            const uint32_t n = eng.lifetimeTypeCount((DetectionType)i);
            if (n > bestN) { bestN = n; best = (DetectionType)i; }
        }
        if (bestN == 0) {
            drawStat(t, w, top + 7 * rowH, rowH, Theme::tr("MOST CAUGHT", "ЧАЩЕ ВСЕХ"), Theme::tr("none yet", "пока никого"));
        } else {
            snprintf(buf, sizeof(buf), "%s %lu", TypeNames::display(best),
                     (unsigned long)bestN);
            drawStat(t, w, top + 7 * rowH, rowH, Theme::tr("MOST CAUGHT", "ЧАЩЕ ВСЕХ"), buf);
        }
    }

    drawStat(t, w, top + 8 * rowH, rowH, Theme::tr("FIRMWARE", "ПРОШИВКА"), FIRMWARE_VERSION);

    // Hint, pulsing gently so it doesn't just look like inert label text.
    // Between 65% and 100% of CYAN: the old 10-70% of VAPOR_BLUE rounded to
    // nearly nothing in the frame buffer, in every theme.
    if (Theme::compact()) return;
    float pulse = 0.825f + 0.175f * sinf((float)(now % 1600) / 1600.0f * 6.2831853f);
    uint16_t col = Theme::blend(Theme::BG, Theme::CYAN, (uint16_t)(pulse * 255.0f));
    Theme::bodyFontOn(t);
    t.setTextColor(col, Theme::BG);
    const char* hint = "tap anywhere to go back";
    int hw = t.textWidth(hint);
    t.setCursor((w - hw) / 2, top + 9 * rowH + (Theme::large() ? 4 : 8));
    t.print(hint);
    Theme::bodyFontOff(t);
}
