// PotsSquad-CYD — on-device diagnostics screen implementation
#include "ui_diagnostics.h"
#include "clock.h"
#include "theme.h"
#include "detection.h"
#include "settings.h"
#if POTS_MESH
#include "meshtalk.h"
#endif
#include <Arduino.h>
#include <stdarg.h>
#if defined(CYD_GPS)
#include "sd_row.h"   // degreesText() for the FIX line
#endif

static void backButtonRect(int screenW, int screenH, int& x, int& y, int& w, int& h) {
    Theme::ButtonBarGeom g = Theme::computeButtonBar(screenW, screenH);
    w = 120;
    h = g.h;
    x = (screenW - w) / 2;
    y = g.y;
}

void uiDiagnosticsInit(TFT_eSPI& t) {
    t.fillRect(0, 0, t.width(), t.height(), Theme::BG);
}

bool uiDiagnosticsHitBack(int x, int y, int screenW, int screenH) {
    int bx, by, bw, bh;
    backButtonRect(screenW, screenH, bx, by, bw, bh);
    return x >= bx && x <= bx + bw && y >= by && y <= by + bh;
}

// A compact screen (240x135) shows the report a page at a time: lines are
// laid out as usual, shifted up by the page, and the ones outside the body
// are not drawn. A tap on the body turns the page.
static int s_page = 0, s_pages = 1, s_clipTop = 0, s_clipBottom = 32767, s_off = 0;
void uiDiagnosticsNextPage() { s_page = (s_page + 1) % (s_pages > 0 ? s_pages : 1); }

// Mixed-script output for the report's 8px rows: ASCII keeps the GLCD face,
// a string with Cyrillic goes through the 9px RU face. Measure and draw both
// route here so the value column lines up under either script.
static void put(TFT_eSPI& t, const char* s) {
    if (Theme::isRu(s)) Theme::printRU(t, s);
    else                t.print(s);
}
static int wid(TFT_eSPI& t, const char* s) {
    return Theme::isRu(s) ? Theme::textWidthRU(t, s) : t.textWidth(s);
}

static int drawLine(TFT_eSPI& t, int y, uint16_t labelColor, const char* label, const char* fmt, ...) {
    const int fh = t.fontHeight();
    const int sy = y - s_off;
    if (sy < s_clipTop || sy + fh > s_clipBottom) return y + fh + 2;
    y = sy;
    t.setTextColor(labelColor, Theme::BG);
    t.setCursor(6, y);
    put(t, label);

    char buf[48];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    t.setTextColor(Theme::WHITE, Theme::BG);
    t.setCursor(6 + wid(t, label) + 6, y);
    put(t, buf);
    return y + s_off + t.fontHeight() + 2;
}

void uiDiagnosticsTick(TFT_eSPI& t, uint32_t now, const DetectionEngine& eng, const DiagnosticsInfo& info) {
    (void)now;
    int w = t.width(), h = t.height();

    Theme::drawTitleBar(t, Theme::tr(">> DIAGNOSTICS <<", ">> ДИАГНОСТИКА <<"));

    Theme::ButtonBarGeom bar = Theme::computeButtonBar(w, h);
    int bodyTop = 16, bodyBottom = bar.y - 4;
    t.fillRect(0, bodyTop, w, bodyBottom - bodyTop, Theme::BG);

    t.setTextSize(1);
    t.setTextWrap(false);
    int y = bodyTop + 2;
    const bool paged = Theme::compact();
    const int pageH = (bodyBottom - bodyTop - 2) / (t.fontHeight() + 2) * (t.fontHeight() + 2);
    s_clipTop = paged ? bodyTop : 0;
    s_clipBottom = paged ? bodyBottom : 32767;
    s_off = paged ? s_page * pageH : 0;

    y = drawLine(t, y, Theme::CYAN, Theme::tr("BOARD:", "ПЛАТА:"), "%s (%s)", info.boardName,
                 info.usingCapTouch ? Theme::tr("capacitive", "ёмкостный") : Theme::tr("resistive", "резистивный"));
    y = drawLine(t, y, Theme::CYAN, Theme::tr("RESET:", "СБРОС:"), "%s", info.resetReason);
    // Only after an actual panic, and only when the breadcrumb survived. A
    // blank line here means a clean boot, not a missing feature.
    if (info.crash.valid || info.crash.haveDump) {
        if (info.crash.valid)
            y = drawLine(t, y, Theme::RED, Theme::tr("LAST CRASH:", "ПАДЕНИЕ:"),
                         Theme::tr("%lum%lus up, %lu free, %lu block", "%luм%luс работы, %lu своб, %lu блок"),
                         (unsigned long)(info.crash.uptimeMs / 60000),
                         (unsigned long)((info.crash.uptimeMs / 1000) % 60),
                         (unsigned long)info.crash.heapFree,
                         (unsigned long)info.crash.heapBlock);
        else
            y = drawLine(t, y, Theme::RED, Theme::tr("LAST CRASH:", "ПАДЕНИЕ:"),
                         Theme::tr("no breadcrumb survived", "хлебных крошек нет"));
        if (info.crash.haveDump) {
            // Where it died, from the core dump. With these two lines and the
            // ELF of the build that crashed, addr2line names the functions --
            // which is the difference between "it crashed" and a fix.
            y = drawLine(t, y, Theme::RED, Theme::tr("  IN:", "  ГДЕ:"), "%s @ %08lX%s", info.crash.task,
                         (unsigned long)info.crash.pc, info.crash.dumpOlder ? Theme::tr(" (old fw)", " (старая прошивка)") : "");
            char bt[40] = "--";
            size_t o = 0;
            for (uint8_t i = 0; i < info.crash.btN && i < 3; i++)
                o += snprintf(bt + o, sizeof bt - o, "%s%08lX", i ? " " : "",
                              (unsigned long)info.crash.bt[i]);
            y = drawLine(t, y, Theme::RED, Theme::tr("  BT:", "  СТЕК:"), "%s", bt);
        } else {
            y = drawLine(t, y, Theme::RED, Theme::tr("  ON:", "  НА ЭКРАНЕ:"),
                         Theme::tr("screen %u, %lu detections", "экран %u, %lu найдено"),
                         (unsigned)info.crash.screen, (unsigned long)info.crash.lifetime);
        }
    }
    // Uptime next to the reset reason on purpose: together they answer
    // "did this thing restart on me", which is one question and not two.
    {
        char up[24];
        Clock::formatUptime(up, sizeof(up));
        y = drawLine(t, y, Theme::CYAN, Theme::tr("UPTIME:", "АПТАЙМ:"), "%s", up);
        char clk[32];
        Clock::formatClock(clk, sizeof(clk));
        y = drawLine(t, y, Theme::CYAN, Theme::tr("CLOCK:", "ЧАСЫ:"), "%s%s", clk,
                     Clock::trusted() ? "" : Theme::tr("  (TIME <epoch> over serial)", "  (ВРЕМЯ <epoch> по serial)"));
    }
    y = drawLine(t, y, Theme::CYAN, Theme::tr("HEAP:", "ПАМЯТЬ:"),
                 Theme::tr("%lu free / %lu largest", "%lu своб / %lu макс"),
                 (unsigned long)info.freeHeap, (unsigned long)info.largestBlock);
#if defined(CYD_GPS)
    if (info.gpsShown) {
        if (info.gpsGood == 0)
            y = drawLine(t, y, Theme::CYAN, "GPS:", Theme::tr("no data from the module", "нет данных от модуля"));
        else
            y = drawLine(t, y, Theme::CYAN, "GPS:", Theme::tr("%u in view, %u heard, %u used", "%u в поле, %u слышно, %u исп"),
                         info.view, info.heard, info.used);
        if (info.fixAgeMs == UINT32_MAX) {
            y = drawLine(t, y, Theme::CYAN, Theme::tr("FIX:", "ФИКС:"), Theme::tr("none", "нет"));
        } else {
            char la[16], lo[16], acc[12];
            SdRow::degreesText(la, sizeof la, info.lat7, 5);
            SdRow::degreesText(lo, sizeof lo, info.lon7, 5);
            if (info.faked) snprintf(acc, sizeof acc, Theme::tr("FAKE", "ФЕЙК"));
            else            snprintf(acc, sizeof acc, Theme::tr("+-%u m", "+-%u м"), info.accM);
            y = drawLine(t, y, info.fresh ? Theme::GREEN : Theme::CYAN, Theme::tr("FIX:", "ФИКС:"), "%s,%s %s %lus",
                         la, lo, acc, (unsigned long)(info.fixAgeMs / 1000));
        }
        // Two lines: the file name does not fit beside the counts at 320 px.
        static const char* const WD[] = { "OFF", "NO CARD", "LOW MEMORY", "WAITING FOR FIX", "ON" };
        static const char* const WD_RU[] = { "ВЫКЛ", "НЕТ КАРТЫ", "МАЛО ПАМЯТИ", "ЖДЁТ ФИКСА", "ВКЛ" };
        const bool ruWd = Settings::lang() == 1;
        if (!info.wdOn)
            y = drawLine(t, y, Theme::CYAN, "WARDRIVE:", Theme::tr("off", "выкл"));
        else
            y = drawLine(t, y, Theme::CYAN, "WARDRIVE:",
                         Theme::tr("%s, %lu written, %lu repeats, %lu dropped",
                                   "%s, %lu записано, %lu повторов, %lu отброшено"),
                         (info.wdState < 5 ? (ruWd ? WD_RU : WD)[info.wdState] : "?"), (unsigned long)info.wdWritten,
                         (unsigned long)info.wdSkipped, (unsigned long)info.wdDropped);
        y = drawLine(t, y, Theme::CYAN, "WIGLE:", "%s", info.wdFile && *info.wdFile ? info.wdFile : Theme::tr("no file yet", "файла ещё нет"));
    }
#endif
#if POTS_MESH
    // Unlike BLE SEEN's old "advertising" flag, this is read from NimBLE's
    // actual controller state. Failed starts can leave the UI looking like
    // SEND worked; show the failure count and last failure stage on-device.
    {
        const Mesh::TxStatus tx = Mesh::txStatus();
        y = drawLine(t, y, tx.advertising ? Theme::GREEN : Theme::AMBER,
                     Theme::tr("MESH TX:", "MESH ПЕРЕД:"), "%s %s, ok %lu / fail %lu (%u)",
                     tx.advertising ? Theme::tr("ON", "ВКЛ") : Theme::tr("OFF", "ВЫКЛ"),
                     tx.sendingFrame ? Theme::tr("FRAME", "КАДР") : Theme::tr("IDLE", "ПАУЗА"),
                     (unsigned long)tx.starts, (unsigned long)tx.failures,
                     (unsigned)tx.lastFailure);
        const MeshTalk::RxDiagnostics rx = MeshTalk::rxDiagnostics();
        y = drawLine(t, y, Theme::CYAN, Theme::tr("MESH RX:", "MESH ПРИЁМ:"),
                     Theme::tr("%lu queued / %lu heard, full %lu", "%lu в очереди / %lu услышано, полн %lu"),
                     (unsigned long)rx.queued, (unsigned long)rx.seen,
                     (unsigned long)rx.queueFull);
        y = drawLine(t, y, Theme::CYAN, Theme::tr("INV RX:", "INV ПРИЁМ:"),
                     Theme::tr("parts %lu mask %x/15 done %lu", "части %lu маска %x/15 готово %lu"),
                     (unsigned long)rx.inviteParts, (unsigned)rx.mask,
                     (unsigned long)rx.inviteComplete);
        y = drawLine(t, y, rx.inviteRejected ? Theme::AMBER : Theme::CYAN,
                     Theme::tr("INV ERR:", "INV ОШИБКА:"),
                     Theme::tr("%lu rejected (%u), replay %lu", "%lu отклонено (%u), повтор %lu"),
                     (unsigned long)rx.inviteRejected,
                     (unsigned)rx.lastReject,
                     (unsigned long)rx.replayRejected);
    }
#endif
    // Where the heap went on the way up, in KB: free/largest with WiFi up,
    // with Bluetooth up, and at the first pass of loop().
    {
        const BootHeap bh = bootHeap();
        y = drawLine(t, y, Theme::CYAN, Theme::tr("BOOT:", "ЗАГРУЗКА:"),
                     Theme::tr("wifi %lu/%lu  ble %lu/%lu  loop %lu/%lu KB",
                               "wifi %lu/%lu  ble %lu/%lu  loop %lu/%lu КБ"),
                     (unsigned long)(bh.wifiFree / 1024), (unsigned long)(bh.wifiLargest / 1024),
                     (unsigned long)(bh.bleFree / 1024),  (unsigned long)(bh.bleLargest / 1024),
                     (unsigned long)(info.loopFree / 1024), (unsigned long)(info.loopLargest / 1024));
    }
    y = drawLine(t, y, Theme::CYAN, Theme::tr("SLOT:", "СЛОТ:"), Theme::tr("%s  other: %s", "%s  другой: %s"),
                 info.otaSlot ? info.otaSlot : "?", info.otaOther ? info.otaOther : Theme::tr("none", "нет"));
    // The black box: what is kept in flash across restarts. The newest crash
    // kept by date and version, which is what a photo of this screen needs
    // to match it to a build; BLACKBOX on the console has the rest.
    if (!info.bbReady) {
        y = drawLine(t, y, Theme::CYAN, Theme::tr("KEPT:", "ХРАНИТСЯ:"), Theme::tr("off (flash space in use)", "выкл (флеш занята)"));
    } else if (!info.bbCrashes) {
        y = drawLine(t, y, Theme::CYAN, Theme::tr("KEPT:", "ХРАНИТСЯ:"),
                     Theme::tr("%u seen, no crashes", "%u увидено, падений нет"), (unsigned)info.bbKept);
    } else {
        char when[12] = "";
        if (info.bbHaveLast) Clock::formatEpochStamp(info.bbLast.epoch, when, sizeof when);
        // The version only where it fits: portrait is forty characters.
        const bool wide = w >= 300;
        if (Settings::lang() == 1)
            y = drawLine(t, y, Theme::RED, Theme::tr("KEPT:", "ХРАНИТСЯ:"),
                         "%u увидено, падений %u, посл %s%s%s",
                         (unsigned)info.bbKept, (unsigned)info.bbCrashes,
                         when, wide ? " v" : "", wide ? info.bbLast.version : "");
        else
            y = drawLine(t, y, Theme::RED, Theme::tr("KEPT:", "ХРАНИТСЯ:"), "%u seen, %u crash%s, last %s%s%s",
                         (unsigned)info.bbKept, (unsigned)info.bbCrashes, info.bbCrashes == 1 ? "" : "es",
                         when, wide ? " v" : "", wide ? info.bbLast.version : "");
    }
    y += 4;

    if (info.hasRaw) {
        y = drawLine(t, y, Theme::VAPOR_PURPLE, Theme::tr("RAW TOUCH:", "RAW ТАЧ:"), "%s a=%d b=%d",
                     info.rawTouching ? Theme::tr("DOWN", "ВНИЗ") : Theme::tr("up", "вверх"), info.rawA, info.rawB);
    }
    y = drawLine(t, y, Theme::VAPOR_PURPLE, Theme::tr("MAPPED:", "ЭКРАН:"), "%s x=%d y=%d",
                 info.touchValid ? Theme::tr("valid", "ок") : "--", info.mappedX, info.mappedY);
    y += 4;

    y = drawLine(t, y, Theme::VAPOR_PINK, Theme::tr("CAL SOURCE:", "КАЛИБРОВКА:"), "%s",
                 info.calSource ? info.calSource
                                : (info.usingSavedCal ? Theme::tr("saved", "сохранено") : Theme::tr("compiled-in default", "по умолчанию")));
    y = drawLine(t, y, Theme::VAPOR_PINK, Theme::tr("CAL RANGE:", "ДИАПАЗОН:"), "A[%d,%d] B[%d,%d]",
                 info.calA0, info.calA1, info.calB0, info.calB1);
    y += 4;

    // Frame cost. The push is a fixed byte count over SPI, so it moves
    // only if the bus clock does -- kept as its own figure so a change to
    // SPI_FREQUENCY shows up here unambiguously instead of being averaged
    // into one "it feels smoother" number. fps is derived from the whole
    // frame, not the push alone. On one line with FRAME since the crash
    // report grew a line: the screen already ran into the BACK button.
    {
        uint32_t fus = info.frameUs ? info.frameUs : 1;
        y = drawLine(t, y, Theme::AMBER, Theme::tr("FRAME:", "КАДР:"),
                     Theme::tr("%lu.%lu ms (%lu fps), push %lu.%lu", "%lu.%lu мс (%lu к/с), push %lu.%lu"),
                     (unsigned long)(fus / 1000),
                     (unsigned long)((fus % 1000) / 100),
                     (unsigned long)(1000000UL / fus),
                     (unsigned long)(info.pushUs / 1000),
                     (unsigned long)((info.pushUs % 1000) / 100));
        // FRAME above is this screen, which has no backdrop. LAST is the
        // screen you came from -- its whole frame, which is the frame rate
        // you actually saw there -- and the part of it its background took.
        if (info.lastScreenName && info.lastScreenUs) {
            const uint32_t lus = info.lastScreenUs;
            y = drawLine(t, y, Theme::AMBER, Theme::tr("LAST:", "ПРЕДЫДУЩИЙ:"),
                         Theme::tr("%s %lu.%lu ms (%lu fps), bg %lu.%lu", "%s %lu.%lu мс (%lu к/с), bg %lu.%lu"),
                         info.lastScreenName,
                         (unsigned long)(lus / 1000), (unsigned long)((lus % 1000) / 100),
                         (unsigned long)(1000000UL / lus),
                         (unsigned long)(info.bgUs / 1000), (unsigned long)((info.bgUs % 1000) / 100));
        } else {
            y = drawLine(t, y, Theme::AMBER, Theme::tr("LAST:", "ПРЕДЫДУЩИЙ:"),
                         Theme::tr("no screen timed yet, bg %lu.%lu", "экран ещё не замерен, bg %lu.%lu"),
                         (unsigned long)(info.bgUs / 1000), (unsigned long)((info.bgUs % 1000) / 100));
        }
    }
    y += 4;

    y = drawLine(t, y, Theme::GREEN, Theme::tr("LOG:", "ЛОГ:"),
                 Theme::tr("%u entries, %lu lifetime", "%u записей, %lu всего"),
                 (unsigned)eng.logCount(), (unsigned long)eng.lifetimeTotal());

#if POTS_MESH
    // Phase 0, read without a computer. The device alternates advertising on
    // and off in 30s arms and pools each separately, so leaving it somewhere
    // for ten minutes gives ten samples of each rather than one of a good
    // moment and one of a bad one.
    {
        y += 4;
        const MeshProbe::Stats ms = MeshProbe::stats();
        y = drawLine(t, y, ms.advOn ? Theme::GREEN : Theme::CYAN, Theme::tr("BLE SEEN:", "BLE ВИДНО:"), "%u.%u /s, %s%s",
                     (unsigned)(ms.offRate / 10), (unsigned)(ms.offRate % 10),
                     ms.advOn ? Theme::tr("advertising", "реклама") : Theme::tr("not advertising", "не реклама"),
                     scanPassiveNow() ? Theme::tr(", passive", ", пассив") : Theme::tr(", active", ", актив"));
        // The seatbelt's count: adverts refused because the largest free
        // block was under 1.5 KB when they came in. A big number here with
        // a small HEAP line is a board that is only alive because it is
        // ignoring the radio.
        if (advertsDropped())
            y = drawLine(t, y, Theme::VAPOR_PINK, Theme::tr("DROPPED:", "ПОТЕРЯНО:"),
                         Theme::tr("%lu adverts, no heap for them", "%lu реклам, нет памяти"),
                         (unsigned long)advertsDropped());
        // What the once-a-minute scan restart gave back: the leak it closes,
        // measured. This row used to repeat the HEAP line at the top, which
        // already carries free and largest block. Kilobytes a restart in a busy
        // room and a flat heap up top means the leak was NimBLE holding on to
        // devices that never answered; zeros here and a falling heap mean it
        // was not, and the search goes on.
        const ScanFlushStats fs = scanFlushStats();
        y = drawLine(t, y, Theme::VAPOR_PINK, Theme::tr("FLUSHED:", "ОЧИЩЕНО:"),
                     Theme::tr("%lu KB total, %lu B last (x%lu)", "%lu КБ всего, %lu Б посл (x%lu)"),
                     (unsigned long)(fs.totalFreed / 1024), (unsigned long)fs.lastFreed,
                     (unsigned long)fs.count);
    }
#endif

    if (paged) {
        s_pages = (y - bodyTop - 2 + pageH - 1) / pageH;
        if (s_pages < 1) s_pages = 1;
        if (s_page >= s_pages) s_page = 0;
        char pg[12];
        snprintf(pg, sizeof pg, "%d/%d", s_page + 1, s_pages);
        t.setTextColor(Theme::W95_SHADOW, Theme::BG);
        t.setCursor(6, bar.y + (bar.h - t.fontHeight()) / 2);
        t.print(pg);
    }
    s_clipTop = 0; s_clipBottom = 32767; s_off = 0;
    int bx, by, bw, bh;
    backButtonRect(w, h, bx, by, bw, bh);
    Theme::drawButton(t, bx, by, bw, bh, Theme::tr("[ BACK ]", "[ НАЗАД ]"), false);
}
