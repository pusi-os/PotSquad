// PotsSquad-CYD — PusiMesh, the half that does not touch a radio.
//
// Who is visiting, what their advert said, when they were last heard, and
// where a message frame goes: all of it is bytes in and state out, so it lives
// here where the emulator can compile it too. The radio half -- NimBLE
// advertising and our own address -- is Mesh::radioTick() in detection.cpp on
// the device and in sim/meshsim.cpp in the emulator.
//
// That split is the point. The emulator's virtual peer feeds real advert bytes
// into the same onManufacturerData() a real scan callback calls, so a visit in
// the emulator goes through the real decoder, the real one-visitor rule and the
// real staleness timeout rather than a copy of them that could drift.
#include "detection.h"

#if POTS_MESH
#include "pusimesh.h"
#include "potsy.h"
#include "settings.h"
#include "meshmsg.h"
#include "meshtalk.h"
#include <string.h>

namespace Mesh {

// One visitor at a time -- decided deliberately, and for screen space rather
// than memory: 320x240 already holds Potsy, a pet, twelve counters and
// three buttons. A second arrival while somebody is here is dropped rather
// than queued, because a visit is a moment and not a message that has to be
// delivered.
static PusiMesh::Peer s_peer{};
static uint8_t          s_peerMac[6] = {0};
static uint32_t         s_peerSeen   = 0;
static bool             s_havePeer   = false;
// A peer's name from its advert, kept for the message frame that arrives in
// the same scan callback. Written and read only in the BLE task.
static uint8_t          s_nameMac[6] = { 0 };
static char             s_name[13]   = { 0 };

// How long a peer survives without being heard from again. Adverts go out
// every 1500ms, but a scanner does not hear every one: at the usual scan
// window the watch caught one in three for stretches (a 4.5 s gap, bench
// 2026-09-24), and a wrist over the antenna eats more. At 12 s that walked
// squad members off the watch's screen and straight back on. 20 s is four
// such gaps -- long enough that a pocket, a wall or an arm does not end a
// visit, short enough that somebody who actually left soon stops standing on
// your screen.
static const uint32_t PEER_STALE_MS = 20000;

// Every PotsSquad heard lately, not just the one visiting: the count behind
// the small "+2" beside the visitor. Addresses only, eight of them, RAM only,
// each gone twenty seconds after it stops being heard. Written in the BLE task
// and read in the loop; a count wrong by one for a frame is the worst a race
// here can do.
static const uint8_t SQUAD_N = 8;
static uint8_t  s_squadMac[SQUAD_N][6];
static uint32_t s_squadSeen[SQUAD_N];
static bool     s_squadLive[SQUAD_N];
static PusiMesh::Peer s_squadPeer[SQUAD_N];   // what each one looks like

// The look extension, keyed by the address it came from. The presence advert
// gives us who somebody is; this gives us the colours of the clothes that a
// presence payload has no room for (see pusimesh.h). Kept beside the squad
// table because both answer "what a peer looks like", but separate because
// the extension may be absent -- a named peer's advert is too big to carry
// it -- and the identity must survive that.
static const uint8_t  LOOK_N = 8;
static uint8_t        s_lookMac[LOOK_N][6];
static PusiMesh::Look s_lookVal[LOOK_N];
static uint32_t       s_lookSeen[LOOK_N];
static bool           s_lookLive[LOOK_N];

static void noteLook(const uint8_t* mac, const PusiMesh::Look& l, uint32_t now) {
    uint8_t slot = LOOK_N;
    for (uint8_t i = 0; i < LOOK_N; i++)
        if (s_lookLive[i] && memcmp(s_lookMac[i], mac, 6) == 0) { slot = i; break; }
    if (slot == LOOK_N) {
        slot = 0;
        for (uint8_t i = 0; i < LOOK_N; i++) {
            if (!s_lookLive[i]) { slot = i; break; }
            if ((int32_t)(s_lookSeen[i] - s_lookSeen[slot]) < 0) slot = i;
        }
        memcpy(s_lookMac[slot], mac, 6);
        s_lookLive[slot] = true;
    }
    s_lookVal[slot]  = l;
    s_lookSeen[slot] = now;
}

bool lookFor(const uint8_t mac[6], PusiMesh::Look& out) {
    const uint32_t now = millis();
    for (uint8_t i = 0; i < LOOK_N; i++) {
        if (!s_lookLive[i] || memcmp(s_lookMac[i], mac, 6) != 0) continue;
        if ((int32_t)(now - s_lookSeen[i]) > (int32_t)PEER_STALE_MS) return false;
        out = s_lookVal[i];
        return true;
    }
    return false;
}

// The visitor somebody picked on the SQUAD screen, if any. RAM only.
static uint8_t  s_preferMac[6] = { 0 };
static bool     s_preferSet    = false;

// Phones heard lately: the address each one advertises from, and the six
// bytes its frames are sealed under (pusimesh.h, phoneMac). A phone's
// address changes every quarter of an hour; its identity does not, so a
// frame is opened, and replay-checked, by the identity. Four is plenty:
// this is the companion app, not a crowd. Written and read in the BLE task.
static const uint8_t PHONE_N = 4;
static uint8_t  s_phoneAddr[PHONE_N][6];
static uint8_t  s_phoneMac[PHONE_N][6];
static uint32_t s_phoneSeen[PHONE_N];
static bool     s_phoneLive[PHONE_N];

static void phoneNote(const uint8_t* addr, const uint8_t id[PusiMesh::PHONE_ID_LEN], uint32_t now) {
    uint8_t slot = 0;
    bool found = false;
    for (uint8_t i = 0; i < PHONE_N; i++)
        if (s_phoneLive[i] && memcmp(s_phoneAddr[i], addr, 6) == 0) { slot = i; found = true; break; }
    if (!found) {
        for (uint8_t i = 0; i < PHONE_N; i++) {
            if (!s_phoneLive[i]) { slot = i; break; }
            if ((int32_t)(s_phoneSeen[i] - s_phoneSeen[slot]) < 0) slot = i;
        }
        memcpy(s_phoneAddr[slot], addr, 6);
        s_phoneLive[slot] = true;
    }
    PusiMesh::phoneMac(id, s_phoneMac[slot]);
    s_phoneSeen[slot] = now;
}

// The identity a frame from this address is sealed under, if it is a phone's.
static const uint8_t* phoneMacFor(const uint8_t* addr) {
    for (uint8_t i = 0; i < PHONE_N; i++)
        if (s_phoneLive[i] && memcmp(s_phoneAddr[i], addr, 6) == 0) return s_phoneMac[i];
    return nullptr;
}

static void squadNote(const uint8_t* mac, uint32_t now, const PusiMesh::Peer& p) {
    uint8_t slot = SQUAD_N;
    for (uint8_t i = 0; i < SQUAD_N; i++)
        if (s_squadLive[i] && memcmp(s_squadMac[i], mac, 6) == 0) { slot = i; break; }
    if (slot == SQUAD_N) {
        // A free slot, else the one heard from longest ago.
        slot = 0;
        for (uint8_t i = 0; i < SQUAD_N; i++) {
            if (!s_squadLive[i]) { slot = i; break; }
            if ((int32_t)(s_squadSeen[i] - s_squadSeen[slot]) < 0) slot = i;
        }
        memcpy(s_squadMac[slot], mac, 6);
        s_squadLive[slot] = true;
        Serial.printf("[squad] %02x%02x arrived\n", mac[4], mac[5]);
    } else if ((int32_t)(now - s_squadSeen[slot]) > 4000) {
        // BENCH: adverts go out every 1.5 s, so a gap this long is three or
        // more missed in a row; past PEER_STALE_MS the member has left the screen.
        Serial.printf("[squad] %02x%02x heard again after %lu ms%s\n", mac[4], mac[5],
                      (unsigned long)(now - s_squadSeen[slot]),
                      (int32_t)(now - s_squadSeen[slot]) > (int32_t)PEER_STALE_MS ? " (had left)" : "");
    }
    s_squadPeer[slot] = p;
    s_squadSeen[slot] = now;
}

uint8_t squadList(uint32_t now, SquadMember* out, uint8_t cap) {
    if (!Settings::meshDetect()) return 0;
    uint8_t n = 0;
    for (uint8_t i = 0; i < SQUAD_N && n < cap; i++) {
        if (!s_squadLive[i] || (int32_t)(now - s_squadSeen[i]) > (int32_t)PEER_STALE_MS) continue;
        memcpy(out[n].mac, s_squadMac[i], 6);
        out[n].peer = s_squadPeer[i];
        out[n].seen = s_squadSeen[i];
        n++;
    }
    // Insertion sort by address: eight at most, and a stable order is what
    // keeps the carousel from shuffling as adverts arrive in a new order.
    for (uint8_t i = 1; i < n; i++)
        for (uint8_t j = i; j > 0 && memcmp(out[j - 1].mac, out[j].mac, 6) > 0; j--) {
            SquadMember t = out[j]; out[j] = out[j - 1]; out[j - 1] = t;
        }
    return n;
}

bool peerLook(const uint8_t mac[6], PusiMesh::Peer& out) {
    for (uint8_t i = 0; i < SQUAD_N; i++)
        if (s_squadLive[i] && memcmp(s_squadMac[i], mac, 6) == 0) { out = s_squadPeer[i]; return true; }
    return false;
}

void preferPeer(const uint8_t mac[6]) {
    memcpy(s_preferMac, mac, 6);
    s_preferSet = true;
}

uint8_t squadCount(uint32_t now) {
    if (!Settings::meshDetect()) return 0;
    uint8_t n = 0;
    // Signed: the BLE task's millis() can be a tick ahead of the loop's.
    for (uint8_t i = 0; i < SQUAD_N; i++)
        if (s_squadLive[i] && (int32_t)(now - s_squadSeen[i]) <= (int32_t)PEER_STALE_MS) n++;
    return n;
}

const PusiMesh::Peer* peer() {
    return s_havePeer ? &s_peer : nullptr;
}
const uint8_t* peerMac() { return s_peerMac; }

// What we look like, read fresh each time rather than cached: the outfit and
// the name can both change while this is running, and a peer drawing a stale
// version of us is a bug nobody would think to look for.
size_t buildSelf(uint8_t* out) {
    PusiMesh::Peer me{};
    me.nick   = Potsy::nicknameIndex();
    me.outfit = Potsy::outfitIndex();
    me.shade  = Potsy::shadesIndex();
    me.aura   = Potsy::hasAura() && Settings::auraShown();
    const char* cn = Potsy::customName();
    me.custom = (cn && cn[0]);
    me.name[0] = '\0';
    if (me.custom) {
        size_t i = 0;
        for (; i < PusiMesh::NAME_LEN && cn[i]; i++) me.name[i] = cn[i];
        me.name[i] = '\0';
    }
    return PusiMesh::encode(me, out);
}

void begin() { s_havePeer = false; }

bool onManufacturerData(const uint8_t* d, size_t len, const uint8_t* mac, uint32_t now) {
    // Refused at the door when detection is off, so a peer cannot be latched
    // between the setting changing and the next tick.
    if (!Settings::meshDetect()) return false;
    // Company ID first, little-endian, then the payload. The look extension
    // is shorter than any presence payload, so the length floor moves down
    // past its size before anything is read.
    if (len < 2) return false;
    const uint16_t cid = (uint16_t)(d[0] | ((uint16_t)d[1] << 8));
    if (cid != PusiMesh::COMPANY_ID) return false;

    // Our look extension, if this is the second manufacturer AD rather than
    // the presence one. Tried first because it is told apart by its own magic
    // and is shorter than a presence payload could ever be.
    {
        PusiMesh::Look lk;
        if (PusiMesh::decodeLook(d + 2, len - 2, lk)) { noteLook(mac, lk, now); return true; }
    }
    if (len < 2 + PusiMesh::LEN_INDEXED) return false;

    // A message frame rather than an advert. Queued raw for the loop task:
    // this runs in the BLE host task and the cipher lives on the loop, so no
    // key is ever used from two tasks at once. Consumed when messages are on,
    // so a message can never reach the signature tables either.
    if (MeshMsg::isFrame(d + 2, len - 2)) {
        if (!Settings::messagesOn()) return false;
        // The name rides the presence advert, which usually arrives in an
        // earlier callback than the frame, so s_name holds somebody else's
        // by now. The roster remembers every member seen: a bubble drawn
        // over a cameo always knows its sender, never "someone".
        const char* nm = "";
        if (memcmp(mac, s_nameMac, 6) == 0 && s_name[0]) nm = s_name;
        else {
            PusiMesh::Peer lk;
            if (peerLook(mac, lk))
                nm = (lk.custom && lk.name[0]) ? lk.name : Potsy::nicknameAt(lk.nick);
        }
        MeshTalk::onFrame(mac, d + 2, len - 2, nm, phoneMacFor(mac));
        return true;
    }

    PusiMesh::Peer p;
    if (!PusiMesh::decode(d + 2, len - 2, p)) return false;
    if (p.phone) phoneNote(mac, p.id, now);
    squadNote(mac, now, p);     // everybody counts, visiting or not

    // Remembered for a message frame later in this same callback: the name
    // goes on the message, and the advert is the only place it travels.
    {
        memcpy(s_nameMac, mac, 6);
        const char* nm = (p.custom && p.name[0]) ? p.name : Potsy::nicknameAt(p.nick);
        size_t i = 0;
        for (; i < sizeof s_name - 1 && nm[i]; i++) s_name[i] = nm[i];
        s_name[i] = '\0';
    }

    // Ours. Keep the one we already have unless this IS the one we already
    // have -- a second PotsSquad arriving mid-visit does not get to shove
    // the first one off the screen.
    //
    // ...unless the SQUAD screen picked this one. Then it takes the slot, and
    // the visit machine sees a different guest and plays the goodbye.
    const bool chosen = s_preferSet && memcmp(mac, s_preferMac, 6) == 0;
    if (s_havePeer && memcmp(mac, s_peerMac, 6) != 0 && !chosen) return true;

#ifdef BENCH_TOOLS
    // A new visitor, for proving a phone (or anything) can visit from the bench.
    if (!s_havePeer || memcmp(mac, s_peerMac, 6) != 0)
        Serial.printf("[mesh] visitor %02X:%02X:%02X:%02X:%02X:%02X nick %u outfit %u shade %u aura %u name \"%s\"\n",
                      mac[5], mac[4], mac[3], mac[2], mac[1], mac[0], (unsigned)p.nick, (unsigned)p.outfit,
                      (unsigned)p.shade, (unsigned)p.aura, p.custom ? p.name : "");
#endif
    s_peer = p;
    memcpy(s_peerMac, mac, 6);
    s_peerSeen = now;
    s_havePeer = true;
    return true;
}

void tick(uint32_t now) {
    radioTick(now);

    // Detecting is its own switch now, and off means no visitor at all --
    // not "advertise less". A peer already on screen is dropped rather than
    // frozen there.
    if (!Settings::meshDetect()) { s_havePeer = false; return; }

    // SIGNED. s_peerSeen is stamped in the BLE task with its own millis(),
    // which can be later than this loop pass's `now`; unsigned, that age was
    // minus a few ms, read as 49 days, and the visitor was dropped the
    // instant it was heard -- then heard again: the watch's guest phasing in
    // and out. The watch loops ten times a second with its screen off, so it
    // hit this far more often than the CYDs. The squad list was already
    // signed, which is why the member count never flickered.
    if (s_havePeer && (int32_t)(now - s_peerSeen) > (int32_t)PEER_STALE_MS) s_havePeer = false;
}

} // namespace Mesh
#endif // POTS_MESH
