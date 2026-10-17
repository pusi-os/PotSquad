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
const uint8_t ROW_N = 8;     // BACK is pinned to the bottom edge now, not a row

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

} // namespace

void uiMeshMenuScroll(int delta) { g_scroll += delta; if (g_scroll < 0) g_scroll = 0; }

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
