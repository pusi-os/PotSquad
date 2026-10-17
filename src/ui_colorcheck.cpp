// PotsSquad-CYD — first-boot color-order sanity check implementation
#include "ui_colorcheck.h"
#include "theme.h"
#include "settings.h"
#include "potsy.h"
#include <Arduino.h>

// Pure RGB565 primaries, deliberately NOT Theme::RED/GREEN (those are
// stylized neon shades, not necessarily pure channels) -- the whole
// point of this screen is testing whether the panel renders a pure red
// as red, so it needs the real thing.
static const uint16_t PURE_RED   = 0xF800;
static const uint16_t PURE_GREEN = 0x07E0;
static const uint16_t PURE_BLUE  = 0x001F;

static const uint8_t CAPTION_MAX_LINES = 3;

// Shared by drawing and hit-testing so they can't drift apart -- no
// live TFT_eSPI& needed here (unlike most other screens' geometry
// helpers) since every row uses a fixed text size rather than metrics
// read back from the font, so there's nothing to query.
static void computeGeom(int w, int h,
                         int& potsyBaseY, float& potsyScale,
                         int& captionTop, int& captionMaxW,
                         int& wordsTop, int& wordRowH,
                         int& invX, int& invY, int& invW, int& invH,
                         int& ordX, int& ordY, int& ordW, int& ordH,
                         int& doneX, int& doneY, int& doneW, int& doneH) {
    (void)h;
    // Everything a step up on the large panels: Potsy, the 16-row body
    // font for the caption, size-4 colour words and taller buttons.
    const bool big = Theme::large();
    potsyScale = big ? 1.0f : 0.6f;
    potsyBaseY = big ? 80 : 50;
    captionTop = potsyBaseY + 14;
    captionMaxW = w - 24;
    wordsTop  = captionTop + CAPTION_MAX_LINES * (big ? 18 : 10) + 4;
    wordRowH  = big ? 32 : 24;

    int btnY = wordsTop + wordRowH * 3 + 6;
    int btnH = big ? 30 : 20;
    const int margin = 12, gap = 8;
    int btnW = (w - 2 * margin - gap) / 2;
    invX = margin;           invY = btnY; invW = btnW; invH = btnH;
    ordX = invX + btnW + gap; ordY = btnY; ordW = btnW; ordH = btnH;

    doneY = btnY + btnH + 6;
    doneX = margin;
    doneW = w - 2 * margin;
    doneH = btnH;
}

void uiColorCheckInit(TFT_eSPI& t) {
    t.fillRect(0, 0, t.width(), t.height(), Theme::BG);
}

void uiColorCheckTick(TFT_eSPI& t, uint32_t now) {
    int w = t.width(), h = t.height();
    t.fillRect(0, 0, w, h, Theme::BG);

    int potsyBaseY, captionTop, captionMaxW, wordsTop, wordRowH;
    float potsyScale;
    int invX, invY, invW, invH, ordX, ordY, ordW, ordH, doneX, doneY, doneW, doneH;
    computeGeom(w, h, potsyBaseY, potsyScale, captionTop, captionMaxW, wordsTop, wordRowH,
                invX, invY, invW, invH, ordX, ordY, ordW, ordH, doneX, doneY, doneW, doneH);

    // Lightweight cameo (same one the boot splash itself uses just
    // before this screen appears) rather than the full tick()
    // idle/quip state machine -- this is a one-shot explanation, not
    // an idle screen he should be chattering on. No bubble here (line
    // left null) -- his normal speech bubble caps at 2 short lines,
    // too little room for this caption, so it's drawn as its own
    // wrapped block below him instead, with a real line budget.
    Potsy::drawWaving(t, w / 2, potsyBaseY, now, potsyScale, nullptr);

    Theme::bodyFontOn(t);
    t.setTextWrap(false);
    t.setTextColor(Theme::WHITE, Theme::BG);
    const char* caption = Theme::tr("Wrong colors? Tap below till RED/GREEN/BLUE match.",
                                    "Не те цвета? Жмите, пока цвета не совпадут.");
    char capLines[CAPTION_MAX_LINES][48];
    uint8_t capN = Theme::isRu(caption)
        ? Theme::wrapTextRU(t, caption, captionMaxW, capLines, CAPTION_MAX_LINES)
        : Theme::wrapText(t, caption, captionMaxW, capLines, CAPTION_MAX_LINES);
    int cy = captionTop;
    for (uint8_t i = 0; i < capN; i++) {
        int lw = Theme::isRu(capLines[i]) ? Theme::textWidthRU(t, capLines[i]) : t.textWidth(capLines[i]);
        t.setCursor((w - lw) / 2, cy);
        if (Theme::isRu(capLines[i])) Theme::printRU(t, capLines[i]); else t.print(capLines[i]);
        cy += t.fontHeight() + 2;
    }
    Theme::bodyFontOff(t);

    t.setTextSize(Theme::upSize(3));
    const char* words[3] = { Theme::tr("RED", "КРАСНЫЙ"), Theme::tr("GREEN", "ЗЕЛЁНЫЙ"), Theme::tr("BLUE", "СИНИЙ") };
    uint16_t cols[3] = { PURE_RED, PURE_GREEN, PURE_BLUE };
    int y = wordsTop;
    for (int i = 0; i < 3; i++) {
        t.setTextColor(cols[i], Theme::BG);
        if (Theme::isRu(words[i])) {
            // The 16px face: smooth at a size the GLCD cannot do in
            // Cyrillic at all.
            t.setTextFont(1);
            t.setTextSize(1);
            int tw = Theme::textWidthRU16(t, words[i]);
            t.setCursor((w - tw) / 2, y);
            Theme::printRU16(t, words[i]);
            t.setTextSize(Theme::upSize(3));
        } else {
            int tw = t.textWidth(words[i]);
            t.setCursor((w - tw) / 2, y);
            t.print(words[i]);
        }
        y += wordRowH;
    }

    char invLabel[16];
    snprintf(invLabel, sizeof(invLabel), Theme::tr("INVERT: %s", "ИНВЕРСИЯ: %s"), Theme::onOff(Settings::inverted()));
    char ordLabel[20];
    snprintf(ordLabel, sizeof(ordLabel), Theme::tr("ORDER: %s", "ПОРЯДОК: %s"), Settings::rgbSwapped() ? Theme::tr("SWAP", "ОБРАТ") : Theme::tr("NORM", "НОРМ"));
    Theme::drawButton(t, invX, invY, invW, invH, invLabel, false);
    Theme::drawButton(t, ordX, ordY, ordW, ordH, ordLabel, false);
    Theme::drawButton(t, doneX, doneY, doneW, doneH, Theme::tr("[ LOOKS GOOD ]", "[ ВСЁ ХОРОШО ]"), false);
}

ColorCheckTap uiColorCheckHitTest(int x, int y, int screenW, int screenH) {
    int potsyBaseY, captionTop, captionMaxW, wordsTop, wordRowH;
    float potsyScale;
    int invX, invY, invW, invH, ordX, ordY, ordW, ordH, doneX, doneY, doneW, doneH;
    computeGeom(screenW, screenH, potsyBaseY, potsyScale, captionTop, captionMaxW, wordsTop, wordRowH,
                invX, invY, invW, invH, ordX, ordY, ordW, ordH, doneX, doneY, doneW, doneH);

    if (x >= invX && x <= invX + invW && y >= invY && y <= invY + invH) return ColorCheckTap::INVERT;
    if (x >= ordX && x <= ordX + ordW && y >= ordY && y <= ordY + ordH) return ColorCheckTap::ORDER;
    if (x >= doneX && x <= doneX + doneW && y >= doneY && y <= doneY + doneH) return ColorCheckTap::DONE;
    return ColorCheckTap::NONE;
}
