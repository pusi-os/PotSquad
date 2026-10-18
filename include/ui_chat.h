// PotsSquad-CYD — the CHAT room. See src/ui_chat.cpp.
//
// Every PusiMesh message that has arrived, newest first, with the sender, how
// long ago, and a line or two of what was said. It is the inbox the SQUAD
// panel already keeps, opened out: the same ring, a whole screen for it, and
// the things you do to a message -- read it, answer it, keep it, react to it,
// or drop it.
//
// The kept messages (ИЗБРАННЫЕ) are a store of their own, so a message kept
// here survives being pushed off the sixteen-deep ring and being deleted from
// the inbox. Kept or not is the whole of that section; everything else works
// the same on both lists.
#pragma once
#if POTS_MESH
#include <TFT_eSPI.h>
#include <stdint.h>

class DetectionEngine;

// What a press landed on. Rows are handled by uiChatRowAt/open/read, so a
// touch on one answers NONE here; these are the buttons.
enum class ChatHit : uint8_t { NONE, CLOSE, WRITE, REPLY };

void     uiChatInit(TFT_eSPI& t);
void     uiChatTick(TFT_eSPI& t, uint32_t now, const DetectionEngine& eng, bool advance = true);
ChatHit  uiChatTouch(int x, int y, uint32_t now);

// The hold gesture. The caller arms a timer when a press lands on a row,
// calls uiChatOpenMenu when it holds past the threshold, and uiChatReadRow
// when it is released before. A press on a button or in the gap answers -1.
int  uiChatRowAt(int x, int y);
// Pure button hit (no action): 1 CLOSE, 2 FAVOURITES, 3 WRITE, -1 none. The
// no-touch cursor sweeps with this; uiChatTouch() is the one that acts.
int  uiChatHit(int x, int y);
void uiChatOpenMenu(int row);
void uiChatReadRow(int row);
// A drag: positive scrolls back towards older messages.
void uiChatScroll(int delta);
// Open on the kept list, as the [ ИЗБРАННЫЕ ] button does. For the emulator,
// which has no finger to press it.
void uiChatShowFavs(bool on);
#endif
