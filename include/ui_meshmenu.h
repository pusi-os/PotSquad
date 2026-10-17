// PotsSquad-CYD — the PusiMesh menu, reached via Settings' "PUSIMESH"
// row.
//
// It exists because PusiMesh is more than one decision and they are not
// the same decision. Detecting and transmitting have genuinely different
// consequences -- one shows you other people's Potsys, the other tells
// everybody with a scanner that you are here -- and collapsing them into a
// single ON/OFF was reported as broken twice before it was split.
//
// It also buys a row back on the Settings screen, which carries twenty-five
// and has three that collide in portrait: the NAME row moved in here, so two
// rows out there became one.
//
// Deliberately not scrollable. Seven rows fit, and a list that cannot
// overflow does not need the gesture -- see uiDetFilterScroll for the one
// that does.
#pragma once
#if POTS_MESH
#include <TFT_eSPI.h>
#include <stdint.h>

class DetectionEngine;

// What a tap landed on. NONE means it hit a gap.
// In the order they are drawn -- the hit test maps a row index straight onto
// this, so the two orders must never differ. TRANSPORT is the ninth row; the
// two CONFIRM values are returned instead of a row while the switch-to-ESP-NOW
// warning is up.
enum class MeshMenuRow : uint8_t { DETECT, TRANSMIT, MESSAGES, HEADSUP, CROWD, SQUAD, PHRASE, NAME, TRANSPORT, BACK, NONE, CONFIRM_YES, CONFIRM_NO };

void uiMeshMenuInit(TFT_eSPI& t);

// The switch-to-ESP-NOW warning. Raising it draws the explanation over the
// menu; the menu is on screen the whole time (nothing changes until YES), so
// the caller raises it, and the hit test answers CONFIRM_YES / CONFIRM_NO
// until it ends.
void uiMeshMenuStartConfirm();
void uiMeshMenuEndConfirm();
bool uiMeshMenuConfirming();
// Rows, on a screen too short to show all of them at once (the StickS3).
void uiMeshMenuScroll(int delta);
// Takes the engine only for the backdrop: THE GIBSON reads the log and the
// live per-channel activity, so it cannot be drawn without one. A screen
// that quietly served digital rain instead because it had no engine is a
// mistake this codebase has already made once.
// `advance` is false on the second of the 3.5"'s two band passes -- the
// same frame drawn again -- so anything that steps by the call rather
// than by the clock must sit still for it. Other boards draw once.
void uiMeshMenuTick(TFT_eSPI& t, uint32_t now, const DetectionEngine& eng, bool advance = true);
MeshMenuRow uiMeshMenuHitTest(TFT_eSPI& t, int x, int y, int screenW, int screenH);
#endif
