// PotsSquad-CYD — the POTSY-DEX screen. See ui_dex.h.
#include "ui_dex.h"
#include "dex.h"
#include "theme.h"
#include "detection.h"
#include "clock.h"
#include <Arduino.h>
#include <string.h>

namespace {

const int TOP = 16;
int8_t s_card = -1;   // the entry whose card is up, or -1 for the index
int s_cardScroll = 0;   // card body pixels scrolled away at the top
int s_cardMax = 0;      // last frame's scroll range, for clamping input

// The DEX draws either script: every card field has an EN and an RU form and
// the screen picks by language through Theme::tr. Measure, wrap and print must
// go through the RU face whenever the chosen string holds Cyrillic -- the GLCD
// has no glyphs for it -- so these three route on Theme::isRu().
static void put(TFT_eSPI& t, const char* s) {
    if (Theme::isRu(s)) Theme::printRU(t, s); else t.print(s);
}
static int wid(TFT_eSPI& t, const char* s) {
    return Theme::isRu(s) ? Theme::textWidthRU(t, s) : t.textWidth(s);
}
static uint8_t wrap(TFT_eSPI& t, const char* s, int maxW, char lines[][48], uint8_t maxLines) {
    return Theme::isRu(s) ? Theme::wrapTextRU(t, s, maxW, lines, maxLines)
                          : Theme::wrapText(t, s, maxW, lines, maxLines);
}
// Wide rows for the lore: at 48 bytes a line the byte cap binds before
// the pixels do (23 Cyrillic letters), wasting a third of the card's
// width and pushing long lores past any sane line cap.
static uint8_t wrap80(TFT_eSPI& t, const char* s, int maxW, char lines[][80], uint8_t maxLines) {
    return Theme::isRu(s) ? Theme::wrapTextRU(t, s, maxW, lines, maxLines)
                          : Theme::wrapText(t, s, maxW, lines, maxLines);
}

bool caught(const DetectionEngine& eng, DetectionType t) { return eng.lifetimeTypeCount(t) > 0; }

// The tiles and the card's panels sit on the dimmed backdrop, so they get a
// fill a shade above it and an outline: the mockup's boxes, which on the
// panel's near-black BG a bare fill would not show.
uint16_t panelFill() { return Theme::blend(Theme::BG, Theme::PURPLE, 28); }
void panel(TFT_eSPI& t, int x, int y, int w, int h, uint16_t edge) {
    t.fillRect(x, y, w, h, panelFill());
    t.drawRect(x, y, w, h, edge);
}

// ---- the index -----------------------------------------------------------
// Six across in landscape, four in portrait, however many rows the seventeen
// need. The tiles fill the space between the heading and the BACK strip.
void grid(int w, int h, int& cols, int& rows, int& gx, int& gy, int& cw, int& ch) {
    cols = w > h ? 6 : 4;
    rows = (Dex::ENTRIES + cols - 1) / cols;
    const int top = TOP + Theme::listHeadingH() + (Theme::large() ? 4 : 2);
    const int bottom = h - Theme::pinnedBackH(w) - 4;
    const int pad = 4, gap = 3;
    cw = (w - 2 * pad - (cols - 1) * gap) / cols;
    ch = (bottom - top - (rows - 1) * gap) / rows;
    gx = (w - (cols * cw + (cols - 1) * gap)) / 2;
    gy = top;
}

void starsText(Dex::Rarity r, char* out) {
    const uint8_t n = Dex::stars(r);
    for (uint8_t i = 0; i < 3; i++) out[i] = i < n ? '*' : '.';
    out[3] = 0;
}

void drawIndex(TFT_eSPI& t, int w, int h, const DetectionEngine& eng) {
    int cols, rows, gx, gy, cw, ch;
    grid(w, h, cols, rows, gx, gy, cw, ch);

    char head[32];
    const uint8_t got = uiDexCaught(eng);
    snprintf(head, sizeof head, Theme::tr("%u OF %u CAUGHT", "%u ИЗ %u ПОЙМАНО"),
             (unsigned)got, (unsigned)Dex::ENTRIES);
    Theme::drawListHeading(t, head, got == Dex::ENTRIES ? Theme::AMBER : Theme::CYAN);

    Theme::bodyFontOn(t);
    for (uint8_t i = 0; i < Dex::ENTRIES; i++) {
        const int x = gx + (i % cols) * (cw + 3);
        const int y = gy + (i / cols) * (ch + 3);
        const DetectionType type = Dex::typeAt(i);
        const bool have = caught(eng, type);

        panel(t, x, y, cw, ch, have ? Theme::colorFor(type) : Theme::W95_SHADOW);

        // The number, small, where a collector expects it.
        char no[4];
        snprintf(no, sizeof no, "%02u", (unsigned)(i + 1));
        t.setTextColor(have ? Theme::colorFor(type) : Theme::W95_SHADOW, panelFill());
        t.setCursor(x + 3, y + 2);
        t.print(no);
        if (have) {
            char st[4];
            starsText(Dex::rarity(type), st);
            t.setTextColor(Theme::AMBER, panelFill());
            t.setCursor(x + cw - t.textWidth(st) - 3, y + 2);
            t.print(st);
        }

        // The icon, the same one the alert and the LOG draw; a silhouette
        // behind BINGO's scanline veil until it has been caught.
        // A little smaller and higher on the large panels, clear of the
        // body-font name under it.
        const int s = (cw < ch ? cw : ch) / 2 - (Theme::large() ? 10 : 6);
        if (s >= 4) {
            Theme::drawTypeIcon(t, type, x + cw / 2, y + ch / 2 - (Theme::large() ? 6 : 2), s);
            if (!have)
                for (int ly = y + 1; ly < y + ch - 1; ly += 2)
                    t.drawFastHLine(x + 1, ly, cw - 2, panelFill());
        }

        // The name is always shown: an uncaught tile wears it dimmed behind
        // the veil. "???" told the collector nothing about which square to
        // tap; the silhouette and the blanked lore keep the mystery instead.
        const char* name = Theme::tr(Dex::shortName(type), Dex::shortNameRU(type));
        t.setTextColor(have ? Theme::WHITE : Theme::W95_SHADOW, panelFill());
        int tw = wid(t, name);
        if (tw > cw - 4) tw = cw - 4;
        t.setCursor(x + (cw - tw) / 2, y + ch - t.fontHeight() - 2);
        put(t, name);
    }
    Theme::bodyFontOff(t);
}

// ---- a card --------------------------------------------------------------
void drawRow(TFT_eSPI& t, int x, int y, int w, const char* k, const char* v, uint16_t kc, uint16_t vc) {
    t.setTextColor(kc, panelFill());
    t.setCursor(x, y);
    put(t, k);
    t.setTextColor(vc, panelFill());
    t.setCursor(x + w - wid(t, v), y);
    put(t, v);
}

// "SEP 03" or "--"; today reads as the time.
void when(uint32_t epoch, char* out, size_t n) {
    if (!epoch) { snprintf(out, n, "--"); return; }
    Clock::formatEpochStamp(epoch, out, n);
}

void drawCard(TFT_eSPI& t, int w, int h, const DetectionEngine& eng) {
    const DetectionType type = Dex::typeAt((uint8_t)s_card);
    const bool have = caught(eng, type);
    const uint16_t col  = have ? Theme::colorFor(type) : Theme::W95_SHADOW;
    const uint16_t text = have ? Theme::WHITE : Theme::W95_SHADOW;
    const Theme::ButtonBarGeom bar = Theme::computeButtonBar(w, h);
    const Dex::Record& rec = Dex::record(type);

    // Heading: number, name, rarity. The name shows even uncaught, dimmed
    // like the tile: the heading is how a collector reads which card this is.
    char head[40];
    char st[4];
    starsText(Dex::rarity(type), st);
    snprintf(head, sizeof head, "%02u  %s  %s %s", (unsigned)(s_card + 1),
             Theme::tr(Dex::shortName(type), Dex::shortNameRU(type)),
             st, Theme::tr(Dex::rarityName(Dex::rarity(type)), Dex::rarityNameRU(Dex::rarity(type))));
    Theme::drawListHeading(t, head, have ? col : Theme::W95_LIGHT);

    Theme::bodyFontOn(t);
    const int lh = t.fontHeight() + 2;
    const bool wide = w > h;
    const int top = TOP + Theme::listHeadingH() + 4;
    const int bottom = bar.y - 4;
    // A long card (the MESH lore in Russian) runs past the buttons: the
    // body scrolls under a fixed heading and button bar, a few pixels per
    // drag, with a scrollbar while there is anywhere to go. Elements
    // straddling the region edge are clipped, never drawn over chrome.
    const int regionTop = top, regionBottom = bottom;
    const int oy = -s_cardScroll;
    const int regionH = regionBottom - regionTop;
    auto rowIn = [&](int y) { return y >= regionTop && y + lh <= regionBottom; };
    auto boxIn = [&](int y, int hh) { return y + hh > regionTop && y < regionBottom; };
    auto boxFull = [&](int y, int hh) { return y >= regionTop && y + hh <= regionBottom; };

    // Left (or top): the icon in a box, and the record under it.
    // A third of the width and a bit under a third of the height: 104x74 on
    // the 2.8", 160x99 on the 3.5", so the icon grows with the panel.
    const int boxW = wide ? w / 3 : w - 12;
    // A compact screen (240x135) has 71 rows between the heading and the
    // buttons: a shorter icon box, three of the five records, and the lore
    // cut to what fits beside them.
    const bool tiny = Theme::compact();
    const int boxH = tiny ? 26 : wide ? (h * 31) / 100 : 60;
    const int bx = 6, by = top + oy;
    if (boxIn(by, boxH)) {
        const int y0 = by < regionTop ? regionTop : by;
        const int y1 = by + boxH > regionBottom ? regionBottom : by + boxH;
        t.fillRect(bx, y0, boxW, y1 - y0, panelFill());
        if (boxFull(by, boxH)) {
            t.drawRect(bx, by, boxW, boxH, col);
            const int s = (boxW < boxH ? boxW : boxH) / 2 - 8;
            Theme::drawTypeIcon(t, type, bx + boxW / 2, by + boxH / 2, s);
        }
    }
    if (!have) {
        for (int ly = by + 1 > regionTop ? by + 1 : regionTop; ly < by + boxH - 1 && ly < regionBottom; ly += 2)
            if (boxIn(by, boxH)) t.drawFastHLine(bx + 1, ly, boxW - 2, panelFill());
        if (boxIn(by, boxH)) {
            const char* nyc = Theme::tr("NOT CAUGHT", "НЕ ПОЙМАН");
            t.setTextColor(Theme::VAPOR_PINK, panelFill());
            t.setCursor(bx + (boxW - wid(t, nyc)) / 2, by + boxH / 2 - 4);
            put(t, nyc);
        }
    }
    if (boxIn(by, boxH)) {
        t.setTextColor(Theme::W95_SHADOW, panelFill());
        const char* kd = Theme::tr(Dex::kind(type), Dex::kindRU(type));
        t.setCursor(bx + boxW - wid(t, kd) - 2, by + boxH - t.fontHeight() - 1);
        put(t, kd);
        if (have && Dex::nemesis(eng) == type) {
            // The type he has caught most: his sworn enemy, stamped on the card.
            t.setTextColor(Theme::RED, panelFill());
            t.setCursor(bx + 3, by + boxH - t.fontHeight() - 1);
            put(t, Theme::tr("NEMESIS", "НЕМЕЗИС"));
        }
    }

    // The record, in a panel of its own under the icon; and the text column
    // in one beside it (or below, in portrait), sized from what it holds.
    const int recH = (tiny ? 3 : 5) * lh + 6;
    const int recY = by + boxH + 3;
    if (boxIn(recY, recH)) {
        const int r0 = recY < regionTop ? regionTop : recY;
        const int r1 = recY + recH > regionBottom ? regionBottom : recY + recH;
        t.fillRect(bx, r0, boxW, r1 - r0, panelFill());
        if (boxFull(recY, recH)) t.drawRect(bx, recY, boxW, recH, Theme::PURPLE);
    }
    int ry = recY + 4;
    const int rx = bx + 3, rw = boxW - 6;
    char v[16];
    snprintf(v, sizeof v, "%lu", (unsigned long)eng.lifetimeTypeCount(type));
    if (rowIn(ry)) drawRow(t, rx, ry, rw, Theme::tr("CAUGHT", "ПОЙМАНО"), have ? v : "0", col, text); ry += lh;
    when(rec.firstEpoch, v, sizeof v);
    if (!tiny) { if (rowIn(ry)) drawRow(t, rx, ry, rw, Theme::tr("FIRST", "ПЕРВЫЙ"), have ? v : "--", col, text); ry += lh; }
    when(rec.lastEpoch, v, sizeof v);
    if (rowIn(ry)) drawRow(t, rx, ry, rw, Theme::tr("LAST", "ПОСЛЕДНИЙ"), have ? v : "--", col, text); ry += lh;
    if (rec.bestRssi > -128) snprintf(v, sizeof v, tiny ? "%d" : "%d dBm", (int)rec.bestRssi); else snprintf(v, sizeof v, "--");
    if (rowIn(ry)) drawRow(t, rx, ry, rw, Theme::tr("CLOSEST", "БЛИЖАЙШИЙ"), have ? v : "--", col, text); ry += lh;
    snprintf(v, sizeof v, "%u", (unsigned)rec.night);
    if (!tiny) { if (rowIn(ry)) drawRow(t, rx, ry, rw, Theme::tr("AT NIGHT", "НОЧЬЮ"), have ? v : "--", col, text); ry += lh; }

    // Right (or below): two panels. The lore (or, uncaught, its blanked
    // shape) in one sized to its lines; where it lives and what Potsy says
    // in a second that takes the rest of the height.
    const int px = wide ? bx + boxW + 4 : 6;
    const int pw = wide ? w - px - 6 : w - 12;
    const int py = (wide ? top : ry + 4) + oy;
    const int tx = px + 4, tw = pw - 8;
    char lines[9][80];
    uint8_t n;

    // First panel. Sized to the wrapped lore, uncapped by guesswork: the
    // longest RU lore needs 8 pixel-bound lines at card width, so the caps
    // sit above that (9/8) and the panel simply grows; the second panel
    // below sizes the same way, and both still fit the 320px layouts.
    int ty = py + 3;
    if (have) {
        n = wrap80(t, Theme::tr(Dex::lore(type), Dex::loreRU(type)), tw, lines, wide ? 9 : 8);
        if (tiny && n > (bottom - py - 6) / lh) n = (uint8_t)((bottom - py - 6) / lh);
        if (boxIn(py, n * lh + 6)) {
            const int q0 = py < regionTop ? regionTop : py;
            const int q1 = py + n * lh + 6 > regionBottom ? regionBottom : py + n * lh + 6;
            t.fillRect(px, q0, pw, q1 - q0, panelFill());
            if (boxFull(py, n * lh + 6)) t.drawRect(px, py, pw, n * lh + 6, Theme::PURPLE);
        }
        t.setTextColor(Theme::WHITE, panelFill());
        for (uint8_t i = 0; i < n; i++, ty += lh) {
            if (!rowIn(ty)) continue;
            t.setCursor(tx, ty); put(t, lines[i]);
        }
    } else {
        // Blanked lore: the shape of a paragraph, none of the words.
        n = wide ? 4 : 2;
        if (boxIn(py, n * lh + 6)) {
            const int q0 = py < regionTop ? regionTop : py;
            const int q1 = py + n * lh + 6 > regionBottom ? regionBottom : py + n * lh + 6;
            t.fillRect(px, q0, pw, q1 - q0, panelFill());
            if (boxFull(py, n * lh + 6)) t.drawRect(px, py, pw, n * lh + 6, Theme::W95_SHADOW);
        }
        for (uint8_t i = 0; i < n; i++, ty += lh) {
            if (!rowIn(ty)) continue;
            const int len = (i == n - 1 ? tw / 2 : tw) - 6;
            for (int x = tx; x < tx + len; x += 8) t.fillRect(x, ty + 1, 6, 6, Theme::W95_SHADOW);
        }
    }

    // Second panel, sized to what it holds: where it lives (or the hint),
    // what Potsy says, and the radio it uses as a tag in the corner.
    // Habitat and quip wrap to what they need (up to 4 lines each) rather
    // than a fixed 2: the longer RU ones were losing their last line.
    // Past the buttons the card scrolls instead of clipping: drag the body.
    const int p2y = py + n * lh + 6 + 3;
    char hab[4][48], say[4][48];
    const uint8_t nh = wrap(t, have ? Theme::tr(Dex::habitat(type), Dex::habitatRU(type))
                                    : Theme::tr(Dex::hint(type),    Dex::hintRU(type)), tw, hab, 4);
    const uint8_t ns = wrap(t, Theme::tr(Dex::quip(type), Dex::quipRU(type)), tw, say, 4);
    int p2h = 3 + lh + nh * lh + 3 + lh + ns * lh + 3 + lh + 3;
    // Not clamped to the buttons: past them the card scrolls instead, and
    // the clipped fill plus the row guards below keep every line inside
    // the body region while it does.
    if (boxIn(p2y, p2h) && p2h >= 2 * lh + 6) {
        const int q0 = p2y < regionTop ? regionTop : p2y;
        const int q1 = p2y + p2h > regionBottom ? regionBottom : p2y + p2h;
        t.fillRect(px, q0, pw, q1 - q0, panelFill());
        if (boxFull(p2y, p2h)) t.drawRect(px, p2y, pw, p2h, Theme::PURPLE);
        const int p2b = p2y + p2h;
        ty = p2y + 3;
        if (rowIn(ty)) {
            t.setTextColor(Theme::CYAN, panelFill()); t.setCursor(tx, ty);
            put(t, Theme::tr(have ? "HABITAT" : "HINT", have ? "СРЕДА" : "ПОДСКАЗКА"));
        }
        ty += lh;
        t.setTextColor(Theme::W95_LIGHT, panelFill());
        for (uint8_t i = 0; i < nh; i++, ty += lh) {
            if (!rowIn(ty) || ty + lh > p2b) continue;
            t.setCursor(tx, ty); put(t, hab[i]);
        }
        ty += 3;
        if (ty + 2 * lh <= p2b && rowIn(ty)) {
            t.setTextColor(Theme::CYAN, panelFill()); t.setCursor(tx, ty); put(t, Theme::tr("POTSY SAYS", "ПОТСИ ГОВОРИТ")); ty += lh;
            t.setTextColor(Theme::AMBER, panelFill());
            for (uint8_t i = 0; i < ns; i++, ty += lh) {
                if (!rowIn(ty) || ty + lh > p2b) continue;
                t.setCursor(tx, ty); put(t, say[i]);
            }
        }
        if (rowIn(p2b - t.fontHeight() - 3)) {
            t.setTextColor(col, panelFill());
            t.setCursor(tx, p2b - t.fontHeight() - 3);
            put(t, Theme::tr(Dex::radio(type), Dex::radioRU(type)));
        }
    }
    // Scroll range for this frame: the next drag is clamped to it, and a
    // scrollbar shows while there is anywhere to go.
    {
        int contentEnd = p2y + p2h;
        const int recEnd = by + boxH + 3 + recH;
        if (recEnd > contentEnd) contentEnd = recEnd;
        int maxS = contentEnd - regionBottom;
        if (maxS < 0) maxS = 0;
        s_cardMax = maxS;
        // s_cardMax is 0 until the first frame measures the card: don't
        // clamp a seeded scroll to an unknown range (it would stick at 0).
        if (s_cardMax > 0 && s_cardScroll > s_cardMax) s_cardScroll = s_cardMax;
        if (s_cardMax > 0)
            Theme::drawScrollbar(t, w - 4, regionTop, regionH, regionH + s_cardMax, regionH, s_cardScroll);
    }

    Theme::bodyFontOff(t);
    Theme::drawButton(t, bar.x[0], bar.y, bar.w[0], bar.h, "[ < ]", false);
    Theme::drawButton(t, bar.x[1], bar.y, bar.w[1], bar.h, Theme::tr("[ DEX ]", "[ ДЕКС ]"), false);
    Theme::drawButton(t, bar.x[2], bar.y, bar.w[2], bar.h, "[ > ]", false);
}

}  // namespace

void uiDexInit(TFT_eSPI& t) {
    s_card = -1;
    t.fillRect(0, 0, t.width(), t.height(), Theme::BG);
}

void uiDexOpenCard(uint8_t entry) {
    s_card = entry < Dex::ENTRIES ? (int8_t)entry : -1;
    s_cardScroll = 0;
    s_cardMax = 0;
}

void uiDexCardScroll(int pixels) {
    // Floor only: the ceiling comes from the draw, which is the only
    // place that knows this card's content height.
    s_cardScroll += pixels;
    if (s_cardScroll < 0) s_cardScroll = 0;
}

uint8_t uiDexCaught(const DetectionEngine& eng) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < Dex::ENTRIES; i++) if (caught(eng, Dex::typeAt(i))) n++;
    return n;
}

void uiDexTick(TFT_eSPI& t, uint32_t now, const DetectionEngine& eng, bool advance) {
    const int w = t.width(), h = t.height();
    Theme::Palette saved = Theme::dimPaletteForOverlay(179);
    Theme::drawActiveBackground(t, now, 0, h, eng, advance);
    Theme::restorePalette(saved);

    Theme::drawTitleBar(t, Theme::tr(">> POTSY-DEX <<", ">> ПОТСИ-ДЕКС <<"));
    if (s_card < 0) {
        drawIndex(t, w, h, eng);
        Theme::drawPinnedBack(t, Theme::tr("[ BACK ]", "[ НАЗАД ]"));
    } else {
        drawCard(t, w, h, eng);
    }
}

uint8_t uiDexStops(int screenW, int screenH, int16_t* xs, int16_t* ys, uint8_t cap) {
    uint8_t n = 0;
    auto add = [&](int x, int y) { if (n < cap) { xs[n] = (int16_t)x; ys[n] = (int16_t)y; n++; } };
    if (s_card < 0) {
        int cols, rows, gx, gy, cw, ch;
        grid(screenW, screenH, cols, rows, gx, gy, cw, ch);
        for (int i = 0; i < Dex::ENTRIES && i < cols * rows; i++)
            add(gx + (i % cols) * (cw + 3) + cw / 2, gy + (i / cols) * (ch + 3) + ch / 2);
        add(screenW / 2, screenH - Theme::pinnedBackH(screenW) / 2);
        return n;
    }
    const Theme::ButtonBarGeom bar = Theme::computeButtonBar(screenW, screenH);
    add(bar.x[2] + bar.w[2] / 2, bar.y + bar.h / 2);   // next
    add(bar.x[0] + bar.w[0] / 2, bar.y + bar.h / 2);   // previous
    add(bar.x[1] + bar.w[1] / 2, bar.y + bar.h / 2);   // back to the index
    return n;
}

DexTap uiDexHitTest(TFT_eSPI& t, int x, int y, int screenW, int screenH) {
    (void)t;
    if (s_card < 0) {
        if (Theme::pinnedBackHit(x, y, screenW, screenH)) return DexTap::BACK;
        int cols, rows, gx, gy, cw, ch;
        grid(screenW, screenH, cols, rows, gx, gy, cw, ch);
        if (x < gx || y < gy) return DexTap::NONE;
        const int c = (x - gx) / (cw + 3), r = (y - gy) / (ch + 3);
        if (c >= cols || r >= rows) return DexTap::NONE;
        const int i = r * cols + c;
        if (i >= Dex::ENTRIES) return DexTap::NONE;
        s_card = (int8_t)i;
        return DexTap::HANDLED;
    }
    switch (Theme::hitTestButtonBar(x, y, screenW, screenH)) {
        case ButtonId::SCAN: s_card = (int8_t)((s_card + Dex::ENTRIES - 1) % Dex::ENTRIES); s_cardScroll = 0; s_cardMax = 0; return DexTap::HANDLED;
        case ButtonId::LOG:  s_card = -1; return DexTap::HANDLED;
        case ButtonId::CLR:  s_card = (int8_t)((s_card + 1) % Dex::ENTRIES); s_cardScroll = 0; s_cardMax = 0; return DexTap::HANDLED;
        default: return DexTap::NONE;
    }
}
