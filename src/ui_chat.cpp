// PotsSquad-CYD — the CHAT room. See include/ui_chat.h.
//
// The list is the inbox ring, and in the ИЗБРАННЫЕ section the kept store;
// both are read through MeshTalk and never copied here, so a message that
// arrives, is dropped or is kept shows on the next frame with nothing to
// synchronise. A row is a sender line and a two-line preview, the sender in
// red while the message is still unread.
//
// The hold is the whole interface for a message: press and it is read (the
// full view, and the receipt goes out), press and hold and the actions open.
// The three bottom buttons leave the room, swap the list for the kept
// messages, or start a new one.
#include "ui_chat.h"

#if POTS_MESH
#include "theme.h"
#include "settings.h"
#include "meshtalk.h"
#include "meshmsg.h"
#include "detection.h"
#include "ui_clear.h"   // uiClearEmote: the local half of a reaction
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

namespace {

struct Rect { int16_t x, y, w, h; };
inline bool in(const Rect& r, int x, int y) {
    return r.w && x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

// The six things a message can be asked to do, in the order they are drawn.
// READ opens it, REPLY writes back, DELETE drops it from whichever list is
// showing, HEART keeps it or stops, LIKE and DISLIKE are the two emotes a
// message can be answered with without a word.
enum Action : uint8_t { ACT_READ, ACT_REPLY, ACT_DELETE, ACT_HEART, ACT_LIKE, ACT_DISLIKE, ACT_N };

bool     s_favMode = false;
int      s_scroll  = 0;
int      s_menuRow = -1;    // the message the actions are open on, or -1
int      s_fullRow = -1;    // the message the full view is showing, or -1

const int ROW_MAX = MeshTalk::INBOX_N > MeshTalk::FAV_N ? MeshTalk::INBOX_N : MeshTalk::FAV_N;
Rect s_rows[ROW_MAX];
uint8_t s_rowN = 0;
Rect s_close, s_favs, s_write;
Rect s_mRows[ACT_N];
// The live screen size, kept from the last draw so the touch test can ask
// about the pinned BACK strip without the TFT passed in.
int  s_scrW = 0, s_scrH = 0;

uint8_t rowCount() { return s_favMode ? MeshTalk::favCount() : MeshTalk::inboxCount(); }
const MeshTalk::Message& rowMsg(uint8_t i) {
    return s_favMode ? MeshTalk::favAt(i) : MeshTalk::inboxAt(i);
}
bool overlayOpen() { return s_menuRow >= 0 || s_fullRow >= 0; }

int btnH() { return Theme::large() ? 32 : 24; }
int rowHOf(TFT_eSPI& t) { return 3 * (t.fontHeight() + 1) + 5; }

// How long ago, in the same short form the SQUAD inbox uses.
void ago(uint32_t now, uint32_t at, char* out, size_t cap) {
    const uint32_t mins = (now - at) / 60000u;
    if (mins == 0) snprintf(out, cap, "%s", Theme::tr("now", "щас"));
    else if (mins < 1000) snprintf(out, cap, Theme::tr("%lum", "%luм"), (unsigned long)mins);
    else snprintf(out, cap, Theme::tr("%luh", "%luч"), (unsigned long)(mins / 60));
}

// A small filled or hollow heart: two lobes and a point, drawn rather than
// typed because the bitmap fonts carry no such glyph. Used as the KEEP
// marker in the list and as the button the actions open on.
void drawHeart(TFT_eSPI& t, int cx, int cy, uint16_t col, bool filled) {
    const int r = 3;
    if (filled) {
        t.fillCircle(cx - r + 1, cy - 1, r, col);
        t.fillCircle(cx + r - 1, cy - 1, r, col);
        t.fillTriangle(cx - 2 * r + 2, cy, cx + 2 * r - 2, cy, cx, cy + 2 * r, col);
    } else {
        t.drawCircle(cx - r + 1, cy - 1, r, col);
        t.drawCircle(cx + r - 1, cy - 1, r, col);
        t.drawLine(cx - 2 * r + 2, cy, cx, cy + 2 * r, col);
        t.drawLine(cx + 2 * r - 2, cy, cx, cy + 2 * r, col);
    }
}

// The one-line text of a message, wrapped into `max` rows in the current
// face. Returns the line count. Cyrillic goes through the RU face.
uint8_t wrapLine(TFT_eSPI& t, const char* text, int maxW, char lines[][48], uint8_t max) {
    if (Theme::isRu(text)) return Theme::wrapTextRU(t, text, maxW, lines, max);
    return Theme::wrapText(t, text, maxW, lines, max);
}

// A quick answer to the message the actions are open on: seal the emote, and
// only if it went out does our own Potsy perform it too -- the same two steps
// the compose screen takes. A board that cannot send (TRANSMIT off, no phrase,
// no key) says so rather than the button looking dead.
void react(uint8_t e) {
    if (MeshTalk::sendEmote(e, 0, millis()) == MeshTalk::Send::OK) {
        uiClearEmote(e, 0, false);
        Theme::showToast(Theme::tr("REACTION SENT", "РЕАКЦИЯ ОТПРАВЛЕНА"),
                         Theme::tr("The squad will see it", "Сквад увидит"), Theme::VAPOR_PINK);
    } else {
        Theme::showToast(Theme::tr("CAN'T SEND", "НЕ МОГУ ОТПРАВИТЬ"),
                         Theme::tr("Turn TRANSMIT on in PusiMesh", "Включи ПЕРЕДАЧУ в ПотсМеш"),
                         Theme::AMBER);
    }
}

void drawRow(TFT_eSPI& t, int w, int y, const MeshTalk::Message& m, uint32_t now) {
    Theme::bodyFontOn(t);
    const int fh = t.fontHeight();
    const bool ru = Theme::isRu(m.from);
    // Sender, red while unread -- the same rule the SQUAD inbox keeps.
    t.setTextSize(1);
    t.setTextColor(m.unread ? Theme::RED : Theme::W95_LIGHT, Theme::BG);
    t.setCursor(6, y + 1);
    if (ru) Theme::printRU(t, m.from); else t.print(m.from);
    // A kept message wears a heart at the right of the sender line.
    if (MeshTalk::isFavorite(m)) drawHeart(t, w - 14, y + 1 + fh / 2, Theme::VAPOR_PINK, true);
    char ag[8];
    ago(now, m.at, ag, sizeof ag);
    Theme::rowPrint(t, ag, w - (MeshTalk::isFavorite(m) ? 26 : 6) - Theme::rowWidth(t, ag), y + 1);
    // Two lines of what was said.
    char lines[2][48];
    const bool lru = Theme::isRu(MeshTalk::lineText(m));
    const uint8_t n = lru ? Theme::wrapTextRU(t, MeshTalk::lineText(m), w - 12, lines, 2)
                          : Theme::wrapText(t, MeshTalk::lineText(m), w - 12, lines, 2);
    t.setTextColor(Theme::WHITE, Theme::BG);
    for (uint8_t k = 0; k < n; k++) {
        t.setCursor(6, y + fh + 3 + k * (fh + 1));
        if (lru) Theme::printRU(t, lines[k]); else t.print(lines[k]);
    }
    Theme::bodyFontOff(t);
}

void drawFull(TFT_eSPI& t, uint32_t now) {
    const int w = t.width(), h = t.height();
    t.fillRect(0, 0, w, h, Theme::BG);
    Theme::drawListHeading(t, Theme::tr("MESSAGE", "СООБЩЕНИЕ"), Theme::VAPOR_PINK);
    if (s_fullRow >= rowCount()) { s_fullRow = -1; return; }
    const MeshTalk::Message& m = rowMsg((uint8_t)s_fullRow);
    Theme::bodyFontOn(t);
    const int fh = t.fontHeight();
    int y = Theme::LIST_TOP + Theme::listHeadingH() + 2;
    t.setTextColor(Theme::CYAN, Theme::BG);
    t.setCursor(6, y);
    if (Theme::isRu(m.from)) Theme::printRU(t, m.from); else t.print(m.from);
    char ag[16];
    ago(now, m.at, ag, sizeof ag);
    t.setTextColor(Theme::W95_SHADOW, Theme::BG);
    Theme::rowPrint(t, ag, w - 6 - Theme::rowWidth(t, ag), y);
    y += fh + 4;
    // The whole message, in as many lines as it takes.
    const char* body = MeshTalk::lineText(m);
    char lines[12][48];
    const uint8_t n = Theme::isRu(body) ? Theme::wrapTextRU(t, body, w - 12, lines, 12)
                                        : Theme::wrapText(t, body, w - 12, lines, 12);
    t.setTextColor(Theme::WHITE, Theme::BG);
    const int bottom = h - Theme::pinnedBackH(w) - 6;
    for (uint8_t k = 0; k < n && y + fh <= bottom; k++) {
        t.setCursor(6, y);
        if (Theme::isRu(body)) Theme::printRU(t, lines[k]); else t.print(lines[k]);
        y += fh + 1;
    }
    // What it is: kept, unread, or neither.
    y += 4;
    t.setTextColor(Theme::VAPOR_PINK, Theme::BG);
    if (MeshTalk::isFavorite(m)) { drawHeart(t, 12, y + fh / 2, Theme::VAPOR_PINK, true);
                                   t.setCursor(22, y); t.print(Theme::tr("KEPT", "В ИЗБРАННОМ")); }
    Theme::bodyFontOff(t);
    Theme::drawPinnedBack(t, Theme::tr("[ BACK ]", "[ НАЗАД ]"));
}

void drawMenu(TFT_eSPI& t, uint32_t now) {
    const int w = t.width(), h = t.height();
    t.fillRect(0, 0, w, h, Theme::BG);
    Theme::drawListHeading(t, Theme::tr("MESSAGE", "СООБЩЕНИЕ"), Theme::VAPOR_PINK);
    if (s_menuRow >= rowCount()) { s_menuRow = -1; return; }
    const MeshTalk::Message& m = rowMsg((uint8_t)s_menuRow);
    Theme::bodyFontOn(t);
    const int fh = t.fontHeight();
    int y = Theme::LIST_TOP + Theme::listHeadingH() + 1;
    // A one-line reminder of which message this is, so the actions are not
    // aimed at nothing.
    char pv[40];
    snprintf(pv, sizeof pv, "%s: %s", m.from, MeshTalk::lineText(m));
    char lines[1][48];
    wrapLine(t, pv, w - 12, lines, 1);
    t.setTextColor(Theme::W95_LIGHT, Theme::BG);
    t.setCursor(6, y);
    if (Theme::isRu(pv)) Theme::printRU(t, lines[0]); else t.print(lines[0]);
    y += fh + 4;
    Theme::bodyFontOff(t);

    const char* labels[ACT_N] = {
        Theme::tr("READ", "ЧИТАТЬ"),
        Theme::tr("REPLY", "ОТВЕТИТЬ"),
        Theme::tr("DELETE", "УДАЛИТЬ"),
        Theme::tr("KEEP", "ИЗБРАННОЕ"),
        Theme::tr("LIKE", "ЛАЙК"),
        Theme::tr("DISLIKE", "ДИЗЛАЙК"),
    };
    const int bottom = h - Theme::pinnedBackH(w) - 4;
    int rowH = (bottom - y) / ACT_N;
    if (rowH > 30) rowH = 30;
    for (uint8_t i = 0; i < ACT_N; i++) {
        const int ry = y + i * rowH;
        s_mRows[i] = { 6, (int16_t)ry, (int16_t)(w - 12), (int16_t)(rowH - 2) };
        Theme::drawListRowPanel(t, w, ry, rowH);
        Theme::bodyFontOn(t);
        t.setTextColor(i == ACT_DELETE ? Theme::RED : Theme::VAPOR_PINK, Theme::BG);
        const int ty = ry + (rowH - 2 - t.fontHeight()) / 2;
        t.setCursor(12, ty);
        if (Theme::isRu(labels[i])) Theme::printRU(t, labels[i]); else t.print(labels[i]);
        Theme::bodyFontOff(t);
        if (i == ACT_HEART)
            drawHeart(t, w - 18, ry + rowH / 2, Theme::VAPOR_PINK, MeshTalk::isFavorite(m));
    }
    Theme::drawPinnedBack(t, Theme::tr("[ BACK ]", "[ НАЗАД ]"));
}

} // namespace

void uiChatInit(TFT_eSPI& t) {
    t.fillRect(0, 0, t.width(), t.height(), Theme::BG);
    s_favMode = false;
    s_scroll  = 0;
    s_menuRow = -1;
    s_fullRow = -1;
    s_rowN    = 0;
}

void uiChatScroll(int delta) {
    s_scroll += delta;
    if (s_scroll < 0) s_scroll = 0;
    // The draw clamps the other end once it knows how many rows fit.
}

void uiChatShowFavs(bool on) { s_favMode = on; s_scroll = 0; }

void uiChatOpenMenu(int row) {
    if (row < 0 || row >= rowCount()) return;
    s_menuRow = row;
    s_fullRow = -1;
}

void uiChatReadRow(int row) {
    if (row < 0 || row >= rowCount()) return;
    // Reading it from the inbox is the moment the receipt goes out, exactly
    // as opening the SQUAD inbox is. A kept copy of an already-read message
    // has nothing to send.
    if (!s_favMode) MeshTalk::markReadAt((uint8_t)row);
    s_fullRow = row;
    s_menuRow = -1;
}

void uiChatTick(TFT_eSPI& t, uint32_t now, const DetectionEngine& eng, bool advance) {
    const int w = t.width(), h = t.height();
    s_scrW = w; s_scrH = h;

    Theme::Palette saved = Theme::dimPaletteForOverlay(150);
    Theme::drawActiveBackground(t, now, 0, h, eng, advance);
    Theme::restorePalette(saved);
    Theme::dimRegion(t, 0, 0, w, h, 128);

    if (s_menuRow >= 0) { drawMenu(t, now); return; }
    if (s_fullRow >= 0) { drawFull(t, now); return; }

    Theme::drawListHeading(t, s_favMode ? Theme::tr("FAVOURITES", "ИЗБРАННОЕ")
                                        : Theme::tr("CHAT", "ЧАТ"), Theme::VAPOR_PINK);

    const int top    = Theme::LIST_TOP + Theme::listHeadingH();
    const int barH   = btnH();
    const int listB  = h - barH - 8;
    const uint8_t count = rowCount();

    // A solid panel under the list, as the SQUAD inbox and every row list
    // have: the messages must read against the background, not through it.
    t.fillRect(2, top - 3, w - 4, listB - top + 5, Theme::BG);
    t.drawRect(2, top - 3, w - 4, listB - top + 5, Theme::PURPLE);

    Theme::bodyFontOn(t);
    const int rh = rowHOf(t);
    int vis = (listB - top) / rh;
    if (vis < 1) vis = 1;
    if (s_scroll > (int)count - vis) s_scroll = (int)count - vis;
    if (s_scroll < 0) s_scroll = 0;
    Theme::bodyFontOff(t);

    s_rowN = 0;
    if (!count) {
        Theme::bodyFontOn(t);
        t.setTextColor(Theme::W95_SHADOW, Theme::BG);
        const char* no = s_favMode ? Theme::tr("Nothing kept yet.", "В избранном пусто.")
                                   : Theme::tr("No messages yet.", "Сообщений пока нет.");
        t.setCursor(8, top + 4);
        if (Theme::isRu(no)) Theme::printRU(t, no); else t.print(no);
        Theme::bodyFontOff(t);
    }
    int y = top;
    for (int i = s_scroll; i < (int)count && y + rh <= listB + 2; i++) {
        if (i > s_scroll) t.drawFastHLine(4, y - 1, w - 8, Theme::W95_SHADOW);
        s_rows[s_rowN++] = { 0, (int16_t)y, (int16_t)w, (int16_t)(rh - 1) };
        drawRow(t, w, y, rowMsg((uint8_t)i), now);
        y += rh;
    }
    // A quiet hint that there is more than fits, in the same thin bar the
    // other scrolled lists use.
    if ((int)count > vis) Theme::drawScrollbar(t, w - 4, top, listB - top, (int)count, vis, s_scroll);

    // The three buttons: leave, swap the list, write a new one.
    const int gap = 4, bw = (w - 2 * gap - 2 * 4) / 3;
    const int by = h - barH - 4;
    s_close = { 4, (int16_t)by, (int16_t)bw, (int16_t)barH };
    s_favs  = { (int16_t)(4 + bw + gap), (int16_t)by, (int16_t)bw, (int16_t)barH };
    s_write = { (int16_t)(4 + 2 * (bw + gap)), (int16_t)by, (int16_t)bw, (int16_t)barH };
    Theme::drawButton(t, s_close.x, s_close.y, s_close.w, s_close.h, Theme::tr("[ CLOSE ]", "[ ЗАКРЫТЬ ]"), false);
    Theme::drawButton(t, s_favs.x, s_favs.y, s_favs.w, s_favs.h,
                      s_favMode ? Theme::tr("[ INBOX ]", "[ ВХОДЯЩИЕ ]") : Theme::tr("[ KEPT ]", "[ ИЗБРАННЫЕ ]"), false);
    Theme::drawButton(t, s_write.x, s_write.y, s_write.w, s_write.h, Theme::tr("[ WRITE ]", "[ НАПИСАТЬ ]"), false);
}

ChatHit uiChatTouch(int x, int y, uint32_t now) {
    (void)now;
    // The full view owns the screen while it is up: any tap dismisses it, and
    // nothing under it can be hit by accident.
    if (s_fullRow >= 0) {
        s_fullRow = -1;
        return ChatHit::NONE;
    }
    if (s_menuRow >= 0) {
        if (Theme::pinnedBackHit(x, y, s_scrW, s_scrH)) { s_menuRow = -1; return ChatHit::NONE; }
        for (uint8_t i = 0; i < ACT_N; i++) {
            if (!in(s_mRows[i], x, y)) continue;
            switch (i) {
                case ACT_READ:  s_fullRow = s_menuRow; s_menuRow = -1; break;
                case ACT_REPLY: s_menuRow = -1; return ChatHit::REPLY;
                case ACT_DELETE:
                    if (s_favMode) MeshTalk::favRemove((uint8_t)s_menuRow);
                    else           MeshTalk::inboxRemove((uint8_t)s_menuRow);
                    s_menuRow = -1;
                    break;
                case ACT_HEART:  MeshTalk::favToggle(rowMsg((uint8_t)s_menuRow)); break;
                case ACT_LIKE:    react((uint8_t)MeshMsg::Emote::HIGH_FIVE); s_menuRow = -1; break;
                case ACT_DISLIKE: react((uint8_t)MeshMsg::Emote::GRR);       s_menuRow = -1; break;
                default: break;
            }
            return ChatHit::NONE;
        }
        return ChatHit::NONE;
    }
    if (in(s_close, x, y)) return ChatHit::CLOSE;
    if (in(s_favs, x, y))  { s_favMode = !s_favMode; s_scroll = 0; return ChatHit::NONE; }
    if (in(s_write, x, y)) return ChatHit::WRITE;
    if (Theme::pinnedBackHit(x, y, s_scrW, s_scrH)) return ChatHit::CLOSE;
    return ChatHit::NONE;
}

int uiChatRowAt(int x, int y) {
    if (overlayOpen()) return -1;
    for (uint8_t i = 0; i < s_rowN; i++)
        if (in(s_rows[i], x, y)) return (int)i + s_scroll;
    return -1;
}

int uiChatHit(int x, int y) {
    if (in(s_close, x, y)) return 1;
    if (in(s_favs, x, y))  return 2;
    if (in(s_write, x, y)) return 3;
    if (Theme::pinnedBackHit(x, y, s_scrW, s_scrH)) return 1;
    return -1;
}

#endif // POTS_MESH
