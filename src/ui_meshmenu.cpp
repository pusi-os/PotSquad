// PotsSquad-CYD — the PusiMesh menu. See include/ui_meshmenu.h.
#include "ui_meshmenu.h"

#if POTS_MESH

#include "theme.h"
#include "settings.h"
#include "potsy.h"
#include "detection.h"
#include "meshtalk.h"
#include <Arduino.h>
#include <string.h>
#include <stdio.h>

namespace {

// Under the heading. Asked at draw time: the heading grows on the large
// panels, and that is only known once setLarge() has run at boot.
int topMargin() { return Theme::LIST_TOP + Theme::listHeadingH(); }
const uint8_t ROW_N = 9;     // BACK is pinned to the bottom edge now, not a row

// The switch-to-ESP-NOW warning. Drawn over the menu, so nothing the setting
// does happens until YES; the hit test answers CONFIRM_* while it is up.
bool s_confirm = false;
int  s_cYesX = 0, s_cYesY = 0, s_cNoX = 0, s_cNoY = 0, s_cBW = 0, s_cBH = 0;

// Row height from live font metrics, shared by drawing and hit-testing so
// the two cannot drift -- the same reason every other row list in this
// codebase computes it rather than hard-coding it.
void geom(TFT_eSPI& t, int& top, int& rowH) {
    top = topMargin();
    t.setTextSize(Theme::uiMenuTextSize(t));
    rowH = t.fontHeight() + Theme::listRowPad();
    // Eight rows at that height run under BACK on a landscape panel, so there
    // they share what room there is instead.
    const int room = t.height() - Theme::pinnedBackH(t.width()) - 2 - top;
    // ...unless FONTS > MENUS made the letters too tall to share it: then
    // they keep their height and the list scrolls, as on a compact screen.
    if (ROW_N * rowH > room && !Theme::compact() && room / ROW_N >= t.fontHeight() + 4) rowH = room / ROW_N;
}

// On a compact screen the rows keep their height and the list scrolls
// instead: g_scroll is the first row shown.
int g_scroll = 0;
int visibleRows(TFT_eSPI& t, int top, int rowH) {
    const int room = t.height() - Theme::pinnedBackH(t.width()) - 2 - top;
    const int v = room / rowH;
    return v > (int)ROW_N ? (int)ROW_N : (v < 1 ? 1 : v);
}

// Row measure/print lives in Theme; thin aliases keep this file's row
// calls short.
static int rowW(TFT_eSPI& t, const char* s) { return Theme::rowWidth(t, s); }
static void rowS(TFT_eSPI& t, const char* s, int x, int y) { Theme::rowPrint(t, s, x, y); }

void row(TFT_eSPI& t, int w, int y, int hgt, const char* label,
         const char* value, uint16_t valueCol) {
    if (y < 0) return;      // scrolled out of sight
    // A solid panel under the row, as the settings screen has. Without it
    // the labels sat straight on the dimmed backdrop, and the synthwave sun
    // came through the gaps in every word.
    Theme::drawListRowPanel(t, w, y, hgt);
    // FONTS > MENUS: a row the bigger size would run off the screen steps
    // back down, label and value together.
    Theme::menuFontOn(t, false);
    while (rowW(t, label) + (value ? rowW(t, value) + 8 : 0) > w - 16 && Theme::menuFontSmaller(t)) {}
    t.setTextWrap(false);
    t.setTextColor(Theme::VAPOR_PINK, Theme::BG);
    // On the large panels the taller heading leaves 31-row rows in landscape,
    // and centring on the row put the letters' backing over the panel's
    // bottom edge; centred on the panel (drawListRowPanel's inset) they clear.
    const int ty = Theme::large() ? y + 1 + (hgt - 4 - t.fontHeight()) / 2
                                  : y + (hgt - t.fontHeight()) / 2;
    rowS(t, label, 8, ty);
    if (value) {
        t.setTextColor(valueCol, Theme::BG);
        const int vw = rowW(t, value);
        rowS(t, value, w - 8 - vw, ty);
    }
    t.setTextFont(1);
}

// The switch-to-ESP-NOW warning: what changes, and why it costs detection. It
// is not a yes/no for its own sake -- ESP-NOW really does park the BLE scan, so
// the owner is told that before the radio moves, not after he notices the log
// has gone quiet.
void drawConfirm(TFT_eSPI& t, int w, int h) {
    const bool ru = Settings::lang() == 1;
    const int px = 14, pw = w - 2 * px;
    const int py = Theme::LIST_TOP + 4;
    const int ph = h - py - 34;
    t.fillRoundRect(px, py, pw, ph, 6, Theme::BLACK);
    t.drawRoundRect(px, py, pw, ph, 6, Theme::AMBER);

    const uint8_t keepF = t.textfont;
    Theme::bodyFontOn(t);
    int y = py + 6;
    // Heading.
    t.setTextColor(Theme::AMBER, Theme::BLACK);
    {
        const char* head = Theme::tr("SWITCH TO ESP-NOW?", "ПЕРЕЙТИ НА ESP-NOW?");
        const int tw = ru ? Theme::textWidthRU(t, head) : t.textWidth(head);
        t.setCursor(px + (pw - tw) / 2, y);
        if (ru) Theme::printRU(t, head); else t.print(head);
    }
    y += t.fontHeight() + 4;

    // Body, wrapped to the panel.
    t.setTextColor(Theme::WHITE, Theme::BLACK);
    const char* body = Theme::tr(
        "ESP-NOW uses the WiFi radio. While it is on the BLE scan stands down, "
        "so DETECTION IS PAUSED -- new cameras, tags and trackers will not be "
        "caught. The squad still works, and reaches further. Switch back to BLE "
        "to detect again.",
        "ESP-NOW использует радиомодуль WiFi. Пока он включён, BLE-сканирование "
        "останавливается, и ПОИСК ПРИОСТАНАВЛИВАЕТСЯ — новые камеры, метки и "
        "трекеры не будут найдены. Сквад продолжит работать, с большей "
        "дальностью. Верните BLE, чтобы снова искать.");
    const uint8_t linesMax = 9;
    char lines[9][80];   // the wide variant: RU lines are two bytes a letter
    const uint8_t n = ru ? Theme::wrapTextRU(t, body, pw - 12, lines, linesMax)
                         : Theme::wrapText(t, body, pw - 12, lines, linesMax);
    for (uint8_t i = 0; i < n; i++) {
        t.setCursor(px + 6, y);
        if (ru) Theme::printRU(t, lines[i]); else t.print(lines[i]);
        y += t.fontHeight() + 1;
    }
    if (ru) t.setTextFont(keepF);
    Theme::bodyFontOff(t);

    // Buttons, side by side at the foot of the panel.
    s_cBW = (pw - 24) / 2;
    s_cBH = Theme::large() ? 30 : 22;
    const int by = py + ph - s_cBH - 8;
    s_cYesX = px + 8;                 s_cYesY = by;
    s_cNoX  = px + 12 + s_cBW;        s_cNoY  = by;
    Theme::drawButton(t, s_cYesX, s_cYesY, s_cBW, s_cBH,
                      Theme::tr("[ SWITCH ]", "[ ПЕРЕКЛЮЧИТЬ ]"), true);
    Theme::drawButton(t, s_cNoX, s_cNoY, s_cBW, s_cBH,
                      Theme::tr("[ KEEP BLE ]", "[ ОСТАТЬСЯ BLE ]"), false);
}

} // namespace

void uiMeshMenuScroll(int delta) { g_scroll += delta; if (g_scroll < 0) g_scroll = 0; }

void uiMeshMenuStartConfirm() { s_confirm = true; }
void uiMeshMenuEndConfirm()   { s_confirm = false; }
bool uiMeshMenuConfirming()   { return s_confirm; }

void uiMeshMenuInit(TFT_eSPI& t) {
    t.fillRect(0, 0, t.width(), t.height(), Theme::BG);
}

void uiMeshMenuTick(TFT_eSPI& t, uint32_t now, const DetectionEngine& eng, bool advance) {
    const int w = t.width(), h = t.height();

    // Same knocked-back backdrop the other sub-screens use, so this reads as
    // part of the device rather than as a dialog bolted on.
    Theme::Palette saved = Theme::dimPaletteForOverlay(150);
    Theme::drawActiveBackground(t, now, 0, h, eng, advance);
    Theme::restorePalette(saved);
    Theme::dimRegion(t, 0, 0, w, h, 128);

    Theme::drawListHeading(t, "PUSIMESH", Theme::VAPOR_PINK);

    // The warning replaces the list, not sits beside it: nothing about the
    // radio moves until YES, and a warning you can tap past is not one.
    if (s_confirm) { drawConfirm(t, w, h); return; }

    int top, rowH;
    geom(t, top, rowH);
    const int vis = visibleRows(t, top, rowH);
    if (g_scroll > (int)ROW_N - vis) g_scroll = (int)ROW_N - vis;
    if (g_scroll < 0) g_scroll = 0;
    auto rowY = [&](int k) { return (k < g_scroll || k >= g_scroll + vis) ? -1 : top + (k - g_scroll) * rowH; };

    const char* nm = Potsy::customName();
    if (!nm) nm = Potsy::nickname();

    // Transmit is coloured by state rather than just labelled, because it is
    // the row with a consequence: green reads as "you are visible to anybody
    // scanning", which is worth being able to see at a glance rather than
    // read.
    row(t, w, rowY(0), rowH, Theme::tr("DETECT", "ПОИСК"),   Settings::meshDetectLabel(),
        Settings::meshDetect() ? Theme::GREEN : Theme::W95_SHADOW);
    row(t, w, rowY(1), rowH, Theme::tr("TRANSMIT", "ПЕРЕДАЧА"), Settings::meshTransmitLabel(),
        Settings::meshTransmit() ? Theme::AMBER : Theme::W95_SHADOW);
    // MESSAGES says ERR rather than OFF when the crypto self-test failed at
    // boot: a switch that cannot be turned on should not look like one that
    // merely isn't.
    const bool st = MeshTalk::selfTestOk();
    row(t, w, rowY(2), rowH, Theme::tr("MESSAGES", "СООБЩЕНИЯ"),
        !st ? Theme::tr("ERR", "ОШИБКА") : Theme::onOff(Settings::messagesOn()),
        !st ? Theme::RED : (Settings::messagesOn() ? Theme::GREEN : Theme::W95_SHADOW));
    // How many of them may be on the main screen at once. A tap steps it
    // here: it had a page of its own holding only this, and the desk's count
    // lives on the DESK MODE page.
    // Serious catches shared with the squad. Grey when it is on but cannot
    // do anything: no messages, or no phrase to seal them with.
    row(t, w, rowY(3), rowH, Theme::tr("HEADS-UP", "ТРЕВОГИ"), Theme::onOff(Settings::meshHeadsUp()),
        Settings::meshHeadsUp() && MeshTalk::ready() ? Theme::GREEN : Theme::W95_SHADOW);
    row(t, w, rowY(4), rowH, Theme::tr("CROWD", "ТОЛПА"), Settings::meshCrowdLabel(),
        Settings::meshCrowd() > 1 ? Theme::GREEN : Settings::meshCrowd() == 0 ? Theme::AMBER : Theme::W95_SHADOW);
    // Everybody who has ever held the phrase, here or not.
    char sq[16];
    const uint8_t members = MeshTalk::rosterCount();
    if (members) snprintf(sq, sizeof sq, "%u >", (unsigned)members);
    else         snprintf(sq, sizeof sq, "%s", Theme::tr("NONE >", "НИКОГО >"));
    row(t, w, rowY(5), rowH, Theme::tr("SQUAD", "СКВАД"), sq, members ? Theme::GREEN : Theme::W95_SHADOW);
    // Never the phrase itself. This screen is looked at over shoulders.
    row(t, w, rowY(6), rowH, Theme::tr("PHRASE", "ФРАЗА"),
        MeshTalk::havePhrase() ? Theme::tr("SET >", "ЕСТЬ >") : Theme::tr("NONE >", "НЕТ >"),
        MeshTalk::havePhrase() ? Theme::VAPOR_YELLOW : Theme::W95_SHADOW);
    row(t, w, rowY(7), rowH, Theme::tr("NAME", "ИМЯ"),     nm, Theme::VAPOR_YELLOW);
    // Which radio the squad rides on. Coloured when ESP-NOW is on, because
    // that is the state worth noticing: detection is paused while it is.
    row(t, w, rowY(8), rowH, Theme::tr("TRANSPORT", "ТРАНСПОРТ"), Settings::meshRadioLabel(),
        Settings::meshRadio() == Settings::MeshRadio::ESPNOW ? Theme::VAPOR_BLUE : Theme::W95_SHADOW);

    // One line saying what the two switches actually mean together, because
    // "DETECT off, TRANSMIT on" is not self-evidently "they can see you but
    // you cannot see them".
    const bool d = Settings::meshDetect(), x = Settings::meshTransmit();
    // Beside the heading there is room for ~25 glyphs, not a sentence:
    // the short RU variants say the same thing in a fragment.
    const bool ru = Settings::lang() == 1;
    const char* note = d && x ? Theme::tr("You see them, they see you.", "Вы видите их, они видят вас.")
                     : d      ? Theme::tr("You see them. They cannot see you.", "Вы их видите. Они вас — нет.")
                     : x      ? Theme::tr("They see you. You cannot see them.", "Они вас видят. Вы их — нет.")
                              : Theme::tr("Off. Nothing sent, nothing shown.", "Выкл. Ничего не шлём, не показываем.");
    const char* noteShort = d && x ? Theme::tr("You see them, they see you.", "Видим друг друга.")
                     : d      ? Theme::tr("You see them. They cannot see you.", "Вы их видите.")
                     : x      ? Theme::tr("They see you. You cannot see them.", "Они вас видят.")
                              : Theme::tr("Off. Nothing sent, nothing shown.", "Выключено.");
    Theme::bodyFontOn(t);
    t.setTextColor(Theme::W95_LIGHT, Theme::BG);
    // Seven rows leave no room under them in landscape, so there the one
    // line that matters most sits beside the heading instead: a warning
    // about messages if there is one, else what the two switches mean.
    const bool below = top + vis * rowH + 6 + 2 * t.fontHeight() + 3 <= h - Theme::pinnedBackH(t.width()) - 2;
    if (below) {
        t.setCursor(8, top + vis * rowH + 6);
        t.print(note);
    }

    // And one for messages, which need both halves: DETECT to hear one,
    // TRANSMIT to answer.
    const char* mnote =
        !MeshTalk::selfTestOk()      ? Theme::tr("Messages off: crypto self-test failed.", "Сообщения выкл: крипто-тест провален.")
      : !Settings::messagesOn()      ? nullptr
      : !MeshTalk::havePhrase()      ? Theme::tr("Messages need a phrase to share first.", "Сообщениям нужна общая фраза.")
      : !Settings::meshDetect()      ? Theme::tr("Messages need DETECT on to be heard.", "Сообщениям нужен включённый ПОИСК.")
      : !Settings::meshTransmit()    ? Theme::tr("Messages: you can read, not reply.", "Сообщения: читаешь, не отвечаешь.")
                                     : Theme::tr("Messages: reading and replying.", "Сообщения: читаю и отвечаю.");
    const char* mnoteShort =
        !MeshTalk::selfTestOk()      ? Theme::tr("Messages off: crypto self-test failed.", "Крипто-тест провален.")
      : !Settings::messagesOn()      ? nullptr
      : !MeshTalk::havePhrase()      ? Theme::tr("Messages need a phrase to share first.", "Нужна фраза.")
      : !Settings::meshDetect()      ? Theme::tr("Messages need DETECT on to be heard.", "Включи ПОИСК.")
      : !Settings::meshTransmit()    ? Theme::tr("Messages: you can read, not reply.", "Только чтение.")
                                     : Theme::tr("Messages: reading and replying.", "Читаю и отвечаю.");
    // RU goes through the RU face at size 1: the body font's FreeFont
    // has no Cyrillic. Lines wrap to the width they have.
    const uint8_t keepF = t.textfont;
    if (ru) { t.setTextFont(1); t.setTextSize(1); }
    if (below) {
        if (ru) {
            char nl[2][48];
            const uint8_t nn = Theme::wrapTextRU(t, note, w - 16, nl, 2);
            for (uint8_t i = 0; i < nn; i++) {
                t.setCursor(8, top + vis * rowH + 6 + i * 11);
                Theme::printRU(t, nl[i]);
            }
        } else {
            t.setCursor(8, top + vis * rowH + 6);
            t.print(note);
        }
    }
    if (below && mnote) {
        if (ru) {
            char ml[2][48];
            const uint8_t mn = Theme::wrapTextRU(t, mnote, w - 16, ml, 2);
            for (uint8_t i = 0; i < mn; i++) {
                t.setCursor(8, top + vis * rowH + 6 + t.fontHeight() + 3 + i * 11);
                Theme::printRU(t, ml[i]);
            }
        } else {
            t.setCursor(8, top + vis * rowH + 6 + t.fontHeight() + 3);
            t.print(mnote);
        }
    }
    if (!below) {
        const bool warn = mnote && strcmp(mnote, Theme::tr("Messages: reading and replying.", "Сообщения: читаю и отвечаю.")) != 0;
        const char* one = warn ? (ru ? mnoteShort : mnote) : (ru ? noteShort : note);
        t.setTextColor(warn ? Theme::AMBER : Theme::W95_LIGHT, Theme::BG);
        const int ow = ru ? (Theme::textWidthRU(t, one)) : t.textWidth(one);
        t.setCursor(w - 8 - ow, Theme::listHeadingY(t));
        if (ru) Theme::printRU(t, one); else t.print(one);
    }
    if (ru) { t.setTextFont(keepF); }
    Theme::bodyFontOff(t);
    Theme::drawPinnedBack(t, Theme::tr("[ BACK ]", "[ НАЗАД ]"));
}

MeshMenuRow uiMeshMenuHitTest(TFT_eSPI& t, int x, int y, int screenW, int screenH) {
    (void)screenW; (void)screenH;
    // While the warning is up it owns every tap: two buttons, or nothing.
    if (s_confirm) {
        if (x >= s_cYesX && x <= s_cYesX + s_cBW && y >= s_cYesY && y <= s_cYesY + s_cBH)
            return MeshMenuRow::CONFIRM_YES;
        if (x >= s_cNoX && x <= s_cNoX + s_cBW && y >= s_cNoY && y <= s_cNoY + s_cBH)
            return MeshMenuRow::CONFIRM_NO;
        return MeshMenuRow::NONE;
    }
    int top, rowH;
    geom(t, top, rowH);
    if (x < 0 || y < top) return MeshMenuRow::NONE;
    const int vis = visibleRows(t, top, rowH);
    if ((y - top) / rowH >= vis) return MeshMenuRow::NONE;
    const int idx = (y - top) / rowH + g_scroll;
    if (idx < 0 || idx >= (int)ROW_N) return MeshMenuRow::NONE;
    return (MeshMenuRow)idx;
}

#endif // POTS_MESH
