// PotsSquad-CYD — the PusiMesh consent gate. See include/ui_meshwarn.h.
#include "ui_meshwarn.h"

#if POTS_MESH

#include "theme.h"
#include "settings.h"
#include "detection.h"
#include <Arduino.h>

namespace {

// Written as paragraphs rather than one block so the wrap can breathe, and
// so the order is an argument: what goes out, what anybody can do with it,
// and which half of the feature is harmless.
//
// Every number here is real. 1.5 seconds is ADV_MS in detection.cpp; the
// address is the stable public one (see the note in include/pusimesh.h --
// it stays stable because ignoring a specific peer needs something stable to
// key on); the name and appearance are literally what the payload carries.
// If any of those change, this text is wrong and has to change with them.
//
// Messages added one line, and the rest got shorter to make room for it. A
// message's words are encrypted; the fact of sending one is not -- the frame
// rides in a scan response anyone can see for thirty seconds (SEND_MS in
// meshtalk.cpp). That is a real disclosure and it goes on this screen, not in
// a menu note somebody may never read.
const char* const PARAS_EN[] = {
    "TRANSMIT broadcasts a Bluetooth advert every 1.5 seconds, carrying a fixed address that never changes and your Potsy's name and outfit.",
    "Anyone nearby with a scanner can log that address with a time and a place. Because it never changes, those sightings join up into a record of where you have been.",
    "The same trick this device warns you about.",
    "Messages are encrypted. That you sent one is not.",
    "DETECT only listens. It broadcasts nothing.",
};
// Shorter than the EN originals: the RU face is wider per line and the
// paragraphs share the screen with the buttons below them.
const char* const PARAS_RU[] = {
    "ПЕРЕДАЧА шлёт Bluetooth-рекламу каждые 1.5 сек с нестираемым адресом, именем и костюмом твоего Потси.",
    "Любой со сканером рядом запишет адрес со временем и местом. Адрес не меняется — точки сложатся в маршрут.",
    "Тот же трюк, от которого плата тебя и защищает.",
    "Сообщения шифруются. Сам факт отправки — нет.",
    "ПОИСК лишь слушает. Сам он не шлёт ничего.",
};
const uint8_t PARA_N = sizeof(PARAS_EN) / sizeof(PARAS_EN[0]);

const int BTN_GAP = 18;
// Bigger on the large panels, where the rest of the screen grew with them.
int btnW() { return Theme::large() ? 120 : 92; }
int btnH() { return Theme::large() ? 36 : 28; }

// Buttons sit a fixed distance off the bottom, and both drawing and hit
// testing come through here so they cannot drift apart -- the same reason
// every other two-part control in this codebase shares its geometry.
void buttonGeom(TFT_eSPI& t, int& yesX, int& noX, int& y) {
    const int w = t.width(), h = t.height();
    const int total = btnW() * 2 + BTN_GAP;
    yesX = (w - total) / 2;
    noX  = yesX + btnW() + BTN_GAP;
    // Inside the dialog window's bottom edge on the large panels (WIN_M).
    y    = h - btnH() - (Theme::large() ? 18 : 10);
}

// The large panels: the same words in a Win95 dialog, black on grey, in
// front of the dimmed room. It is a system warning, so it looks like one,
// and dark text on a light face is the easiest thing on the board to read.
const int WIN_M = 8;
const uint16_t WARN_RED = 0xA000;

void drawAsWindow(TFT_eSPI& t) {
    const int w = t.width(), h = t.height();
    const int wx = WIN_M, ww = w - 2 * WIN_M;
    int y = Theme::drawWin95Window(t, wx, WIN_M, ww, h - 2 * WIN_M, "PusiMesh");
    const int tx = wx + 12, tw = ww - 24;
    t.setTextWrap(false);

    const bool ru = Settings::lang() == 1;
    const char* warn = Theme::tr("THIS MAKES YOU TRACKABLE", "ТЫ СТАНЕШЬ ОТСЛЕЖИВАЕМ");
    if (ru) {
        t.setTextFont(1);
        t.setTextSize(1);
        t.setTextColor(WARN_RED, Theme::W95_FACE);
        t.setCursor(tx, y);
        Theme::printRU16(t, warn);
        y += 16 + 8;
    } else {
        t.setTextFont(1);
        t.setTextSize(2);
        t.setTextColor(WARN_RED, Theme::W95_FACE);
        t.setCursor(tx, y);
        t.print(warn);
        y += t.fontHeight() + 8;
    }

    Theme::bodyFontOn(t);
    if (ru) { t.setTextFont(1); t.setTextSize(1); }
    int maxW = tw;
    if (maxW > 79 * t.textWidth("M")) maxW = 79 * t.textWidth("M");
    const int lineH = t.fontHeight() + 1;
    t.setTextColor(Theme::BLACK, Theme::W95_FACE);
    for (uint8_t p = 0; p < PARA_N; p++) {
        const char* para = ru ? PARAS_RU[p] : PARAS_EN[p];
        char lines[8][80];
        const uint8_t n = ru ? Theme::wrapTextRU(t, para, maxW, lines, 8)
                             : Theme::wrapText(t, para, maxW, lines, 8);
        for (uint8_t i = 0; i < n; i++) {
            t.setCursor(tx, y);
            if (ru) Theme::printRU(t, lines[i]); else t.print(lines[i]);
            y += lineH;
        }
        y += 5;
    }

    int yesX, noX, by;
    buttonGeom(t, yesX, noX, by);
    // The question, in bold the way Win95 drew one: the same word twice, a
    // pixel apart.
    const char* q = Theme::tr("Turn on PusiMesh?", "Включить ПотсМеш?");
    if (ru) {
        t.setTextFont(1);
        t.setTextSize(1);
        const int qw = Theme::textWidthRU(t, q);
        const int qx = (w - qw) / 2, qy = by - t.fontHeight() - 8;
        t.setCursor(qx, qy);     Theme::printRU(t, q);
        t.setTextColor(Theme::BLACK);   // transparent: the second pass only thickens
        t.setCursor(qx + 1, qy); Theme::printRU(t, q);
    } else {
        const int qx = (w - t.textWidth(q)) / 2, qy = by - t.fontHeight() - 8;
        t.setCursor(qx, qy);     t.print(q);
        t.setTextColor(Theme::BLACK);   // transparent: the second pass only thickens
        t.setCursor(qx + 1, qy); t.print(q);
    }
    Theme::bodyFontOff(t);

    Theme::drawWin95Button(t, yesX, by, btnW(), btnH(), Theme::tr("YES", "ДА"), false);
    Theme::drawWin95Button(t, noX,  by, btnW(), btnH(), Theme::tr("NO", "НЕТ"), false);
}

} // namespace

void uiMeshWarnInit(TFT_eSPI& t) {
    t.fillRect(0, 0, t.width(), t.height(), Theme::BG);
}

void uiMeshWarnTick(TFT_eSPI& t, uint32_t now, const DetectionEngine& eng, bool advance) {
    const int w = t.width(), h = t.height();

    // Knocked back further than the menu behind it (110 against 128). This
    // screen is all small text and it has to be read, not glanced at.
    Theme::Palette saved = Theme::dimPaletteForOverlay(150);
    Theme::drawActiveBackground(t, now, 0, h, eng, advance);
    Theme::restorePalette(saved);
    Theme::dimRegion(t, 0, 0, w, h, 110);

    if (Theme::large()) { drawAsWindow(t); return; }

    t.setTextWrap(false);
    t.setTextSize(2);
    t.setTextColor(Theme::VAPOR_PINK, Theme::BG);
    t.setCursor(8, 6);
    t.print("PUSIMESH");

    const bool ru = Settings::lang() == 1;
    const char* warn = Theme::tr("THIS MAKES YOU TRACKABLE", "ТЫ СТАНЕШЬ ОТСЛЕЖИВАЕМ");
    t.setTextColor(Theme::AMBER, Theme::BG);
    if (ru) {
        t.setTextSize(1);
        t.setCursor(8, 26);
        Theme::printRU16(t, warn);
    } else {
        t.setTextSize(1);
        t.setCursor(8, 26);
        t.print(warn);
    }

    // Theme::wrapText fills fixed 48-char rows, so the wrap width is capped
    // at what 47 characters occupy regardless of how wide the panel is. At
    // 320px the margins alone would allow 50, and the two extra characters
    // would be written past the end of the row.
    const int charW = t.textWidth("M");
    int maxW = w - 16;
    if (maxW > 47 * charW) maxW = 47 * charW;

    const int lineH = t.fontHeight() + 1;
    int y = 40;
    t.setTextColor(Theme::WHITE, Theme::BG);
    for (uint8_t p = 0; p < PARA_N; p++) {
        const char* para = ru ? PARAS_RU[p] : PARAS_EN[p];
        char lines[8][48];
        const uint8_t n = ru ? Theme::wrapTextRU(t, para, maxW, lines, 8)
                             : Theme::wrapText(t, para, maxW, lines, 8);
        for (uint8_t i = 0; i < n; i++) {
            t.setCursor(8, y);
            if (ru) Theme::printRU(t, lines[i]); else t.print(lines[i]);
            y += lineH;
        }
        y += 4;                      // a gap between paragraphs, not a line
    }

    int yesX, noX, by;
    buttonGeom(t, yesX, noX, by);

    // The question sits with the buttons rather than at the top, so whatever
    // is being answered is the last thing read before answering it.
    t.setTextColor(Theme::CYAN, Theme::BG);
    const char* q = Theme::tr("Turn on PusiMesh?", "Включить ПотсМеш?");
    if (ru) {
        const int qw = Theme::textWidthRU(t, q);
        t.setCursor((w - qw) / 2, by - 14);
        Theme::printRU(t, q);
    } else {
        t.setCursor((w - t.textWidth(q)) / 2, by - 14);
        t.print(q);
    }

    // NO is not the quiet one. A consent dialog whose decline is styled as
    // the lesser option is doing the opposite of what it is for, so both are
    // ordinary system buttons and neither is preselected.
    Theme::drawWin95Button(t, yesX, by, btnW(), btnH(), Theme::tr("YES", "ДА"), false);
    Theme::drawWin95Button(t, noX,  by, btnW(), btnH(), Theme::tr("NO", "НЕТ"), false);
}

MeshWarnHit uiMeshWarnHitTest(TFT_eSPI& t, int x, int y) {
    int yesX, noX, by;
    buttonGeom(t, yesX, noX, by);
    const int SLOP = 6;
    if (y < by - SLOP || y > by + btnH() + SLOP) return MeshWarnHit::NONE;
    if (x >= yesX - SLOP && x <= yesX + btnW() + SLOP) return MeshWarnHit::YES;
    if (x >= noX  - SLOP && x <= noX  + btnW() + SLOP) return MeshWarnHit::NO;
    return MeshWarnHit::NONE;
}

#endif // POTS_MESH
